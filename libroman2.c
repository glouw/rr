#include "roman2.h"

#define QUEUE_BLOCK_SIZE (8)
#define MAP_UNPRIMED (-1)
#define STR_CAP_SIZE (16)

struct RR_String
{
    char* value;
    int size;
    int cap;
};

struct RR_Char
{
    RR_Value* string;
    char* value;
};

struct RR_Function
{
    RR_String* name;
    int size;
    int address;
};

struct RR_Block
{
    void* value[QUEUE_BLOCK_SIZE];
    int a;
    int b;
};

struct RR_Queue
{
    RR_Block** block;
    RR_Kill kill;
    RR_Copy copy;
    int size;
    int blocks;
};

struct RR_Node
{
    struct RR_Node* next;
    RR_String* key;
    void* value;
};

struct RR_File
{
    RR_String* path;
    RR_String* mode;
    FILE* file;
};

struct RR_Map
{
    RR_Node** bucket;
    RR_Kill kill;
    RR_Copy copy;
    int size;
    int prime_index;
    float load_factor;
    unsigned rand1;
    unsigned rand2;
};

union RR_Of
{
    RR_File* file;
    RR_Function* function;
    RR_Queue* queue;
    RR_Map* map;
    RR_String* string;
    RR_Char* character;
    double number;
    bool boolean;
};

struct RR_Value
{
    RR_Type type;
    RR_Of of;
    int refs;
};

typedef enum
{
    FRONT,
    BACK,
}
End;

void*
RR_Malloc(int size)
{
    return malloc(size);
}

void*
RR_Realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void
RR_Free(void* pointer)
{
    free(pointer);
}

double
RR_Microseconds(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1e6 + tv.tv_usec;
}

void
RR_Delete(RR_Kill kill, void* value)
{
    if(kill)
        kill(value);
}

bool
RR_Equals(char* a, char* b)
{
    return a[0] != b[0] ? false : (strcmp(a, b) == 0);
}

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

int
RR_Value_Len(RR_Value* self)
{
    switch(self->type)
    {
    case TYPE_FILE:
        return RR_File_Size(self->of.file);
    case TYPE_FUNCTION:
        return RR_Function_Size(self->of.function);
    case TYPE_QUEUE:
        return RR_Queue_Size(self->of.queue);
    case TYPE_MAP:
        return RR_Map_Size(self->of.map);
    case TYPE_STRING:
        return RR_String_Size(self->of.string);
    case TYPE_CHAR:
    case TYPE_NUMBER:
    case TYPE_BOOL:
    case TYPE_NULL:
        break;
    }
    return 0;
}

int
RR_Value_Refs(RR_Value* self)
{
    return self->refs;
}

RR_Of*
RR_Value_Of(RR_Value* self)
{
    return &self->of;
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
        RR_Type_Kill(self->type, &self->of);
        RR_Free(self);
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
        return RR_Queue_Size(a->of.queue)         \
           CMP RR_Queue_Size(b->of.queue);        \
    case TYPE_CHAR:                               \
        return *a->of.character->value            \
           CMP *b->of.character->value;           \
    case TYPE_MAP:                                \
        return RR_Map_Size(a->of.map)             \
           CMP RR_Map_Size(b->of.map);            \
    case TYPE_BOOL:                               \
        return a->of.boolean                      \
           CMP b->of.boolean;                     \
    case TYPE_FUNCTION:                           \
        return RR_Function_Size(a->of.function)   \
           CMP RR_Function_Size(b->of.function);  \
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
    RR_Value* copy = RR_Malloc(sizeof(*copy));
    copy->type = self->type;
    copy->refs = 0;
    RR_Type_Copy(copy, self);
    return copy;
}

RR_Value*
RR_Value_Init(RR_Of of, RR_Type type)
{
    RR_Value* self = RR_Malloc(sizeof(*self));
    self->type = type;
    self->refs = 0;
    self->of = of;
    return self;
}

