/* 
 * The Roman II Programming Language
 *
 * Copyright (C) 2021-2022 Gustav Louw. All rights reserved.
 * This work is licensed under the terms of the MIT license.  
 * For a copy, see <https://opensource.org/licenses/MIT>.
 *
 */

#include "rr.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/time.h>
#include <stdio.h>

#define QUEUE_BLOCK_SIZE (8)
#define MAP_UNPRIMED (-1)
#define MODULE_BUFFER_SIZE (512)
#define STR_CAP_SIZE (16)
#define LEN(a) (sizeof(a) / sizeof(*a))

typedef bool(*Compare)(void*, void*);

typedef void (*Kill)(void*);

typedef void* (*Copy)(void*);

struct String
{
    char* value;
    int size;
    int cap;
};

struct File
{
    String* path;
    String* mode;
    FILE* file;
};

typedef struct Node
{
    struct Node* next;
    String* key;
    void* value;
}
Node;

struct Map
{
    Node** bucket;
    Kill kill;
    Copy copy;
    int size;
    int prime_index;
    float load_factor;
    unsigned rand1;
    unsigned rand2;
};

typedef struct
{
    void* value[QUEUE_BLOCK_SIZE];
    int a;
    int b;
}
Block;

struct Queue
{
    Block** block;
    Kill kill;
    Copy copy;
    int size;
    int blocks;
};

typedef enum
{
    TYPE_FILE,
    TYPE_FUNCTION,
    TYPE_QUEUE,
    TYPE_CHAR,
    TYPE_MAP,
    TYPE_STRING,
    TYPE_NUMBER,
    TYPE_BOOL,
    TYPE_NULL,
}
Type;

struct Function
{
    String* name;
    int size;
    int address;
};

typedef struct
{
    Value* string;
    char* value;
}
Char;

typedef union
{
    File* file;
    Function* function;
    Queue* queue;
    Map* map;
    String* string;
    Char* character;
    double number;
    bool boolean;
}
Of;

struct Value
{
    Type type;
    Of of;
    int refs;
};

typedef struct
{
    char* entry;
    bool dump;
    bool help;
}
Args;

typedef enum
{
    FRONT,
    BACK,
}
End;

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
    Queue* modules;
    Queue* assembly;
    Queue* data;
    Map* identifiers;
    Map* included;
    String* prime;
    int globals;
    int locals;
    int labels;
}
CC;

typedef struct
{
    FILE* file;
    String* name;
    int index;
    int size;
    int line;
    unsigned char buffer[MODULE_BUFFER_SIZE];
}
Module;

typedef struct
{
    Class class;
    int stack;
    String* path;
}
Meta;

typedef struct
{
    Queue* data;
    Queue* stack;
    Queue* frame;
    Map* track;
    Value* ret;
    uint64_t* instructions;
    int size;
    int pc;
    int spds;
}
VM;

typedef struct
{
    int pc;
    int sp;
}
Frame;

File*
Value_ToFile(Value* value)
{
    return value->type == TYPE_FILE
         ? value->of.file
         : NULL;
}

Queue*
Value_ToQueue(Value* value)
{
    return value->type == TYPE_QUEUE
         ? value->of.queue
         : NULL;
}

Map*
Value_ToMap(Value* value)
{
    return value->type == TYPE_MAP
         ? value->of.map
         : NULL;
}

String*
Value_ToString(Value* value)
{
    return value->type == TYPE_STRING
         ? value->of.string
         : NULL;
}

char*
Value_ToChar(Value* value)
{
    return value->type == TYPE_CHAR
         ? value->of.character->value
         : NULL;
}

double*
Value_ToNumber(Value* value)
{
    return value->type == TYPE_NUMBER
         ? &value->of.number
         : NULL;
}

bool*
Value_ToBool(Value* value)
{
    return value->type == TYPE_BOOL
         ? &value->of.boolean
         : NULL;
}

static String*
Value_Print(Value*, bool newline, int indents);

static void
Map_Emplace(Map*, String*, Node*);

static void
CC_Expression(CC*);

static bool
CC_Factor(CC*);

static void
CC_Block(CC*, int head, int tail, bool loop);

static void
Value_Kill(Value*);

static int
VM_Run(VM*, bool arbitrary);

static void*
Malloc(int size)
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

static inline double
Microseconds(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1e6 + tv.tv_usec;
}

static void
Delete(Kill kill, void* value)
{
    if(kill)
        kill(value);
}

static bool
Equals(char* a, char* b)
{
    return a[0] != b[0] ? false : (strcmp(a, b) == 0);
}

Args
Args_Parse(int argc, char* argv[])
{
    Args self = {
        .entry = NULL,
        .dump = false,
        .help = false,
    };
    for(int i = 1; i < argc; i++)
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

void
Args_Help(void)
{
    puts(
        "The Roman II Programming Language\n"
        "-h: this help screen\n"
        "-d: print generated assembly to stdout" 
    );
}

static char*
Class_String(Class self)
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

static void
String_Alloc(String* self, int cap)
{
    self->cap = cap;
    self->value = Realloc(self->value, (1 + cap) * sizeof(*self->value));
}

static String*
String_Init(char* string)
{
    String* self = Malloc(sizeof(*self));
    self->value = NULL;
    self->size = strlen(string);
    String_Alloc(self, self->size < STR_CAP_SIZE ? STR_CAP_SIZE : self->size);
    strcpy(self->value, string);
    return self;
}

static void
String_Kill(String* self)
{
    Delete(Free, self->value);
    Free(self);
}

static String*
String_FromChar(char c)
{
    char string[] = { c, '\0' };
    return String_Init(string);
}

static void
String_Swap(String** self, String** other)
{
    String* temp = *self;
    *self = *other;
    *other = temp;
}

static String*
String_Copy(String* self)
{
    return String_Init(self->value);
}

static int
String_Size(String* self)
{
    return self->size;
}

static void
String_Replace(String* self, char x, char y)
{
    for(int i = 0; i < String_Size(self); i++)
        if(self->value[i] == x)
            self->value[i] = y;
}

static char*
String_End(String* self)
{
    return &self->value[String_Size(self) - 1];
}

static char
String_Back(String* self)
{
    return *String_End(self);
}

static char*
String_Begin(String* self)
{
    return &self->value[0];
}

static int
String_Empty(String* self)
{
    return String_Size(self) == 0;
}

static void
String_PshB(String* self, char ch)
{
    if(String_Size(self) == self->cap)
        String_Alloc(self, (self->cap == 0) ? 1 : (2 * self->cap));
    self->value[self->size + 0] = ch;
    self->value[self->size + 1] = '\0';
    self->size += 1;
}

static String*
String_Indent(int indents)
{
    int width = 4;
    String* ident = String_Init("");
    while(indents > 0)
    {
        for(int i = 0; i < width; i++)
            String_PshB(ident, ' ');
        indents -= 1;
    }
    return ident;
}

static void
String_PopB(String* self)
{
    self->size -= 1;
    self->value[self->size] = '\0';
}

static void
String_Resize(String* self, int size)
{
    if(size > self->cap)
        String_Alloc(self, size);
    self->size = size;
    self->value[size] = '\0';
}

static bool
String_Equals(String* a, char* b)
{
    return Equals(a->value, b);
}

static bool
String_Equal(String* a, String* b)
{
    return Equals(a->value, b->value);
}

static void
String_Appends(String* self, char* str)
{
    while(*str)
    {
        String_PshB(self, *str);
        str += 1;
    }
}

static void
String_Append(String* self, String* other)
{
    String_Appends(self, other->value);
    String_Kill(other);
}

static String*
String_Base(String* path)
{
    String* base = String_Copy(path);
    while(!String_Empty(base))
        if(String_Back(base) == '/')
            break;
        else
            String_PopB(base);
    return base;
}

static String*
String_Moves(char** from)
{
    String* self = Malloc(sizeof(*self));
    self->value = *from;
    self->size = strlen(self->value);
    self->cap = self->size;
    *from = NULL;
    return self;
}

static String*
String_Skip(String* self, char c)
{
    char* value = self->value;
    while(*value)
    {
        if(*value != c)
            break;
        value += 1;
    }
    return String_Init(value);
}

static bool
String_IsBoolean(String* ident)
{
    return String_Equals(ident, "true") || String_Equals(ident, "false");
}

static bool
String_IsNull(String* ident)
{
    return String_Equals(ident, "null");
}

static String*
String_Format(char* format, ...)
{
    va_list args;
    va_start(args, format);
    String* line = String_Init("");
    String_Resize(line, STR_CAP_SIZE);
    int size = vsnprintf(line->value, line->size, format, args);
    va_end(args);
    if(size > line->size)
    {
        va_start(args, format);
        String_Resize(line, size);
        vsprintf(line->value, format, args);
        va_end(args);
    }
    return line;
}

static char*
String_Get(String* self, int index)
{
    if(index < 0 || index >= String_Size(self))
        return NULL;
    return &self->value[index];
}

static bool
String_Del(String* self, int index)
{
    if(String_Get(self, index))
    {
        for(int i = index; i < String_Size(self) - 1; i++)
            self->value[i] = self->value[i + 1];
        String_PopB(self);
        return true;
    }
    return false;
}

static int
String_EscToByte(int ch)
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

static bool
String_IsUpper(int c)
{
    return c >= 'A' && c <= 'Z';
}

static bool
String_IsLower(int c)
{
    return c >= 'a' && c <= 'z';
}

static bool
String_IsAlpha(int c)
{
    return String_IsLower(c) || String_IsUpper(c);
}

static bool
String_IsDigit(int c)
{
    return c >= '0' && c <= '9';
}

static bool
String_IsNumber(int c)
{
    return String_IsDigit(c) || c == '.';
}

static bool
String_IsIdentLeader(int c)
{
    return String_IsAlpha(c) || c == '_';
}

static bool
String_IsIdent(int c)
{
    return String_IsIdentLeader(c) || String_IsDigit(c);
}

static bool
String_IsModule(int c)
{
    return String_IsIdent(c) || c == '.';
}

static bool
String_IsOp(int c)
{
    return c == '*' || c == '/' || c == '%' || c == '+' || c == '-' || c == '='
        || c == '<' || c == '>' || c == '!' || c == '&' || c == '|' || c == '?';
}

static bool
String_IsSpace(int c)
{
    return c == '\n' || c == '\t' || c == '\r' || c == ' ';
}

static File*
File_Init(String* path, String* mode)
{
    File* self = Malloc(sizeof(*self));
    self->path = path;
    self->mode = mode;
    self->file = fopen(self->path->value, self->mode->value);
    return self;
}

static bool
File_Equal(File* a, File* b)
{
    return String_Equal(a->path, b->path) 
        && String_Equal(a->mode, b->mode)
        && a->file == b->file;
}

static void
File_Kill(File* self)
{
    String_Kill(self->path);
    String_Kill(self->mode);
    if(self->file)
        fclose(self->file);
    Free(self);
}

static File*
File_Copy(File* self)
{
    return File_Init(String_Copy(self->path), String_Copy(self->mode));
}

static int
File_Size(File* self)
{
    if(self->file)
    {
        int prev = ftell(self->file);
        fseek(self->file, 0L, SEEK_END);
        int size = ftell(self->file);
        fseek(self->file, prev, SEEK_SET);
        return size;
    }
    else
        return 0;
}

static Function*
Function_Init(String* name, int size, int address)
{
    Function* self = malloc(sizeof(*self));
    self->name = name;
    self->size = size;
    self->address = address;
    return self;
}

static void
Function_Kill(Function* self)
{
    String_Kill(self->name);
    Free(self);
}

static Function*
Function_Copy(Function* self)
{
    return Function_Init(String_Copy(self->name), self->size, self->address);
}

static int
Function_Size(Function* self)
{
    return self->size;
}

static bool
Function_Equal(Function* a, Function* b)
{
    return String_Equal(a->name, b->name)
        && a->address == b->address
        && Function_Size(a) == Function_Size(b);
}

static Block*
Block_Init(End end)
{
    Block* self = Malloc(sizeof(*self));
    self->a = (end == BACK) ? 0 : QUEUE_BLOCK_SIZE;
    self->b = (end == BACK) ? 0 : QUEUE_BLOCK_SIZE;
    return self;
}

static int*
Int_Init(int value)
{
    int* self = Malloc(sizeof(*self));
    *self = value;
    return self;
}

static void
Int_Kill(int* self)
{
    Free(self);
}

static Frame*
Frame_Init(int pc, int sp)
{
    Frame* self = Malloc(sizeof(*self));
    self->pc = pc;
    self->sp = sp;
    return self;
}

static void
Frame_Free(Frame* self)
{
    Free(self);
}

static int
Queue_Size(Queue* self)
{
    return self->size;
}

static bool
Queue_Empty(Queue* self)
{
    return Queue_Size(self) == 0;
}

static Block**
Queue_BlockF(Queue* self)
{
    return &self->block[0];
}

static Block**
Queue_BlockB(Queue* self)
{
    return &self->block[self->blocks - 1];
}

static void*
Queue_Front(Queue* self)
{
    if(Queue_Empty(self))
        return NULL;
    Block* block = *Queue_BlockF(self);
    return block->value[block->a];
}

static void*
Queue_Back(Queue* self)
{
    if(Queue_Empty(self))
        return NULL;
    Block* block = *Queue_BlockB(self);
    return block->value[block->b - 1];
}

static Queue*
Queue_Init(Kill kill, Copy copy)
{
    Queue* self = Malloc(sizeof(*self));
    self->block = NULL;
    self->kill = kill;
    self->copy = copy;
    self->size = 0;
    self->blocks = 0;
    return self;
}

static void
Queue_Kill(Queue* self)
{
    for(int step = 0; step < self->blocks; step++)
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

static void**
Queue_At(Queue* self, int index)
{
    if(index < self->size)
    {
        Block* block = *Queue_BlockF(self);
        int at = index + block->a;
        int step = at / QUEUE_BLOCK_SIZE;
        int item = at % QUEUE_BLOCK_SIZE;
        return &self->block[step]->value[item];
    }
    return NULL;
}

static void*
Queue_Get(Queue* self, int index)
{
    if(index == 0)
        return Queue_Front(self);
    else
    if(index == Queue_Size(self) - 1)
        return Queue_Back(self);
    else
    {
        void** at = Queue_At(self, index);
        return at ? *at : NULL;
    }
}

static void
Queue_Alloc(Queue* self, int blocks)
{
    self->blocks = blocks;
    self->block = Realloc(self->block, blocks * sizeof(*self->block));
}

static void
Queue_Push(Queue* self, void* value, End end)
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

static void
Queue_PshB(Queue* self, void* value)
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
    Queue_Push(self, value, BACK);
}

static void
Queue_PshF(Queue* self, void* value)
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
            for(int i = self->blocks - 1; i > 0; i--)
                self->block[i] = self->block[i - 1];
            *Queue_BlockF(self) = Block_Init(FRONT);
        }
    }
    Queue_Push(self, value, FRONT);
}

