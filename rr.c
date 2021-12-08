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
 * Copyright (C) 2021 Gustav Louw. All rights reserved.
 * This work is licensed under the terms of the MIT license.  
 * For a copy, see <https://opensource.org/licenses/MIT>.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#define QUEUE_BLOCK_SIZE (8)
#define MAP_UNPRIMED (-1)
#define MODULE_BUFFER_SIZE (512)
#define STRING_CAP_SIZE (16)

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
    CHAIN_NONE,
    CHAIN_IF,
    CHAIN_ELIF,
    CHAIN_ELSE,
}
Chain;

typedef struct
{
    char* value;
    size_t size;
    size_t cap;
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
    size_t size;
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
    size_t size;
    size_t blocks;
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
    Chain chain;
    int globals;
    int locals;
}
Compiler;

typedef struct
{
    FILE* file;
    Str* name;
    size_t index;
    size_t size;
    size_t line;
    unsigned char buffer[MODULE_BUFFER_SIZE];
}
Module;

typedef struct
{
    Class class;
    int stack;
}
Meta;

void*
Malloc(size_t size)
{
    return malloc(size);
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

size_t
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
    size_t step = 0;
    while(step < self->blocks)
    {
        Block* block = self->block[step];
        while(block->a < block->b)
        {
            Delete(self->kill, block->value[block->a]);
            block->a += 1;
        }
        step += 1;
        Free(block);
    }
    Free(self->block);
    Free(self);
}

void**
Queue_At(Queue* self, size_t index)
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

void
Queue_Set(Queue* self, size_t index, void* data)
{
    void** at = Queue_At(self, index);
    if(at)
    {
        Delete(self->kill, *at);
        *at = data;
    }
}

void*
Queue_Get(Queue* self, size_t index)
{
    void** at = Queue_At(self, index);
    return at ? *at : NULL;
}

void
Queue_Alloc(Queue* self, size_t blocks)
{
    self->blocks = blocks;
    self->block = realloc(self->block, blocks * sizeof(*self->block));
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
        if(block->b == QUEUE_BLOCK_SIZE
        || block->a == QUEUE_BLOCK_SIZE)
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
            size_t index;
            Queue_Alloc(self, self->blocks + 1);
            index = self->blocks - 1;
            while(index > 0)
            {
                self->block[index] = self->block[index - 1];
                index -= 1;
            }
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
                size_t index = 0;
                while(index < self->blocks - 1)
                {
                    self->block[index] = self->block[index + 1];
                    index += 1;
                }
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
Queue_Del(Queue* self, size_t index)
{
    Kill kill = self->kill;
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
            self->kill = NULL;
            Queue_PopF(self);
            self->kill = kill;
        }
        else
        {
            while(index < Queue_Size(self) - 1)
            {
                *Queue_At(self, index) = *Queue_At(self, index + 1);
                index += 1;
            }
            self->kill = NULL;
            Queue_PopB(self);
            self->kill = kill;
        }
    }
}

Queue*
Queue_Copy(Queue* self)
{
    Queue* copy = Queue_Init(self->kill, self->copy);
    size_t index = 0;
    while(index < Queue_Size(self))
    {
        void* temp = Queue_Get(self, index);
        void* value = copy->copy ? copy->copy(temp) : temp;
        Queue_PshB(copy, value);
        index += 1;
    }
    return copy;
}

void
Queue_Append(Queue* self, Queue* other)
{
    size_t index = 0;
    while(index < Queue_Size(other))
    {
        void* temp = Queue_Get(other, index);
        void* value = self->copy ? self->copy(temp) : temp;
        Queue_PshB(self, value);
        index += 1;
    }
}

bool
Queue_Equal(Queue* self, Queue* other, Equal equal)
{
    if(Queue_Size(self) != Queue_Size(other))
        return false;
    else
    {
        size_t index = 0;
        while(index < Queue_Size(self))
        {
            if(!equal(Queue_Get(self, index), Queue_Get(other, index)))
                return false;
            index += 1;
        }
    }
    return true;
}

void
Str_Alloc(Str* self, size_t cap)
{
    self->cap = cap;
    self->value = realloc(self->value, (1 + cap) * sizeof(*self->value));
}

