#include "roman2.h"

#include <unistd.h>
#include <dlfcn.h>
#include <stdint.h>

#define MODULE_BUFFER_SIZE (512)

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

void
CC_Expression(CC*);

bool
CC_Factor(CC*);

void
CC_Block(CC*, int head, int tail, bool loop);

int
VM_Run(VM*, bool arbitrary);

char*
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

Meta*
Meta_Init(Class class, int stack, String* path)
{
    Meta* self = Malloc(sizeof(*self));
    self->class = class;
    self->stack = stack;
    self->path = path;
    return self;
}

void
Meta_Kill(Meta* self)
{
    String_Kill(self->path);
    Free(self);
}

void
Module_Buffer(Module* self)
{
    self->index = 0;
    self->size = fread(self->buffer, sizeof(*self->buffer), MODULE_BUFFER_SIZE, self->file);
}

Module*
Module_Init(String* path)
{
    FILE* file = fopen(String_Value(path), "r");
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

void
Module_Kill(Module* self)
{
    String_Kill(self->name);
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
    if(at == '\n')
        self->line += 1;
    self->index += 1;
}

void
CC_Quit(CC* self, const char* const message, ...)
{
    Module* back = Queue_Back(self->modules);
    va_list args;
    va_start(args, message);
    fprintf(stderr, "error: file `%s`: line `%d`: ", back ? String_Value(back->name): "?", back ? back->line : 0);
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(0xFE);
}

void
CC_Advance(CC* self)
{
    Module_Advance(Queue_Back(self->modules));
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
        if(String_IsSpace(peak) || comment)
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

String*
CC_Stream(CC* self, bool clause(int))
{
    String* str = String_Init("");
    CC_Spin(self);
    while(clause(CC_Peak(self)))
        String_PshB(str, CC_Read(self));
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
            CC_Quit(self, "matched character `%s` but expected character `%c`", (peak == EOF) ? "EOF" : formatted, *expect);
        }
        expect += 1;
    }
}

String*
CC_Mod(CC* self)
{
    return CC_Stream(self, String_IsModule);
}

String*
CC_Ident(CC* self)
{
    return CC_Stream(self, String_IsIdent);
}

String*
CC_Operator(CC* self)
{
    return CC_Stream(self, String_IsOp);
}

String*
CC_Number(CC* self)
{
    return CC_Stream(self, String_IsNumber);
}

String*
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

CC*
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

char*
CC_RealPath(CC* self, String* file)
{
    String* path;
    if(Queue_Empty(self->modules))
    {
        path = String_Init("");
        String_Resize(path, 4096);
        getcwd(String_Begin(path), String_Size(path));
        strcat(String_Begin(path), "/");
        strcat(String_Begin(path), String_Value(file));
    }
    else
    {
        Module* back = Queue_Back(self->modules);
        path = String_Base(back->name);
        String_Appends(path, String_Value(file));
    }
    char* real = realpath(String_Value(path), NULL);
    if(real == NULL)
        CC_Quit(self, "`%s` could not be resolved as a real path", String_Value(path));
    String_Kill(path);
    return real;
}

void
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

String*
CC_Parents(String* module)
{
    String* parents = String_Init("");
    for(char* value = String_Value(module); *value; value++)
    {
        if(*value != '.')
            break;
        String_Appends(parents, "../");
    }
    return parents;
}

String*
CC_ModuleName(CC* self, char* postfix)
{
    String* module = CC_Mod(self);
    String* skipped = String_Skip(module, '.');
    String_Replace(skipped, '.', '/');
    String_Appends(skipped, postfix);
    String* name = CC_Parents(module);
    String_Appends(name, String_Value(skipped));
    String_Kill(skipped);
    String_Kill(module);
    return name;
}

void
CC_Include(CC* self)
{
    String* name = CC_ModuleName(self, ".rr");
    CC_Match(self, ";");
    CC_IncludeModule(self, name);
    String_Kill(name);
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
    return class == CLASS_FUNCTION
        || class == CLASS_FUNCTION_PROTOTYPE
        || class == CLASS_FUNCTION_PROTOTYPE_NATIVE;
}

void
CC_Define(CC* self, Class class, int stack, String* ident, String* path)
{
    Meta* old = Map_Get(self->identifiers, String_Value(ident));
    Meta* new = Meta_Init(class, stack, path);
    if(old)
    {
        // Only function prototypes can be upgraded to functions.
        if(old->class == CLASS_FUNCTION_PROTOTYPE
        && new->class == CLASS_FUNCTION)
        {
            if(new->stack != old->stack)
                CC_Quit(self, "function `%s` with `%d` argument(s) was previously defined as a function prototype with `%d` argument(s)", String_Value(ident), new->stack, old->stack);
        }
        else
            CC_Quit(self, "%s `%s` was already defined as a %s", Class_String(new->class), String_Value(ident), Class_String(old->class));
    }
    Map_Set(self->identifiers, ident, new);
}

