// THE ROMAN II PROGRAMMING LANGUAGE
// 
// COPYRIGHT (C) 2021-2022 GUSTAV LOUW. ALL RIGHTS RESERVED.
// THIS WORK IS LICENSED UNDER THE TERMS OF THE MIT LICENSE.  

#include <string.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

#define QUEUE_BLOCK_SIZE (8)
#define STR_CAP_SIZE (16)
#define MODULE_BUFFER_SIZE (8192)
#define LEN(a) (sizeof(a) / sizeof(*a))

typedef int64_t
(*Diff)(void*, void*);

typedef bool
(*Compare)(void*, void*);

typedef void
(*Kill)(void*);

typedef void*
(*Copy)(void*);

typedef struct Value Value;

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
    TYPE_POINTER,
    TYPE_NULL,
}
Type;

typedef struct
{
    char* value;
    int64_t size;
    int64_t cap;
}
String;

typedef struct
{
    Value* string;
    char* value;
}
Char;

typedef struct
{
    String* name;
    int64_t size;
    int64_t address;
}
Function;

typedef struct
{
    void* value[QUEUE_BLOCK_SIZE];
    int64_t a;
    int64_t b;
}
Block;

typedef struct
{
    Block** block;
    Kill kill;
    Copy copy;
    int64_t size;
    int64_t blocks;
}
Queue;

typedef struct Node
{
    struct Node* next;
    String* key;
    void* value;
}
Node;

typedef struct
{
    String* path;
    String* mode;
    FILE* file;
}
File;

typedef struct
{
    Node** bucket;
    Kill kill;
    Copy copy;
    int64_t size;
    int64_t prime_index;
    float load_factor;
    uint64_t rand1;
    uint64_t rand2;
}
Map;

typedef struct
{
    Value* value;
}
Pointer;

typedef union
{
    File* file;
    Function* function;
    Queue* queue;
    Map* map;
    String* string;
    Char* character;
    Pointer* pointer;
    double number;
    bool boolean;
}
Of;

struct Value
{
    Type type;
    Of of;
    int64_t refs;
};

typedef enum
{
    FRONT, BACK
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
}
Class;

#define OPCODES \
    X(Abs) X(Aco) X(Add) X(All) X(And) X(Any) X(Asi) X(Asr) X(Ata) X(Bol) X(Brf) X(Bsr) \
    X(Cal) X(Cel) X(Cop) X(Cos) X(Del) X(Div) X(Drf) X(End) X(Eql) X(Ext) X(Flr) X(Fls) \
    X(Get) X(Glb) X(God) X(Grt) X(Gte) X(Idv) X(Imd) X(Ins) X(Jmp) X(Key) X(Len) X(Loc) \
    X(Lod) X(Log) X(Lor) X(Lst) X(Lte) X(Max) X(Mem) X(Min) X(Mod) X(Mov) X(Mul) X(Neq) \
    X(Not) X(Num) X(Opn) X(Pop) X(Pow) X(Prt) X(Psb) X(Psf) X(Psh) X(Ptr) X(Qso) X(Ran) \
    X(Red) X(Ref) X(Ret) X(Sav) X(Sin) X(Slc) X(Spd) X(Sqr) X(Srd) X(Sub) X(Tan) X(Tim) \
    X(Trv) X(Typ) X(Vrt) X(Wrt)

typedef enum
{
#define X(name) OPCODE_##name,
OPCODES
#undef X
}
Opcode;

typedef struct
{
    Queue* modules;
    Queue* assembly;
    Queue* data;
    Queue* debug;
    Map* identifiers;
    Map* files;
    Map* included;
    String* prime;
    int64_t globals;
    int64_t locals;
    int64_t labels;
}
CC;

typedef struct
{
    FILE* file;
    String* name;
    int64_t index;
    int64_t size;
    int64_t line;
    unsigned char buffer[MODULE_BUFFER_SIZE];
}
Module;

typedef struct
{
    Class class;
    int64_t stack;
    String* path;
}
Meta;

typedef struct
{
    Queue* data;
    Queue* stack;
    Queue* frame;
    Queue* debug; // NOTE: OWNED BY CC.
    Queue* addresses;
    Map* data_dups;
    Map* track;
    Value* ret;
    uint64_t* instructions;
    int64_t size;
    int64_t pc;
    int64_t spds;
    int64_t retno;
    bool done;
}
VM;

typedef struct
{
    int64_t pc;
    int64_t sp;
    int64_t address;
}
Frame;

typedef struct
{
    String* label;
    int64_t address;
}
Stack;

typedef struct
{
    char* file;
    int64_t line;
}
Debug;

typedef struct
{
    char* mnemonic;
    Opcode opcode;
    void (*exec)(VM*, int64_t);
}
Gen;

typedef struct
{
    char* name;
    char* mnemonic;
    int64_t args;
}
Handle;

static void*
Malloc(int64_t size)
{
    return malloc(size);
}

static void*
Realloc(void *ptr, int64_t size)
{
    return realloc(ptr, size);
}

static void
Free(void* pointer)
{
    free(pointer);
}

