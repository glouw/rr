#include "roman2.h"

#include <sys/time.h>

#define QUEUE_BLOCK_SIZE (8)
#define MAP_UNPRIMED (-1)
#define STR_CAP_SIZE (16)

struct String
{
    char* value;
    int size;
    int cap;
};

struct Char
{
    Value* string;
    char* value;
};

struct Function
{
    String* name;
    int size;
    int address;
};

struct Block
{
    void* value[QUEUE_BLOCK_SIZE];
    int a;
    int b;
};

struct Queue
{
    Block** block;
    Kill kill;
    Copy copy;
    int size;
    int blocks;
};

struct Node
{
    struct Node* next;
    String* key;
    void* value;
};

struct File
{
    String* path;
    String* mode;
    FILE* file;
};

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

union Of
{
    File* file;
    Function* function;
    Queue* queue;
    Map* map;
    String* string;
    Char* character;
    double number;
    bool boolean;
};

struct Value
{
    Type type;
    Of of;
    int refs;
};

typedef enum
{
    FRONT,
    BACK,
}
End;

void*
Malloc(int size)
{
    return malloc(size);
}

void*
Realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void
Free(void* pointer)
{
    free(pointer);
}

void
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

double
Microseconds(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1e6 + tv.tv_usec;
}

void
Delete(Kill kill, void* value)
{
    if(kill)
        kill(value);
}

bool
Equals(char* a, char* b)
{
    return a[0] != b[0] ? false : (strcmp(a, b) == 0);
}

Type
Value_Type(Value* self)
{
    return self->type;
}

File*
Value_File(Value* self)
{
    return self->type == TYPE_FILE
         ? self->of.file
         : NULL;
}

Queue*
Value_Queue(Value* self)
{
    return self->type == TYPE_QUEUE
         ? self->of.queue
         : NULL;
}

Map*
Value_Map(Value* self)
{
    return self->type == TYPE_MAP
         ? self->of.map
         : NULL;
}

String*
Value_String(Value* self)
{
    return self->type == TYPE_STRING
         ? self->of.string
         : NULL;
}

Char*
Value_Char(Value* self)
{
    return self->type == TYPE_CHAR
         ? self->of.character
         : NULL;
}

double*
Value_Number(Value* self)
{
    return self->type == TYPE_NUMBER
         ? &self->of.number
         : NULL;
}

bool*
Value_Bool(Value* self)
{
    return self->type == TYPE_BOOL
         ? &self->of.boolean
         : NULL;
}

Function*
Value_Function(Value* self)
{
    return self->type == TYPE_FUNCTION
         ? self->of.function
         : NULL;
}

int
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

int
Value_Refs(Value* self)
{
    return self->refs;
}

Of*
Value_Of(Value* self)
{
    return &self->of;
}

void
Value_Dec(Value* self)
{
    self->refs -= 1;
}

void
Value_Inc(Value* self)
{
    self->refs += 1;
}

