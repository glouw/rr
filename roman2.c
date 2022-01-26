/*
 * The Roman II Programming Language
 *
 * Copyright (c) 2021-2022 Gustav Louw. All rights reserved.
 * 
 * This work is licensed under the terms of the MIT license.  
 * For a copy, see <https://opensource.org/licenses/MIT>.
 *
 */

#include <roman2.h>

#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <dlfcn.h>

#define QUEUE_BLOCK_SIZE (8)
#define MAP_UNPRIMED (-1)
#define STR_CAP_SIZE (16)
#define MODULE_BUFFER_SIZE (512)
#define RR_LEN(a) (sizeof(a) / sizeof(*a))

struct RR_String
{
    char* value;
    size_t size;
    size_t cap;
};

struct RR_Char
{
    RR_Value* string;
    char* value;
};

struct RR_Function
{
    RR_String* name;
    size_t size;
    size_t address;
};

typedef struct
{
    void* value[QUEUE_BLOCK_SIZE];
    size_t a;
    size_t b;
}
Block;

struct RR_Queue
{
    Block** block;
    RR_Kill kill;
    RR_Copy copy;
    size_t size;
    size_t blocks;
};

typedef struct Node
{
    struct Node* next;
    RR_String* key;
    void* value;
}
Node;

struct RR_File
{
    RR_String* path;
    RR_String* mode;
    FILE* file;
};

struct RR_Map
{
    Node** bucket;
    RR_Kill kill;
    RR_Copy copy;
    size_t size;
    int64_t prime_index;
    float load_factor;
    unsigned rand1;
    unsigned rand2;
};

typedef union
{
    RR_File* file;
    RR_Function* function;
    RR_Queue* queue;
    RR_Map* map;
    RR_String* string;
    RR_Char* character;
    double number;
    bool boolean;
}
Of;

struct RR_Value
{
    RR_Type type;
    Of of;
    size_t refs;
};

typedef enum
{
    FRONT,
    BACK,
}
End;

typedef struct
{
    char* entry;
    bool dump;
    bool help;
}
Args;

typedef enum
{
    CLASS_VARIABLE_GLOBAL,
    CLASS_VARIABLE_LOCAL,
    CLASS_FUNCTION,
    CLASS_FUNCTION_PROTOTYPE,
    CLASS_FUNCTION_PROTOTYPE_NATIVE,
}
Class;

typedef enum
{
    OPCODE_ADD,
    OPCODE_AND,
    OPCODE_BRF,
    OPCODE_CAL,
    OPCODE_CPY,
    OPCODE_DEL,
    OPCODE_DIV,
    OPCODE_DLL,
    OPCODE_EXT,
    OPCODE_END,
    OPCODE_EQL,
    OPCODE_FLS,
    OPCODE_FMT,
    OPCODE_GET,
    OPCODE_GLB,
    OPCODE_GOD,
    OPCODE_GRT,
    OPCODE_GTE,
    OPCODE_INS,
    OPCODE_JMP,
    OPCODE_KEY,
    OPCODE_LEN,
    OPCODE_LOC,
    OPCODE_LOD,
    OPCODE_LOR,
    OPCODE_LST,
    OPCODE_LTE,
    OPCODE_MEM,
    OPCODE_MOV,
    OPCODE_MUL,
    OPCODE_NEQ,
    OPCODE_NOT,
    OPCODE_OPN,
    OPCODE_POP,
    OPCODE_PRT,
    OPCODE_PSB,
    OPCODE_PSF,
    OPCODE_PSH,
    OPCODE_RED,
    OPCODE_REF,
    OPCODE_RET,
    OPCODE_SAV,
    OPCODE_SPD,
    OPCODE_SRT,
    OPCODE_SUB,
    OPCODE_TYP,
    OPCODE_VRT,
    OPCODE_WRT,
}
Opcode;

typedef struct
{
    RR_Queue* modules;
    RR_Queue* assembly;
    RR_Queue* data;
    RR_Queue* debug;
    RR_Map* identifiers;
    RR_Map* files;
    RR_Map* included;
    RR_String* prime;
    size_t globals;
    size_t locals;
    size_t labels;
}
CC;

typedef struct
{
    FILE* file;
    RR_String* name;
    size_t index;
    size_t size;
    size_t line;
    unsigned char buffer[MODULE_BUFFER_SIZE];
}
Module;

typedef struct
{
    Class class;
    size_t stack;
    RR_String* path;
}
Meta;

typedef struct
{
    RR_Queue* data;
    RR_Queue* stack;
    RR_Queue* frame;
    RR_Queue* debug; // NOTE: Owned by CC.
    RR_Queue* addresses;
    RR_Map* track;
    RR_Value* ret;
    uint64_t* instructions;
    size_t size;
    size_t pc;
    size_t spds;
}
VM;

typedef struct
{
    size_t pc;
    size_t sp;
    size_t address;
}
Frame;

typedef struct
{
    RR_String* label;
    size_t address;
}
Stack;

typedef struct
{
    char* file;
    size_t line;
}
Debug;

static void*
Malloc(size_t size)
{
    return malloc(size);
}

static void*
Realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

static void
Free(void* pointer)
{
    free(pointer);
}

static void*
Calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

static void
Delete(RR_Kill kill, void* value)
{
    if(kill)
        kill(value);
}

static bool
Equals(char* a, char* b)
{
    return a[0] != b[0] ? false : (strcmp(a, b) == 0);
}

static void
Type_Kill(RR_Type type, Of* of);


RR_Type
RR_Value_ToType(RR_Value* self)
{
    return self->type;
}

RR_File*
RR_Value_ToFile(RR_Value* self)
{
    return self->type == TYPE_FILE
         ? self->of.file
         : NULL;
}

RR_Queue*
RR_Value_ToQueue(RR_Value* self)
{
    return self->type == TYPE_QUEUE
         ? self->of.queue
         : NULL;
}

RR_Map*
RR_Value_ToMap(RR_Value* self)
{
    return self->type == TYPE_MAP
         ? self->of.map
         : NULL;
}

RR_String*
RR_Value_ToString(RR_Value* self)
{
    return self->type == TYPE_STRING
         ? self->of.string
         : NULL;
}

RR_Char*
RR_Value_ToChar(RR_Value* self)
{
    return self->type == TYPE_CHAR
         ? self->of.character
         : NULL;
}

double*
RR_Value_ToNumber(RR_Value* self)
{
    return self->type == TYPE_NUMBER
         ? &self->of.number
         : NULL;
}

bool*
RR_Value_ToBool(RR_Value* self)
{
    return self->type == TYPE_BOOL
         ? &self->of.boolean
         : NULL;
}

RR_Function*
RR_Value_ToFunction(RR_Value* self)
{
    return self->type == TYPE_FUNCTION
         ? self->of.function
         : NULL;
}

size_t
RR_Value_Len(RR_Value* self)
{
    switch(self->type)
    {
    case TYPE_FILE:
        return RR_File_Size(self->of.file);
    case TYPE_FUNCTION:
        return self->of.function->size;
    case TYPE_QUEUE:
        return self->of.queue->size;
    case TYPE_MAP:
        return self->of.map->size;
    case TYPE_STRING:
        return self->of.string->size;
    case TYPE_CHAR:
    case TYPE_NUMBER:
    case TYPE_BOOL:
    case TYPE_NULL:
        break;
    }
    return 0;
}

size_t
RR_Value_Refs(RR_Value* self)
{
    return self->refs;
}

void
RR_Value_Dec(RR_Value* self)
{
    self->refs -= 1;
}

void
RR_Value_Inc(RR_Value* self)
{
    self->refs += 1;
}

void
RR_Value_Kill(RR_Value* self)
{
    if(self->refs == 0)
    {
        Type_Kill(self->type, &self->of);
        Free(self);
    }
    else
        RR_Value_Dec(self);
}

#define COMPARE_TABLE(CMP)                        \
    case TYPE_STRING:                             \
        return strcmp(a->of.string->value,        \
                      b->of.string->value) CMP 0; \
    case TYPE_NUMBER:                             \
        return a->of.number                       \
           CMP b->of.number;                      \
    case TYPE_FILE:                               \
        return RR_File_Size(a->of.file)           \
           CMP RR_File_Size(b->of.file);          \
    case TYPE_QUEUE:                              \
        return a->of.queue->size                  \
           CMP b->of.queue->size;                 \
    case TYPE_CHAR:                               \
        return *a->of.character->value            \
           CMP *b->of.character->value;           \
    case TYPE_MAP:                                \
        return a->of.map->size                    \
           CMP b->of.map->size;                   \
    case TYPE_BOOL:                               \
        return a->of.boolean                      \
           CMP b->of.boolean;                     \
    case TYPE_FUNCTION:                           \
        return a->of.function->size               \
           CMP b->of.function->size;              \
    case TYPE_NULL:                               \
        break;

bool
RR_Value_LessThan(RR_Value* a, RR_Value* b)
{
    if(a->type == b->type)
        switch(a->type) { COMPARE_TABLE(<); }
    return false;
}

bool
RR_Value_GreaterThanEqualTo(RR_Value* a, RR_Value* b)
{
    if(a->type == b->type)
        switch(a->type) { COMPARE_TABLE(>=); }
    return false;
}

bool
RR_Value_GreaterThan(RR_Value* a, RR_Value* b)
{
    if(a->type == b->type)
        switch(a->type) { COMPARE_TABLE(>); }
    return false;
}

bool
RR_Value_LessThanEqualTo(RR_Value* a, RR_Value* b)
{
    if(a->type == b->type)
        switch(a->type) { COMPARE_TABLE(<=); }
    return false;
}

bool
RR_Value_Equal(RR_Value* a, RR_Value* b)
{
    if(a->type == TYPE_CHAR && b->type == TYPE_STRING)
        return *a->of.character->value == b->of.string->value[0];
    else
    if(b->type == TYPE_CHAR && a->type == TYPE_STRING)
        return *b->of.character->value == a->of.string->value[0];
    else
    if(a->type == b->type)
        switch(a->type)
        {
        case TYPE_FILE:
            return RR_File_Equal(a->of.file, b->of.file);
        case TYPE_FUNCTION:
            return RR_Function_Equal(a->of.function, b->of.function);
        case TYPE_QUEUE:
            return RR_Queue_Equal(a->of.queue, b->of.queue, (RR_Compare) RR_Value_Equal);
        case TYPE_MAP:
            return RR_Map_Equal(a->of.map, b->of.map, (RR_Compare) RR_Value_Equal);
        case TYPE_STRING:
            return RR_String_Equal(a->of.string, b->of.string);
        case TYPE_NUMBER:
            return a->of.number == b->of.number;
        case TYPE_BOOL:
            return a->of.boolean == b->of.boolean;
        case TYPE_CHAR:
            return *a->of.character->value == *b->of.character->value;
        case TYPE_NULL:
            return b->type == TYPE_NULL;
        }
    return false;
}

RR_Value*
RR_Value_Copy(RR_Value* self)
{
    RR_Value* copy = Malloc(sizeof(*copy));
    copy->refs = 0;
    RR_Type_Copy(copy, self);
    return copy;
}

static RR_Value*
Value_Init(Of of, RR_Type type)
{
    RR_Value* self = Malloc(sizeof(*self));
    self->type = type;
    self->refs = 0;
    self->of = of;
    return self;
}

RR_Value*
RR_Value_NewQueue(void)
{
    Of of;
    of.queue = RR_Queue_Init((RR_Kill) RR_Value_Kill, (RR_Copy) RR_Value_Copy);
    return Value_Init(of, TYPE_QUEUE);
}

RR_Value*
RR_Value_NewMap(void)
{
    Of of;
    of.map = RR_Map_Init((RR_Kill) RR_Value_Kill, (RR_Copy) RR_Value_Copy);
    return Value_Init(of, TYPE_MAP);
}

RR_Value*
RR_Value_NewFunction(RR_Function* function)
{
    Of of;
    of.function = function;
    return Value_Init(of, TYPE_FUNCTION);
}

RR_Value*
RR_Value_NewChar(RR_Char* character)
{
    Of of;
    of.character = character;
    return Value_Init(of, TYPE_CHAR);
}

RR_Value*
RR_Value_NewString(RR_String* string)
{
    Of of;
    of.string = string;
    return Value_Init(of, TYPE_STRING);
}

RR_Value*
RR_Value_NewNumber(double number)
{
    Of of;
    of.number = number;
    return Value_Init(of, TYPE_NUMBER);
}

RR_Value*
RR_Value_NewBool(bool boolean)
{
    Of of;
    of.boolean = boolean;
    return Value_Init(of, TYPE_BOOL);
}

RR_Value*
RR_Value_NewFile(RR_File* file)
{
    Of of;
    of.file = file;
    return Value_Init(of, TYPE_FILE);
}

RR_Value*
RR_Value_NewNull(void)
{
    Of of;
    return Value_Init(of, TYPE_NULL);
}

static void
String_Append(RR_String* self, RR_String* other)
{
    RR_String_Appends(self, other->value);
    RR_String_Kill(other);
}

RR_String*
RR_Value_Sprint(RR_Value* self, bool newline, size_t indents)
{
    RR_String* print = RR_String_Init("");
    switch(self->type)
    {
    case TYPE_FILE:
        String_Append(print, RR_String_Format("{ \"%s\", \"%s\", %p }", self->of.file->path->value, self->of.file->mode->value, self->of.file->file));
        break;
    case TYPE_FUNCTION:
        String_Append(print, RR_String_Format("{ %s, %lu, %lu }", self->of.function->name->value, self->of.function->size, self->of.function->address));
        break;
    case TYPE_QUEUE:
        String_Append(print, RR_Queue_Print(self->of.queue, indents));
        break;
    case TYPE_MAP:
        String_Append(print, RR_Map_Print(self->of.map, indents));
        break;
    case TYPE_STRING:
        String_Append(print, RR_String_Format(indents == 0 ? "%s" : "\"%s\"", self->of.string->value));
        break;
    case TYPE_NUMBER:
        String_Append(print, RR_String_Format("%f", self->of.number));
        break;
    case TYPE_BOOL:
        String_Append(print, RR_String_Format("%s", self->of.boolean ? "true" : "false"));
        break;
    case TYPE_CHAR:
        String_Append(print, RR_String_Format(indents == 0 ? "%c" : "\"%c\"", *self->of.character->value));
        break;
    case TYPE_NULL:
        RR_String_Appends(print, "null");
        break;
    }
    if(newline)
        RR_String_Appends(print, "\n");
    return print; 
}

void
RR_Value_Print(RR_Value* self)
{
    RR_String* out = RR_Value_Sprint(self, false, 0);
    puts(out->value);
    RR_String_Kill(out);
}

void
RR_Value_Sub(RR_Value* a, RR_Value* b)
{
    char* x = a->of.character->value;
    char* y = b->of.string->value;
    while(*x && *y)
    {
        *x = *y;
        x += 1;
        y += 1;
    }
}

static void
String_Alloc(RR_String* self, size_t cap)
{
    self->cap = cap;
    self->value = Realloc(self->value, (1 + cap) * sizeof(*self->value));
}

char*
RR_String_Value(RR_String* self)
{
    return self->value;
}