void
CC_ConsumeExpression(CC* self)
{
    CC_Expression(self);
    Queue_PshB(self->assembly, String_Init("\tpop"));
}

void
CC_EmptyExpression(CC* self)
{
    CC_ConsumeExpression(self);
    CC_Match(self, ";");
}

void
CC_Assign(CC* self)
{
    CC_Match(self, ":=");
    CC_Expression(self);
    CC_Match(self, ";");
}

void
CC_Local(CC* self, String* ident)
{
    CC_Define(self, CLASS_VARIABLE_LOCAL, self->locals, ident, String_Init(""));
    self->locals += 1;
}

void
CC_AssignLocal(CC* self, String* ident)
{
    CC_Assign(self);
    CC_Local(self, ident);
}

String*
CC_Global(CC* self, String* ident)
{
    String* label = String_Format("!%s", String_Value(ident));
    Queue_PshB(self->assembly, String_Format("%s:", String_Value(label)));
    CC_Assign(self);
    CC_Define(self, CLASS_VARIABLE_GLOBAL, self->globals, ident, String_Init(""));
    Queue_PshB(self->assembly, String_Init("\tret"));
    self->globals += 1;
    return label;
}

Queue*
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

void
CC_DefineParams(CC* self, Queue* params)
{
    self->locals = 0;
    for(int i = 0; i < Queue_Size(params); i++)
    {
        String* ident = String_Copy(Queue_Get(params, i));
        CC_Local(self, ident);
    }
}

int
CC_PopScope(CC* self, Queue* scope)
{
    int popped = Queue_Size(scope);
    for(int i = 0; i < popped; i++)
    {
        String* key = Queue_Get(scope, i);
        Map_Del(self->identifiers, String_Value(key));
        self->locals -= 1;
    }
    for(int i = 0; i < popped; i++)
        Queue_PshB(self->assembly, String_Init("\tpop"));
    Queue_Kill(scope);
    return popped;
}

Meta*
CC_Meta(CC* self, String* ident)
{
    Meta* meta = Map_Get(self->identifiers, String_Value(ident));
    if(meta == NULL)
        CC_Quit(self, "identifier `%s` not defined", String_Value(ident));
    return meta;
}

Meta*
CC_Expect(CC* self, String* ident, bool clause(Class))
{
    Meta* meta = CC_Meta(self, ident);
    if(!clause(meta->class))
        CC_Quit(self, "identifier `%s` cannot be of class `%s`", String_Value(ident), Class_String(meta->class));
    return meta;
}

void
CC_Ref(CC* self, String* ident)
{
    Meta* meta = CC_Expect(self, ident, CC_IsVariable);
    if(meta->class == CLASS_VARIABLE_GLOBAL)
        Queue_PshB(self->assembly, String_Format("\tglb %d", meta->stack));
    else
    if(meta->class == CLASS_VARIABLE_LOCAL)
        Queue_PshB(self->assembly, String_Format("\tloc %d", meta->stack));
}

void
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

int
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

void
CC_Call(CC* self, String* ident, int args)
{
    for(int i = 0; i < args; i++)
        Queue_PshB(self->assembly, String_Init("\tspd"));
    Queue_PshB(self->assembly, String_Format("\tcal %s", String_Value(ident)));
    Queue_PshB(self->assembly, String_Init("\tlod"));
}

void
CC_IndirectCalling(CC* self, String* ident, int args)
{
    CC_Ref(self, ident);
    Queue_PshB(self->assembly, String_Format("\tpsh %d", args));
    Queue_PshB(self->assembly, String_Init("\tvrt"));
    Queue_PshB(self->assembly, String_Init("\tlod"));
}

void
CC_NativeCalling(CC* self, String* ident, Meta* meta)
{
    Queue_PshB(self->assembly, String_Format("\tpsh \"%s\"", String_Value(meta->path)));
    Queue_PshB(self->assembly, String_Format("\tpsh \"%s\"", String_Value(ident)));
    Queue_PshB(self->assembly, String_Format("\tpsh %d", meta->stack));
    Queue_PshB(self->assembly, String_Init("\tdll"));
    Queue_PshB(self->assembly, String_Init("\tpsh null"));
}

