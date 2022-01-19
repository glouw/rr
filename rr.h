#pragma once

#include <stdbool.h>

typedef struct Value Value;
typedef struct String String;
typedef struct File File;
typedef struct Queue Queue;
typedef struct Function Function;
typedef struct Map Map;

File*
Value_ToFile(Value*);

Queue*
Value_ToQueue(Value*);

Map*
Value_ToMap(Value*);

String*
Value_ToString(Value*);

char*
Value_ToChar(Value*);

double*
Value_ToNumber(Value*);

bool*
Value_ToBool(Value*);

void
Value_Println(Value*);
