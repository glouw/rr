/* 
 * The Roman II Programming Language
 *
 * Copyright (C) 2021-2022 Gustav Louw. All rights reserved.
 * This work is licensed under the terms of the MIT license.  
 * For a copy, see <https://opensource.org/licenses/MIT>.
 *
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/time.h>

#define QUEUE_BLOCK_SIZE (8)
#define MAP_UNPRIMED (-1)
#define MODULE_BUFFER_SIZE (512)
#define STR_CAP_SIZE (16)

typedef void
(*Kill)(void*);

typedef void*
(*Copy)(void*);

typedef bool
(*Equal)(void*, void*);

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
    CLASS_FUNCTION
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
    OPCODE_END,
    OPCODE_EQL,
    OPCODE_FLS,
    OPCODE_FMT,
    OPCODE_GET,
    OPCODE_GLB,
    OPCODE_GRT,
    OPCODE_GTE,
    OPCODE_INS,
    OPCODE_JMP,
    OPCODE_LEN,
    OPCODE_LOC,
    OPCODE_LOD,
    OPCODE_LOR,
    OPCODE_LST,
    OPCODE_LTE,
    OPCODE_MOV,
    OPCODE_MUL,
    OPCODE_NEQ,
    OPCODE_NOT,
    OPCODE_POP,
    OPCODE_PRT,
    OPCODE_PSB,
    OPCODE_PSF,
    OPCODE_PSH,
    OPCODE_RET,
    OPCODE_SAV,
    OPCODE_SPD,
    OPCODE_SUB,
    OPCODE_TYP,
}
Opcode;

typedef enum
{
    TYPE_ARRAY,
    TYPE_CHAR,
    TYPE_DICT,
    TYPE_STRING,
    TYPE_NUMBER,
    TYPE_BOOL,
    TYPE_NULL,
}
Type;

typedef struct
{
    char* value;
    int size;
    int cap;
}
Str;

typedef struct Node
{
    struct Node* next;
    Str* key;
    void* value;
}
Node;

typedef struct
{
    Node** bucket;
    Kill kill;
    Copy copy;
    int size;
    int prime_index;
    float load_factor;
}
Map;

typedef struct
{
    void* value[QUEUE_BLOCK_SIZE];
    int a;
    int b;
}
Block;

typedef struct
{
    Block** block;
    Kill kill;
    Copy copy;
    int size;
    int blocks;
}
Queue;

typedef struct
{
    Queue* modules;
    Queue* assembly;
    Queue* data;
    Map* identifiers;
    Map* included;
    Str* prime;
    int globals;
    int locals;
    int labels;
}
CC;

typedef struct
{
    FILE* file;
    Str* name;
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
}
Meta;

typedef union
{
    Queue* array;
    Map* dict;
    Str* string;
    char* character;
    double number;
    bool boolean;
}
Of;

typedef struct
{
    Type type;
    Of of;
    int refs;
}
Value;

typedef struct
{
    Value* a;
    Value* b;
}
Head;

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
US(void)
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

static inline bool
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
Type_String(Type self)
{
    switch(self)
    {
    case TYPE_ARRAY:
        return "array";
    case TYPE_CHAR:
        return "char";
    case TYPE_DICT:
        return "dict";
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
    }
    return "N/A";
}

static Block*
Block_Init(End end)
{
    Block* self = Malloc(sizeof(*self));
    self->a = (end == BACK) ? 0 : QUEUE_BLOCK_SIZE;
    self->b = (end == BACK) ? 0 : QUEUE_BLOCK_SIZE;
    return self;
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

static void
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
            Quit("vm: array delete: out of range error");
    }
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
Queue_Equal(Queue* self, Queue* other, Equal equal)
{
    if(Queue_Size(self) != Queue_Size(other))
        return false;
    else
        for(int i = 0; i < Queue_Size(self); i++)
            if(!equal(Queue_Get(self, i), Queue_Get(other, i)))
                return false;
    return true;
}

static inline void
Str_Alloc(Str* self, int cap)
{
    self->cap = cap;
    self->value = Realloc(self->value, (1 + cap) * sizeof(*self->value));
}

static inline Str*
Str_Init(char* string)
{
    Str* self = Malloc(sizeof(*self));
    self->value = NULL;
    self->size = strlen(string);
    Str_Alloc(self, self->size < STR_CAP_SIZE ? STR_CAP_SIZE : self->size);
    strcpy(self->value, string);
    return self;
}

static inline void
Str_Kill(Str* self)
{
    Delete(Free, self->value);
    Free(self);
}

static inline Str*
Str_FromChar(char c)
{
    char string[] = { c, '\0' };
    return Str_Init(string);
}

static inline void
Str_Swap(Str** self, Str** other)
{
    Str* temp = *self;
    *self = *other;
    *other = temp;
}

static inline Str*
Str_Copy(Str* self)
{
    return Str_Init(self->value);
}

static inline int
Str_Size(Str* self)
{
    return self->size;
}

static inline char*
Str_End(Str* self)
{
    return &self->value[Str_Size(self) - 1];
}

static inline char
Str_Back(Str* self)
{
    return *Str_End(self);
}

static inline char*
Str_Begin(Str* self)
{
    return &self->value[0];
}

static inline int
Str_Empty(Str* self)
{
    return Str_Size(self) == 0;
}

static inline void
Str_PshB(Str* self, char ch)
{
    if(Str_Size(self) == self->cap)
        Str_Alloc(self, (self->cap == 0) ? 1 : (2 * self->cap));
    self->value[self->size + 0] = ch;
    self->value[self->size + 1] = '\0';
    self->size += 1;
}

static inline Str*
Str_Indent(int indents)
{
    int width = 4;
    Str* ident = Str_Init("");
    while(indents > 0)
    {
        for(int i = 0; i < width; i++)
            Str_PshB(ident, ' ');
        indents -= 1;
    }
    return ident;
}

static inline void
Str_PopB(Str* self)
{
    self->size -= 1;
    self->value[self->size] = '\0';
}

static inline void
Str_Resize(Str* self, int size)
{
    if(size > self->cap)
        Str_Alloc(self, size);
    self->size = size;
    self->value[size] = '\0';
}

static inline bool
Str_Equals(Str* a, char* b)
{
    return Equals(a->value, b);
}

static inline bool
Str_Equal(Str* a, Str* b)
{
    return Equals(a->value, b->value);
}

static inline void
Str_Appends(Str* self, char* str)
{
    while(*str)
    {
        Str_PshB(self, *str);
        str += 1;
    }
}

static inline void
Str_Append(Str* self, Str* other)
{
    Str_Appends(self, other->value);
    Str_Kill(other);
}

static inline Str*
Str_Base(Str* path)
{
    Str* base = Str_Copy(path);
    while(!Str_Empty(base))
        if(Str_Back(base) == '/')
            break;
        else
            Str_PopB(base);
    return base;
}

static inline Str*
Str_Moves(char** from)
{
    Str* self = Malloc(sizeof(*self));
    self->value = *from;
    self->size = strlen(self->value);
    self->cap = self->size;
    *from = NULL;
    return self;
}

static inline Str*
Str_Skip(Str* self, char c)
{
    char* value = self->value;
    while(*value)
    {
        if(*value != c)
            break;
        value += 1;
    }
    return Str_Init(value);
}

static inline bool
Str_IsBoolean(Str* ident)
{
    return Str_Equals(ident, "true") || Str_Equals(ident, "false");
}

static inline bool
Str_IsNull(Str* ident)
{
    return Str_Equals(ident, "null");
}

static inline Str*
Str_Format(char* format, ...)
{
    va_list args;
    va_start(args, format);
    Str* line = Str_Init("");
    Str_Resize(line, STR_CAP_SIZE);
    int size = vsnprintf(line->value, line->size, format, args);
    va_end(args);
    if(size > line->size)
    {
        va_start(args, format);
        Str_Resize(line, size);
        vsprintf(line->value, format, args);
        va_end(args);
    }
    return line;
}

static inline char*
Str_Get(Str* self, int index)
{
    if(index < 0 || index >= Str_Size(self))
        return NULL;
    return &self->value[index];
}

static inline void
Str_Del(Str* self, int index)
{
    if(Str_Get(self, index))
    {
        for(int i = index; i < Str_Size(self) - 1; i++)
            self->value[i] = self->value[i + 1];
        Str_PopB(self);
    }
    else
        Quit("vm: string delete: out of range error");
}

static Node*
Node_Init(Str* key, void* value)
{
    Node* self = Malloc(sizeof(*self));
    self->next = NULL;
    self->key = key;
    self->value = value;
    return self;
}

static void
Node_Set(Node* self, Kill kill, Str* key, void* value)
{
    Delete((Kill) Str_Kill, self->key);
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
    return Node_Init(Str_Copy(self->key), copy ? copy(self->value) : self->value);
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
    int size = sizeof(Map_Primes) / sizeof(Map_Primes[0]);
    return self->prime_index < size - 1;
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
Map_Hash(char* key)
{
    unsigned hash = 5381;
    char ch = 0;
    while((ch = *key++))
        hash = ((hash << 5) + hash) + ch;
    return hash;
}

static Node**
Map_Bucket(Map* self, char* key)
{
    int index = Map_Hash(key) % Map_Buckets(self);
    return &self->bucket[index];
}

static void
Map_Alloc(Map* self, int index)
{
    self->prime_index = index;
    self->bucket = calloc(Map_Buckets(self), sizeof(*self->bucket));
}

static void
Map_Emplace(Map*, Str*, Node*);

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
Map_Emplace(Map* self, Str* key, Node* node)
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
            if(Str_Equals(chain->key, key))
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
            if(Str_Equals(chain->key, key))
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
Map_Set(Map* self, Str* key, void* value)
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
            Map_Set(self, Str_Copy(chain->key), self->copy ? self->copy(chain->value) : chain->value);
            chain = chain->next;
        }
    }
}

static bool
Map_Equal(Map* self, Map* other, Equal equal)
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
                if(!equal(chain->value, got))
                    return false;
                chain = chain->next;
            }
        }
    }
    return true;
}

static Meta*
Meta_Init(Class class, int stack)
{
    Meta* self = Malloc(sizeof(*self));
    self->class = class;
    self->stack = stack;
    return self;
}

static void
Meta_Kill(Meta* self)
{
    Free(self);
}

static void
Module_Buffer(Module* self)
{
    self->index = 0;
    self->size = fread(self->buffer, sizeof(*self->buffer), MODULE_BUFFER_SIZE, self->file);
}

static Module*
Module_Init(Str* path)
{
    FILE* file = fopen(path->value, "r");
    if(file)
    {
        Module* self = Malloc(sizeof(*self));
        self->file = file;
        self->line = 1;
        self->name = Str_Copy(path);
        Module_Buffer(self);
        return self;
    }
    return NULL;
}

static void
Module_Kill(Module* self)
{
    Str_Kill(self->name);
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
    fprintf(stderr, "compilation error: file `%s`: line `%d`: ", back ? back->name->value : "?", back ? back->line : 0);
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(0xFE);
}

static void
CC_LoadModule(CC* self, Str* file)
{
    Str* path;
    if(Queue_Empty(self->modules))
    {
        path = Str_Init("");
        Str_Resize(path, 4096);
        getcwd(Str_Begin(path), Str_Size(path));
        strcat(Str_Begin(path), "/");
        strcat(Str_Begin(path), file->value);
    }
    else
    {
        Module* back = Queue_Back(self->modules);
        path = Str_Base(back->name);
        Str_Appends(path, file->value);
    }
    char* real = realpath(path->value, NULL);
    if(real == NULL)
        CC_Quit(self, "could not resolve path `%s`", path->value);
    if(Map_Exists(self->included, real))
        Free(real);
    else
    {
        Str* resolved = Str_Moves(&real);
        Map_Set(self->included, resolved, NULL);
        Queue_PshB(self->modules, Module_Init(resolved));
    }
    Str_Kill(path);
}

static void
CC_Advance(CC* self)
{
    Module_Advance(Queue_Back(self->modules));
}

static bool
CC_IsLower(int c)
{
    return c >= 'a' && c <= 'z';
}

static bool
CC_IsUpper(int c)
{
    return c >= 'A' && c <= 'Z';
}

static bool
CC_IsAlpha(int c)
{
    return CC_IsLower(c) || CC_IsUpper(c);
}

static bool
CC_IsDigit(int c)
{
    return c >= '0' && c <= '9';
}

static bool
CC_IsNumber(int c)
{
    return CC_IsDigit(c) || c == '.';
}

static bool
CC_IsIdentLeader(int c)
{
    return CC_IsAlpha(c) || c == '_';
}

static bool
CC_IsIdent(int c)
{
    return CC_IsIdentLeader(c) || CC_IsDigit(c);
}

static bool
CC_IsMod(int c)
{
    return CC_IsIdent(c) || c == '.';
}

static bool
CC_IsOp(int c)
{
    return c == '*' || c == '/' || c == '%' || c == '+' || c == '-' || c == '='
        || c == '<' || c == '>' || c == '!' || c == '&' || c == '|';
}

static bool
CC_IsEsc(int c)
{
    return c == '"' || c == '/' || c == 'b' || c == 'f' || c == 'n' || c == 'r' 
        || c == 't' || c == 'u' || c == '\\';
}

static bool
CC_IsSpace(int c)
{
    return c == '\n' || c == '\t' || c == '\r' || c == ' ';
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
        if(CC_IsSpace(peak) || comment)
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

static Str*
CC_Stream(CC* self, bool clause(int))
{
    Str* str = Str_Init("");
    CC_Spin(self);
    while(clause(CC_Peak(self)))
        Str_PshB(str, CC_Read(self));
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
            CC_Quit(self, "got `%s`, expected `%c`", (peak == EOF) ? "EOF" : formatted, *expect);
        }
        expect += 1;
    }
}

static Str*
CC_Mod(CC* self)
{
    return CC_Stream(self, CC_IsMod);
}

static Str*
CC_Ident(CC* self)
{
    return CC_Stream(self, CC_IsIdent);
}

static Str*
CC_Operator(CC* self)
{
    return CC_Stream(self, CC_IsOp);
}

static Str*
CC_Number(CC* self)
{
    return CC_Stream(self, CC_IsNumber);
}

static Str*
CC_Str(CC* self)
{
    Str* str = Str_Init("");
    CC_Spin(self);
    CC_Match(self, "\"");
    while(true)
    {
        int lead = CC_Peak(self);
        if(lead == '"')
            break;
        else
        {
            Str_PshB(str, CC_Read(self));
            if(lead == '\\')
            {
                int escape = CC_Peak(self);
                if(!CC_IsEsc(escape))
                    CC_Quit(self, "unknown escape char `0x%02X`\n", escape);
                Str_PshB(str, CC_Read(self));
            }
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
    self->assembly = Queue_Init((Kill) Str_Kill, (Copy) NULL);
    self->data = Queue_Init((Kill) Str_Kill, (Copy) NULL);
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

static Str*
CC_Parents(Str* module)
{
    Str* parents = Str_Init("");
    for(char* value = module->value; *value; value++)
    {
        if(*value != '.')
            break;
        Str_Appends(parents, "../");
    }
    return parents;
}

static void
CC_Include(CC* self)
{
    Str* module = CC_Mod(self);
    CC_Match(self, ";");
    Str* skipped = Str_Skip(module, '.');
    Str_Appends(skipped, ".rr");
    Str* parents = CC_Parents(module);
    Str_Appends(parents, skipped->value);
    CC_LoadModule(self, parents);
    Str_Kill(skipped);
    Str_Kill(module);
    Str_Kill(parents);
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
    return class == CLASS_FUNCTION;
}

static void
CC_Declare(CC* self, Class class, int stack, Str* ident)
{
    if(Map_Exists(self->identifiers, ident->value))
        CC_Quit(self, "`%s` already defined", ident->value);
    Map_Set(self->identifiers, ident, Meta_Init(class, stack));
}

static void
CC_Expression(CC*);

static void
CC_EmptyExpression(CC* self)
{
    CC_Expression(self);
    CC_Match(self, ";");
    Queue_PshB(self->assembly, Str_Init("\tpop"));
}

static void
CC_Assign(CC* self)
{
    CC_Match(self, ":=");
    CC_Expression(self);
    Queue_PshB(self->assembly, Str_Init("\tcpy"));
    CC_Match(self, ";");
}

static void
CC_Local(CC* self, Str* ident, bool assign)
{
    if(assign)
        CC_Assign(self);
    CC_Declare(self, CLASS_VARIABLE_LOCAL, self->locals, ident);
    self->locals += 1;
}

static Str*
CC_Global(CC* self, Str* ident)
{
    Str* label = Str_Format("!%s", ident->value);
    Queue_PshB(self->assembly, Str_Format("%s:", label->value));
    CC_Assign(self);
    CC_Declare(self, CLASS_VARIABLE_GLOBAL, self->globals, ident);
    Queue_PshB(self->assembly, Str_Init("\tret"));
    self->globals += 1;
    return label;
}

static Queue*
CC_Params(CC* self)
{
    Queue* params = Queue_Init((Kill) Str_Kill, (Copy) NULL);
    CC_Match(self, "(");
    while(CC_Next(self) != ')')
    {
        Str* ident = CC_Ident(self);
        Queue_PshB(params, ident);
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    self->locals = 0;
    for(int i = 0; i < Queue_Size(params); i++)
    {
        Str* ident = Str_Copy(Queue_Get(params, i));
        CC_Local(self, ident, false);
    }
    CC_Match(self, ")");
    return params;
}

static int
CC_PopScope(CC* self, Queue* scope)
{
    int popped = Queue_Size(scope);
    for(int i = 0; i < popped; i++)
    {
        Str* key = Queue_Get(scope, i);
        Map_Del(self->identifiers, key->value);
        self->locals -= 1;
    }
    for(int i = 0; i < popped; i++)
        Queue_PshB(self->assembly, Str_Init("\tpop"));
    Queue_Kill(scope);
    return popped;
}

static Meta*
CC_Expect(CC* self, Str* ident, bool clause(Class))
{
    Meta* meta = Map_Get(self->identifiers, ident->value);
    if(meta == NULL)
        CC_Quit(self, "identifier `%s` not defined", ident->value);
    if(!clause(meta->class))
        CC_Quit(self, "identifier `%s` is `%s`", ident->value, Class_String(meta->class));
    return meta;
}

static void
CC_Ref(CC* self, Str* ident)
{
    Meta* meta = CC_Expect(self, ident, CC_IsVariable);
    if(meta->class == CLASS_VARIABLE_GLOBAL)
        Queue_PshB(self->assembly, Str_Format("\tglb %d", meta->stack));
    else
    if(meta->class == CLASS_VARIABLE_LOCAL)
        Queue_PshB(self->assembly, Str_Format("\tloc %d", meta->stack));
}

static void
CC_Resolve(CC* self)
{
    while(CC_Next(self) == '[')
    {
        CC_Match(self, "[");
        CC_Expression(self);
        CC_Match(self, "]");
        Queue_PshB(self->assembly, Str_Init("\tget"));
    }
}

static void
CC_ByRef(CC* self)
{
    CC_Match(self, "&");
    Str* ident = CC_Ident(self);
    CC_Ref(self, ident);
    CC_Resolve(self);
    Str_Kill(ident);
}

static void
CC_ByVal(CC* self)
{
    CC_Expression(self);
    Queue_PshB(self->assembly, Str_Init("\tcpy"));
}

static void
CC_Pass(CC* self)
{
    if(CC_Next(self) == '&')
        CC_ByRef(self);
    else
        CC_ByVal(self);
}

static void
CC_Args(CC* self, int required)
{
    CC_Match(self, "(");
    int args = 0;
    while(CC_Next(self) != ')')
    {
        CC_Pass(self);
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
        args += 1;
    }
    if(args != required)
        CC_Quit(self, "required `%d` arguments but got `%d` args", required, args);
    CC_Match(self, ")");
}

static void
CC_Call(CC* self, Str* ident, Meta* meta)
{
    for(int i = 0; i < meta->stack; i++)
        Queue_PshB(self->assembly, Str_Init("\tspd"));
    Queue_PshB(self->assembly, Str_Format("\tcal %s", ident->value));
    Queue_PshB(self->assembly, Str_Init("\tlod"));
}

static void
CC_Dict(CC* self)
{
    Queue_PshB(self->assembly, Str_Init("\tpsh {}"));
    Queue_PshB(self->assembly, Str_Init("\tcpy"));
    CC_Match(self, "{");
    while(CC_Next(self) != '}')
    {
        CC_Expression(self); // No need to copy, because dict keys aren't tracked as stack values.
        CC_Match(self, ":");
        CC_Expression(self);
        Queue_PshB(self->assembly, Str_Init("\tcpy"));
        Queue_PshB(self->assembly, Str_Init("\tins"));
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    CC_Match(self, "}");
}

static void
CC_Array(CC* self)
{
    Queue_PshB(self->assembly, Str_Init("\tpsh []"));
    Queue_PshB(self->assembly, Str_Init("\tcpy"));
    CC_Match(self, "[");
    while(CC_Next(self) != ']')
    {
        CC_Expression(self);
        Queue_PshB(self->assembly, Str_Init("\tcpy"));
        Queue_PshB(self->assembly, Str_Init("\tpsb"));
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    CC_Match(self, "]");
}

static bool
CC_Factor(CC*);

static void
CC_Not(CC* self)
{
    CC_Match(self, "!");
    CC_Factor(self);
    Queue_PshB(self->assembly, Str_Init("\tnot"));
}

static void
CC_VM_Direct(CC* self)
{
    Str* number = CC_Number(self);
    Queue_PshB(self->assembly, Str_Format("\tpsh %s", number->value));
    Str_Kill(number);
}

static bool
CC_VM_Indirect(CC* self)
{
    bool storage = false;
    Str* ident = NULL;
    if(self->prime)
        Str_Swap(&self->prime, &ident);
    else
        ident = CC_Ident(self);
    if(Str_IsBoolean(ident))
        Queue_PshB(self->assembly, Str_Format("\tpsh %s", ident->value));
    else
    if(Str_IsNull(ident))
        Queue_PshB(self->assembly, Str_Format("\tpsh %s", ident->value));
    else
    if(CC_Next(self) == '(')
    {
        Meta* meta = CC_Expect(self, ident, CC_IsFunction);
        CC_Args(self, meta->stack);
        if(Str_Equals(ident, "len"))
            Queue_PshB(self->assembly, Str_Init("\tlen"));
        else
        if(Str_Equals(ident, "del"))
            Queue_PshB(self->assembly, Str_Init("\tdel"));
        else
        if(Str_Equals(ident, "type"))
            Queue_PshB(self->assembly, Str_Init("\ttyp"));
        else
        if(Str_Equals(ident, "print"))
            Queue_PshB(self->assembly, Str_Init("\tprt"));
        else
            CC_Call(self, ident, meta);
    }
    else
    {
        CC_Ref(self, ident);
        storage = true;
    }
    Str_Kill(ident);
    return storage;
}

static void
CC_Reserve(CC* self)
{
    CC_Declare(self, CLASS_FUNCTION, 1, Str_Init("len"));
    CC_Declare(self, CLASS_FUNCTION, 2, Str_Init("del"));
    CC_Declare(self, CLASS_FUNCTION, 1, Str_Init("type"));
    CC_Declare(self, CLASS_FUNCTION, 1, Str_Init("print"));
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
    Str* string = CC_Str(self);
    Queue_PshB(self->assembly, Str_Format("\tpsh \"%s\"", string->value));
    Str_Kill(string);
}

static bool
CC_Factor(CC* self)
{
    bool storage = false;
    int next = CC_Next(self);
    if(next == '!')
        CC_Not(self);
    else
    if(CC_IsDigit(next))
        CC_VM_Direct(self);
    else
    if(CC_IsIdent(next) || self->prime)
        storage = CC_VM_Indirect(self);
    else
    if(next == '(')
        CC_Force(self);
    else
    if(next == '{')
        CC_Dict(self);
    else
    if(next == '[')
        CC_Array(self);
    else
    if(next == '"')
        CC_String(self);
    else
        CC_Quit(self, "unknown factor starting with `%c`", next);
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
       || CC_Next(self) == '|')
    {
        Str* operator = CC_Operator(self);
        if(Str_Equals(operator, "*="))
        {
            if(!storage)
                CC_Quit(self, "factor - left hand side must be storage");
            CC_Expression(self);
            Queue_PshB(self->assembly, Str_Init("\tmul"));
        }
        else
        if(Str_Equals(operator, "/="))
        {
            if(!storage)
                CC_Quit(self, "factor - left hand side must be storage");
            CC_Expression(self);
            Queue_PshB(self->assembly, Str_Init("\tdiv"));
        }
        else
        {
            storage = false;
            Queue_PshB(self->assembly, Str_Init("\tcpy"));
            CC_Factor(self);
            if(Str_Equals(operator, "*"))
                Queue_PshB(self->assembly, Str_Init("\tmul"));
            else
            if(Str_Equals(operator, "/"))
                Queue_PshB(self->assembly, Str_Init("\tdiv"));
            else
            if(Str_Equals(operator, "%"))
                Queue_PshB(self->assembly, Str_Init("\tfmt"));
            else
            if(Str_Equals(operator, "||"))
                Queue_PshB(self->assembly, Str_Init("\tlor"));
            else
                CC_Quit(self, "operator `%s` not supported", operator->value);
        }
        Str_Kill(operator);
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
       || CC_Next(self) == '>'
       || CC_Next(self) == '<'
       || CC_Next(self) == '&')
    {
        Str* operator = CC_Operator(self);
        if(Str_Equals(operator, "="))
        {
            if(!storage)
                CC_Quit(self, "term - left hand side must be storage");
            CC_Expression(self);
            Queue_PshB(self->assembly, Str_Init("\tmov"));
        }
        else
        if(Str_Equals(operator, "+="))
        {
            if(!storage)
                CC_Quit(self, "term - left hand side must be storage");
            CC_Expression(self);
            Queue_PshB(self->assembly, Str_Init("\tadd"));
        }
        else
        if(Str_Equals(operator, "-="))
        {
            if(!storage)
                CC_Quit(self, "term - left hand side must be storage");
            CC_Expression(self);
            Queue_PshB(self->assembly, Str_Init("\tsub"));
        }
        else
        if(Str_Equals(operator, "=="))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, Str_Init("\teql"));
        }
        else
        if(Str_Equals(operator, "!="))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, Str_Init("\tneq"));
        }
        else
        if(Str_Equals(operator, ">"))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, Str_Init("\tgrt"));
        }
        else
        if(Str_Equals(operator, "<"))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, Str_Init("\tlst"));
        }
        else
        if(Str_Equals(operator, ">="))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, Str_Init("\tgte"));
        }
        else
        if(Str_Equals(operator, "<="))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, Str_Init("\tlte"));
        }
        else
        {
            storage = false;
            Queue_PshB(self->assembly, Str_Init("\tcpy"));
            CC_Term(self);
            if(Str_Equals(operator, "+"))
                Queue_PshB(self->assembly, Str_Init("\tadd"));
            else
            if(Str_Equals(operator, "-"))
                Queue_PshB(self->assembly, Str_Init("\tsub"));
            else
            if(Str_Equals(operator, "&&"))
                Queue_PshB(self->assembly, Str_Init("\tand"));
            else
                CC_Quit(self, "operator `%s` not supported", operator->value);
        }
        Str_Kill(operator);
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
CC_Block(CC*, int head, int tail, bool loop);

static void
CC_Branch(CC* self, int head, int tail, int end, bool loop)
{
    int next = CC_Label(self);
    CC_Match(self, "(");
    CC_Expression(self);
    CC_Match(self, ")");
    Queue_PshB(self->assembly, Str_Format("\tbrf @l%d", next));
    CC_Block(self, head, tail, loop);
    Queue_PshB(self->assembly, Str_Format("\tjmp @l%d", end));
    Queue_PshB(self->assembly, Str_Format("@l%d:", next));
}

static Str*
CC_Branches(CC* self, int head, int tail, int loop)
{
    int end = CC_Label(self);
    CC_Branch(self, head, tail, end, loop);
    Str* buffer = CC_Ident(self);
    while(Str_Equals(buffer, "elif"))
    {
        CC_Branch(self, head, tail, end, loop);
        Str_Kill(buffer);
        buffer = CC_Ident(self);
    }
    if(Str_Equals(buffer, "else"))
        CC_Block(self, head, tail, loop);
    Queue_PshB(self->assembly, Str_Format("@l%d:", end));
    Str* backup = NULL;
    if(Str_Equals(buffer, "elif") || Str_Equals(buffer, "else"))
        Str_Kill(buffer);
    else
        backup = buffer;
    return backup;
}

static void
CC_While(CC* self)
{
    int entry = CC_Label(self);
    int final = CC_Label(self);
    Queue_PshB(self->assembly, Str_Format("@l%d:", entry));
    CC_Match(self, "(");
    CC_Expression(self);
    Queue_PshB(self->assembly, Str_Format("\tbrf @l%d", final));
    CC_Match(self, ")");
    CC_Block(self, entry, final, true);
    Queue_PshB(self->assembly, Str_Format("\tjmp @l%d", entry));
    Queue_PshB(self->assembly, Str_Format("@l%d:", final));
}

static void
CC_Ret(CC* self)
{
    CC_Expression(self);
    Queue_PshB(self->assembly, Str_Init("\tsav"));
    Queue_PshB(self->assembly, Str_Init("\tfls"));
    CC_Match(self, ";");
}

static void
CC_Continue(CC* self, int head, bool loop)
{
    if(!loop)
        CC_Quit(self, "`continue` can only be used within while loops");
    CC_Match(self, ";");
    Queue_PshB(self->assembly, Str_Format("\tjmp @l%d", head));
}

static void
CC_Break(CC* self, int tail, bool loop)
{
    if(!loop)
        CC_Quit(self, "`break` can only be used within while loops");
    CC_Match(self, ";");
    Queue_PshB(self->assembly, Str_Format("\tjmp @l%d", tail));
}

static void
CC_Block(CC* self, int head, int tail, bool loop)
{
    Queue* scope = Queue_Init((Kill) Str_Kill, (Copy) NULL);
    CC_Match(self, "{");
    Str* prime = NULL; 
    while(CC_Next(self) != '}')
    {
        if(CC_IsIdentLeader(CC_Next(self)) || prime)
        {
            Str* ident = NULL;
            if(prime)
                Str_Swap(&prime, &ident);
            else
                ident = CC_Ident(self);
            if(Str_Equals(ident, "if"))
                prime = CC_Branches(self, head, tail, loop);
            else
            if(Str_Equals(ident, "elif"))
                CC_Quit(self, "`elif` must follow an `if` or `elif` block");
            else
            if(Str_Equals(ident, "else"))
                CC_Quit(self, "`else` must follow an `if` or `elif` block");
            else
            if(Str_Equals(ident, "while"))
                CC_While(self);
            else
            if(Str_Equals(ident, "ret"))
                CC_Ret(self);
            else
            if(Str_Equals(ident, "continue"))
                CC_Continue(self, head, loop);
            else
            if(Str_Equals(ident, "break"))
                CC_Break(self, tail, loop);
            else
            if(CC_Next(self) == ':')
            {
                Queue_PshB(scope, Str_Copy(ident));
                CC_Local(self, Str_Copy(ident), true);
            }
            else
            {
                self->prime = Str_Copy(ident);
                CC_EmptyExpression(self);
            }
            Str_Kill(ident);
        }
        else
            CC_EmptyExpression(self);
    }
    CC_Match(self, "}");
    CC_PopScope(self, scope);
}

static void
CC_Function(CC* self, Str* ident)
{
    Queue* params = CC_Params(self);
    CC_Declare(self, CLASS_FUNCTION, Queue_Size(params), ident);
    Queue_PshB(self->assembly, Str_Format("%s:", ident->value));
    CC_Block(self, 0, 0, false);
    CC_PopScope(self, params);
    Queue_PshB(self->assembly, Str_Init("\tpsh null"));
    Queue_PshB(self->assembly, Str_Init("\tsav"));
    Queue_PshB(self->assembly, Str_Init("\tret"));
}

static void
CC_Spool(CC* self, Queue* start)
{
    Queue* spool = Queue_Init((Kill) Str_Kill, NULL);
    Str* main = Str_Init("main");
    CC_Expect(self, main, CC_IsFunction);
    Queue_PshB(spool, Str_Init("!start:"));
    for(int i = 0; i < Queue_Size(start); i++)
    {
        Str* label = Queue_Get(start, i);
        Queue_PshB(spool, Str_Format("\tcal %s", label->value));
    }
    Queue_PshB(spool, Str_Format("\tcal %s", main->value));
    Queue_PshB(spool, Str_Format("\tend"));
    for(int i = Queue_Size(spool) - 1; i >= 0; i--)
        Queue_PshF(self->assembly, Str_Copy(Queue_Get(spool, i)));
    Str_Kill(main);
    Queue_Kill(spool);
}

static void
CC_Parse(CC* self)
{
    Queue* start = Queue_Init((Kill) Str_Kill, (Copy) NULL);
    while(CC_Peak(self) != EOF)
    {
        Str* ident = CC_Ident(self);
        if(Str_Equals(ident, "inc"))
        {
            CC_Include(self);
            Str_Kill(ident);
        }
        else
        if(CC_Next(self) == '(')
            CC_Function(self, ident);
        else
        if(CC_Next(self) == ':')
        {
            Str* label = CC_Global(self, ident);
            Queue_PshB(start, label);
        }
        else
            CC_Quit(self, "expected function, global, or include statement");
        CC_Spin(self);
    }
    CC_Spool(self, start);
    Queue_Kill(start);
}

static void
Type_Kill(Type type, Of of)
{
    switch(type)
    {
    case TYPE_ARRAY:
        Queue_Kill(of.array);
        break;
    case TYPE_DICT:
        Map_Kill(of.dict);
        break;
    case TYPE_STRING:
        Str_Kill(of.string);
        break;
    case TYPE_NUMBER:
    case TYPE_CHAR:
    case TYPE_BOOL:
    case TYPE_NULL:
        break;
    }
}

static int
Value_Len(Value* self)
{
    switch(self->type)
    {
    case TYPE_ARRAY:
        return Queue_Size(self->of.array);
    case TYPE_DICT:
        return Map_Size(self->of.dict);
    case TYPE_STRING:
        return Str_Size(self->of.string);
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
Value_Kill(Value* self)
{
    Type_Kill(self->type, self->of);
    Free(self);
}

static bool
Value_Equal(Value* a, Value* b)
{
    if(a->type == b->type)
        switch(a->type)
        {
        case TYPE_ARRAY:
            return Queue_Equal(a->of.array, b->of.array, (Equal) Value_Equal);
        case TYPE_DICT:
            return Map_Equal(a->of.dict, b->of.dict, (Equal) Value_Equal);
        case TYPE_STRING:
            return Str_Equal(a->of.string, b->of.string);
        case TYPE_NUMBER:
            return a->of.number == b->of.number;
        case TYPE_BOOL:
            return a->of.boolean == b->of.boolean;
        case TYPE_CHAR:
            return *a->of.character == *b->of.character;
        case TYPE_NULL:
            return b->type == TYPE_NULL;
        }
    return false;
}

static Str*
Value_Print(Value* self, bool newline, int indents);

static Str*
Queue_Print(Queue* self, int indents)
{
    if(Queue_Empty(self))
        return Str_Init("[]");
    else
    {
        Str* print = Str_Init("[\n");
        int size = Queue_Size(self);
        for(int i = 0; i < size; i++)
        {
            Str_Append(print, Str_Indent(indents + 1));
            Str_Append(print, Str_Format("[%d] = ", i));
            Str_Append(print, Value_Print(Queue_Get(self, i), false, indents + 1));
            if(i < size - 1)
                Str_Appends(print, ",");
            Str_Appends(print, "\n");
        }
        Str_Append(print, Str_Indent(indents));
        Str_Appends(print, "]");
        return print;
    }
}

static Str*
Map_Print(Map* self, int indents)
{
    if(Map_Empty(self))
        return Str_Init("{}");
    else
    {
        Str* print = Str_Init("{\n");
        for(int i = 0; i < Map_Buckets(self); i++)
        {
            Node* bucket = self->bucket[i];
            while(bucket)
            {
                Str_Append(print, Str_Indent(indents + 1));
                Str_Append(print, Str_Format("\"%s\" : ", bucket->key->value));
                Str_Append(print, Value_Print(bucket->value, false, indents + 1));
                if(i < Map_Size(self) - 1)
                    Str_Appends(print, ",");
                Str_Appends(print, "\n");
                bucket = bucket->next;
            }
        }
        Str_Append(print, Str_Indent(indents));
        Str_Appends(print, "}");
        return print;
    }
}

static Str*
Value_Print(Value* self, bool newline, int indents)
{
    Str* print = Str_Init("");
    switch(self->type)
    {
    case TYPE_ARRAY:
        Str_Append(print, Queue_Print(self->of.array, indents));
        break;
    case TYPE_DICT:
        Str_Append(print, Map_Print(self->of.dict, indents));
        break;
    case TYPE_STRING:
        Str_Append(print, Str_Format(indents == 0 ? "%s" : "\"%s\"", self->of.string->value));
        break;
    case TYPE_NUMBER:
        Str_Append(print, Str_Format("%f", self->of.number));
        break;
    case TYPE_BOOL:
        Str_Append(print, Str_Format("%s", self->of.boolean ? "true" : "false"));
        break;
    case TYPE_CHAR:
        Str_Append(print, Str_Format("%c", *self->of.character));
        break;
    case TYPE_NULL:
        Str_Appends(print, "null");
        break;
    }
    if(newline)
        Str_Appends(print, "\n");
    return print; 
}

static Value*
Value_Copy(Value*);

static Value*
Value_Init(Of of, Type type)
{
    Value* self = Malloc(sizeof(*self));
    self->type = type;
    self->refs = 0;
    switch(type)
    {
    case TYPE_ARRAY:
        self->of.array = Queue_Init((Kill) Value_Kill, (Copy) Value_Copy);
        break;
    case TYPE_DICT:
        self->of.dict = Map_Init((Kill) Value_Kill, (Copy) Value_Copy);
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
    case TYPE_CHAR:
        self->of.character = of.character;
        break;
    case TYPE_NULL:
        break;
    }
    return self;
}

static void
Value_PromoteChar(Value* self)
{
    self->type = TYPE_STRING;
    self->of.string = Str_FromChar(*self->of.character);
    self->refs = 0;
}

static void
Value_PromoteChars(Value* a, Value* b)
{
    if((a->type == TYPE_CHAR && b->type == TYPE_STRING)
    || (b->type == TYPE_CHAR && a->type == TYPE_STRING))
    {
        if(a->type == TYPE_CHAR) Value_PromoteChar(a);
        if(b->type == TYPE_CHAR) Value_PromoteChar(b);
    }
}

static void
Type_Copy(Value* copy, Value* self)
{
    switch(self->type)
    {
    case TYPE_ARRAY:
        copy->of.array = Queue_Copy(self->of.array);
        break;
    case TYPE_DICT:
        copy->of.dict = Map_Copy(self->of.dict);
        break;
    case TYPE_STRING:
        copy->of.string = Str_Copy(self->of.string);
        break;
    case TYPE_CHAR:
        copy->type = TYPE_STRING;
        copy->of.string = Str_FromChar(*self->of.character);
        break;
    case TYPE_NUMBER:
    case TYPE_BOOL:
    case TYPE_NULL:
        copy->of = self->of;
        break;
    }
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

static Map*
ASM_Label(Queue* assembly, int* size)
{
    Map* labels = Map_Init((Kill) Free, (Copy) NULL);
    for(int i = 0; i < Queue_Size(assembly); i++)
    {
        Str* stub = Queue_Get(assembly, i);
        if(stub->value[0] == '\t')
            *size += 1;
        else 
        {
            Str* line = Str_Copy(stub);
            char* label = strtok(line->value, ":");
            if(Map_Exists(labels, label))
                Quit("asm: label %s already defined", label);
            Map_Set(labels, Str_Init(label), Int_Init(*size));
            Str_Kill(line);
        }
    }
    return labels;
}

static inline void
ASM_Dump(Queue* assembly)
{
    for(int i = 0; i < Queue_Size(assembly); i++)
    {
        Str* assem = Queue_Get(assembly, i);
        puts(assem->value);
    }
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

static inline void
VM_Data(VM* self)
{
    fprintf(stderr, ".data:\n");
    for(int i = 0; i < Queue_Size(self->data); i++)
    {
        Value* value = Queue_Get(self->data, i);
        fprintf(stderr, "%4d : %2d : ", i, value->refs);
        Str* print = Value_Print(value, true, 0);
        printf("%s", print->value);
        Str_Kill(print);
        if(value->refs > 0)
            Quit("internal: data segment contained references at time of exit");
    }
}

static inline void
VM_Text(VM* self)
{
    fprintf(stderr, ".text:\n");
    for(int i = 0; i < self->size; i++)
        fprintf(stderr, "%4d : 0x%08lX\n", i, self->instructions[i]);
}

static void
VM_Pop(VM* self)
{
    Value* value = Queue_Back(self->stack);
    if(value->refs == 0 || value->type == TYPE_CHAR)
        Queue_PopB(self->stack);
    else
    {
        value->refs -= 1;
        Queue_PopBSoft(self->stack);
    }
}

static void
VM_Store(VM* self, char* operator)
{
    Of of;
    Value* value;
    char ch = operator[0];
    if(ch == '[')
        value = Value_Init(of, TYPE_ARRAY);
    else
    if(ch == '{')
        value = Value_Init(of, TYPE_DICT);
    else
    if(ch == '"')
    {
        int len = strlen(operator);
        operator[len - 1] = '\0';
        of.string = Str_Init(operator + 1);
        value = Value_Init(of, TYPE_STRING);
    }
    else
    if(ch == 't' || ch == 'f')
    {
        of.boolean = Equals(operator, "true") ? true : false;
        value = Value_Init(of, TYPE_BOOL);
    }
    else
    if(ch == 'n')
        value = Value_Init(of, TYPE_NULL);
    else
    if(CC_IsNumber(ch))
    {
        of.number = atof(operator);
        value = Value_Init(of, TYPE_NUMBER);
    }
    else
        Quit("vm: unknown psh operand encountered");
    Queue_PshB(self->data, value);
}

static int
VM_Datum(VM* self, char* operator)
{
    VM_Store(self, operator);
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
        Str* stub = Queue_Get(assembly, i);
        if(stub->value[0] == '\t')
        {
            Str* line = Str_Init(stub->value + 1);
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
                instruction = VM_Datum(self, strtok(NULL, "\n"));
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
            if(Equals(mnemonic, "sub"))
                instruction = OPCODE_SUB;
            else
            if(Equals(mnemonic, "typ"))
                instruction = OPCODE_TYP;
            else
                Quit("asm: unknown mnemonic `%s`", mnemonic);
            self->instructions[pc] = instruction;
            pc += 1;
            Str_Kill(line);
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
    if(back->refs > 0)
    {
        Value* value = Value_Copy(back);
        VM_Pop(self);
        Queue_PshB(self->stack, value);
    }
}

static int
VM_End(VM* self)
{
    if(self->ret->type != TYPE_NUMBER)
        Quit("`vm: main` return type was type `%s`; expected `%s`", Type_String(self->ret->type), Type_String(TYPE_NUMBER));
    int ret = self->ret->of.number;
    if(self->ret->refs == 0) // Expression at teardown.
        Value_Kill(self->ret);
    else
        self->ret->refs = 0; // Owned by data.
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
    value->refs += 1;
    Queue_PshB(self->stack, value);
}

static void
VM_Loc(VM* self, int address)
{
    Frame* frame = Queue_Back(self->frame);
    Value* value = Queue_Get(self->stack, frame->sp + address);
    value->refs += 1;
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
    value->refs += 1;
    self->ret = value;
}

static void
VM_Psh(VM* self, int address)
{
    Value* value = Queue_Get(self->data, address);
    value->refs += 1;
    Queue_PshB(self->stack, value);
}

static void
VM_Mov(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(a->type == TYPE_CHAR && b->type == TYPE_STRING)
    {
        if(Str_Size(b->of.string) != 1)
            Quit("vm: expected char");
        *a->of.character = b->of.string->value[0];
    }
    else
    if(a->type == b->type)
    {
        Type_Kill(a->type, a->of);
        Type_Copy(a, b);
    }
    else
        Quit("vm: operator `=` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
    VM_Pop(self);
}

static void
VM_Add(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1); // PROMOTE CHAR.
    if(a->type == TYPE_CHAR)
        Quit("vm: cannot add to char");
    else
    if(a->type == TYPE_ARRAY && b->type != TYPE_ARRAY)
        Queue_PshB(a->of.array, Value_Copy(b));
    else
    {
        Value_PromoteChars(a, b);
        if(a->type == b->type)
            switch(a->type)
            {
            case TYPE_ARRAY:
                Queue_Append(a->of.array, b->of.array);
                break;
            case TYPE_DICT:
                Map_Append(a->of.dict, b->of.dict);
                break;
            case TYPE_STRING:
                Str_Appends(a->of.string, b->of.string->value);
                break;
            case TYPE_NUMBER:
                a->of.number += b->of.number;
                break;
            case TYPE_CHAR:
                break;
            case TYPE_BOOL:
            case TYPE_NULL:
                Quit("vm: operator `+` not supported for type `%s`", Type_String(a->type));
            }
        else
            Quit("vm: operator `+` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
    }
    VM_Pop(self);
}

static void
VM_Sub(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(a->type == TYPE_ARRAY && b->type != TYPE_ARRAY)
        Queue_PshF(a->of.array, Value_Copy(b));
    else
    if(a->type == b->type)
        switch(a->type)
        {
        case TYPE_ARRAY:
            Queue_Prepend(a->of.array, b->of.array);
            break;
        case TYPE_NUMBER:
            a->of.number -= b->of.number;
            break;
        case TYPE_DICT:
        case TYPE_STRING:
        case TYPE_CHAR:
        case TYPE_BOOL:
        case TYPE_NULL:
            Quit("vm: operator `-` not supported for type `%s`", Type_String(a->type));
        }
    else
        Quit("vm: operator `-` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
    VM_Pop(self);
}

static void
VM_Mul(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(a->type != b->type)
        Quit("vm: operator `*` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
    if(a->type != TYPE_NUMBER)
        Quit("vm: operator `*` not supported for type `%s`", Type_String(a->type));
    a->of.number *= b->of.number;
    VM_Pop(self);
}

static void
VM_Div(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(a->type != b->type)
        Quit("vm: operator `/` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
    if(a->type != TYPE_NUMBER)
        Quit("vm: operator `/` not supported for type `%s`", Type_String(a->type));
    a->of.number /= b->of.number;
    VM_Pop(self);
}

static void
VM_Lst(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(a->type != b->type)
        Quit("vm: operator `<` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
    if(a->type != TYPE_NUMBER)
        Quit("vm: operator `<` not supported for type `%s`", Type_String(a->type));
    bool boolean = a->of.number < b->of.number;
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
    if(a->type != b->type)
        Quit("vm: operator `<=` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
    if(a->type != TYPE_NUMBER)
        Quit("vm: operator `<=` not supported for type `%s`", Type_String(a->type));
    bool boolean = a->of.number <= b->of.number;
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
    if(a->type != b->type)
        Quit("vm: operator `>` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
    if(a->type != TYPE_NUMBER)
        Quit("vm: operator `>` not supported for type `%s`", Type_String(a->type));
    bool boolean = a->of.number > b->of.number;
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
    if(a->type != b->type)
        Quit("vm: operator `>=` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
    if(a->type != TYPE_NUMBER)
        Quit("vm: operator `>=` not supported for type `%s`", Type_String(a->type));
    bool boolean = a->of.number >= b->of.number;
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
    if(a->type != b->type)
        Quit("vm: operator `&&` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
    if(a->type != TYPE_BOOL)
        Quit("vm: operator `&&` not supported for type `%s`", Type_String(a->type));
    a->of.boolean = a->of.boolean && b->of.boolean;
    VM_Pop(self);
}

static void
VM_Lor(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(a->type != b->type)
        Quit("vm: operator `||` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
    if(a->type != TYPE_BOOL)
        Quit("vm: operator `||` not supported for type `%s`", Type_String(a->type));
    a->of.boolean = a->of.boolean || b->of.boolean;
    VM_Pop(self);
}

static void
VM_Eql(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    Value_PromoteChars(a, b);
    if(a->type != b->type)
        Quit("vm: operator `==` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
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
    if(a->type != b->type)
        Quit("vm: operator `!=` types `%s` and `%s` mismatch", Type_String(a->type), Type_String(b->type));
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
    if(value->type != TYPE_BOOL)
        Quit("vm: operator `!` not supported for type `%s`", Type_String(value->type));
    value->of.boolean = !value->of.boolean;
}

static void
VM_Brf(VM* self, int address)
{
    Value* value = Queue_Back(self->stack);
    if(value->type != TYPE_BOOL)
        Quit("vm: boolean expression expected");
    if(value->of.boolean == false)
        self->pc = address;
    VM_Pop(self);
}

static void
VM_Prt(VM* self)
{
    Value* value = Queue_Back(self->stack);
    Str* print = Value_Print(value, true, 0);
    printf("%s", print->value);
    VM_Pop(self);
    Of of = { .number = Str_Size(print) };
    Queue_PshB(self->stack, Value_Init(of, TYPE_NUMBER));
    Str_Kill(print);
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
    if(a->type != TYPE_ARRAY)
        Quit("vm: array expected for push back operand `%s`", Type_String(b->type));
    Queue_PshB(a->of.array, b);
    b->refs += 1;
    VM_Pop(self);
}

static void
VM_Psf(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(a->type != TYPE_ARRAY)
        Quit("vm: array expected for push front operand `%s`", Type_String(b->type));
    Queue_PshF(a->of.array, b);
    b->refs += 1;
    VM_Pop(self);
}

static void
VM_Ins(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 3);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* c = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(a->type != TYPE_DICT)
        Quit("vm: dict expected");
    if(b->type != TYPE_STRING)
        Quit("vm: string expected");
    Map_Set(a->of.dict, Str_Copy(b->of.string), c);
    // No need to increment the reference count of b, the string, as just
    // a copy of the string was copied for the map set.
    c->refs += 1;
    VM_Pop(self);
    VM_Pop(self);
}

static Value*
VM_Index(Value* storage, Value* index)
{
    Value* value = NULL;
    if(storage->type == TYPE_ARRAY)
    {
        value = Queue_Get(storage->of.array, index->of.number);
    }
    else
    if(storage->type == TYPE_STRING)
    {
        char* character = Str_Get(storage->of.string, index->of.number);
        if(character)
        {
            Of of = { .character = character };
            value = Value_Init(of, TYPE_CHAR);
        }
    }
    else
        Quit("vm: type `%s` cannot be indexed", Type_String(storage->type));
    // Not allowed to return null.
    if(value == NULL)
        Quit("vm: type `%s` indexed out of bounds", Type_String(storage->type));
    return value;
}

static Value*
VM_Lookup(VM* self, Value* storage, Value* key)
{
    if(storage->type != TYPE_DICT)
        Quit("vm: type `%s` cannot be looked up", Type_String(storage->type));
    Value* value = Map_Get(storage->of.dict, key->of.string->value);
    // Allowed to return null.
    return value;
}

static void
VM_Get(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    Value* value = NULL;
    if(b->type == TYPE_NUMBER)
        value = VM_Index(a, b);
    else
    if(b->type == TYPE_STRING)
        value = VM_Lookup(self, a, b);
    else
        Quit("vm: type `%s` cannot be indexed", Type_String(a->type));
    VM_Pop(self);
    VM_Pop(self);
    if(value == NULL)
    {
        Of of;
        Queue_PshB(self->stack, Value_Init(of, TYPE_NULL));
    }
    else
    {
        value->refs += 1;
        Queue_PshB(self->stack, value);
    }
}

static void
VM_Fmt(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(a->type != TYPE_STRING)
        Quit("vm: string expected");
    if(b->type != TYPE_ARRAY)
        Quit("vm: array expected");
    Str* formatted = Str_Init("");
    int index = 0;
    char* ref = a->of.string->value;
    for(char* c = ref; *c; c++)
    {
        if(*c == '{')
        {
            char next = *(c + 1);
            if(next == '}')
            {
                Value* value = Queue_Get(b->of.array, index);
                if(value == NULL)
                    Quit("vm: string formatter out of range");
                Str_Append(formatted, Value_Print(value, false, 0));
                index += 1;
                c += 1;
            }
        }
        else
            Str_PshB(formatted, *c);
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
    Of of = { .string = Str_Init(Type_String(a->type)) };
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
        if(a->type == TYPE_ARRAY)
            Queue_Del(a->of.array, b->of.number);
        else
        if(a->type == TYPE_STRING)
            Str_Del(a->of.string, b->of.number);
        else
            Quit("vm: type `%s` cannot be indexed for deletion", Type_String(a->type));
    }
    else
    if(b->type == TYPE_STRING)
    {
        if(a->type != TYPE_DICT)
            Quit("vm: type `%s` cannot be indexed for deletion", Type_String(a->type));
        Map_Del(a->of.dict, b->of.string->value);
    }
    else
        Quit("vm: type `%s` cannot have an element deleted");
    VM_Pop(self);
}

static int
VM_Run(VM* self)
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
        case OPCODE_END: return VM_End(self);
        case OPCODE_EQL: VM_Eql(self); break;
        case OPCODE_FLS: VM_Fls(self); break;
        case OPCODE_FMT: VM_Fmt(self); break;
        case OPCODE_GET: VM_Get(self); break;
        case OPCODE_GLB: VM_Glb(self, address); break;
        case OPCODE_GRT: VM_Grt(self); break;
        case OPCODE_GTE: VM_Gte(self); break;
        case OPCODE_INS: VM_Ins(self); break;
        case OPCODE_JMP: VM_Jmp(self, address); break;
        case OPCODE_LEN: VM_Len(self); break;
        case OPCODE_LOC: VM_Loc(self, address); break;
        case OPCODE_LOD: VM_Lod(self); break;
        case OPCODE_LOR: VM_Lor(self); break;
        case OPCODE_LST: VM_Lst(self); break;
        case OPCODE_LTE: VM_Lte(self); break;
        case OPCODE_MOV: VM_Mov(self); break;
        case OPCODE_MUL: VM_Mul(self); break;
        case OPCODE_NEQ: VM_Neq(self); break;
        case OPCODE_NOT: VM_Not(self); break;
        case OPCODE_POP: VM_Pop(self); break;
        case OPCODE_PRT: VM_Prt(self); break;
        case OPCODE_PSB: VM_Psb(self); break;
        case OPCODE_PSF: VM_Psf(self); break;
        case OPCODE_PSH: VM_Psh(self, address); break;
        case OPCODE_RET: VM_Ret(self); break;
        case OPCODE_SAV: VM_Sav(self); break;
        case OPCODE_SPD: VM_Spd(self); break;
        case OPCODE_SUB: VM_Sub(self); break;
        case OPCODE_TYP: VM_Typ(self); break;
        }
    }
}

int
main(int argc, char* argv[])
{
    Args args = Args_Parse(argc, argv);
    if(args.entry)
    {
        int ret = 0;
        Str* entry = Str_Init(args.entry);
        CC* cc = CC_Init();
        CC_Reserve(cc);
        CC_LoadModule(cc, entry);
        CC_Parse(cc);
        if(args.dump)
            ASM_Dump(cc->assembly);
        else
        {
            VM* vm = VM_Assemble(cc->assembly);
            ret = VM_Run(vm);
            VM_Kill(vm);
        }
        CC_Kill(cc);
        Str_Kill(entry);
        return ret;
    }
    else
    if(args.help)
        Args_Help();
    else
        Quit("usage: rr -h");
}