void
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

void
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

void
CC_Not(CC* self)
{
    CC_Match(self, "!");
    CC_Factor(self);
    Queue_PshB(self->assembly, String_Init("\tnot"));
}

void
CC_Direct(CC* self)
{
    String* number = CC_Number(self);
    Queue_PshB(self->assembly, String_Format("\tpsh %s", String_Value(number)));
    String_Kill(number);
}

void
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

void
CC_Calling(CC* self, String* ident)
{
    Meta* meta = CC_Meta(self, ident);
    int args = CC_Args(self);
    if(CC_IsFunction(meta->class))
    {
        if(args != meta->stack)
            CC_Quit(self, "calling function `%s` required `%d` arguments but got `%d` arguments", String_Value(ident), meta->stack, args);
        if(meta->class == CLASS_FUNCTION_PROTOTYPE_NATIVE)
            CC_NativeCalling(self, ident, meta);
        else
            CC_DirectCalling(self, ident, args);
    }
    else
    if(CC_IsVariable(meta->class))
        CC_IndirectCalling(self, ident, args);
    else
        CC_Quit(self, "identifier `%s` is not callable", String_Value(ident));
}

bool
CC_Referencing(CC* self, String* ident)
{
    Meta* meta = CC_Meta(self, ident);
    if(CC_IsFunction(meta->class))
        Queue_PshB(self->assembly, String_Format("\tpsh @%s,%d", String_Value(ident), meta->stack));
    else
    {
        CC_Ref(self, ident);
        return true;
    }
    return false;
}

bool
CC_Identifier(CC* self)
{
    bool storage = false;
    String* ident = NULL;
    if(self->prime)
        String_Swap(&self->prime, &ident);
    else
        ident = CC_Ident(self);
    if(String_IsBoolean(ident))
        Queue_PshB(self->assembly, String_Format("\tpsh %s", String_Value(ident)));
    else
    if(String_IsNull(ident))
        Queue_PshB(self->assembly, String_Format("\tpsh %s", String_Value(ident)));
    else
    if(CC_Next(self) == '(')
        CC_Calling(self, ident);
    else
        storage = CC_Referencing(self, ident);
    String_Kill(ident);
    return storage;
}

void
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

void
CC_Force(CC* self)
{
    CC_Match(self, "(");
    CC_Expression(self);
    CC_Match(self, ")");
}

void
CC_String(CC* self)
{
    String* string = CC_StringStream(self);
    Queue_PshB(self->assembly, String_Format("\tpsh \"%s\"", String_Value(string)));
    String_Kill(string);
}

bool
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

bool
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
                    CC_Quit(self, "operator `%s` is not supported for factors", String_Value(operator));
            }
        }
        String_Kill(operator);
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
                CC_Quit(self, "operator `%s` is not supported for terms", String_Value(operator));
        }
        String_Kill(operator);
    }
}

int
CC_Label(CC* self)
{
    int label = self->labels;
    self->labels += 1;
    return label;
}

void
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

String*
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

void
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

void
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

void
CC_Ret(CC* self)
{
    CC_Expression(self);
    Queue_PshB(self->assembly, String_Init("\tsav"));
    Queue_PshB(self->assembly, String_Init("\tfls"));
    CC_Match(self, ";");
}

void
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

void
CC_FunctionPrototypeNative(CC* self, Queue* params, String* ident, String* path)
{
    CC_Define(self, CLASS_FUNCTION_PROTOTYPE_NATIVE, Queue_Size(params), ident, path);
    Queue_Kill(params);
    CC_Match(self, ";");
}

void
CC_FunctionPrototype(CC* self, Queue* params, String* ident)
{
    CC_Define(self, CLASS_FUNCTION_PROTOTYPE, Queue_Size(params), ident, String_Init(""));
    Queue_Kill(params);
    CC_Match(self, ";");
}

