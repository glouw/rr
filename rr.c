/*           _____                    _____ 
 *          /\    \                  /\    \
 *         /::\    \                /::\    \
 *        /::::\    \              /::::\    \
 *       /::::::\    \            /::::::\    \
 *      /:::/\:::\    \          /:::/\:::\    \
 *     /:::/__\:::\    \        /:::/__\:::\    \
 *    /::::\   \:::\    \      /::::\   \:::\    \
 *   /::::::\   \:::\    \    /::::::\   \:::\    \
 *  /:::/\:::\   \:::\____\  /:::/\:::\   \:::\____\
 * /:::/  \:::\   \:::|    |/:::/  \:::\   \:::|    |
 * \::/   |::::\  /:::|____|\::/   |::::\  /:::|____|
 *  \/____|:::::\/:::/    /  \/____|:::::\/:::/    /
 *        |:::::::::/    /         |:::::::::/    /
 *        |::|\::::/    /          |::|\::::/    / 
 *        |::| \::/____/           |::| \::/____/
 *        |::|  ~|                 |::|  ~|
 *        |::|   |                 |::|   |
 *        \::|   |                 \::|   |
 *         \:|   |                  \:|   |
 *          \|___|                   \|___|
 *
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

#define QUEUE_BLOCK_SIZE (8)
#define MAP_UNPRIMED (-1)
#define MODULE_BUFFER_SIZE (512)
#define STR_CAP_SIZE (16)

#define DEBUG (1)

typedef void
(*Kill)(void*);

typedef void*
(*Copy)(void*);

typedef bool
(*Equal)(void*, void*);

typedef enum
{
    FRONT, BACK
}
End;

typedef enum
{
    CLASS_VARIABLE_GLOBAL,
    CLASS_VARIABLE_LOCAL,
    CLASS_FUNCTION,
}
Class;

typedef enum
{
    STATE_NONE,
    STATE_IF,
    STATE_ELIF,
    STATE_ELSE,
}
State;

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

typedef enum Type
{
    TYPE_ARRAY,
    TYPE_DICT,
    TYPE_STRING,
    TYPE_NUMBER,
    TYPE_BOOL,
    TYPE_NULL,
}
Type;

typedef union
{
    Queue* array;
    Map* dict;
    Str* string;
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

typedef enum
{
    OPCODE_ADD,
    OPCODE_AND,
    OPCODE_BRF,
    OPCODE_CAL,
    OPCODE_CPY,
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
    OPCODE_PSB,
    OPCODE_PSF,
    OPCODE_RET,
    OPCODE_SAV,
    OPCODE_SPD,
    OPCODE_SUB,
    OPCODE_PSH,
}
Opcode;

char*
Opcode_String(Opcode oc)
{
    switch(oc)
    {
        case OPCODE_ADD: return "OPCODE_ADD";
        case OPCODE_AND: return "OPCODE_AND";
        case OPCODE_BRF: return "OPCODE_BRF";
        case OPCODE_CAL: return "OPCODE_CAL";
        case OPCODE_CPY: return "OPCODE_CPY";
        case OPCODE_DIV: return "OPCODE_DIV";
        case OPCODE_END: return "OPCODE_END";
        case OPCODE_EQL: return "OPCODE_EQL";
        case OPCODE_FLS: return "OPCODE_FLS";
        case OPCODE_FMT: return "OPCODE_FMT";
        case OPCODE_GET: return "OPCODE_GET";
        case OPCODE_GLB: return "OPCODE_GLB";
        case OPCODE_GRT: return "OPCODE_GRT";
        case OPCODE_GTE: return "OPCODE_GTE";
        case OPCODE_INS: return "OPCODE_INS";
        case OPCODE_JMP: return "OPCODE_JMP";
        case OPCODE_LOC: return "OPCODE_LOC";
        case OPCODE_LOD: return "OPCODE_LOD";
        case OPCODE_LOR: return "OPCODE_LOR";
        case OPCODE_LST: return "OPCODE_LST";
        case OPCODE_LTE: return "OPCODE_LTE";
        case OPCODE_MOV: return "OPCODE_MOV";
        case OPCODE_MUL: return "OPCODE_MUL";
        case OPCODE_NEQ: return "OPCODE_NEQ";
        case OPCODE_NOT: return "OPCODE_NOT";
        case OPCODE_POP: return "OPCODE_POP";
        case OPCODE_PSB: return "OPCODE_PSB";
        case OPCODE_PSF: return "OPCODE_PSF";
        case OPCODE_RET: return "OPCODE_RET";
        case OPCODE_SAV: return "OPCODE_SAV";
        case OPCODE_SPD: return "OPCODE_SPD";
        case OPCODE_SUB: return "OPCODE_SUB";
        case OPCODE_PSH: return "OPCODE_PSH";
    }
    return "N/A";
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
    exit(1);
}

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
Delete(Kill kill, void* value)
{
    if(kill) kill(value);
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

void
Queue_Set(Queue* self, int index, void* data)
{
    void** at = Queue_At(self, index);
    if(at)
    {
        Delete(self->kill, *at);
        *at = data;
    }
}

void*
Queue_Get(Queue* self, int index)
{
    void** at = Queue_At(self, index);
    return at ? *at : NULL;
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

void
Queue_Del(Queue* self, int index)
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

void
Str_Alloc(Str* self, int cap)
{
    self->cap = cap;
    self->value = Realloc(self->value, (1 + cap) * sizeof(*self->value));
}

Str*
Str_Init(char* string)
{
    Str* self = Malloc(sizeof(*self));
    self->value = NULL;
    self->size = strlen(string);
    Str_Alloc(self, self->size < STR_CAP_SIZE ? STR_CAP_SIZE : self->size);
    strcpy(self->value, string);
    return self;
}

void
Str_Kill(Str* self)
{
    Delete(Free, self->value);
    Free(self);
}

void
Str_Swap(Str** self, Str** other)
{
    Str* temp = *self;
    *self = *other;
    *other = temp;
}

Str*
Str_Copy(Str* self)
{
    return Str_Init(self->value);
}

int
Str_Size(Str* self)
{
    return self->size;
}

char*
Str_End(Str* self)
{
    return &self->value[Str_Size(self) - 1];
}

char
Str_Back(Str* self)
{
    return *Str_End(self);
}

char*
Str_Begin(Str* self)
{
    return &self->value[0];
}

char
Str_Front(Str* self)
{
    return *Str_Begin(self);
}

int
Str_Empty(Str* self)
{
    return Str_Size(self) == 0;
}

void
Str_PshB(Str* self, char ch)
{
    if(Str_Size(self) == self->cap)
        Str_Alloc(self, (self->cap == 0) ? 1 : (2 * self->cap));
    self->value[self->size + 0] = ch;
    self->value[self->size + 1] = '\0';
    self->size += 1;
}

void
Str_PopB(Str* self)
{
    self->size -= 1;
    self->value[self->size] = '\0';
}

void
Str_Clear(Str* self)
{
    while(Str_Size(self) > 0)
        Str_PopB(self);
}

void
Str_Resize(Str* self, char ch, int size)
{
    if(self->size < size)
    {
        Str_Alloc(self, size);
        while(self->size < size)
            Str_PshB(self, ch);
    }
    else
        while(self->size > size)
            Str_PopB(self);
}

bool
Equals(char* a, char* b)
{
    return strcmp(a, b) == 0;
}

bool
Str_Equals(Str* a, char* b)
{
    return Equals(a->value, b);
}

bool
Str_Equal(Str* a, Str* b)
{
    return Equals(a->value, b->value);
}

void
Str_Replace(Str* self, char a, char b)
{
    for(int i = 0; i < self->size; i++)
        if(self->value[i] == a)
            self->value[i] = b;
}

void
Str_Appends(Str* self, char* str)
{
    while(*str)
    {
        Str_PshB(self, *str);
        str += 1;
    }
}

void
Str_Prepends(Str* self, Str* other)
{
    Str* copy = Str_Copy(self);
    Str_Clear(self);
    Str_Appends(self, other->value);
    Str_Appends(self, copy->value);
    Str_Kill(copy);
}

Str*
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

Queue*
Str_Split(Str* path, char* delim)
{
    Str* buffer = Str_Copy(path);
    Queue* toks = Queue_Init((Kill) Str_Kill, NULL);
    while(true)
    {
        char* tok = strtok(Queue_Empty(toks) ? buffer->value : NULL, delim);
        if(tok == NULL)
            break;
        Queue_PshB(toks, Str_Init(tok));
    }
    Str_Kill(buffer); 
    return toks;
}

Str*
Str_Moves(char** from)
{
    Str* self = Malloc(sizeof(*self));
    self->value = *from;
    self->size = strlen(self->value);
    self->cap = self->size;
    *from = NULL;
    return self;
}

Str*
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

bool
Str_IsBoolean(Str* ident)
{
    return Str_Equals(ident, "true") || Str_Equals(ident, "false");
}

bool
Str_IsNull(Str* ident)
{
    return Str_Equals(ident, "null");
}

Node*
Node_Init(Str* key, void* value)
{
    Node* self = Malloc(sizeof(*self));
    self->next = NULL;
    self->key = key;
    self->value = value;
    return self;
}

void
Node_Set(Node* self, Kill kill, Str* key, void* value)
{
    Delete((Kill) Str_Kill, self->key);
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
    void* value = copy ? copy(self->value) : self->value;
    return Node_Init(Str_Copy(self->value), value);
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
    return Map_Primes[self->prime_index];
}

bool
Map_Resizable(Map* self)
{
    int size = sizeof(Map_Primes) / sizeof(Map_Primes[0]);
    return self->prime_index < size - 1;
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
    if(!Map_Empty(self))
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
    }
    Free(self->bucket);
    Free(self);
}

unsigned
Map_Hash(char* key)
{
    unsigned hash = 5381;
    char ch = 0;
    while((ch = *key++))
        hash = ((hash << 5) + hash) + ch;
    return hash;
}

Node**
Map_Bucket(Map* self, char* key)
{
    int index = Map_Hash(key) % Map_Buckets(self);
    return &self->bucket[index];
}

void
Map_Alloc(Map* self, int index)
{
    self->prime_index = index;
    self->bucket = calloc(Map_Buckets(self), sizeof(*self->bucket));
}

void
Map_Emplace(Map*, Str*, Node*);

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

Node*
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

void
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
            Map_Emplace(copy, chain->key, node);
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
            Map_Set(self, Str_Copy(chain->key), self->copy ? self->copy(chain->value) : chain->value);
            chain = chain->next;
        }
    }
}

bool
Map_Equal(Map* self, Map* other, Equal equal)
{
    if(Map_Size(self) != Map_Size(other))
        return false;
    else
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

char*
Type_String(Type self)
{
    switch(self)
    {
        case TYPE_ARRAY:
            return "TYPE_ARRAY";
        case TYPE_DICT:
            return "TYPE_DICT";
        case TYPE_STRING:
            return "TYPE_STRING";
        case TYPE_NUMBER:
            return "TYPE_NUMBER";
        case TYPE_BOOL:
            return "TYPE_BOOL";
        case TYPE_NULL:
            return "TYPE_NULL";
    }
    return "N/A";
}

char*
Class_String(Class self)
{
    switch(self)
    {
        case CLASS_VARIABLE_LOCAL:
            return "CLASS_VARIABLE_LOCAL";
        case CLASS_VARIABLE_GLOBAL:
            return "CLASS_VARIABLE_GLOBAL";
        case CLASS_FUNCTION:
            return "CLASS_FUNCTION";
    }
    return "N/A";
}

Meta*
Meta_Init(Class class, int stack)
{
    Meta* self = Malloc(sizeof(*self));
    self->class = class;
    self->stack = stack;
    return self;
}

void
Meta_Kill(Meta* self)
{
    Free(self);
}

Str*
CC_Format(char* format, ...)
{
    va_list args;
    va_start(args, format);
    int size = vsnprintf(NULL, 0, format, args);
    va_end(args);
    va_start(args, format);
    Str* line = Str_Init("");
    Str_Resize(line, '\0', size + 1);
    vsprintf(line->value, format, args);
    va_end(args);
    return line;
}

void
CC_Quit(CC* self, const char* const message, ...)
{
    Module* back = Queue_Back(self->modules);
    va_list args;
    va_start(args, message);
    fprintf(stderr, "compilation error: file `%s`: line `%d`: ", back ? back->name->value : "?", back ? back->line : 0);
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(2);
}

void
Module_Buffer(Module* self)
{
    self->index = 0;
    self->size = fread(self->buffer, sizeof(*self->buffer), MODULE_BUFFER_SIZE, self->file);
}

Module*
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

void
Module_Kill(Module* self)
{
    Str_Kill(self->name);
    fclose(self->file);
    Free(self);
}

int
Module_Size(Module* self)
{
    return self->size;
}

int
Module_Empty(Module* self)
{
    return Module_Size(self) == 0;
}

int
Module_At(Module* self)
{
    return self->buffer[self->index];
}

int
Module_Peak(Module* self)
{
    if(self->index == self->size)
        Module_Buffer(self);
    return Module_Empty(self) ? EOF : Module_At(self);
}

void
Module_Advance(Module* self)
{
    int at = Module_At(self);
#if DEBUG
    putchar(at);
#endif
    if(at == '\n')
        self->line += 1;
    self->index += 1;
}

void
CC_LoadModule(CC* self, Str* file)
{
    Str* path;
    if(Queue_Empty(self->modules))
    {
        path = Str_Init("");
        Str_Resize(path, '\0', 4096);
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

void
CC_Advance(CC* self)
{
    Module_Advance(Queue_Back(self->modules));
}

bool
CC_IsLower(int c)
{
    return c >= 'a' && c <= 'z';
}

bool
CC_IsUpper(int c)
{
    return c >= 'A' && c <= 'Z';
}

bool
CC_IsAlpha(int c)
{
    return CC_IsLower(c) || CC_IsUpper(c);
}

bool
CC_IsDigit(int c)
{
    return c >= '0' && c <= '9';
}

bool
CC_IsNumber(int c)
{
    return CC_IsDigit(c) || c == '.';
}

bool
CC_IsAlnum(int c)
{
    return CC_IsAlpha(c) || CC_IsDigit(c);
}

bool
CC_IsIdentLeader(int c)
{
    return CC_IsAlpha(c) || c == '_';
}

bool
CC_IsIdent(int c)
{
    return CC_IsIdentLeader(c) || CC_IsDigit(c);
}

bool
CC_IsMod(int c)
{
    return CC_IsIdent(c) || c == '.';
}

bool
CC_IsOp(int c)
{
    switch(c)
    {
        case '*':
        case '/':
        case '%':
        case '+':
        case '-':
        case '=':
        case '<':
        case '>':
        case '!':
        case '&':
        case '|':
            return true;
    }
    return false;
}

bool
CC_IsEsc(int c)
{
    switch(c)
    {
        case '"':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
        case 'u':
        case '\\':
            return true;
    }
    return false;
}

bool
CC_IsSpace(int c)
{
    switch(c)
    {
        case '\n':
        case '\t':
        case '\r':
        case ' ':
            return true;
    }
    return false;
}

int
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

void
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

int
CC_Next(CC* self)
{
    CC_Spin(self);
    return CC_Peak(self);
}

int
CC_Read(CC* self)
{
    int peak = CC_Peak(self);
    if(peak != EOF)
        CC_Advance(self);
    return peak;
}

Str*
CC_Stream(CC* self, bool clause(int))
{
    Str* str = Str_Init("");
    CC_Spin(self);
    while(clause(CC_Peak(self)))
        Str_PshB(str, CC_Read(self));
    return str;
}

void
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

Str*
CC_Mod(CC* self)
{
    return CC_Stream(self, CC_IsMod);
}

Str*
CC_Ident(CC* self)
{
    return CC_Stream(self, CC_IsIdent);
}

Str*
CC_Operator(CC* self)
{
    return CC_Stream(self, CC_IsOp);
}

Str*
CC_Number(CC* self)
{
    return CC_Stream(self, CC_IsNumber);
}

Str*
CC_String(CC* self)
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

CC*
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

void
CC_Kill(CC* self)
{
    Queue_Kill(self->modules);
    Queue_Kill(self->assembly);
    Queue_Kill(self->data);
    Map_Kill(self->included);
    Map_Kill(self->identifiers);
    Free(self);
}

Str*
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

void
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

bool
CC_IsGlobal(Class class)
{
    return class == CLASS_VARIABLE_GLOBAL;
}

bool
CC_IsLocal(Class class)
{
    return class == CLASS_VARIABLE_LOCAL;
}

bool
CC_IsVariable(Class class)
{
    return CC_IsGlobal(class) || CC_IsLocal(class);
}

bool
CC_IsFunction(Class class)
{
    return class == CLASS_FUNCTION;
}

void
CC_Declare(CC* self, Class class, int stack, Str* ident)
{
    if(Map_Exists(self->identifiers, ident->value))
        CC_Quit(self, "`%s` already defined", ident->value);
    Map_Set(self->identifiers, ident, Meta_Init(class, stack));
}

void
CC_Expression(CC*);

void
CC_EmptyExpression(CC* self)
{
    CC_Expression(self);
    CC_Match(self, ";");
    Queue_PshB(self->assembly, CC_Format("\tpop"));
}

void
CC_Assign(CC* self)
{
    CC_Match(self, ":=");
    CC_Expression(self);
    Queue_PshB(self->assembly, CC_Format("\tcpy"));
    CC_Match(self, ";");
}

void
CC_Local(CC* self, Str* ident, bool assign)
{
    if(assign)
        CC_Assign(self);
    CC_Declare(self, CLASS_VARIABLE_LOCAL, self->locals, ident);
    self->locals += 1;
}

Str*
CC_Global(CC* self, Str* ident)
{
    Str* label = CC_Format("!%s", ident->value);
    Queue_PshB(self->assembly, CC_Format("%s:", label->value));
    CC_Assign(self);
    CC_Declare(self, CLASS_VARIABLE_GLOBAL, self->globals, ident);
    Queue_PshB(self->assembly, CC_Format("\tret"));
    self->globals += 1;
    return label;
}

Queue*
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

int
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
        Queue_PshB(self->assembly, CC_Format("\tpop"));
    Queue_Kill(scope);
    return popped;
}

Meta*
CC_Expect(CC* self, Str* ident, bool clause(Class))
{
    Meta* meta = Map_Get(self->identifiers, ident->value);
    if(meta == NULL)
        CC_Quit(self, "identifier `%s` not defined", ident->value);
    if(!clause(meta->class))
        CC_Quit(self, "identifier `%s` is `%s`", ident->value, Class_String(meta->class));
    return meta;
}

void
CC_Ref(CC* self, Str* ident)
{
    Meta* meta = CC_Expect(self, ident, CC_IsVariable);
    if(meta->class == CLASS_VARIABLE_GLOBAL)
        Queue_PshB(self->assembly, CC_Format("\tglb %d", meta->stack));
    else
    if(meta->class == CLASS_VARIABLE_LOCAL)
        Queue_PshB(self->assembly, CC_Format("\tloc %d", meta->stack));
}

bool
CC_Term(CC*);

void
CC_Pass(CC* self)
{
    if(CC_Next(self) == '&')
    {
        CC_Match(self, "&");
        Str* ident = CC_Ident(self);
        CC_Ref(self, ident);
        Str_Kill(ident);
    }
    else
    {
        CC_Expression(self);
        Queue_PshB(self->assembly, CC_Format("\tcpy"));
    }
}

void
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

bool
CC_Primed(CC* self)
{
    return self->prime != NULL;
}

bool
CC_Factor(CC* self)
{
    bool storage = false;
    int next = CC_Next(self);
    if(next == '!')
    {
        CC_Match(self, "!");
        CC_Factor(self);
        Queue_PshB(self->assembly, CC_Format("\tnot"));
    }
    else
    if(CC_IsDigit(next))
    {
        Str* number = CC_Number(self);
        Queue_PshB(self->assembly, CC_Format("\tpsh %s", number->value));
        Str_Kill(number);
    }
    else
    if(CC_IsIdent(next)
    || CC_Primed(self))
    {
        Str* ident = NULL;
        if(CC_Primed(self))
            Str_Swap(&self->prime, &ident);
        else
            ident = CC_Ident(self);
        if(Str_IsBoolean(ident))
            Queue_PshB(self->assembly, CC_Format("\tpsh %s", ident->value));
        else
        if(Str_IsNull(ident))
            Queue_PshB(self->assembly, CC_Format("\tpsh %s", ident->value));
        else
        if(CC_Next(self) == '(')
        {
            Meta* meta = CC_Expect(self, ident, CC_IsFunction);
            CC_Args(self, meta->stack);
            for(int i = 0; i < meta->stack; i++)
                Queue_PshB(self->assembly, CC_Format("\tspd"));
            Queue_PshB(self->assembly, CC_Format("\tcal %s", ident->value));
            Queue_PshB(self->assembly, CC_Format("\tlod"));
        }
        else
        {
            CC_Ref(self, ident);
            storage = true;
        }
        Str_Kill(ident);
    }
    else
    if(next == '(')
    {
        CC_Match(self, "(");
        CC_Expression(self);
        CC_Match(self, ")");
    }
    else
    if(next == '{')
    {
        Queue_PshB(self->assembly, CC_Format("\tpsh {}"));
        CC_Match(self, "{");
        while(CC_Next(self) != '}')
        {
            CC_Expression(self);
            CC_Match(self, ":");
            CC_Expression(self);
            Queue_PshB(self->assembly, CC_Format("\tins"));
            if(CC_Next(self) == ',')
                CC_Match(self, ",");
        }
        CC_Match(self, "}");
    }
    else
    if(next == '[')
    {
        Queue_PshB(self->assembly, CC_Format("\tpsh []"));
        CC_Match(self, "[");
        while(CC_Next(self) != ']')
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, CC_Format("\tpsb"));
            if(CC_Next(self) == ',')
                CC_Match(self, ",");
        }
        CC_Match(self, "]");
    }
    else
    if(next == '"')
    {
        Str* string = CC_String(self);
        Queue_PshB(self->assembly, CC_Format("\tpsh \"%s\"", string->value));
        Str_Kill(string);
    }
    else
        CC_Quit(self, "unknown factor starting with `%c`", next);
    while(CC_Next(self) == '[')
    {
        CC_Match(self, "[");
        CC_Expression(self);
        CC_Match(self, "]");
        Queue_PshB(self->assembly, CC_Format("\tget"));
    }
    return storage;
}

bool
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
            Queue_PshB(self->assembly, CC_Format("\tmul"));
        }
        else
        if(Str_Equals(operator, "/="))
        {
            if(!storage)
                CC_Quit(self, "factor - left hand side must be storage");
            CC_Expression(self);
            Queue_PshB(self->assembly, CC_Format("\tdiv"));
        }
        else
        {
            storage = false;
            Queue_PshB(self->assembly, CC_Format("\tcpy"));
            CC_Factor(self);
            if(Str_Equals(operator, "*"))
                Queue_PshB(self->assembly, CC_Format("\tmul"));
            else
            if(Str_Equals(operator, "/"))
                Queue_PshB(self->assembly, CC_Format("\tdiv"));
            else
            if(Str_Equals(operator, "%"))
                Queue_PshB(self->assembly, CC_Format("\tfmt"));
            else
            if(Str_Equals(operator, "||"))
                Queue_PshB(self->assembly, CC_Format("\tlor"));
            else
                CC_Quit(self, "operator `%s` not supported", operator->value);
        }
        Str_Kill(operator);
    }
    return storage;
}

void
CC_Expression(CC* self)
{
    bool storage = CC_Term(self);
    while(CC_Next(self) == '+' 
       || CC_Next(self) == '-'
       || CC_Next(self) == '='
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
            Queue_PshB(self->assembly, CC_Format("\tmov"));
        }
        else
        if(Str_Equals(operator, "+="))
        {
            if(!storage)
                CC_Quit(self, "term - left hand side must be storage");
            CC_Expression(self);
            Queue_PshB(self->assembly, CC_Format("\tadd"));
        }
        else
        if(Str_Equals(operator, "-="))
        {
            if(!storage)
                CC_Quit(self, "term - left hand side must be storage");
            CC_Expression(self);
            Queue_PshB(self->assembly, CC_Format("\tsub"));
        }
        else
        if(Str_Equals(operator, "=="))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, CC_Format("\teql"));
        }
        else
        if(Str_Equals(operator, "!="))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, CC_Format("\tneq"));
        }
        else
        if(Str_Equals(operator, ">"))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, CC_Format("\tgrt"));
        }
        else
        if(Str_Equals(operator, "<"))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, CC_Format("\tlst"));
        }
        else
        if(Str_Equals(operator, ">="))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, CC_Format("\tgte"));
        }
        else
        if(Str_Equals(operator, "<="))
        {
            CC_Expression(self);
            Queue_PshB(self->assembly, CC_Format("\tlte"));
        }
        else
        {
            storage = false;
            Queue_PshB(self->assembly, CC_Format("\tcpy"));
            CC_Term(self);
            if(Str_Equals(operator, "+"))
                Queue_PshB(self->assembly, CC_Format("\tadd"));
            else
            if(Str_Equals(operator, "-"))
                Queue_PshB(self->assembly, CC_Format("\tsub"));
            else
            if(Str_Equals(operator, "&&"))
                Queue_PshB(self->assembly, CC_Format("\tand"));
            else
                CC_Quit(self, "operator `%s` not supported", operator->value);
        }
        Str_Kill(operator);
    }
}

void
CC_Block(CC* self, int head, int tail)
{
    Queue* scope = Queue_Init((Kill) Str_Kill, (Copy) NULL);
    State chain = STATE_NONE;
    int entry = self->labels + 0;
    int final = self->labels + 1;
    self->labels += 2;
    CC_Match(self, "{");
    while(CC_Next(self) != '}')
    {
        if(CC_IsIdentLeader(CC_Next(self)))
        {
            Str* ident = CC_Ident(self);
            if(Str_Equals(ident, "if"))
            {
                CC_Match(self, "(");
                CC_Expression(self);
                CC_Match(self, ")");
                Queue_PshB(self->assembly, CC_Format("\tbrf @l%d", final));
                CC_Block(self, head, tail);
                Queue_PshB(self->assembly, CC_Format("@l%d:", final));
                Str_Kill(ident);
                chain = STATE_IF;
            }
            else
            if(Str_Equals(ident, "elif"))
            {
                if(chain == STATE_IF
                || chain == STATE_ELIF)
                {
                    CC_Match(self, "(");
                    CC_Expression(self);
                    CC_Match(self, ")");
                    Queue_PshB(self->assembly, CC_Format("\tbrf @l%d", final));
                    CC_Block(self, head, tail);
                    Queue_PshB(self->assembly, CC_Format("@l%d:", final));
                    Str_Kill(ident);
                }
                else
                    CC_Quit(self, "`elif` must follow either `if` or `elif`");
                chain = STATE_ELIF;
            }
            else
            if(Str_Equals(ident, "else"))
            {
                if(chain == STATE_IF
                || chain == STATE_ELIF)
                {
                    CC_Block(self, head, tail);
                    Str_Kill(ident);
                }
                else
                    CC_Quit(self, "`else` must follow either `if` or `elif`");
                chain = STATE_ELSE;
            }
            else
            if(Str_Equals(ident, "while"))
            {
                Queue_PshB(self->assembly, CC_Format("@l%d:", entry));
                CC_Match(self, "(");
                CC_Expression(self);
                Queue_PshB(self->assembly, CC_Format("\tbrf @l%d", final));
                CC_Match(self, ")");
                CC_Block(self, entry, final);
                Queue_PshB(self->assembly, CC_Format("\tjmp @l%d", entry));
                Queue_PshB(self->assembly, CC_Format("@l%d:", final));
                Str_Kill(ident);
                chain = STATE_NONE;
            }
            else
            if(Str_Equals(ident, "ret"))
            {
                CC_Expression(self);
                Queue_PshB(self->assembly, CC_Format("\tsav"));
                Queue_PshB(self->assembly, CC_Format("\tfls"));
                CC_Match(self, ";");
                Str_Kill(ident);
                chain = STATE_NONE;
            }
            else
            if(Str_Equals(ident, "continue"))
            {
                if(head == 0)
                    CC_Quit(self, "`continue` can only be used within while loops");
                CC_Match(self, ";");
                Queue_PshB(self->assembly, CC_Format("\tjmp @l%d", head));
                Str_Kill(ident);
                chain = STATE_NONE;
            }
            else
            if(Str_Equals(ident, "break"))
            {
                if(tail == 0)
                    CC_Quit(self, "`break` can only be used within while loops");
                CC_Match(self, ";");
                Queue_PshB(self->assembly, CC_Format("\tjmp @l%d", tail));
                Str_Kill(ident);
                chain = STATE_NONE;
            }
            else
            if(CC_Next(self) == ':')
            {
                Queue_PshB(scope, ident);
                CC_Local(self, Str_Copy(ident), true);
                chain = STATE_NONE;
            }
            else
            {
                self->prime = ident;
                CC_EmptyExpression(self);
                chain = STATE_NONE;
            }
        }
        else
        {
            CC_EmptyExpression(self);
            chain = STATE_NONE;
        }
    }
    CC_Match(self, "}");
    CC_PopScope(self, scope);
}

void
CC_Reserve(CC* self)
{
    CC_Declare(self, CLASS_FUNCTION, 1, Str_Init("len"));
    CC_Declare(self, CLASS_FUNCTION, 2, Str_Init("del"));
    CC_Declare(self, CLASS_FUNCTION, 1, Str_Init("assert"));
    CC_Declare(self, CLASS_FUNCTION, 1, Str_Init("print"));
}

void
CC_Function(CC* self, Str* ident)
{
    Queue* params = CC_Params(self);
    CC_Declare(self, CLASS_FUNCTION, Queue_Size(params), ident);
    Queue_PshB(self->assembly, CC_Format("%s:", ident->value));
    CC_Block(self, 0, 0);
    CC_PopScope(self, params);
    Queue_PshB(self->assembly, CC_Format("\tpsh null"));
    Queue_PshB(self->assembly, CC_Format("\tsav"));
    Queue_PshB(self->assembly, CC_Format("\tret"));
}

void
CC_Spool(CC* self, Queue* start)
{
    Queue* spool = Queue_Init((Kill) Str_Kill, NULL);
    Str* main = Str_Init("main");
    CC_Expect(self, main, CC_IsFunction);
    Queue_PshB(spool, CC_Format("!start:"));
    for(int i = 0; i < Queue_Size(start); i++)
    {
        Str* label = Queue_Get(start, i);
        Queue_PshB(spool, CC_Format("\tcal %s", label->value));
    }
    Queue_PshB(spool, CC_Format("\tcal %s", main->value));
    Queue_PshB(spool, CC_Format("\tend"));
    for(int i = Queue_Size(spool) - 1; i >= 0; i--)
        Queue_PshF(self->assembly, Str_Copy(Queue_Get(spool, i)));
    Str_Kill(main);
    Queue_Kill(spool);
}

void
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

void
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
        case TYPE_BOOL:
        case TYPE_NULL:
            break;
    }
}

void
Value_Kill(Value* self)
{
    Type_Kill(self->type, self->of);
    Free(self);
}

bool
Value_Equal(Value* a, Value* b)
{
    if(a->type != b->type)
        return false;
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
        case TYPE_NULL:
            return b->type == TYPE_NULL;
    }
    return false;
}

void
Value_Print(Value* self)
{
    switch(self->type)
    {
        case TYPE_ARRAY:
            printf("[]\n");
            break;
        case TYPE_DICT:
            printf("{}\n");
            break;
        case TYPE_STRING:
            printf("%s\n", self->of.string->value);
            break;
        case TYPE_NUMBER:
            printf("%f\n", self->of.number);
            break;
        case TYPE_BOOL:
            printf("%s\n", self->of.boolean ? "true" : "false");
            break;
        case TYPE_NULL:
            printf("null\n");
            break;
    }
}

Value*
Value_Copy(Value*);

Value*
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
        case TYPE_NULL:
            break;
    }
    return self;
}

void
Type_Copy(Value* a, Value* b)
{
    switch(a->type)
    {
        case TYPE_ARRAY:
            a->of.array = Queue_Copy(b->of.array);
            break;
        case TYPE_DICT:
            a->of.dict = Map_Copy(b->of.dict);
            break;
        case TYPE_STRING:
            a->of.string = Str_Copy(b->of.string);
            break;
        case TYPE_NUMBER:
        case TYPE_BOOL:
        case TYPE_NULL:
            a->of = b->of;
    }
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

int*
Int_Init(int value)
{
    int* self = Malloc(sizeof(*self));
    *self = value;
    return self;
}

void
Int_Kill(int* self)
{
    Free(self);
}

Map*
ASM_Label(Queue* assembly, int* size)
{
    Map* labels = Map_Init((Kill) Free, (Copy) NULL);
    for(int i = 0; i < Queue_Size(assembly); i++)
    {
        Str* line = Str_Copy(Queue_Get(assembly, i));
        if(line->value[0] == '\t') // Instruction.
            *size += 1;
        else // Label.
        {
            char* label = strtok(line->value, ":");
            int* at = Int_Init(*size);
            Map_Set(labels, Str_Init(label), at);
        }
        Str_Kill(line);
    }
    return labels;
}

void
ASM_Dump(Queue* assembly)
{
    for(int i = 0; i < Queue_Size(assembly); i++)
    {
        Str* assem = Queue_Get(assembly, i);
        puts(assem->value);
    }
}

Frame*
Frame_Init(int pc, int sp)
{
    Frame* self = Malloc(sizeof(*self));
    self->pc = pc;
    self->sp = sp;
    return self;
}

void
Frame_Free(Frame* self)
{
    Free(self);
}

VM*
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

void
VM_Kill(VM* self)
{
    Queue_Kill(self->data);
    Queue_Kill(self->stack);
    Queue_Kill(self->frame);
    Map_Kill(self->track);
    Value_Kill(self->ret);
    Free(self->instructions);
    Free(self);
}

void
VM_Data(VM* self)
{
    fprintf(stderr, ".data:\n");
    for(int i = 0; i < Queue_Size(self->data); i++)
    {
        Value* value = Queue_Get(self->data, i);
        if(value->refs > 0)
            Quit("internal: data segment contained references at time of exit");
        fprintf(stderr, "%4d : ", i);
        Value_Print(value);
    }
}

void
VM_Text(VM* self)
{
    fprintf(stderr, ".text:\n");
    for(int i = 0; i < self->size; i++)
        fprintf(stderr, "%4d : 0x%08lX\n", i, self->instructions[i]);
}

void
VM_Pop(VM* self)
{
    Value* value = Queue_Back(self->stack);
    if(value->refs == 0)
        Queue_PopB(self->stack);
    else
    {
        value->refs -= 1;
        Queue_PopBSoft(self->stack);
    }
}

void
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
        of.string = Str_Init(operator);
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
        Quit("unknown psh operand");
    Queue_PshB(self->data, value);
}

int
Datum(VM* self, char* operator)
{
    VM_Store(self, operator);
    return ((Queue_Size(self->data) - 1) << 8) | OPCODE_PSH;
}

int
Indirect(Opcode oc, Map* labels, char* label)
{
    return (*(int*) Map_Get(labels, label) << 8) | oc;
}

int
Direct(Opcode oc, char* number)
{
    return (atoi(number) << 8) | oc;
}

VM*
VM_Assemble(Queue* assembly)
{
    int size = 0;
    Map* labels = ASM_Label(assembly, &size);
    VM* self = VM_Init(size);
    int pc = 0;
    for(int i = 0; i < Queue_Size(assembly); i++)
    {
        Str* line = Str_Copy(Queue_Get(assembly, i));
        if(line->value[0] == '\t')
        {
            int instruction = 0;
            char* mnemonic = strtok(line->value, " \t\n");
            char* operator = strtok(NULL, "\n");
            if(Equals(mnemonic, "add"))
                instruction = OPCODE_ADD;
            else
            if(Equals(mnemonic, "and"))
                instruction = OPCODE_AND;
            else
            if(Equals(mnemonic, "brf"))
                instruction = Indirect(OPCODE_BRF, labels, operator);
            else
            if(Equals(mnemonic, "cal"))
                instruction = Indirect(OPCODE_CAL, labels, operator);
            else
            if(Equals(mnemonic, "cpy"))
                instruction = OPCODE_CPY;
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
                instruction = Direct(OPCODE_GLB, operator);
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
                instruction = Indirect(OPCODE_JMP, labels, operator);
            else
            if(Equals(mnemonic, "loc"))
                instruction = Direct(OPCODE_LOC, operator);
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
            if(Equals(mnemonic, "psb"))
                instruction = OPCODE_PSB;
            else
            if(Equals(mnemonic, "psf"))
                instruction = OPCODE_PSF;
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
            if(Equals(mnemonic, "psh"))
                instruction = Datum(self, operator);
            else
                Quit("unknown mnemonic `%s`", mnemonic);
            self->instructions[pc] = instruction;
            pc += 1;
        }
        Str_Kill(line);
    }
    Map_Kill(labels);
    return self;
}

void
VM_Lod(VM* self)
{
    Queue_PshB(self->stack, Value_Copy(self->ret));
    Value_Kill(self->ret);
}

void
VM_Cal(VM* self, int address)
{
    int sp = Queue_Size(self->stack) - self->spds;
    Queue_PshB(self->frame, Frame_Init(self->pc, sp));
    self->pc = address;
    self->spds = 0;
}

void
VM_Cpy(VM* self)
{
    Value* value = Value_Copy(Queue_Back(self->stack));
    VM_Pop(self);
    Queue_PshB(self->stack, value);
}

int
VM_End(VM* self)
{
    if(self->ret->type != TYPE_NUMBER)
        Quit("`main` return type was type `%s`; expected `%s`", Type_String(self->ret->type), Type_String(TYPE_NUMBER));
    return self->ret->of.number;
}

void
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

void
VM_Glb(VM* self, int address)
{
    Value* value = Queue_Get(self->stack, address);
    value->refs += 1;
    Queue_PshB(self->stack, value);
}

void
VM_Loc(VM* self, int address)
{
    Frame* frame = Queue_Back(self->frame);
    Value* value = Queue_Get(self->stack, frame->sp + address);
    value->refs += 1;
    Queue_PshB(self->stack, value);
}

void
VM_Jmp(VM* self, int address)
{
    self->pc = address;
}

void
VM_Ret(VM* self)
{
    Frame* frame = Queue_Back(self->frame);
    self->pc = frame->pc;
    Queue_PopB(self->frame);
}

void
VM_Sav(VM* self)
{
    self->ret = Value_Copy(Queue_Back(self->stack));
}

void
VM_Psh(VM* self, int address)
{
    Value* value = Queue_Get(self->data, address);
    value->refs += 1;
    Queue_PshB(self->stack, value);
}

Head
VM_Head(VM* self)
{
    Head head;
    head.a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    head.b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(head.a->type != head.b->type)
        Quit("operator types `%s` and `%s` mismatch", Type_String(head.a->type), Type_String(head.b->type));
    return head;
}

void
VM_Mov(VM* self)
{
    Head head = VM_Head(self);
    Type_Kill(head.a->type, head.a->of);
    Type_Copy(head.a, head.b);
    VM_Pop(self);
}

void
VM_Add(VM* self)
{
    Head head = VM_Head(self);
    switch(head.a->type)
    {
        case TYPE_ARRAY:
            Queue_Append(head.a->of.array, head.b->of.array);
            break;
        case TYPE_DICT:
            Map_Append(head.a->of.dict, head.b->of.dict);
            break;
        case TYPE_STRING:
            Str_Appends(head.a->of.string, head.b->of.string->value);
            break;
        case TYPE_NUMBER:
            head.a->of.number += head.b->of.number;
            break;
        case TYPE_BOOL:
        case TYPE_NULL:
            Quit("operator `+` not supported for type `%s`", Type_String(head.a->type));
    }
    VM_Pop(self);
}

void
VM_Sub(VM* self)
{
    Head head = VM_Head(self);
    switch(head.a->type)
    {
        case TYPE_ARRAY:
            Queue_Prepend(head.a->of.array, head.b->of.array);
            break;
        case TYPE_STRING:
            Str_Prepends(head.a->of.string, head.b->of.string);
            break;
        case TYPE_NUMBER:
            head.a->of.number -= head.b->of.number;
            break;
        case TYPE_DICT:
        case TYPE_BOOL:
        case TYPE_NULL:
            Quit("operator `-` not supported for type `%s`", Type_String(head.a->type));
    }
    VM_Pop(self);
}

void
VM_Mul(VM* self)
{
    Head head = VM_Head(self);
    if(head.a->type != TYPE_NUMBER)
        Quit("operator `*` not supported for type `%s`", Type_String(head.a->type));
    head.a->of.number *= head.b->of.number;
    VM_Pop(self);
}

void
VM_Div(VM* self)
{
    Head head = VM_Head(self);
    if(head.a->type != TYPE_NUMBER)
        Quit("operator `/` not supported for type `%s`", Type_String(head.a->type));
    head.a->of.number /= head.b->of.number;
    VM_Pop(self);
}

void
VM_Lst(VM* self)
{
    Head head = VM_Head(self);
    if(head.a->type != TYPE_NUMBER)
        Quit("operator `<` not supported for type `%s`", Type_String(head.a->type));
    bool boolean = head.a->of.number < head.b->of.number;
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

void
VM_Lte(VM* self)
{
    Head head = VM_Head(self);
    if(head.a->type != TYPE_NUMBER)
        Quit("operator `<=` not supported for type `%s`", Type_String(head.a->type));
    bool boolean = head.a->of.number <= head.b->of.number;
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

void
VM_Grt(VM* self)
{
    Head head = VM_Head(self);
    if(head.a->type != TYPE_NUMBER)
        Quit("operator `>` not supported for type `%s`", Type_String(head.a->type));
    bool boolean = head.a->of.number > head.b->of.number;
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

void
VM_Gte(VM* self)
{
    Head head = VM_Head(self);
    if(head.a->type != TYPE_NUMBER)
        Quit("operator `>=` not supported for type `%s`", Type_String(head.a->type));
    bool boolean = head.a->of.number >= head.b->of.number;
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

void
VM_And(VM* self)
{
    Head head = VM_Head(self);
    if(head.a->type != TYPE_BOOL)
        Quit("operator `&&` not supported for type `%s`", Type_String(head.a->type));
    head.a->of.boolean = head.a->of.boolean && head.b->of.boolean;
    VM_Pop(self);
}

void
VM_Lor(VM* self)
{
    Head head = VM_Head(self);
    if(head.a->type != TYPE_BOOL)
        Quit("operator `||` not supported for type `%s`", Type_String(head.a->type));
    head.a->of.boolean = head.a->of.boolean || head.b->of.boolean;
    VM_Pop(self);
}

void
VM_Eql(VM* self)
{
    Head head = VM_Head(self);
    bool boolean = Value_Equal(head.a, head.b);
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

void
VM_Neq(VM* self)
{
    Head head = VM_Head(self);
    bool boolean = !Value_Equal(head.a, head.b);
    VM_Pop(self);
    VM_Pop(self);
    Of of = { .boolean = boolean };
    Queue_PshB(self->stack, Value_Init(of, TYPE_BOOL));
}

void
VM_Spd(VM* self)
{
    self->spds += 1;
}

void
VM_Not(VM* self)
{
    Value* value = Queue_Back(self->stack);
    if(value->type != TYPE_BOOL)
        Quit("operator `!` not supported for type `%s`", Type_String(value->type));
    value->of.boolean = !value->of.boolean;
    VM_Pop(self);
}

void
VM_Brf(VM* self, int address)
{
    Value* value = Queue_Back(self->stack);
    if(value->type != TYPE_BOOL)
        Quit("boolean expression expected");
    if(value->of.boolean == false)
        self->pc = address;
    VM_Pop(self);
}

int
VM_Run(VM* self)
{
    while(true)
    {
        int instruction = self->instructions[self->pc];
        self->pc += 1;
        Opcode oc = instruction & 0xFF;
        int address = instruction >> 8;
#if DEBUG
        fprintf(stderr, ">>> %s (%d)\n", Opcode_String(oc), address);
#endif
        switch(oc)
        {
            case OPCODE_ADD: VM_Add(self); break;
            case OPCODE_AND: VM_And(self); break;
            case OPCODE_BRF: VM_Brf(self, address); break;
            case OPCODE_CAL: VM_Cal(self, address); break;
            case OPCODE_CPY: VM_Cpy(self); break;
            case OPCODE_DIV: VM_Div(self); break;
            case OPCODE_END: return VM_End(self);
            case OPCODE_EQL: VM_Eql(self); break;
            case OPCODE_FLS: VM_Fls(self); break;
            case OPCODE_FMT: break;
            case OPCODE_GET: break;
            case OPCODE_GLB: VM_Glb(self, address); break;
            case OPCODE_GRT: VM_Grt(self); break;
            case OPCODE_GTE: VM_Gte(self); break;
            case OPCODE_INS: break;
            case OPCODE_JMP: VM_Jmp(self, address); break;
            case OPCODE_LOC: VM_Loc(self, address); break;
            case OPCODE_LOD: VM_Lod(self); break;
            case OPCODE_LOR: VM_Lor(self); break;
            case OPCODE_LST: VM_Lst(self); break;
            case OPCODE_LTE: VM_Lte(self); break;
            case OPCODE_MOV: VM_Mov(self); break;
            case OPCODE_MUL: VM_Mul(self); break;
            case OPCODE_NOT: VM_Not(self); break;
            case OPCODE_NEQ: VM_Neq(self); break;
            case OPCODE_POP: VM_Pop(self); break;
            case OPCODE_PSB: break;
            case OPCODE_PSF: break;
            case OPCODE_RET: VM_Ret(self); break;
            case OPCODE_SAV: VM_Sav(self); break;
            case OPCODE_SPD: VM_Spd(self); break;
            case OPCODE_SUB: VM_Sub(self); break;
            case OPCODE_PSH: VM_Psh(self, address); break;
        }
    }
}

int
main(int argc, char* argv[])
{
    if(argc == 2)
    {
        Str* entry = Str_Init(argv[1]);
        CC* cc = CC_Init();
        CC_Reserve(cc);
        CC_LoadModule(cc, entry);
        CC_Parse(cc);
        ASM_Dump(cc->assembly);
        VM* vm = VM_Assemble(cc->assembly);
        int ret = VM_Run(vm);
        VM_Text(vm);
        VM_Data(vm);
        CC_Kill(cc);
        VM_Kill(vm);
        Str_Kill(entry);
        return ret;
    }
    Quit("usage: rr main.rr");
}