void
Value_Kill(Value* self)
{
    if(self->refs == 0)
    {
        Type_Kill(self->type, &self->of);
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

bool
Value_LessThan(Value* a, Value* b)
{
    if(a->type == b->type)
        switch(a->type) { COMPARE_TABLE(<); }
    return false;
}

bool
Value_GreaterThanEqualTo(Value* a, Value* b)
{
    if(a->type == b->type)
        switch(a->type) { COMPARE_TABLE(>=); }
    return false;
}

bool
Value_GreaterThan(Value* a, Value* b)
{
    if(a->type == b->type)
        switch(a->type) { COMPARE_TABLE(>); }
    return false;
}

bool
Value_LessThanEqualTo(Value* a, Value* b)
{
    if(a->type == b->type)
        switch(a->type) { COMPARE_TABLE(<=); }
    return false;
}

bool
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

Value*
Value_Copy(Value* self)
{
    Value* copy = Malloc(sizeof(*copy));
    copy->type = self->type;
    copy->refs = 0;
    Type_Copy(copy, self);
    return copy;
}

Value*
Value_Init(Of of, Type type)
{
    Value* self = Malloc(sizeof(*self));
    self->type = type;
    self->refs = 0;
    self->of = of;
    return self;
}

Value*
Value_NewQueue(void)
{
    Of of;
    of.queue = Queue_Init((Kill) Value_Kill, (Copy) Value_Copy);
    return Value_Init(of, TYPE_QUEUE);
}

Value*
Value_NewMap(void)
{
    Of of;
    of.map = Map_Init((Kill) Value_Kill, (Copy) Value_Copy);
    return Value_Init(of, TYPE_MAP);
}

Value*
Value_NewFunction(Function* function)
{
    Of of;
    of.function = function;
    return Value_Init(of, TYPE_FUNCTION);
}

Value*
Value_NewChar(Char* character)
{
    Of of;
    of.character = character;
    return Value_Init(of, TYPE_CHAR);
}

Value*
Value_NewString(String* string)
{
    Of of;
    of.string = string;
    return Value_Init(of, TYPE_STRING);
}

Value*
Value_NewNumber(double number)
{
    Of of;
    of.number = number;
    return Value_Init(of, TYPE_NUMBER);
}

Value*
Value_NewBool(bool boolean)
{
    Of of;
    of.boolean = boolean;
    return Value_Init(of, TYPE_BOOL);
}

Value*
Value_NewFile(File* file)
{
    Of of;
    of.file = file;
    return Value_Init(of, TYPE_FILE);
}

Value*
Value_NewNull(void)
{
    Of of;
    return Value_Init(of, TYPE_NULL);
}

String*
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

bool
Value_Subbing(Value* a, Value* b)
{
    return a->type == TYPE_CHAR && b->type == TYPE_STRING;
}

void
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

bool
Value_Pushing(Value* a, Value* b)
{
    return a->type == TYPE_QUEUE && b->type != TYPE_QUEUE;
}

bool
Value_CharAppending(Value* a, Value* b)
{
    return a->type == TYPE_STRING && b->type == TYPE_CHAR;
}

void
String_Alloc(String* self, int cap)
{
    self->cap = cap;
    self->value = Realloc(self->value, (1 + cap) * sizeof(*self->value));
}

char*
String_Value(String* self)
{
    return self->value;
}

String*
String_Init(char* string)
{
    String* self = Malloc(sizeof(*self));
    self->value = NULL;
    self->size = strlen(string);
    String_Alloc(self, self->size < STR_CAP_SIZE ? STR_CAP_SIZE : self->size);
    strcpy(self->value, string);
    return self;
}

void
String_Kill(String* self)
{
    Delete(Free, self->value);
    Free(self);
}

String*
String_FromChar(char c)
{
    char string[] = { c, '\0' };
    return String_Init(string);
}

void
String_Swap(String** self, String** other)
{
    String* temp = *self;
    *self = *other;
    *other = temp;
}

String*
String_Copy(String* self)
{
    return String_Init(self->value);
}

int
String_Size(String* self)
{
    return self->size;
}

void
String_Replace(String* self, char x, char y)
{
    for(int i = 0; i < String_Size(self); i++)
        if(self->value[i] == x)
            self->value[i] = y;
}

char*
String_End(String* self)
{
    return &self->value[String_Size(self) - 1];
}

char
String_Back(String* self)
{
    return *String_End(self);
}

char*
String_Begin(String* self)
{
    return &self->value[0];
}

int
String_Empty(String* self)
{
    return String_Size(self) == 0;
}

void
String_PshB(String* self, char ch)
{
    if(String_Size(self) == self->cap)
        String_Alloc(self, (self->cap == 0) ? 1 : (2 * self->cap));
    self->value[self->size + 0] = ch;
    self->value[self->size + 1] = '\0';
    self->size += 1;
}

String*
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

void
String_PopB(String* self)
{
    self->size -= 1;
    self->value[self->size] = '\0';
}

void
String_Resize(String* self, int size)
{
    if(size > self->cap)
        String_Alloc(self, size);
    self->size = size;
    self->value[size] = '\0';
}

bool
String_Equals(String* a, char* b)
{
    return Equals(a->value, b);
}

bool
String_Equal(String* a, String* b)
{
    return Equals(a->value, b->value);
}

void
String_Appends(String* self, char* str)
{
    while(*str)
    {
        String_PshB(self, *str);
        str += 1;
    }
}

void
String_Append(String* self, String* other)
{
    String_Appends(self, other->value);
    String_Kill(other);
}

String*
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

String*
String_Moves(char** from)
{
    String* self = Malloc(sizeof(*self));
    self->value = *from;
    self->size = strlen(self->value);
    self->cap = self->size;
    *from = NULL;
    return self;
}

String*
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

bool
String_IsBoolean(String* ident)
{
    return String_Equals(ident, "true") || String_Equals(ident, "false");
}

bool
String_IsNull(String* ident)
{
    return String_Equals(ident, "null");
}

String*
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

char*
String_Get(String* self, int index)
{
    if(index < 0 || index >= String_Size(self))
        return NULL;
    return &self->value[index];
}

bool
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

int
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

bool
String_IsUpper(int c)
{
    return c >= 'A' && c <= 'Z';
}

bool
String_IsLower(int c)
{
    return c >= 'a' && c <= 'z';
}

bool
String_IsAlpha(int c)
{
    return String_IsLower(c) || String_IsUpper(c);
}

bool
String_IsDigit(int c)
{
    return c >= '0' && c <= '9';
}

bool
String_IsNumber(int c)
{
    return String_IsDigit(c) || c == '.';
}

bool
String_IsIdentLeader(int c)
{
    return String_IsAlpha(c) || c == '_';
}

bool
String_IsIdent(int c)
{
    return String_IsIdentLeader(c) || String_IsDigit(c);
}

bool
String_IsModule(int c)
{
    return String_IsIdent(c) || c == '.';
}

bool
String_IsOp(int c)
{
    return c == '*' || c == '/' || c == '%' || c == '+' || c == '-' || c == '='
        || c == '<' || c == '>' || c == '!' || c == '&' || c == '|' || c == '?';
}

bool
String_IsSpace(int c)
{
    return c == '\n' || c == '\t' || c == '\r' || c == ' ';
}

File*
File_Init(String* path, String* mode)
{
    File* self = Malloc(sizeof(*self));
    self->path = path;
    self->mode = mode;
    self->file = fopen(self->path->value, self->mode->value);
    return self;
}

bool
File_Equal(File* a, File* b)
{
    return String_Equal(a->path, b->path) 
        && String_Equal(a->mode, b->mode)
        && a->file == b->file;
}

void
File_Kill(File* self)
{
    String_Kill(self->path);
    String_Kill(self->mode);
    if(self->file)
        fclose(self->file);
    Free(self);
}

File*
File_Copy(File* self)
{
    return File_Init(String_Copy(self->path), String_Copy(self->mode));
}

int
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

FILE*
File_File(File* self)
{
    return self->file;
}

Function*
Function_Init(String* name, int size, int address)
{
    Function* self = malloc(sizeof(*self));
    self->name = name;
    self->size = size;
    self->address = address;
    return self;
}

void
Function_Kill(Function* self)
{
    String_Kill(self->name);
    Free(self);
}

Function*
Function_Copy(Function* self)
{
    return Function_Init(String_Copy(self->name), Function_Size(self), Function_Address(self));
}

int
Function_Size(Function* self)
{
    return self->size;
}

int
Function_Address(Function* self)
{
    return self->address;
}

bool
Function_Equal(Function* a, Function* b)
{
    return String_Equal(a->name, b->name)
        && a->address == b->address
        && Function_Size(a) == Function_Size(b);
}

Block*
Block_Init(End end)
{
    Block* self = Malloc(sizeof(*self));
    self->a = (end == BACK) ? 0 : QUEUE_BLOCK_SIZE;
    self->b = (end == BACK) ? 0 : QUEUE_BLOCK_SIZE;
    return self;
}

int
Queue_Size(Queue* self)
{
    return self->size;
}

bool
Queue_Empty(Queue* self)
{
    return Queue_Size(self) == 0;
}

Block**
Queue_BlockF(Queue* self)
{
    return &self->block[0];
}

Block**
Queue_BlockB(Queue* self)
{
    return &self->block[self->blocks - 1];
}

void*
Queue_Front(Queue* self)
{
    if(Queue_Empty(self))
        return NULL;
    Block* block = *Queue_BlockF(self);
    return block->value[block->a];
}

void*
Queue_Back(Queue* self)
{
    if(Queue_Empty(self))
        return NULL;
    Block* block = *Queue_BlockB(self);
    return block->value[block->b - 1];
}

Queue*
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

void
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

void**
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

void*
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

void
Queue_Alloc(Queue* self, int blocks)
{
    self->blocks = blocks;
    self->block = Realloc(self->block, blocks * sizeof(*self->block));
}

void
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

void
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

void
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

void
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

void
Queue_PopB(Queue* self)
{
    Queue_Pop(self, BACK);
}

void
Queue_PopF(Queue* self)
{
    Queue_Pop(self, FRONT);
}

void
Queue_PopFSoft(Queue* self)
{
    Kill kill = self->kill;
    self->kill = NULL;
    Queue_PopF(self);
    self->kill = kill;
}

void
Queue_PopBSoft(Queue* self)
{
    Kill kill = self->kill;
    self->kill = NULL;
    Queue_PopB(self);
    self->kill = kill;
}

bool
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

Queue*
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

void
Queue_Prepend(Queue* self, Queue* other)
{
    for(int i = Queue_Size(other) - 1; i >= 0; i--)
    {
        void* temp = Queue_Get(other, i);
        void* value = self->copy ? self->copy(temp) : temp;
        Queue_PshF(self, value);
    }
}

void
Queue_Append(Queue* self, Queue* other)
{
    for(int i = 0; i < Queue_Size(other); i++)
    {
        void* temp = Queue_Get(other, i);
        void* value = self->copy ? self->copy(temp) : temp;
        Queue_PshB(self, value);
    }
}

bool
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

void
Queue_Swap(Queue* self, int a, int b)
{
    void** x = Queue_At(self, a);
    void** y = Queue_At(self, b);
    void* temp = *x;
    *x = *y;
    *y = temp;
}

void
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

void
Queue_Sort(Queue* self, bool (*compare)(void*, void*))
{
    Queue_RangedSort(self, compare, 0, Queue_Size(self) - 1);
}

String*
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

Node*
Node_Init(String* key, void* value)
{
    Node* self = Malloc(sizeof(*self));
    self->next = NULL;
    self->key = key;
    self->value = value;
    return self;
}

void
Node_Set(Node* self, Kill kill, String* key, void* value)
{
    Delete((Kill) String_Kill, self->key);
    Delete(kill, self->value);
    self->key = key;
    self->value = value;
}

void
Node_Kill(Node* self, Kill kill)
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
Node_Copy(Node* self, Copy copy)
{
    return Node_Init(String_Copy(self->key), copy ? copy(self->value) : self->value);
}

const int
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

int
Map_Buckets(Map* self)
{
    if(self->prime_index == MAP_UNPRIMED)
        return 0; 
    return Map_Primes[self->prime_index];
}

bool
Map_Resizable(Map* self)
{
    return self->prime_index < (int) LEN(Map_Primes) - 1;
}

Map*
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

int
Map_Size(Map* self)
{
    return self->size;
}

bool
Map_Empty(Map* self)
{
    return Map_Size(self) == 0;
}

void
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

unsigned
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

Node**
Map_Bucket(Map* self, char* key)
{
    unsigned index = Map_Hash(self, key);
    return &self->bucket[index];
}

void
Map_Alloc(Map* self, int index)
{
    self->prime_index = index;
    self->bucket = calloc(Map_Buckets(self), sizeof(*self->bucket));
}

void
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

bool
Map_Full(Map* self)
{
    return Map_Size(self) / (float) Map_Buckets(self) > self->load_factor;
}

void
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

Node*
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

bool
Map_Exists(Map* self, char* key)
{
    return Map_At(self, key) != NULL;
}

void
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

void
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

void*
Map_Get(Map* self, char* key)
{
    Node* found = Map_At(self, key);
    return found ? found->value : NULL;
}

Map*
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

void
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

bool
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

String*
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

Value*
Map_Key(Map* self)
{
    Value* queue = Value_NewQueue();
    for(int i = 0; i < Map_Buckets(self); i++)
    {
        Node* chain = self->bucket[i];
        while(chain)
        {
            Value* string = Value_NewString(String_Copy(chain->key));
            Queue_PshB(queue->of.queue, string);
            chain = chain->next;
        }
    }
    Queue_Sort(queue->of.queue, (Compare) Value_LessThan);
    return queue;
}

Char*
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

char*
Char_Value(Char* self)
{
    return self->value;
}

void
Char_Kill(Char* self)
{
    Value_Kill(self->string);
    Free(self);
}

char*
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

void
Type_Kill(Type type, Of* of)
{
    switch(type)
    {
    case TYPE_FILE:
        File_Kill(of-> file);
        break;
    case TYPE_FUNCTION:
        Function_Kill(of-> function);
        break;
    case TYPE_QUEUE:
        Queue_Kill(of-> queue);
        break;
    case TYPE_MAP:
        Map_Kill(of-> map);
        break;
    case TYPE_STRING:
        String_Kill(of-> string);
        break;
    case TYPE_CHAR:
        Char_Kill(of-> character);
        break;
    case TYPE_NUMBER:
    case TYPE_BOOL:
    case TYPE_NULL:
        break;
    }
}

void
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