RR_String*
RR_String_Init(char* string)
{
    RR_String* self = Malloc(sizeof(*self));
    self->value = NULL;
    self->size = strlen(string);
    String_Alloc(self, self->size < STR_CAP_SIZE ? STR_CAP_SIZE : self->size);
    strcpy(self->value, string);
    return self;
}

void
RR_String_Kill(RR_String* self)
{
    Delete(Free, self->value);
    Free(self);
}

static RR_String*
String_FromChar(char c)
{
    char string[] = { c, '\0' };
    return RR_String_Init(string);
}

void
RR_String_Swap(RR_String** self, RR_String** other)
{
    RR_String* temp = *self;
    *self = *other;
    *other = temp;
}

RR_String*
RR_String_Copy(RR_String* self)
{
    return RR_String_Init(self->value);
}

size_t
RR_String_Size(RR_String* self)
{
    return self->size;
}

void
RR_String_Replace(RR_String* self, char x, char y)
{
    for(size_t i = 0; i < self->size; i++)
        if(self->value[i] == x)
            self->value[i] = y;
}

char*
RR_String_End(RR_String* self)
{
    return &self->value[self->size - 1];
}

char
RR_String_Back(RR_String* self)
{
    return *RR_String_End(self);
}

char*
RR_String_Begin(RR_String* self)
{
    return &self->value[0];
}

size_t
RR_String_Empty(RR_String* self)
{
    return self->size == 0;
}

void
RR_String_PshB(RR_String* self, char ch)
{
    if(self->size == self->cap)
        String_Alloc(self, (self->cap == 0) ? 1 : (2 * self->cap));
    self->value[self->size + 0] = ch;
    self->value[self->size + 1] = '\0';
    self->size += 1;
}

RR_String*
RR_String_Indent(size_t indents)
{
    size_t width = 4;
    RR_String* ident = RR_String_Init("");
    while(indents > 0)
    {
        for(size_t i = 0; i < width; i++)
            RR_String_PshB(ident, ' ');
        indents -= 1;
    }
    return ident;
}

void
RR_String_PopB(RR_String* self)
{
    self->size -= 1;
    self->value[self->size] = '\0';
}

void
RR_String_Resize(RR_String* self, size_t size)
{
    if(size > self->cap)
        String_Alloc(self, size);
    self->size = size;
    self->value[size] = '\0';
}

bool
RR_String_Equals(RR_String* a, char* b)
{
    return Equals(a->value, b);
}

bool
RR_String_Equal(RR_String* a, RR_String* b)
{
    return Equals(a->value, b->value);
}

void
RR_String_Appends(RR_String* self, char* str)
{
    while(*str)
    {
        RR_String_PshB(self, *str);
        str += 1;
    }
}

RR_String*
RR_String_Base(RR_String* path)
{
    RR_String* base = RR_String_Copy(path);
    while(base->size != 0)
        if(RR_String_Back(base) == '/')
            break;
        else
            RR_String_PopB(base);
    return base;
}

RR_String*
RR_String_Moves(char** from)
{
    RR_String* self = Malloc(sizeof(*self));
    self->value = *from;
    self->size = strlen(self->value);
    self->cap = self->size;
    *from = NULL;
    return self;
}

static inline bool
String_IsBoolean(RR_String* ident)
{
    return RR_String_Equals(ident, "true") || RR_String_Equals(ident, "false");
}

static inline bool
String_IsNull(RR_String* ident)
{
    return RR_String_Equals(ident, "null");
}

RR_String*
RR_String_Format(char* format, ...)
{
    va_list args;
    va_start(args, format);
    RR_String* line = RR_String_Init("");
    size_t size = vsnprintf(line->value, line->size, format, args);
    va_end(args);
    if(size >= line->size)
    {
        va_start(args, format);
        RR_String_Resize(line, size);
        vsprintf(line->value, format, args);
        va_end(args);
    }
    else
        line->size = size;
    return line;
}

char*
RR_String_Get(RR_String* self, size_t index)
{
    if(index >= self->size)
        return NULL;
    return &self->value[index];
}

bool
RR_String_Del(RR_String* self, size_t index)
{
    if(RR_String_Get(self, index))
    {
        for(size_t i = index; i < self->size - 1; i++)
            self->value[i] = self->value[i + 1];
        RR_String_PopB(self);
        return true;
    }
    return false;
}

RR_File*
RR_File_Init(RR_String* path, RR_String* mode)
{
    RR_File* self = Malloc(sizeof(*self));
    self->path = path;
    self->mode = mode;
    self->file = fopen(self->path->value, self->mode->value);
    return self;
}

bool
RR_File_Equal(RR_File* a, RR_File* b)
{
    struct stat stat1;
    struct stat stat2;
    if(fstat(fileno(RR_File_File(a)), &stat1) < 0
    || fstat(fileno(RR_File_File(b)), &stat2) < 0)
        return false;
    return RR_String_Equal(a->path, b->path) 
        && RR_String_Equal(a->mode, b->mode)
        && stat1.st_dev == stat2.st_dev
        && stat1.st_ino == stat2.st_ino;
}

void
RR_File_Kill(RR_File* self)
{
    RR_String_Kill(self->path);
    RR_String_Kill(self->mode);
    if(self->file)
        fclose(self->file);
    Free(self);
}

RR_File*
RR_File_Copy(RR_File* self)
{
    return RR_File_Init(RR_String_Copy(self->path), RR_String_Copy(self->mode));
}

size_t
RR_File_Size(RR_File* self)
{
    if(self->file)
    {
        size_t prev = ftell(self->file);
        fseek(self->file, 0L, SEEK_END);
        size_t size = ftell(self->file);
        fseek(self->file, prev, SEEK_SET);
        return size;
    }
    else
        return 0;
}

FILE*
RR_File_File(RR_File* self)
{
    return self->file;
}

RR_Function*
RR_Function_Init(RR_String* name, size_t size, size_t address)
{
    RR_Function* self = Malloc(sizeof(*self));
    self->name = name;
    self->size = size;
    self->address = address;
    return self;
}

void
RR_Function_Kill(RR_Function* self)
{
    RR_String_Kill(self->name);
    Free(self);
}

RR_Function*
RR_Function_Copy(RR_Function* self)
{
    return RR_Function_Init(RR_String_Copy(self->name), self->size, self->address);
}

size_t
RR_Function_Size(RR_Function* self)
{
    return self->size;
}

size_t
RR_Function_Address(RR_Function* self)
{
    return self->address;
}

bool
RR_Function_Equal(RR_Function* a, RR_Function* b)
{
    return RR_String_Equal(a->name, b->name) && a->address == b->address && a->size == b->size;
}

Block*
Block_Init(End end)
{
    Block* self = Malloc(sizeof(*self));
    self->a = (end == BACK) ? 0 : QUEUE_BLOCK_SIZE;
    self->b = (end == BACK) ? 0 : QUEUE_BLOCK_SIZE;
    return self;
}

size_t
RR_Queue_Size(RR_Queue* self)
{
    return self->size;
}

bool
RR_Queue_Empty(RR_Queue* self)
{
    return self->size == 0;
}

static inline Block**
Queue_BlockF(RR_Queue* self)
{
    return &self->block[0];
}

static inline Block**
Queue_BlockB(RR_Queue* self)
{
    return &self->block[self->blocks - 1];
}

void*
RR_Queue_Front(RR_Queue* self)
{
    if(self->size == 0)
        return NULL;
    Block* block = *Queue_BlockF(self);
    return block->value[block->a];
}

void*
RR_Queue_Back(RR_Queue* self)
{
    if(self->size == 0)
        return NULL;
    Block* block = *Queue_BlockB(self);
    return block->value[block->b - 1];
}

RR_Queue*
RR_Queue_Init(RR_Kill kill, RR_Copy copy)
{
    RR_Queue* self = Malloc(sizeof(*self));
    self->block = NULL;
    self->kill = kill;
    self->copy = copy;
    self->size = 0;
    self->blocks = 0;
    return self;
}

void
RR_Queue_Kill(RR_Queue* self)
{
    for(size_t step = 0; step < self->blocks; step++)
    {
        Block* block = self->block[step];
        while(block->a < block->b)
        {
            Delete(self->kill, block->value[block->a]);
            block->a += 1;
        }
        Free(block);
    }
    Free(self->block);
    Free(self);
}

void**
RR_Queue_At(RR_Queue* self, size_t index)
{
    if(index < self->size)
    {
        Block* block = *Queue_BlockF(self);
        size_t at = index + block->a;
        size_t step = at / QUEUE_BLOCK_SIZE;
        size_t item = at % QUEUE_BLOCK_SIZE;
        return &self->block[step]->value[item];
    }
    return NULL;
}

void*
RR_Queue_Get(RR_Queue* self, size_t index)
{
    if(index == 0)
        return RR_Queue_Front(self);
    else
    if(index == self->size - 1)
        return RR_Queue_Back(self);
    else
    {
        void** at = RR_Queue_At(self, index);
        return at ? *at : NULL;
    }
}

static void
Queue_Alloc(RR_Queue* self, size_t blocks)
{
    self->blocks = blocks;
    self->block = Realloc(self->block, blocks * sizeof(*self->block));
}

void
RR_Queue_Push(RR_Queue* self, void* value, End end)
{
    if(end == BACK)
    {
        Block* block = *Queue_BlockB(self);
        block->value[block->b] = value;
        block->b += 1;
    }
    else
    {
        Block* block = *Queue_BlockF(self);
        block->a -= 1;
        block->value[block->a] = value;
    }
    self->size += 1;
}

void
RR_Queue_PshB(RR_Queue* self, void* value)
{
    if(self->blocks == 0)
    {
        Queue_Alloc(self, 1);
        *Queue_BlockB(self) = Block_Init(BACK);
    }
    else
    {
        Block* block = *Queue_BlockB(self);
        if(block->b == QUEUE_BLOCK_SIZE || block->a == QUEUE_BLOCK_SIZE)
        {
            Queue_Alloc(self, self->blocks + 1);
            *Queue_BlockB(self) = Block_Init(BACK);
        }
    }
    RR_Queue_Push(self, value, BACK);
}

void
RR_Queue_PshF(RR_Queue* self, void* value)
{
    if(self->blocks == 0)
    {
        Queue_Alloc(self, 1);
        *Queue_BlockF(self) = Block_Init(FRONT);
    }
    else
    {
        Block* block = *Queue_BlockF(self);
        if(block->b == 0 || block->a == 0)
        {
            Queue_Alloc(self, self->blocks + 1);
            for(size_t i = self->blocks - 1; i > 0; i--)
                self->block[i] = self->block[i - 1];
            *Queue_BlockF(self) = Block_Init(FRONT);
        }
    }
    RR_Queue_Push(self, value, FRONT);
}

void
RR_Queue_Pop(RR_Queue* self, End end)
{
    if(self->size != 0)
    {
        if(end == BACK)
        {
            Block* block = *Queue_BlockB(self);
            block->b -= 1;
            Delete(self->kill, block->value[block->b]);
            if(block->b == 0)
            {
                Free(block);
                Queue_Alloc(self, self->blocks - 1);
            }
        }
        else
        {
            Block* block = *Queue_BlockF(self);
            Delete(self->kill, block->value[block->a]);
            block->a += 1;
            if(block->a == QUEUE_BLOCK_SIZE)
            {
                for(size_t i = 0; i < self->blocks - 1; i++)
                    self->block[i] = self->block[i + 1];
                Free(block);
                Queue_Alloc(self, self->blocks - 1);
            }
        }
        self->size -= 1;
    }
}

void
RR_Queue_PopB(RR_Queue* self)
{
    RR_Queue_Pop(self, BACK);
}

void
RR_Queue_PopF(RR_Queue* self)
{
    RR_Queue_Pop(self, FRONT);
}

static void
Queue_PopFSoft(RR_Queue* self)
{
    RR_Kill kill = self->kill;
    self->kill = NULL;
    RR_Queue_PopF(self);
    self->kill = kill;
}

static void
Queue_PopBSoft(RR_Queue* self)
{
    RR_Kill kill = self->kill;
    self->kill = NULL;
    RR_Queue_PopB(self);
    self->kill = kill;
}

bool
RR_Queue_Del(RR_Queue* self, size_t index)
{
    if(index == 0)
        RR_Queue_PopF(self);
    else
    if(index == self->size - 1)
        RR_Queue_PopB(self);
    else
    {
        void** at = RR_Queue_At(self, index);
        if(at)
        {
            Delete(self->kill, *at);
            if(index < self->size / 2)
            {
                while(index > 0)
                {
                    *RR_Queue_At(self, index) = *RR_Queue_At(self, index - 1);
                    index -= 1;
                }
                Queue_PopFSoft(self);
            }
            else
            {
                while(index < self->size - 1)
                {
                    *RR_Queue_At(self, index) = *RR_Queue_At(self, index + 1);
                    index += 1;
                }
                Queue_PopBSoft(self);
            }
        }
        else
            return false;
    }
    return true;
}

RR_Queue*
RR_Queue_Copy(RR_Queue* self)
{
    RR_Queue* copy = RR_Queue_Init(self->kill, self->copy);
    for(size_t i = 0; i < self->size; i++)
    {
        void* temp = RR_Queue_Get(self, i);
        void* value = copy->copy ? copy->copy(temp) : temp;
        RR_Queue_PshB(copy, value);
    }
    return copy;
}

void
RR_Queue_Prepend(RR_Queue* self, RR_Queue* other)
{
    size_t i = other->size;
    while(i != 0)
    {
        i -= 1;
        void* temp = RR_Queue_Get(other, i);
        void* value = self->copy ? self->copy(temp) : temp;
        RR_Queue_PshF(self, value);
    }
}

void
RR_Queue_Append(RR_Queue* self, RR_Queue* other)
{
    for(size_t i = 0; i < other->size; i++)
    {
        void* temp = RR_Queue_Get(other, i);
        void* value = self->copy ? self->copy(temp) : temp;
        RR_Queue_PshB(self, value);
    }
}

bool
RR_Queue_Equal(RR_Queue* self, RR_Queue* other, RR_Compare compare)
{
    if(self->size != other->size)
        return false;
    else
        for(size_t i = 0; i < self->size; i++)
            if(!compare(RR_Queue_Get(self, i), RR_Queue_Get(other, i)))
                return false;
    return true;
}

void
RR_Queue_Swap(RR_Queue* self, int64_t a, int64_t b)
{
    void** x = RR_Queue_At(self, a);
    void** y = RR_Queue_At(self, b);
    void* temp = *x;
    *x = *y;
    *y = temp;
}

static void
RR_Queue_RangedSort(RR_Queue* self, RR_Compare compare, int64_t left, int64_t right)
{
    if(left >= right)
        return;
    RR_Queue_Swap(self, left, (left + right) / 2);
    int64_t last = left;
    for(int64_t i = left + 1; i <= right; i++)
    {
        void* a = RR_Queue_Get(self, i);
        void* b = RR_Queue_Get(self, left);
        if(compare(a, b))
             RR_Queue_Swap(self, ++last, i);
    }
   RR_Queue_Swap(self, left, last);
   RR_Queue_RangedSort(self, compare, left, last - 1);
   RR_Queue_RangedSort(self, compare, last + 1, right);
}