RR_Value*
RR_Value_NewQueue(void)
{
    RR_Of of;
    of.queue = RR_Queue_Init((RR_Kill) RR_Value_Kill, (RR_Copy) RR_Value_Copy);
    return RR_Value_Init(of, TYPE_QUEUE);
}

RR_Value*
RR_Value_NewMap(void)
{
    RR_Of of;
    of.map = RR_Map_Init((RR_Kill) RR_Value_Kill, (RR_Copy) RR_Value_Copy);
    return RR_Value_Init(of, TYPE_MAP);
}

RR_Value*
RR_Value_NewFunction(RR_Function* function)
{
    RR_Of of;
    of.function = function;
    return RR_Value_Init(of, TYPE_FUNCTION);
}

RR_Value*
RR_Value_NewChar(RR_Char* character)
{
    RR_Of of;
    of.character = character;
    return RR_Value_Init(of, TYPE_CHAR);
}

RR_Value*
RR_Value_NewString(RR_String* string)
{
    RR_Of of;
    of.string = string;
    return RR_Value_Init(of, TYPE_STRING);
}

RR_Value*
RR_Value_NewNumber(double number)
{
    RR_Of of;
    of.number = number;
    return RR_Value_Init(of, TYPE_NUMBER);
}

RR_Value*
RR_Value_NewBool(bool boolean)
{
    RR_Of of;
    of.boolean = boolean;
    return RR_Value_Init(of, TYPE_BOOL);
}

RR_Value*
RR_Value_NewFile(RR_File* file)
{
    RR_Of of;
    of.file = file;
    return RR_Value_Init(of, TYPE_FILE);
}

RR_Value*
RR_Value_NewNull(void)
{
    RR_Of of;
    return RR_Value_Init(of, TYPE_NULL);
}