void
CC_ImportModule(CC* self, String* file)
{
    char* real = CC_RealPath(self, file);
    String* source = String_Init(real);
    String* library = String_Init(real);
    String_PopB(library);
    String_Appends(library, "so");
    if(Map_Exists(self->included, String_Value(library)) == false)
    {
        String* command = String_Format("gcc %s -fpic -shared -o %s -l%s", String_Value(source), String_Value(library), ROMAN2SO);
        int ret = system(String_Value(command));
        if(ret != 0)
            CC_Quit(self, "compilation errors encountered while compiling native library `%s`", String_Value(source));
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

void
CC_Import(CC* self)
{
    String* name = CC_ModuleName(self, ".c");
    CC_ImportModule(self, name);
    String_Kill(name);
}

void
CC_Function(CC* self, String* ident)
{
    Queue* params = CC_ParamRoll(self);
    if(CC_Next(self) == '{')
    {
        CC_DefineParams(self, params);
        CC_Define(self, CLASS_FUNCTION, Queue_Size(params), ident, String_Init(""));
        Queue_PshB(self->assembly, String_Format("%s:", String_Value(ident)));
        CC_Block(self, 0, 0, false);
        CC_PopScope(self, params);
        Queue_PshB(self->assembly, String_Init("\tpsh null"));
        Queue_PshB(self->assembly, String_Init("\tsav"));
        Queue_PshB(self->assembly, String_Init("\tret"));
    }
    else
        CC_FunctionPrototype(self, params, ident);
}

void
CC_Spool(CC* self, Queue* start)
{
    Queue* spool = Queue_Init((Kill) String_Kill, NULL);
    String* main = String_Init("main");
    CC_Expect(self, main, CC_IsFunction);
    Queue_PshB(spool, String_Init("!start:"));
    for(int i = 0; i < Queue_Size(start); i++)
    {
        String* label = Queue_Get(start, i);
        Queue_PshB(spool, String_Format("\tcal %s", String_Value(label)));
    }
    Queue_PshB(spool, String_Format("\tcal %s", String_Value(main)));
    Queue_PshB(spool, String_Format("\tend"));
    for(int i = Queue_Size(spool) - 1; i >= 0; i--)
        Queue_PshF(self->assembly, String_Copy(Queue_Get(spool, i)));
    String_Kill(main);
    Queue_Kill(spool);
}

void
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

Map*
ASM_Label(Queue* assembly, int* size)
{
    Map* labels = Map_Init((Kill) Free, (Copy) NULL);
    for(int i = 0; i < Queue_Size(assembly); i++)
    {
        String* stub = Queue_Get(assembly, i);
        if(String_Value(stub)[0] == '\t')
            *size += 1;
        else 
        {
            String* line = String_Copy(stub);
            char* label = strtok(String_Value(line), ":");
            if(Map_Exists(labels, label))
                Quit("asm: label %s already defined", label);
            Map_Set(labels, String_Init(label), Int_Init(*size));
            String_Kill(line);
        }
    }
    return labels;
}

void
ASM_Dump(Queue* assembly)
{
    for(int i = 0; i < Queue_Size(assembly); i++)
    {
        String* assem = Queue_Get(assembly, i);
        puts(String_Value(assem));
    }
}

void
VM_TypeExpect(VM* self, Type a, Type b)
{
    (void) self;
    if(a != b)
        Quit("`vm: encountered type `%s` but expected type `%s`", Type_String(a), Type_String(b));
}

void
VM_BadOperator(VM* self, Type a, const char* op)
{
    (void) self;
    Quit("vm: type `%s` not supported with operator `%s`", Type_String(a), op);
}

void
VM_TypeMatch(VM* self, Type a, Type b, const char* op)
{
    (void) self;
    if(a != b)
        Quit("vm: type `%s` and type `%s` mismatch with operator `%s`", Type_String(a), Type_String(b), op);
}

void
VM_OutOfBounds(VM* self, Type a, int index)
{
    (void) self;
    Quit("vm: type `%s` was accessed out of bounds with index `%d`", Type_String(a), index);
}

void
VM_TypeBadIndex(VM* self, Type a, Type b)
{
    (void) self;
    Quit("vm: type `%s` cannot be indexed with type `%s`", Type_String(a), Type_String(b));
}

void
VM_TypeBad(VM* self, Type a)
{
    (void) self;
    Quit("vm: type `%s` cannot be used for this operation", Type_String(a));
}

void
VM_ArgMatch(VM* self, int a, int b)
{
    (void) self;
    if(a != b)
        Quit("vm: expected `%d` arguments but encountered `%d` arguments", a, b);
}

void
VM_UnknownEscapeChar(VM* self, int esc)
{
    (void) self;
    Quit("vm: an unknown escape character `0x%02X` was encountered\n", esc);
}

void
VM_RefImpurity(VM* self, Value* value)
{
    (void) self;
    String* print = Value_Print(value, true, 0);
    Quit("vm: the .data segment value `%s` contained `%d` references at the time of exit", String_Value(print), Value_Refs(value));
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
        fprintf(stderr, "%4d : %2d : ", i, Value_Refs(value));
        String* print = Value_Print(value, false, 0);
        fprintf(stderr, "%s\n", String_Value(print));
        String_Kill(print);
    }
}

void
VM_AssertRefCounts(VM* self)
{
    for(int i = 0; i < Queue_Size(self->data); i++)
    {
        Value* value = Queue_Get(self->data, i);
        if(Value_Refs(value) > 0)
            VM_RefImpurity(self, value);
    }
}

void
VM_Pop(VM* self)
{
    Queue_PopB(self->stack);
}

String*
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

void
VM_Store(VM* self, Map* labels, char* operand)
{
    Value* value;
    char ch = operand[0];
    if(ch == '[')
        value = Value_NewQueue();
    else
    if(ch == '{')
        value = Value_NewMap();
    else
    if(ch == '"')
    {
        String* string = VM_ConvertEscs(self, operand);
        value = Value_NewString(string);
    }
    else
    if(ch == '@')
    {
        String* name = String_Init(strtok(operand + 1, ","));
        int size = atoi(strtok(NULL, " \n"));
        int* address = Map_Get(labels, String_Value(name));
        if(address == NULL)
            Quit("asm: label `%s` not defined", name);
        value = Value_NewFunction(Function_Init(name, size, *address));
    }
    else
    if(ch == 't' || ch == 'f')
        value = Value_NewBool(Equals(operand, "true") ? true : false);
    else
    if(ch == 'n')
        value = Value_NewNull();
    else
    if(String_IsNumber(ch))
        value = Value_NewNumber(atof(operand));
    else
        Quit("vm: unknown psh operand `%s` encountered", operand);
    Queue_PshB(self->data, value);
}

int
VM_Datum(VM* self, Map* labels, char* operand)
{
    VM_Store(self, labels, operand);
    return ((Queue_Size(self->data) - 1) << 8) | OPCODE_PSH;
}

int
VM_Indirect(Opcode oc, Map* labels, char* label)
{
    int* address = Map_Get(labels, label);
    if(address == NULL)
        Quit("asm: label `%s` not defined", label);
    return *address << 8 | oc;
}

int
VM_Direct(Opcode oc, char* number)
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
        String* stub = Queue_Get(assembly, i);
        if(String_Value(stub)[0] == '\t')
        {
            String* line = String_Init(String_Value(stub) + 1);
            int instruction = 0;
            char* mnemonic = strtok(String_Value(line), " \n");
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
    Value* back = Queue_Back(self->stack);
    Value* value = Value_Copy(back);
    VM_Pop(self);
    Queue_PshB(self->stack, value);
}

int
VM_End(VM* self)
{
    VM_TypeExpect(self, Value_Type(self->ret), TYPE_NUMBER);
    int ret = *Value_Number(self->ret);
    Value_Kill(self->ret);
    return ret;
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
    Value_Inc(value);
    Queue_PshB(self->stack, value);
}

void
VM_Loc(VM* self, int address)
{
    Frame* frame = Queue_Back(self->frame);
    Value* value = Queue_Get(self->stack, frame->sp + address);
    Value_Inc(value);
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
VM_Lod(VM* self)
{
    Queue_PshB(self->stack, self->ret);
}

void
VM_Sav(VM* self)
{
    Value* value = Queue_Back(self->stack);
    Value_Inc(value);
    self->ret = value;
}

void
VM_Psh(VM* self, int address)
{
    Value* value = Queue_Get(self->data, address);
    Value* copy = Value_Copy(value);
    Queue_PshB(self->stack, copy);
}

void
VM_Mov(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(Value_Subbing(a, b))
        Value_Sub(a, b);
    else
    if(Value_Type(a) == Value_Type(b))
    {
        Type_Kill(Value_Type(a), Value_Of(a));
        Type_Copy(a, b);
    }
    else
        VM_TypeMatch(self, Value_Type(a), Value_Type(b), "=");
    VM_Pop(self);
}

void
VM_Add(VM* self)
{
    char* operator = "+";
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(Value_Pushing(a, b))
    {
        Value_Inc(b);
        Queue_PshB(Value_Queue(a), b);
    }
    else
    if(Value_CharAppending(a, b))
        String_PshB(Value_String(a), *Char_Value(Value_Char(b)));
    else
    if(Value_Type(a) == Value_Type(b))
        switch(Value_Type(a))
        {
        case TYPE_QUEUE:
            Queue_Append(Value_Queue(a), Value_Queue(b));
            break;
        case TYPE_MAP:
            Map_Append(Value_Map(a), Value_Map(b));
            break;
        case TYPE_STRING:
            String_Appends(Value_String(a), String_Value(Value_String(b)));
            break;
        case TYPE_NUMBER:
            *Value_Number(a) += *Value_Number(b);
            break;
        case TYPE_FUNCTION:
        case TYPE_CHAR:
        case TYPE_BOOL:
        case TYPE_NULL:
        case TYPE_FILE:
            VM_BadOperator(self, Value_Type(a), operator);
        }
    else
        VM_TypeMatch(self, Value_Type(a), Value_Type(b), operator);
    VM_Pop(self);
}

void
VM_Sub(VM* self)
{
    char* operator = "-";
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(Value_Pushing(a, b))
    {
        Value_Inc(b);
        Queue_PshF(Value_Queue(a), b);
    }
    else
    if(Value_Type(a) == Value_Type(b))
        switch(Value_Type(a))
        {
        case TYPE_QUEUE:
            Queue_Prepend(Value_Queue(a), Value_Queue(b));
            break;
        case TYPE_NUMBER:
            *Value_Number(a) -= *Value_Number(b);
            break;
        case TYPE_FUNCTION:
        case TYPE_MAP:
        case TYPE_STRING:
        case TYPE_CHAR:
        case TYPE_BOOL:
        case TYPE_NULL:
        case TYPE_FILE:
            VM_BadOperator(self, Value_Type(a), operator);
        }
    else
        VM_TypeMatch(self, Value_Type(a), Value_Type(b), operator);
    VM_Pop(self);
}

void
VM_Mul(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, Value_Type(a), Value_Type(b), "*");
    VM_TypeExpect(self, Value_Type(a), TYPE_NUMBER);
    *Value_Number(a) *= *Value_Number(b);
    VM_Pop(self);
}

void
VM_Div(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, Value_Type(a), Value_Type(b), "/");
    VM_TypeExpect(self, Value_Type(a), TYPE_NUMBER);
    *Value_Number(a) /= *Value_Number(b);
    VM_Pop(self);
}