static void
Queue_Pop(Queue* self, End end)
{
    if(!Queue_Empty(self))
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
                for(int i = 0; i < self->blocks - 1; i++)
                    self->block[i] = self->block[i + 1];
                Free(block);
                Queue_Alloc(self, self->blocks - 1);
            }
        }
        self->size -= 1;
    }
}

static void
Queue_PopB(Queue* self)
{
    Queue_Pop(self, BACK);
}

static void
Queue_PopF(Queue* self)
{
    Queue_Pop(self, FRONT);
}

static void
Queue_PopFSoft(Queue* self)
{
    Kill kill = self->kill;
    self->kill = NULL;
    Queue_PopF(self);
    self->kill = kill;
}

static void
Queue_PopBSoft(Queue* self)
{
    Kill kill = self->kill;
    self->kill = NULL;
    Queue_PopB(self);
    self->kill = kill;
}

static bool
Queue_Del(Queue* self, int index)
{
    if(index == 0)
        Queue_PopF(self);
    else
    if(index == Queue_Size(self) - 1)
        Queue_PopB(self);
    else
    {
        void** at = Queue_At(self, index);
        if(at)
        {
            Delete(self->kill, *at);
            if(index < Queue_Size(self) / 2)
            {
                while(index > 0)
                {
                    *Queue_At(self, index) = *Queue_At(self, index - 1);
                    index -= 1;
                }
                Queue_PopFSoft(self);
            }
            else
            {
                while(index < Queue_Size(self) - 1)
                {
                    *Queue_At(self, index) = *Queue_At(self, index + 1);
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

static Queue*
Queue_Copy(Queue* self)
{
    Queue* copy = Queue_Init(self->kill, self->copy);
    for(int i = 0; i < Queue_Size(self); i++)
    {
        void* temp = Queue_Get(self, i);
        void* value = copy->copy ? copy->copy(temp) : temp;
        Queue_PshB(copy, value);
    }
    return copy;
}

static void
Queue_Prepend(Queue* self, Queue* other)
{
    for(int i = Queue_Size(other) - 1; i >= 0; i--)
    {
        void* temp = Queue_Get(other, i);
        void* value = self->copy ? self->copy(temp) : temp;
        Queue_PshF(self, value);
    }
}

static void
Queue_Append(Queue* self, Queue* other)
{
    for(int i = 0; i < Queue_Size(other); i++)
    {
        void* temp = Queue_Get(other, i);
        void* value = self->copy ? self->copy(temp) : temp;
        Queue_PshB(self, value);
    }
}

static bool
Queue_Equal(Queue* self, Queue* other, Compare compare)
{
    if(Queue_Size(self) != Queue_Size(other))
        return false;
    else
        for(int i = 0; i < Queue_Size(self); i++)
            if(!compare(Queue_Get(self, i), Queue_Get(other, i)))
                return false;
    return true;
}

static void
Queue_Swap(Queue* self, int a, int b)
{
    void** x = Queue_At(self, a);
    void** y = Queue_At(self, b);
    void* temp = *x;
    *x = *y;
    *y = temp;
}

static void
Queue_RangedSort(Queue* self, bool (*compare)(void*, void*), int left, int right)
{
    if(left >= right)
        return;
    Queue_Swap(self, left, (left + right) / 2);
    int last = left;
    for(int i = left + 1; i <= right; i++)
    {
        void* a = Queue_Get(self, i);
        void* b = Queue_Get(self, left);
        if(compare(a, b))
             Queue_Swap(self, ++last, i);
    }
   Queue_Swap(self, left, last);
   Queue_RangedSort(self, compare, left, last - 1);
   Queue_RangedSort(self, compare, last + 1, right);
}

static void
Queue_Sort(Queue* self, bool (*compare)(void*, void*))
{
    Queue_RangedSort(self, compare, 0, Queue_Size(self) - 1);
}

static String*
Queue_Print(Queue* self, int indents)
{
    if(Queue_Empty(self))
        return String_Init("[]");
    else
    {
        String* print = String_Init("[\n");
        int size = Queue_Size(self);
        for(int i = 0; i < size; i++)
        {
            String_Append(print, String_Indent(indents + 1));
            String_Append(print, String_Format("[%d] = ", i));
            String_Append(print, Value_Print(Queue_Get(self, i), false, indents + 1));
            if(i < size - 1)
                String_Appends(print, ",");
            String_Appends(print, "\n");
        }
        String_Append(print, String_Indent(indents));
        String_Appends(print, "]");
        return print;
    }
}

static Node*
Node_Init(String* key, void* value)
{
    Node* self = Malloc(sizeof(*self));
    self->next = NULL;
    self->key = key;
    self->value = value;
    return self;
}

static void
Node_Set(Node* self, Kill kill, String* key, void* value)
{
    Delete((Kill) String_Kill, self->key);
    Delete(kill, self->value);
    self->key = key;
    self->value = value;
}

static void
Node_Kill(Node* self, Kill kill)
{
    Node_Set(self, kill, NULL, NULL);
    Free(self);
}

static void
Node_Push(Node** self, Node* node)
{
    node->next = *self;
    *self = node;
}

static Node*
Node_Copy(Node* self, Copy copy)
{
    return Node_Init(String_Copy(self->key), copy ? copy(self->value) : self->value);
}

static const int
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

static int
Map_Buckets(Map* self)
{
    if(self->prime_index == MAP_UNPRIMED)
        return 0; 
    return Map_Primes[self->prime_index];
}

static bool
Map_Resizable(Map* self)
{
    return self->prime_index < (int) LEN(Map_Primes) - 1;
}

static Map*
Map_Init(Kill kill, Copy copy)
{
    Map* self = Malloc(sizeof(*self));
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

static int
Map_Size(Map* self)
{
    return self->size;
}

static bool
Map_Empty(Map* self)
{
    return Map_Size(self) == 0;
}

static void
Map_Kill(Map* self)
{
    for(int i = 0; i < Map_Buckets(self); i++)
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

static unsigned
Map_Hash(Map* self, char* key)
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
    return hash % Map_Buckets(self);
}

static Node**
Map_Bucket(Map* self, char* key)
{
    unsigned index = Map_Hash(self, key);
    return &self->bucket[index];
}

static void
Map_Alloc(Map* self, int index)
{
    self->prime_index = index;
    self->bucket = calloc(Map_Buckets(self), sizeof(*self->bucket));
}

static void
Map_Rehash(Map* self)
{
    Map* other = Map_Init(self->kill, self->copy);
    Map_Alloc(other, self->prime_index + 1);
    for(int i = 0; i < Map_Buckets(self); i++)
    {
        Node* bucket = self->bucket[i];
        while(bucket)
        {
            Node* next = bucket->next;
            Map_Emplace(other, bucket->key, bucket);
            bucket = next;
        }
    }
    Free(self->bucket);
    *self = *other;
    Free(other);
}

static bool
Map_Full(Map* self)
{
    return Map_Size(self) / (float) Map_Buckets(self) > self->load_factor;
}

static void
Map_Emplace(Map* self, String* key, Node* node)
{
    if(self->prime_index == MAP_UNPRIMED)
        Map_Alloc(self, 0);
    else
    if(Map_Resizable(self) && Map_Full(self))
        Map_Rehash(self);
    Node_Push(Map_Bucket(self, key->value), node);
    self->size += 1;
}

static Node*
Map_At(Map* self, char* key)
{
    if(!Map_Empty(self)) 
    {
        Node* chain = *Map_Bucket(self, key);
        while(chain)
        {
            if(String_Equals(chain->key, key))
                return chain;
            chain = chain->next;
        }
    }
    return NULL;
}

static bool
Map_Exists(Map* self, char* key)
{
    return Map_At(self, key) != NULL;
}

static void
Map_Del(Map* self, char* key)
{
    if(!Map_Empty(self)) 
    {
        Node** head = Map_Bucket(self, key);
        Node* chain = *head;
        Node* prev = NULL;
        while(chain)
        {
            if(String_Equals(chain->key, key))
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

static void
Map_Set(Map* self, String* key, void* value)
{
    Node* found = Map_At(self, key->value);
    if(found)
        Node_Set(found, self->kill, key, value);
    else
    {
        Node* node = Node_Init(key, value);
        Map_Emplace(self, key, node);
    }
}

static void*
Map_Get(Map* self, char* key)
{
    Node* found = Map_At(self, key);
    return found ? found->value : NULL;
}

static Map*
Map_Copy(Map* self)
{
    Map* copy = Map_Init(self->kill, self->copy);
    for(int i = 0; i < Map_Buckets(self); i++)
    {
        Node* chain = self->bucket[i];
        while(chain)
        {
            Node* node = Node_Copy(chain, copy->copy);
            Map_Emplace(copy, node->key, node);
            chain = chain->next;
        }
    }
    return copy;
}

static void
Map_Append(Map* self, Map* other)
{
    for(int i = 0; i < Map_Buckets(other); i++)
    {
        Node* chain = other->bucket[i];
        while(chain)
        {
            Map_Set(self, String_Copy(chain->key), self->copy ? self->copy(chain->value) : chain->value);
            chain = chain->next;
        }
    }
}

static bool
Map_Equal(Map* self, Map* other, Compare compare)
{
    if(Map_Size(self) == Map_Size(other))
    {
        for(int i = 0; i < Map_Buckets(self); i++)
        {
            Node* chain = self->bucket[i];
            while(chain)
            {
                void* got = Map_Get(other, chain->key->value);
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

static String*
Map_Print(Map* self, int indents)
{
    if(Map_Empty(self))
        return String_Init("{}");
    else
    {
        String* print = String_Init("{\n");
        for(int i = 0; i < Map_Buckets(self); i++)
        {
            Node* bucket = self->bucket[i];
            while(bucket)
            {
                String_Append(print, String_Indent(indents + 1));
                String_Append(print, String_Format("\"%s\" : ", bucket->key->value));
                String_Append(print, Value_Print(bucket->value, false, indents + 1));
                if(i < Map_Size(self) - 1)
                    String_Appends(print, ",");
                String_Appends(print, "\n");
                bucket = bucket->next;
            }
        }
        String_Append(print, String_Indent(indents));
        String_Appends(print, "}");
        return print;
    }
}

static Char*
Char_Init(Value* string, int index)
{
    char* value = String_Get(string->of.string, index);
    if(value)
    {
        Char* self = Malloc(sizeof(*self));
        self->string = string;
        self->value = value;
        return self;
    }
    return NULL;
}

static void
Char_Kill(Char* self)
{
    Value_Kill(self->string);
    Free(self);
}

static char*
Type_String(Type self)
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
Type_Kill(Type type, Of of)
{
    switch(type)
    {
    case TYPE_FILE:
        File_Kill(of.file);
        break;
    case TYPE_FUNCTION:
        Function_Kill(of.function);
        break;
    case TYPE_QUEUE:
        Queue_Kill(of.queue);
        break;
    case TYPE_MAP:
        Map_Kill(of.map);
        break;
    case TYPE_STRING:
        String_Kill(of.string);
        break;
    case TYPE_CHAR:
        Char_Kill(of.character);
        break;
    case TYPE_NUMBER:
    case TYPE_BOOL:
    case TYPE_NULL:
        break;
    }
}

static void
Type_Copy(Value* copy, Value* self)
{
    switch(self->type)
    {
    case TYPE_FILE:
        copy->of.file = File_Copy(self->of.file);
        break;
    case TYPE_FUNCTION:
        copy->of.function = Function_Copy(self->of.function);
        break;
    case TYPE_QUEUE:
        copy->of.queue = Queue_Copy(self->of.queue);
        break;
    case TYPE_MAP:
        copy->of.map = Map_Copy(self->of.map);
        break;
    case TYPE_STRING:
        copy->of.string = String_Copy(self->of.string);
        break;
    case TYPE_CHAR:
        copy->type = TYPE_STRING;
        copy->of.string = String_FromChar(*self->of.character->value);
        break;
    case TYPE_NUMBER:
    case TYPE_BOOL:
    case TYPE_NULL:
        copy->of = self->of;
        break;
    }
}

static int
Value_Len(Value* self)
{
    switch(self->type)
    {
    case TYPE_FILE:
        return File_Size(self->of.file);
    case TYPE_FUNCTION:
        return Function_Size(self->of.function);
    case TYPE_QUEUE:
        return Queue_Size(self->of.queue);
    case TYPE_MAP:
        return Map_Size(self->of.map);
    case TYPE_STRING:
        return String_Size(self->of.string);
    case TYPE_CHAR:
    case TYPE_NUMBER:
    case TYPE_BOOL:
    case TYPE_NULL:
        break;
    }
    Quit("vm: type `%s` does not have length", Type_String(self->type));
    return -1;
}

static void
Value_Dec(Value* self)
{
    self->refs -= 1;
}

static void
Value_Inc(Value* self)
{
    self->refs += 1;
}

static void
Value_Kill(Value* self)
{
    if(self->refs == 0)
    {
        Type_Kill(self->type, self->of);
        Free(self);
    }
    else
        Value_Dec(self);
}

#define COMPARE_TABLE(CMP)                        \
    case TYPE_STRING:                             \
        return strcmp(a->of.string->value,        \
                      b->of.string->value) CMP 0; \
    case TYPE_NUMBER:                             \
        return a->of.number                       \
           CMP b->of.number;                      \
    case TYPE_FILE:                               \
        return File_Size(a->of.file)              \
           CMP File_Size(b->of.file);             \
    case TYPE_QUEUE:                              \
        return Queue_Size(a->of.queue)            \
           CMP Queue_Size(b->of.queue);           \
    case TYPE_CHAR:                               \
        return *a->of.character->value            \
           CMP *b->of.character->value;           \
    case TYPE_MAP:                                \
        return Map_Size(a->of.map)                \
           CMP Map_Size(b->of.map);               \
    case TYPE_BOOL:                               \
        return a->of.boolean                      \
           CMP b->of.boolean;                     \
    case TYPE_FUNCTION:                           \
        return Function_Size(a->of.function)      \
           CMP Function_Size(b->of.function);     \
    case TYPE_NULL:                               \
        break;

static bool
Value_LessThan(Value* a, Value* b)
{
    if(a->type == b->type)
        switch(a->type) { COMPARE_TABLE(<); }
    return false;
}

static bool
Value_GreaterThanEqualTo(Value* a, Value* b)
{
    if(a->type == b->type)
        switch(a->type) { COMPARE_TABLE(>=); }
    return false;
}

static bool
Value_GreaterThan(Value* a, Value* b)
{
    if(a->type == b->type)
        switch(a->type) { COMPARE_TABLE(>); }
    return false;
}

static bool
Value_LessThanEqualTo(Value* a, Value* b)
{
    if(a->type == b->type)
        switch(a->type) { COMPARE_TABLE(<=); }
    return false;
}

static bool
Value_Equal(Value* a, Value* b)
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
            return File_Equal(a->of.file, b->of.file);
        case TYPE_FUNCTION:
            return Function_Equal(a->of.function, b->of.function);
        case TYPE_QUEUE:
            return Queue_Equal(a->of.queue, b->of.queue, (Compare) Value_Equal);
        case TYPE_MAP:
            return Map_Equal(a->of.map, b->of.map, (Compare) Value_Equal);
        case TYPE_STRING:
            return String_Equal(a->of.string, b->of.string);
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

static Value*
Value_Copy(Value* self)
{
    Value* copy = Malloc(sizeof(*copy));
    copy->type = self->type;
    copy->refs = 0;
    Type_Copy(copy, self);
    return copy;
}

static Value*
Value_Init(Of of, Type type)
{
    Value* self = Malloc(sizeof(*self));
    self->type = type;
    self->refs = 0;
    switch(type)
    {
    case TYPE_QUEUE:
        self->of.queue = Queue_Init((Kill) Value_Kill, (Copy) Value_Copy);
        break;
    case TYPE_MAP:
        self->of.map = Map_Init((Kill) Value_Kill, (Copy) Value_Copy);
        break;
    case TYPE_FUNCTION:
        self->of.function = of.function;
        break;
    case TYPE_CHAR:
        self->of.character = of.character;
        break;
    case TYPE_STRING:
        self->of.string = of.string;
        break;
    case TYPE_NUMBER:
        self->of.number = of.number;
        break;
    case TYPE_BOOL:
        self->of.boolean = of.boolean;
        break;
    case TYPE_FILE:
        self->of.file = of.file;
        break;
    case TYPE_NULL:
        break;
    }
    return self;
}

static String*
Value_Print(Value* self, bool newline, int indents)
{
    String* print = String_Init("");
    switch(self->type)
    {
    case TYPE_FILE:
        String_Append(print, String_Format("{ \"%s\", \"%s\", %p }", self->of.file->path->value, self->of.file->mode->value, self->of.file->file));
        break;
    case TYPE_FUNCTION:
        String_Append(print, String_Format("{ %s, %d, %d }", self->of.function->name->value, self->of.function->size, self->of.function->address));
        break;
    case TYPE_QUEUE:
        String_Append(print, Queue_Print(self->of.queue, indents));
        break;
    case TYPE_MAP:
        String_Append(print, Map_Print(self->of.map, indents));
        break;
    case TYPE_STRING:
        String_Append(print, String_Format(indents == 0 ? "%s" : "\"%s\"", self->of.string->value));
        break;
    case TYPE_NUMBER:
        String_Append(print, String_Format("%f", self->of.number));
        break;
    case TYPE_BOOL:
        String_Append(print, String_Format("%s", self->of.boolean ? "true" : "false"));
        break;
    case TYPE_CHAR:
        String_Append(print, String_Format(indents == 0 ? "%c" : "\"%c\"", *self->of.character->value));
        break;
    case TYPE_NULL:
        String_Appends(print, "null");
        break;
    }
    if(newline)
        String_Appends(print, "\n");
    return print; 
}

void
Value_Println(Value* self)
{
    String* out = Value_Print(self, false, 0);
    puts(out->value);
    String_Kill(out);
}

static bool
Value_Subbing(Value* a, Value* b)
{
    return a->type == TYPE_CHAR && b->type == TYPE_STRING;
}

static void
Value_Sub(Value* a, Value* b)
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

static bool
Value_Pushing(Value* a, Value* b)
{
    return a->type == TYPE_QUEUE && b->type != TYPE_QUEUE;
}

static bool
Value_CharAppending(Value* a, Value* b)
{
    return a->type == TYPE_STRING && b->type == TYPE_CHAR;
}

static Meta*
Meta_Init(Class class, int stack, String* path)
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
    String_Kill(self->path);
    Free(self);
}

static void
Module_Buffer(Module* self)
{
    self->index = 0;
    self->size = fread(self->buffer, sizeof(*self->buffer), MODULE_BUFFER_SIZE, self->file);
}

static Module*
Module_Init(String* path)
{
    FILE* file = fopen(path->value, "r");
    if(file)
    {
        Module* self = Malloc(sizeof(*self));
        self->file = file;
        self->line = 1;
        self->name = String_Copy(path);
        Module_Buffer(self);
        return self;
    }
    return NULL;
}

static void
Module_Kill(Module* self)
{
    String_Kill(self->name);
    fclose(self->file);
    Free(self);
}

static int
Module_Size(Module* self)
{
    return self->size;
}

static int
Module_Empty(Module* self)
{
    return Module_Size(self) == 0;
}

static int
Module_At(Module* self)
{
    return self->buffer[self->index];
}

static int
Module_Peak(Module* self)
{
    if(self->index == self->size)
        Module_Buffer(self);
    return Module_Empty(self) ? EOF : Module_At(self);
}

static void
Module_Advance(Module* self)
{
    int at = Module_At(self);
    if(at == '\n')
        self->line += 1;
    self->index += 1;
}

static void
CC_Quit(CC* self, const char* const message, ...)
{
    Module* back = Queue_Back(self->modules);
    va_list args;
    va_start(args, message);
    fprintf(stderr, "error: file `%s`: line `%d`: ", back ? back->name->value : "?", back ? back->line : 0);
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(0xFE);
}

static void
CC_Advance(CC* self)
{
    Module_Advance(Queue_Back(self->modules));
}

static int
CC_Peak(CC* self)
{
    int peak;
    while(true)
    {
        peak = Module_Peak(Queue_Back(self->modules));
        if(peak == EOF)
        {
            if(Queue_Size(self->modules) == 1)
                return EOF;
            Queue_PopB(self->modules);
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
        int peak = CC_Peak(self);
        if(peak == '#')
            comment = true;
        if(peak == '\n')
            comment = false;
        if(String_IsSpace(peak) || comment)
            CC_Advance(self);
        else
            break;
    }
}

static int
CC_Next(CC* self)
{
    CC_Spin(self);
    return CC_Peak(self);
}

static int
CC_Read(CC* self)
{
    int peak = CC_Peak(self);
    if(peak != EOF)
        CC_Advance(self);
    return peak;
}

static String*
CC_Stream(CC* self, bool clause(int))
{
    String* str = String_Init("");
    CC_Spin(self);
    while(clause(CC_Peak(self)))
        String_PshB(str, CC_Read(self));
    return str;
}

static void
CC_Match(CC* self, char* expect)
{
    CC_Spin(self);
    while(*expect)
    {
        int peak = CC_Read(self);
        if(peak != *expect)
        {
            char formatted[] = { peak, '\0' };
            CC_Quit(self, "matched character `%s` but expected character `%c`", (peak == EOF) ? "EOF" : formatted, *expect);
        }
        expect += 1;
    }
}

static String*
CC_Mod(CC* self)
{
    return CC_Stream(self, String_IsModule);
}

static String*
CC_Ident(CC* self)
{
    return CC_Stream(self, String_IsIdent);
}

static String*
CC_Operator(CC* self)
{
    return CC_Stream(self, String_IsOp);
}

static String*
CC_Number(CC* self)
{
    return CC_Stream(self, String_IsNumber);
}

static String*
CC_StringStream(CC* self)
{
    String* str = String_Init("");
    CC_Spin(self);
    CC_Match(self, "\"");
    while(CC_Peak(self) != '"')
    {
        int ch = CC_Read(self);
        String_PshB(str, ch);
        if(ch == '\\')
        {
            ch = CC_Read(self);
            int byte = String_EscToByte(ch);
            if(byte == -1)
                CC_Quit(self, "an unknown escape char `0x%02X` was encountered", ch);
            String_PshB(str, ch);
        }
    }
    CC_Match(self, "\"");
    return str;
}

static CC*
CC_Init(void)
{
    CC* self = Malloc(sizeof(*self));
    self->modules = Queue_Init((Kill) Module_Kill, (Copy) NULL);
    self->assembly = Queue_Init((Kill) String_Kill, (Copy) NULL);
    self->data = Queue_Init((Kill) String_Kill, (Copy) NULL);
    self->included = Map_Init((Kill) NULL, (Copy) NULL);
    self->identifiers = Map_Init((Kill) Meta_Kill, (Copy) NULL);
    self->globals = 0;
    self->locals = 0;
    self->labels = 0;
    self->prime = NULL;
    return self;
}

static void
CC_Kill(CC* self)
{
    Queue_Kill(self->modules);
    Queue_Kill(self->assembly);
    Queue_Kill(self->data);
    Map_Kill(self->included);
    Map_Kill(self->identifiers);
    Free(self);
}

static char*
CC_RealPath(CC* self, String* file)
{
    String* path;
    if(Queue_Empty(self->modules))
    {
        path = String_Init("");
        String_Resize(path, 4096);
        getcwd(String_Begin(path), String_Size(path));
        strcat(String_Begin(path), "/");
        strcat(String_Begin(path), file->value);
    }
    else
    {
        Module* back = Queue_Back(self->modules);
        path = String_Base(back->name);
        String_Appends(path, file->value);
    }
    char* real = realpath(path->value, NULL);
    if(real == NULL)
        CC_Quit(self, "`%s` could not be resolved as a real path", path->value);
    String_Kill(path);
    return real;
}

static void
CC_IncludeModule(CC* self, String* file)
{
    char* real = CC_RealPath(self, file);
    if(Map_Exists(self->included, real))
        Free(real);
    else
    {
        String* resolved = String_Moves(&real);
        Map_Set(self->included, resolved, NULL);
        Queue_PshB(self->modules, Module_Init(resolved));
    }
}

static String*
CC_Parents(String* module)
{
    String* parents = String_Init("");
    for(char* value = module->value; *value; value++)
    {
        if(*value != '.')
            break;
        String_Appends(parents, "../");
    }
    return parents;
}

static String*
CC_ModuleName(CC* self, char* postfix)
{
    String* module = CC_Mod(self);
    String* skipped = String_Skip(module, '.');
    String_Replace(skipped, '.', '/');
    String_Appends(skipped, postfix);
    String* name = CC_Parents(module);
    String_Appends(name, skipped->value);
    String_Kill(skipped);
    String_Kill(module);
    return name;
}

static void
CC_Include(CC* self)
{
    String* name = CC_ModuleName(self, ".rr");
    CC_Match(self, ";");
    CC_IncludeModule(self, name);
    String_Kill(name);
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
CC_Define(CC* self, Class class, int stack, String* ident, String* path)
{
    Meta* old = Map_Get(self->identifiers, ident->value);
    Meta* new = Meta_Init(class, stack, path);
    if(old)
    {
        // Only function prototypes can be upgraded to functions.
        if(old->class == CLASS_FUNCTION_PROTOTYPE
        && new->class == CLASS_FUNCTION)
        {
            if(new->stack != old->stack)
                CC_Quit(self, "function `%s` with `%d` argument(s) was previously defined as a function prototype with `%d` argument(s)", ident->value, new->stack, old->stack);
        }
        else
            CC_Quit(self, "%s `%s` was already defined as a %s", Class_String(new->class), ident->value, Class_String(old->class));
    }
    Map_Set(self->identifiers, ident, new);
}

static void
CC_ConsumeExpression(CC* self)
{
    CC_Expression(self);
    Queue_PshB(self->assembly, String_Init("\tpop"));
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
CC_Local(CC* self, String* ident)
{
    CC_Define(self, CLASS_VARIABLE_LOCAL, self->locals, ident, String_Init(""));
    self->locals += 1;
}

static void
CC_AssignLocal(CC* self, String* ident)
{
    CC_Assign(self);
    CC_Local(self, ident);
}

static String*
CC_Global(CC* self, String* ident)
{
    String* label = String_Format("!%s", ident->value);
    Queue_PshB(self->assembly, String_Format("%s:", label->value));
    CC_Assign(self);
    CC_Define(self, CLASS_VARIABLE_GLOBAL, self->globals, ident, String_Init(""));
    Queue_PshB(self->assembly, String_Init("\tret"));
    self->globals += 1;
    return label;
}

static Queue*
CC_ParamRoll(CC* self)
{
    Queue* params = Queue_Init((Kill) String_Kill, (Copy) NULL);
    CC_Match(self, "(");
    while(CC_Next(self) != ')')
    {
        String* ident = CC_Ident(self);
        Queue_PshB(params, ident);
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    CC_Match(self, ")");
    return params;
}

static void
CC_DefineParams(CC* self, Queue* params)
{
    self->locals = 0;
    for(int i = 0; i < Queue_Size(params); i++)
    {
        String* ident = String_Copy(Queue_Get(params, i));
        CC_Local(self, ident);
    }
}

static int
CC_PopScope(CC* self, Queue* scope)
{
    int popped = Queue_Size(scope);
    for(int i = 0; i < popped; i++)
    {
        String* key = Queue_Get(scope, i);
        Map_Del(self->identifiers, key->value);
        self->locals -= 1;
    }
    for(int i = 0; i < popped; i++)
        Queue_PshB(self->assembly, String_Init("\tpop"));
    Queue_Kill(scope);
    return popped;
}

static Meta*
CC_Meta(CC* self, String* ident)
{
    Meta* meta = Map_Get(self->identifiers, ident->value);
    if(meta == NULL)
        CC_Quit(self, "identifier `%s` not defined", ident->value);
    return meta;
}

static Meta*
CC_Expect(CC* self, String* ident, bool clause(Class))
{
    Meta* meta = CC_Meta(self, ident);
    if(!clause(meta->class))
        CC_Quit(self, "identifier `%s` cannot be of class `%s`", ident->value, Class_String(meta->class));
    return meta;
}

static void
CC_Ref(CC* self, String* ident)
{
    Meta* meta = CC_Expect(self, ident, CC_IsVariable);
    if(meta->class == CLASS_VARIABLE_GLOBAL)
        Queue_PshB(self->assembly, String_Format("\tglb %d", meta->stack));
    else
    if(meta->class == CLASS_VARIABLE_LOCAL)
        Queue_PshB(self->assembly, String_Format("\tloc %d", meta->stack));
}

static void
CC_Resolve(CC* self)
{
    while(CC_Next(self) == '[')
    {
        CC_Match(self, "[");
        CC_Expression(self);
        CC_Match(self, "]");
        if(CC_Next(self) == ':')
        {
            CC_Match(self, ":=");
            CC_Expression(self);
            Queue_PshB(self->assembly, String_Init("\tins"));
        }
        else
            Queue_PshB(self->assembly, String_Init("\tget"));
    }
}

static int
CC_Args(CC* self)
{
    CC_Match(self, "(");
    int args = 0;
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
CC_Call(CC* self, String* ident, int args)
{
    for(int i = 0; i < args; i++)
        Queue_PshB(self->assembly, String_Init("\tspd"));
    Queue_PshB(self->assembly, String_Format("\tcal %s", ident->value));
    Queue_PshB(self->assembly, String_Init("\tlod"));
}

static void
CC_IndirectCalling(CC* self, String* ident, int args)
{
    CC_Ref(self, ident);
    Queue_PshB(self->assembly, String_Format("\tpsh %d", args));
    Queue_PshB(self->assembly, String_Init("\tvrt"));
    Queue_PshB(self->assembly, String_Init("\tlod"));
}

static void
CC_NativeCalling(CC* self, String* ident, Meta* meta)
{
    Queue_PshB(self->assembly, String_Format("\tpsh \"%s\"", meta->path->value));
    Queue_PshB(self->assembly, String_Format("\tpsh \"%s\"", ident->value));
    Queue_PshB(self->assembly, String_Format("\tpsh %d", meta->stack));
    Queue_PshB(self->assembly, String_Init("\tdll"));
    Queue_PshB(self->assembly, String_Init("\tpsh null"));
}

static void
CC_Map(CC* self)
{
    Queue_PshB(self->assembly, String_Init("\tpsh {}"));
    CC_Match(self, "{");
    while(CC_Next(self) != '}')
    {
        CC_Expression(self);
        if(CC_Next(self) == ':')
        {
            CC_Match(self, ":");
            CC_Expression(self);
        }
        else
            Queue_PshB(self->assembly, String_Init("\tpsh true"));
        Queue_PshB(self->assembly, String_Init("\tins"));
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    CC_Match(self, "}");
}

static void
CC_Queue(CC* self)
{
    Queue_PshB(self->assembly, String_Init("\tpsh []"));
    CC_Match(self, "[");
    while(CC_Next(self) != ']')
    {
        CC_Expression(self);
        Queue_PshB(self->assembly, String_Init("\tpsb"));
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
    Queue_PshB(self->assembly, String_Init("\tnot"));
}

static void
CC_Direct(CC* self)
{
    String* number = CC_Number(self);
    Queue_PshB(self->assembly, String_Format("\tpsh %s", number->value));
    String_Kill(number);
}

static void
CC_DirectCalling(CC* self, String* ident, int args)
{
    if(String_Equals(ident, "len"))
        Queue_PshB(self->assembly, String_Init("\tlen"));
    else
    if(String_Equals(ident, "good"))
        Queue_PshB(self->assembly, String_Init("\tgod"));
    else
    if(String_Equals(ident, "key"))
        Queue_PshB(self->assembly, String_Init("\tkey"));
    else
    if(String_Equals(ident, "sort"))
        Queue_PshB(self->assembly, String_Init("\tsrt"));
    else
    if(String_Equals(ident, "open"))
        Queue_PshB(self->assembly, String_Init("\topn"));
    else
    if(String_Equals(ident, "read"))
        Queue_PshB(self->assembly, String_Init("\tred"));
    else
    if(String_Equals(ident, "copy"))
        Queue_PshB(self->assembly, String_Init("\tcpy"));
    else
    if(String_Equals(ident, "write"))
        Queue_PshB(self->assembly, String_Init("\twrt"));
    else
    if(String_Equals(ident, "refs"))
        Queue_PshB(self->assembly, String_Init("\tref"));
    else
    if(String_Equals(ident, "del"))
        Queue_PshB(self->assembly, String_Init("\tdel"));
    else
    if(String_Equals(ident, "type"))
        Queue_PshB(self->assembly, String_Init("\ttyp"));
    else
    if(String_Equals(ident, "print"))
        Queue_PshB(self->assembly, String_Init("\tprt"));
    else
        CC_Call(self, ident, args);
}

static void
CC_Calling(CC* self, String* ident)
{
    Meta* meta = CC_Meta(self, ident);
    int args = CC_Args(self);
    if(CC_IsFunction(meta->class))
    {
        if(args != meta->stack)
            CC_Quit(self, "calling function `%s` required `%d` arguments but got `%d` arguments", ident->value, meta->stack, args);
        if(meta->class == CLASS_FUNCTION_PROTOTYPE_NATIVE)
            CC_NativeCalling(self, ident, meta);
        else
            CC_DirectCalling(self, ident, args);
    }
    else
    if(CC_IsVariable(meta->class))
        CC_IndirectCalling(self, ident, args);
    else
        CC_Quit(self, "identifier `%s` is not callable", ident->value);
}

static bool
CC_Referencing(CC* self, String* ident)
{
    Meta* meta = CC_Meta(self, ident);
    if(CC_IsFunction(meta->class))
        Queue_PshB(self->assembly, String_Format("\tpsh @%s,%d", ident->value, meta->stack));
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
    String* ident = NULL;
    if(self->prime)
        String_Swap(&self->prime, &ident);
    else
        ident = CC_Ident(self);
    if(String_IsBoolean(ident))
        Queue_PshB(self->assembly, String_Format("\tpsh %s", ident->value));
    else
    if(String_IsNull(ident))
        Queue_PshB(self->assembly, String_Format("\tpsh %s", ident->value));
    else
    if(CC_Next(self) == '(')
        CC_Calling(self, ident);
    else
        storage = CC_Referencing(self, ident);
    String_Kill(ident);
    return storage;
}

static void
CC_ReserveFunctions(CC* self)
{
    struct { int args; char* name; } items[] = {
        { 2, "sort"  },
        { 1, "good"  },
        { 1, "key"   },
        { 1, "copy"  },
        { 2, "open"  },
        { 2, "read"  },
        { 1, "close" },
        { 1, "refs"  },
        { 1, "len"   },
        { 2, "del"   },
        { 1, "type"  },
        { 1, "print" },
        { 2, "write" },
    };
    for(int i = 0; i < (int) LEN(items); i++)
        CC_Define(self, CLASS_FUNCTION, items[i].args, String_Init(items[i].name), String_Init(""));
}

static void
CC_Force(CC* self)
{
    CC_Match(self, "(");
    CC_Expression(self);
    CC_Match(self, ")");
}

static void
CC_String(CC* self)
{
    String* string = CC_StringStream(self);
    Queue_PshB(self->assembly, String_Format("\tpsh \"%s\"", string->value));
    String_Kill(string);
}

static bool
CC_Factor(CC* self)
{
    bool storage = false;
    int next = CC_Next(self);
    if(next == '!')
        CC_Not(self);
    else
    if(String_IsDigit(next))
        CC_Direct(self);
    else
    if(String_IsIdent(next) || self->prime)
        storage = CC_Identifier(self);
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
        CC_Quit(self, "an unknown factor starting with `%c` was encountered", next);
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
        String* operator = CC_Operator(self);
        if(String_Equals(operator, "*="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator `%s` must be storage", operator);
            CC_Expression(self);
            Queue_PshB(self->assembly, String_Init("\tmul"));
        }
        else
        if(String_Equals(operator, "/="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator `%s` must be storage", operator);
            CC_Expression(self);
            Queue_PshB(self->assembly, String_Init("\tdiv"));
        }
        else
        {
            storage = false;
            if(String_Equals(operator, "?"))
            {
                CC_Factor(self);
                Queue_PshB(self->assembly, String_Init("\tmem"));
            }
            else
            {
                Queue_PshB(self->assembly, String_Init("\tcpy"));
                CC_Factor(self);
                if(String_Equals(operator, "*"))
                    Queue_PshB(self->assembly, String_Init("\tmul"));
                else
                if(String_Equals(operator, "/"))
                    Queue_PshB(self->assembly, String_Init("\tdiv"));
                else
                if(String_Equals(operator, "%"))
                    Queue_PshB(self->assembly, String_Init("\tfmt"));
                else
                if(String_Equals(operator, "||"))
                    Queue_PshB(self->assembly, String_Init("\tlor"));
                else
                    CC_Quit(self, "operator `%s` is not supported for factors", operator->value);
            }
        }
        String_Kill(operator);
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
        String* operator = CC_Operator(self);
        if(String_Equals(operator, "="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator `%s` must be storage", operator);
            CC_Expression(self);
            Queue_PshB(self->assembly, String_Init("\tmov"));
        }
        else
        if(String_Equals(operator, "+="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator `%s` must be storage", operator);
            CC_Expression(self);
            Queue_PshB(self->assembly, String_Init("\tadd"));
        }
        else
        if(String_Equals(operator, "-="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator `%s` must be storage", operator);
            CC_Expression(self);
            Queue_PshB(self->assembly, String_Init("\tsub"));
        }
        else
        if(String_Equals(operator, "=="))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, String_Init("\teql"));
        }
        else
        if(String_Equals(operator, "!="))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, String_Init("\tneq"));
        }
        else
        if(String_Equals(operator, ">"))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, String_Init("\tgrt"));
        }
        else
        if(String_Equals(operator, "<"))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, String_Init("\tlst"));
        }
        else
        if(String_Equals(operator, ">="))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, String_Init("\tgte"));
        }
        else
        if(String_Equals(operator, "<="))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, String_Init("\tlte"));
        }
        else
        {
            storage = false;
            Queue_PshB(self->assembly, String_Init("\tcpy"));
            CC_Term(self);
            if(String_Equals(operator, "+"))
                Queue_PshB(self->assembly, String_Init("\tadd"));
            else
            if(String_Equals(operator, "-"))
                Queue_PshB(self->assembly, String_Init("\tsub"));
            else
            if(String_Equals(operator, "&&"))
                Queue_PshB(self->assembly, String_Init("\tand"));
            else
                CC_Quit(self, "operator `%s` is not supported for terms", operator->value);
        }
        String_Kill(operator);
    }
}

static int
CC_Label(CC* self)
{
    int label = self->labels;
    self->labels += 1;
    return label;
}

static void
CC_Branch(CC* self, int head, int tail, int end, bool loop)
{
    int next = CC_Label(self);
    CC_Match(self, "(");
    CC_Expression(self);
    CC_Match(self, ")");
    Queue_PshB(self->assembly, String_Format("\tbrf @l%d", next));
    CC_Block(self, head, tail, loop);
    Queue_PshB(self->assembly, String_Format("\tjmp @l%d", end));
    Queue_PshB(self->assembly, String_Format("@l%d:", next));
}

static String*
CC_Branches(CC* self, int head, int tail, int loop)
{
    int end = CC_Label(self);
    CC_Branch(self, head, tail, end, loop);
    String* buffer = CC_Ident(self);
    while(String_Equals(buffer, "elif"))
    {
        CC_Branch(self, head, tail, end, loop);
        String_Kill(buffer);
        buffer = CC_Ident(self);
    }
    if(String_Equals(buffer, "else"))
        CC_Block(self, head, tail, loop);
    Queue_PshB(self->assembly, String_Format("@l%d:", end));
    String* backup = NULL;
    if(String_Empty(buffer) || String_Equals(buffer, "elif") || String_Equals(buffer, "else"))
        String_Kill(buffer);
    else
        backup = buffer;
    return backup;
}

static void
CC_While(CC* self)
{
    int A = CC_Label(self);
    int B = CC_Label(self);
    Queue_PshB(self->assembly, String_Format("@l%d:", A));
    CC_Match(self, "(");
    CC_Expression(self);
    Queue_PshB(self->assembly, String_Format("\tbrf @l%d", B));
    CC_Match(self, ")");
    CC_Block(self, A, B, true);
    Queue_PshB(self->assembly, String_Format("\tjmp @l%d", A));
    Queue_PshB(self->assembly, String_Format("@l%d:", B));
}

static void
CC_For(CC* self)
{
    int A = CC_Label(self);
    int B = CC_Label(self);
    int C = CC_Label(self);
    int D = CC_Label(self);
    Queue* init = Queue_Init((Kill) String_Kill, (Copy) NULL);
    CC_Match(self, "(");
    String* ident = CC_Ident(self);
    Queue_PshB(init, String_Copy(ident));
    CC_AssignLocal(self, ident);
    Queue_PshB(self->assembly, String_Format("@l%d:", A));
    CC_Expression(self);
    CC_Match(self, ";");
    Queue_PshB(self->assembly, String_Format("\tbrf @l%d", D));
    Queue_PshB(self->assembly, String_Format("\tjmp @l%d", C));
    Queue_PshB(self->assembly, String_Format("@l%d:", B));
    CC_ConsumeExpression(self);
    CC_Match(self, ")");
    Queue_PshB(self->assembly, String_Format("\tjmp @l%d", A));
    Queue_PshB(self->assembly, String_Format("@l%d:", C));
    CC_Block(self, B, D, true);
    Queue_PshB(self->assembly, String_Format("\tjmp @l%d", B));
    Queue_PshB(self->assembly, String_Format("@l%d:", D));
    CC_PopScope(self, init);
}

static void
CC_Ret(CC* self)
{
    CC_Expression(self);
    Queue_PshB(self->assembly, String_Init("\tsav"));
    Queue_PshB(self->assembly, String_Init("\tfls"));
    CC_Match(self, ";");
}

static void
CC_Block(CC* self, int head, int tail, bool loop)
{
    Queue* scope = Queue_Init((Kill) String_Kill, (Copy) NULL);
    CC_Match(self, "{");
    String* prime = NULL; 
    while(CC_Next(self) != '}')
    {
        if(String_IsIdentLeader(CC_Next(self)) || prime)
        {
            String* ident = NULL;
            if(prime)
                String_Swap(&prime, &ident);
            else
                ident = CC_Ident(self);
            if(String_Equals(ident, "if"))
                prime = CC_Branches(self, head, tail, loop);
            else
            if(String_Equals(ident, "elif"))
                CC_Quit(self, "the keyword `elif` must follow an `if` or `elif` block");
            else
            if(String_Equals(ident, "else"))
                CC_Quit(self, "the keyword `else` must follow an `if` or `elif` block");
            else
            if(String_Equals(ident, "while"))
                CC_While(self);
            else
            if(String_Equals(ident, "for"))
                CC_For(self);
            else
            if(String_Equals(ident, "ret"))
                CC_Ret(self);
            else
            if(String_Equals(ident, "continue"))
            {
                if(loop)
                {
                    CC_Match(self, ";");
                    Queue_PshB(self->assembly, String_Format("\tjmp @l%d", head));
                }
                else
                    CC_Quit(self, "the keyword `continue` can only be used within `while` or for `loops`");
            }
            else
            if(String_Equals(ident, "break"))
            {
                if(loop)
                {
                    CC_Match(self, ";");
                    Queue_PshB(self->assembly, String_Format("\tjmp @l%d", tail));
                }
                else
                    CC_Quit(self, "the keyword `break` can only be used within `while` or `for` loops");
            }
            else
            if(CC_Next(self) == ':')
            {
                Queue_PshB(scope, String_Copy(ident));
                CC_AssignLocal(self, String_Copy(ident));
            }
            else
            {
                self->prime = String_Copy(ident);
                CC_EmptyExpression(self);
            }
            String_Kill(ident);
        }
        else
            CC_EmptyExpression(self);
    }
    CC_Match(self, "}");
    CC_PopScope(self, scope);
}

static void
CC_FunctionPrototypeNative(CC* self, Queue* params, String* ident, String* path)
{
    CC_Define(self, CLASS_FUNCTION_PROTOTYPE_NATIVE, Queue_Size(params), ident, path);
    Queue_Kill(params);
    CC_Match(self, ";");
}

static void
CC_FunctionPrototype(CC* self, Queue* params, String* ident)
{
    CC_Define(self, CLASS_FUNCTION_PROTOTYPE, Queue_Size(params), ident, String_Init(""));
    Queue_Kill(params);
    CC_Match(self, ";");
}

static void
CC_ImportModule(CC* self, String* file)
{
    char* real = CC_RealPath(self, file);
    String* source = String_Init(real);
    String* library = String_Init(real);
    String_PopB(library);
    String_Appends(library, "so");
    if(Map_Exists(self->included, library->value) == false)
    {
        String* command = String_Format("gcc %s %s -fpic -shared -o %s", source->value, ROMAN2SO, library->value);
        int ret = system(command->value);
        if(ret != 0)
            CC_Quit(self, "compilation errors encountered while compiling native library `%s`", source->value);
        String_Kill(command);
        Map_Set(self->included, String_Copy(library), NULL);
    }
    CC_Match(self, "{");
    while(CC_Next(self) != '}')
    {
        String* ident = CC_Ident(self);
        Queue* params = CC_ParamRoll(self);
        CC_FunctionPrototypeNative(self, params, ident, String_Copy(library));
    }
    CC_Match(self, "}");
    CC_Match(self, ";");
    String_Kill(library);
    String_Kill(source);
    Free(real);
}

static void
CC_Import(CC* self)
{
    String* name = CC_ModuleName(self, ".c");
    CC_ImportModule(self, name);
    String_Kill(name);
}

static void
CC_Function(CC* self, String* ident)
{
    Queue* params = CC_ParamRoll(self);
    if(CC_Next(self) == '{')
    {
        CC_DefineParams(self, params);
        CC_Define(self, CLASS_FUNCTION, Queue_Size(params), ident, String_Init(""));
        Queue_PshB(self->assembly, String_Format("%s:", ident->value));
        CC_Block(self, 0, 0, false);
        CC_PopScope(self, params);
        Queue_PshB(self->assembly, String_Init("\tpsh null"));
        Queue_PshB(self->assembly, String_Init("\tsav"));
        Queue_PshB(self->assembly, String_Init("\tret"));
    }
    else
        CC_FunctionPrototype(self, params, ident);
}

static void
CC_Spool(CC* self, Queue* start)
{
    Queue* spool = Queue_Init((Kill) String_Kill, NULL);
    String* main = String_Init("main");
    CC_Expect(self, main, CC_IsFunction);
    Queue_PshB(spool, String_Init("!start:"));
    for(int i = 0; i < Queue_Size(start); i++)
    {
        String* label = Queue_Get(start, i);
        Queue_PshB(spool, String_Format("\tcal %s", label->value));
    }
    Queue_PshB(spool, String_Format("\tcal %s", main->value));
    Queue_PshB(spool, String_Format("\tend"));
    for(int i = Queue_Size(spool) - 1; i >= 0; i--)
        Queue_PshF(self->assembly, String_Copy(Queue_Get(spool, i)));
    String_Kill(main);
    Queue_Kill(spool);
}

static void
CC_Parse(CC* self)
{
    Queue* start = Queue_Init((Kill) String_Kill, (Copy) NULL);
    while(CC_Peak(self) != EOF)
    {
        String* ident = CC_Ident(self);
        if(String_Equals(ident, "inc"))
        {
            CC_Include(self);
            String_Kill(ident);
        }
        else
        if(String_Equals(ident, "imp"))
        {
            CC_Import(self);
            String_Kill(ident);
        }
        else
        if(CC_Next(self) == '(')
            CC_Function(self, ident);
        else
        if(CC_Next(self) == ':')
        {
            String* label = CC_Global(self, ident);
            Queue_PshB(start, label);
        }
        else
            CC_Quit(self, "`%s` must either be a function or function prototype, a global variable, an import statement, or an include statement");
        CC_Spin(self);
    }
    CC_Spool(self, start);
    Queue_Kill(start);
}

static Map*
ASM_Label(Queue* assembly, int* size)
{
    Map* labels = Map_Init((Kill) Free, (Copy) NULL);
    for(int i = 0; i < Queue_Size(assembly); i++)
    {
        String* stub = Queue_Get(assembly, i);
        if(stub->value[0] == '\t')
            *size += 1;
        else 
        {
            String* line = String_Copy(stub);
            char* label = strtok(line->value, ":");
            if(Map_Exists(labels, label))
                Quit("asm: label %s already defined", label);
            Map_Set(labels, String_Init(label), Int_Init(*size));
            String_Kill(line);
        }
    }
    return labels;
}

static void
ASM_Dump(Queue* assembly)
{
    for(int i = 0; i < Queue_Size(assembly); i++)
    {
        String* assem = Queue_Get(assembly, i);
        puts(assem->value);
    }
}

static void
VM_TypeExpect(VM* self, Type a, Type b)
{
    (void) self;
    if(a != b)
        Quit("`vm: encountered type `%s` but expected type `%s`", Type_String(a), Type_String(b));
}

static void
VM_BadOperator(VM* self, Type a, const char* op)
{
    (void) self;
    Quit("vm: type `%s` not supported with operator `%s`", Type_String(a), op);
}

static void
VM_TypeMatch(VM* self, Type a, Type b, const char* op)
{
    (void) self;
    if(a != b)
        Quit("vm: type `%s` and type `%s` mismatch with operator `%s`", Type_String(a), Type_String(b), op);
}

static void
VM_OutOfBounds(VM* self, Type a, int index)
{
    (void) self;
    Quit("vm: type `%s` was accessed out of bounds with index `%d`", Type_String(a), index);
}

static void
VM_TypeBadIndex(VM* self, Type a, Type b)
{
    (void) self;
    Quit("vm: type `%s` cannot be indexed with type `%s`", Type_String(a), Type_String(b));
}

static void
VM_TypeBad(VM* self, Type a)
{
    (void) self;
    Quit("vm: type `%s` cannot be used for this operation", Type_String(a));
}

static void
VM_ArgMatch(VM* self, int a, int b)
{
    (void) self;
    if(a != b)
        Quit("vm: expected `%d` arguments but encountered `%d` arguments", a, b);
}

static void
VM_UnknownEscapeChar(VM* self, int esc)
{
    (void) self;
    Quit("vm: an unknown escape character `0x%02X` was encountered\n", esc);
}

static void
VM_RefImpurity(VM* self, Value* value)
{
    (void) self;
    String* print = Value_Print(value, true, 0);
    Quit("vm: the .data segment value `%s` contained `%d` references at the time of exit", print->value, value->refs);
}

static VM*
VM_Init(int size)
{
    VM* self = Malloc(sizeof(*self));
    self->data = Queue_Init((Kill) Value_Kill, (Copy) NULL);
    self->stack = Queue_Init((Kill) Value_Kill, (Copy) NULL);
    self->frame = Queue_Init((Kill) Frame_Free, (Copy) NULL);
    self->track = Map_Init((Kill) Int_Kill, (Copy) NULL);
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
    Queue_Kill(self->data);
    Queue_Kill(self->stack);
    Queue_Kill(self->frame);
    Map_Kill(self->track);
    Free(self->instructions);
    Free(self);
}

static void
VM_Data(VM* self)
{
    fprintf(stderr, ".data:\n");
    for(int i = 0; i < Queue_Size(self->data); i++)
    {
        Value* value = Queue_Get(self->data, i);
        fprintf(stderr, "%4d : %2d : ", i, value->refs);
        String* print = Value_Print(value, false, 0);
        fprintf(stderr, "%s\n", print->value);
        String_Kill(print);
    }
}

static void
VM_AssertRefCounts(VM* self)
{
    for(int i = 0; i < Queue_Size(self->data); i++)
    {
        Value* value = Queue_Get(self->data, i);
        if(value->refs > 0)
            VM_RefImpurity(self, value);
    }
}

static void
VM_Pop(VM* self)
{
    Queue_PopB(self->stack);
}

static String*
VM_ConvertEscs(VM* self, char* chars)
{
    int len = strlen(chars);
    String* string = String_Init("");
    for(int i = 1; i < len - 1; i++)
    {
        char ch = chars[i];
        if(ch == '\\')
        {
            i += 1;
            int esc = chars[i];
            ch = String_EscToByte(esc);
            if(ch == -1)
                VM_UnknownEscapeChar(self, esc);
        }
        String_PshB(string, ch);
    }
    return string;
}

static void
VM_Store(VM* self, Map* labels, char* operand)
{
    Of of;
    Value* value;
    char ch = operand[0];
    if(ch == '[')
        value = Value_Init(of, TYPE_QUEUE);
    else
    if(ch == '{')
        value = Value_Init(of, TYPE_MAP);
    else
    if(ch == '"')
    {
        of.string = VM_ConvertEscs(self, operand);
        value = Value_Init(of, TYPE_STRING);
    }
    else
    if(ch == '@')
    {
        String* name = String_Init(strtok(operand + 1, ","));
        int size = atoi(strtok(NULL, " \n"));
        int* address = Map_Get(labels, name->value);
        if(address == NULL)
            Quit("asm: label `%s` not defined", name);
        of.function = Function_Init(name, size, *address);
        value = Value_Init(of, TYPE_FUNCTION);
    }
    else
    if(ch == 't' || ch == 'f')
    {
        of.boolean = Equals(operand, "true") ? true : false;
        value = Value_Init(of, TYPE_BOOL);
    }
    else
    if(ch == 'n')
        value = Value_Init(of, TYPE_NULL);
    else
    if(String_IsNumber(ch))
    {
        of.number = atof(operand);
        value = Value_Init(of, TYPE_NUMBER);
    }
    else
        Quit("vm: unknown psh operand `%s` encountered", operand);
    Queue_PshB(self->data, value);
}

static int
VM_Datum(VM* self, Map* labels, char* operand)
{
    VM_Store(self, labels, operand);
    return ((Queue_Size(self->data) - 1) << 8) | OPCODE_PSH;
}

static int
VM_Indirect(Opcode oc, Map* labels, char* label)
{
    int* address = Map_Get(labels, label);
    if(address == NULL)
        Quit("asm: label `%s` not defined", label);
    return *address << 8 | oc;
}

static int
VM_Direct(Opcode oc, char* number)
{
    return (atoi(number) << 8) | oc;
}

static VM*
VM_Assemble(Queue* assembly)
{
    int size = 0;
    Map* labels = ASM_Label(assembly, &size);
    VM* self = VM_Init(size);
    int pc = 0;
    for(int i = 0; i < Queue_Size(assembly); i++)
    {
        String* stub = Queue_Get(assembly, i);
        if(stub->value[0] == '\t')
        {
            String* line = String_Init(stub->value + 1);
            int instruction = 0;
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
                Quit("asm: unknown mnemonic `%s`", mnemonic);
            self->instructions[pc] = instruction;
            pc += 1;
            String_Kill(line);
        }
    }
    Map_Kill(labels);
    return self;
}

static void
VM_Cal(VM* self, int address)
{
    int sp = Queue_Size(self->stack) - self->spds;
    Queue_PshB(self->frame, Frame_Init(self->pc, sp));
    self->pc = address;
    self->spds = 0;
}

static void
VM_Cpy(VM* self)
{
    Value* back = Queue_Back(self->stack);
    Value* value = Value_Copy(back);
    VM_Pop(self);
    Queue_PshB(self->stack, value);
}

static int
VM_End(VM* self)
{
    VM_TypeExpect(self, self->ret->type, TYPE_NUMBER);
    int ret = self->ret->of.number;
    Value_Kill(self->ret);
    return ret;
}

static void
VM_Fls(VM* self)
{
    Frame* frame = Queue_Back(self->frame);
    int pops = Queue_Size(self->stack) - frame->sp;
    while(pops > 0)
    {
        VM_Pop(self);
        pops -= 1;
    }
    self->pc = frame->pc;
    Queue_PopB(self->frame);
}

static void
VM_Glb(VM* self, int address)
{
    Value* value = Queue_Get(self->stack, address);
    Value_Inc(value);
    Queue_PshB(self->stack, value);
}

static void
VM_Loc(VM* self, int address)
{
    Frame* frame = Queue_Back(self->frame);
    Value* value = Queue_Get(self->stack, frame->sp + address);
    Value_Inc(value);
    Queue_PshB(self->stack, value);
}

static void
VM_Jmp(VM* self, int address)
{
    self->pc = address;
}

static void
VM_Ret(VM* self)
{
    Frame* frame = Queue_Back(self->frame);
    self->pc = frame->pc;
    Queue_PopB(self->frame);
}

static void
VM_Lod(VM* self)
{
    Queue_PshB(self->stack, self->ret);
}

static void
VM_Sav(VM* self)
{
    Value* value = Queue_Back(self->stack);
    Value_Inc(value);
    self->ret = value;
}

static void
VM_Psh(VM* self, int address)
{
    Value* value = Queue_Get(self->data, address);
    Value* copy = Value_Copy(value);
    Queue_PshB(self->stack, copy);
}

static void
VM_Mov(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(Value_Subbing(a, b))
        Value_Sub(a, b);
    else
    if(a->type == b->type)
    {
        Type_Kill(a->type, a->of);
        Type_Copy(a, b);
    }
    else
        VM_TypeMatch(self, a->type, b->type, "=");
    VM_Pop(self);
}

static void
VM_Add(VM* self)
{
    char* operator = "+";
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(Value_Pushing(a, b))
    {
        Value_Inc(b);
        Queue_PshB(a->of.queue, b);
    }
    else
    if(Value_CharAppending(a, b))
        String_PshB(a->of.string, *b->of.character->value);
    else
    if(a->type == b->type)
        switch(a->type)
        {
        case TYPE_QUEUE:
            Queue_Append(a->of.queue, b->of.queue);
            break;
        case TYPE_MAP:
            Map_Append(a->of.map, b->of.map);
            break;
        case TYPE_STRING:
            String_Appends(a->of.string, b->of.string->value);
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
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(Value_Pushing(a, b))
    {
        Value_Inc(b);
        Queue_PshF(a->of.queue, b);
    }
    else
    if(a->type == b->type)
        switch(a->type)
        {
        case TYPE_QUEUE:
            Queue_Prepend(a->of.queue, b->of.queue);
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
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, a->type, b->type, "*");
    VM_TypeExpect(self, a->type, TYPE_NUMBER);
    a->of.number *= b->of.number;
    VM_Pop(self);
}

static void
VM_Div(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, a->type, b->type, "/");
    VM_TypeExpect(self, a->type, TYPE_NUMBER);
    a->of.number /= b->of.number;
    VM_Pop(self);
}

static void
VM_Dll(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 3);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* c = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, a->type, TYPE_STRING);
    VM_TypeExpect(self, b->type, TYPE_STRING);
    VM_TypeExpect(self, c->type, TYPE_NUMBER);
    typedef void* (*Fun)();
    Fun fun;
    void* so = dlopen(a->of.string->value, RTLD_NOW | RTLD_GLOBAL);
    if(so == NULL)
        Quit("vm: could not open shared object library `%s`\n", a->of.string->value);
    *(void**)(&fun) = dlsym(so, b->of.string->value);
    if(fun == NULL)
        Quit("vm: could not open shared object function `%s` from shared object library `%s`\n", b->of.string->value, a->of.string->value);
    int start = Queue_Size(self->stack) - 3;
    int args = c->of.number;
    switch(args)
    {
    case 0: fun(); break;
    case 1: fun(Queue_Get(self->stack, start - 1));
            break;
    case 2: fun(Queue_Get(self->stack, start - 1),
                Queue_Get(self->stack, start - 2));
            break;
    case 3: fun(Queue_Get(self->stack, start - 1),
                Queue_Get(self->stack, start - 2),
                Queue_Get(self->stack, start - 3));
            break;
    case 4: fun(Queue_Get(self->stack, start - 1),
                Queue_Get(self->stack, start - 2),
                Queue_Get(self->stack, start - 3),
                Queue_Get(self->stack, start - 4));
            break;
    // MAYBE MORE?
    }
    VM_Pop(self);
    VM_Pop(self);
    VM_Pop(self);
    for(int i = 0; i < args; i++)
        VM_Pop(self);
}

static void
VM_Vrt(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, a->type, TYPE_FUNCTION);
    VM_TypeExpect(self, b->type, TYPE_NUMBER);
    VM_ArgMatch(self, Function_Size(a->of.function), b->of.number);
    int spds = b->of.number;
    int pc = a->of.function->address;
    VM_Pop(self);
    VM_Pop(self);
    int sp = Queue_Size(self->stack) - spds;
    Queue_PshB(self->frame, Frame_Init(self->pc, sp));
    self->pc = pc;
}

static void
VM_RangedSort(VM* self, Queue* queue, Value* compare, int left, int right)
{
    if(left >= right)
        return;
    Queue_Swap(queue, left, (left + right) / 2);
    int last = left;
    for(int i = left + 1; i <= right; i++)
    {
        Value* a = Queue_Get(queue, i);
        Value* b = Queue_Get(queue, left);
        Value_Inc(a);
        Value_Inc(b);
        Value_Inc(compare);
        Queue_PshB(self->stack, a);
        Queue_PshB(self->stack, b);
        Queue_PshB(self->stack, compare);
        Of of = { .number = 2 };
        Queue_PshB(self->stack, Value_Init(of, TYPE_NUMBER));
        VM_Vrt(self);
        VM_Run(self, true);
        VM_TypeExpect(self, self->ret->type, TYPE_BOOL);
        if(self->ret->of.boolean)
             Queue_Swap(queue, ++last, i);
        Value_Kill(self->ret);
    }
   Queue_Swap(queue, left, last);
   VM_RangedSort(self, queue, compare, left, last - 1);
   VM_RangedSort(self, queue, compare, last + 1, right);
}

static void
VM_Sort(VM* self, Queue* queue, Value* compare)
{
    VM_TypeExpect(self, compare->type, TYPE_FUNCTION);
    VM_ArgMatch(self, Function_Size(compare->of.function), 2);
    VM_RangedSort(self, queue, compare, 0, Queue_Size(queue) - 1);
}

static void
VM_Srt(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, a->type, TYPE_QUEUE);
    VM_TypeExpect(self, b->type, TYPE_FUNCTION);
    VM_Sort(self, a->of.queue, b);
    VM_Pop(self);
    VM_Pop(self);
    Of of;
    Queue_PshB(self->stack, Value_Init(of, TYPE_NULL));
}

static void
VM_Lst(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, a->type, b->type, "<");
    bool boolean = Value_LessThan(a, b);
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

static void
VM_Lte(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, a->type, b->type, "<=");
    bool boolean = Value_LessThanEqualTo(a, b);
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

static void
VM_Grt(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, a->type, b->type, ">");
    bool boolean = Value_GreaterThan(a, b);
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

static void
VM_Gte(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, a->type, b->type, ">=");
    bool boolean = Value_GreaterThanEqualTo(a, b);
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

static void
VM_And(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, a->type, b->type, "&&");
    VM_TypeExpect(self, a->type, TYPE_BOOL);
    a->of.boolean = a->of.boolean && b->of.boolean;
    VM_Pop(self);
}

static void
VM_Lor(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, a->type, b->type, "||");
    VM_TypeExpect(self, a->type, TYPE_BOOL);
    a->of.boolean = a->of.boolean || b->of.boolean;
    VM_Pop(self);
}

static void
VM_Eql(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    bool boolean = Value_Equal(a, b);
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

static void
VM_Neq(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    bool boolean = !Value_Equal(a, b);
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

static void
VM_Spd(VM* self)
{
    self->spds += 1;
}

static void
VM_Not(VM* self)
{
    Value* value = Queue_Back(self->stack);
    VM_TypeExpect(self, value->type, TYPE_BOOL);
    value->of.boolean = !value->of.boolean;
}

static void
VM_Brf(VM* self, int address)
{
    Value* value = Queue_Back(self->stack);
    VM_TypeExpect(self, value->type, TYPE_BOOL);
    if(value->of.boolean == false)
        self->pc = address;
    VM_Pop(self);
}

static void
VM_Prt(VM* self)
{
    Value* value = Queue_Back(self->stack);
    String* print = Value_Print(value, true, 0);
    printf("%s", print->value);
    VM_Pop(self);
    Of of = { .number = String_Size(print) };
    Queue_PshB(self->stack, Value_Init(of, TYPE_NUMBER));
    String_Kill(print);
}

static void
VM_Len(VM* self)
{
    Value* value = Queue_Back(self->stack);
    int len = Value_Len(value);
    VM_Pop(self);
    Of of = { .number = len };
    Queue_PshB(self->stack, Value_Init(of, TYPE_NUMBER));
}

static void
VM_Psb(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, a->type, TYPE_QUEUE);
    Value_Inc(b);
    Queue_PshB(a->of.queue, b);
    VM_Pop(self);
}

static void
VM_Psf(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, a->type, TYPE_QUEUE);
    Value_Inc(b);
    Queue_PshF(a->of.queue, b);
    VM_Pop(self);
}

static void
VM_Ins(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 3);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* c = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, a->type, TYPE_MAP);
    if(b->type == TYPE_CHAR)
        VM_TypeBadIndex(self, a->type, b->type);
    else
    if(b->type == TYPE_STRING)
        Map_Set(a->of.map, String_Copy(b->of.string), c); // Keys are not reference counted.
    else
        VM_TypeBad(self, b->type);
    Value_Inc(c);
    VM_Pop(self);
    VM_Pop(self);
}

static void
VM_Ref(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    Of of = { .number = a->refs };
    VM_Pop(self);
    Queue_PshB(self->stack, Value_Init(of, TYPE_NUMBER));
}

static Value*
VM_IndexQueue(VM* self, Value* queue, Value* index)
{
    Value* value = Queue_Get(queue->of.queue, index->of.number);
    if(value == NULL)
        VM_OutOfBounds(self, queue->type, index->of.number);
    Value_Inc(value);
    return value;
}

static Value*
VM_IndexString(VM* self, Value* queue, Value* index)
{
    Char* character = Char_Init(queue, index->of.number);
    if(character == NULL)
        VM_OutOfBounds(self, queue->type, index->of.number);
    Of of = { .character = character };
    Value* value = Value_Init(of, TYPE_CHAR);
    Value_Inc(queue);
    return value;
}

static Value*
VM_Index(VM* self, Value* storage, Value* index)
{
    if(storage->type == TYPE_QUEUE)
        return VM_IndexQueue(self, storage, index);
    else
    if(storage->type == TYPE_STRING)
        return VM_IndexString(self, storage, index);
    VM_TypeBadIndex(self, storage->type, index->type);
    return NULL;
}

static Value*
VM_Lookup(VM* self, Value* map, Value* index)
{
    VM_TypeExpect(self, map->type, TYPE_MAP);
    Value* value = Map_Get(map->of.map, index->of.string->value);
    if(value != NULL)
        Value_Inc(value);
    return value;
}

static void
VM_Get(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    Value* value = NULL;
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
    {
        Of of;
        Queue_PshB(self->stack, Value_Init(of, TYPE_NULL));
    }
    else
        Queue_PshB(self->stack, value);
}

static void
VM_Fmt(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, a->type, TYPE_STRING);
    VM_TypeExpect(self, b->type, TYPE_QUEUE);
    String* formatted = String_Init("");
    int index = 0;
    char* ref = a->of.string->value;
    for(char* c = ref; *c; c++)
    {
        if(*c == '{')
        {
            char next = *(c + 1);
            if(next == '}')
            {
                Value* value = Queue_Get(b->of.queue, index);
                if(value == NULL)
                    VM_OutOfBounds(self, b->type, index);
                String_Append(formatted, Value_Print(value, false, 0));
                index += 1;
                c += 1;
                continue;
            }
        }
        String_PshB(formatted, *c);
    }
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .string = formatted };
    Queue_PshB(self->stack, Value_Init(of, TYPE_STRING));
}

static void
VM_Typ(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    Of of = { .string = String_Init(Type_String(a->type)) };
    VM_Pop(self);
    Queue_PshB(self->stack, Value_Init(of, TYPE_STRING));
}

static void
VM_Del(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(b->type == TYPE_NUMBER)
    {
        if(a->type == TYPE_QUEUE)
        {
            bool success = Queue_Del(a->of.queue, b->of.number);
            if(success == false)
                VM_OutOfBounds(self, a->type, b->of.number);
        }
        else
        if(a->type == TYPE_STRING)
        {
            bool success = String_Del(a->of.string, b->of.number);
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
        Map_Del(a->of.map, b->of.string->value);
    }
    else
        VM_TypeBad(self, a->type);
    VM_Pop(self);
    VM_Pop(self);
    Of of;
    Queue_PshB(self->stack, Value_Init(of, TYPE_NULL));
}

static void
VM_Mem(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    bool boolean = a == b;
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

static void
VM_Opn(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, a->type, TYPE_STRING);
    VM_TypeExpect(self, b->type, TYPE_STRING);
    File* file = File_Init(String_Copy(a->of.string), String_Copy(b->of.string));
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .file = file };
    Queue_PshB(self->stack, Value_Init(of, TYPE_FILE));
}

static void
VM_Red(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, a->type, TYPE_FILE);
    VM_TypeExpect(self, b->type, TYPE_NUMBER);
    String* buffer = String_Init("");
    if(a->of.file->file)
    {
        String_Resize(buffer, b->of.number);
        int size = fread(buffer->value, sizeof(char), b->of.number, a->of.file->file);
        int diff = b->of.number - size;
        while(diff)
        {
            String_PopB(buffer);
            diff -= 1;
        }
    }
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .string = buffer };
    Queue_PshB(self->stack, Value_Init(of, TYPE_STRING));
}