RR_String*
RR_Value_Sprint(RR_Value* self, bool newline, int indents)
{
    RR_String* print = RR_String_Init("");
    switch(self->type)
    {
    case TYPE_FILE:
        RR_String_Append(print, RR_String_Format("{ \"%s\", \"%s\", %p }", self->of.file->path->value, self->of.file->mode->value, self->of.file->file));
        break;
    case TYPE_FUNCTION:
        RR_String_Append(print, RR_String_Format("{ %s, %d, %d }", self->of.function->name->value, self->of.function->size, self->of.function->address));
        break;
    case TYPE_QUEUE:
        RR_String_Append(print, RR_Queue_Print(self->of.queue, indents));
        break;
    case TYPE_MAP:
        RR_String_Append(print, RR_Map_Print(self->of.map, indents));
        break;
    case TYPE_STRING:
        RR_String_Append(print, RR_String_Format(indents == 0 ? "%s" : "\"%s\"", self->of.string->value));
        break;
    case TYPE_NUMBER:
        RR_String_Append(print, RR_String_Format("%f", self->of.number));
        break;
    case TYPE_BOOL:
        RR_String_Append(print, RR_String_Format("%s", self->of.boolean ? "true" : "false"));
        break;
    case TYPE_CHAR:
        RR_String_Append(print, RR_String_Format(indents == 0 ? "%c" : "\"%c\"", *self->of.character->value));
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

bool
RR_Value_Subbing(RR_Value* a, RR_Value* b)
{
    return a->type == TYPE_CHAR && b->type == TYPE_STRING;
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

bool
RR_Value_Pushing(RR_Value* a, RR_Value* b)
{
    return a->type == TYPE_QUEUE && b->type != TYPE_QUEUE;
}

bool
RR_Value_CharAppending(RR_Value* a, RR_Value* b)
{
    return a->type == TYPE_STRING && b->type == TYPE_CHAR;
}

void
RR_String_Alloc(RR_String* self, int cap)
{
    self->cap = cap;
    self->value = RR_Realloc(self->value, (1 + cap) * sizeof(*self->value));
}

char*
RR_String_Value(RR_String* self)
{
    return self->value;
}

RR_String*
RR_String_Init(char* string)
{
    RR_String* self = RR_Malloc(sizeof(*self));
    self->value = NULL;
    self->size = strlen(string);
    RR_String_Alloc(self, self->size < STR_CAP_SIZE ? STR_CAP_SIZE : self->size);
    strcpy(self->value, string);
    return self;
}

void
RR_String_Kill(RR_String* self)
{
    RR_Delete(RR_Free, self->value);
    RR_Free(self);
}

RR_String*
RR_String_FromChar(char c)
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

int
RR_String_Size(RR_String* self)
{
    return self->size;
}

void
RR_String_Replace(RR_String* self, char x, char y)
{
    for(int i = 0; i < RR_String_Size(self); i++)
        if(self->value[i] == x)
            self->value[i] = y;
}

char*
RR_String_End(RR_String* self)
{
    return &self->value[RR_String_Size(self) - 1];
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

int
RR_String_Empty(RR_String* self)
{
    return RR_String_Size(self) == 0;
}

void
RR_String_PshB(RR_String* self, char ch)
{
    if(RR_String_Size(self) == self->cap)
        RR_String_Alloc(self, (self->cap == 0) ? 1 : (2 * self->cap));
    self->value[self->size + 0] = ch;
    self->value[self->size + 1] = '\0';
    self->size += 1;
}

RR_String*
RR_String_Indent(int indents)
{
    int width = 4;
    RR_String* ident = RR_String_Init("");
    while(indents > 0)
    {
        for(int i = 0; i < width; i++)
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
RR_String_Resize(RR_String* self, int size)
{
    if(size > self->cap)
        RR_String_Alloc(self, size);
    self->size = size;
    self->value[size] = '\0';
}

bool
RR_String_Equals(RR_String* a, char* b)
{
    return RR_Equals(a->value, b);
}

bool
RR_String_Equal(RR_String* a, RR_String* b)
{
    return RR_Equals(a->value, b->value);
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

void
RR_String_Append(RR_String* self, RR_String* other)
{
    RR_String_Appends(self, other->value);
    RR_String_Kill(other);
}

RR_String*
RR_String_Base(RR_String* path)
{
    RR_String* base = RR_String_Copy(path);
    while(!RR_String_Empty(base))
        if(RR_String_Back(base) == '/')
            break;
        else
            RR_String_PopB(base);
    return base;
}

RR_String*
RR_String_Moves(char** from)
{
    RR_String* self = RR_Malloc(sizeof(*self));
    self->value = *from;
    self->size = strlen(self->value);
    self->cap = self->size;
    *from = NULL;
    return self;
}

RR_String*
RR_String_Skip(RR_String* self, char c)
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

bool
RR_String_IsBoolean(RR_String* ident)
{
    return RR_String_Equals(ident, "true") || RR_String_Equals(ident, "false");
}

bool
RR_String_IsNull(RR_String* ident)
{
    return RR_String_Equals(ident, "null");
}

RR_String*
RR_String_Format(char* format, ...)
{
    va_list args;
    va_start(args, format);
    RR_String* line = RR_String_Init("");
    int size = vsnprintf(line->value, line->size, format, args);
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
RR_String_Get(RR_String* self, int index)
{
    if(index < 0 || index >= RR_String_Size(self))
        return NULL;
    return &self->value[index];
}

bool
RR_String_Del(RR_String* self, int index)
{
    if(RR_String_Get(self, index))
    {
        for(int i = index; i < RR_String_Size(self) - 1; i++)
            self->value[i] = self->value[i + 1];
        RR_String_PopB(self);
        return true;
    }
    return false;
}

int
RR_String_EscToByte(int ch)
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

bool
RR_String_IsUpper(int c)
{
    return c >= 'A' && c <= 'Z';
}

bool
RR_String_IsLower(int c)
{
    return c >= 'a' && c <= 'z';
}

bool
RR_String_IsAlpha(int c)
{
    return RR_String_IsLower(c) || RR_String_IsUpper(c);
}

bool
RR_String_IsDigit(int c)
{
    return c >= '0' && c <= '9';
}

bool
RR_String_IsNumber(int c)
{
    return RR_String_IsDigit(c) || c == '.';
}

bool
RR_String_IsIdentLeader(int c)
{
    return RR_String_IsAlpha(c) || c == '_';
}

bool
RR_String_IsIdent(int c)
{
    return RR_String_IsIdentLeader(c) || RR_String_IsDigit(c);
}

bool
RR_String_IsModule(int c)
{
    return RR_String_IsIdent(c) || c == '.';
}

bool
RR_String_IsOp(int c)
{
    return c == '*' || c == '/' || c == '%' || c == '+' || c == '-' || c == '='
        || c == '<' || c == '>' || c == '!' || c == '&' || c == '|' || c == '?';
}

bool
RR_String_IsSpace(int c)
{
    return c == '\n' || c == '\t' || c == '\r' || c == ' ';
}

RR_File*
RR_File_Init(RR_String* path, RR_String* mode)
{
    RR_File* self = RR_Malloc(sizeof(*self));
    self->path = path;
    self->mode = mode;
    self->file = fopen(self->path->value, self->mode->value);
    return self;
}

bool
RR_File_Equal(RR_File* a, RR_File* b)
{
    return RR_String_Equal(a->path, b->path) 
        && RR_String_Equal(a->mode, b->mode)
        && a->file == b->file;
}

void
RR_File_Kill(RR_File* self)
{
    RR_String_Kill(self->path);
    RR_String_Kill(self->mode);
    if(self->file)
        fclose(self->file);
    RR_Free(self);
}

RR_File*
RR_File_Copy(RR_File* self)
{
    return RR_File_Init(RR_String_Copy(self->path), RR_String_Copy(self->mode));
}

int
RR_File_Size(RR_File* self)
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

FILE*
RR_File_File(RR_File* self)
{
    return self->file;
}

RR_Function*
RR_Function_Init(RR_String* name, int size, int address)
{
    RR_Function* self = malloc(sizeof(*self));
    self->name = name;
    self->size = size;
    self->address = address;
    return self;
}

void
RR_Function_Kill(RR_Function* self)
{
    RR_String_Kill(self->name);
    RR_Free(self);
}

RR_Function*
RR_Function_Copy(RR_Function* self)
{
    return RR_Function_Init(RR_String_Copy(self->name), RR_Function_Size(self), RR_Function_Address(self));
}

int
RR_Function_Size(RR_Function* self)
{
    return self->size;
}

int
RR_Function_Address(RR_Function* self)
{
    return self->address;
}

bool
RR_Function_Equal(RR_Function* a, RR_Function* b)
{
    return RR_String_Equal(a->name, b->name)
        && a->address == b->address
        && RR_Function_Size(a) == RR_Function_Size(b);
}

RR_Block*
RR_Block_Init(End end)
{
    RR_Block* self = RR_Malloc(sizeof(*self));
    self->a = (end == BACK) ? 0 : QUEUE_BLOCK_SIZE;
    self->b = (end == BACK) ? 0 : QUEUE_BLOCK_SIZE;
    return self;
}

int
RR_Queue_Size(RR_Queue* self)
{
    return self->size;
}

bool
RR_Queue_Empty(RR_Queue* self)
{
    return RR_Queue_Size(self) == 0;
}

RR_Block**
RR_QueueBlockF(RR_Queue* self)
{
    return &self->block[0];
}

RR_Block**
RR_QueueBlockB(RR_Queue* self)
{
    return &self->block[self->blocks - 1];
}

void*
RR_Queue_Front(RR_Queue* self)
{
    if(RR_Queue_Empty(self))
        return NULL;
    RR_Block* block = *RR_QueueBlockF(self);
    return block->value[block->a];
}

void*
RR_Queue_Back(RR_Queue* self)
{
    if(RR_Queue_Empty(self))
        return NULL;
    RR_Block* block = *RR_QueueBlockB(self);
    return block->value[block->b - 1];
}

RR_Queue*
RR_Queue_Init(RR_Kill kill, RR_Copy copy)
{
    RR_Queue* self = RR_Malloc(sizeof(*self));
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
    for(int step = 0; step < self->blocks; step++)
    {
        RR_Block* block = self->block[step];
        while(block->a < block->b)
        {
            RR_Delete(self->kill, block->value[block->a]);
            block->a += 1;
        }
        RR_Free(block);
    }
    RR_Free(self->block);
    RR_Free(self);
}

void**
RR_Queue_At(RR_Queue* self, int index)
{
    if(index < self->size)
    {
        RR_Block* block = *RR_QueueBlockF(self);
        int at = index + block->a;
        int step = at / QUEUE_BLOCK_SIZE;
        int item = at % QUEUE_BLOCK_SIZE;
        return &self->block[step]->value[item];
    }
    return NULL;
}

void*
RR_Queue_Get(RR_Queue* self, int index)
{
    if(index == 0)
        return RR_Queue_Front(self);
    else
    if(index == RR_Queue_Size(self) - 1)
        return RR_Queue_Back(self);
    else
    {
        void** at = RR_Queue_At(self, index);
        return at ? *at : NULL;
    }
}

void
RR_Queue_Alloc(RR_Queue* self, int blocks)
{
    self->blocks = blocks;
    self->block = RR_Realloc(self->block, blocks * sizeof(*self->block));
}

void
RR_Queue_Push(RR_Queue* self, void* value, End end)
{
    if(end == BACK)
    {
        RR_Block* block = *RR_QueueBlockB(self);
        block->value[block->b] = value;
        block->b += 1;
    }
    else
    {
        RR_Block* block = *RR_QueueBlockF(self);
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
        RR_Queue_Alloc(self, 1);
        *RR_QueueBlockB(self) = RR_Block_Init(BACK);
    }
    else
    {
        RR_Block* block = *RR_QueueBlockB(self);
        if(block->b == QUEUE_BLOCK_SIZE || block->a == QUEUE_BLOCK_SIZE)
        {
            RR_Queue_Alloc(self, self->blocks + 1);
            *RR_QueueBlockB(self) = RR_Block_Init(BACK);
        }
    }
    RR_Queue_Push(self, value, BACK);
}

void
RR_Queue_PshF(RR_Queue* self, void* value)
{
    if(self->blocks == 0)
    {
        RR_Queue_Alloc(self, 1);
        *RR_QueueBlockF(self) = RR_Block_Init(FRONT);
    }
    else
    {
        RR_Block* block = *RR_QueueBlockF(self);
        if(block->b == 0 || block->a == 0)
        {
            RR_Queue_Alloc(self, self->blocks + 1);
            for(int i = self->blocks - 1; i > 0; i--)
                self->block[i] = self->block[i - 1];
            *RR_QueueBlockF(self) = RR_Block_Init(FRONT);
        }
    }
    RR_Queue_Push(self, value, FRONT);
}

void
RR_Queue_Pop(RR_Queue* self, End end)
{
    if(!RR_Queue_Empty(self))
    {
        if(end == BACK)
        {
            RR_Block* block = *RR_QueueBlockB(self);
            block->b -= 1;
            RR_Delete(self->kill, block->value[block->b]);
            if(block->b == 0)
            {
                RR_Free(block);
                RR_Queue_Alloc(self, self->blocks - 1);
            }
        }
        else
        {
            RR_Block* block = *RR_QueueBlockF(self);
            RR_Delete(self->kill, block->value[block->a]);
            block->a += 1;
            if(block->a == QUEUE_BLOCK_SIZE)
            {
                for(int i = 0; i < self->blocks - 1; i++)
                    self->block[i] = self->block[i + 1];
                RR_Free(block);
                RR_Queue_Alloc(self, self->blocks - 1);
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

void
RR_Queue_PopFSoft(RR_Queue* self)
{
    RR_Kill kill = self->kill;
    self->kill = NULL;
    RR_Queue_PopF(self);
    self->kill = kill;
}

void
RR_Queue_PopBSoft(RR_Queue* self)
{
    RR_Kill kill = self->kill;
    self->kill = NULL;
    RR_Queue_PopB(self);
    self->kill = kill;
}

bool
RR_Queue_Del(RR_Queue* self, int index)
{
    if(index == 0)
        RR_Queue_PopF(self);
    else
    if(index == RR_Queue_Size(self) - 1)
        RR_Queue_PopB(self);
    else
    {
        void** at = RR_Queue_At(self, index);
        if(at)
        {
            RR_Delete(self->kill, *at);
            if(index < RR_Queue_Size(self) / 2)
            {
                while(index > 0)
                {
                    *RR_Queue_At(self, index) = *RR_Queue_At(self, index - 1);
                    index -= 1;
                }
                RR_Queue_PopFSoft(self);
            }
            else
            {
                while(index < RR_Queue_Size(self) - 1)
                {
                    *RR_Queue_At(self, index) = *RR_Queue_At(self, index + 1);
                    index += 1;
                }
                RR_Queue_PopBSoft(self);
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
    for(int i = 0; i < RR_Queue_Size(self); i++)
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
    for(int i = RR_Queue_Size(other) - 1; i >= 0; i--)
    {
        void* temp = RR_Queue_Get(other, i);
        void* value = self->copy ? self->copy(temp) : temp;
        RR_Queue_PshF(self, value);
    }
}

void
RR_Queue_Append(RR_Queue* self, RR_Queue* other)
{
    for(int i = 0; i < RR_Queue_Size(other); i++)
    {
        void* temp = RR_Queue_Get(other, i);
        void* value = self->copy ? self->copy(temp) : temp;
        RR_Queue_PshB(self, value);
    }
}

bool
RR_Queue_Equal(RR_Queue* self, RR_Queue* other, RR_Compare compare)
{
    if(RR_Queue_Size(self) != RR_Queue_Size(other))
        return false;
    else
        for(int i = 0; i < RR_Queue_Size(self); i++)
            if(!compare(RR_Queue_Get(self, i), RR_Queue_Get(other, i)))
                return false;
    return true;
}

void
RR_Queue_Swap(RR_Queue* self, int a, int b)
{
    void** x = RR_Queue_At(self, a);
    void** y = RR_Queue_At(self, b);
    void* temp = *x;
    *x = *y;
    *y = temp;
}

void
RR_Queue_RangedSort(RR_Queue* self, bool (*compare)(void*, void*), int left, int right)
{
    if(left >= right)
        return;
    RR_Queue_Swap(self, left, (left + right) / 2);
    int last = left;
    for(int i = left + 1; i <= right; i++)
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
RR_Queue_Sort(RR_Queue* self, bool (*compare)(void*, void*))
{
    RR_Queue_RangedSort(self, compare, 0, RR_Queue_Size(self) - 1);
}

RR_String*
RR_Queue_Print(RR_Queue* self, int indents)
{
    if(RR_Queue_Empty(self))
        return RR_String_Init("[]");
    else
    {
        RR_String* print = RR_String_Init("[\n");
        int size = RR_Queue_Size(self);
        for(int i = 0; i < size; i++)
        {
            RR_String_Append(print, RR_String_Indent(indents + 1));
            RR_String_Append(print, RR_String_Format("[%d] = ", i));
            RR_String_Append(print, RR_Value_Sprint(RR_Queue_Get(self, i), false, indents + 1));
            if(i < size - 1)
                RR_String_Appends(print, ",");
            RR_String_Appends(print, "\n");
        }
        RR_String_Append(print, RR_String_Indent(indents));
        RR_String_Appends(print, "]");
        return print;
    }
}

RR_Node*
RR_Node_Init(RR_String* key, void* value)
{
    RR_Node* self = RR_Malloc(sizeof(*self));
    self->next = NULL;
    self->key = key;
    self->value = value;
    return self;
}

void
RR_Node_Set(RR_Node* self, RR_Kill kill, RR_String* key, void* value)
{
    RR_Delete((RR_Kill) RR_String_Kill, self->key);
    RR_Delete(kill, self->value);
    self->key = key;
    self->value = value;
}

void
RR_Node_Kill(RR_Node* self, RR_Kill kill)
{
    RR_Node_Set(self, kill, NULL, NULL);
    RR_Free(self);
}

void
RR_Node_Push(RR_Node** self, RR_Node* node)
{
    node->next = *self;
    *self = node;
}

RR_Node*
RR_Node_Copy(RR_Node* self, RR_Copy copy)
{
    return RR_Node_Init(RR_String_Copy(self->key), copy ? copy(self->value) : self->value);
}

static const int
RR_Map_Primes[] = {
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

int
RR_Map_Buckets(RR_Map* self)
{
    if(self->prime_index == MAP_UNPRIMED)
        return 0; 
    return RR_Map_Primes[self->prime_index];
}

bool
RR_Map_Resizable(RR_Map* self)
{
    return self->prime_index < (int) LEN(RR_Map_Primes) - 1;
}

RR_Map*
RR_Map_Init(RR_Kill kill, RR_Copy copy)
{
    RR_Map* self = RR_Malloc(sizeof(*self));
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

int
RR_Map_Size(RR_Map* self)
{
    return self->size;
}

bool
RR_Map_Empty(RR_Map* self)
{
    return RR_Map_Size(self) == 0;
}

void
RR_Map_Kill(RR_Map* self)
{
    for(int i = 0; i < RR_Map_Buckets(self); i++)
    {
        RR_Node* bucket = self->bucket[i];
        while(bucket)
        {
            RR_Node* next = bucket->next;
            RR_Node_Kill(bucket, self->kill);
            bucket = next;
        }
    }
    RR_Free(self->bucket);
    RR_Free(self);
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

RR_Node**
RR_Map_Bucket(RR_Map* self, char* key)
{
    unsigned index = RR_Map_Hash(self, key);
    return &self->bucket[index];
}

void
RR_Map_Alloc(RR_Map* self, int index)
{
    self->prime_index = index;
    self->bucket = calloc(RR_Map_Buckets(self), sizeof(*self->bucket));
}

void
RR_Map_Rehash(RR_Map* self)
{
    RR_Map* other = RR_Map_Init(self->kill, self->copy);
    RR_Map_Alloc(other, self->prime_index + 1);
    for(int i = 0; i < RR_Map_Buckets(self); i++)
    {
        RR_Node* bucket = self->bucket[i];
        while(bucket)
        {
            RR_Node* next = bucket->next;
            RR_Map_Emplace(other, bucket->key, bucket);
            bucket = next;
        }
    }
    RR_Free(self->bucket);
    *self = *other;
    RR_Free(other);
}

bool
RR_Map_Full(RR_Map* self)
{
    return RR_Map_Size(self) / (float) RR_Map_Buckets(self) > self->load_factor;
}

void
RR_Map_Emplace(RR_Map* self, RR_String* key, RR_Node* node)
{
    if(self->prime_index == MAP_UNPRIMED)
        RR_Map_Alloc(self, 0);
    else
    if(RR_Map_Resizable(self) && RR_Map_Full(self))
        RR_Map_Rehash(self);
    RR_Node_Push(RR_Map_Bucket(self, key->value), node);
    self->size += 1;
}

RR_Node*
RR_Map_At(RR_Map* self, char* key)
{
    if(!RR_Map_Empty(self)) 
    {
        RR_Node* chain = *RR_Map_Bucket(self, key);
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
    if(!RR_Map_Empty(self)) 
    {
        RR_Node** head = RR_Map_Bucket(self, key);
        RR_Node* chain = *head;
        RR_Node* prev = NULL;
        while(chain)
        {
            if(RR_String_Equals(chain->key, key))
            {
                if(prev)
                    prev->next = chain->next;
                else
                    *head = chain->next;
                RR_Node_Kill(chain, self->kill);
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
    RR_Node* found = RR_Map_At(self, key->value);
    if(found)
        RR_Node_Set(found, self->kill, key, value);
    else
    {
        RR_Node* node = RR_Node_Init(key, value);
        RR_Map_Emplace(self, key, node);
    }
}

void*
RR_Map_Get(RR_Map* self, char* key)
{
    RR_Node* found = RR_Map_At(self, key);
    return found ? found->value : NULL;
}

RR_Map*
RR_Map_Copy(RR_Map* self)
{
    RR_Map* copy = RR_Map_Init(self->kill, self->copy);
    for(int i = 0; i < RR_Map_Buckets(self); i++)
    {
        RR_Node* chain = self->bucket[i];
        while(chain)
        {
            RR_Node* node = RR_Node_Copy(chain, copy->copy);
            RR_Map_Emplace(copy, node->key, node);
            chain = chain->next;
        }
    }
    return copy;
}

void
RR_Map_Append(RR_Map* self, RR_Map* other)
{
    for(int i = 0; i < RR_Map_Buckets(other); i++)
    {
        RR_Node* chain = other->bucket[i];
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
    if(RR_Map_Size(self) == RR_Map_Size(other))
    {
        for(int i = 0; i < RR_Map_Buckets(self); i++)
        {
            RR_Node* chain = self->bucket[i];
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
RR_Map_Print(RR_Map* self, int indents)
{
    if(RR_Map_Empty(self))
        return RR_String_Init("{}");
    else
    {
        RR_String* print = RR_String_Init("{\n");
        for(int i = 0; i < RR_Map_Buckets(self); i++)
        {
            RR_Node* bucket = self->bucket[i];
            while(bucket)
            {
                RR_String_Append(print, RR_String_Indent(indents + 1));
                RR_String_Append(print, RR_String_Format("\"%s\" : ", bucket->key->value));
                RR_String_Append(print, RR_Value_Sprint(bucket->value, false, indents + 1));
                if(i < RR_Map_Size(self) - 1)
                    RR_String_Appends(print, ",");
                RR_String_Appends(print, "\n");
                bucket = bucket->next;
            }
        }
        RR_String_Append(print, RR_String_Indent(indents));
        RR_String_Appends(print, "}");
        return print;
    }
}

RR_Value*
RR_Map_Key(RR_Map* self)
{
    RR_Value* queue = RR_Value_NewQueue();
    for(int i = 0; i < RR_Map_Buckets(self); i++)
    {
        RR_Node* chain = self->bucket[i];
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
RR_Char_Init(RR_Value* string, int index)
{
    char* value = RR_String_Get(string->of.string, index);
    if(value)
    {
        RR_Char* self = RR_Malloc(sizeof(*self));
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
    RR_Free(self);
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

void
RR_Type_Kill(RR_Type type, RR_Of* of)
{
    switch(type)
    {
    case TYPE_FILE:
        RR_File_Kill(of-> file);
        break;
    case TYPE_FUNCTION:
        RR_Function_Kill(of-> function);
        break;
    case TYPE_QUEUE:
        RR_Queue_Kill(of-> queue);
        break;
    case TYPE_MAP:
        RR_Map_Kill(of-> map);
        break;
    case TYPE_STRING:
        RR_String_Kill(of-> string);
        break;
    case TYPE_CHAR:
        RR_Char_Kill(of-> character);
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
        copy->type = TYPE_STRING;
        copy->of.string = RR_String_FromChar(*self->of.character->value);
        break;
    case TYPE_NUMBER:
    case TYPE_BOOL:
    case TYPE_NULL:
        copy->of = self->of;
        break;
    }
}