Str*
Str_Init(char* string)
{
    Str* self = Malloc(sizeof(*self));
    self->value = NULL;
    self->size = strlen(string);
    Str_Alloc(self, self->size < STRING_CAP_SIZE ? STRING_CAP_SIZE : self->size);
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

size_t
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

size_t
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
Str_Resize(Str* self, char ch, size_t size)
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
Str_Equals(Str* a, char* b)
{
    return strcmp(a->value, b) == 0;
}

bool
Str_Equal(Str* a, Str* b)
{
    return Str_Equals(a, b->value);
}

void
Str_Replace(Str* self, char a, char b)
{
    size_t index = 0;
    while(index < self->size)
    {
        if(self->value[index] == a)
            self->value[index] = b;
        index += 1;
    }
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
    return Str_Equals(ident, "true")
        || Str_Equals(ident, "false");
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

const size_t
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

size_t
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
        size_t index = 0;
        while(index < Map_Buckets(self))
        {
            Node* bucket = self->bucket[index];
            while(bucket)
            {
                Node* next = bucket->next;
                Node_Kill(bucket, self->kill);
                bucket = next;
            }
            index += 1;
        }
    }
    Free(self->bucket);
    Free(self);
}

size_t
Map_Hash(char* key)
{
    size_t hash = 5381;
    char ch = 0;
    while((ch = *key++))
        hash = ((hash << 5) + hash) + ch;
    return hash;
}

Node**
Map_Bucket(Map* self, char* key)
{
    size_t index = Map_Hash(key) % Map_Buckets(self);
    return &self->bucket[index];
}

void
Map_Alloc(Map* self, size_t index)
{
    self->prime_index = index;
    self->bucket = calloc(Map_Buckets(self), sizeof(*self->bucket));
}

void
Map_Emplace(Map*, Str*, Node*);

