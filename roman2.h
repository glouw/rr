/*
 * The Roman II Programming Language
 *
 * Copyright (c) 2021-2022 Gustav Louw. All rights reserved.
 * 
 * This work is licensed under the terms of the MIT license.  
 * For a copy, see <https://opensource.org/licenses/MIT>.
 *
 *           _____                    _____
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
 */


#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct RR_File RR_File;
typedef struct RR_Function RR_Function;
typedef struct RR_Queue RR_Queue;
typedef struct RR_Char RR_Char;
typedef struct RR_Map RR_Map;
typedef struct RR_String RR_String;
typedef struct RR_Value RR_Value;
typedef struct RR_Block RR_Block;
typedef struct RR_Node RR_Node;
typedef union RR_Of RR_Of;

typedef bool
(*RR_Compare)(void*, void*);

typedef void
(*RR_Kill)(void*);

typedef void*
(*RR_Copy)(void*);

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
RR_Type;

void*
RR_Malloc(int size);

void*
RR_Realloc(void *ptr, size_t size);

void
RR_Free(void* pointer);

double
RR_Microseconds(void);

void
RR_Delete(RR_Kill kill, void* value);

bool
RR_Equals(char* a, char* b);

RR_Type
RR_Value_ToType(RR_Value* self);

RR_File*
RR_Value_ToFile(RR_Value* self);

RR_Queue*
RR_Value_ToQueue(RR_Value* self);

RR_Map*
RR_Value_ToMap(RR_Value* self);

RR_String*
RR_Value_ToString(RR_Value* self);

RR_Char*
RR_Value_ToChar(RR_Value* self);

double*
RR_Value_ToNumber(RR_Value* self);

bool*
RR_Value_ToBool(RR_Value* self);

RR_Function*
RR_Value_ToFunction(RR_Value* self);

int
RR_Value_Len(RR_Value* self);

int
RR_Value_Refs(RR_Value* self);

RR_Of*
RR_Value_Of(RR_Value* self);

void
RR_Value_Dec(RR_Value* self);

void
RR_Value_Inc(RR_Value* self);

void
RR_Value_Kill(RR_Value* self);

bool
RR_Value_LessThan(RR_Value* a, RR_Value* b);

bool
RR_Value_GreaterThanEqualTo(RR_Value* a, RR_Value* b);

bool
RR_Value_GreaterThan(RR_Value* a, RR_Value* b);

bool
RR_Value_LessThanEqualTo(RR_Value* a, RR_Value* b);

bool
RR_Value_Equal(RR_Value* a, RR_Value* b);

RR_Value*
RR_Value_Copy(RR_Value* self);

RR_Value*
RR_Value_Init(RR_Of of, RR_Type type);

RR_Value*
RR_Value_NewQueue(void);

RR_Value*
RR_Value_NewMap(void);

RR_Value*
RR_Value_NewFunction(RR_Function* function);

RR_Value*
RR_Value_NewChar(RR_Char* character);

RR_Value*
RR_Value_NewString(RR_String* string);

RR_Value*
RR_Value_NewNumber(double number);

RR_Value*
RR_Value_NewBool(bool boolean);

RR_Value*
RR_Value_NewFile(RR_File* file);

RR_Value*
RR_Value_NewNull(void);

RR_String*
RR_Value_Sprint(RR_Value* self, bool newline, int indents);

void
RR_Value_Print(RR_Value* self);

bool
RR_Value_Subbing(RR_Value* a, RR_Value* b);

void
RR_Value_Sub(RR_Value* a, RR_Value* b);

bool
RR_Value_Pushing(RR_Value* a, RR_Value* b);

void
RR_String_Alloc(RR_String* self, int cap);

char*
RR_String_Value(RR_String* self);

RR_String*
RR_String_Init(char* string);

void
RR_String_Kill(RR_String* self);

RR_String*
RR_String_FromChar(char c);

void
RR_String_Swap(RR_String** self, RR_String** other);

RR_String*
RR_String_Copy(RR_String* self);

int
RR_String_Size(RR_String* self);

void
RR_String_Replace(RR_String* self, char x, char y);

char*
RR_String_End(RR_String* self);

char
RR_String_Back(RR_String* self);

char*
RR_String_Begin(RR_String* self);

int
RR_String_Empty(RR_String* self);

void
RR_String_PshB(RR_String* self, char ch);

RR_String*
RR_String_Indent(int indents);

void
RR_String_PopB(RR_String* self);

void
RR_String_Resize(RR_String* self, int size);

bool
RR_String_Equals(RR_String* a, char* b);

bool
RR_String_Equal(RR_String* a, RR_String* b);

void
RR_String_Appends(RR_String* self, char* str);

void
RR_String_Append(RR_String* self, RR_String* other);

RR_String*
RR_String_Base(RR_String* path);

RR_String*
RR_String_Moves(char** from);

RR_String*
RR_String_Skip(RR_String* self, char c);

bool
RR_String_IsBoolean(RR_String* ident);

bool
RR_String_IsNull(RR_String* ident);