void
VM_Dll(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 3);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* c = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, Value_Type(a), TYPE_STRING);
    VM_TypeExpect(self, Value_Type(b), TYPE_STRING);
    VM_TypeExpect(self, Value_Type(c), TYPE_NUMBER);
    typedef void* (*Fun)();
    Fun fun;
    void* so = dlopen(String_Value(Value_String(a)), RTLD_NOW | RTLD_GLOBAL);
    if(so == NULL)
        Quit("vm: could not open shared object library `%s`\n", String_Value(Value_String(a)));
    *(void**)(&fun) = dlsym(so, String_Value(Value_String(b)));
    if(fun == NULL)
        Quit("vm: could not open shared object function `%s` from shared object library `%s`\n", String_Value(Value_String(b)), String_Value(Value_String(a)));
    int start = Queue_Size(self->stack) - 3;
    int args = *Value_Number(c);
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

void
VM_Vrt(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, Value_Type(a), TYPE_FUNCTION);
    VM_TypeExpect(self, Value_Type(b), TYPE_NUMBER);
    VM_ArgMatch(self, Function_Size(Value_Function(a)), *Value_Number(b));
    int spds = *Value_Number(b);
    int pc = Function_Address(Value_Function(a));
    VM_Pop(self);
    VM_Pop(self);
    int sp = Queue_Size(self->stack) - spds;
    Queue_PshB(self->frame, Frame_Init(self->pc, sp));
    self->pc = pc;
}