static void*
Calloc(int64_t nmemb, int64_t size)
{
    return calloc(nmemb, size);
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

static double
Microseconds(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1e6 + tv.tv_usec;
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
String_Alloc(String* self, int64_t cap)
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

static void
String_Replace(String* self, char x, char y)
{
    for(int64_t i = 0; i < self->size; i++)
        if(self->value[i] == x)
            self->value[i] = y;
}

static char*
String_End(String* self)
{
    return &self->value[self->size - 1];
}

static char
String_Back(String* self)
{
    return *String_End(self);
}

static void
String_PshB(String* self, char ch)
{
    if(self->size == self->cap)
        String_Alloc(self, (self->cap == 0) ? 1 : (2 * self->cap));
    self->value[self->size + 0] = ch;
    self->value[self->size + 1] = '\0';
    self->size += 1;
}

static String*
String_Indent(int64_t indents)
{
    int64_t width = 4;
    String* ident = String_Init("");
    while(indents > 0)
    {
        for(int64_t i = 0; i < width; i++)
            String_PshB(ident, ' ');
        indents -= 1;
    }
    return ident;
}

static int64_t
String_ToUll(char* string)
{
    errno = 0;
    char* end;
    int64_t ull = strtoull(string, &end, 10);
    if(*end != '\0' || errno != 0)
        Quit("%s is not a valid number", string);
    return ull;
}

static double
String_ToNumber(char* string)
{
    errno = 0;
    char* end;
    double decimal = strtod(string, &end);
    if(*end != '\0' || errno != 0)
        Quit("%s is not a valid number", string);
    return decimal;
}

static void
String_PopB(String* self)
{
    self->size -= 1;
    self->value[self->size] = '\0';
}

static void
String_Resize(String* self, int64_t size)
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
    while(base->size != 0)
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

static inline bool
String_IsBoolean(String* ident)
{
    return String_Equals(ident, "true") || String_Equals(ident, "false");
}

static inline bool
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
    int64_t size = vsnprintf(line->value, line->size, format, args);
    va_end(args);
    if(size >= line->size)
    {
        va_start(args, format);
        String_Resize(line, size);
        vsprintf(line->value, format, args);
        va_end(args);
    }
    else
        line->size = size;
    return line;
}

static char*
String_Get(String* self, int64_t index)
{
    if(index == -1)
        return &self->value[self->size - 1];
    if(index >= self->size)
        return NULL;
    return &self->value[index];
}

static bool
String_Del(String* self, int64_t index)
{
    if(index == -1)
    {
        String_PopB(self);
        return true;
    }
    else
    if(String_Get(self, index))
    {
        for(int64_t i = index; i < self->size - 1; i++)
            self->value[i] = self->value[i + 1];
        String_PopB(self);
        return true;
    }
    return false;
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
    struct stat stat1;
    struct stat stat2;
    if(fstat(fileno(a->file), &stat1) < 0
    || fstat(fileno(b->file), &stat2) < 0)
        return false;
    return String_Equal(a->path, b->path) 
        && String_Equal(a->mode, b->mode)
        && stat1.st_dev == stat2.st_dev
        && stat1.st_ino == stat2.st_ino;
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

static int64_t
File_Size(File* self)
{
    if(self->file)
    {
        int64_t prev = ftell(self->file);
        fseek(self->file, 0L, SEEK_END);
        int64_t size = ftell(self->file);
        fseek(self->file, prev, SEEK_SET);
        return size;
    }
    else
        return 0;
}

static Function*
Function_Init(String* name, int64_t size, int64_t address)
{
    Function* self = Malloc(sizeof(*self));
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

static bool
Function_Equal(Function* a, Function* b)
{
    return String_Equal(a->name, b->name) && a->address == b->address && a->size == b->size;
}

static Block*
Block_Init(End end)
{
    Block* self = Malloc(sizeof(*self));
    self->a = (end == BACK) ? 0 : QUEUE_BLOCK_SIZE;
    self->b = (end == BACK) ? 0 : QUEUE_BLOCK_SIZE;
    return self;
}

static inline Block**
Queue_BlockF(Queue* self)
{
    return &self->block[0];
}

static inline Block**
Queue_BlockB(Queue* self)
{
    return &self->block[self->blocks - 1];
}

static void*
Queue_Front(Queue* self)
{
    if(self->size == 0)
        return NULL;
    Block* block = *Queue_BlockF(self);
    return block->value[block->a];
}

static void*
Queue_Back(Queue* self)
{
    if(self->size == 0)
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
    for(int64_t step = 0; step < self->blocks; step++)
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
Queue_At(Queue* self, int64_t index)
{
    if(index < self->size)
    {
        Block* block = *Queue_BlockF(self);
        int64_t at = index + block->a;
        int64_t step = at / QUEUE_BLOCK_SIZE;
        int64_t item = at % QUEUE_BLOCK_SIZE;
        return &self->block[step]->value[item];
    }
    return NULL;
}

static void*
Queue_Get(Queue* self, int64_t index)
{
    if(index == 0)
        return Queue_Front(self);
    else
    if(index == self->size - 1 || index == -1)
        return Queue_Back(self);
    else
    {
        void** at = Queue_At(self, index);
        return at ? *at : NULL;
    }
}

static void
Queue_Alloc(Queue* self, int64_t blocks)
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
            for(int64_t i = self->blocks - 1; i > 0; i--)
                self->block[i] = self->block[i - 1];
            *Queue_BlockF(self) = Block_Init(FRONT);
        }
    }
    Queue_Push(self, value, FRONT);
}

static void
Queue_Pop(Queue* self, End end)
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
                for(int64_t i = 0; i < self->blocks - 1; i++)
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
Queue_Del(Queue* self, int64_t index)
{
    if(index == 0)
        Queue_PopF(self);
    else
    if(index == self->size - 1 || index == -1)
        Queue_PopB(self);
    else
    {
        void** at = Queue_At(self, index);
        if(at)
        {
            Delete(self->kill, *at);
            if(index < self->size / 2)
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
                while(index < self->size - 1)
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
    for(int64_t i = 0; i < self->size; i++)
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
    int64_t i = other->size;
    while(i != 0)
    {
        i -= 1;
        void* temp = Queue_Get(other, i);
        void* value = self->copy ? self->copy(temp) : temp;
        Queue_PshF(self, value);
    }
}

static void
Queue_Append(Queue* self, Queue* other)
{
    for(int64_t i = 0; i < other->size; i++)
    {
        void* temp = Queue_Get(other, i);
        void* value = self->copy ? self->copy(temp) : temp;
        Queue_PshB(self, value);
    }
}

static bool
Queue_Equal(Queue* self, Queue* other, Compare compare)
{
    if(self->size != other->size)
        return false;
    else
        for(int64_t i = 0; i < self->size; i++)
            if(!compare(Queue_Get(self, i), Queue_Get(other, i)))
                return false;
    return true;
}

static void
Queue_Swap(Queue* self, int64_t a, int64_t b)
{
    void** x = Queue_At(self, a);
    void** y = Queue_At(self, b);
    void* temp = *x;
    *x = *y;
    *y = temp;
}

static void
Queue_RangedSort(Queue* self, Compare compare, int64_t left, int64_t right)
{
    if(left >= right)
        return;
    Queue_Swap(self, left, (left + right) / 2);
    int64_t last = left;
    for(int64_t i = left + 1; i <= right; i++)
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
Queue_Sort(Queue* self, Compare compare)
{
    Queue_RangedSort(self, compare, 0, self->size - 1);
}

static String*
Value_Sprint(Value*, bool newline, int64_t indents, int64_t width, int64_t preci);

static String*
Queue_Print(Queue* self, int64_t indents)
{
    if(self->size == 0)
        return String_Init("[]");
    else
    {
        String* print = String_Init("[\n");
        int64_t size = self->size;
        for(int64_t i = 0; i < size; i++)
        {
            String_Append(print, String_Indent(indents + 1));
            String_Append(print, String_Format("[%ld] = ", i));
            String_Append(print, Value_Sprint(Queue_Get(self, i), false, indents + 1, -1, -1));
            if(i < size - 1)
                String_Appends(print, ",");
            String_Appends(print, "\n");
        }
        String_Append(print, String_Indent(indents));
        String_Appends(print, "]");
        return print;
    }
}

static void*
Queue_BSearch(Queue* self, void* key, Diff diff)
{
    int64_t low = 0;
    int64_t high = self->size - 1;
    while(low <= high)
    {
        int64_t mid = (low + high) / 2;
        void* now = Queue_Get(self, mid);
        int64_t cmp = diff(key, now);
        if(cmp == 0)
            return now;
        else
        if(cmp < 0)
            high = mid - 1;
        else
            low = mid + 1;
    }
    return NULL;
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

static const int64_t
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

static int64_t
Map_Buckets(Map* self)
{
    if(self->prime_index == -1)
        return 0; 
    return Map_Primes[self->prime_index];
}

static bool
Map_Resizable(Map* self)
{
    return self->prime_index < (int64_t) LEN(Map_Primes) - 1;
}

static Map*
Map_Init(Kill kill, Copy copy)
{
    Map* self = Malloc(sizeof(*self));
    self->kill = kill;
    self->copy = copy;
    self->bucket = NULL;
    self->size = 0;
    self->prime_index = -1;
    self->load_factor = 0.7f;
    self->rand1 = rand();
    self->rand2 = rand();
    return self;
}

static void
Map_Kill(Map* self)
{
    for(int64_t i = 0; i < Map_Buckets(self); i++)
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

static uint64_t
Map_Hash(Map* self, char* key)
{
    uint64_t r1 = self->rand1;
    uint64_t r2 = self->rand2;
    uint64_t hash = 0;
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
    uint64_t index = Map_Hash(self, key);
    return &self->bucket[index];
}

static void
Map_Alloc(Map* self, int64_t index)
{
    self->prime_index = index;
    self->bucket = Calloc(Map_Buckets(self), sizeof(*self->bucket));
}

static void
Map_Emplace(Map* self, String* key, Node* node);

static void
Map_Rehash(Map* self)
{
    Map* other = Map_Init(self->kill, self->copy);
    Map_Alloc(other, self->prime_index + 1);
    for(int64_t i = 0; i < Map_Buckets(self); i++)
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
    return self->size / (float) Map_Buckets(self) > self->load_factor;
}

static void
Map_Emplace(Map* self, String* key, Node* node)
{
    if(self->prime_index == -1)
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
    if(self->size != 0) 
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
    if(self->size != 0) 
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
    for(int64_t i = 0; i < Map_Buckets(self); i++)
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
    for(int64_t i = 0; i < Map_Buckets(other); i++)
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
    if(self->size == other->size)
    {
        for(int64_t i = 0; i < Map_Buckets(self); i++)
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
Map_Print(Map* self, int64_t indents)
{
    if(self->size == 0)
        return String_Init("{}");
    else
    {
        String* print = String_Init("{\n");
        for(int64_t i = 0; i < Map_Buckets(self); i++)
        {
            Node* bucket = self->bucket[i];
            while(bucket)
            {
                String_Append(print, String_Indent(indents + 1));
                String_Append(print, String_Format("\"%s\" : ", bucket->key->value));
                String_Append(print, Value_Sprint(bucket->value, false, indents + 1, -1, -1));
                if(i < self->size - 1)
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

static bool
Value_LessThan(Value*, Value*);

static Value*
Value_Queue(void);

static Value*
Value_String(String*);

static Value*
Map_Key(Map* self)
{
    Value* queue = Value_Queue();
    for(int64_t i = 0; i < Map_Buckets(self); i++)
    {
        Node* chain = self->bucket[i];
        while(chain)
        {
            Value* string = Value_String(String_Copy(chain->key));
            Queue_PshB(queue->of.queue, string);
            chain = chain->next;
        }
    }
    Queue_Sort(queue->of.queue, (Compare) Value_LessThan);
    return queue;
}

static Char*
Char_Init(Value* string, int64_t index)
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
Value_Kill(Value*);

static void
Char_Kill(Char* self)
{
    Value_Kill(self->string);
    Free(self);
}

static char*
Type_ToString(Type self)
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
    case TYPE_POINTER:
        return "pointer";
    case TYPE_BOOL:
        return "bool";
    case TYPE_NULL:
        return "null";
    }
    return "N/A";
}

static void Value_Dec(Value* self) { self->refs -= 1; }
static void Value_Inc(Value* self) { self->refs += 1; }

static void
Pointer_Kill(Pointer* self)
{
    Value_Kill(self->value);
    Free(self);
}

static Pointer*
Pointer_Init(Value* value)
{
    Value_Inc(value);
    Pointer* self = Malloc(sizeof(*self));
    self->value = value;
    return self;
}

static Pointer*
Pointer_Copy(Pointer* other)
{
    return Pointer_Init(other->value);
}

static void
Type_Kill(Type type, Of* of)
{
    switch(type)
    {
    case TYPE_FILE:
        File_Kill(of->file);
        break;
    case TYPE_FUNCTION:
        Function_Kill(of->function);
        break;
    case TYPE_QUEUE:
        Queue_Kill(of->queue);
        break;
    case TYPE_MAP:
        Map_Kill(of->map);
        break;
    case TYPE_POINTER:
        Pointer_Kill(of->pointer);
        break;
    case TYPE_STRING:
        String_Kill(of->string);
        break;
    case TYPE_CHAR:
        Char_Kill(of->character);
        break;
    case TYPE_NUMBER:
    case TYPE_BOOL:
    case TYPE_NULL:
        break;
    }
}

static int64_t*
Int_Init(int64_t value)
{
    int64_t* self = Malloc(sizeof(*self));
    *self = value;
    return self;
}

static void
Int_Kill(int64_t* self)
{
    Free(self);
}

static void
Type_Copy(Value* copy, Value* self)
{
    copy->type = self->type;
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
    case TYPE_POINTER:
        copy->of.pointer = Pointer_Copy(self->of.pointer);
        break;
    case TYPE_STRING:
        copy->of.string = String_Copy(self->of.string);
        break;
    case TYPE_CHAR:
        // CHARS PROMOTE TO STRINGS ON COPY.
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

static int64_t
Value_Len(Value* self)
{
    switch(self->type)
    {
    case TYPE_FILE:
        return File_Size(self->of.file);
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
    case TYPE_POINTER:
    case TYPE_NULL:
        break;
    }
    return 0;
}

static void
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
    case TYPE_POINTER:                            \
        break;                                    \
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
        case TYPE_POINTER:
            return a->of.pointer->value == b->of.pointer->value;
        }
    return false;
}

static bool
Value_NotEqual(Value* a, Value* b)
{
    return !Value_Equal(a, b);
}

static Value*
Value_Copy(Value* self)
{
    Value* copy = Malloc(sizeof(*copy));
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
    self->of = of;
    return self;
}

static Value*
Value_Queue(void)
{
    Of of;
    of.queue = Queue_Init((Kill) Value_Kill, (Copy) Value_Copy);
    return Value_Init(of, TYPE_QUEUE);
}

static Value*
Value_Map(void)
{
    Of of;
    of.map = Map_Init((Kill) Value_Kill, (Copy) Value_Copy);
    return Value_Init(of, TYPE_MAP);
}

static Value*
Value_Pointer(Pointer* pointer)
{
    Of of;
    of.pointer = pointer;
    return Value_Init(of, TYPE_POINTER);
}

static Value*
Value_Function(Function* function)
{
    Of of;
    of.function = function;
    return Value_Init(of, TYPE_FUNCTION);
}

static Value*
Value_Char(Char* character)
{
    Of of;
    of.character = character;
    return Value_Init(of, TYPE_CHAR);
}

static Value*
Value_String(String* string)
{
    Of of;
    of.string = string;
    return Value_Init(of, TYPE_STRING);
}

static Value*
Value_Number(double number)
{
    Of of;
    of.number = number;
    return Value_Init(of, TYPE_NUMBER);
}

static Value*
Value_Bool(bool boolean)
{
    Of of;
    of.boolean = boolean;
    return Value_Init(of, TYPE_BOOL);
}

static Value*
Value_File(File* file)
{
    Of of;
    of.file = file;
    return Value_Init(of, TYPE_FILE);
}

static Value*
Value_Null(void)
{
    Of of;
    return Value_Init(of, TYPE_NULL);
}

static String*
Value_Sprint(Value* self, bool newline, int64_t indents, int64_t width, int64_t preci)
{
    if(width == -1) width = 0;
    if(preci == -1) preci = 5;
    String* print = String_Init("");
    switch(self->type)
    {
    case TYPE_FILE:
        String_Append(print, String_Format("<\"%s\", \"%s\", %p>", self->of.file->path->value, self->of.file->mode->value, self->of.file->file));
        break;
    case TYPE_FUNCTION:
        String_Append(print, String_Format("<%s, %ld, %ld>", self->of.function->name->value, self->of.function->size, self->of.function->address));
        break;
    case TYPE_QUEUE:
        String_Append(print, Queue_Print(self->of.queue, indents));
        break;
    case TYPE_MAP:
        String_Append(print, Map_Print(self->of.map, indents));
        break;
    case TYPE_STRING:
        String_Append(print, String_Format(indents == 0 ? "%*s" : "\"%*s\"", width, self->of.string->value));
        break;
    case TYPE_NUMBER:
        String_Append(print, String_Format("%*.*f", width, preci, self->of.number));
        break;
    case TYPE_BOOL:
        String_Append(print, String_Format("%s", self->of.boolean ? "true" : "false"));
        break;
    case TYPE_CHAR:
        String_Append(print, String_Format(indents == 0 ? "%c" : "\"%c\"", *self->of.character->value));
        break;
    case TYPE_POINTER:
        String_Append(print, String_Format("%p", self->of.pointer->value));
        break;
    case TYPE_NULL:
        String_Appends(print, "null");
        break;
    }
    if(newline)
        String_Appends(print, "\n");
    return print; 
}

static void
Value_Print(Value* self, int64_t width, int64_t preci)
{
    String* out = Value_Sprint(self, false, 0, width, preci);
    puts(out->value);
    String_Kill(out);
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

static void
Value_PromoteChar(Value* ch)
{
    Value* copy = Value_Copy(ch);
    Type_Kill(ch->type, &ch->of);
    Type_Copy(ch, copy);
    Value_Kill(copy);
}

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
    }
    return "N/A";
}

static Frame*
Frame_Init(int64_t pc, int64_t sp, int64_t address)
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
Meta_Init(Class class, int64_t stack, String* path)
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
Module_Init(String* name)
{
    FILE* file = fopen(name->value, "r");
    if(file)
    {
        Module* self = Malloc(sizeof(*self));
        self->file = file;
        self->line = 1;
        self->name = String_Copy(name);
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

static int64_t
Module_At(Module* self)
{
    return self->buffer[self->index];
}

static int64_t
Module_Peak(Module* self)
{
    if(self->index == self->size)
        Module_Buffer(self);
    return self->size == 0 ? EOF : Module_At(self);
}

static void
Module_Advance(Module* self)
{
    int64_t at = Module_At(self);
    if(at == '\n')
        self->line += 1;
    self->index += 1;
}

static String*
CC_CurrentFile(CC* self)
{
    Module* back = Queue_Back(self->modules);
    return back->name;
}

static void
CC_Quit(CC* self, const char* const message, ...)
{
    Module* back = Queue_Back(self->modules);
    va_list args;
    va_start(args, message);
    fprintf(stderr, "error: file %s: line %ld: ", back ? back->name->value : "?", back ? back->line : 0);
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(0xFF);
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
    return CC_String_IsDigit(c) || c == '.' || c == 'e' || c == 'E';
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
    return c == '*' || c == '/' || c == '%' || c == '+' || c == '-' || c == '=' || c == '<' || c == '>' || c == '!' || c == '&' || c == '|' || c == '?';
}

static bool
CC_String_IsSpace(int64_t c)
{
    return c == '\n' || c == '\t' || c == '\r' || c == ' ';
}

static void
CC_Advance(CC* self)
{
    Module_Advance(Queue_Back(self->modules));
}

static int64_t
CC_Peak(CC* self)
{
    int64_t peak;
    while(true)
    {
        peak = Module_Peak(Queue_Back(self->modules));
        if(peak == EOF)
        {
            if(self->modules->size == 1)
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

static String*
CC_Stream(CC* self, bool clause(int64_t ))
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
        int64_t peak = CC_Read(self);
        if(peak != *expect)
        {
            char formatted[] = { peak, '\0' };
            CC_Quit(self, "matched character `%s` but expected character `%c`", (peak == EOF) ? "EOF" : formatted, *expect);
        }
        expect += 1;
    }
}

static int64_t
CC_String_EscToByte(int64_t ch)
{
    switch(ch)
    {
    case '"' : return '\"';
    case '\\': return '\\';
    case '/' : return '/';
    case 'b' : return '\b';
    case 'f' : return '\f';
    case 'n' : return '\n';
    case 'r' : return '\r';
    case 't' : return '\t';
    }
    return -1;
}

static String*
CC_Mod(CC* self)
{
    return CC_Stream(self, CC_String_IsModule);
}

static String*
CC_Ident(CC* self)
{
    return CC_Stream(self, CC_String_IsIdent);
}

static String*
CC_Operator(CC* self)
{
    return CC_Stream(self, CC_String_IsOp);
}

static String*
CC_Number(CC* self)
{
    return CC_Stream(self, CC_String_IsNumber);
}

static String*
CC_StringStream(CC* self)
{
    String* str = String_Init("");
    CC_Spin(self);
    CC_Match(self, "\"");
    while(CC_Peak(self) != '"')
    {
        int64_t ch = CC_Read(self);
        String_PshB(str, ch);
        if(ch == '\\')
        {
            ch = CC_Read(self);
            int64_t byte = CC_String_EscToByte(ch);
            if(byte == -1)
                CC_Quit(self, "an unknown escape char 0x%02X was encountered", ch);
            String_PshB(str, ch);
        }
    }
    CC_Match(self, "\"");
    return str;
}

static Debug*
Debug_Init(char* file, int64_t line)
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
    self->modules = Queue_Init((Kill) Module_Kill, (Copy) NULL);
    self->assembly = Queue_Init((Kill) String_Kill, (Copy) NULL);
    self->data = Queue_Init((Kill) String_Kill, (Copy) NULL);
    self->debug = Queue_Init((Kill) Debug_Kill, (Copy) NULL);
    self->files = Map_Init((Kill) NULL, (Copy) NULL);
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
    Queue_Kill(self->debug);
    Map_Kill(self->included);
    Map_Kill(self->files);
    Map_Kill(self->identifiers);
    Free(self);
}

static char*
CC_RealPath(CC* self, String* file)
{
    String* path;
    if(self->modules->size == 0)
    {
        path = String_Init("");
        String_Resize(path, 4096);
        getcwd(path->value, path->size);
        strcat(path->value, "/");
        strcat(path->value, file->value);
    }
    else
    {
        Module* back = Queue_Back(self->modules);
        path = String_Base(back->name);
        String_Appends(path, file->value);
    }
    char* real = realpath(path->value, NULL);
    if(real == NULL)
        CC_Quit(self, "%s could not be resolved as a real path", path->value);
    String_Kill(path);
    return real;
}

static void
CC_Including(CC* self, String* file)
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

static String*
Module_Name(CC* self, char* postfix)
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

static Handle Handles[] = {
    { "Abs",     "Abs", 1 },
    { "Acos",    "Aco", 1 },
    { "All",     "All", 1 },
    { "Any",     "Any", 1 },
    { "Asin",    "Asi", 1 },
    { "Assert",  "Asr", 1 },
    { "Atan",    "Ata", 1 },
    { "Bool",    "Bol", 1 },
    { "Bsearch", "Bsr", 3 },
    { "Ceil",    "Cel", 1 },
    { "Copy",    "Cop", 1 },
    { "Cos",     "Cos", 1 },
    { "Del",     "Del", 2 },
    { "Exit",    "Ext", 1 },
    { "Floor",   "Flr", 1 },
    { "Keys",    "Key", 1 },
    { "Len",     "Len", 1 },
    { "Log",     "Log", 1 },
    { "Num",     "Num", 1 },
    { "Max",     "Max", 2 },
    { "Min",     "Min", 2 },
    { "Open",    "Opn", 2 },
    { "Pow",     "Pow", 1 },
    { "Print",   "Prt", 1 },
    { "Qsort",   "Qso", 2 },
    { "Rand",    "Ran", 0 },
    { "Read",    "Red", 2 },
    { "Refs",    "Ref", 1 },
    { "Sin",     "Sin", 1 },
    { "Sqrt",    "Sqr", 1 },
    { "Srand",   "Srd", 1 },
    { "String",  "Str", 1 },
    { "Tan",     "Tan", 1 },
    { "Time",    "Tim", 0 },
    { "Type",    "Typ", 1 },
    { "Write",   "Wrt", 2 },
};

static int
Handle_Compare(const void* a, const void* b)
{
    const Handle* aa = a;
    const Handle* bb = b;
    return strcmp(aa->name, bb->name);
}

static void
Handle_Sort(void)
{
    qsort(Handles, LEN(Handles), sizeof(Handle), Handle_Compare);
}

static Handle*
Handle_Find(char* name)
{
    Handle key = { .name = name };
    return bsearch(&key, Handles, LEN(Handles), sizeof(Handle), Handle_Compare);
}

static void
CC_Include(CC* self)
{
    String* name = Module_Name(self, ".rr");
    CC_Match(self, ";");
    CC_Including(self, name);
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
        || class == CLASS_FUNCTION_PROTOTYPE;
}

static void
CC_Define(CC* self, Class class, int64_t stack, String* ident, String* path)
{
    Meta* old = Map_Get(self->identifiers, ident->value);
    Meta* new = Meta_Init(class, stack, path);
    if(old)
    {
        if(old->class == CLASS_FUNCTION_PROTOTYPE && new->class == CLASS_FUNCTION)
        {
            if(new->stack != old->stack)
                CC_Quit(self, "function %s with %ld argument(s) was previously defined in file %s as a function prototype with %ld argument(s)",
                        ident->value, new->stack, old->path->value, old->stack);
        }
        else
            CC_Quit(self, "%s %s was already defined in file %s as a %s",
                    Class_ToString(new->class), ident->value, old->path->value, Class_ToString(old->class));
    }
    Map_Set(self->identifiers, ident, new);
}

static Debug*
CC_Debug(CC* self)
{
    Module* back = Queue_Back(self->modules);
    Debug* debug;
    Node* at = Map_At(self->files, back->name->value);
    if(at)
    {
        String* string = at->key;
        debug = Debug_Init(string->value, back->line);
    }
    else
    {
        String* name = String_Copy(back->name);
        Map_Set(self->files, name, NULL);
        debug = Debug_Init(name->value, back->line);
    }
    return debug;
}

static void
CC_Assem(CC* self, String* assem, End end)
{
    void (*push)(Queue*, void*) = (end == BACK) ? Queue_PshB : Queue_PshF;
    push(self->assembly, assem);
    if(assem->value[0] == '\t')
    {
        Debug* debug = CC_Debug(self);
        push(self->debug, debug);
    }
}

static void
CC_AssemB(CC* self, String* assem)
{
    CC_Assem(self, assem, BACK);
}

static void
CC_AssemF(CC* self, String* assem)
{
    CC_Assem(self, assem, FRONT);
}

static void
CC_Pops(CC* self, int64_t count)
{
    if(count > 0)
        CC_AssemB(self, String_Format("\tPop %ld", count));
}

static void
CC_Expression(CC*);

static void
CC_ConsumeExpression(CC* self)
{
    CC_Expression(self);
    CC_Pops(self, 1);
}

static void
CC_Assign(CC* self)
{
    CC_Match(self, ":=");
    CC_Expression(self);
    CC_AssemB(self, String_Init("\tCop"));
}

static void
CC_Local(CC* self, String* ident)
{
    CC_Define(self, CLASS_VARIABLE_LOCAL, self->locals, ident, String_Copy(CC_CurrentFile(self)));
    self->locals += 1;
}

static void
CC_AssignLocal(CC* self, String* ident)
{
    CC_Assign(self);
    CC_Match(self, ";");
    CC_Local(self, ident);
}

static String*
CC_Global(CC* self, String* ident)
{
    String* label = String_Format("!%s", ident->value);
    CC_AssemB(self, String_Format("%s:", label->value));
    CC_Assign(self);
    CC_Match(self, ";");
    CC_Define(self, CLASS_VARIABLE_GLOBAL, self->globals, ident, String_Copy(CC_CurrentFile(self)));
    CC_AssemB(self, String_Init("\tRet"));
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
    for(int64_t i = 0; i < params->size; i++)
    {
        String* ident = String_Copy(Queue_Get(params, i));
        // ARGUMENTS ARE PASSED TO FUNCTIONS AS REFERENCE TYPES.
        CC_Local(self, ident);
    }
}

static int64_t
CC_PopScope(CC* self, Queue* scope)
{
    int64_t popped = scope->size;
    for(int64_t i = 0; i < popped; i++)
    {
        String* key = Queue_Get(scope, i);
        Map_Del(self->identifiers, key->value);
        self->locals -= 1;
    }
    CC_Pops(self, popped);
    Queue_Kill(scope);
    return popped;
}

static Meta*
CC_Meta(CC* self, String* ident)
{
    Meta* meta = Map_Get(self->identifiers, ident->value);
    if(meta == NULL)
        CC_Quit(self, "identifier %s not defined", ident->value);
    return meta;
}

static Meta*
CC_Expect(CC* self, String* ident, bool clause(Class))
{
    Meta* meta = CC_Meta(self, ident);
    if(!clause(meta->class))
        CC_Quit(self, "identifier %s cannot be of class %s", ident->value, Class_ToString(meta->class));
    return meta;
}

static void
CC_Ref(CC* self, String* ident)
{
    Meta* meta = CC_Expect(self, ident, CC_IsVariable);
    if(meta->class == CLASS_VARIABLE_GLOBAL)
        CC_AssemB(self, String_Format("\tGlb %ld", meta->stack));
    else
    if(meta->class == CLASS_VARIABLE_LOCAL)
        CC_AssemB(self, String_Format("\tLoc %ld", meta->stack));
}

static bool
CC_Factor(CC*);

static void
CC_Pointer(CC* self)
{
    CC_Match(self, "&");
    CC_Factor(self);
    CC_AssemB(self, String_Init("\tPtr"));
}

static void
CC_String(CC* self)
{
    String* string = CC_StringStream(self);
    CC_AssemB(self, String_Format("\tPsh \"%s\"", string->value));
    String_Kill(string);
}

static int64_t
CC_Args(CC* self)
{
    CC_Match(self, "(");
    int64_t args = 0;
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
CC_Vrt(CC* self)
{
    int64_t size = CC_Args(self);
    CC_AssemB(self, String_Format("\tPsh %ld", size));
    CC_AssemB(self, String_Init("\tVrt"));
    CC_AssemB(self, String_Init("\tTrv"));
}

static bool
CC_Resolve(CC* self)
{
    bool storage = true;
    while(CC_Next(self) == '['
       || CC_Next(self) == '.'
       || CC_Next(self) == '(')
    {
        if(CC_Next(self) == '(')
        {
            CC_Vrt(self);
            storage = false;
        }
        else
        {
            bool slice = false;
            if(CC_Next(self) == '[')
            {
                CC_Match(self, "[");
                CC_Expression(self);
                if(CC_Next(self) == ':')
                {
                    CC_Match(self, ":");
                    CC_Expression(self);
                    slice = true;
                    storage = false;
                }
                else
                    storage = true;
                CC_Match(self, "]");
            }
            else
            if(CC_Next(self) == '.')
            {
                CC_Match(self, ".");
                String* ident = CC_Ident(self);
                CC_AssemB(self, String_Format("\tPsh \"%s\"", ident->value));
                String_Kill(ident);
                storage = true;
            }
            if(CC_Next(self) == ':')
            {
                CC_Assign(self);
                CC_AssemB(self, String_Init("\tIns"));
            }
            else
            {
                if(slice)
                    CC_AssemB(self, String_Init("\tSlc"));
                else
                    CC_AssemB(self, String_Init("\tGet"));
            }
        }
    }
    return storage;
}

static void
CC_Call(CC* self, String* ident, int64_t args)
{
    for(int64_t i = 0; i < args; i++)
        CC_AssemB(self, String_Init("\tSpd"));
    CC_AssemB(self, String_Format("\tCal %s", ident->value));
    CC_AssemB(self, String_Init("\tLod"));
}

static void
CC_Map(CC* self)
{
    CC_AssemB(self, String_Init("\tPsh {}"));
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
            CC_AssemB(self, String_Init("\tPsh true"));
        CC_AssemB(self, String_Init("\tIns"));
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    CC_Match(self, "}");
}

static void
CC_Queue(CC* self)
{
    CC_AssemB(self, String_Init("\tPsh []"));
    CC_Match(self, "[");
    while(CC_Next(self) != ']')
    {
        CC_Expression(self);
        CC_AssemB(self, String_Init("\tPsb"));
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    CC_Match(self, "]");
}

static void
CC_Direct(CC* self, bool negative)
{
    String* number = CC_Number(self);
    CC_AssemB(self, String_Format("\tPsh %s%s", negative ? "-" : "", number->value));
    String_Kill(number);
}

static void
CC_DirectCalling(CC* self, String* ident, Meta* meta)
{
    int64_t size = CC_Args(self);
    if(size != meta->stack)
        CC_Quit(self, "calling function %s required %ld arguments but got %ld arguments", ident->value, meta->stack, size);
    Handle* handle = Handle_Find(ident->value);
    if(handle == NULL || (handle && handle->args == -1))
        CC_Call(self, ident, size);
    else
        CC_AssemB(self, String_Format("\t%s", handle->mnemonic));
}

static void
CC_Calling(CC* self, String* ident)
{
    Meta* meta = CC_Meta(self, ident);
    if(CC_IsFunction(meta->class))
        CC_DirectCalling(self, ident, meta);
    else
    if(CC_IsVariable(meta->class))
        // LEAVE A REFERENCE VALUE ON THE STACK FOR CC_RESOLVE TO HANDLE.
        CC_Ref(self, ident);
    else
        CC_Quit(self, "identifier %s is not callable", ident->value);
}

static bool
CC_Referencing(CC* self, String* ident)
{
    Meta* meta = CC_Meta(self, ident);
    if(CC_IsFunction(meta->class))
        CC_AssemB(self, String_Format("\tPsh @%s,%ld", ident->value, meta->stack));
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
        CC_AssemB(self, String_Format("\tPsh %s", ident->value));
    else
    if(String_IsNull(ident))
        CC_AssemB(self, String_Format("\tPsh %s", ident->value));
    else
    if(CC_Next(self) == '(')
        CC_Calling(self, ident);
    else
        storage = CC_Referencing(self, ident);
    String_Kill(ident);
    return storage;
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

static void
CC_Deref(CC* self)
{
    CC_Match(self, "*");
    CC_Factor(self);
    CC_AssemB(self, String_Init("\tDrf"));
}

static void
CC_Not(CC*);

static bool
CC_Factor(CC* self)
{
    bool storage = false;
    int64_t next = CC_Next(self);
    if(CC_String_IsDigit(next))
        CC_Direct(self, false);
    else
    if(CC_String_IsIdent(next) || self->prime)
        storage = CC_Identifier(self);
    else switch(next)
    {
    case '!':
        CC_Not(self);
        break;
    case '-':
        CC_DirectNeg(self);
        break;
    case '+':
        CC_DirectPos(self);
        break;
    case '(':
        CC_Force(self);
        break;
    case '{':
        CC_Map(self);
        break;
    case '[':
        CC_Queue(self);
        break;
    case '"':
        CC_String(self);
        break;
    case '*':
        storage = true;
        CC_Deref(self);
        break;
    case '&':
        CC_Pointer(self);
        break;
    default:
        CC_Quit(self, "an unknown factor starting with `%c` was encountered", next);
        break;
    }
    storage &= CC_Resolve(self);
    return storage;
}

static void
CC_Not(CC* self)
{
    CC_Match(self, "!");
    CC_Factor(self);
    CC_AssemB(self, String_Init("\tNot"));
}

static void
CC_StorageCheck(CC* self, String* operator, bool storage)
{
    if(storage == false)
        CC_Quit(self, "the left hand side of operator %s must be storage", operator->value);
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
            CC_StorageCheck(self, operator, storage);
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tMul"));
        }
        else
        if(String_Equals(operator, "%%="))
        {
            CC_StorageCheck(self, operator, storage);
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tImd"));
        }
        else
        if(String_Equals(operator, "//="))
        {
            CC_StorageCheck(self, operator, storage);
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tIdv"));
        }
        else
        if(String_Equals(operator, "/="))
        {
            CC_StorageCheck(self, operator, storage);
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tDiv"));
        }
        else
        if(String_Equals(operator, "%="))
        {
            CC_StorageCheck(self, operator, storage);
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tMod"));
        }
        else
        if(String_Equals(operator, "**="))
        {
            CC_StorageCheck(self, operator, storage);
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tPow"));
        }
        else
        {
            storage = false;
            if(String_Equals(operator, "?"))
            {
                CC_Factor(self);
                CC_AssemB(self, String_Init("\tMem"));
            }
            else
            {
                CC_AssemB(self, String_Init("\tCop"));
                CC_Factor(self);
                if(String_Equals(operator, "*"))
                    CC_AssemB(self, String_Init("\tMul"));
                else
                if(String_Equals(operator, "/"))
                    CC_AssemB(self, String_Init("\tDiv"));
                else
                if(String_Equals(operator, "//"))
                    CC_AssemB(self, String_Init("\tIdv"));
                else
                if(String_Equals(operator, "%"))
                    CC_AssemB(self, String_Init("\tMod"));
                else
                if(String_Equals(operator, "%%"))
                    CC_AssemB(self, String_Init("\tImd"));
                else
                if(String_Equals(operator, "||"))
                    CC_AssemB(self, String_Init("\tLor"));
                else
                if(String_Equals(operator, "**"))
                    CC_AssemB(self, String_Init("\tPow"));
                else
                    CC_Quit(self, "operator %s not supported", operator->value);
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
            CC_StorageCheck(self, operator, storage);
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tMov"));
        }
        else
        if(String_Equals(operator, "+="))
        {
            CC_StorageCheck(self, operator, storage);
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tAdd"));
        }
        else
        if(String_Equals(operator, "-="))
        {
            CC_StorageCheck(self, operator, storage);
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tSub"));
        }
        else
        if(String_Equals(operator, "=="))
        {
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tEql"));
        }
        else
        if(String_Equals(operator, "!="))
        {
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tNeq"));
        }
        else
        if(String_Equals(operator, ">"))
        {
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tGrt"));
        }
        else
        if(String_Equals(operator, "<"))
        {
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tLst"));
        }
        else
        if(String_Equals(operator, ">="))
        {
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tGte"));
        }
        else
        if(String_Equals(operator, "<="))
        {
            CC_Expression(self);
            CC_AssemB(self, String_Init("\tLte"));
        }
        else
        if(String_Equals(operator, "->"))
        {
            String* ident = CC_Ident(self);
            CC_AssemB(self, String_Init("\tDrf"));
            CC_AssemB(self, String_Format("\tPsh \"%s\"", ident->value));
            CC_AssemB(self, String_Init("\tGet"));
            String_Kill(ident);
        }
        else
        {
            storage = false;
            CC_AssemB(self, String_Init("\tCop"));
            CC_Term(self);
            if(String_Equals(operator, "+"))
                CC_AssemB(self, String_Init("\tAdd"));
            else
            if(String_Equals(operator, "-"))
                CC_AssemB(self, String_Init("\tSub"));
            else
            if(String_Equals(operator, "&&"))
                CC_AssemB(self, String_Init("\tAnd"));
            else
                CC_Quit(self, "operator %s not supported", operator->value);
        }
        String_Kill(operator);
    }
}

static int64_t
CC_Label(CC* self)
{
    int64_t label = self->labels;
    self->labels += 1;
    return label;
}

static void
CC_Block(CC*, int64_t head, int64_t tail, int64_t scoping, bool loop);

static void
CC_Branch(CC* self, int64_t head, int64_t tail, int64_t end, int64_t scoping, bool loop)
{
    int64_t next = CC_Label(self);
    CC_Match(self, "(");
    CC_Expression(self);
    CC_Match(self, ")");
    CC_AssemB(self, String_Format("\tBrf @l%ld", next));
    CC_Block(self, head, tail, scoping, loop);
    CC_AssemB(self, String_Format("\tJmp @l%ld", end));
    CC_AssemB(self, String_Format("@l%ld:", next));
}

static String*
CC_Branches(CC* self, int64_t head, int64_t tail, int64_t scoping, bool loop)
{
    int64_t end = CC_Label(self);
    CC_Branch(self, head, tail, end, scoping, loop);
    String* buffer = CC_Ident(self);
    while(String_Equals(buffer, "elif"))
    {
        CC_Branch(self, head, tail, end, scoping, loop);
        String_Kill(buffer);
        buffer = CC_Ident(self);
    }
    if(String_Equals(buffer, "else"))
        CC_Block(self, head, tail, scoping, loop);
    CC_AssemB(self, String_Format("@l%ld:", end));
    String* backup = NULL;
    if(buffer->size == 0 || String_Equals(buffer, "elif") || String_Equals(buffer, "else"))
        String_Kill(buffer);
    else
        backup = buffer;
    return backup;
}

static void
CC_While(CC* self, int64_t scoping)
{
    int64_t A = CC_Label(self);
    int64_t B = CC_Label(self);
    CC_AssemB(self, String_Format("@l%ld:", A));
    CC_Match(self, "(");
    CC_Expression(self);
    CC_AssemB(self, String_Format("\tBrf @l%ld", B));
    CC_Match(self, ")");
    CC_Block(self, A, B, scoping, true);
    CC_AssemB(self, String_Format("\tJmp @l%ld", A));
    CC_AssemB(self, String_Format("@l%ld:", B));
}

static void
CC_Foreach(CC* self, int64_t scoping)
{
    int64_t A = CC_Label(self);
    int64_t B = CC_Label(self);
    int64_t C = CC_Label(self);
    Queue* init = Queue_Init((Kill) String_Kill, (Copy) NULL);
    CC_Match(self, "(");
    String* item = CC_Ident(self);
    CC_Match(self, ":");
    CC_Expression(self);
    CC_Match(self, ")");
    String* queue = String_Format("!queue_%s", item->value);
    CC_Local(self, queue);
    Queue_PshB(init, String_Copy(queue));
    CC_AssemB(self, String_Init("\tPsh 0"));
    String* index = String_Format("!index_%s", item->value);
    CC_Local(self, index);
    Queue_PshB(init, String_Copy(index));
    // NULL VALUE SERVES AS TEMPORARY STORAGE HOLDER FOR ITERATOR REFERENCE TYPE.
    CC_AssemB(self, String_Init("\tPsh null"));
    CC_Local(self, item);
    Queue_PshB(init, String_Copy(item));
    CC_AssemB(self, String_Format("@l%ld:", A));
    CC_Ref(self, queue);
    CC_AssemB(self, String_Init("\tLen"));
    CC_Ref(self, index);
    CC_AssemB(self, String_Init("\tEql"));
    CC_AssemB(self, String_Init("\tNot"));
    CC_AssemB(self, String_Format("\tBrf @l%ld", B));
    // THIS POP REMOVES THE UPDATED NULL VALUE SUCH THAT IT MAY BE PUSHED AGAIN NEXT ITERATION.
    CC_AssemB(self, String_Init("\tPop 1"));
    CC_Ref(self, queue);
    CC_Ref(self, index);
    CC_AssemB(self, String_Init("\tGet"));
    CC_Block(self, C, B, scoping, true);
    CC_AssemB(self, String_Format("@l%ld:", C));
    CC_Ref(self, index);
    CC_AssemB(self, String_Init("\tPsh 1"));
    CC_AssemB(self, String_Init("\tAdd"));
    CC_AssemB(self, String_Init("\tPop 1"));
    CC_AssemB(self, String_Format("\tJmp @l%ld", A));
    CC_AssemB(self, String_Format("@l%ld:", B));
    CC_PopScope(self, init);
}

static void
CC_For(CC* self, int64_t scoping)
{
    int64_t A = CC_Label(self);
    int64_t B = CC_Label(self);
    int64_t C = CC_Label(self);
    int64_t D = CC_Label(self);
    Queue* init = Queue_Init((Kill) String_Kill, (Copy) NULL);
    CC_Match(self, "(");
    String* ident = CC_Ident(self);
    Queue_PshB(init, String_Copy(ident));
    CC_AssignLocal(self, ident);
    CC_AssemB(self, String_Format("@l%ld:", A));
    CC_Expression(self);
    CC_Match(self, ";");
    CC_AssemB(self, String_Format("\tBrf @l%ld", D));
    CC_AssemB(self, String_Format("\tJmp @l%ld", C));
    CC_AssemB(self, String_Format("@l%ld:", B));
    CC_ConsumeExpression(self);
    CC_Match(self, ")");
    CC_AssemB(self, String_Format("\tJmp @l%ld", A));
    CC_AssemB(self, String_Format("@l%ld:", C));
    CC_Block(self, B, D, scoping, true);
    CC_AssemB(self, String_Format("\tJmp @l%ld", B));
    CC_AssemB(self, String_Format("@l%ld:", D));
    CC_PopScope(self, init);
}

static void
CC_Ret(CC* self)
{
    if(CC_Next(self) == ';')
        CC_AssemB(self, String_Init("\tPsh null"));
    else
        CC_Expression(self);
    CC_AssemB(self, String_Init("\tSav"));
    CC_AssemB(self, String_Init("\tFls"));
    CC_Match(self, ";");
}

static void
CC_Block(CC* self, int64_t head, int64_t tail, int64_t scoping, bool loop)
{
    Queue* scope = Queue_Init((Kill) String_Kill, (Copy) NULL);
    CC_Match(self, "{");
    String* prime = NULL; 
    while(CC_Next(self) != '}')
    {
        if(CC_String_IsIdentLeader(CC_Next(self)) || prime)
        {
            String* ident = NULL;
            if(prime)
                String_Swap(&prime, &ident);
            else
                ident = CC_Ident(self);
            if(String_Equals(ident, "if"))
                prime = CC_Branches(self, head, tail, scoping + scope->size, loop);
            else
            if(String_Equals(ident, "elif"))
                CC_Quit(self, "keyword elif must follow an if or elif block");
            else
            if(String_Equals(ident, "else"))
                CC_Quit(self, "keyword else must follow an if or elif block");
            else
            if(String_Equals(ident, "while"))
                CC_While(self, 0);
            else
            if(String_Equals(ident, "foreach"))
                CC_Foreach(self, 0);
            else
            if(String_Equals(ident, "for"))
                CC_For(self, 0);
            else
            if(String_Equals(ident, "ret"))
                CC_Ret(self);
            else
            if(String_Equals(ident, "continue"))
            {
                if(loop)
                {
                    CC_Match(self, ";");
                    CC_Pops(self, scoping + scope->size);
                    CC_AssemB(self, String_Format("\tJmp @l%ld", head));
                }
                else
                    CC_Quit(self, "the keyword continue can only be used within a while, for, or foreach loop");
            }
            else
            if(String_Equals(ident, "break"))
            {
                if(loop)
                {
                    CC_Match(self, ";");
                    CC_Pops(self, scoping + scope->size);
                    CC_AssemB(self, String_Format("\tJmp @l%ld", tail));
                }
                else
                    CC_Quit(self, "the keyword break can only be used within a while, for, or foreach loop");
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
                CC_ConsumeExpression(self);
                CC_Match(self, ";");
            }
            String_Kill(ident);
        }
        else
        {
            CC_ConsumeExpression(self);
            CC_Match(self, ";");
        }
    }
    CC_Match(self, "}");
    CC_PopScope(self, scope);
}

static void
CC_Prototype(CC* self, Queue* params, String* ident)
{
    CC_Define(self, CLASS_FUNCTION_PROTOTYPE, params->size, ident, String_Copy(CC_CurrentFile(self)));
    Queue_Kill(params);
    CC_Match(self, ";");
}

static void
CC_Function(CC* self, String* ident)
{
    Queue* params = CC_ParamRoll(self);
    if(CC_Next(self) == '{')
    {
        CC_DefineParams(self, params);
        CC_Define(self, CLASS_FUNCTION, params->size, ident, String_Copy(CC_CurrentFile(self)));
        CC_AssemB(self, String_Format("%s:", ident->value));
        CC_Block(self, 0, 0, 0, false);
        CC_PopScope(self, params);
        CC_AssemB(self, String_Init("\tPsh null"));
        CC_AssemB(self, String_Init("\tSav"));
        CC_AssemB(self, String_Init("\tRet"));
    }
    else
        CC_Prototype(self, params, ident);
}

static void
CC_Spool(CC* self, Queue* start)
{
    Queue* spool = Queue_Init((Kill) String_Kill, NULL);
    String* main = String_Init("Main");
    CC_Expect(self, main, CC_IsFunction);
    Queue_PshB(spool, String_Init("!start:"));
    for(int64_t i = 0; i < start->size; i++)
    {
        String* label = Queue_Get(start, i);
        Queue_PshB(spool, String_Format("\tCal %s", label->value));
    }
    Queue_PshB(spool, String_Format("\tCal %s", main->value));
    Queue_PshB(spool, String_Init("\tEnd"));
    int64_t i = spool->size;
    while(i != 0)
    {
        i -= 1;
        CC_AssemF(self, String_Copy(Queue_Get(spool, i)));
    }
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
        if(CC_Next(self) == '(')
            CC_Function(self, ident);
        else
        if(CC_Next(self) == ':')
        {
            String* label = CC_Global(self, ident);
            Queue_PshB(start, label);
        }
        else
            CC_Quit(self, "%s must either be a function or function prototype, a global variable, or an include statement", ident->value);
        CC_Spin(self);
    }
    CC_Spool(self, start);
    Queue_Kill(start);
}

static Stack*
Stack_Init(String* label, int64_t address)
{
    Stack* self = Malloc(sizeof(*self));
    self->label = label;
    self->address = address;
    return self;
}

static void
Stack_Kill(Stack* self)
{
    String_Kill(self->label);
    Free(self);
}

static bool
Stack_Compare(Stack* a, Stack* b)
{
    return a->address < b->address;
}

static Queue*
ASM_Flatten(Map* labels)
{
    Queue* addresses = Queue_Init((Kill) Stack_Kill, (Copy) NULL);
    for(int64_t i = 0; i < Map_Buckets(labels); i++)
    {
        Node* chain = labels->bucket[i];
        while(chain)
        {
            int64_t* address = Map_Get(labels, chain->key->value);
            Stack* stack = Stack_Init(String_Init(chain->key->value), *address);
            Queue_PshB(addresses, stack);
            chain = chain->next;
        }
    }
    Queue_Sort(addresses, (Compare) Stack_Compare);
    return addresses;
}

static Map*
ASM_Label(Queue* assembly, int64_t* size)
{
    Map* labels = Map_Init((Kill) Free, (Copy) NULL);
    for(int64_t i = 0; i < assembly->size; i++)
    {
        String* stub = Queue_Get(assembly, i);
        if(stub->value[0] == '\t')
            *size += 1;
        else 
        {
            String* line = String_Copy(stub);
            char* label = strtok(line->value, ":");
            if(Map_Exists(labels, label))
                Quit("assembler label %s already defined", label);
            Map_Set(labels, String_Init(label), Int_Init(*size));
            String_Kill(line);
        }
    }
    return labels;
}

static void
ASM_Dump(Queue* assembly)
{
    int64_t instructions = 0;
    int64_t labels = 0;
    for(int64_t i = 0; i < assembly->size; i++)
    {
        String* assem = Queue_Get(assembly, i);
        if(assem->value[0] == '\t')
            instructions += 1;
        else
            labels += 1;
        puts(assem->value);
    }
    fprintf(stdout, "instructions %ld : labels %ld\n", instructions, labels);
}

static int64_t
Stack_Diff(void* a, void* b)
{
    Stack* aa = a;
    Stack* bb = b;
    return aa->address - bb->address;
}

static void
VM_Quit(VM* self, const char* const message, ...)
{
    va_list args;
    va_start(args, message);
    if(self->frame->size != 0)
    {
        for(int64_t i = 0; i < self->frame->size - 1; i++)
        {
            Frame* a = Queue_Get(self->frame, i + 0);
            Frame* b = Queue_Get(self->frame, i + 1);
            Stack key = { NULL, a->address };
            Stack* found = Queue_BSearch(self->addresses, &key, Stack_Diff);
            fprintf(stderr, "%s(...): ", found->label->value);
            Debug* sub = Queue_Get(self->debug, b->pc);
            fprintf(stderr, "%s: line %ld\n", sub->file, sub->line);
        }
        Debug* debug = Queue_Get(self->debug, self->pc);
        fprintf(stderr, "error: file %s: line %ld: ", debug->file, debug->line);
        vfprintf(stderr, message, args);
        fprintf(stderr, "\n");
    }
    else
    {
        fprintf(stderr, "error: Main return type ");
        vfprintf(stderr, message, args);
        fprintf(stderr, "\n");
    }
    va_end(args);
    exit(0xFF);
}

static VM*
VM_Init(int64_t size, Queue* debug, Queue* addresses)
{
    VM* self = Malloc(sizeof(*self));
    self->data = Queue_Init((Kill) Value_Kill, (Copy) NULL);
    self->data_dups = Map_Init((Kill) Int_Kill, (Copy) NULL);
    self->stack = Queue_Init((Kill) Value_Kill, (Copy) NULL);
    self->frame = Queue_Init((Kill) Frame_Free, (Copy) NULL);
    self->track = Map_Init((Kill) Int_Kill, (Copy) NULL);
    self->addresses = addresses;
    self->debug = debug;
    self->ret = NULL;
    self->size = size;
    self->instructions = Malloc(size * sizeof(*self->instructions));
    self->pc = 0;
    self->spds = 0;
    self->retno = 0;
    self->done = false;
    return self;
}

static void
VM_Kill(VM* self)
{
    Queue_Kill(self->data);
    Queue_Kill(self->stack);
    Queue_Kill(self->frame);
    Queue_Kill(self->addresses);
    Map_Kill(self->track);
    Map_Kill(self->data_dups);
    Free(self->instructions);
    Free(self);
}

static void
VM_Data(VM* self)
{
    fprintf(stdout, ".data:\n");
    for(int64_t i = 0; i < self->data->size; i++)
    {
        Value* value = Queue_Get(self->data, i);
        fprintf(stdout, "%2lu : %2lu : ", i, value->refs);
        Value_Print(value, 0, 6);
    }
}

static void
VM_AssertRefs(VM* self)
{
    for(int64_t i = 0; i < self->data->size; i++)
    {
        Value* value = Queue_Get(self->data, i);
        if(value->refs > 0)
        {
            String* print = Value_Sprint(value, true, 0, -1, -1);
            VM_Quit(self, "the .data segment value %s contained %ld references at the time of exit", print->value, value->refs);
        }
    }
}

static void
VM_Pop(VM* self, int64_t count)
{
    while(count--)
        Queue_PopB(self->stack);
}

static String*
VM_ConvertEscs(VM* self, char* chars)
{
    int64_t len = strlen(chars);
    String* string = String_Init("");
    for(int64_t i = 1; i < len - 1; i++)
    {
        char ch = chars[i];
        if(ch == '\\')
        {
            i += 1;
            int64_t esc = chars[i];
            ch = CC_String_EscToByte(esc);
            if(ch == -1)
                VM_Quit(self, "an unknown escape character 0x%02X was encountered\n", esc);
        }
        String_PshB(string, ch);
    }
    return string;
}

static int64_t
VM_Store(VM* self, Map* labels, char* operand)
{
    String* key = String_Init(operand);
    int64_t* index = Map_Get(self->data_dups, key->value);
    if(index)
    {
        String_Kill(key);
        return *index;
    }
    else
    {
        Value* value = NULL;
        char ch = operand[0];
        if(ch == '[')
            value = Value_Queue();
        else
        if(ch == '{')
            value = Value_Map();
        else
        if(ch == '"')
        {
            String* string = VM_ConvertEscs(self, operand);
            value = Value_String(string);
        }
        else
        if(ch == '@')
        {
            String* name = String_Init(strtok(operand + 1, ","));
            int64_t size = String_ToUll(strtok(NULL, " \n"));
            int64_t* address = Map_Get(labels, name->value);
            if(address == NULL)
                Quit("assembler label %s not defined", name);
            value = Value_Function(Function_Init(name, size, *address));
        }
        else
        if(ch == 't' || ch == 'f')
            value = Value_Bool(Equals(operand, "true") ? true : false);
        else
        if(ch == 'n')
            value = Value_Null();
        else
        if(CC_String_IsDigit(ch) || ch == '-')
            value = Value_Number(String_ToNumber(operand));
        else
            Quit("assembler unknown psh operand %s encountered", operand);
        Map_Set(self->data_dups, key, Int_Init(self->data->size));
        Queue_PshB(self->data, value);
        return self->data->size - 1;
    }
}

static int64_t
VM_Datum(VM* self, Map* labels, char* operand)
{
    int64_t index = VM_Store(self, labels, operand);
    return (index << 8) | OPCODE_Psh;
}

static int64_t
VM_Indirect(Opcode oc, Map* labels, char* label)
{
    int64_t* address = Map_Get(labels, label);
    if(address == NULL)
        Quit("assembler label %s not defined", label);
    return *address << 8 | oc;
}

static int64_t
VM_Direct(Opcode oc, char* number)
{
    return (String_ToUll(number) << 8) | oc;
}

static int64_t
VM_Redirect(VM* self, Map* labels, Opcode opcode)
{
    switch(opcode)
    {
    case OPCODE_Psh:
        return VM_Datum(self, labels, strtok(NULL, "\n"));
    case OPCODE_Brf:
    case OPCODE_Cal:
    case OPCODE_Jmp:
        return VM_Indirect(opcode, labels, strtok(NULL, "\n"));
    case OPCODE_Glb:
    case OPCODE_Loc:
    case OPCODE_Pop:
        return VM_Direct(opcode, strtok(NULL, "\n"));
    default:
        break;
    }
    return opcode;
}

static void
VM_Cal(VM* self, int64_t address)
{
    int64_t sp = self->stack->size - self->spds;
    Frame* frame = Frame_Init(self->pc, sp, address);
    Queue_PshB(self->frame, frame);
    self->pc = address;
    self->spds = 0;
}

static void
VM_Cop(VM* self, int64_t unused)
{
    (void) unused;
    Value* back = Queue_Back(self->stack);
    Value* copy = Value_Copy(back);
    VM_Pop(self, 1);
    Queue_PshB(self->stack, copy);
}

static void
VM_Ptr(VM* self, int64_t unused)
{
    (void) unused;
    Value* back = Queue_Back(self->stack);
    if(back->type != TYPE_FUNCTION)
    {
        Value* pointer = Value_Pointer(Pointer_Init(back));
        VM_Pop(self, 1);
        Queue_PshB(self->stack, pointer);
    }
}

static void
VM_TypeExpect(VM* self, Type a, Type b)
{
    if(a != b)
        VM_Quit(self, "encountered type %s but expected type %s", Type_ToString(a), Type_ToString(b));
}

static void
VM_TypeAllign(VM* self, Type a, Type b, char* operator)
{
    if(a != b)
        VM_Quit(self, "type mismatch with type %s and type %s with operator %s", Type_ToString(a), Type_ToString(b), operator);
}

static void
VM_End(VM* self, int64_t unused)
{
    (void) unused;
    VM_TypeExpect(self, self->ret->type, TYPE_NUMBER);
    self->retno = self->ret->of.number;
    self->done = true;
    Value_Kill(self->ret);
}

static void
VM_Fls(VM* self, int64_t unused)
{
    (void) unused;
    Frame* frame = Queue_Back(self->frame);
    int64_t pops = self->stack->size - frame->sp;
    VM_Pop(self, pops);
    self->pc = frame->pc;
    Queue_PopB(self->frame);
}

static void
VM_Glb(VM* self, int64_t address)
{
    Value* value = Queue_Get(self->stack, address);
    Value_Inc(value);
    Queue_PshB(self->stack, value);
}

static void
VM_Loc(VM* self, int64_t address)
{
    Frame* frame = Queue_Back(self->frame);
    Value* value = Queue_Get(self->stack, address + frame->sp);
    Value_Inc(value);
    Queue_PshB(self->stack, value);
}

static void
VM_Jmp(VM* self, int64_t address)
{
    self->pc = address;
}

static void
VM_Ret(VM* self, int64_t unused)
{
    (void) unused;
    Frame* frame = Queue_Back(self->frame);
    self->pc = frame->pc;
    Queue_PopB(self->frame);
}

static void
VM_Sav(VM* self, int64_t unused)
{
    (void) unused;
    Value* value = Queue_Back(self->stack);
    Value_Inc(value);
    self->ret = value;
    VM_Pop(self, 1);
}

static void
VM_Lod(VM* self, int64_t unused)
{
    (void) unused;
    // OPCODE (SAV) INCREMENTED REF COUNT - REFERENCE COUNTER DECREMENT
    // SINCE TRANSITION FROM RETURN REGISTER TO STACK IS DIRECT.
    Queue_PshB(self->stack, self->ret);
}

static void
VM_Trv(VM* self, int64_t unused)
{
    (void) unused;
    VM_Pop(self, 1);
    VM_Lod(self, unused);
}

static void
VM_Psh(VM* self, int64_t address)
{
    Value* value = Queue_Get(self->data, address);
    Queue_PshB(self->stack, Value_Copy(value));
}

static void
VM_Mov(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    if(a->type == TYPE_CHAR && b->type == TYPE_STRING)
        Value_Sub(a, b);
    else
    {
        if(a->type == TYPE_NULL)
            VM_Quit(self, "values cannot be moved into null storage. Value type was %s", Type_ToString(b->type));
        if(a != b)
        {
            Type_Kill(a->type, &a->of);
            Type_Copy(a, b);
        }
    }
    VM_Pop(self, 1);
}

static void
VM_Operate(VM* self, double (*exec)(double, double), char* operator)
{
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_NUMBER);
    VM_TypeAllign(self, a->type, b->type, operator);
    a->of.number = exec(a->of.number, b->of.number);
    VM_Pop(self, 1);
}

static void
VM_Math(VM* self, double (*math)(double))
{
    Value* a = Queue_Back(self->stack);
    VM_TypeExpect(self, a->type, TYPE_NUMBER);
    double value = math(a->of.number);
    VM_Pop(self, 1);
    Queue_PshB(self->stack, Value_Number(value));
}

static void
VM_Associate(VM* self, bool (*exec)(Value*, Value*))
{
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    bool boolean = exec(a, b);
    VM_Pop(self, 2);
    Queue_PshB(self->stack, Value_Bool(boolean));
}

static bool Value_And(Value* a, Value* b) { return (a->type == TYPE_BOOL && b->type == TYPE_BOOL) ? (a->of.boolean && b->of.boolean) : false; }
static bool Value_Lor(Value* a, Value* b) { return (a->type == TYPE_BOOL && b->type == TYPE_BOOL) ? (a->of.boolean || b->of.boolean) : false; }
static double Number_Mul(double x, double y) { return x * y; }
static double Number_Div(double x, double y) { return x / y; }
static double Number_Pow(double x, double y) { return pow(x, y); }
static double Number_Idv(double x, double y) { return (int64_t) x / (int64_t) y; }
static double Number_Imd(double x, double y) { return (int64_t) x % (int64_t) y; }
static void VM_Pow(VM* self, int64_t unused) { (void) unused; VM_Operate(self, Number_Pow, "**"); }
static void VM_Mul(VM* self, int64_t unused) { (void) unused; VM_Operate(self, Number_Mul, "*"); }
static void VM_Div(VM* self, int64_t unused) { (void) unused; VM_Operate(self, Number_Div, "/"); }
static void VM_Idv(VM* self, int64_t unused) { (void) unused; VM_Operate(self, Number_Idv, "//"); }
static void VM_Imd(VM* self, int64_t unused) { (void) unused; VM_Operate(self, Number_Imd, "%%"); }
static void VM_Abs(VM* self, int64_t unused) { (void) unused; VM_Math(self, fabs); }
static void VM_Tan(VM* self, int64_t unused) { (void) unused; VM_Math(self, tan); }
static void VM_Sin(VM* self, int64_t unused) { (void) unused; VM_Math(self, sin); }
static void VM_Cos(VM* self, int64_t unused) { (void) unused; VM_Math(self, cos); }
static void VM_Ata(VM* self, int64_t unused) { (void) unused; VM_Math(self, atan); }
static void VM_Asi(VM* self, int64_t unused) { (void) unused; VM_Math(self, asin); }
static void VM_Aco(VM* self, int64_t unused) { (void) unused; VM_Math(self, acos); }
static void VM_Log(VM* self, int64_t unused) { (void) unused; VM_Math(self, log); }
static void VM_Sqr(VM* self, int64_t unused) { (void) unused; VM_Math(self, sqrt); }
static void VM_Cel(VM* self, int64_t unused) { (void) unused; VM_Math(self, ceil); }
static void VM_Flr(VM* self, int64_t unused) { (void) unused; VM_Math(self, floor); }
static void VM_Lst(VM* self, int64_t unused) { (void) unused; VM_Associate(self, Value_LessThan); }
static void VM_Lte(VM* self, int64_t unused) { (void) unused; VM_Associate(self, Value_LessThanEqualTo); }
static void VM_Grt(VM* self, int64_t unused) { (void) unused; VM_Associate(self, Value_GreaterThan); }
static void VM_Gte(VM* self, int64_t unused) { (void) unused; VM_Associate(self, Value_GreaterThanEqualTo); }
static void VM_Eql(VM* self, int64_t unused) { (void) unused; VM_Associate(self, Value_Equal); }
static void VM_Neq(VM* self, int64_t unused) { (void) unused; VM_Associate(self, Value_NotEqual); }
static void VM_And(VM* self, int64_t unused) { (void) unused; VM_Associate(self, Value_And); }
static void VM_Lor(VM* self, int64_t unused) { (void) unused; VM_Associate(self, Value_Lor); }

static void
VM_Psb(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_QUEUE);
    if(b->type == TYPE_NULL)
        VM_Quit(self, "nulls cannot be appended to queues");
    Queue_PshB(a->of.queue, Value_Copy(b));
    VM_Pop(self, 1);
}

static void
VM_Psf(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_QUEUE);
    if(b->type == TYPE_NULL)
        VM_Quit(self, "nulls cannot be prepended to queues");
    Queue_PshF(a->of.queue, Value_Copy(b));
    VM_Pop(self, 1);
}

static void
VM_Add(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    if(a->type == TYPE_QUEUE && b->type != TYPE_QUEUE)
        VM_Psb(self, unused);
    else
    {
        if(a->type == TYPE_STRING && b->type == TYPE_CHAR)
            String_PshB(a->of.string, *b->of.character->value);
        else
        if(a->type == b->type)
            switch(a->type)
            {
            case TYPE_QUEUE:
                if(a == b)
                {
                    Queue* copy = Queue_Copy(b->of.queue);
                    Queue_Append(a->of.queue, copy);
                    Queue_Kill(copy);
                }
                else
                    Queue_Append(a->of.queue, b->of.queue);
                break;
            case TYPE_MAP:
                if(a == b)
                {
                    Map* copy = Map_Copy(b->of.map);
                    Map_Append(a->of.map, copy);
                    Map_Kill(copy);
                }
                else
                    Map_Append(a->of.map, b->of.map);
                break;
            case TYPE_STRING:
                if(a == b)
                {
                    String* copy = String_Copy(b->of.string);
                    String_Append(a->of.string, copy);
                }
                else
                    String_Appends(a->of.string, b->of.string->value);
                break;
            case TYPE_NUMBER:
                a->of.number += b->of.number;
                break;
            case TYPE_FUNCTION:
            case TYPE_POINTER:
            case TYPE_CHAR:
            case TYPE_BOOL:
            case TYPE_NULL:
            case TYPE_FILE:
                VM_Quit(self, "type %s not supported with operator +", Type_ToString(a->type));
            }
        else
            VM_TypeAllign(self, a->type, b->type, "+");
        VM_Pop(self, 1);
    }
}

static void
VM_Sub(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    if(a->type == TYPE_QUEUE && b->type != TYPE_QUEUE)
        VM_Psf(self, unused);
    else
    {
        if(a->type == b->type)
            switch(a->type)
            {
            case TYPE_QUEUE:
                if(a == b)
                {
                    Queue* copy = Queue_Copy(b->of.queue);
                    Queue_Prepend(a->of.queue, copy);
                    Queue_Kill(copy);
                }
                else
                    Queue_Prepend(a->of.queue, b->of.queue);
                break;
            case TYPE_NUMBER:
                a->of.number -= b->of.number;
                break;
            case TYPE_STRING:
            {
                double diff = strcmp(a->of.string->value, b->of.string->value);
                Type_Kill(a->type, &a->of);
                a->type = TYPE_NUMBER;
                a->of.number = diff;
                break;
            }
            case TYPE_POINTER:
            case TYPE_FUNCTION:
            case TYPE_MAP:
            case TYPE_CHAR:
            case TYPE_BOOL:
            case TYPE_NULL:
            case TYPE_FILE:
                VM_Quit(self, "type %s not supported with operator -", Type_ToString(a->type));
            }
        else
            VM_TypeAllign(self, a->type, b->type, "-");
        VM_Pop(self, 1);
    }
}

static void
VM_Vrt(VM* self, int64_t unused)
{
    (void) unused;
    Value* size = Queue_Back(self->stack);
    VM_TypeExpect(self, size->type, TYPE_NUMBER);
    int64_t ofsize = size->of.number;
    VM_Pop(self, 1);
    Value* function = Queue_Get(self->stack, self->stack->size - ofsize - 1);
    int64_t ofargs = function->of.function->size;
    if(ofsize != ofargs)
    {  
        char* name = function->of.function->name->value;
        VM_Quit(self, "expected %ld arguments for indirect function call %s but encountered %ld arguments", ofsize, name, ofargs);
    }
    VM_TypeExpect(self, function->type, TYPE_FUNCTION);
    int64_t sp = self->stack->size - ofsize;
    Queue_PshB(self->frame, Frame_Init(self->pc, sp, function->of.function->address));
    self->pc = function->of.function->address;
}

static void
VM_Run(VM*, bool arbitrary);

static Value*
VM_BSearch(VM* self, Queue* queue, Value* key, Value* comparator)
{
    int64_t low = 0;
    int64_t high = queue->size - 1;
    while(low <= high)
    {
        int64_t mid = (low + high) / 2;
        Value* now = Queue_Get(queue, mid);
        Value_Inc(comparator);
        Value_Inc(key);
        Value_Inc(now);
        // OPCODE (VRT) EXPECTS CALLBACK AS THE FIRST ARGUMENT.
        Queue_PshB(self->stack, comparator);
        Queue_PshB(self->stack, key);
        Queue_PshB(self->stack, now);
        Queue_PshB(self->stack, Value_Number(2));
        VM_Vrt(self, 0);
        VM_Run(self, true);
        // POPS COMPARATOR - THIS IS A VIRTUAL (VRT) CALL - SEE OPCODE (TRV).
        VM_Pop(self, 1);
        VM_TypeExpect(self, self->ret->type, TYPE_NUMBER);
        int64_t cmp = self->ret->of.number;
        Value_Kill(self->ret);
        if(cmp == 0)
            return now;
        else
        if(cmp < 0)
            high = mid - 1;
        else
            low = mid + 1;
    }
    return NULL;
}

static void
VM_Bsr(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 3);
    Value* b = Queue_Get(self->stack, self->stack->size - 2);
    Value* c = Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_QUEUE);
    VM_TypeExpect(self, c->type, TYPE_FUNCTION);
    Value* found = VM_BSearch(self, a->of.queue, b, c);
    VM_Pop(self, 3);
    if(found == NULL)
        Queue_PshB(self->stack, Value_Null());
    else
    {
        Value_Inc(found);
        Queue_PshB(self->stack, found);
    }
}

static void
VM_RangedSort(VM* self, Queue* queue, Value* comparator, int64_t left, int64_t right)
{
    if(left >= right)
        return;
    Queue_Swap(queue, left, (left + right) / 2);
    int64_t last = left;
    for(int64_t i = left + 1; i <= right; i++)
    {
        Value* a = Queue_Get(queue, i);
        Value* b = Queue_Get(queue, left);
        Value_Inc(comparator);
        Value_Inc(a);
        Value_Inc(b);
        // OPCODE (VRT) EXPECTS CALLBACK AS THE FIRST ARGUMENT.
        Queue_PshB(self->stack, comparator);
        Queue_PshB(self->stack, a);
        Queue_PshB(self->stack, b);
        Queue_PshB(self->stack, Value_Number(2));
        VM_Vrt(self, 0);
        VM_Run(self, true);
        // POPS comparator - THIS IS A VIRTUAL (VRT) CALL - SEE OPCODE (TRV).
        VM_Pop(self, 1);
        VM_TypeExpect(self, self->ret->type, TYPE_BOOL);
        if(self->ret->of.boolean)
             Queue_Swap(queue, ++last, i);
        Value_Kill(self->ret);
    }
   Queue_Swap(queue, left, last);
   VM_RangedSort(self, queue, comparator, left, last - 1);
   VM_RangedSort(self, queue, comparator, last + 1, right);
}

static void
VM_Sort(VM* self, Queue* queue, Value* comparator)
{
    VM_TypeExpect(self, comparator->type, TYPE_FUNCTION);
    if(comparator->of.function->size != 2)
        VM_Quit(self, "expected 2 arguments for Sort's comparator but encountered %ld arguments", comparator->of.function->size);
    VM_RangedSort(self, queue, comparator, 0, queue->size - 1);
}

static void
VM_Qso(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_QUEUE);
    VM_TypeExpect(self, b->type, TYPE_FUNCTION);
    VM_Sort(self, a->of.queue, b);
    VM_Pop(self, 2);
    Queue_PshB(self->stack, Value_Null());
}

static void
VM_All(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Back(self->stack);;
    VM_TypeExpect(self, a->type, TYPE_QUEUE);
    bool boolean = true;
    for(int64_t i = 0; i < a->of.queue->size; i++)
    {
        Value* value = Queue_Get(a->of.queue, i);
        if(value->type == TYPE_BOOL)
        {
            if(value->of.boolean == false)
            {
                boolean = false;
                break;
            }
        }
        else
        {
            boolean = false;
            break;
        }
    }
    VM_Pop(self, 1);
    Queue_PshB(self->stack, Value_Bool(boolean));
}

static void
VM_Any(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Back(self->stack);;
    VM_TypeExpect(self, a->type, TYPE_QUEUE);
    bool boolean = false;
    for(int64_t i = 0; i < a->of.queue->size; i++)
    {
        Value* value = Queue_Get(a->of.queue, i);
        if(value->type == TYPE_BOOL)
        {
            if(value->of.boolean == true)
            {
                boolean = true;
                break;
            }
        }
    }
    VM_Pop(self, 1);
    Queue_PshB(self->stack, Value_Bool(boolean));
}

static void
VM_Max(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    Value* copy = Value_Copy(Value_GreaterThan(a, b) ? a : b);
    VM_Pop(self, 2);
    Queue_PshB(self->stack, copy);
}

static void
VM_Min(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    Value* copy = Value_Copy(Value_LessThan(a, b) ? a : b);
    VM_Pop(self, 2);
    Queue_PshB(self->stack, copy);
}

static void
VM_Spd(VM* self, int64_t unused)
{
    (void) unused;
    self->spds += 1;
}

static void
VM_Not(VM* self, int64_t unused)
{
    (void) unused;
    Value* value = Queue_Back(self->stack);
    VM_TypeExpect(self, value->type, TYPE_BOOL);
    value->of.boolean = !value->of.boolean;
}

static void
VM_Brf(VM* self, int64_t address)
{
    Value* value = Queue_Back(self->stack);
    VM_TypeExpect(self, value->type, TYPE_BOOL);
    if(value->of.boolean == false)
        self->pc = address;
    VM_Pop(self, 1);
}

static void
VM_Prt(VM* self, int64_t unused)
{
    (void) unused;
    Value* value = Queue_Back(self->stack);
    String* print = Value_Sprint(value, false, 0, -1, -1);
    puts(print->value);
    VM_Pop(self, 1);
    Queue_PshB(self->stack, Value_Number(print->size));
    String_Kill(print);
}

static void
VM_Len(VM* self, int64_t unused)
{
    (void) unused;
    Value* value = Queue_Back(self->stack);
    int64_t len = Value_Len(value);
    VM_Pop(self, 1);
    Queue_PshB(self->stack, Value_Number(len));
}

static void
VM_Ins(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 3);
    Value* b = Queue_Get(self->stack, self->stack->size - 2);
    Value* c = Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_MAP);
    if(c->type == TYPE_NULL)
        VM_Quit(self, "nulls cannot be inserted into maps. See key %s", b->of.string->value);
    if(b->type == TYPE_CHAR)
        Value_PromoteChar(b);
    if(b->type == TYPE_STRING)
        Map_Set(a->of.map, String_Copy(b->of.string), Value_Copy(c));
    else
        VM_Quit(self, "type %s was attempted to be used as a map key - only strings may be used as keys", Type_ToString(b->type));
    VM_Pop(self, 2);
}

static void
VM_Ref(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Back(self->stack);
    int64_t refs = a->refs;
    VM_Pop(self, 1);
    Queue_PshB(self->stack, Value_Number(refs));
}

static Value*
VM_IndexQueue(VM* self, Value* queue, Value* index)
{
    if(queue->of.queue->size == 0)
        VM_Quit(self, "cannot index empty queue");
    int64_t ind = index->of.number;
    Value* value = Queue_Get(queue->of.queue, ind);
    if(value == NULL)
        VM_Quit(self, "queue element access out of bounds with index %ld", ind);
    Value_Inc(value);
    return value;
}

static Value*
VM_IndexString(VM* self, Value* string, Value* index)
{
    if(string->of.string->size == 0)
        VM_Quit(self, "cannot index empty string");
    int64_t ind = index->of.number;
    Char* character = Char_Init(string, ind);
    if(character == NULL)
        VM_Quit(self, "string character access out of bounds with index %ld", ind);
    Value* value = Value_Char(character);
    Value_Inc(string);
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
    else
    {
        VM_Quit(self, "type %s cannot be indexed with type %s", Type_ToString(storage->type), Type_ToString(index->type));
        return NULL;
    }
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
VM_Get(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    Value* value = NULL;
    if(b->type == TYPE_CHAR)
        Value_PromoteChar(b);
    if(b->type == TYPE_NUMBER)
        value = VM_Index(self, a, b);
    else
    if(b->type == TYPE_STRING)
        value = VM_Lookup(self, a, b);
    else
        VM_Quit(self, "type %s cannot be indexed", Type_ToString(b->type));
    VM_Pop(self, 2);
    if(value == NULL)
        Queue_PshB(self->stack, Value_Null());
    else
        Queue_PshB(self->stack, value);
}

static void
VM_Mod(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    if(a->type == TYPE_NUMBER && b->type == TYPE_NUMBER)
    {
        a->of.number = fmod(a->of.number, b->of.number);
        VM_Pop(self, 1);
    }
    else
    if(a->type == TYPE_STRING && b->type == TYPE_QUEUE)
    {
        String* formatted = String_Init("");
        int64_t index = 0;
        char* ref = a->of.string->value;
        for(char* c = ref; *c; c++)
        {
            if(*c == '{')
                if(index < b->of.queue->size)
                {
                    String* buffer = String_Init("");
                    c += 1;
                    while(*c != '}')
                    {
                        if(CC_String_IsSpace(*c))
                            VM_Quit(self, "spaces may not be inserted between { and } with formatted printing");
                        String_PshB(buffer, *c);
                        c += 1;
                    }
                    int64_t iw = -1;
                    int64_t ip = -1;
                    if(sscanf(buffer->value, "%ld.%ld", &iw, &ip) == 0)
                        sscanf(buffer->value, ".%ld", &ip);
                    Value* value = Queue_Get(b->of.queue, index);
                    String_Append(formatted, Value_Sprint(value, false, 0, iw, ip));
                    index += 1;
                    String_Kill(buffer);
                    continue;
                }
            String_PshB(formatted, *c);
        }
        VM_Pop(self, 2);
        Queue_PshB(self->stack, Value_String(formatted));
    }
    else
        VM_Quit(self, "type %s and type %s not supported with modulus % operator", Type_ToString(a->type), Type_ToString(b->type));
}

static void
VM_Typ(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Back(self->stack);
    Type type = a->type;
    VM_Pop(self, 1);
    Queue_PshB(self->stack, Value_String(String_Init(Type_ToString(type))));
}

static void
VM_Drf(VM* self, int64_t unused)
{
    (void) unused;
    Value* back = Queue_Back(self->stack);
    VM_TypeExpect(self, back->type, TYPE_POINTER);
    Value* deref = back->of.pointer->value;
    Value_Inc(deref);
    VM_Pop(self, 1);
    Queue_PshB(self->stack, deref);
}

static void
VM_Del(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    if(b->type == TYPE_NUMBER)
    {
        int64_t ind = b->of.number;
        if(a->type == TYPE_QUEUE)
        {
            bool success = Queue_Del(a->of.queue, ind);
            if(success == false)
                VM_Quit(self, "queue element deletion out of bounds with index %ld", ind);
        }
        else
        if(a->type == TYPE_STRING)
        {
            bool success = String_Del(a->of.string, ind);
            if(success == false)
                VM_Quit(self, "string character deletion out of bounds with index %ld", ind);
        }
        else
            VM_Quit(self, "type %s cannot be indexed for deletion", Type_ToString(a->type));
    }
    else
    if(b->type == TYPE_STRING)
    {
        if(a->type != TYPE_MAP)
            VM_Quit(self, "maps can only be index with string keys");
        Map_Del(a->of.map, b->of.string->value);
    }
    else
        VM_Quit(self, "type %s cannot be used as a deletion index", Type_ToString(b->type));
    VM_Pop(self, 2);
    Queue_PshB(self->stack, Value_Null());
}

static void
VM_Mem(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    bool boolean = a == b;
    VM_Pop(self, 2);
    Queue_PshB(self->stack, Value_Bool(boolean));
}

static void
VM_Opn(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_STRING);
    VM_TypeExpect(self, b->type, TYPE_STRING);
    File* file = File_Init(String_Copy(a->of.string), String_Copy(b->of.string));
    VM_Pop(self, 2);
    Queue_PshB(self->stack, Value_File(file));
}

static void
VM_Red(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_FILE);
    VM_TypeExpect(self, b->type, TYPE_NUMBER);
    String* buffer = String_Init("");
    if(a->of.file->file)
    {
        String_Resize(buffer, b->of.number);
        int64_t size = fread(buffer->value, sizeof(char), b->of.number, a->of.file->file);
        int64_t diff = b->of.number - size;
        while(diff)
        {
            String_PopB(buffer);
            diff -= 1;
        }
    }
    VM_Pop(self, 2);
    Queue_PshB(self->stack, Value_String(buffer));
}

static void
VM_Wrt(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 2);
    Value* b = Queue_Get(self->stack, self->stack->size - 1);
    VM_TypeExpect(self, a->type, TYPE_FILE);
    VM_TypeExpect(self, b->type, TYPE_STRING);
    int64_t bytes = 0;
    if(a->of.file->file)
        bytes = fwrite(b->of.string->value, sizeof(char), b->of.string->size, a->of.file->file);
    VM_Pop(self, 2);
    Queue_PshB(self->stack, Value_Number(bytes));
}

static int64_t
Queue_Index(Queue* self, int64_t from, void* value, Compare compare)
{
    for(int64_t x = from; x < self->size; x++)
        if(compare(Queue_Get(self, x), value))
            return x;
    return SIZE_MAX;
}

static void
VM_Slc(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Get(self->stack, self->stack->size - 3);
    Value* b = Queue_Get(self->stack, self->stack->size - 2);
    Value* c = Queue_Get(self->stack, self->stack->size - 1);
    Value* value;
    if(b->type == TYPE_NUMBER)
    {
        int64_t x = b->of.number;
        int64_t y = c->of.number;
        if(a->type == TYPE_STRING)
        {
            VM_TypeExpect(self, a->type, TYPE_STRING);
            VM_TypeExpect(self, b->type, TYPE_NUMBER);
            VM_TypeExpect(self, c->type, TYPE_NUMBER);
            int64_t len = a->of.string->size;
            if(x == -1) x = len - 1;
            if(y == -1) y = len - 1;
            if(x > y || x < 0)
                VM_Quit(self, "string slice [%ld : %ld] not possible", x, y);
            if(y > len)
                VM_Quit(self, "string slice [%ld : %ld] not possible - right bound larger than string of size %ld", x, y, len);
            int64_t size = y - x;
            String* sub = String_Init("");
            if(size > 0)
            {
                String_Resize(sub, size);
                strncpy(sub->value, a->of.string->value + x, size);
            }
            value = Value_String(sub);
        }
        else
        if(a->type == TYPE_QUEUE)
        {
            VM_TypeExpect(self, a->type, TYPE_QUEUE);
            VM_TypeExpect(self, b->type, TYPE_NUMBER);
            VM_TypeExpect(self, c->type, TYPE_NUMBER);
            int64_t len = a->of.queue->size;
            if(x == -1) x = len - 1;
            if(y == -1) y = len - 1;
            if(x > y || x < 0)
                VM_Quit(self, "queue slice [%ld : %ld] not possible", x, y);
            if(y > len)
                VM_Quit(self, "queue slice [%ld : %ld] not possible - right bound larger than queue of size %ld", x, y, len);
            value = Value_Queue();
            for(int64_t i = x; i < y; i++)
                Queue_PshB(value->of.queue, Value_Copy(Queue_Get(a->of.queue, i)));
        }
        else
            VM_Quit(self, "type %s was attempted to be sliced - only maps, queues, and strings can be sliced", Type_ToString(a->type));
    }
    else
    if(b->type == TYPE_STRING)
    {
        value = Value_Map();
        VM_TypeExpect(self, a->type, TYPE_MAP);
        VM_TypeExpect(self, b->type, TYPE_STRING);
        VM_TypeExpect(self, c->type, TYPE_STRING);
        if(!Map_Exists(a->of.map, b->of.string->value)) VM_Quit(self, "key %s does not exist with map slice", b->of.string->value); 
        if(!Map_Exists(a->of.map, c->of.string->value)) VM_Quit(self, "key %s does not exist with map slice", c->of.string->value); 
        Value* keys = Map_Key(a->of.map);
        int64_t x = Queue_Index(keys->of.queue, 0, b, (Compare) Value_Equal);
        int64_t y = Queue_Index(keys->of.queue, x, c, (Compare) Value_Equal);
        for(int64_t i = x; i < y; i++)
        {
            Value* temp = Queue_Get(keys->of.queue, i);
            String* key = String_Copy(temp->of.string);
            Map_Set(value->of.map, key, Value_Copy(Map_Get(a->of.map, key->value)));
        }
        Value_Kill(keys);
    }
    else
        VM_Quit(self, "type %s cannot be indexed for array slicing", Type_ToString(b->type));
    VM_Pop(self, 3);
    Queue_PshB(self->stack, value);
}

static void
VM_Num(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Back(self->stack);
    VM_TypeExpect(self, a->type, TYPE_STRING);
    double number = String_ToNumber(a->of.string->value);
    VM_Pop(self, 1);
    Queue_PshB(self->stack, Value_Number(number));
}

static void
VM_Bol(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Back(self->stack);
    VM_TypeExpect(self, a->type, TYPE_STRING);
    if(String_IsBoolean(a->of.string))
    {
        bool boolean = String_Equals(a->of.string, "true");
        VM_Pop(self, 1);
        Queue_PshB(self->stack, Value_Bool(boolean));
    }
    else
        VM_Quit(self, "cannot convert %s to boolean", a->of.string->value);
}

static void
VM_God(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Back(self->stack);
    VM_TypeExpect(self, a->type, TYPE_FILE);
    bool boolean = a->of.file->file != NULL;
    VM_Pop(self, 1);
    Queue_PshB(self->stack, Value_Bool(boolean));
}

static void
VM_Key(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Back(self->stack);
    VM_TypeExpect(self, a->type, TYPE_MAP);
    Value* queue = Map_Key(a->of.map);
    VM_Pop(self, 1);
    Queue_PshB(self->stack, queue);
}

static void
VM_Ext(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Back(self->stack);
    VM_TypeExpect(self, a->type, TYPE_NUMBER);
    exit(a->of.number);
    VM_Pop(self, 1);
    Queue_PshB(self->stack, Value_Null());
}

static void
VM_Tim(VM* self, int64_t unused)
{
    (void) unused;
    Queue_PshB(self->stack, Value_Number(Microseconds()));
}

static void
VM_Srd(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Back(self->stack);
    VM_TypeExpect(self, a->type, TYPE_NUMBER);
    srand(a->of.number);
    VM_Pop(self, 1);
    Queue_PshB(self->stack, Value_Null());
}

static void
VM_Ran(VM* self, int64_t unused)
{
    (void) unused;
    double number = rand();
    Queue_PshB(self->stack, Value_Number(number));
}

static void
VM_Asr(VM* self, int64_t unused)
{
    (void) unused;
    Value* a = Queue_Back(self->stack);
    VM_TypeExpect(self, a->type, TYPE_BOOL);
    if(a->of.boolean == false)
        VM_Quit(self, "assert");
    VM_Pop(self, 1);
    Queue_PshB(self->stack, Value_Null());
}

static const Gen Gens[] = {
#define X(name) { #name, OPCODE_##name, VM_##name },
OPCODES
#undef X
};

static int
Gen_Compare(const void* a, const void* b)
{
    const Gen* aa = a;
    const Gen* bb = b;
    return strcmp(aa->mnemonic, bb->mnemonic);
}

static Gen*
Gen_Find(char* mnemonic)
{
    Gen key = { .mnemonic = mnemonic };
    return bsearch(&key, Gens, LEN(Gens), sizeof(Gen), Gen_Compare);
}

static VM*
VM_Assemble(Queue* assembly, Queue* debug)
{
    int64_t size = 0;
    Map* labels = ASM_Label(assembly, &size);
    Queue* addresses = ASM_Flatten(labels);
    VM* self = VM_Init(size, debug, addresses);
    int64_t pc = 0;
    for(int64_t i = 0; i < assembly->size; i++)
    {
        String* stub = Queue_Get(assembly, i);
        if(stub->value[0] == '\t')
        {
            String* line = String_Init(stub->value + 1);
            char* mnemonic = strtok(line->value, " \n");
            Gen* gen = Gen_Find(mnemonic);
            if(gen == NULL)
                Quit("assembler unknown mnemonic %s", mnemonic);
            self->instructions[pc] = VM_Redirect(self, labels, gen->opcode);
            pc += 1;
            String_Kill(line);
        }
    }
    Map_Kill(labels);
    return self;
}

static void
CC_Reserve(CC* self)
{
    for(uint64_t i = 0; i < LEN(Handles); i++)
    {
        Handle handle = Handles[i];
        CC_Define(self, CLASS_FUNCTION, handle.args, String_Init(handle.name), String_Init("reserved"));
    }
}

static void
VM_Run(VM* self, bool arbitrary)
{
    while(self->done == false)
    {
        uint64_t instruction = self->instructions[self->pc];
        self->pc += 1;
        Opcode oc = instruction & 0xFF;
        Gens[oc].exec(self, instruction >> 8);
        if(arbitrary)
            if(oc == OPCODE_Ret || oc == OPCODE_Fls)
                break;
    }
}

static Args
Args_Parse(int64_t argc, char* argv[])
{
    Args self = { NULL, false, false };
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
    Handle_Sort();
    Args args = Args_Parse(argc, argv);
    if(args.entry)
    {
        String* entry = String_Init(args.entry);
        CC* cc = CC_Init();
        CC_Reserve(cc);
        CC_Including(cc, entry);
        CC_Parse(cc);
        VM* vm = VM_Assemble(cc->assembly, cc->debug);
        if(args.dump)
        {
            ASM_Dump(cc->assembly);
            VM_Data(vm);
        }
        else
            VM_Run(vm, false);
        int64_t retno = vm->retno;
        VM_AssertRefs(vm);
        VM_Kill(vm);
        CC_Kill(cc);
        String_Kill(entry);
        return retno;
    }
    else
    if(args.help)
        Args_Help();
    else
        Quit("usage: rr -h");
}