RR_String*
RR_String_Format(char* format, ...);

char*
RR_String_Get(RR_String* self, int index);

bool
RR_String_Del(RR_String* self, int index);

RR_File*
RR_File_Init(RR_String* path, RR_String* mode);

bool
RR_File_Equal(RR_File* a, RR_File* b);

void
RR_File_Kill(RR_File* self);

RR_File*
RR_File_Copy(RR_File* self);

int
RR_File_Size(RR_File* self);

FILE*
RR_File_File(RR_File* self);

RR_Function*
RR_Function_Init(RR_String* name, int size, int address);

void
RR_Function_Kill(RR_Function* self);

RR_Function*
RR_Function_Copy(RR_Function* self);

int
RR_Function_Size(RR_Function* self);

int
RR_Function_Address(RR_Function* self);

bool
RR_Function_Equal(RR_Function* a, RR_Function* b);

int
RR_Queue_Size(RR_Queue* self);

bool
RR_Queue_Empty(RR_Queue* self);

RR_Block**
RR_QueueBlockF(RR_Queue* self);

RR_Block**
RR_QueueBlockB(RR_Queue* self);

void*
RR_Queue_Front(RR_Queue* self);

void*
RR_Queue_Back(RR_Queue* self);

RR_Queue*
RR_Queue_Init(RR_Kill kill, RR_Copy copy);

void
RR_Queue_Kill(RR_Queue* self);

void**
RR_Queue_At(RR_Queue* self, int index);

void*
RR_Queue_Get(RR_Queue* self, int index);

void
RR_Queue_Alloc(RR_Queue* self, int blocks);

void
RR_Queue_PshB(RR_Queue* self, void* value);

void
RR_Queue_PshF(RR_Queue* self, void* value);

void
RR_Queue_PopB(RR_Queue* self);

void
RR_Queue_PopF(RR_Queue* self);

void
RR_Queue_PopFSoft(RR_Queue* self);

void
RR_Queue_PopBSoft(RR_Queue* self);

bool
RR_Queue_Del(RR_Queue* self, int index);

RR_Queue*
RR_Queue_Copy(RR_Queue* self);

void
RR_Queue_Prepend(RR_Queue* self, RR_Queue* other);

void
RR_Queue_Append(RR_Queue* self, RR_Queue* other);

bool
RR_Queue_Equal(RR_Queue* self, RR_Queue* other, RR_Compare compare);

void
RR_Queue_Swap(RR_Queue* self, int a, int b);

void
RR_Queue_RangedSort(RR_Queue* self, bool (*compare)(void*, void*), int left, int right);

void
RR_Queue_Sort(RR_Queue* self, bool (*compare)(void*, void*));

RR_String*
RR_Queue_Print(RR_Queue* self, int indents);

RR_Node*
RR_Node_Init(RR_String* key, void* value);

void
RR_Node_Set(RR_Node* self, RR_Kill kill, RR_String* key, void* value);

void
RR_Node_Kill(RR_Node* self, RR_Kill kill);

void
RR_Node_Push(RR_Node** self, RR_Node* node);

RR_Node*
RR_Node_Copy(RR_Node* self, RR_Copy copy);

int
RR_Map_Buckets(RR_Map* self);

bool
RR_Map_Resizable(RR_Map* self);

RR_Map*
RR_Map_Init(RR_Kill kill, RR_Copy copy);

int
RR_Map_Size(RR_Map* self);

bool
RR_Map_Empty(RR_Map* self);

void
RR_Map_Kill(RR_Map* self);

unsigned
RR_Map_Hash(RR_Map* self, char* key);

RR_Node**
RR_Map_Bucket(RR_Map* self, char* key);

void
RR_Map_Alloc(RR_Map* self, int index);

void
RR_Map_Rehash(RR_Map* self);

bool
RR_Map_Full(RR_Map* self);

void
RR_Map_Emplace(RR_Map* self, RR_String* key, RR_Node* node);

RR_Node*
RR_Map_At(RR_Map* self, char* key);

bool
RR_Map_Exists(RR_Map* self, char* key);

void
RR_Map_Del(RR_Map* self, char* key);

void
RR_Map_Set(RR_Map* self, RR_String* key, void* value);

void*
RR_Map_Get(RR_Map* self, char* key);

RR_Map*
RR_Map_Copy(RR_Map* self);

void
RR_Map_Append(RR_Map* self, RR_Map* other);

bool
RR_Map_Equal(RR_Map* self, RR_Map* other, RR_Compare compare);

RR_String*
RR_Map_Print(RR_Map* self, int indents);

RR_Value*
RR_Map_Key(RR_Map* self);

RR_Char*
RR_Char_Init(RR_Value* string, int index);

char*
RR_Char_Value(RR_Char* self);

void
RR_Char_Kill(RR_Char* self);

char*
RR_Type_ToString(RR_Type self);

void
RR_Type_Kill(RR_Type type, RR_Of* of);

void
RR_Type_Copy(RR_Value* copy, RR_Value* self);