void
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
        Queue_PshB(self->stack, Value_NewNumber(2));
        VM_Vrt(self);
        VM_Run(self, true);
        VM_TypeExpect(self, Value_Type(self->ret), TYPE_BOOL);
        if(*Value_Bool(self->ret))
             Queue_Swap(queue, ++last, i);
        Value_Kill(self->ret);
    }
   Queue_Swap(queue, left, last);
   VM_RangedSort(self, queue, compare, left, last - 1);
   VM_RangedSort(self, queue, compare, last + 1, right);
}

void
VM_Sort(VM* self, Queue* queue, Value* compare)
{
    VM_TypeExpect(self, Value_Type(compare), TYPE_FUNCTION);
    VM_ArgMatch(self, Function_Size(Value_Function(compare)), 2);
    VM_RangedSort(self, queue, compare, 0, Queue_Size(queue) - 1);
}

void
VM_Srt(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, Value_Type(a), TYPE_QUEUE);
    VM_TypeExpect(self, Value_Type(b), TYPE_FUNCTION);
    VM_Sort(self, Value_Queue(a), b);
    VM_Pop(self);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewNull());
}

void
VM_Lst(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, Value_Type(a), Value_Type(b), "<");
    bool boolean = Value_LessThan(a, b);
    VM_Pop(self);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewBool(boolean));
}

