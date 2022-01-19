#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define LEN(a) (sizeof(a) / sizeof(*a))

typedef bool
(*Compare)(void*, void*);

typedef void
(*Kill)(void*);

typedef void*
(*Copy)(void*);

typedef struct Node Node;
typedef struct Value Value;
typedef struct String String;
typedef struct File File;
typedef struct Queue Queue;
typedef struct Function Function;
typedef struct Map Map;
typedef struct Block Block;
typedef struct Char Char;
typedef union Of Of;

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

/* MISC */

void*
Malloc(int size);

void*
Realloc(void *ptr, size_t size);

void
Free(void* pointer);

void
Quit(const char* const message, ...);

double
Microseconds(void);

void
Delete(Kill kill, void* value);

bool
Equals(char* a, char* b);

/* TYPE */

char*
Type_String(Type);

void
Type_Kill(Type type, Of* of);

void
Type_Copy(Value* copy, Value* self);

/* VALUE */

Type
Value_Type(Value*);

File*
Value_File(Value*);

Queue*
Value_Queue(Value*);

Map*
Value_Map(Value*);

String*
Value_String(Value*);

Char*
Value_Char(Value*);

double*
Value_Number(Value*);

bool*
Value_Bool(Value*);

Function*
Value_Function(Value*);

int
Value_Len(Value*);

int
Value_Refs(Value*);

Of*
Value_Of(Value*);

void
Value_Dec(Value*);

void
Value_Inc(Value*);

void
Value_Kill(Value*);

bool
Value_LessThan(Value* a, Value* b);

bool
Value_GreaterThanEqualTo(Value* a, Value* b);

bool
Value_GreaterThan(Value* a, Value* b);

bool
Value_LessThanEqualTo(Value* a, Value* b);

bool
Value_Equal(Value* a, Value* b);

Value*
Value_Copy(Value*);

Value*
Value_NewQueue(void);

Value*
Value_NewMap(void);

Value*
Value_NewFunction(Function*);

Value*
Value_NewChar(Char*);

Value*
Value_NewString(String*);

Value*
Value_NewNumber(double);

Value*
Value_NewBool(bool);

Value*
Value_NewFile(File*);

Value*
Value_NewNull(void);

String*
Value_Print(Value*, bool newline, int indents);

void
Value_Println(Value*);

bool
Value_Subbing(Value* a, Value* b);

void
Value_Sub(Value* a, Value* b);

bool
Value_Pushing(Value* a, Value* b);

bool
Value_CharAppending(Value* a, Value* b);

/* STRING */

void
String_Alloc(String*, int cap);

char*
String_Value(String*);

String*
String_Init(char* string);

void
String_Kill(String*);

String*
String_FromChar(char c);

void
String_Swap(String**, String** other);

String*
String_Copy(String*);

int
String_Size(String*);

void
String_Replace(String*, char x, char y);

char*
String_End(String*);

char
String_Back(String*);

char*
String_Begin(String*);

int
String_Empty(String*);

void
String_PshB(String*, char ch);

String*
String_Indent(int indents);

void
String_PopB(String*);

void
String_Resize(String*, int size);

bool
String_Equals(String* a, char* b);

bool
String_Equal(String* a, String* b);

void
String_Appends(String*, char* str);

void
String_Append(String*, String* other);

String*
String_Base(String* path);

String*
String_Moves(char** from);

String*
String_Skip(String*, char c);

bool
String_IsBoolean(String* ident);

bool
String_IsNull(String* ident);

String*
String_Format(char* format, ...);

char*
String_Get(String*, int index);

bool
String_Del(String*, int index);

int
String_EscToByte(int ch);

bool
String_IsUpper(int c);

bool
String_IsLower(int c);

bool
String_IsAlpha(int c);

bool
String_IsDigit(int c);

bool
String_IsNumber(int c);

bool
String_IsIdentLeader(int c);

bool
String_IsIdent(int c);

bool
String_IsModule(int c);

bool
String_IsOp(int c);

bool
String_IsSpace(int c);

/* FILE */

File*
File_Init(String* path, String* mode);

bool
File_Equal(File* a, File* b);

void
File_Kill(File*);

File*
File_Copy(File*);

int
File_Size(File*);

FILE*
File_File(File*);

/* FUNCTION */

Function*
Function_Init(String* name, int size, int address);

void
Function_Kill(Function*);

Function*
Function_Copy(Function*);

int
Function_Size(Function*);

int
Function_Address(Function*);

bool
Function_Equal(Function* a, Function* b);

/* QUEUE */

int
Queue_Size(Queue*);

bool
Queue_Empty(Queue*);

Block**
Queue_BlockF(Queue*);

Block**
Queue_BlockB(Queue*);

void*
Queue_Front(Queue*);

void*
Queue_Back(Queue*);

Queue*
Queue_Init(Kill kill, Copy copy);

void
Queue_Kill(Queue*);

void**
Queue_At(Queue*, int index);

void*
Queue_Get(Queue*, int index);

void
Queue_Alloc(Queue*, int blocks);

void
Queue_PshB(Queue*, void* value);

void
Queue_PshF(Queue*, void* value);

void
Queue_PopB(Queue*);

void
Queue_PopF(Queue*);

void
Queue_PopFSoft(Queue*);

void
Queue_PopBSoft(Queue*);

bool
Queue_Del(Queue*, int index);

Queue*
Queue_Copy(Queue*);

void
Queue_Prepend(Queue*, Queue* other);

void
Queue_Append(Queue*, Queue* other);

bool
Queue_Equal(Queue*, Queue* other, Compare compare);

void
Queue_Swap(Queue*, int a, int b);

void
Queue_RangedSort(Queue*, bool (*compare)(void*, void*), int left, int right);

void
Queue_Sort(Queue*, bool (*compare)(void*, void*));

String*
Queue_Print(Queue*, int indents);

/* MAP */

int
Map_Buckets(Map*);

bool
Map_Resizable(Map*);

Map*
Map_Init(Kill kill, Copy copy);

int
Map_Size(Map*);

bool
Map_Empty(Map*);

void
Map_Kill(Map*);

unsigned
Map_Hash(Map*, char* key);

Node**
Map_Bucket(Map*, char* key);

void
Map_Alloc(Map*, int index);

void
Map_Rehash(Map*);

bool
Map_Full(Map*);

void
Map_Emplace(Map*, String* key, Node* node);

Node*
Map_At(Map*, char* key);

bool
Map_Exists(Map*, char* key);

void
Map_Del(Map*, char* key);

void
Map_Set(Map*, String* key, void* value);

void*
Map_Get(Map*, char* key);

Map*
Map_Copy(Map*);

void
Map_Append(Map*, Map* other);

bool
Map_Equal(Map*, Map* other, Compare compare);

String*
Map_Print(Map*, int indents);

Value*
Map_Key(Map*);

/* CHAR */

Char*
Char_Init(Value* string, int index);

char*
Char_Value(Char*);

void
Char_Kill(Char*);