void
RR_Queue_Sort(RR_Queue* self, RR_Compare compare)
{
    RR_Queue_RangedSort(self, compare, 0, self->size - 1);
}

RR_String*
RR_Queue_Print(RR_Queue* self, size_t indents)
{
    if(self->size == 0)
        return RR_String_Init("[]");
    else
    {
        RR_String* print = RR_String_Init("[\n");
        size_t size = self->size;
        for(size_t i = 0; i < size; i++)
        {
            String_Append(print, RR_String_Indent(indents + 1));
            String_Append(print, RR_String_Format("[%lu] = ", i));
            String_Append(print, RR_Value_Sprint(RR_Queue_Get(self, i), false, indents + 1));
            if(i < size - 1)
                RR_String_Appends(print, ",");
            RR_String_Appends(print, "\n");
        }
        String_Append(print, RR_String_Indent(indents));
        RR_String_Appends(print, "]");
        return print;
    }
}

Node*
Node_Init(RR_String* key, void* value)
{
    Node* self = Malloc(sizeof(*self));
    self->next = NULL;
    self->key = key;
    self->value = value;
    return self;
}

void
Node_Set(Node* self, RR_Kill kill, RR_String* key, void* value)
{
    Delete((RR_Kill) RR_String_Kill, self->key);
    Delete(kill, self->value);
    self->key = key;
    self->value = value;
}

void
Node_Kill(Node* self, RR_Kill kill)
{
    Node_Set(self, kill, NULL, NULL);
    Free(self);
}

void
Node_Push(Node** self, Node* node)
{
    node->next = *self;
    *self = node;
}

Node*
Node_Copy(Node* self, RR_Copy copy)
{
    return Node_Init(RR_String_Copy(self->key), copy ? copy(self->value) : self->value);
}

static const size_t
Map_Primes[] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67,
    71, 73, 79, 83, 89, 97, 103, 109, 113, 127, 137, 139, 149, 157, 167, 179,
    193, 199, 211, 227, 241, 257, 277, 293, 313, 337, 359, 383, 409, 439, 467,
    503, 541, 577, 619, 661, 709, 761, 823, 887, 953, 1031, 1109, 1193, 1289,
    1381, 1493, 1613, 1741, 1879, 2029, 2179, 2357, 2549, 2753, 2971, 3209,
    3469, 3739, 4027, 4349, 4703, 5087, 5503, 5953, 6427, 6949, 7517, 8123,
    8783, 9497, 10273, 11113, 12011, 12983, 14033, 15173, 16411, 17749, 19183,
    20753, 22447, 24281, 26267, 28411, 30727, 33223, 35933, 38873, 42043,
    45481, 49201, 53201, 57557, 62233, 67307, 72817, 78779, 85229, 92203,
};

size_t
RR_Map_Buckets(RR_Map* self)
{
    if(self->prime_index == MAP_UNPRIMED)
        return 0; 
    return Map_Primes[self->prime_index];
}

bool
RR_Map_Resizable(RR_Map* self)
{
    return self->prime_index < (int64_t) RR_LEN(Map_Primes) - 1;
}

RR_Map*
RR_Map_Init(RR_Kill kill, RR_Copy copy)
{
    RR_Map* self = Malloc(sizeof(*self));
    self->kill = kill;
    self->copy = copy;
    self->bucket = NULL;
    self->size = 0;
    self->prime_index = MAP_UNPRIMED;
    self->load_factor = 0.7f;
    self->rand1 = rand();
    self->rand2 = rand();
    return self;
}

size_t
RR_Map_Size(RR_Map* self)
{
    return self->size;
}

bool
RR_Map_Empty(RR_Map* self)
{
    return self->size == 0;
}

void
RR_Map_Kill(RR_Map* self)
{
    for(size_t i = 0; i < RR_Map_Buckets(self); i++)
    {
        Node* bucket = self->bucket[i];
        while(bucket)
        {
            Node* next = bucket->next;
            Node_Kill(bucket, self->kill);
            bucket = next;
        }
    }
    Free(self->bucket);
    Free(self);
}

unsigned
RR_Map_Hash(RR_Map* self, char* key)
{
    unsigned r1 = self->rand1;
    unsigned r2 = self->rand2;
    unsigned hash = 0;
    while(*key)
    {
        hash *= r1;
        hash += *key;
        r1 *= r2;
        key += 1;
    }
    return hash % RR_Map_Buckets(self);
}

Node**
RR_Map_Bucket(RR_Map* self, char* key)
{
    unsigned index = RR_Map_Hash(self, key);
    return &self->bucket[index];
}

static void
Map_Alloc(RR_Map* self, size_t index)
{
    self->prime_index = index;
    self->bucket = Calloc(RR_Map_Buckets(self), sizeof(*self->bucket));
}

static void
RR_Map_Emplace(RR_Map* self, RR_String* key, Node* node);

void
RR_Map_Rehash(RR_Map* self)
{
    RR_Map* other = RR_Map_Init(self->kill, self->copy);
    Map_Alloc(other, self->prime_index + 1);
    for(size_t i = 0; i < RR_Map_Buckets(self); i++)
    {
        Node* bucket = self->bucket[i];
        while(bucket)
        {
            Node* next = bucket->next;
            RR_Map_Emplace(other, bucket->key, bucket);
            bucket = next;
        }
    }
    Free(self->bucket);
    *self = *other;
    Free(other);
}

bool
RR_Map_Full(RR_Map* self)
{
    return self->size / (float) RR_Map_Buckets(self) > self->load_factor;
}

static void
RR_Map_Emplace(RR_Map* self, RR_String* key, Node* node)
{
    if(self->prime_index == MAP_UNPRIMED)
        Map_Alloc(self, 0);
    else
    if(RR_Map_Resizable(self) && RR_Map_Full(self))
        RR_Map_Rehash(self);
    Node_Push(RR_Map_Bucket(self, key->value), node);
    self->size += 1;
}

Node*
RR_Map_At(RR_Map* self, char* key)
{
    if(self->size != 0) 
    {
        Node* chain = *RR_Map_Bucket(self, key);
        while(chain)
        {
            if(RR_String_Equals(chain->key, key))
                return chain;
            chain = chain->next;
        }
    }
    return NULL;
}

bool
RR_Map_Exists(RR_Map* self, char* key)
{
    return RR_Map_At(self, key) != NULL;
}

void
RR_Map_Del(RR_Map* self, char* key)
{
    if(self->size != 0) 
    {
        Node** head = RR_Map_Bucket(self, key);
        Node* chain = *head;
        Node* prev = NULL;
        while(chain)
        {
            if(RR_String_Equals(chain->key, key))
            {
                if(prev)
                    prev->next = chain->next;
                else
                    *head = chain->next;
                Node_Kill(chain, self->kill);
                self->size -= 1;
                break;
            }
            prev = chain;
            chain = chain->next;
        }
    }
}

void
RR_Map_Set(RR_Map* self, RR_String* key, void* value)
{
    Node* found = RR_Map_At(self, key->value);
    if(found)
        Node_Set(found, self->kill, key, value);
    else
    {
        Node* node = Node_Init(key, value);
        RR_Map_Emplace(self, key, node);
    }
}

void*
RR_Map_Get(RR_Map* self, char* key)
{
    Node* found = RR_Map_At(self, key);
    return found ? found->value : NULL;
}

RR_Map*
RR_Map_Copy(RR_Map* self)
{
    RR_Map* copy = RR_Map_Init(self->kill, self->copy);
    for(size_t i = 0; i < RR_Map_Buckets(self); i++)
    {
        Node* chain = self->bucket[i];
        while(chain)
        {
            Node* node = Node_Copy(chain, copy->copy);
            RR_Map_Emplace(copy, node->key, node);
            chain = chain->next;
        }
    }
    return copy;
}

void
RR_Map_Append(RR_Map* self, RR_Map* other)
{
    for(size_t i = 0; i < RR_Map_Buckets(other); i++)
    {
        Node* chain = other->bucket[i];
        while(chain)
        {
            RR_Map_Set(self, RR_String_Copy(chain->key), self->copy ? self->copy(chain->value) : chain->value);
            chain = chain->next;
        }
    }
}


bool
RR_Map_Equal(RR_Map* self, RR_Map* other, RR_Compare compare)
{
    if(self->size == other->size)
    {
        for(size_t i = 0; i < RR_Map_Buckets(self); i++)
        {
            Node* chain = self->bucket[i];
            while(chain)
            {
                void* got = RR_Map_Get(other, chain->key->value);
                if(got == NULL)
                    return false;
                if(!compare(chain->value, got))
                    return false;
                chain = chain->next;
            }
        }
    }
    return true;
}

RR_String*
RR_Map_Print(RR_Map* self, size_t indents)
{
    if(self->size == 0)
        return RR_String_Init("{}");
    else
    {
        RR_String* print = RR_String_Init("{\n");
        for(size_t i = 0; i < RR_Map_Buckets(self); i++)
        {
            Node* bucket = self->bucket[i];
            while(bucket)
            {
                String_Append(print, RR_String_Indent(indents + 1));
                String_Append(print, RR_String_Format("\"%s\" : ", bucket->key->value));
                String_Append(print, RR_Value_Sprint(bucket->value, false, indents + 1));
                if(i < self->size - 1)
                    RR_String_Appends(print, ",");
                RR_String_Appends(print, "\n");
                bucket = bucket->next;
            }
        }
        String_Append(print, RR_String_Indent(indents));
        RR_String_Appends(print, "}");
        return print;
    }
}

RR_Value*
RR_Map_Key(RR_Map* self)
{
    RR_Value* queue = RR_Value_NewQueue();
    for(size_t i = 0; i < RR_Map_Buckets(self); i++)
    {
        Node* chain = self->bucket[i];
        while(chain)
        {
            RR_Value* string = RR_Value_NewString(RR_String_Copy(chain->key));
            RR_Queue_PshB(queue->of.queue, string);
            chain = chain->next;
        }
    }
    RR_Queue_Sort(queue->of.queue, (RR_Compare) RR_Value_LessThan);
    return queue;
}

RR_Char*
RR_Char_Init(RR_Value* string, size_t index)
{
    char* value = RR_String_Get(string->of.string, index);
    if(value)
    {
        RR_Char* self = Malloc(sizeof(*self));
        self->string = string;
        self->value = value;
        return self;
    }
    return NULL;
}

char*
RR_Char_Value(RR_Char* self)
{
    return self->value;
}

void
RR_Char_Kill(RR_Char* self)
{
    RR_Value_Kill(self->string);
    Free(self);
}

char*
RR_Type_ToString(RR_Type self)
{
    switch(self)
    {
    case TYPE_FILE:
        return "file";
    case TYPE_FUNCTION:
        return "function";
    case TYPE_QUEUE:
        return "queue";
    case TYPE_CHAR:
        return "char";
    case TYPE_MAP:
        return "map";
    case TYPE_STRING:
        return "string";
    case TYPE_NUMBER:
        return "number";
    case TYPE_BOOL:
        return "bool";
    case TYPE_NULL:
        return "null";
    }
    return "N/A";
}

static void
Type_Kill(RR_Type type, Of* of)
{
    switch(type)
    {
    case TYPE_FILE:
        RR_File_Kill(of->file);
        break;
    case TYPE_FUNCTION:
        RR_Function_Kill(of->function);
        break;
    case TYPE_QUEUE:
        RR_Queue_Kill(of->queue);
        break;
    case TYPE_MAP:
        RR_Map_Kill(of->map);
        break;
    case TYPE_STRING:
        RR_String_Kill(of->string);
        break;
    case TYPE_CHAR:
        RR_Char_Kill(of->character);
        break;
    case TYPE_NUMBER:
    case TYPE_BOOL:
    case TYPE_NULL:
        break;
    }
}

void
RR_Type_Copy(RR_Value* copy, RR_Value* self)
{
    copy->type = self->type;
    switch(self->type)
    {
    case TYPE_FILE:
        copy->of.file = RR_File_Copy(self->of.file);
        break;
    case TYPE_FUNCTION:
        copy->of.function = RR_Function_Copy(self->of.function);
        break;
    case TYPE_QUEUE:
        copy->of.queue = RR_Queue_Copy(self->of.queue);
        break;
    case TYPE_MAP:
        copy->of.map = RR_Map_Copy(self->of.map);
        break;
    case TYPE_STRING:
        copy->of.string = RR_String_Copy(self->of.string);
        break;
    case TYPE_CHAR:
        copy->type = TYPE_STRING; // Chars promote to strings on copy.
        copy->of.string = String_FromChar(*self->of.character->value);
        break;
    case TYPE_NUMBER:
    case TYPE_BOOL:
    case TYPE_NULL:
        copy->of = self->of;
        break;
    }
}

#ifdef RR_MAIN

static void
CC_Expression(CC*);

static bool
CC_Factor(CC*);

static void
CC_Block(CC*, size_t head, size_t tail, bool loop);

static int64_t
VM_Run(VM*, bool arbitrary);

static char*
Class_ToString(Class self)
{
    switch(self)
    {
    case CLASS_VARIABLE_GLOBAL:
        return "global";
    case CLASS_VARIABLE_LOCAL:
        return "local";
    case CLASS_FUNCTION:
        return "function";
    case CLASS_FUNCTION_PROTOTYPE:
        return "function prototype";
    case CLASS_FUNCTION_PROTOTYPE_NATIVE:
        return "native function prototype";
    }
    return "N/A";
}

static size_t*
Int_Init(size_t value)
{
    size_t* self = Malloc(sizeof(*self));
    *self = value;
    return self;
}

static void
Int_Kill(size_t* self)
{
    Free(self);
}

static Frame*
Frame_Init(size_t pc, size_t sp, size_t address)
{
    Frame* self = Malloc(sizeof(*self));
    self->pc = pc;
    self->sp = sp;
    self->address = address;
    return self;
}

static void
Frame_Free(Frame* self)
{
    Free(self);
}

static Meta*
Meta_Init(Class class, size_t stack, RR_String* path)
{
    Meta* self = Malloc(sizeof(*self));
    self->class = class;
    self->stack = stack;
    self->path = path;
    return self;
}

static void
Meta_Kill(Meta* self)
{
    RR_String_Kill(self->path);
    Free(self);
}

static void
Module_Buffer(Module* self)
{
    self->index = 0;
    self->size = fread(self->buffer, sizeof(*self->buffer), MODULE_BUFFER_SIZE, self->file);
}

static Module*
Module_Init(RR_String* name)
{
    FILE* file = fopen(name->value, "r");
    if(file)
    {
        Module* self = Malloc(sizeof(*self));
        self->file = file;
        self->line = 1;
        self->name = RR_String_Copy(name);
        Module_Buffer(self);
        return self;
    }
    return NULL;
}

static void
Module_Kill(Module* self)
{
    RR_String_Kill(self->name);
    fclose(self->file);
    Free(self);
}

static size_t
Module_At(Module* self)
{
    return self->buffer[self->index];
}

static size_t
Module_Peak(Module* self)
{
    if(self->index == self->size)
        Module_Buffer(self);
    return self->size == 0 ? EOF : Module_At(self);
}

static void
Module_Advance(Module* self)
{
    size_t at = Module_At(self);
    if(at == '\n')
        self->line += 1;
    self->index += 1;
}