void
Map_Rehash(Map* self)
{
    size_t index = 0;
    Map* other = Map_Init(self->kill, self->copy);
    Map_Alloc(other, self->prime_index + 1);
    while(index < Map_Buckets(self))
    {
        Node* bucket = self->bucket[index];
        while(bucket)
        {
            Node* next = bucket->next;
            Map_Emplace(other, bucket->key, bucket);
            bucket = next;
        }
        index += 1;
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
    size_t index = 0;
    while(index < Map_Buckets(self))
    {
        Node* chain = self->bucket[index];
        while(chain)
        {
            Node* node = Node_Copy(chain, copy->copy);
            Map_Emplace(copy, chain->key, node);
            chain = chain->next;
        }
        index += 1;
    }
    return copy;
}

void
Map_Append(Map* self, Map* other)
{
    size_t index = 0;
    while(index < Map_Buckets(other))
    {
        Node* chain = other->bucket[index];
        while(chain)
        {
            Map_Set(self, Str_Copy(chain->key), 
                self->copy ? self->copy(chain->value) : chain->value);
            chain = chain->next;
        }
        index += 1;
    }
}

bool
Map_Equal(Map* self, Map* other, Equal equal)
{
    if(Map_Size(self) != Map_Size(other))
        return false;
    else
    {
        size_t index = 0;
        while(index < Map_Buckets(self))
        {
            Node* chain = self->bucket[index];
            while(chain)
            {
                void* got = Map_Get(other, chain->key->value);
                if(got == NULL)
                    return false;
                if(!equal(chain->value, got))
                    return false;
                chain = chain->next;
            }
            index += 1;
        }
    }
    return true;
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
Meta_Init(Class class, size_t stack)
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

void
Compiler_Label(Compiler* self, char* label, bool private)
{
    Str* assem = Str_Init("");
    if(private)
        Str_Appends(assem, "!");
    Str_Appends(assem, label);
    if(private)
        Queue_PshB(self->data, Str_Copy(assem));
    Str_Appends(assem, ":");
    Queue_PshB(self->assembly, assem);
}

void
Compiler_Assem(Compiler* self, char* format, ...)
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
    Queue_PshB(self->assembly, line);
}

void
Compiler_Quit(Compiler* self, const char* const message, ...)
{
    Module* back = Queue_Back(self->modules);
    va_list args;
    va_start(args, message);
    printf("error: file `%s`: line `%lu`: ",
        back ? back->name->value : "?",
        back ? back->line : 0);
    vprintf(message, args);
    printf("\n");
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

size_t
Module_Size(Module* self)
{
    return self->size;
}

size_t
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
Compiler_LoadModule(Compiler* self, Str* file)
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
        Compiler_Quit(self, "could not resolve path `%s`", path->value);
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
Compiler_Advance(Compiler* self)
{
    Module_Advance(Queue_Back(self->modules));
}

bool
Compiler_IsLower(int c)
{
    return c >= 'a'
        && c <= 'z';
}

bool
Compiler_IsUpper(int c)
{
    return c >= 'A'
        && c <= 'Z';
}

bool
Compiler_IsAlpha(int c)
{
    return Compiler_IsLower(c)
        || Compiler_IsUpper(c);
}

bool
Compiler_IsDigit(int c)
{
    return c >= '0'
        && c <= '9';
}

bool
Compiler_IsNumber(int c)
{
    return Compiler_IsDigit(c)
        || c == '.';
}

bool
Compiler_IsAlnum(int c)
{
    return Compiler_IsAlpha(c)
        || Compiler_IsDigit(c);
}

bool
Compiler_IsIdentLeader(int c)
{
    return Compiler_IsAlpha(c)
        || c == '_';
}

bool
Compiler_IsIdent(int c)
{
    return Compiler_IsIdentLeader(c)
        || Compiler_IsDigit(c);
}

bool
Compiler_IsMod(int c)
{
    return Compiler_IsIdent(c)
        || c == '.';
}

bool
Compiler_IsOp(int c)
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
Compiler_IsEsc(int c)
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
Compiler_IsSpace(int c)
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
Compiler_Peak(Compiler* self)
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
Compiler_Spin(Compiler* self)
{
    bool comment = false;
    while(true)
    {
        int peak = Compiler_Peak(self);
        if(peak == '#')
            comment = true;
        if(peak == '\n')
            comment = false;
        if(Compiler_IsSpace(peak) || comment)
            Compiler_Advance(self);
        else
            break;
    }
}

int
Compiler_Next(Compiler* self)
{
    Compiler_Spin(self);
    return Compiler_Peak(self);
}

int
Compiler_Read(Compiler* self)
{
    int peak = Compiler_Peak(self);
    if(peak != EOF)
        Compiler_Advance(self);
    return peak;
}

Str*
Compiler_Stream(Compiler* self, bool clause(int))
{
    Str* str = Str_Init("");
    Compiler_Spin(self);
    while(clause(Compiler_Peak(self)))
        Str_PshB(str, Compiler_Read(self));
    return str;
}

void
Compiler_Match(Compiler* self, char* expect)
{
    Compiler_Spin(self);
    while(*expect)
    {
        int peak = Compiler_Read(self);
        if(peak != *expect)
        {
            char formatted[] = { peak, '\0' };
            Compiler_Quit(self, "got `%s`, expected `%c`", (peak == EOF) ? "EOF" : formatted, *expect);
        }
        expect += 1;
    }
}

Str*
Compiler_Mod(Compiler* self)
{
    return Compiler_Stream(self, Compiler_IsMod);
}

Str*
Compiler_Ident(Compiler* self)
{
    return Compiler_Stream(self, Compiler_IsIdent);
}

Str*
Compiler_Operator(Compiler* self)
{
    return Compiler_Stream(self, Compiler_IsOp);
}

Str*
Compiler_Number(Compiler* self)
{
    return Compiler_Stream(self, Compiler_IsNumber);
}

Str*
Compiler_String(Compiler* self)
{
    Str* str = Str_Init("");
    Compiler_Spin(self);
    Compiler_Match(self, "\"");
    while(true)
    {
        int lead = Compiler_Peak(self);
        if(lead == '"')
            break;
        else
        {
            Str_PshB(str, Compiler_Read(self));
            if(lead == '\\')
            {
                int escape = Compiler_Peak(self);
                if(!Compiler_IsEsc(escape))
                    Compiler_Quit(self, "unknown escape char `0x%02X`\n", escape);
                Str_PshB(str, Compiler_Read(self));
            }
        }
    }
    Compiler_Match(self, "\"");
    return str;
}

Compiler*
Compiler_Init(void)
{
    Compiler* self = Malloc(sizeof(*self));
    self->modules = Queue_Init(
        (Kill) Module_Kill,
        (Copy) NULL);
    self->assembly = Queue_Init(
        (Kill) Str_Kill,
        (Copy) NULL);
    self->data = Queue_Init(
        (Kill) Str_Kill,
        (Copy) NULL);
    self->included = Map_Init(
        (Kill) NULL,
        (Copy) NULL);
    self->identifiers = Map_Init(
        (Kill) Meta_Kill,
        (Copy) NULL);
    self->globals = self->locals = 0;
    self->prime = NULL;
    self->chain = CHAIN_NONE;
    return self;
}

void
Compiler_PrintSyms(Compiler* self)
{
    size_t index = 0;
    Map* identifiers = self->identifiers;
    while(index < Map_Buckets(identifiers))
    {
        Node* bucket = identifiers->bucket[index];
        while(bucket)
        {
            Meta* meta = bucket->value;
            printf("\n%s : %2d : %s", Class_String(meta->class), meta->stack, bucket->key->value);
            bucket = bucket->next;
        }
        index += 1;
    }
    putchar('\n');
}

void
Compiler_PrintStringQueue(Queue* queue)
{
    size_t index = 0;
    while(index < Queue_Size(queue))
    {
        Str* assem = Queue_Get(queue, index);
        puts(assem->value);
        index += 1;
    }
}

void
Compiler_Kill(Compiler* self)
{
    Queue_Kill(self->modules);
    Queue_Kill(self->assembly);
    Queue_Kill(self->data);
    Map_Kill(self->included);
    Map_Kill(self->identifiers);
    Free(self);
}

Str*
Compiler_Parents(Str* module)
{
    Str* parents = Str_Init("");
    char* value = module->value;
    while(*value)
    {
        if(*value != '.')
            break;
        Str_Appends(parents, "../");
        value += 1;
    }
    return parents;
}

void
Compiler_Include(Compiler* self)
{
    Str* module = Compiler_Mod(self);
    Compiler_Match(self, ";");
    Str* skipped = Str_Skip(module, '.');
    Str_Appends(skipped, ".rr");
    Str* parents = Compiler_Parents(module);
    Str_Appends(parents, skipped->value);
    Compiler_LoadModule(self, parents);
    Str_Kill(skipped);
    Str_Kill(module);
    Str_Kill(parents);
}

bool
Compiler_IsGlobal(Class class)
{
    return class == CLASS_VARIABLE_GLOBAL;
}

bool
Compiler_IsLocal(Class class)
{
    return class == CLASS_VARIABLE_LOCAL;
}

bool
Compiler_IsVariable(Class class)
{
    return Compiler_IsGlobal(class) || Compiler_IsLocal(class);
}

bool
Compiler_IsFunction(Class class)
{
    return class == CLASS_FUNCTION;
}

void
Compiler_Declare(Compiler* self, Str* name, Class class, size_t stack)
{
    if(Map_Exists(self->identifiers, name->value))
        Compiler_Quit(self, "`%s` already defined", name->value);
    Map_Set(self->identifiers, name, Meta_Init(class, stack));
}

void
Compiler_Expression(Compiler*);

void
Compiler_Initialize(Compiler* self)
{
    Compiler_Match(self, ":=");
    Compiler_Expression(self);
    Compiler_Match(self, ";");
}

void
Compiler_Local(Compiler* self, Str* ident, bool assign)
{
    if(assign)
        Compiler_Initialize(self);
    Compiler_Declare(self, ident, CLASS_VARIABLE_LOCAL, self->locals);
    self->locals += 1;
}

void
Compiler_Global(Compiler* self, Str* ident)
{
    Compiler_Label(self, ident->value, true);
    Compiler_Initialize(self);
    Compiler_Declare(self, ident, CLASS_VARIABLE_GLOBAL, self->globals);
    Compiler_Assem(self, "\tret");
    self->globals += 1;
}

Queue*
Compiler_Params(Compiler* self)
{
    Queue* params = Queue_Init((Kill) Str_Kill, (Copy) NULL);
    Compiler_Match(self, "(");
    while(Compiler_Next(self) != ')')
    {
        Str* ident = Compiler_Ident(self);
        Queue_PshB(params, ident);
        if(Compiler_Next(self) == ',')
            Compiler_Match(self, ",");
    }
    self->locals = -Queue_Size(params);
    size_t index = 0;
    while(index < Queue_Size(params))
    {
        Str* ident = Str_Copy(Queue_Get(params, index));
        Compiler_Local(self, ident, false);
        index += 1;
    }
    Compiler_Match(self, ")");
    return params;
}

void
Compiler_PopScope(Compiler* self, Queue* scope)
{
    size_t index = 0;
    while(index < Queue_Size(scope))
    {
        Str* key = Queue_Get(scope, index);
        Meta* meta = Map_Get(self->identifiers, key->value);
        printf("\n\t%s : %2d : %s", Class_String(meta->class), meta->stack, key->value);
        Map_Del(self->identifiers, key->value);
        index += 1;
        self->locals -= 1;
    }
    Queue_Kill(scope);
}

void
Compiler_Dict(Compiler* self)
{
    Compiler_Assem(self, "\tval {}");
    Compiler_Match(self, "{");
    while(Compiler_Next(self) != '}')
    {
        Compiler_Expression(self);
        Compiler_Match(self, ":");
        Compiler_Expression(self);
        Compiler_Assem(self, "\tins");
        if(Compiler_Next(self) == ',')
            Compiler_Match(self, ",");
    }
    Compiler_Match(self, "}");
}

void
Compiler_Array(Compiler* self)
{
    Compiler_Assem(self, "\tval []");
    Compiler_Match(self, "[");
    while(Compiler_Next(self) != ']')
    {
        Compiler_Expression(self);
        Compiler_Assem(self, "\tpsb");
        if(Compiler_Next(self) == ',')
            Compiler_Match(self, ",");
    }
    Compiler_Match(self, "]");
}

Meta*
Compiler_Expect(Compiler* self, Str* ident, bool clause(Class))
{
    Meta* meta = Map_Get(self->identifiers , ident->value);
    if(meta == NULL)
        Compiler_Quit(self, "identifier `%s` not defined", ident->value);
    if(!clause(meta->class))
        Compiler_Quit(self, "identifier `%s` is `%s`", ident->value, Class_String(meta->class));
    return meta;
}

void
Compiler_Ref(Compiler* self, Str* ident)
{
    Meta* meta = Compiler_Expect(self, ident, Compiler_IsVariable);
    if(meta->class == CLASS_VARIABLE_GLOBAL)
        Compiler_Assem(self, "\tref %d", meta->stack);
    else
    if(meta->class == CLASS_VARIABLE_LOCAL)
        Compiler_Assem(self, "\tref [%d]", meta->stack);
}

void
Compiler_Copy(Compiler* self)
{
    Compiler_Assem(self, "\tcpy");
}

void
Compiler_Pass(Compiler* self)
{
    if(Compiler_Next(self) == '&')
    {
        Compiler_Match(self, "&");
        Str* ident = Compiler_Ident(self);
        Compiler_Ref(self, ident);
        Str_Kill(ident);
    }
    else
        Compiler_Expression(self);
}

void
Compiler_Args(Compiler* self, size_t required)
{
    Compiler_Match(self, "(");
    size_t args = 0;
    while(Compiler_Next(self) != ')')
    {
        Compiler_Pass(self);
        if(Compiler_Next(self) == ',')
            Compiler_Match(self, ",");
        args += 1;
    }
    if(args != required)
        Compiler_Quit(self, "required `%d` arguments but got `%d` args", required, args);
    Compiler_Match(self, ")");
}

void
Compiler_Call(Compiler* self, char* ident)
{
    Compiler_Assem(self, "\tcal %s", ident);
}

bool
Compiler_Resolve(Compiler* self)
{
    bool resolved = false;
    while(Compiler_Next(self) == '[')
    {
        Compiler_Match(self, "[");
        Compiler_Expression(self);
        Compiler_Match(self, "]");
        Compiler_Assem(self, "\tget");
        resolved = true;;
    }
    return resolved;
}

bool
Compiler_Primed(Compiler* self)
{
    return self->prime != NULL;
}

void
Compiler_Not(Compiler* self)
{
    Compiler_Assem(self, "\tnot");
}

bool
Compiler_Factor(Compiler* self)
{
    bool storage = false;
    int next = Compiler_Next(self);
    /* UNARY */
    if(next == '!')
    {
        Compiler_Match(self, "!");
        Compiler_Factor(self);
        Compiler_Not(self);
    }
    else
    if(next == '&')
        Compiler_Quit(self, "references may only be passed as function arguments");
    /* DIGIT */
    else
    if(Compiler_IsDigit(next))
    {
        Str* number = Compiler_Number(self);
        Compiler_Assem(self, "\tval %s", number->value);
        Str_Kill(number);
    }
    /* IDENT */
    else
    if(Compiler_IsIdent(next)
    || Compiler_Primed(self))
    {
        Str* ident = NULL;
        if(Compiler_Primed(self))
            Str_Swap(&self->prime, &ident);
        else
            ident = Compiler_Ident(self);
        /* BOOL AND NULL */
        if(Str_IsBoolean(ident) || Str_IsNull(ident))
            Compiler_Assem(self, "\tval %s", ident->value);
        /* FUN CALL */
        else
        if(Compiler_Next(self) == '(')
        {
            Meta* meta = Compiler_Expect(self, ident, Compiler_IsFunction);
            Compiler_Args(self, meta->stack);
            Compiler_Call(self, ident->value);
            if(Compiler_Resolve(self))
                storage = true;
        }
        /* STORAGE */
        else
        {
            Compiler_Ref(self, ident);
            Compiler_Resolve(self);
            if(Compiler_Next(self) != '=')
                Compiler_Copy(self);
            storage = true;
        }
        Str_Kill(ident);
    }
    /* PAREN */
    else
    if(next == '(')
    {
        Compiler_Match(self, "(");
        Compiler_Expression(self);
        Compiler_Match(self, ")");
        Compiler_Resolve(self);
    }
    /* DICT */
    else
    if(next == '{')
    {
        Compiler_Dict(self);
        Compiler_Resolve(self);
    }
    /* ARRAY */
    else
    if(next == '[')
    {
        Compiler_Array(self);
        Compiler_Resolve(self);
    }
    /* STRING */
    else
    if(next == '"')
    {
        Str* string = Compiler_String(self);
        Compiler_Assem(self, "\tval \"%s\"", string->value);
        Str_Kill(string);
        Compiler_Resolve(self);
    }
    else
        Compiler_Quit(self, "unknown factor starting with `%c`", next);
    return storage;
}

void
Compiler_Add(Compiler* self)
{
    Compiler_Assem(self, "\tadd");
}

void
Compiler_Sub(Compiler* self)
{
    Compiler_Assem(self, "\tsub");
}

void
Compiler_Mul(Compiler* self)
{
    Compiler_Assem(self, "\tmul");
}

void
Compiler_Div(Compiler* self)
{
    Compiler_Assem(self, "\tdiv");
}

void
Compiler_Format(Compiler* self)
{
    Compiler_Assem(self, "\tfmt");
}

void
Compiler_And(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Assem(self, "\tand");
}

void
Compiler_Or(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Assem(self, "\tor");
}

void
Compiler_Assign(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Assem(self, "\tmov");
    Compiler_Copy(self);
}

void
Compiler_AddAssign(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Assem(self, "\tadd");
    Compiler_Copy(self);
}

void
Compiler_SubAssign(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Assem(self, "\tsub");
    Compiler_Copy(self);
}

void
Compiler_MulAssign(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Assem(self, "\tmul");
    Compiler_Copy(self);
}

void
Compiler_DivAssign(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Assem(self, "\tdiv");
    Compiler_Copy(self);
}

void
Compiler_Equals(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Assem(self, "\teql");
}

void
Compiler_GreaterThan(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Assem(self, "\tgt");
}

void
Compiler_LessThan(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Assem(self, "\tlt");
}

void
Compiler_GreaterThanOrEquals(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Assem(self, "\tgte");
}

void
Compiler_LessThanOrEquals(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Assem(self, "\tlte");
}

bool
Compiler_IsLvalue(int terms, bool storage)
{
    return terms == 1 && storage == true;
}

bool
Compiler_Term(Compiler* self)
{
    bool storage = Compiler_Factor(self);
    int terms = 1;
    while(Compiler_Next(self) == '*'
       || Compiler_Next(self) == '/'
       || Compiler_Next(self) == '%')
    {
        Str* operator = Compiler_Operator(self);
        if(Str_Equals(operator, "*="))
        {
            if(Compiler_IsLvalue(terms, storage))
                Compiler_MulAssign(self);
            else
                Compiler_Quit(self, "expected lvalue");
        }
        else
        if(Str_Equals(operator, "/="))
        {
            if(Compiler_IsLvalue(terms, storage))
                Compiler_DivAssign(self);
            else
                Compiler_Quit(self, "expected lvalue");
        }
        else
        {
            Compiler_Factor(self);
            if(Str_Equals(operator, "*"))
                Compiler_Mul(self);
            else
            if(Str_Equals(operator, "/"))
                Compiler_Div(self);
            else
            if(Str_Equals(operator, "%"))
                Compiler_Format(self);
            else
                Compiler_Quit(self, "operator `%s` not supported", operator->value);
        }
        Str_Kill(operator);
    }
    return storage;
}

void
Compiler_Expression(Compiler* self)
{
    bool storage = Compiler_Term(self);
    int terms = 1;
    while(Compiler_Next(self) == '+' 
       || Compiler_Next(self) == '-'
       || Compiler_Next(self) == '='
       || Compiler_Next(self) == '>'
       || Compiler_Next(self) == '<'
       || Compiler_Next(self) == '&'
       || Compiler_Next(self) == '|')
    {
        Str* operator = Compiler_Operator(self);
        if(Str_Equals(operator, "="))
        {
            if(Compiler_IsLvalue(terms, storage))
                Compiler_Assign(self);
            else
                Compiler_Quit(self, "expected lvalue");
        }
        else
        if(Str_Equals(operator, "+="))
        {
            if(Compiler_IsLvalue(terms, storage))
                Compiler_AddAssign(self);
            else
                Compiler_Quit(self, "expected lvalue");
        }
        else
        if(Str_Equals(operator, "-="))
        {
            if(Compiler_IsLvalue(terms, storage))
                Compiler_SubAssign(self);
            else
                Compiler_Quit(self, "expected lvalue");
        }
        else
        if(Str_Equals(operator, "=="))
            Compiler_Equals(self);
        else
        if(Str_Equals(operator, ">"))
            Compiler_GreaterThan(self);
        else
        if(Str_Equals(operator, "<"))
            Compiler_LessThan(self);
        else
        if(Str_Equals(operator, ">="))
            Compiler_GreaterThanOrEquals(self);
        else
        if(Str_Equals(operator, "<="))
            Compiler_LessThanOrEquals(self);
        else
        if(Str_Equals(operator, "&&"))
            Compiler_And(self);
        else
        if(Str_Equals(operator, "||"))
            Compiler_Or(self);
        else
        {
            Compiler_Term(self);
            terms += 1;
            if(Str_Equals(operator, "+"))
                Compiler_Add(self);
            else
            if(Str_Equals(operator, "-"))
                Compiler_Sub(self);
            else
                Compiler_Quit(self, "operator `%s` not supported", operator->value);
        }
        Str_Kill(operator);
    }
}

void
Compiler_Ret(Compiler* self)
{
    Compiler_Expression(self);
    Compiler_Match(self, ";");
    Compiler_Assem(self, "\tret");
}

void
Compiler_Block(Compiler*);

void
Compiler_If(Compiler* self)
{
    Compiler_Match(self, "(");
    Compiler_Expression(self);
    Compiler_Match(self, ")");
    Compiler_Block(self);
}

void
Compiler_Elif(Compiler* self)
{
    Compiler_Match(self, "(");
    Compiler_Expression(self);
    Compiler_Match(self, ")");
    Compiler_Block(self);
}

void
Compiler_Else(Compiler* self)
{
    Compiler_Block(self);
}

void
Compiler_While(Compiler* self)
{
    Compiler_Match(self, "(");
    Compiler_Expression(self);
    Compiler_Match(self, ")");
    Compiler_Block(self);
}

void
Compiler_Continue(Compiler* self)
{
    Compiler_Match(self, ";");
}

void
Compiler_Break(Compiler* self)
{
    Compiler_Match(self, ";");
}

void
Compiler_For(Compiler* self)
{
    Compiler_Match(self, "(");
    Str* first = Compiler_Ident(self);
    Queue* scope = Queue_Init((Kill) Str_Kill, (Copy) NULL);
    Queue_PshB(scope, Str_Copy(first));
    Compiler_Local(self, first, false);
    if(Compiler_Next(self) == ',')
    {
        Compiler_Match(self, ",");
        Str* second = Compiler_Ident(self);
        Compiler_Local(self, second, false);
        Queue_PshB(scope, Str_Copy(second));
    }
    Compiler_Match(self, ":");
    Compiler_Pass(self);
    Compiler_Match(self, ")");
    Compiler_Block(self);
    Compiler_PopScope(self, scope);
}

void
Compiler_Block(Compiler* self)
{
    Queue* scope = Queue_Init((Kill) Str_Kill, (Copy) NULL);
    Compiler_Match(self, "{");
    while(Compiler_Next(self) != '}')
    {
        if(Compiler_IsIdentLeader(Compiler_Next(self)))
        {
            Str* ident = Compiler_Ident(self);
            if(Str_Equals(ident, "if"))
            {
                Compiler_If(self);
                Str_Kill(ident);
                self->chain = CHAIN_IF;
            }
            else
            if(Str_Equals(ident, "elif"))
            {
                if(self->chain == CHAIN_IF
                || self->chain == CHAIN_ELIF)
                {
                    Compiler_Elif(self);
                    Str_Kill(ident);
                }
                else
                    Compiler_Quit(self, "`elif` must follow either `if` or `elif`");
                self->chain = CHAIN_ELIF;
            }
            else
            if(Str_Equals(ident, "else"))
            {
                if(self->chain == CHAIN_IF
                || self->chain == CHAIN_ELIF)
                {
                    Compiler_Else(self);
                    Str_Kill(ident);
                }
                else
                    Compiler_Quit(self, "`else` must follow either `if` or `elif`");
                self->chain = CHAIN_ELSE;
            }
            else
            if(Str_Equals(ident, "for"))
            {
                Compiler_For(self);
                Str_Kill(ident);
                self->chain = CHAIN_NONE;
            }
            else
            if(Str_Equals(ident, "while"))
            {
                Compiler_While(self);
                Str_Kill(ident);
                self->chain = CHAIN_NONE;
            }
            else
            if(Str_Equals(ident, "ret"))
            {
                Compiler_Ret(self);
                Str_Kill(ident);
                self->chain = CHAIN_NONE;
            }
            else
            if(Str_Equals(ident, "continue"))
            {
                Compiler_Continue(self);
                Str_Kill(ident);
                self->chain = CHAIN_NONE;
            }
            else
            if(Str_Equals(ident, "break"))
            {
                Compiler_Break(self);
                Str_Kill(ident);
                self->chain = CHAIN_NONE;
            }
            else
            if(Compiler_Next(self) == ':')
            {
                Queue_PshB(scope, ident);
                Compiler_Local(self, Str_Copy(ident), true);
                self->chain = CHAIN_NONE;
            }
            else /* PRIMED EXPRESSION */
            {
                self->prime = ident;
                Compiler_Expression(self);
                Compiler_Match(self, ";");
                self->chain = CHAIN_NONE;
                Compiler_Assem(self, "\tpop");
            }
        }
        else /* UNPRIMED EXPRESSION */
        {
            Compiler_Expression(self);
            Compiler_Match(self, ";");
            self->chain = CHAIN_NONE;
            Compiler_Assem(self, "\tpop");
        }
    }
    Compiler_Match(self, "}");
    Compiler_PopScope(self, scope);
}

void
Compiler_Function(Compiler* self, Str* ident)
{
    Queue* params = Compiler_Params(self);
    Compiler_Declare(self, ident, CLASS_FUNCTION, Queue_Size(params));
    Compiler_Label(self, ident->value, false);
    Compiler_Block(self);
    Compiler_Assem(self, "\tret");
    Compiler_PopScope(self, params);
}

void
Compiler_Parse(Compiler* self)
{
    while(Compiler_Peak(self) != EOF)
    {
        Str* ident = Compiler_Ident(self);
        if(Str_Equals(ident, "inc"))
        {
            Compiler_Include(self);
            Str_Kill(ident);
        }
        else
        if(Compiler_Next(self) == '(')
            Compiler_Function(self, ident);
        else
        if(Compiler_Next(self) == ':')
            Compiler_Global(self, ident);
        else
            Compiler_Quit(self, "expected function, global, or include statement");
        Compiler_Spin(self);
    }
}

int
main(int argc, char* argv[])
{
    if(argc != 2)
    {
        printf("usage: rr main.rr\n");
        return 1;
    }
    else
    {
        Str* entry = Str_Init(argv[1]);
        Compiler* compiler = Compiler_Init();
        Compiler_LoadModule(compiler, entry);
        Compiler_Parse(compiler);
        Compiler_PrintStringQueue(compiler->assembly);
        Compiler_PrintSyms(compiler);
        Compiler_Kill(compiler);
        Str_Kill(entry);
    }
}