static void
VM_Wrt(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, a->type, TYPE_FILE);
    VM_TypeExpect(self, b->type, TYPE_STRING);
    size_t bytes = 0;
    if(a->of.file->file)
        bytes = fwrite(b->of.string->value, sizeof(char), String_Size(b->of.string), a->of.file->file);
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .number = bytes };
    Queue_PshB(self->stack, Value_Init(of, TYPE_NUMBER));
}

static void
VM_God(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, a->type, TYPE_FILE);
    bool boolean = a->of.file->file != NULL;
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

static void
VM_Key(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, a->type, TYPE_MAP);
    Of parent;
    Value* queue = Value_Init(parent, TYPE_QUEUE);
    for(int i = 0; i < Map_Buckets(a->of.map); i++)
    {
        Node* chain = a->of.map->bucket[i];
        while(chain)
        {
            Of child = { .string = String_Copy(chain->key) };
            Value* string = Value_Init(child, TYPE_STRING);
            Queue_PshB(queue->of.queue, string);
            chain = chain->next;
        }
    }
    VM_Pop(self);
    Queue_Sort(queue->of.queue, (Compare) Value_LessThan);
    Queue_PshB(self->stack, queue);
}

static int
VM_Run(VM* self, bool arbitrary)
{
    while(true)
    {
        int instruction = self->instructions[self->pc];
        self->pc += 1;
        Opcode oc = instruction & 0xFF;
        int address = instruction >> 8;
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

int
main(int argc, char* argv[])
{
    Args args = Args_Parse(argc, argv);
    if(args.entry)
    {
        int ret = 0;
        String* entry = String_Init(args.entry);
        CC* cc = CC_Init();
        CC_ReserveFunctions(cc);
        CC_IncludeModule(cc, entry);
        CC_Parse(cc);
        VM* vm = VM_Assemble(cc->assembly);
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
        String_Kill(entry);
        return ret;
    }
    else
    if(args.help)
        Args_Help();
    else
        Quit("usage: rr -h");
}