void
VM_Lte(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, Value_Type(a), Value_Type(b), "<=");
    bool boolean = Value_LessThanEqualTo(a, b);
    VM_Pop(self);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewBool(boolean));
}

void
VM_Grt(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, Value_Type(a), Value_Type(b), ">");
    bool boolean = Value_GreaterThan(a, b);
    VM_Pop(self);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewBool(boolean));
}

void
VM_Gte(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, Value_Type(a), Value_Type(b), ">=");
    bool boolean = Value_GreaterThanEqualTo(a, b);
    VM_Pop(self);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewBool(boolean));
}

void
VM_And(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, Value_Type(a), Value_Type(b), "&&");
    VM_TypeExpect(self, Value_Type(a), TYPE_BOOL);
    *Value_Bool(a) = *Value_Bool(a) && *Value_Bool(b);
    VM_Pop(self);
}

void
VM_Lor(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeMatch(self, Value_Type(a), Value_Type(b), "||");
    VM_TypeExpect(self, Value_Type(a), TYPE_BOOL);
    *Value_Bool(a) = *Value_Bool(a) || *Value_Bool(b);
    VM_Pop(self);
}

void
VM_Eql(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    bool boolean = Value_Equal(a, b);
    VM_Pop(self);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewBool(boolean));
}

void
VM_Neq(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    bool boolean = !Value_Equal(a, b);
    VM_Pop(self);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewBool(boolean));
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
    VM_TypeExpect(self, Value_Type(value), TYPE_BOOL);
    *Value_Bool(value) = !*Value_Bool(value);
}

void
VM_Brf(VM* self, int address)
{
    Value* value = Queue_Back(self->stack);
    VM_TypeExpect(self, Value_Type(value), TYPE_BOOL);
    if(*Value_Bool(value) == false)
        self->pc = address;
    VM_Pop(self);
}

void
VM_Prt(VM* self)
{
    Value* value = Queue_Back(self->stack);
    String* print = Value_Print(value, true, 0);
    printf("%s", String_Value(print));
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewNumber(String_Size(print)));
    String_Kill(print);
}

void
VM_Len(VM* self)
{
    Value* value = Queue_Back(self->stack);
    int len = Value_Len(value);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewNumber(len));
}

void
VM_Psb(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, Value_Type(a), TYPE_QUEUE);
    Value_Inc(b);
    Queue_PshB(Value_Queue(a), b);
    VM_Pop(self);
}

void
VM_Psf(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, Value_Type(a), TYPE_QUEUE);
    Value_Inc(b);
    Queue_PshF(Value_Queue(a), b);
    VM_Pop(self);
}

void
VM_Ins(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 3);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* c = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, Value_Type(a), TYPE_MAP);
    if(Value_Type(b)== TYPE_CHAR)
        VM_TypeBadIndex(self, Value_Type(a), Value_Type(b));
    else
    if(Value_Type(b) == TYPE_STRING)
        Map_Set(Value_Map(a), String_Copy(Value_String(b)), c); // Keys are not reference counted.
    else
        VM_TypeBad(self, Value_Type(b));
    Value_Inc(c);
    VM_Pop(self);
    VM_Pop(self);
}

void
VM_Ref(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    int refs = Value_Refs(a);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewNumber(refs));
}

Value*
VM_IndexQueue(VM* self, Value* queue, Value* index)
{
    Value* value = Queue_Get(Value_Queue(queue), *Value_Number(index));
    if(value == NULL)
        VM_OutOfBounds(self, Value_Type(queue), *Value_Number(index));
    Value_Inc(value);
    return value;
}

Value*
VM_IndexString(VM* self, Value* queue, Value* index)
{
    Char* character = Char_Init(queue, *Value_Number(index));
    if(character == NULL)
        VM_OutOfBounds(self, Value_Type(queue), *Value_Number(index));
    Value* value = Value_NewChar(character);
    Value_Inc(queue);
    return value;
}

Value*
VM_Index(VM* self, Value* storage, Value* index)
{
    if(Value_Type(storage) == TYPE_QUEUE)
        return VM_IndexQueue(self, storage, index);
    else
    if(Value_Type(storage) == TYPE_STRING)
        return VM_IndexString(self, storage, index);
    VM_TypeBadIndex(self, Value_Type(storage), Value_Type(index));
    return NULL;
}

Value*
VM_Lookup(VM* self, Value* map, Value* index)
{
    VM_TypeExpect(self, Value_Type(map), TYPE_MAP);
    Value* value = Map_Get(Value_Map(map), String_Value(Value_String(index)));
    if(value != NULL)
        Value_Inc(value);
    return value;
}