static void
Quit(const char* const message, ...)
{
    va_list args;
    va_start(args, message);
    fprintf(stderr, "error: ");
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(0xFF);
}

static void
CC_Quit(CC* self, const char* const message, ...)
{
    Module* back = RR_Queue_Back(self->modules);
    va_list args;
    va_start(args, message);
    fprintf(stderr, "error: file %s: line %lu: ", back ? back->name->value : "?", back ? back->line : 0);
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(0xFE);
}

static bool
CC_String_IsUpper(int64_t c)
{
    return c >= 'A' && c <= 'Z';
}

static bool
CC_String_IsLower(int64_t c)
{
    return c >= 'a' && c <= 'z';
}

static bool
CC_String_IsAlpha(int64_t c)
{
    return CC_String_IsLower(c) || CC_String_IsUpper(c);
}

static bool
CC_String_IsDigit(int64_t c)
{
    return c >= '0' && c <= '9';
}

static bool
CC_String_IsNumber(int64_t c)
{
    return CC_String_IsDigit(c) || c == '.';
}

static bool
CC_String_IsIdentLeader(int64_t c)
{
    return CC_String_IsAlpha(c) || c == '_';
}

static bool
CC_String_IsIdent(int64_t c)
{
    return CC_String_IsIdentLeader(c) || CC_String_IsDigit(c);
}

static bool
CC_String_IsModule(int64_t c)
{
    return CC_String_IsIdent(c) || c == '.';
}

static bool
CC_String_IsOp(int64_t c)
{
    return c == '*' || c == '/' || c == '%' || c == '+' || c == '-' || c == '='
        || c == '<' || c == '>' || c == '!' || c == '&' || c == '|' || c == '?';
}

static bool
CC_String_IsSpace(int64_t c)
{
    return c == '\n' || c == '\t' || c == '\r' || c == ' ';
}

static void
CC_Advance(CC* self)
{
    Module_Advance(RR_Queue_Back(self->modules));
}

static int64_t
CC_Peak(CC* self)
{
    int64_t peak;
    while(true)
    {
        peak = Module_Peak(RR_Queue_Back(self->modules));
        if(peak == EOF)
        {
            if(self->modules->size == 1)
                return EOF;
            RR_Queue_PopB(self->modules);
        }
        else
            break;
    }
    return peak;
}

static void
CC_Spin(CC* self)
{
    bool comment = false;
    while(true)
    {
        int64_t peak = CC_Peak(self);
        if(peak == '#')
            comment = true;
        if(peak == '\n')
            comment = false;
        if(CC_String_IsSpace(peak) || comment)
            CC_Advance(self);
        else
            break;
    }
}

static int64_t
CC_Next(CC* self)
{
    CC_Spin(self);
    return CC_Peak(self);
}

static int64_t
CC_Read(CC* self)
{
    int64_t peak = CC_Peak(self);
    if(peak != EOF)
        CC_Advance(self);
    return peak;
}

static RR_String*
CC_Stream(CC* self, bool clause(int64_t ))
{
    RR_String* str = RR_String_Init("");
    CC_Spin(self);
    while(clause(CC_Peak(self)))
        RR_String_PshB(str, CC_Read(self));
    return str;
}

static void
CC_Match(CC* self, char* expect)
{
    CC_Spin(self);
    while(*expect)
    {
        int64_t peak = CC_Read(self);
        if(peak != *expect)
        {
            char formatted[] = { peak, '\0' };
            CC_Quit(self, "matched character %s but expected character %c", (peak == EOF) ? "EOF" : formatted, *expect);
        }
        expect += 1;
    }
}

static int64_t
CC_String_EscToByte(int64_t ch)
{
    switch(ch)
    {
    case '"':
        return '\"';
    case '\\':
        return '\\';
    case '/':
        return '/';
    case 'b':
        return '\b';
    case 'f':
        return '\f';
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    }
    return -1;
}

static RR_String*
CC_Mod(CC* self)
{
    return CC_Stream(self, CC_String_IsModule);
}

static RR_String*
CC_Ident(CC* self)
{
    return CC_Stream(self, CC_String_IsIdent);
}

static RR_String*
CC_Operator(CC* self)
{
    return CC_Stream(self, CC_String_IsOp);
}

static RR_String*
CC_Number(CC* self)
{
    return CC_Stream(self, CC_String_IsNumber);
}

static RR_String*
CC_StringStream(CC* self)
{
    RR_String* str = RR_String_Init("");
    CC_Spin(self);
    CC_Match(self, "\"");
    while(CC_Peak(self) != '"')
    {
        int64_t ch = CC_Read(self);
        RR_String_PshB(str, ch);
        if(ch == '\\')
        {
            ch = CC_Read(self);
            int64_t byte = CC_String_EscToByte(ch);
            if(byte == -1)
                CC_Quit(self, "an unknown escape char 0x%02X was encountered", ch);
            RR_String_PshB(str, ch);
        }
    }
    CC_Match(self, "\"");
    return str;
}

static Debug*
Debug_Init(char* file, size_t line)
{
    Debug* self = Malloc(sizeof(*self));
    self->file = file;
    self->line = line;
    return self;
}

static void
Debug_Kill(Debug* self)
{
    Free(self);
}

static CC*
CC_Init(void)
{
    CC* self = Malloc(sizeof(*self));
    self->modules = RR_Queue_Init((RR_Kill) Module_Kill, (RR_Copy) NULL);
    self->assembly = RR_Queue_Init((RR_Kill) RR_String_Kill, (RR_Copy) NULL);
    self->data = RR_Queue_Init((RR_Kill) RR_String_Kill, (RR_Copy) NULL);
    self->debug = RR_Queue_Init((RR_Kill) Debug_Kill, (RR_Copy) NULL);
    self->files = RR_Map_Init((RR_Kill) NULL, (RR_Copy) NULL);
    self->included = RR_Map_Init((RR_Kill) NULL, (RR_Copy) NULL);
    self->identifiers = RR_Map_Init((RR_Kill) Meta_Kill, (RR_Copy) NULL);
    self->globals = 0;
    self->locals = 0;
    self->labels = 0;
    self->prime = NULL;
    return self;
}

static void
CC_Kill(CC* self)
{
    RR_Queue_Kill(self->modules);
    RR_Queue_Kill(self->assembly);
    RR_Queue_Kill(self->data);
    RR_Queue_Kill(self->debug);
    RR_Map_Kill(self->included);
    RR_Map_Kill(self->files);
    RR_Map_Kill(self->identifiers);
    Free(self);
}

static char*
CC_RealPath(CC* self, RR_String* file)
{
    RR_String* path;
    if(self->modules->size == 0)
    {
        path = RR_String_Init("");
        RR_String_Resize(path, 4096);
        getcwd(path->value, path->size);
        strcat(path->value, "/");
        strcat(path->value, file->value);
    }
    else
    {
        Module* back = RR_Queue_Back(self->modules);
        path = RR_String_Base(back->name);
        RR_String_Appends(path, file->value);
    }
    char* real = realpath(path->value, NULL);
    if(real == NULL)
        CC_Quit(self, "%s could not be resolved as a real path", path->value);
    RR_String_Kill(path);
    return real;
}

static void
CC_IncludeModule(CC* self, RR_String* file)
{
    char* real = CC_RealPath(self, file);
    if(RR_Map_Exists(self->included, real))
        Free(real);
    else
    {
        RR_String* resolved = RR_String_Moves(&real);
        RR_Map_Set(self->included, resolved, NULL);
        RR_Queue_PshB(self->modules, Module_Init(resolved));
    }
}

static RR_String*
CC_Parents(RR_String* module)
{
    RR_String* parents = RR_String_Init("");
    for(char* value = module->value; *value; value++)
    {
        if(*value != '.')
            break;
        RR_String_Appends(parents, "../");
    }
    return parents;
}

static RR_String*
String_Skip(RR_String* self, char c)
{
    char* value = self->value;
    while(*value)
    {
        if(*value != c)
            break;
        value += 1;
    }
    return RR_String_Init(value);
}

static RR_String*
Module_Name(CC* self, char* postfix)
{
    RR_String* module = CC_Mod(self);
    RR_String* skipped = String_Skip(module, '.');
    RR_String_Replace(skipped, '.', '/');
    RR_String_Appends(skipped, postfix);
    RR_String* name = CC_Parents(module);
    RR_String_Appends(name, skipped->value);
    RR_String_Kill(skipped);
    RR_String_Kill(module);
    return name;
}

static void
CC_Include(CC* self)
{
    RR_String* name = Module_Name(self, ".rr");
    CC_Match(self, ";");
    CC_IncludeModule(self, name);
    RR_String_Kill(name);
}

static bool
CC_IsGlobal(Class class)
{
    return class == CLASS_VARIABLE_GLOBAL;
}

static bool
CC_IsLocal(Class class)
{
    return class == CLASS_VARIABLE_LOCAL;
}

static bool
CC_IsVariable(Class class)
{
    return CC_IsGlobal(class) || CC_IsLocal(class);
}

static bool
CC_IsFunction(Class class)
{
    return class == CLASS_FUNCTION
        || class == CLASS_FUNCTION_PROTOTYPE
        || class == CLASS_FUNCTION_PROTOTYPE_NATIVE;
}

static void
CC_Define(CC* self, Class class, size_t stack, RR_String* ident, RR_String* path)
{
    Meta* old = RR_Map_Get(self->identifiers, ident->value);
    Meta* new = Meta_Init(class, stack, path);
    if(old)
    {
        // Only function prototypes can be upgraded to functions.
        if(old->class == CLASS_FUNCTION_PROTOTYPE
        && new->class == CLASS_FUNCTION)
        {
            if(new->stack != old->stack)
                CC_Quit(self, "function %s with %lu argument(s) was previously defined as a function prototype with %lu argument(s)", ident->value, new->stack, old->stack);
        }
        else
            CC_Quit(self, "%s %s was already defined as a %s", Class_ToString(new->class), ident->value, Class_ToString(old->class));
    }
    RR_Map_Set(self->identifiers, ident, new);
}

static void
CC_Assem(CC* self, RR_String* assem)
{
    RR_Queue_PshB(self->assembly, assem);
    if(assem->value[0] == '\t')
    {
        Module* back = RR_Queue_Back(self->modules);
        Debug* debug;
        Node* at = RR_Map_At(self->files, back->name->value);
        if(at)
        {
            RR_String* string = at->key;
            debug = Debug_Init(string->value, back->line);
        }
        else
        {
            RR_String* name = RR_String_Copy(back->name);
            RR_Map_Set(self->files, name, NULL);
            debug = Debug_Init(name->value, back->line);
        }
        RR_Queue_PshB(self->debug, debug);
    }
}

static void
CC_ConsumeExpression(CC* self)
{
    CC_Expression(self);
    CC_Assem(self, RR_String_Init("\tpop"));
}

static void
CC_EmptyExpression(CC* self)
{
    CC_ConsumeExpression(self);
    CC_Match(self, ";");
}

static void
CC_Assign(CC* self)
{
    CC_Match(self, ":=");
    CC_Expression(self);
    CC_Match(self, ";");
}

static void
CC_Local(CC* self, RR_String* ident)
{
    CC_Define(self, CLASS_VARIABLE_LOCAL, self->locals, ident, RR_String_Init(""));
    self->locals += 1;
}

static void
CC_AssignLocal(CC* self, RR_String* ident)
{
    CC_Assign(self);
    CC_Local(self, ident);
}

static RR_String*
CC_Global(CC* self, RR_String* ident)
{
    RR_String* label = RR_String_Format("!%s", ident->value);
    CC_Assem(self, RR_String_Format("%s:", label->value));
    CC_Assign(self);
    CC_Define(self, CLASS_VARIABLE_GLOBAL, self->globals, ident, RR_String_Init(""));
    CC_Assem(self, RR_String_Init("\tret"));
    self->globals += 1;
    return label;
}