void
VM_Get(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    Value* value = NULL;
    if(Value_Type(b) == TYPE_NUMBER)
        value = VM_Index(self, a, b);
    else
    if(Value_Type(b) == TYPE_STRING)
        value = VM_Lookup(self, a, b);
    else
        VM_TypeBadIndex(self, Value_Type(a), Value_Type(b));
    VM_Pop(self);
    VM_Pop(self);
    if(value == NULL)
        Queue_PshB(self->stack, Value_NewNull());
    else
        Queue_PshB(self->stack, value);
}

void
VM_Fmt(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, Value_Type(a), TYPE_STRING);
    VM_TypeExpect(self, Value_Type(b), TYPE_QUEUE);
    String* formatted = String_Init("");
    int index = 0;
    char* ref = String_Value(Value_String(a));
    for(char* c = ref; *c; c++)
    {
        if(*c == '{')
        {
            char next = *(c + 1);
            if(next == '}')
            {
                Value* value = Queue_Get(Value_Queue(b), index);
                if(value == NULL)
                    VM_OutOfBounds(self, Value_Type(b), index);
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
    Queue_PshB(self->stack, Value_NewString(formatted));
}

void
VM_Typ(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    Type type = Value_Type(a);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewString(String_Init(Type_String(type))));
}

void
VM_Del(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    if(Value_Type(b) == TYPE_NUMBER)
    {
        if(Value_Type(a) == TYPE_QUEUE)
        {
            bool success = Queue_Del(Value_Queue(a), *Value_Number(b));
            if(success == false)
                VM_OutOfBounds(self, Value_Type(a), *Value_Number(b));
        }
        else
        if(Value_Type(a) == TYPE_STRING)
        {
            bool success = String_Del(Value_String(a), *Value_Number(b));
            if(success == false)
                VM_OutOfBounds(self, Value_Type(a), *Value_Number(b));
        }
        else
            VM_TypeBadIndex(self, Value_Type(a), Value_Type(b));
    }
    else
    if(Value_Type(b) == TYPE_STRING)
    {
        if(Value_Type(a) != TYPE_MAP)
            VM_TypeBadIndex(self, Value_Type(a), Value_Type(b));
        Map_Del(Value_Map(a), String_Value(Value_String(b)));
    }
    else
        VM_TypeBad(self, Value_Type(a));
    VM_Pop(self);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewNull());
}

void
VM_Mem(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    bool boolean = a == b;
    VM_Pop(self);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewBool(boolean));
}

void
VM_Opn(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, Value_Type(a), TYPE_STRING);
    VM_TypeExpect(self, Value_Type(b), TYPE_STRING);
    File* file = File_Init(String_Copy(Value_String(a)), String_Copy(Value_String(b)));
    VM_Pop(self);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewFile(file));
}

void
VM_Red(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, Value_Type(a), TYPE_FILE);
    VM_TypeExpect(self, Value_Type(b), TYPE_NUMBER);
    String* buffer = String_Init("");
    if(File_File(Value_File(a)))
    {
        String_Resize(buffer, *Value_Number(b));
        int size = fread(String_Value(buffer), sizeof(char), *Value_Number(b), File_File(Value_File(a)));
        int diff = *Value_Number(b) - size;
        while(diff)
        {
            String_PopB(buffer);
            diff -= 1;
        }
    }
    VM_Pop(self);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewString(buffer));
}

void
VM_Wrt(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 2);
    Value* b = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, Value_Type(a), TYPE_FILE);
    VM_TypeExpect(self, Value_Type(b), TYPE_STRING);
    size_t bytes = 0;
    if(File_File(Value_File(a)))
        bytes = fwrite(String_Value(Value_String(b)), sizeof(char), String_Size(Value_String(b)), File_File(Value_File(a)));
    VM_Pop(self);
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewNumber(bytes));
}

void
VM_God(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, Value_Type(a), TYPE_FILE);
    bool boolean = File_File(Value_File(a)) != NULL;
    VM_Pop(self);
    Queue_PshB(self->stack, Value_NewBool(boolean));
}

void
VM_Key(VM* self)
{
    Value* a = Queue_Get(self->stack, Queue_Size(self->stack) - 1);
    VM_TypeExpect(self, Value_Type(a), TYPE_MAP);
    Value* queue = Map_Key(Value_Map(a));
    VM_Pop(self);
    Queue_PshB(self->stack, queue);
}

int
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