static RR_Queue*
CC_ParamRoll(CC* self)
{
    RR_Queue* params = RR_Queue_Init((RR_Kill) RR_String_Kill, (RR_Copy) NULL);
    CC_Match(self, "(");
    while(CC_Next(self) != ')')
    {
        RR_String* ident = CC_Ident(self);
        RR_Queue_PshB(params, ident);
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    CC_Match(self, ")");
    return params;
}

static void
CC_DefineParams(CC* self, RR_Queue* params)
{
    self->locals = 0;
    for(size_t i = 0; i < params->size; i++)
    {
        RR_String* ident = RR_String_Copy(RR_Queue_Get(params, i));
        CC_Local(self, ident);
    }
}

static size_t
CC_PopScope(CC* self, RR_Queue* scope)
{
    size_t popped = scope->size;
    for(size_t i = 0; i < popped; i++)
    {
        RR_String* key = RR_Queue_Get(scope, i);
        RR_Map_Del(self->identifiers, key->value);
        self->locals -= 1;
    }
    for(size_t i = 0; i < popped; i++)
        CC_Assem(self, RR_String_Init("\tpop"));
    RR_Queue_Kill(scope);
    return popped;
}

static Meta*
CC_Meta(CC* self, RR_String* ident)
{
    Meta* meta = RR_Map_Get(self->identifiers, ident->value);
    if(meta == NULL)
        CC_Quit(self, "identifier %s not defined", ident->value);
    return meta;
}

static Meta*
CC_Expect(CC* self, RR_String* ident, bool clause(Class))
{
    Meta* meta = CC_Meta(self, ident);
    if(!clause(meta->class))
        CC_Quit(self, "identifier %s cannot be of class %s", ident->value, Class_ToString(meta->class));
    return meta;
}

static void
CC_Ref(CC* self, RR_String* ident)
{
    Meta* meta = CC_Expect(self, ident, CC_IsVariable);
    if(meta->class == CLASS_VARIABLE_GLOBAL)
        CC_Assem(self, RR_String_Format("\tglb %lu", meta->stack));
    else
    if(meta->class == CLASS_VARIABLE_LOCAL)
        CC_Assem(self, RR_String_Format("\tloc %lu", meta->stack));
}

static void
CC_String(CC* self)
{
    RR_String* string = CC_StringStream(self);
    CC_Assem(self, RR_String_Format("\tpsh \"%s\"", string->value));
    RR_String_Kill(string);
}

static void
CC_Resolve(CC* self)
{
    while(CC_Next(self) == '['
       || CC_Next(self) == '.')
    {
        if(CC_Next(self) == '[')
        {
            CC_Match(self, "[");
            CC_Expression(self);
            CC_Match(self, "]");
        }
        else
        if(CC_Next(self) == '.')
        {
            CC_Match(self, ".");
            RR_String* ident = CC_Ident(self);
            CC_Assem(self, RR_String_Format("\tpsh \"%s\"", ident->value));
            RR_String_Kill(ident);
        }
        if(CC_Next(self) == ':')
        {
            CC_Match(self, ":=");
            CC_Expression(self);
            CC_Assem(self, RR_String_Init("\tcpy"));
            CC_Assem(self, RR_String_Init("\tins"));
        }
        else
            CC_Assem(self, RR_String_Init("\tget"));
    }
}

static size_t
CC_Args(CC* self)
{
    CC_Match(self, "(");
    size_t args = 0;
    while(CC_Next(self) != ')')
    {
        CC_Expression(self);
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
        args += 1;
    }
    CC_Match(self, ")");
    return args;
}

static void
CC_Call(CC* self, RR_String* ident, size_t args)
{
    for(size_t i = 0; i < args; i++)
        CC_Assem(self, RR_String_Init("\tspd"));
    CC_Assem(self, RR_String_Format("\tcal %s", ident->value));
    CC_Assem(self, RR_String_Init("\tlod"));
}

static void
CC_IndirectCalling(CC* self, RR_String* ident, size_t args)
{
    CC_Ref(self, ident);
    CC_Assem(self, RR_String_Format("\tpsh %lu", args));
    CC_Assem(self, RR_String_Init("\tvrt"));
    CC_Assem(self, RR_String_Init("\tlod"));
}

static void
CC_NativeCalling(CC* self, RR_String* ident, Meta* meta)
{
    CC_Assem(self, RR_String_Format("\tpsh \"%s\"", meta->path->value));
    CC_Assem(self, RR_String_Format("\tpsh \"%s\"", ident->value));
    CC_Assem(self, RR_String_Format("\tpsh %lu", meta->stack));
    CC_Assem(self, RR_String_Init("\tdll"));
}

static void
CC_Map(CC* self)
{
    CC_Assem(self, RR_String_Init("\tpsh {}"));
    CC_Match(self, "{");
    while(CC_Next(self) != '}')
    {
        CC_Expression(self);
        CC_Assem(self, RR_String_Init("\tcpy"));
        if(CC_Next(self) == ':')
        {
            CC_Match(self, ":");
            CC_Expression(self);
        }
        else
            CC_Assem(self, RR_String_Init("\tpsh true")); // No values default to true for quick to build string sets.
        CC_Assem(self, RR_String_Init("\tcpy"));
        CC_Assem(self, RR_String_Init("\tins"));
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    CC_Match(self, "}");
}

static void
CC_Queue(CC* self)
{
    CC_Assem(self, RR_String_Init("\tpsh []"));
    CC_Match(self, "[");
    while(CC_Next(self) != ']')
    {
        CC_Expression(self);
        CC_Assem(self, RR_String_Init("\tcpy"));
        CC_Assem(self, RR_String_Init("\tpsb"));
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    CC_Match(self, "]");
}

static void
CC_Not(CC* self)
{
    CC_Match(self, "!");
    CC_Factor(self);
    CC_Assem(self, RR_String_Init("\tnot"));
}

static void
CC_Direct(CC* self, bool negative)
{
    RR_String* number = CC_Number(self);
    CC_Assem(self, RR_String_Format("\tpsh %s%s", negative ? "-" : "", number->value));
    RR_String_Kill(number);
}

static void
CC_DirectCalling(CC* self, RR_String* ident, size_t args)
{
    if(RR_String_Equals(ident, "Len"))
        CC_Assem(self, RR_String_Init("\tlen"));
    else
    if(RR_String_Equals(ident, "Good"))
        CC_Assem(self, RR_String_Init("\tgod"));
    else
    if(RR_String_Equals(ident, "Key"))
        CC_Assem(self, RR_String_Init("\tkey"));
    else
    if(RR_String_Equals(ident, "Sort"))
        CC_Assem(self, RR_String_Init("\tsrt"));
    else
    if(RR_String_Equals(ident, "Open"))
        CC_Assem(self, RR_String_Init("\topn"));
    else
    if(RR_String_Equals(ident, "Read"))
        CC_Assem(self, RR_String_Init("\tred"));
    else
    if(RR_String_Equals(ident, "Copy"))
        CC_Assem(self, RR_String_Init("\tcpy"));
    else
    if(RR_String_Equals(ident, "Write"))
        CC_Assem(self, RR_String_Init("\twrt"));
    else
    if(RR_String_Equals(ident, "Refs"))
        CC_Assem(self, RR_String_Init("\tref"));
    else
    if(RR_String_Equals(ident, "Del"))
        CC_Assem(self, RR_String_Init("\tdel"));
    else
    if(RR_String_Equals(ident, "Exit"))
        CC_Assem(self, RR_String_Init("\text"));
    else
    if(RR_String_Equals(ident, "Type"))
        CC_Assem(self, RR_String_Init("\ttyp"));
    else
    if(RR_String_Equals(ident, "Print"))
        CC_Assem(self, RR_String_Init("\tprt"));
    else
        CC_Call(self, ident, args);
}

static void
CC_Calling(CC* self, RR_String* ident)
{
    Meta* meta = CC_Meta(self, ident);
    size_t args = CC_Args(self);
    if(CC_IsFunction(meta->class))
    {
        if(args != meta->stack)
            CC_Quit(self, "calling function %s required %lu arguments but got %lu arguments", ident->value, meta->stack, args);
        if(meta->class == CLASS_FUNCTION_PROTOTYPE_NATIVE)
            CC_NativeCalling(self, ident, meta);
        else
            CC_DirectCalling(self, ident, args);
    }
    else
    if(CC_IsVariable(meta->class))
        CC_IndirectCalling(self, ident, args);
    else
        CC_Quit(self, "identifier %s is not callable", ident->value);
}

static bool
CC_Referencing(CC* self, RR_String* ident)
{
    Meta* meta = CC_Meta(self, ident);
    if(CC_IsFunction(meta->class))
        CC_Assem(self, RR_String_Format("\tpsh @%s,%lu", ident->value, meta->stack));
    else
    {
        CC_Ref(self, ident);
        return true;
    }
    return false;
}

static bool
CC_Identifier(CC* self)
{
    bool storage = false;
    RR_String* ident = NULL;
    if(self->prime)
        RR_String_Swap(&self->prime, &ident);
    else
        ident = CC_Ident(self);
    if(String_IsBoolean(ident))
        CC_Assem(self, RR_String_Format("\tpsh %s", ident->value));
    else
    if(String_IsNull(ident))
        CC_Assem(self, RR_String_Format("\tpsh %s", ident->value));
    else
    if(CC_Next(self) == '(')
        CC_Calling(self, ident);
    else
        storage = CC_Referencing(self, ident);
    RR_String_Kill(ident);
    return storage;
}

static void
CC_ReserveFunctions(CC* self)
{
    struct { size_t args; char* name; } items[] = {
        { 2, "Sort"  },
        { 1, "Good"  },
        { 1, "Key"   },
        { 1, "Copy"  },
        { 2, "Open"  },
        { 2, "Read"  },
        { 1, "Close" },
        { 1, "Refs"  },
        { 1, "Len"   },
        { 2, "Del"   },
        { 1, "Exit"  },
        { 1, "Type"  },
        { 1, "Print" },
        { 2, "Write" },
    };
    for(size_t i = 0; i < RR_LEN(items); i++)
        CC_Define(self, CLASS_FUNCTION, items[i].args, RR_String_Init(items[i].name), RR_String_Init(""));
}

static void
CC_Force(CC* self)
{
    CC_Match(self, "(");
    CC_Expression(self);
    CC_Match(self, ")");
}

static void
CC_DirectNeg(CC* self)
{
    CC_Match(self, "-");
    CC_Direct(self, true);
}

static void
CC_DirectPos(CC* self)
{
    CC_Match(self, "+");
    CC_Direct(self, false);
}

static bool
CC_Factor(CC* self)
{
    bool storage = false;
    int64_t next = CC_Next(self);
    if(next == '!')
        CC_Not(self);
    else
    if(CC_String_IsDigit(next))
        CC_Direct(self, false);
    else
    if(CC_String_IsIdent(next) || self->prime)
        storage = CC_Identifier(self);
    else
    if(next == '-')
        CC_DirectNeg(self);
    else
    if(next == '+')
        CC_DirectPos(self);
    else
    if(next == '(')
        CC_Force(self);
    else
    if(next == '{')
        CC_Map(self);
    else
    if(next == '[')
        CC_Queue(self);
    else
    if(next == '"')
        CC_String(self);
    else
        CC_Quit(self, "an unknown factor starting with %c was encountered", next);
    CC_Resolve(self);
    return storage;
}

static bool
CC_Term(CC* self)
{
    bool storage = CC_Factor(self);
    while(CC_Next(self) == '*'
       || CC_Next(self) == '/'
       || CC_Next(self) == '%'
       || CC_Next(self) == '?'
       || CC_Next(self) == '|')
    {
        RR_String* operator = CC_Operator(self);
        if(RR_String_Equals(operator, "*="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator %s must be storage", operator);
            CC_Expression(self);
            CC_Assem(self, RR_String_Init("\tmul"));
        }
        else
        if(RR_String_Equals(operator, "/="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator %s must be storage", operator);
            CC_Expression(self);
            CC_Assem(self, RR_String_Init("\tdiv"));
        }
        else
        {
            storage = false;
            if(RR_String_Equals(operator, "?"))
            {
                CC_Factor(self);
                CC_Assem(self, RR_String_Init("\tmem"));
            }
            else
            {
                CC_Assem(self, RR_String_Init("\tcpy"));
                CC_Factor(self);
                if(RR_String_Equals(operator, "*"))
                    CC_Assem(self, RR_String_Init("\tmul"));
                else
                if(RR_String_Equals(operator, "/"))
                    CC_Assem(self, RR_String_Init("\tdiv"));
                else
                if(RR_String_Equals(operator, "%"))
                    CC_Assem(self, RR_String_Init("\tfmt"));
                else
                if(RR_String_Equals(operator, "||"))
                    CC_Assem(self, RR_String_Init("\tlor"));
                else
                    CC_Quit(self, "operator %s is not supported for factors", operator->value);
            }
        }
        RR_String_Kill(operator);
    }
    return storage;
}

static void
CC_Expression(CC* self)
{
    bool storage = CC_Term(self);
    while(CC_Next(self) == '+' 
       || CC_Next(self) == '-'
       || CC_Next(self) == '='
       || CC_Next(self) == '!'
       || CC_Next(self) == '?'
       || CC_Next(self) == '>'
       || CC_Next(self) == '<'
       || CC_Next(self) == '&')
    {
        RR_String* operator = CC_Operator(self);
        if(RR_String_Equals(operator, "="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator %s must be storage", operator);
            CC_Expression(self);
            CC_Assem(self, RR_String_Init("\tcpy"));
            CC_Assem(self, RR_String_Init("\tmov"));
        }
        else
        if(RR_String_Equals(operator, "+="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator %s must be storage", operator);
            CC_Expression(self);
            CC_Assem(self, RR_String_Init("\tcpy"));
            CC_Assem(self, RR_String_Init("\tadd"));
        }
        else
        if(RR_String_Equals(operator, "-="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator %s must be storage", operator);
            CC_Expression(self);
            CC_Assem(self, RR_String_Init("\tcpy"));
            CC_Assem(self, RR_String_Init("\tsub"));
        }
        else
        if(RR_String_Equals(operator, "=="))
        {
            CC_Expression(self);
            CC_Assem(self, RR_String_Init("\teql"));
        }
        else
        if(RR_String_Equals(operator, "!="))
        {
            CC_Expression(self);
            CC_Assem(self, RR_String_Init("\tneq"));
        }
        else
        if(RR_String_Equals(operator, ">"))
        {
            CC_Expression(self);
            CC_Assem(self, RR_String_Init("\tgrt"));
        }
        else
        if(RR_String_Equals(operator, "<"))
        {
            CC_Expression(self);
            CC_Assem(self, RR_String_Init("\tlst"));
        }
        else
        if(RR_String_Equals(operator, ">="))
        {
            CC_Expression(self);
            CC_Assem(self, RR_String_Init("\tgte"));
        }
        else
        if(RR_String_Equals(operator, "<="))
        {
            CC_Expression(self);
            CC_Assem(self, RR_String_Init("\tlte"));
        }
        else
        {
            storage = false;
            CC_Assem(self, RR_String_Init("\tcpy"));
            CC_Term(self);
            if(RR_String_Equals(operator, "+"))
                CC_Assem(self, RR_String_Init("\tadd"));
            else
            if(RR_String_Equals(operator, "-"))
                CC_Assem(self, RR_String_Init("\tsub"));
            else
            if(RR_String_Equals(operator, "&&"))
                CC_Assem(self, RR_String_Init("\tand"));
            else
                CC_Quit(self, "operator %s is not supported for terms", operator->value);
        }
        RR_String_Kill(operator);
    }
}

static size_t
CC_Label(CC* self)
{
    size_t label = self->labels;
    self->labels += 1;
    return label;
}

static void
CC_Branch(CC* self, size_t head, size_t tail, size_t end, bool loop)
{
    size_t next = CC_Label(self);
    CC_Match(self, "(");
    CC_Expression(self);
    CC_Match(self, ")");
    CC_Assem(self, RR_String_Format("\tbrf @l%lu", next));
    CC_Block(self, head, tail, loop);
    CC_Assem(self, RR_String_Format("\tjmp @l%lu", end));
    CC_Assem(self, RR_String_Format("@l%lu:", next));
}

static RR_String*
CC_Branches(CC* self, size_t head, size_t tail, size_t loop)
{
    size_t end = CC_Label(self);
    CC_Branch(self, head, tail, end, loop);
    RR_String* buffer = CC_Ident(self);
    while(RR_String_Equals(buffer, "elif"))
    {
        CC_Branch(self, head, tail, end, loop);
        RR_String_Kill(buffer);
        buffer = CC_Ident(self);
    }
    if(RR_String_Equals(buffer, "else"))
        CC_Block(self, head, tail, loop);
    CC_Assem(self, RR_String_Format("@l%lu:", end));
    RR_String* backup = NULL;
    if(buffer->size == 0 || RR_String_Equals(buffer, "elif") || RR_String_Equals(buffer, "else"))
        RR_String_Kill(buffer);
    else
        backup = buffer;
    return backup;
}

static void
CC_While(CC* self)
{
    size_t A = CC_Label(self);
    size_t B = CC_Label(self);
    CC_Assem(self, RR_String_Format("@l%lu:", A));
    CC_Match(self, "(");
    CC_Expression(self);
    CC_Assem(self, RR_String_Format("\tbrf @l%lu", B));
    CC_Match(self, ")");
    CC_Block(self, A, B, true);
    CC_Assem(self, RR_String_Format("\tjmp @l%lu", A));
    CC_Assem(self, RR_String_Format("@l%lu:", B));
}

static void
CC_For(CC* self)
{
    size_t A = CC_Label(self);
    size_t B = CC_Label(self);
    size_t C = CC_Label(self);
    size_t D = CC_Label(self);
    RR_Queue* init = RR_Queue_Init((RR_Kill) RR_String_Kill, (RR_Copy) NULL);
    CC_Match(self, "(");
    RR_String* ident = CC_Ident(self);
    RR_Queue_PshB(init, RR_String_Copy(ident));
    CC_AssignLocal(self, ident);
    CC_Assem(self, RR_String_Format("@l%lu:", A));
    CC_Expression(self);
    CC_Match(self, ";");
    CC_Assem(self, RR_String_Format("\tbrf @l%lu", D));
    CC_Assem(self, RR_String_Format("\tjmp @l%lu", C));
    CC_Assem(self, RR_String_Format("@l%lu:", B));
    CC_ConsumeExpression(self);
    CC_Match(self, ")");
    CC_Assem(self, RR_String_Format("\tjmp @l%lu", A));
    CC_Assem(self, RR_String_Format("@l%lu:", C));
    CC_Block(self, B, D, true);
    CC_Assem(self, RR_String_Format("\tjmp @l%lu", B));
    CC_Assem(self, RR_String_Format("@l%lu:", D));
    CC_PopScope(self, init);
}

static void
CC_Ret(CC* self)
{
    CC_Expression(self);
    CC_Assem(self, RR_String_Init("\tsav"));
    CC_Assem(self, RR_String_Init("\tfls"));
    CC_Match(self, ";");
}

static void
CC_Block(CC* self, size_t head, size_t tail, bool loop)
{
    RR_Queue* scope = RR_Queue_Init((RR_Kill) RR_String_Kill, (RR_Copy) NULL);
    CC_Match(self, "{");
    RR_String* prime = NULL; 
    while(CC_Next(self) != '}')
    {
        if(CC_String_IsIdentLeader(CC_Next(self)) || prime)
        {
            RR_String* ident = NULL;
            if(prime)
                RR_String_Swap(&prime, &ident);
            else
                ident = CC_Ident(self);
            if(RR_String_Equals(ident, "if"))
                prime = CC_Branches(self, head, tail, loop);
            else
            if(RR_String_Equals(ident, "elif"))
                CC_Quit(self, "the keyword elif must follow an if or elif block");
            else
            if(RR_String_Equals(ident, "else"))
                CC_Quit(self, "the keyword else must follow an if or elif block");
            else
            if(RR_String_Equals(ident, "while"))
                CC_While(self);
            else
            if(RR_String_Equals(ident, "for"))
                CC_For(self);
            else
            if(RR_String_Equals(ident, "ret"))
                CC_Ret(self);
            else
            if(RR_String_Equals(ident, "continue"))
            {
                if(loop)
                {
                    CC_Match(self, ";");
                    CC_Assem(self, RR_String_Format("\tjmp @l%lu", head));
                }
                else
                    CC_Quit(self, "the keyword continue can only be used within while or for loops");
            }
            else
            if(RR_String_Equals(ident, "break"))
            {
                if(loop)
                {
                    CC_Match(self, ";");
                    CC_Assem(self, RR_String_Format("\tjmp @l%lu", tail));
                }
                else
                    CC_Quit(self, "the keyword break can only be used within while or for loops");
            }
            else
            if(CC_Next(self) == ':')
            {
                RR_Queue_PshB(scope, RR_String_Copy(ident));
                CC_AssignLocal(self, RR_String_Copy(ident));
            }
            else
            {
                self->prime = RR_String_Copy(ident);
                CC_EmptyExpression(self);
            }
            RR_String_Kill(ident);
        }
        else
            CC_EmptyExpression(self);
    }
    CC_Match(self, "}");
    CC_PopScope(self, scope);
}

static void
CC_FunctionPrototypeNative(CC* self, RR_Queue* params, RR_String* ident, RR_String* path)
{
    CC_Define(self, CLASS_FUNCTION_PROTOTYPE_NATIVE, params->size, ident, path);
    RR_Queue_Kill(params);
    CC_Match(self, ";");
}

static void
CC_FunctionPrototype(CC* self, RR_Queue* params, RR_String* ident)
{
    CC_Define(self, CLASS_FUNCTION_PROTOTYPE, params->size, ident, RR_String_Init(""));
    RR_Queue_Kill(params);
    CC_Match(self, ";");
}

static RR_String*
CC_GetModuleLibs(RR_String* source)
{
    RR_File* file = RR_File_Init(RR_String_Copy(source), RR_String_Init("r"));
    FILE* fp = RR_File_File(file);
    char* line = NULL;
    size_t len = 0;
    getline(&line, &len, fp);
    RR_File_Kill(file);
    size_t expect = 3;
    if(len > expect)
    {
        if(line[0] == '/'
        && line[1] == '/'
        && line[2] == '!')
        {
            RR_String* libs = RR_String_Init(line + expect);
            Free(line);
            return libs;
        }
    }
    Free(line);
    return RR_String_Init("");;
}

static void
CC_ImportModule(CC* self, RR_String* file)
{
    char* real = CC_RealPath(self, file);
    RR_String* source = RR_String_Init(real);
    RR_String* library = RR_String_Init(real);
    RR_String_PopB(library);
    RR_String_Appends(library, "so");
    if(RR_Map_Exists(self->included, library->value) == false)
    {
        RR_String* libs = CC_GetModuleLibs(source);
        RR_String* command = RR_String_Format("gcc -std=gnu99 -O3 -march=native -Wall -Wextra -Wpedantic %s -fpic -shared -o %s -l%s %s", source->value, library->value, RR_LIBROMAN2, libs->value);
        int64_t ret = system(command->value);
        if(ret != 0)
            CC_Quit(self, "compilation errors encountered while compiling native library %s", source->value);
        RR_String_Kill(command);
        RR_String_Kill(libs);
        RR_Map_Set(self->included, RR_String_Copy(library), NULL);
    }
    CC_Match(self, "{");
    while(CC_Next(self) != '}')
    {
        RR_String* ident = CC_Ident(self);
        RR_Queue* params = CC_ParamRoll(self);
        CC_FunctionPrototypeNative(self, params, ident, RR_String_Copy(library));
    }
    CC_Match(self, "}");
    CC_Match(self, ";");
    RR_String_Kill(library);
    RR_String_Kill(source);
    Free(real);
}

static void
CC_Import(CC* self)
{
    RR_String* name = Module_Name(self, ".c");
    CC_ImportModule(self, name);
    RR_String_Kill(name);
}

static void
CC_Function(CC* self, RR_String* ident)
{
    RR_Queue* params = CC_ParamRoll(self);
    if(CC_Next(self) == '{')
    {
        CC_DefineParams(self, params);
        CC_Define(self, CLASS_FUNCTION, params->size, ident, RR_String_Init(""));
        CC_Assem(self, RR_String_Format("%s:", ident->value));
        CC_Block(self, 0, 0, false);
        CC_PopScope(self, params);
        CC_Assem(self, RR_String_Init("\tpsh null"));
        CC_Assem(self, RR_String_Init("\tsav"));
        CC_Assem(self, RR_String_Init("\tret"));
    }
    else
        CC_FunctionPrototype(self, params, ident);
}

static void
CC_Spool(CC* self, RR_Queue* start)
{
    RR_Queue* spool = RR_Queue_Init((RR_Kill) RR_String_Kill, NULL);
    RR_String* main = RR_String_Init("Main");
    CC_Expect(self, main, CC_IsFunction);
    RR_Queue_PshB(spool, RR_String_Init("!start:"));
    for(size_t i = 0; i < start->size; i++)
    {
        RR_String* label = RR_Queue_Get(start, i);
        RR_Queue_PshB(spool, RR_String_Format("\tcal %s", label->value));
    }
    RR_Queue_PshB(spool, RR_String_Format("\tcal %s", main->value));
    RR_Queue_PshB(spool, RR_String_Format("\tend"));
    size_t i = spool->size;
    while(i != 0)
    {
        i -= 1;
        RR_Queue_PshF(self->assembly, RR_String_Copy(RR_Queue_Get(spool, i)));
        RR_Queue_PshF(self->debug, Debug_Init("N/A", -1));
    }
    RR_String_Kill(main);
    RR_Queue_Kill(spool);
}

static void
CC_Parse(CC* self)
{
    RR_Queue* start = RR_Queue_Init((RR_Kill) RR_String_Kill, (RR_Copy) NULL);
    while(CC_Peak(self) != EOF)
    {
        RR_String* ident = CC_Ident(self);
        if(RR_String_Equals(ident, "inc"))
        {
            CC_Include(self);
            RR_String_Kill(ident);
        }
        else
        if(RR_String_Equals(ident, "imp"))
        {
            CC_Import(self);
            RR_String_Kill(ident);
        }
        else
        if(CC_Next(self) == '(')
            CC_Function(self, ident);
        else
        if(CC_Next(self) == ':')
        {
            RR_String* label = CC_Global(self, ident);
            RR_Queue_PshB(start, label);
        }
        else
            CC_Quit(self, "%s must either be a function or function prototype, a global variable, an import statement, or an include statement", ident->value);
        CC_Spin(self);
    }
    CC_Spool(self, start);
    RR_Queue_Kill(start);
}

static Stack*
Stack_Init(RR_String* label, size_t address)
{
    Stack* self = Malloc(sizeof(*self));
    self->label = label;
    self->address = address;
    return self;
}

static void
Stack_Kill(Stack* self)
{
    RR_String_Kill(self->label);
    Free(self);
}

static bool
Stack_Compare(Stack* a, Stack* b)
{
    return a->address < b->address;
}

RR_Queue*
ASM_Flatten(RR_Map* labels)
{
    RR_Queue* addresses = RR_Queue_Init((RR_Kill) Stack_Kill, (RR_Copy) NULL);
    for(size_t i = 0; i < RR_Map_Buckets(labels); i++)
    {
        Node* chain = labels->bucket[i];
        while(chain)
        {
            size_t* address = RR_Map_Get(labels, chain->key->value);
            Stack* stack = Stack_Init(RR_String_Init(chain->key->value), *address);
            RR_Queue_PshB(addresses, stack);
            chain = chain->next;
        }
    }
    RR_Queue_Sort(addresses, (RR_Compare) Stack_Compare);
    return addresses;
}

static RR_Map*
ASM_Label(RR_Queue* assembly, size_t* size)
{
    RR_Map* labels = RR_Map_Init((RR_Kill) Free, (RR_Copy) NULL);
    for(size_t i = 0; i < assembly->size; i++)
    {
        RR_String* stub = RR_Queue_Get(assembly, i);
        if(stub->value[0] == '\t')
            *size += 1;
        else 
        {
            RR_String* line = RR_String_Copy(stub);
            char* label = strtok(line->value, ":");
            if(RR_Map_Exists(labels, label))
                Quit("asm: label %s already defined", label);
            RR_Map_Set(labels, RR_String_Init(label), Int_Init(*size));
            RR_String_Kill(line);
        }
    }
    return labels;
}

static void
ASM_Dump(RR_Queue* assembly)
{
    size_t instructions = 0;
    size_t labels = 0;
    for(size_t i = 0; i < assembly->size; i++)
    {
        RR_String* assem = RR_Queue_Get(assembly, i);
        if(assem->value[0] == '\t')
            instructions += 1;
        else
            labels += 1;
        puts(assem->value);
    }
    printf("instructions %lu : labels %lu\n", instructions, labels);
}

static void
VM_Quit(VM* self, const char* const message, ...)
{
    va_list args;
    Debug* debug = RR_Queue_Get(self->debug, self->pc);
    va_start(args, message);
    fprintf(stderr, "error: file %s: line %lu: ", debug->file, debug->line);
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
    for(size_t i = 0; i < self->frame->size - 1; i++)
    {
        Frame* a = RR_Queue_Get(self->frame, i + 0);
        for(size_t j = 0; j < self->addresses->size; j++) // Could be a binary search.
        {
            Stack* stack = RR_Queue_Get(self->addresses, j);
            if(a->address == stack->address)
                fprintf(stderr, "%s(...): ", stack->label->value);
        }
        Frame* b = RR_Queue_Get(self->frame, i + 1);
        Debug* sub = RR_Queue_Get(self->debug, b->pc);
        fprintf(stderr, "%s: line %lu\n", sub->file, sub->line);
    }
    exit(0xFE);
}

static void
VM_TypeExpect(VM* self, RR_Type a, RR_Type b)
{
    if(a != b)
        VM_Quit(self, "vm: encountered type %s but expected type %s", RR_Type_ToString(a), RR_Type_ToString(b));
}

static void
VM_BadOperator(VM* self, RR_Type a, const char* op)
{
    VM_Quit(self, "vm: type %s not supported with operator %s", RR_Type_ToString(a), op);
}

static void
VM_TypeMatch(VM* self, RR_Type a, RR_Type b, const char* op)
{
    if(a != b)
        VM_Quit(self, "vm: type %s and type %s mismatch with operator %s", RR_Type_ToString(a), RR_Type_ToString(b), op);
}

static void
VM_OutOfBounds(VM* self, RR_Type a, size_t index)
{
    VM_Quit(self, "vm: type %s was accessed out of bounds with index %lu", RR_Type_ToString(a), index);
}

static void
VM_TypeBadIndex(VM* self, RR_Type a, RR_Type b)
{
    VM_Quit(self, "vm: type %s cannot be indexed with type %s", RR_Type_ToString(a), RR_Type_ToString(b));
}

static void
VM_TypeBad(VM* self, RR_Type a)
{
    VM_Quit(self, "vm: type %s cannot be used for this operation", RR_Type_ToString(a));
}

static void
VM_ArgMatch(VM* self, size_t a, size_t b)
{
    if(a != b)
        VM_Quit(self, "vm: expected %lu arguments but encountered %lu arguments", a, b);
}

static void
VM_UnknownEscapeChar(VM* self, size_t esc)
{
    VM_Quit(self, "vm: an unknown escape character 0x%02X was encountered\n", esc);
}

static void
VM_RefImpurity(VM* self, RR_Value* value)
{
    RR_String* print = RR_Value_Sprint(value, true, 0);
    VM_Quit(self, "vm: the .data segment value %s contained %lu references at the time of exit", print->value, value->refs);
}

static VM*
VM_Init(size_t size, RR_Queue* debug, RR_Queue* addresses)
{
    VM* self = Malloc(sizeof(*self));
    self->data = RR_Queue_Init((RR_Kill) RR_Value_Kill, (RR_Copy) NULL);
    self->stack = RR_Queue_Init((RR_Kill) RR_Value_Kill, (RR_Copy) NULL);
    self->frame = RR_Queue_Init((RR_Kill) Frame_Free, (RR_Copy) NULL);
    self->track = RR_Map_Init((RR_Kill) Int_Kill, (RR_Copy) NULL);
    self->addresses = addresses;
    self->debug = debug;
    self->ret = NULL;
    self->size = size;
    self->instructions = Malloc(size * sizeof(*self->instructions));
    self->pc = 0;
    self->spds = 0;
    return self;
}

static void
VM_Kill(VM* self)
{
    RR_Queue_Kill(self->data);
    RR_Queue_Kill(self->stack);
    RR_Queue_Kill(self->frame);
    RR_Queue_Kill(self->addresses);
    RR_Map_Kill(self->track);
    Free(self->instructions);
    Free(self);
}

static void
VM_Data(VM* self)
{
    printf(".data:\n");
    for(size_t i = 0; i < self->data->size; i++)
    {
        RR_Value* value = RR_Queue_Get(self->data, i);
        printf("%2lu : %2lu : ", i, value->refs);
        RR_Value_Print(value);
    }
}

static void
VM_AssertRefCounts(VM* self)
{
    for(size_t i = 0; i < self->data->size; i++)
    {
        RR_Value* value = RR_Queue_Get(self->data, i);
        if(value->refs > 0)
            VM_RefImpurity(self, value);
    }
}

static void
VM_Pop(VM* self)
{
    RR_Queue_PopB(self->stack);
}

static RR_String*
VM_ConvertEscs(VM* self, char* chars)
{
    size_t len = strlen(chars);
    RR_String* string = RR_String_Init("");
    for(size_t i = 1; i < len - 1; i++)
    {
        char ch = chars[i];
        if(ch == '\\')
        {
            i += 1;
            size_t esc = chars[i];
            ch = CC_String_EscToByte(esc);
            if(ch == -1)
                VM_UnknownEscapeChar(self, esc);
        }
        RR_String_PshB(string, ch);
    }
    return string;
}

static void
VM_Store(VM* self, RR_Map* labels, char* operand)
{
    RR_Value* value;
    char ch = operand[0];
    if(ch == '[')
        value = RR_Value_NewQueue();
    else
    if(ch == '{')
        value = RR_Value_NewMap();
    else
    if(ch == '"')
    {
        RR_String* string = VM_ConvertEscs(self, operand);
        value = RR_Value_NewString(string);
    }
    else
    if(ch == '@')
    {
        RR_String* name = RR_String_Init(strtok(operand + 1, ","));
        size_t size = atoi(strtok(NULL, " \n"));
        size_t* address = RR_Map_Get(labels, name->value);
        if(address == NULL)
            Quit("asm: label %s not defined", name);
        value = RR_Value_NewFunction(RR_Function_Init(name, size, *address));
    }
    else
    if(ch == 't' || ch == 'f')
        value = RR_Value_NewBool(Equals(operand, "true") ? true : false);
    else
    if(ch == 'n')
        value = RR_Value_NewNull();
    else
    if(CC_String_IsDigit(ch) || ch == '-')
        value = RR_Value_NewNumber(atof(operand));
    else
    {
        value = NULL;
        Quit("asm: unknown psh operand %s encountered", operand);
    }
    RR_Queue_PshB(self->data, value);
}

static size_t
VM_Datum(VM* self, RR_Map* labels, char* operand)
{
    VM_Store(self, labels, operand);
    return ((self->data->size - 1) << 8) | OPCODE_PSH;
}

static size_t
VM_Indirect(Opcode oc, RR_Map* labels, char* label)
{
    size_t* address = RR_Map_Get(labels, label);
    if(address == NULL)
        Quit("asm: label %s not defined", label);
    return *address << 8 | oc;
}

static size_t
VM_Direct(Opcode oc, char* number)
{
    return (atoi(number) << 8) | oc;
}

static VM*
VM_Assemble(RR_Queue* assembly, RR_Queue* debug)
{
    size_t size = 0;
    RR_Map* labels = ASM_Label(assembly, &size);
    RR_Queue* addresses = ASM_Flatten(labels);
    VM* self = VM_Init(size, debug, addresses);
    size_t pc = 0;
    for(size_t i = 0; i < assembly->size; i++)
    {
        RR_String* stub = RR_Queue_Get(assembly, i);
        if(stub->value[0] == '\t')
        {
            RR_String* line = RR_String_Init(stub->value + 1);
            size_t instruction = 0;
            char* mnemonic = strtok(line->value, " \n");
            if(Equals(mnemonic, "add"))
                instruction = OPCODE_ADD;
            else
            if(Equals(mnemonic, "and"))
                instruction = OPCODE_AND;
            else
            if(Equals(mnemonic, "brf"))
                instruction = VM_Indirect(OPCODE_BRF, labels, strtok(NULL, "\n"));
            else
            if(Equals(mnemonic, "cal"))
                instruction = VM_Indirect(OPCODE_CAL, labels, strtok(NULL, "\n"));
            else
            if(Equals(mnemonic, "cpy"))
                instruction = OPCODE_CPY;
            else
            if(Equals(mnemonic, "del"))
                instruction = OPCODE_DEL;
            else
            if(Equals(mnemonic, "div"))
                instruction = OPCODE_DIV;
            else
            if(Equals(mnemonic, "dll"))
                instruction = OPCODE_DLL;
            else
            if(Equals(mnemonic, "end"))
                instruction = OPCODE_END;
            else
            if(Equals(mnemonic, "eql"))
                instruction = OPCODE_EQL;
            else
            if(Equals(mnemonic, "ext"))
                instruction = OPCODE_EXT;
            else
            if(Equals(mnemonic, "fls"))
                instruction = OPCODE_FLS;
            else
            if(Equals(mnemonic, "fmt"))
                instruction = OPCODE_FMT;
            else
            if(Equals(mnemonic, "get"))
                instruction = OPCODE_GET;
            else
            if(Equals(mnemonic, "glb"))
                instruction = VM_Direct(OPCODE_GLB, strtok(NULL, "\n"));
            else
            if(Equals(mnemonic, "god"))
                instruction = OPCODE_GOD;
            else
            if(Equals(mnemonic, "grt"))
                instruction = OPCODE_GRT;
            else
            if(Equals(mnemonic, "gte"))
                instruction = OPCODE_GTE;
            else
            if(Equals(mnemonic, "ins"))
                instruction = OPCODE_INS;
            else
            if(Equals(mnemonic, "jmp"))
                instruction = VM_Indirect(OPCODE_JMP, labels, strtok(NULL, "\n"));
            else
            if(Equals(mnemonic, "key"))
                instruction = OPCODE_KEY;
            else
            if(Equals(mnemonic, "len"))
                instruction = OPCODE_LEN;
            else
            if(Equals(mnemonic, "loc"))
                instruction = VM_Direct(OPCODE_LOC, strtok(NULL, "\n"));
            else
            if(Equals(mnemonic, "lod"))
                instruction = OPCODE_LOD;
            else
            if(Equals(mnemonic, "lor"))
                instruction = OPCODE_LOR;
            else
            if(Equals(mnemonic, "lst"))
                instruction = OPCODE_LST;
            else
            if(Equals(mnemonic, "lte"))
                instruction = OPCODE_LTE;
            else
            if(Equals(mnemonic, "mem"))
                instruction = OPCODE_MEM;
            else
            if(Equals(mnemonic, "mov"))
                instruction = OPCODE_MOV;
            else
            if(Equals(mnemonic, "mul"))
                instruction = OPCODE_MUL;
            else
            if(Equals(mnemonic, "neq"))
                instruction = OPCODE_NEQ;
            else
            if(Equals(mnemonic, "not"))
                instruction = OPCODE_NOT;
            else
            if(Equals(mnemonic, "opn"))
                instruction = OPCODE_OPN;
            else
            if(Equals(mnemonic, "pop"))
                instruction = OPCODE_POP;
            else
            if(Equals(mnemonic, "prt"))
                instruction = OPCODE_PRT;
            else
            if(Equals(mnemonic, "psb"))
                instruction = OPCODE_PSB;
            else
            if(Equals(mnemonic, "psf"))
                instruction = OPCODE_PSF;
            else
            if(Equals(mnemonic, "psh"))
                instruction = VM_Datum(self, labels, strtok(NULL, "\n"));
            else
            if(Equals(mnemonic, "red"))
                instruction = OPCODE_RED;
            else
            if(Equals(mnemonic, "ref"))
                instruction = OPCODE_REF;
            else
            if(Equals(mnemonic, "ret"))
                instruction = OPCODE_RET;
            else
            if(Equals(mnemonic, "sav"))
                instruction = OPCODE_SAV;
            else
            if(Equals(mnemonic, "spd"))
                instruction = OPCODE_SPD;
            else
            if(Equals(mnemonic, "srt"))
                instruction = OPCODE_SRT;
            else
            if(Equals(mnemonic, "sub"))
                instruction = OPCODE_SUB;
            else
            if(Equals(mnemonic, "typ"))
                instruction = OPCODE_TYP;
            else
            if(Equals(mnemonic, "vrt"))
                instruction = OPCODE_VRT;
            else
            if(Equals(mnemonic, "wrt"))
                instruction = OPCODE_WRT;
            else
                Quit("asm: unknown mnemonic %s", mnemonic);
            self->instructions[pc] = instruction;
            pc += 1;
            RR_String_Kill(line);
        }
    }
    RR_Map_Kill(labels);
    return self;
}

static void
VM_Cal(VM* self, size_t address)
{
    size_t sp = self->stack->size - self->spds;
    Frame* frame = Frame_Init(self->pc, sp, address);
    RR_Queue_PshB(self->frame, frame);
    self->pc = address;
    self->spds = 0;
}

static void
VM_Cpy(VM* self)
{
    RR_Value* back = RR_Queue_Back(self->stack);
    RR_Value* value = RR_Value_Copy(back);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, value);
}

static size_t
VM_End(VM* self)
{
    VM_TypeExpect(self, self->ret->type, TYPE_NUMBER);
    size_t ret = self->ret->of.number;
    RR_Value_Kill(self->ret);
    return ret;
}

static void
VM_Fls(VM* self)
{
    Frame* frame = RR_Queue_Back(self->frame);
    size_t pops = self->stack->size - frame->sp;
    while(pops > 0)
    {
        VM_Pop(self);
        pops -= 1;
    }
    self->pc = frame->pc;
    RR_Queue_PopB(self->frame);
}

static void
VM_Glb(VM* self, size_t address)
{
    RR_Value* value = RR_Queue_Get(self->stack, address);
    RR_Value_Inc(value);
    RR_Queue_PshB(self->stack, value);
}

static void
VM_Loc(VM* self, size_t address)
{
    Frame* frame = RR_Queue_Back(self->frame);
    RR_Value* value = RR_Queue_Get(self->stack, address + frame->sp);
    RR_Value_Inc(value);
    RR_Queue_PshB(self->stack, value);
}

static void
VM_Jmp(VM* self, size_t address)
{
    self->pc = address;
}

static void
VM_Ret(VM* self)
{
    Frame* frame = RR_Queue_Back(self->frame);
    self->pc = frame->pc;
    RR_Queue_PopB(self->frame);
}

static void
VM_Lod(VM* self)
{
    RR_Queue_PshB(self->stack, self->ret);
}

static void
VM_Sav(VM* self)
{
    RR_Value* value = RR_Queue_Back(self->stack);
    RR_Value_Inc(value);
    self->ret = value;
}

static void
VM_Psh(VM* self, size_t address)
{
    RR_Value* value = RR_Queue_Get(self->data, address);
    RR_Value* copy = RR_Value_Copy(value); // Copy because .data is read only.
    RR_Queue_PshB(self->stack, copy);
}

static void
VM_Mov(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    if(a->type == TYPE_CHAR && b->type == TYPE_STRING)
        RR_Value_Sub(a, b);
    else
    {
        RR_Type type = a->type;
        if(type == TYPE_NULL)
            VM_Quit(self, "cannot move %s to type null", RR_Type_ToString(b->type));
        Type_Kill(a->type, &a->of);
        RR_Type_Copy(a, b);
    }
    VM_Pop(self);
}

static void
VM_Add(VM* self)
{
    char* operator = "+";
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    if(a->type == TYPE_QUEUE && b->type != TYPE_QUEUE)
    {
        RR_Value_Inc(b);
        RR_Queue_PshB(a->of.queue, b);
    }
    else
    if(a->type == b->type)
        switch(a->type)
        {
        case TYPE_QUEUE:
            RR_Queue_Append(a->of.queue, b->of.queue);
            break;
        case TYPE_MAP:
            RR_Map_Append(a->of.map, b->of.map);
            break;
        case TYPE_STRING:
            RR_String_Appends(a->of.string, b->of.string->value);
            break;
        case TYPE_NUMBER:
            a->of.number += b->of.number;
            break;
        case TYPE_FUNCTION:
        case TYPE_CHAR:
        case TYPE_BOOL:
        case TYPE_NULL:
        case TYPE_FILE:
            VM_BadOperator(self, a->type, operator);
        }
    else
        VM_TypeMatch(self, a->type, b->type, operator);
    VM_Pop(self);
}

static void
VM_Sub(VM* self)
{
    char* operator = "-";
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    if(a->type == TYPE_QUEUE && b->type != TYPE_QUEUE)
    {
        RR_Value_Inc(b);
        RR_Queue_PshF(a->of.queue, b);
    }
    else
    if(a->type == b->type)
        switch(a->type)
        {
        case TYPE_QUEUE:
            RR_Queue_Prepend(a->of.queue, b->of.queue);
            break;
        case TYPE_NUMBER:
            a->of.number -= b->of.number;
            break;
        case TYPE_FUNCTION:
        case TYPE_MAP:
        case TYPE_STRING:
        case TYPE_CHAR:
        case TYPE_BOOL:
        case TYPE_NULL:
        case TYPE_FILE:
            VM_BadOperator(self, a->type, operator);
        }
    else
        VM_TypeMatch(self, a->type, b->type, operator);
    VM_Pop(self);
}

static void
VM_Mul(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeMatch(self, a->type, b->type, "*");
    VM_TypeExpect(self, a->type, TYPE_NUMBER);
    a->of.number *= b->of.number;
    VM_Pop(self);
}

static void
VM_Div(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeMatch(self, a->type, b->type, "/");
    VM_TypeExpect(self, a->type, TYPE_NUMBER);
    a->of.number /= b->of.number;
    VM_Pop(self);
}

static void
VM_Dll(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 3);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* c = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_STRING);
    VM_TypeExpect(self, b->type, TYPE_STRING);
    VM_TypeExpect(self, c->type, TYPE_NUMBER);
    typedef RR_Value* (*Call)();
    Call call;
    void* so = dlopen(a->of.string->value, RTLD_NOW | RTLD_GLOBAL);
    if(so == NULL)
        VM_Quit(self, "could not open shared object library %s\n", a->of.string->value);
    *(void**)(&call) = dlsym(so, b->of.string->value);
    if(call == NULL)
        VM_Quit(self, "could not open shared object function %s from shared object library %s\n", b->of.string->value, a->of.string->value);
    size_t params = 3;
    size_t start = self->stack->size - params;
    size_t args = c->of.number;
    RR_Value* value = NULL;
    switch(args)
    {
    case 0:
        value = call();
        break;
    case 1:
        value = call(
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 2:
        value = call(
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 3:
        value = call(
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 4:
        value = call(
            RR_Queue_Get(self->stack, start - 4),
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 5:
        value = call(
            RR_Queue_Get(self->stack, start - 5),
            RR_Queue_Get(self->stack, start - 4),
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 6:
        value = call(
            RR_Queue_Get(self->stack, start - 6),
            RR_Queue_Get(self->stack, start - 5),
            RR_Queue_Get(self->stack, start - 4),
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 7:
        value = call(
            RR_Queue_Get(self->stack, start - 7),
            RR_Queue_Get(self->stack, start - 6),
            RR_Queue_Get(self->stack, start - 5),
            RR_Queue_Get(self->stack, start - 4),
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 8:
        value = call(
            RR_Queue_Get(self->stack, start - 8),
            RR_Queue_Get(self->stack, start - 7),
            RR_Queue_Get(self->stack, start - 6),
            RR_Queue_Get(self->stack, start - 5),
            RR_Queue_Get(self->stack, start - 4),
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 9:
        value = call(
            RR_Queue_Get(self->stack, start - 9),
            RR_Queue_Get(self->stack, start - 8),
            RR_Queue_Get(self->stack, start - 7),
            RR_Queue_Get(self->stack, start - 6),
            RR_Queue_Get(self->stack, start - 5),
            RR_Queue_Get(self->stack, start - 4),
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    default:
        VM_Quit(self, "only 9 arguments at most are supported for native functions calls");
    }
    for(size_t i = 0; i < args + params; i++)
        VM_Pop(self);
    RR_Value_Print(value);
    RR_Queue_PshB(self->stack, value);
}

static void
VM_Vrt(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_FUNCTION);
    VM_TypeExpect(self, b->type, TYPE_NUMBER);
    VM_ArgMatch(self, a->of.function->size, b->of.number);
    size_t spds = b->of.number;
    size_t address = a->of.function->address;
    VM_Pop(self);
    VM_Pop(self);
    size_t sp = self->stack->size - spds;
    RR_Queue_PshB(self->frame, Frame_Init(self->pc, sp, address));
    self->pc = address;
}

static void
VM_RangedSort(VM* self, RR_Queue* queue, RR_Value* compare, int64_t left, int64_t right)
{
    if(left >= right)
        return;
    RR_Queue_Swap(queue, left, (left + right) / 2);
    int64_t last = left;
    for(int64_t i = left + 1; i <= right; i++)
    {
        RR_Value* a = RR_Queue_Get(queue, i);
        RR_Value* b = RR_Queue_Get(queue, left);
        RR_Value_Inc(a);
        RR_Value_Inc(b);
        RR_Value_Inc(compare);
        RR_Queue_PshB(self->stack, a);
        RR_Queue_PshB(self->stack, b);
        RR_Queue_PshB(self->stack, compare);
        RR_Queue_PshB(self->stack, RR_Value_NewNumber(2));
        VM_Vrt(self);
        VM_Run(self, true);
        VM_TypeExpect(self, self->ret->type, TYPE_BOOL);
        if(self->ret->of.boolean)
             RR_Queue_Swap(queue, ++last, i);
        RR_Value_Kill(self->ret);
    }
   RR_Queue_Swap(queue, left, last);
   VM_RangedSort(self, queue, compare, left, last - 1);
   VM_RangedSort(self, queue, compare, last + 1, right);
}

static void
VM_Sort(VM* self, RR_Queue* queue, RR_Value* compare)
{
    VM_TypeExpect(self, compare->type, TYPE_FUNCTION);
    VM_ArgMatch(self, compare->of.function->size, 2);
    VM_RangedSort(self, queue, compare, 0, queue->size - 1);
}

static void
VM_Srt(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_QUEUE);
    VM_TypeExpect(self, b->type, TYPE_FUNCTION);
    VM_Sort(self, a->of.queue, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNull());
}

static void
VM_Lst(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeMatch(self, a->type, b->type, "<");
    bool boolean = RR_Value_LessThan(a, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Lte(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeMatch(self, a->type, b->type, "<=");
    bool boolean = RR_Value_LessThanEqualTo(a, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Grt(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeMatch(self, a->type, b->type, ">");
    bool boolean = RR_Value_GreaterThan(a, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Gte(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeMatch(self, a->type, b->type, ">=");
    bool boolean = RR_Value_GreaterThanEqualTo(a, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_And(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeMatch(self, a->type, b->type, "&&");
    VM_TypeExpect(self, a->type, TYPE_BOOL);
    a->of.boolean = a->of.boolean && b->of.boolean;
    VM_Pop(self);
}

static void
VM_Lor(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeMatch(self, a->type, b->type, "||");
    VM_TypeExpect(self, a->type, TYPE_BOOL);
    a->of.boolean = a->of.boolean || b->of.boolean;
    VM_Pop(self);
}

static void
VM_Eql(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    bool boolean = RR_Value_Equal(a, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Neq(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    bool boolean = !RR_Value_Equal(a, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Spd(VM* self)
{
    self->spds += 1;
}

static void
VM_Not(VM* self)
{
    RR_Value* value = RR_Queue_Back(self->stack);
    VM_TypeExpect(self, value->type, TYPE_BOOL);
    value->of.boolean = !value->of.boolean;
}

static void
VM_Brf(VM* self, size_t address)
{
    RR_Value* value = RR_Queue_Back(self->stack);
    VM_TypeExpect(self, value->type, TYPE_BOOL);
    if(value->of.boolean == false)
        self->pc = address;
    VM_Pop(self);
}

static void
VM_Prt(VM* self)
{
    RR_Value* value = RR_Queue_Back(self->stack);
    RR_String* print = RR_Value_Sprint(value, false, 0);
    puts(print->value);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNumber(print->size));
    RR_String_Kill(print);
}

static void
VM_Len(VM* self)
{
    RR_Value* value = RR_Queue_Back(self->stack);
    size_t len = RR_Value_Len(value);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNumber(len));
}

static void
VM_Psb(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_QUEUE);
    RR_Queue_PshB(a->of.queue, b);
    RR_Value_Inc(b);
    VM_Pop(self);
}

static void
VM_Psf(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_QUEUE);
    RR_Queue_PshF(a->of.queue, b);
    RR_Value_Inc(b);
    VM_Pop(self);
}

static void
VM_Ins(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 3);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* c = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_MAP);
    if(b->type == TYPE_CHAR)
        VM_TypeBadIndex(self, a->type, b->type);
    else
    if(b->type == TYPE_STRING)
    {
        RR_Map_Set(a->of.map, RR_String_Copy(b->of.string), c); // Map keys are not reference counted.
        RR_Value_Inc(c);
    }
    else
        VM_TypeBad(self, b->type);
    VM_Pop(self);
    VM_Pop(self);
}

static void
VM_Ref(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 1);
    size_t refs = a->refs;
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNumber(refs));
}

static RR_Value*
VM_IndexRR_Queue(VM* self, RR_Value* queue, RR_Value* index)
{
    RR_Value* value = RR_Queue_Get(queue->of.queue, index->of.number);
    if(value == NULL)
        VM_OutOfBounds(self, queue->type, index->of.number);
    RR_Value_Inc(value);
    return value;
}

static RR_Value*
VM_IndexRR_String(VM* self, RR_Value* queue, RR_Value* index)
{
    RR_Char* character = RR_Char_Init(queue, index->of.number);
    if(character == NULL)
        VM_OutOfBounds(self, queue->type, index->of.number);
    RR_Value* value = RR_Value_NewChar(character);
    RR_Value_Inc(queue);
    return value;
}

static RR_Value*
VM_Index(VM* self, RR_Value* storage, RR_Value* index)
{
    if(storage->type == TYPE_QUEUE)
        return VM_IndexRR_Queue(self, storage, index);
    else
    if(storage->type == TYPE_STRING)
        return VM_IndexRR_String(self, storage, index);
    VM_TypeBadIndex(self, storage->type, index->type);
    return NULL;
}

static RR_Value*
VM_Lookup(VM* self, RR_Value* map, RR_Value* index)
{
    VM_TypeExpect(self, map->type, TYPE_MAP);
    RR_Value* value = RR_Map_Get(map->of.map, index->of.string->value);
    if(value != NULL)
        RR_Value_Inc(value);
    return value;
}

static void
VM_Get(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    RR_Value* value = NULL;
    if(b->type == TYPE_NUMBER)
        value = VM_Index(self, a, b);
    else
    if(b->type == TYPE_STRING)
        value = VM_Lookup(self, a, b);
    else
        VM_TypeBadIndex(self, a->type, b->type);
    VM_Pop(self);
    VM_Pop(self);
    if(value == NULL)
        RR_Queue_PshB(self->stack, RR_Value_NewNull());
    else
        RR_Queue_PshB(self->stack, value);
}

static void
VM_Fmt(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_STRING);
    VM_TypeExpect(self, b->type, TYPE_QUEUE);
    RR_String* formatted = RR_String_Init("");
    size_t index = 0;
    char* ref = a->of.string->value;
    for(char* c = ref; *c; c++)
    {
        if(*c == '{')
        {
            char next = *(c + 1);
            if(next == '}')
            {
                RR_Value* value = RR_Queue_Get(b->of.queue, index);
                if(value == NULL)
                    VM_OutOfBounds(self, b->type, index);
                String_Append(formatted, RR_Value_Sprint(value, false, 0));
                index += 1;
                c += 1;
                continue;
            }
        }
        RR_String_PshB(formatted, *c);
    }
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewString(formatted));
}

static void
VM_Typ(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 1);
    RR_Type type = a->type;
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewString(RR_String_Init(RR_Type_ToString(type))));
}

static void
VM_Del(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    if(b->type == TYPE_NUMBER)
    {
        if(a->type == TYPE_QUEUE)
        {
            bool success = RR_Queue_Del(a->of.queue, b->of.number);
            if(success == false)
                VM_OutOfBounds(self, a->type, b->of.number);
        }
        else
        if(a->type == TYPE_STRING)
        {
            bool success = RR_String_Del(a->of.string, b->of.number);
            if(success == false)
                VM_OutOfBounds(self, a->type, b->of.number);
        }
        else
            VM_TypeBadIndex(self, a->type, b->type);
    }
    else
    if(b->type == TYPE_STRING)
    {
        if(a->type != TYPE_MAP)
            VM_TypeBadIndex(self, a->type, b->type);
        RR_Map_Del(a->of.map, b->of.string->value);
    }
    else
        VM_TypeBad(self, a->type);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNull());
}

static void
VM_Mem(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    bool boolean = a == b;
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Opn(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_STRING);
    VM_TypeExpect(self, b->type, TYPE_STRING);
    RR_File* file = RR_File_Init(RR_String_Copy(a->of.string), RR_String_Copy(b->of.string));
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewFile(file));
}

static void
VM_Red(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_FILE);
    VM_TypeExpect(self, b->type, TYPE_NUMBER);
    RR_String* buffer = RR_String_Init("");
    if(a->of.file->file)
    {
        RR_String_Resize(buffer, b->of.number);
        size_t size = fread(buffer->value, sizeof(char), b->of.number, a->of.file->file);
        size_t diff = b->of.number - size;
        while(diff)
        {
            RR_String_PopB(buffer);
            diff -= 1;
        }
    }
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewString(buffer));
}

static void
VM_Wrt(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 2);
    RR_Value* b = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_FILE);
    VM_TypeExpect(self, b->type, TYPE_STRING);
    size_t bytes = 0;
    if(a->of.file->file)
        bytes = fwrite(b->of.string->value, sizeof(char), b->of.string->size, a->of.file->file);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNumber(bytes));
}

static void
VM_God(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_FILE);
    bool boolean = a->of.file->file != NULL;
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Key(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_MAP);
    RR_Value* queue = RR_Map_Key(a->of.map);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, queue);
}

static void
VM_Ext(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_NUMBER);
    exit(a->of.number);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNull());
}

static int64_t
VM_Run(VM* self, bool arbitrary)
{
    while(true)
    {
        size_t instruction = self->instructions[self->pc];
        self->pc += 1;
        Opcode oc = instruction & 0xFF;
        size_t address = instruction >> 8;
        switch(oc)
        {
        case OPCODE_ADD: VM_Add(self); break;
        case OPCODE_AND: VM_And(self); break;
        case OPCODE_BRF: VM_Brf(self, address); break;
        case OPCODE_CAL: VM_Cal(self, address); break;
        case OPCODE_CPY: VM_Cpy(self); break;
        case OPCODE_DEL: VM_Del(self); break;
        case OPCODE_DIV: VM_Div(self); break;
        case OPCODE_DLL: VM_Dll(self); break;
        case OPCODE_END: return VM_End(self);
        case OPCODE_EQL: VM_Eql(self); break;
        case OPCODE_EXT: VM_Ext(self); break;
        case OPCODE_FLS: VM_Fls(self); break;
        case OPCODE_FMT: VM_Fmt(self); break;
        case OPCODE_GET: VM_Get(self); break;
        case OPCODE_GLB: VM_Glb(self, address); break;
        case OPCODE_GOD: VM_God(self); break;
        case OPCODE_GRT: VM_Grt(self); break;
        case OPCODE_GTE: VM_Gte(self); break;
        case OPCODE_INS: VM_Ins(self); break;
        case OPCODE_JMP: VM_Jmp(self, address); break;
        case OPCODE_KEY: VM_Key(self); break;
        case OPCODE_LEN: VM_Len(self); break;
        case OPCODE_LOC: VM_Loc(self, address); break;
        case OPCODE_LOD: VM_Lod(self); break;
        case OPCODE_LOR: VM_Lor(self); break;
        case OPCODE_LST: VM_Lst(self); break;
        case OPCODE_LTE: VM_Lte(self); break;
        case OPCODE_MEM: VM_Mem(self); break;
        case OPCODE_MOV: VM_Mov(self); break;
        case OPCODE_MUL: VM_Mul(self); break;
        case OPCODE_NEQ: VM_Neq(self); break;
        case OPCODE_NOT: VM_Not(self); break;
        case OPCODE_OPN: VM_Opn(self); break;
        case OPCODE_POP: VM_Pop(self); break;
        case OPCODE_PRT: VM_Prt(self); break;
        case OPCODE_PSB: VM_Psb(self); break;
        case OPCODE_PSF: VM_Psf(self); break;
        case OPCODE_PSH: VM_Psh(self, address); break;
        case OPCODE_RED: VM_Red(self); break;
        case OPCODE_REF: VM_Ref(self); break;
        case OPCODE_RET: VM_Ret(self); break;
        case OPCODE_SAV: VM_Sav(self); break;
        case OPCODE_SPD: VM_Spd(self); break;
        case OPCODE_SRT: VM_Srt(self); break;
        case OPCODE_SUB: VM_Sub(self); break;
        case OPCODE_TYP: VM_Typ(self); break;
        case OPCODE_VRT: VM_Vrt(self); break;
        case OPCODE_WRT: VM_Wrt(self); break;
        }
        if(arbitrary)
            if(oc == OPCODE_RET || oc == OPCODE_FLS)
                return 0;
    }
}

static Args
Args_Parse(int64_t argc, char* argv[])
{
    Args self = {
        .entry = NULL,
        .dump = false,
        .help = false,
    };
    for(int64_t i = 1; i < argc; i++)
    {
        char* arg = argv[i];
        if(arg[0] == '-')
        {
            if(Equals(arg, "-d")) self.dump = true;
            if(Equals(arg, "-h")) self.help = true;
        }
        else
            self.entry = arg;
    }
    return self;
}

static void
Args_Help(void)
{
    puts(
        "The Roman II Programming Language\n"
        "-h: this help screen\n"
        "-d: print generated assembly to stdout" 
    );
}

int
main(int argc, char* argv[])
{
    Args args = Args_Parse(argc, argv);
    if(args.entry)
    {
        int64_t ret = 0;
        RR_String* entry = RR_String_Init(args.entry);
        CC* cc = CC_Init();
        CC_ReserveFunctions(cc);
        CC_IncludeModule(cc, entry);
        CC_Parse(cc);
        VM* vm = VM_Assemble(cc->assembly, cc->debug);
        if(args.dump)
        {
            ASM_Dump(cc->assembly);
            VM_Data(vm);
        }
        else
            ret = VM_Run(vm, false);
        VM_AssertRefCounts(vm);
        VM_Kill(vm);
        CC_Kill(cc);
        RR_String_Kill(entry);
        return ret;
    }
    else
    if(args.help)
        Args_Help();
    else
        Quit("usage: rr -h");
}

#endif
