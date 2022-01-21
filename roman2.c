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
    OPCODE_EXT,
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
    RR_Queue* modules;
    RR_Queue* assembly;
    RR_Queue* data;
    RR_Map* identifiers;
    RR_Map* included;
    RR_String* prime;
    int globals;
    int locals;
    int labels;
}
CC;

typedef struct
{
    FILE* file;
    RR_String* name;
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
    RR_String* path;
}
Meta;

typedef struct
{
    RR_Queue* data;
    RR_Queue* stack;
    RR_Queue* frame;
    RR_Map* track;
    RR_Value* ret;
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

static void
CC_Expression(CC*);

static bool
CC_Factor(CC*);

static void
CC_Block(CC*, int head, int tail, bool loop);

static int
VM_Run(VM*, bool arbitrary);

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
    case CLASS_FUNCTION_PROTOTYPE_NATIVE:
        return "native function prototype";
    }
    return "N/A";
}

static int*
Int_Init(int value)
{
    int* self = RR_Malloc(sizeof(*self));
    *self = value;
    return self;
}

static void
Int_Kill(int* self)
{
    RR_Free(self);
}

static Frame*
Frame_Init(int pc, int sp)
{
    Frame* self = RR_Malloc(sizeof(*self));
    self->pc = pc;
    self->sp = sp;
    return self;
}

static void
Frame_RR_Free(Frame* self)
{
    RR_Free(self);
}

static Meta*
Meta_Init(Class class, int stack, RR_String* path)
{
    Meta* self = RR_Malloc(sizeof(*self));
    self->class = class;
    self->stack = stack;
    self->path = path;
    return self;
}

static void
Meta_Kill(Meta* self)
{
    RR_String_Kill(self->path);
    RR_Free(self);
}

static void
Module_Buffer(Module* self)
{
    self->index = 0;
    self->size = fread(self->buffer, sizeof(*self->buffer), MODULE_BUFFER_SIZE, self->file);
}

static Module*
Module_Init(RR_String* name)
{
    FILE* file = fopen(RR_String_Value(name), "r");
    if(file)
    {
        Module* self = RR_Malloc(sizeof(*self));
        self->file = file;
        self->line = 1;
        self->name = RR_String_Copy(name);
        Module_Buffer(self);
        return self;
    }
    return NULL;
}

static void
Module_Kill(Module* self)
{
    RR_String_Kill(self->name);
    fclose(self->file);
    RR_Free(self);
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
CC_Quit(CC* self, const char* const message, ...)
{
    Module* back = RR_Queue_Back(self->modules);
    va_list args;
    va_start(args, message);
    fprintf(stderr, "error: file `%s`: line `%d`: ", back ? RR_String_Value(back->name) : "?", back ? back->line : 0);
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(0xFE);
}

static bool
CC_String_IsUpper(int c)
{
    return c >= 'A' && c <= 'Z';
}

static bool
CC_String_IsLower(int c)
{
    return c >= 'a' && c <= 'z';
}

static bool
CC_String_IsAlpha(int c)
{
    return CC_String_IsLower(c) || CC_String_IsUpper(c);
}

static bool
CC_String_IsDigit(int c)
{
    return c >= '0' && c <= '9';
}

static bool
CC_String_IsNumber(int c)
{
    return CC_String_IsDigit(c) || c == '.';
}

static bool
CC_String_IsIdentLeader(int c)
{
    return CC_String_IsAlpha(c) || c == '_';
}

static bool
CC_String_IsIdent(int c)
{
    return CC_String_IsIdentLeader(c) || CC_String_IsDigit(c);
}

static bool
CC_String_IsModule(int c)
{
    return CC_String_IsIdent(c) || c == '.';
}

static bool
CC_String_IsOp(int c)
{
    return c == '*' || c == '/' || c == '%' || c == '+' || c == '-' || c == '='
        || c == '<' || c == '>' || c == '!' || c == '&' || c == '|' || c == '?';
}

static bool
CC_String_IsSpace(int c)
{
    return c == '\n' || c == '\t' || c == '\r' || c == ' ';
}

static void
CC_Advance(CC* self)
{
    Module_Advance(RR_Queue_Back(self->modules));
}

static int
CC_Peak(CC* self)
{
    int peak;
    while(true)
    {
        peak = Module_Peak(RR_Queue_Back(self->modules));
        if(peak == EOF)
        {
            if(RR_Queue_Size(self->modules) == 1)
                return EOF;
            RR_Queue_PopB(self->modules);
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
        if(CC_String_IsSpace(peak) || comment)
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

static RR_String*
CC_Stream(CC* self, bool clause(int))
{
    RR_String* str = RR_String_Init("");
    CC_Spin(self);
    while(clause(CC_Peak(self)))
        RR_String_PshB(str, CC_Read(self));
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
            CC_Quit(self, "matched character `%s` but expected character `%c`", (peak == EOF) ? "EOF" : formatted, *expect);
        }
        expect += 1;
    }
}

static int
CC_String_EscToByte(int ch)
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

static RR_String*
CC_Mod(CC* self)
{
    return CC_Stream(self, CC_String_IsModule);
}

static RR_String*
CC_Ident(CC* self)
{
    return CC_Stream(self, CC_String_IsIdent);
}

static RR_String*
CC_Operator(CC* self)
{
    return CC_Stream(self, CC_String_IsOp);
}

static RR_String*
CC_Number(CC* self)
{
    return CC_Stream(self, CC_String_IsNumber);
}

static RR_String*
CC_StringStream(CC* self)
{
    RR_String* str = RR_String_Init("");
    CC_Spin(self);
    CC_Match(self, "\"");
    while(CC_Peak(self) != '"')
    {
        int ch = CC_Read(self);
        RR_String_PshB(str, ch);
        if(ch == '\\')
        {
            ch = CC_Read(self);
            int byte = CC_String_EscToByte(ch);
            if(byte == -1)
                CC_Quit(self, "an unknown escape char `0x%02X` was encountered", ch);
            RR_String_PshB(str, ch);
        }
    }
    CC_Match(self, "\"");
    return str;
}

static CC*
CC_Init(void)
{
    CC* self = RR_Malloc(sizeof(*self));
    self->modules = RR_Queue_Init((RR_Kill) Module_Kill, (RR_Copy) NULL);
    self->assembly = RR_Queue_Init((RR_Kill) RR_String_Kill, (RR_Copy) NULL);
    self->data = RR_Queue_Init((RR_Kill) RR_String_Kill, (RR_Copy) NULL);
    self->included = RR_Map_Init((RR_Kill) NULL, (RR_Copy) NULL);
    self->identifiers = RR_Map_Init((RR_Kill) Meta_Kill, (RR_Copy) NULL);
    self->globals = 0;
    self->locals = 0;
    self->labels = 0;
    self->prime = NULL;
    return self;
}

static void
CC_Kill(CC* self)
{
    RR_Queue_Kill(self->modules);
    RR_Queue_Kill(self->assembly);
    RR_Queue_Kill(self->data);
    RR_Map_Kill(self->included);
    RR_Map_Kill(self->identifiers);
    RR_Free(self);
}

static char*
CC_RealPath(CC* self, RR_String* file)
{
    RR_String* path;
    if(RR_Queue_Empty(self->modules))
    {
        path = RR_String_Init("");
        RR_String_Resize(path, 4096);
        getcwd(RR_String_Begin(path), RR_String_Size(path));
        strcat(RR_String_Begin(path), "/");
        strcat(RR_String_Begin(path), RR_String_Value(file));
    }
    else
    {
        Module* back = RR_Queue_Back(self->modules);
        path = RR_String_Base(back->name);
        RR_String_Appends(path, RR_String_Value(file));
    }
    char* real = realpath(RR_String_Value(path), NULL);
    if(real == NULL)
        CC_Quit(self, "`%s` could not be resolved as a real path", RR_String_Value(path));
    RR_String_Kill(path);
    return real;
}

static void
CC_IncludeModule(CC* self, RR_String* file)
{
    char* real = CC_RealPath(self, file);
    if(RR_Map_Exists(self->included, real))
        RR_Free(real);
    else
    {
        RR_String* resolved = RR_String_Moves(&real);
        RR_Map_Set(self->included, resolved, NULL);
        RR_Queue_PshB(self->modules, Module_Init(resolved));
    }
}

static RR_String*
CC_Parents(RR_String* module)
{
    RR_String* parents = RR_String_Init("");
    for(char* value = RR_String_Value(module); *value; value++)
    {
        if(*value != '.')
            break;
        RR_String_Appends(parents, "../");
    }
    return parents;
}

static RR_String*
CC_ModuleName(CC* self, char* postfix)
{
    RR_String* module = CC_Mod(self);
    RR_String* skipped = RR_String_Skip(module, '.');
    RR_String_Replace(skipped, '.', '/');
    RR_String_Appends(skipped, postfix);
    RR_String* name = CC_Parents(module);
    RR_String_Appends(name, RR_String_Value(skipped));
    RR_String_Kill(skipped);
    RR_String_Kill(module);
    return name;
}

static void
CC_Include(CC* self)
{
    RR_String* name = CC_ModuleName(self, ".rr");
    CC_Match(self, ";");
    CC_IncludeModule(self, name);
    RR_String_Kill(name);
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
        || class == CLASS_FUNCTION_PROTOTYPE
        || class == CLASS_FUNCTION_PROTOTYPE_NATIVE;
}

static void
CC_Define(CC* self, Class class, int stack, RR_String* ident, RR_String* path)
{
    Meta* old = RR_Map_Get(self->identifiers, RR_String_Value(ident));
    Meta* new = Meta_Init(class, stack, path);
    if(old)
    {
        // Only function prototypes can be upgraded to functions.
        if(old->class == CLASS_FUNCTION_PROTOTYPE
        && new->class == CLASS_FUNCTION)
        {
            if(new->stack != old->stack)
                CC_Quit(self, "function `%s` with `%d` argument(s) was previously defined as a function prototype with `%d` argument(s)", RR_String_Value(ident), new->stack, old->stack);
        }
        else
            CC_Quit(self, "%s `%s` was already defined as a %s", Class_ToString(new->class), RR_String_Value(ident), Class_ToString(old->class));
    }
    RR_Map_Set(self->identifiers, ident, new);
}

static void
CC_ConsumeExpression(CC* self)
{
    CC_Expression(self);
    RR_Queue_PshB(self->assembly, RR_String_Init("\tpop"));
}

static void
CC_EmptyExpression(CC* self)
{
    CC_ConsumeExpression(self);
    CC_Match(self, ";");
}

static void
CC_Assign(CC* self)
{
    CC_Match(self, ":=");
    CC_Expression(self);
    CC_Match(self, ";");
}

static void
CC_Local(CC* self, RR_String* ident)
{
    CC_Define(self, CLASS_VARIABLE_LOCAL, self->locals, ident, RR_String_Init(""));
    self->locals += 1;
}

static void
CC_AssignLocal(CC* self, RR_String* ident)
{
    CC_Assign(self);
    CC_Local(self, ident);
}

static RR_String*
CC_Global(CC* self, RR_String* ident)
{
    RR_String* label = RR_String_Format("!%s", RR_String_Value(ident));
    RR_Queue_PshB(self->assembly, RR_String_Format("%s:", RR_String_Value(label)));
    CC_Assign(self);
    CC_Define(self, CLASS_VARIABLE_GLOBAL, self->globals, ident, RR_String_Init(""));
    RR_Queue_PshB(self->assembly, RR_String_Init("\tret"));
    self->globals += 1;
    return label;
}

static RR_Queue*
CC_ParamRoll(CC* self)
{
    RR_Queue* params = RR_Queue_Init((RR_Kill) RR_String_Kill, (RR_Copy) NULL);
    CC_Match(self, "(");
    while(CC_Next(self) != ')')
    {
        RR_String* ident = CC_Ident(self);
        RR_Queue_PshB(params, ident);
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    CC_Match(self, ")");
    return params;
}

static void
CC_DefineParams(CC* self, RR_Queue* params)
{
    self->locals = 0;
    for(int i = 0; i < RR_Queue_Size(params); i++)
    {
        RR_String* ident = RR_String_Copy(RR_Queue_Get(params, i));
        CC_Local(self, ident);
    }
}

static int
CC_PopScope(CC* self, RR_Queue* scope)
{
    int popped = RR_Queue_Size(scope);
    for(int i = 0; i < popped; i++)
    {
        RR_String* key = RR_Queue_Get(scope, i);
        RR_Map_Del(self->identifiers, RR_String_Value(key));
        self->locals -= 1;
    }
    for(int i = 0; i < popped; i++)
        RR_Queue_PshB(self->assembly, RR_String_Init("\tpop"));
    RR_Queue_Kill(scope);
    return popped;
}

static Meta*
CC_Meta(CC* self, RR_String* ident)
{
    Meta* meta = RR_Map_Get(self->identifiers, RR_String_Value(ident));
    if(meta == NULL)
        CC_Quit(self, "identifier `%s` not defined", RR_String_Value(ident));
    return meta;
}

static Meta*
CC_Expect(CC* self, RR_String* ident, bool clause(Class))
{
    Meta* meta = CC_Meta(self, ident);
    if(!clause(meta->class))
        CC_Quit(self, "identifier `%s` cannot be of class `%s`", RR_String_Value(ident), Class_ToString(meta->class));
    return meta;
}

static void
CC_Ref(CC* self, RR_String* ident)
{
    Meta* meta = CC_Expect(self, ident, CC_IsVariable);
    if(meta->class == CLASS_VARIABLE_GLOBAL)
        RR_Queue_PshB(self->assembly, RR_String_Format("\tglb %d", meta->stack));
    else
    if(meta->class == CLASS_VARIABLE_LOCAL)
        RR_Queue_PshB(self->assembly, RR_String_Format("\tloc %d", meta->stack));
}

static void
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
            RR_Queue_PshB(self->assembly, RR_String_Init("\tcpy"));
            RR_Queue_PshB(self->assembly, RR_String_Init("\tins"));
        }
        else
            RR_Queue_PshB(self->assembly, RR_String_Init("\tget"));
    }
}

static int
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

static void
CC_Call(CC* self, RR_String* ident, int args)
{
    for(int i = 0; i < args; i++)
        RR_Queue_PshB(self->assembly, RR_String_Init("\tspd"));
    RR_Queue_PshB(self->assembly, RR_String_Format("\tcal %s", RR_String_Value(ident)));
    RR_Queue_PshB(self->assembly, RR_String_Init("\tlod"));
}

static void
CC_IndirectCalling(CC* self, RR_String* ident, int args)
{
    CC_Ref(self, ident);
    RR_Queue_PshB(self->assembly, RR_String_Format("\tpsh %d", args));
    RR_Queue_PshB(self->assembly, RR_String_Init("\tvrt"));
    RR_Queue_PshB(self->assembly, RR_String_Init("\tlod"));
}

static void
CC_NativeCalling(CC* self, RR_String* ident, Meta* meta)
{
    RR_Queue_PshB(self->assembly, RR_String_Format("\tpsh \"%s\"", RR_String_Value(meta->path)));
    RR_Queue_PshB(self->assembly, RR_String_Format("\tpsh \"%s\"", RR_String_Value(ident)));
    RR_Queue_PshB(self->assembly, RR_String_Format("\tpsh %d", meta->stack));
    RR_Queue_PshB(self->assembly, RR_String_Init("\tdll"));
}

static void
CC_Map(CC* self)
{
    RR_Queue_PshB(self->assembly, RR_String_Init("\tpsh {}"));
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
            RR_Queue_PshB(self->assembly, RR_String_Init("\tpsh true"));
        RR_Queue_PshB(self->assembly, RR_String_Init("\tcpy"));
        RR_Queue_PshB(self->assembly, RR_String_Init("\tins"));
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    CC_Match(self, "}");
}

static void
CC_Queue(CC* self)
{
    RR_Queue_PshB(self->assembly, RR_String_Init("\tpsh []"));
    CC_Match(self, "[");
    while(CC_Next(self) != ']')
    {
        CC_Expression(self);
        RR_Queue_PshB(self->assembly, RR_String_Init("\tcpy"));
        RR_Queue_PshB(self->assembly, RR_String_Init("\tpsb"));
        if(CC_Next(self) == ',')
            CC_Match(self, ",");
    }
    CC_Match(self, "]");
}

static void
CC_Not(CC* self)
{
    CC_Match(self, "!");
    CC_Factor(self);
    RR_Queue_PshB(self->assembly, RR_String_Init("\tnot"));
}

static void
CC_Direct(CC* self, bool negative)
{
    RR_String* number = CC_Number(self);
    RR_Queue_PshB(self->assembly, RR_String_Format("\tpsh %s%s", negative ? "-" : "", RR_String_Value(number)));
    RR_String_Kill(number);
}

static void
CC_DirectCalling(CC* self, RR_String* ident, int args)
{
    if(RR_String_Equals(ident, "Len"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\tlen"));
    else
    if(RR_String_Equals(ident, "Good"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\tgod"));
    else
    if(RR_String_Equals(ident, "Key"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\tkey"));
    else
    if(RR_String_Equals(ident, "Sort"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\tsrt"));
    else
    if(RR_String_Equals(ident, "Open"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\topn"));
    else
    if(RR_String_Equals(ident, "Read"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\tred"));
    else
    if(RR_String_Equals(ident, "Copy"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\tcpy"));
    else
    if(RR_String_Equals(ident, "Write"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\twrt"));
    else
    if(RR_String_Equals(ident, "Refs"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\tref"));
    else
    if(RR_String_Equals(ident, "Del"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\tdel"));
    else
    if(RR_String_Equals(ident, "Exit"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\text"));
    else
    if(RR_String_Equals(ident, "Type"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\ttyp"));
    else
    if(RR_String_Equals(ident, "Print"))
        RR_Queue_PshB(self->assembly, RR_String_Init("\tprt"));
    else
        CC_Call(self, ident, args);
}

static void
CC_Calling(CC* self, RR_String* ident)
{
    Meta* meta = CC_Meta(self, ident);
    int args = CC_Args(self);
    if(CC_IsFunction(meta->class))
    {
        if(args != meta->stack)
            CC_Quit(self, "calling function `%s` required `%d` arguments but got `%d` arguments", RR_String_Value(ident), meta->stack, args);
        if(meta->class == CLASS_FUNCTION_PROTOTYPE_NATIVE)
            CC_NativeCalling(self, ident, meta);
        else
            CC_DirectCalling(self, ident, args);
    }
    else
    if(CC_IsVariable(meta->class))
        CC_IndirectCalling(self, ident, args);
    else
        CC_Quit(self, "identifier `%s` is not callable", RR_String_Value(ident));
}

static bool
CC_Referencing(CC* self, RR_String* ident)
{
    Meta* meta = CC_Meta(self, ident);
    if(CC_IsFunction(meta->class))
        RR_Queue_PshB(self->assembly, RR_String_Format("\tpsh @%s,%d", RR_String_Value(ident), meta->stack));
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
    RR_String* ident = NULL;
    if(self->prime)
        RR_String_Swap(&self->prime, &ident);
    else
        ident = CC_Ident(self);
    if(RR_String_IsBoolean(ident))
        RR_Queue_PshB(self->assembly, RR_String_Format("\tpsh %s", RR_String_Value(ident)));
    else
    if(RR_String_IsNull(ident))
        RR_Queue_PshB(self->assembly, RR_String_Format("\tpsh %s", RR_String_Value(ident)));
    else
    if(CC_Next(self) == '(')
        CC_Calling(self, ident);
    else
        storage = CC_Referencing(self, ident);
    RR_String_Kill(ident);
    return storage;
}

static void
CC_ReserveFunctions(CC* self)
{
    struct { int args; char* name; } items[] = {
        { 2, "Sort"  },
        { 1, "Good"  },
        { 1, "Key"   },
        { 1, "Copy"  },
        { 2, "Open"  },
        { 2, "Read"  },
        { 1, "Close" },
        { 1, "Refs"  },
        { 1, "Len"   },
        { 2, "Del"   },
        { 1, "Exit"  },
        { 1, "Type"  },
        { 1, "Print" },
        { 2, "Write" },
    };
    for(int i = 0; i < (int) RR_LEN(items); i++)
        CC_Define(self, CLASS_FUNCTION, items[i].args, RR_String_Init(items[i].name), RR_String_Init(""));
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
    RR_String* string = CC_StringStream(self);
    RR_Queue_PshB(self->assembly, RR_String_Format("\tpsh \"%s\"", RR_String_Value(string)));
    RR_String_Kill(string);
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

static bool
CC_Factor(CC* self)
{
    bool storage = false;
    int next = CC_Next(self);
    if(next == '!')
        CC_Not(self);
    else
    if(CC_String_IsDigit(next))
        CC_Direct(self, false);
    else
    if(CC_String_IsIdent(next) || self->prime)
        storage = CC_Identifier(self);
    else
    if(next == '-')
        CC_DirectNeg(self);
    else
    if(next == '+')
        CC_DirectPos(self);
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
        RR_String* operator = CC_Operator(self);
        if(RR_String_Equals(operator, "*="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator `%s` must be storage", operator);
            CC_Expression(self);
            RR_Queue_PshB(self->assembly, RR_String_Init("\tmul"));
        }
        else
        if(RR_String_Equals(operator, "/="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator `%s` must be storage", operator);
            CC_Expression(self);
            RR_Queue_PshB(self->assembly, RR_String_Init("\tdiv"));
        }
        else
        {
            storage = false;
            if(RR_String_Equals(operator, "?"))
            {
                CC_Factor(self);
                RR_Queue_PshB(self->assembly, RR_String_Init("\tmem"));
            }
            else
            {
                RR_Queue_PshB(self->assembly, RR_String_Init("\tcpy"));
                CC_Factor(self);
                if(RR_String_Equals(operator, "*"))
                    RR_Queue_PshB(self->assembly, RR_String_Init("\tmul"));
                else
                if(RR_String_Equals(operator, "/"))
                    RR_Queue_PshB(self->assembly, RR_String_Init("\tdiv"));
                else
                if(RR_String_Equals(operator, "%"))
                    RR_Queue_PshB(self->assembly, RR_String_Init("\tfmt"));
                else
                if(RR_String_Equals(operator, "||"))
                    RR_Queue_PshB(self->assembly, RR_String_Init("\tlor"));
                else
                    CC_Quit(self, "operator `%s` is not supported for factors", RR_String_Value(operator));
            }
        }
        RR_String_Kill(operator);
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
        RR_String* operator = CC_Operator(self);
        if(RR_String_Equals(operator, "="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator `%s` must be storage", operator);
            CC_Expression(self);
            RR_Queue_PshB(self->assembly, RR_String_Init("\tcpy"));
            RR_Queue_PshB(self->assembly, RR_String_Init("\tmov"));
        }
        else
        if(RR_String_Equals(operator, "+="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator `%s` must be storage", operator);
            CC_Expression(self);
            RR_Queue_PshB(self->assembly, RR_String_Init("\tcpy")); // Incase of self referential list push back.
            RR_Queue_PshB(self->assembly, RR_String_Init("\tadd"));
        }
        else
        if(RR_String_Equals(operator, "-="))
        {
            if(!storage)
                CC_Quit(self, "the left hand side of operator `%s` must be storage", operator);
            CC_Expression(self);
            RR_Queue_PshB(self->assembly, RR_String_Init("\tcpy")); // Incase of self referential list push front.
            RR_Queue_PshB(self->assembly, RR_String_Init("\tsub"));
        }
        else
        if(RR_String_Equals(operator, "=="))
        {
            CC_Expression(self);
            RR_Queue_PshB(self->assembly, RR_String_Init("\teql"));
        }
        else
        if(RR_String_Equals(operator, "!="))
        {
            CC_Expression(self);
            RR_Queue_PshB(self->assembly, RR_String_Init("\tneq"));
        }
        else
        if(RR_String_Equals(operator, ">"))
        {
            CC_Expression(self);
            RR_Queue_PshB(self->assembly, RR_String_Init("\tgrt"));
        }
        else
        if(RR_String_Equals(operator, "<"))
        {
            CC_Expression(self);
            RR_Queue_PshB(self->assembly, RR_String_Init("\tlst"));
        }
        else
        if(RR_String_Equals(operator, ">="))
        {
            CC_Expression(self);
            RR_Queue_PshB(self->assembly, RR_String_Init("\tgte"));
        }
        else
        if(RR_String_Equals(operator, "<="))
        {
            CC_Expression(self);
            RR_Queue_PshB(self->assembly, RR_String_Init("\tlte"));
        }
        else
        {
            storage = false;
            RR_Queue_PshB(self->assembly, RR_String_Init("\tcpy"));
            CC_Term(self);
            if(RR_String_Equals(operator, "+"))
                RR_Queue_PshB(self->assembly, RR_String_Init("\tadd"));
            else
            if(RR_String_Equals(operator, "-"))
                RR_Queue_PshB(self->assembly, RR_String_Init("\tsub"));
            else
            if(RR_String_Equals(operator, "&&"))
                RR_Queue_PshB(self->assembly, RR_String_Init("\tand"));
            else
                CC_Quit(self, "operator `%s` is not supported for terms", RR_String_Value(operator));
        }
        RR_String_Kill(operator);
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
CC_Branch(CC* self, int head, int tail, int end, bool loop)
{
    int next = CC_Label(self);
    CC_Match(self, "(");
    CC_Expression(self);
    CC_Match(self, ")");
    RR_Queue_PshB(self->assembly, RR_String_Format("\tbrf @l%d", next));
    CC_Block(self, head, tail, loop);
    RR_Queue_PshB(self->assembly, RR_String_Format("\tjmp @l%d", end));
    RR_Queue_PshB(self->assembly, RR_String_Format("@l%d:", next));
}

static RR_String*
CC_Branches(CC* self, int head, int tail, int loop)
{
    int end = CC_Label(self);
    CC_Branch(self, head, tail, end, loop);
    RR_String* buffer = CC_Ident(self);
    while(RR_String_Equals(buffer, "elif"))
    {
        CC_Branch(self, head, tail, end, loop);
        RR_String_Kill(buffer);
        buffer = CC_Ident(self);
    }
    if(RR_String_Equals(buffer, "else"))
        CC_Block(self, head, tail, loop);
    RR_Queue_PshB(self->assembly, RR_String_Format("@l%d:", end));
    RR_String* backup = NULL;
    if(RR_String_Empty(buffer) || RR_String_Equals(buffer, "elif") || RR_String_Equals(buffer, "else"))
        RR_String_Kill(buffer);
    else
        backup = buffer;
    return backup;
}

static void
CC_While(CC* self)
{
    int A = CC_Label(self);
    int B = CC_Label(self);
    RR_Queue_PshB(self->assembly, RR_String_Format("@l%d:", A));
    CC_Match(self, "(");
    CC_Expression(self);
    RR_Queue_PshB(self->assembly, RR_String_Format("\tbrf @l%d", B));
    CC_Match(self, ")");
    CC_Block(self, A, B, true);
    RR_Queue_PshB(self->assembly, RR_String_Format("\tjmp @l%d", A));
    RR_Queue_PshB(self->assembly, RR_String_Format("@l%d:", B));
}

static void
CC_For(CC* self)
{
    int A = CC_Label(self);
    int B = CC_Label(self);
    int C = CC_Label(self);
    int D = CC_Label(self);
    RR_Queue* init = RR_Queue_Init((RR_Kill) RR_String_Kill, (RR_Copy) NULL);
    CC_Match(self, "(");
    RR_String* ident = CC_Ident(self);
    RR_Queue_PshB(init, RR_String_Copy(ident));
    CC_AssignLocal(self, ident);
    RR_Queue_PshB(self->assembly, RR_String_Format("@l%d:", A));
    CC_Expression(self);
    CC_Match(self, ";");
    RR_Queue_PshB(self->assembly, RR_String_Format("\tbrf @l%d", D));
    RR_Queue_PshB(self->assembly, RR_String_Format("\tjmp @l%d", C));
    RR_Queue_PshB(self->assembly, RR_String_Format("@l%d:", B));
    CC_ConsumeExpression(self);
    CC_Match(self, ")");
    RR_Queue_PshB(self->assembly, RR_String_Format("\tjmp @l%d", A));
    RR_Queue_PshB(self->assembly, RR_String_Format("@l%d:", C));
    CC_Block(self, B, D, true);
    RR_Queue_PshB(self->assembly, RR_String_Format("\tjmp @l%d", B));
    RR_Queue_PshB(self->assembly, RR_String_Format("@l%d:", D));
    CC_PopScope(self, init);
}

static void
CC_Ret(CC* self)
{
    CC_Expression(self);
    RR_Queue_PshB(self->assembly, RR_String_Init("\tsav"));
    RR_Queue_PshB(self->assembly, RR_String_Init("\tfls"));
    CC_Match(self, ";");
}

static void
CC_Block(CC* self, int head, int tail, bool loop)
{
    RR_Queue* scope = RR_Queue_Init((RR_Kill) RR_String_Kill, (RR_Copy) NULL);
    CC_Match(self, "{");
    RR_String* prime = NULL; 
    while(CC_Next(self) != '}')
    {
        if(CC_String_IsIdentLeader(CC_Next(self)) || prime)
        {
            RR_String* ident = NULL;
            if(prime)
                RR_String_Swap(&prime, &ident);
            else
                ident = CC_Ident(self);
            if(RR_String_Equals(ident, "if"))
                prime = CC_Branches(self, head, tail, loop);
            else
            if(RR_String_Equals(ident, "elif"))
                CC_Quit(self, "the keyword `elif` must follow an `if` or `elif` block");
            else
            if(RR_String_Equals(ident, "else"))
                CC_Quit(self, "the keyword `else` must follow an `if` or `elif` block");
            else
            if(RR_String_Equals(ident, "while"))
                CC_While(self);
            else
            if(RR_String_Equals(ident, "for"))
                CC_For(self);
            else
            if(RR_String_Equals(ident, "ret"))
                CC_Ret(self);
            else
            if(RR_String_Equals(ident, "continue"))
            {
                if(loop)
                {
                    CC_Match(self, ";");
                    RR_Queue_PshB(self->assembly, RR_String_Format("\tjmp @l%d", head));
                }
                else
                    CC_Quit(self, "the keyword `continue` can only be used within `while` or for `loops`");
            }
            else
            if(RR_String_Equals(ident, "break"))
            {
                if(loop)
                {
                    CC_Match(self, ";");
                    RR_Queue_PshB(self->assembly, RR_String_Format("\tjmp @l%d", tail));
                }
                else
                    CC_Quit(self, "the keyword `break` can only be used within `while` or `for` loops");
            }
            else
            if(CC_Next(self) == ':')
            {
                RR_Queue_PshB(scope, RR_String_Copy(ident));
                CC_AssignLocal(self, RR_String_Copy(ident));
            }
            else
            {
                self->prime = RR_String_Copy(ident);
                CC_EmptyExpression(self);
            }
            RR_String_Kill(ident);
        }
        else
            CC_EmptyExpression(self);
    }
    CC_Match(self, "}");
    CC_PopScope(self, scope);
}

static void
CC_FunctionPrototypeNative(CC* self, RR_Queue* params, RR_String* ident, RR_String* path)
{
    CC_Define(self, CLASS_FUNCTION_PROTOTYPE_NATIVE, RR_Queue_Size(params), ident, path);
    RR_Queue_Kill(params);
    CC_Match(self, ";");
}

static void
CC_FunctionPrototype(CC* self, RR_Queue* params, RR_String* ident)
{
    CC_Define(self, CLASS_FUNCTION_PROTOTYPE, RR_Queue_Size(params), ident, RR_String_Init(""));
    RR_Queue_Kill(params);
    CC_Match(self, ";");
}

static RR_String*
CC_GetModuleLibs(RR_String* source)
{
    RR_File* file = RR_File_Init(RR_String_Copy(source), RR_String_Init("r"));
    FILE* fp = RR_File_File(file);
    char* line = NULL;
    size_t len = 0;
    getline(&line, &len, fp);
    RR_File_Kill(file);
    size_t expect = 3;
    if(len > expect)
    {
        if(line[0] == '/'
        && line[1] == '/'
        && line[2] == '!')
        {
            RR_String* libs = RR_String_Init(line + expect);
            RR_Free(line);
            return libs;
        }
    }
    RR_Free(line);
    return RR_String_Init("");;
}

static void
CC_ImportModule(CC* self, RR_String* file)
{
    char* real = CC_RealPath(self, file);
    RR_String* source = RR_String_Init(real);
    RR_String* library = RR_String_Init(real);
    RR_String_PopB(library);
    RR_String_Appends(library, "so");
    if(RR_Map_Exists(self->included, RR_String_Value(library)) == false)
    {
        RR_String* libs = CC_GetModuleLibs(source);
        RR_String* command = RR_String_Format("gcc -O3 -march=native -std=gnu99 -Wall -Wextra -Wpedantic %s -fpic -shared -o %s -l%s %s", RR_String_Value(source), RR_String_Value(library), RR_LIBROMAN2, RR_String_Value(libs));
        int ret = system(RR_String_Value(command));
        if(ret != 0)
            CC_Quit(self, "compilation errors encountered while compiling native library `%s`", RR_String_Value(source));
        RR_String_Kill(command);
        RR_String_Kill(libs);
        RR_Map_Set(self->included, RR_String_Copy(library), NULL);
    }
    CC_Match(self, "{");
    while(CC_Next(self) != '}')
    {
        RR_String* ident = CC_Ident(self);
        RR_Queue* params = CC_ParamRoll(self);
        CC_FunctionPrototypeNative(self, params, ident, RR_String_Copy(library));
    }
    CC_Match(self, "}");
    CC_Match(self, ";");
    RR_String_Kill(library);
    RR_String_Kill(source);
    RR_Free(real);
}

static void
CC_Import(CC* self)
{
    RR_String* name = CC_ModuleName(self, ".c");
    CC_ImportModule(self, name);
    RR_String_Kill(name);
}

static void
CC_Function(CC* self, RR_String* ident)
{
    RR_Queue* params = CC_ParamRoll(self);
    if(CC_Next(self) == '{')
    {
        CC_DefineParams(self, params);
        CC_Define(self, CLASS_FUNCTION, RR_Queue_Size(params), ident, RR_String_Init(""));
        RR_Queue_PshB(self->assembly, RR_String_Format("%s:", RR_String_Value(ident)));
        CC_Block(self, 0, 0, false);
        CC_PopScope(self, params);
        RR_Queue_PshB(self->assembly, RR_String_Init("\tpsh null"));
        RR_Queue_PshB(self->assembly, RR_String_Init("\tsav"));
        RR_Queue_PshB(self->assembly, RR_String_Init("\tret"));
    }
    else
        CC_FunctionPrototype(self, params, ident);
}

static void
CC_Spool(CC* self, RR_Queue* start)
{
    RR_Queue* spool = RR_Queue_Init((RR_Kill) RR_String_Kill, NULL);
    RR_String* main = RR_String_Init("Main");
    CC_Expect(self, main, CC_IsFunction);
    RR_Queue_PshB(spool, RR_String_Init("!start:"));
    for(int i = 0; i < RR_Queue_Size(start); i++)
    {
        RR_String* label = RR_Queue_Get(start, i);
        RR_Queue_PshB(spool, RR_String_Format("\tcal %s", RR_String_Value(label)));
    }
    RR_Queue_PshB(spool, RR_String_Format("\tcal %s", RR_String_Value(main)));
    RR_Queue_PshB(spool, RR_String_Format("\tend"));
    for(int i = RR_Queue_Size(spool) - 1; i >= 0; i--)
        RR_Queue_PshF(self->assembly, RR_String_Copy(RR_Queue_Get(spool, i)));
    RR_String_Kill(main);
    RR_Queue_Kill(spool);
}

static void
CC_Parse(CC* self)
{
    RR_Queue* start = RR_Queue_Init((RR_Kill) RR_String_Kill, (RR_Copy) NULL);
    while(CC_Peak(self) != EOF)
    {
        RR_String* ident = CC_Ident(self);
        if(RR_String_Equals(ident, "inc"))
        {
            CC_Include(self);
            RR_String_Kill(ident);
        }
        else
        if(RR_String_Equals(ident, "imp"))
        {
            CC_Import(self);
            RR_String_Kill(ident);
        }
        else
        if(CC_Next(self) == '(')
            CC_Function(self, ident);
        else
        if(CC_Next(self) == ':')
        {
            RR_String* label = CC_Global(self, ident);
            RR_Queue_PshB(start, label);
        }
        else
            CC_Quit(self, "`%s` must either be a function or function prototype, a global variable, an import statement, or an include statement", RR_String_Value(ident));
        CC_Spin(self);
    }
    CC_Spool(self, start);
    RR_Queue_Kill(start);
}

static RR_Map*
ASM_Label(RR_Queue* assembly, int* size)
{
    RR_Map* labels = RR_Map_Init((RR_Kill) RR_Free, (RR_Copy) NULL);
    for(int i = 0; i < RR_Queue_Size(assembly); i++)
    {
        RR_String* stub = RR_Queue_Get(assembly, i);
        if(RR_String_Value(stub)[0] == '\t')
            *size += 1;
        else 
        {
            RR_String* line = RR_String_Copy(stub);
            char* label = strtok(RR_String_Value(line), ":");
            if(RR_Map_Exists(labels, label))
                Quit("asm: label %s already defined", label);
            RR_Map_Set(labels, RR_String_Init(label), Int_Init(*size));
            RR_String_Kill(line);
        }
    }
    return labels;
}

static void
ASM_Dump(RR_Queue* assembly)
{
    int instructions = 0;
    int labels = 0;
    for(int i = 0; i < RR_Queue_Size(assembly); i++)
    {
        RR_String* assem = RR_Queue_Get(assembly, i);
        if(RR_String_Value(assem)[0] == '\t')
            instructions += 1;
        else
            labels += 1;
        puts(RR_String_Value(assem));
    }
    printf("instructions %d : labels %d\n", instructions, labels);
}

static void
VMTypeExpect(VM* self, RR_Type a, RR_Type b)
{
    (void) self;
    if(a != b)
        Quit("`vm: encountered type `%s` but expected type `%s`", RR_Type_ToString(a), RR_Type_ToString(b));
}

static void
VM_BadOperator(VM* self, RR_Type a, const char* op)
{
    (void) self;
    Quit("vm: type `%s` not supported with operator `%s`", RR_Type_ToString(a), op);
}

static void
VMTypeMatch(VM* self, RR_Type a, RR_Type b, const char* op)
{
    (void) self;
    if(a != b)
        Quit("vm: type `%s` and type `%s` mismatch with operator `%s`", RR_Type_ToString(a), RR_Type_ToString(b), op);
}

static void
VM_OutOfBounds(VM* self, RR_Type a, int index)
{
    (void) self;
    Quit("vm: type `%s` was accessed out of bounds with index `%d`", RR_Type_ToString(a), index);
}

static void
VMTypeBadIndex(VM* self, RR_Type a, RR_Type b)
{
    (void) self;
    Quit("vm: type `%s` cannot be indexed with type `%s`", RR_Type_ToString(a), RR_Type_ToString(b));
}

static void
VMTypeBad(VM* self, RR_Type a)
{
    (void) self;
    Quit("vm: type `%s` cannot be used for this operation", RR_Type_ToString(a));
}

static void
VM_ArgMatch(VM* self, int a, int b)
{
    (void) self;
    if(a != b)
        Quit("vm: expected `%d` arguments but encountered `%d` arguments", a, b);
}

static void
VM_UnknownEscapeChar(VM* self, int esc)
{
    (void) self;
    Quit("vm: an unknown escape character `0x%02X` was encountered\n", esc);
}

static void
VM_RefImpurity(VM* self, RR_Value* value)
{
    (void) self;
    RR_String* print = RR_Value_Sprint(value, true, 0);
    Quit("vm: the .data segment value `%s` contained `%d` references at the time of exit", RR_String_Value(print), RR_Value_Refs(value));
}

static VM*
VM_Init(int size)
{
    VM* self = RR_Malloc(sizeof(*self));
    self->data = RR_Queue_Init((RR_Kill) RR_Value_Kill, (RR_Copy) NULL);
    self->stack = RR_Queue_Init((RR_Kill) RR_Value_Kill, (RR_Copy) NULL);
    self->frame = RR_Queue_Init((RR_Kill) Frame_RR_Free, (RR_Copy) NULL);
    self->track = RR_Map_Init((RR_Kill) Int_Kill, (RR_Copy) NULL);
    self->ret = NULL;
    self->size = size;
    self->instructions = RR_Malloc(size * sizeof(*self->instructions));
    self->pc = 0;
    self->spds = 0;
    return self;
}

static void
VM_Kill(VM* self)
{
    RR_Queue_Kill(self->data);
    RR_Queue_Kill(self->stack);
    RR_Queue_Kill(self->frame);
    RR_Map_Kill(self->track);
    RR_Free(self->instructions);
    RR_Free(self);
}

static void
VM_Data(VM* self)
{
    printf(".data:\n");
    for(int i = 0; i < RR_Queue_Size(self->data); i++)
    {
        RR_Value* value = RR_Queue_Get(self->data, i);
        printf("%4d : %2d : ", i, RR_Value_Refs(value));
        RR_Value_Print(value);
    }
}

static void
VM_AssertRefCounts(VM* self)
{
    for(int i = 0; i < RR_Queue_Size(self->data); i++)
    {
        RR_Value* value = RR_Queue_Get(self->data, i);
        if(RR_Value_Refs(value) > 0)
            VM_RefImpurity(self, value);
    }
}

static void
VM_Pop(VM* self)
{
    RR_Queue_PopB(self->stack);
}

static RR_String*
VM_ConvertEscs(VM* self, char* chars)
{
    int len = strlen(chars);
    RR_String* string = RR_String_Init("");
    for(int i = 1; i < len - 1; i++)
    {
        char ch = chars[i];
        if(ch == '\\')
        {
            i += 1;
            int esc = chars[i];
            ch = CC_String_EscToByte(esc);
            if(ch == -1)
                VM_UnknownEscapeChar(self, esc);
        }
        RR_String_PshB(string, ch);
    }
    return string;
}

static void
VM_Store(VM* self, RR_Map* labels, char* operand)
{
    RR_Value* value;
    char ch = operand[0];
    if(ch == '[')
        value = RR_Value_NewQueue();
    else
    if(ch == '{')
        value = RR_Value_NewMap();
    else
    if(ch == '"')
    {
        RR_String* string = VM_ConvertEscs(self, operand);
        value = RR_Value_NewString(string);
    }
    else
    if(ch == '@')
    {
        RR_String* name = RR_String_Init(strtok(operand + 1, ","));
        int size = atoi(strtok(NULL, " \n"));
        int* address = RR_Map_Get(labels, RR_String_Value(name));
        if(address == NULL)
            Quit("asm: label `%s` not defined", name);
        value = RR_Value_NewFunction(RR_Function_Init(name, size, *address));
    }
    else
    if(ch == 't' || ch == 'f')
        value = RR_Value_NewBool(RR_Equals(operand, "true") ? true : false);
    else
    if(ch == 'n')
        value = RR_Value_NewNull();
    else
    if(CC_String_IsDigit(ch) || ch == '-')
        value = RR_Value_NewNumber(atof(operand));
    else
    {
        value = NULL;
        Quit("vm: unknown psh operand `%s` encountered", operand);
    }
    RR_Queue_PshB(self->data, value);
}

static int
VM_Datum(VM* self, RR_Map* labels, char* operand)
{
    VM_Store(self, labels, operand);
    return ((RR_Queue_Size(self->data) - 1) << 8) | OPCODE_PSH;
}

static int
VM_Indirect(Opcode oc, RR_Map* labels, char* label)
{
    int* address = RR_Map_Get(labels, label);
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
VM_Assemble(RR_Queue* assembly)
{
    int size = 0;
    RR_Map* labels = ASM_Label(assembly, &size);
    VM* self = VM_Init(size);
    int pc = 0;
    for(int i = 0; i < RR_Queue_Size(assembly); i++)
    {
        RR_String* stub = RR_Queue_Get(assembly, i);
        if(RR_String_Value(stub)[0] == '\t')
        {
            RR_String* line = RR_String_Init(RR_String_Value(stub) + 1);
            int instruction = 0;
            char* mnemonic = strtok(RR_String_Value(line), " \n");
            if(RR_Equals(mnemonic, "add"))
                instruction = OPCODE_ADD;
            else
            if(RR_Equals(mnemonic, "and"))
                instruction = OPCODE_AND;
            else
            if(RR_Equals(mnemonic, "brf"))
                instruction = VM_Indirect(OPCODE_BRF, labels, strtok(NULL, "\n"));
            else
            if(RR_Equals(mnemonic, "cal"))
                instruction = VM_Indirect(OPCODE_CAL, labels, strtok(NULL, "\n"));
            else
            if(RR_Equals(mnemonic, "cpy"))
                instruction = OPCODE_CPY;
            else
            if(RR_Equals(mnemonic, "del"))
                instruction = OPCODE_DEL;
            else
            if(RR_Equals(mnemonic, "div"))
                instruction = OPCODE_DIV;
            else
            if(RR_Equals(mnemonic, "dll"))
                instruction = OPCODE_DLL;
            else
            if(RR_Equals(mnemonic, "end"))
                instruction = OPCODE_END;
            else
            if(RR_Equals(mnemonic, "eql"))
                instruction = OPCODE_EQL;
            else
            if(RR_Equals(mnemonic, "ext"))
                instruction = OPCODE_EXT;
            else
            if(RR_Equals(mnemonic, "fls"))
                instruction = OPCODE_FLS;
            else
            if(RR_Equals(mnemonic, "fmt"))
                instruction = OPCODE_FMT;
            else
            if(RR_Equals(mnemonic, "get"))
                instruction = OPCODE_GET;
            else
            if(RR_Equals(mnemonic, "glb"))
                instruction = VM_Direct(OPCODE_GLB, strtok(NULL, "\n"));
            else
            if(RR_Equals(mnemonic, "god"))
                instruction = OPCODE_GOD;
            else
            if(RR_Equals(mnemonic, "grt"))
                instruction = OPCODE_GRT;
            else
            if(RR_Equals(mnemonic, "gte"))
                instruction = OPCODE_GTE;
            else
            if(RR_Equals(mnemonic, "ins"))
                instruction = OPCODE_INS;
            else
            if(RR_Equals(mnemonic, "jmp"))
                instruction = VM_Indirect(OPCODE_JMP, labels, strtok(NULL, "\n"));
            else
            if(RR_Equals(mnemonic, "key"))
                instruction = OPCODE_KEY;
            else
            if(RR_Equals(mnemonic, "len"))
                instruction = OPCODE_LEN;
            else
            if(RR_Equals(mnemonic, "loc"))
                instruction = VM_Direct(OPCODE_LOC, strtok(NULL, "\n"));
            else
            if(RR_Equals(mnemonic, "lod"))
                instruction = OPCODE_LOD;
            else
            if(RR_Equals(mnemonic, "lor"))
                instruction = OPCODE_LOR;
            else
            if(RR_Equals(mnemonic, "lst"))
                instruction = OPCODE_LST;
            else
            if(RR_Equals(mnemonic, "lte"))
                instruction = OPCODE_LTE;
            else
            if(RR_Equals(mnemonic, "mem"))
                instruction = OPCODE_MEM;
            else
            if(RR_Equals(mnemonic, "mov"))
                instruction = OPCODE_MOV;
            else
            if(RR_Equals(mnemonic, "mul"))
                instruction = OPCODE_MUL;
            else
            if(RR_Equals(mnemonic, "neq"))
                instruction = OPCODE_NEQ;
            else
            if(RR_Equals(mnemonic, "not"))
                instruction = OPCODE_NOT;
            else
            if(RR_Equals(mnemonic, "opn"))
                instruction = OPCODE_OPN;
            else
            if(RR_Equals(mnemonic, "pop"))
                instruction = OPCODE_POP;
            else
            if(RR_Equals(mnemonic, "prt"))
                instruction = OPCODE_PRT;
            else
            if(RR_Equals(mnemonic, "psb"))
                instruction = OPCODE_PSB;
            else
            if(RR_Equals(mnemonic, "psf"))
                instruction = OPCODE_PSF;
            else
            if(RR_Equals(mnemonic, "psh"))
                instruction = VM_Datum(self, labels, strtok(NULL, "\n"));
            else
            if(RR_Equals(mnemonic, "red"))
                instruction = OPCODE_RED;
            else
            if(RR_Equals(mnemonic, "ref"))
                instruction = OPCODE_REF;
            else
            if(RR_Equals(mnemonic, "ret"))
                instruction = OPCODE_RET;
            else
            if(RR_Equals(mnemonic, "sav"))
                instruction = OPCODE_SAV;
            else
            if(RR_Equals(mnemonic, "spd"))
                instruction = OPCODE_SPD;
            else
            if(RR_Equals(mnemonic, "srt"))
                instruction = OPCODE_SRT;
            else
            if(RR_Equals(mnemonic, "sub"))
                instruction = OPCODE_SUB;
            else
            if(RR_Equals(mnemonic, "typ"))
                instruction = OPCODE_TYP;
            else
            if(RR_Equals(mnemonic, "vrt"))
                instruction = OPCODE_VRT;
            else
            if(RR_Equals(mnemonic, "wrt"))
                instruction = OPCODE_WRT;
            else
                Quit("asm: unknown mnemonic `%s`", mnemonic);
            self->instructions[pc] = instruction;
            pc += 1;
            RR_String_Kill(line);
        }
    }
    RR_Map_Kill(labels);
    return self;
}

static void
VM_Cal(VM* self, int address)
{
    int sp = RR_Queue_Size(self->stack) - self->spds;
    RR_Queue_PshB(self->frame, Frame_Init(self->pc, sp));
    self->pc = address;
    self->spds = 0;
}

static void
VM_Cpy(VM* self)
{
    RR_Value* back = RR_Queue_Back(self->stack);
    RR_Value* value = RR_Value_Copy(back);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, value);
}

static int
VM_End(VM* self)
{
    VMTypeExpect(self, RR_Value_ToType(self->ret), TYPE_NUMBER);
    int ret = *RR_Value_ToNumber(self->ret);
    RR_Value_Kill(self->ret);
    return ret;
}

static void
VM_Fls(VM* self)
{
    Frame* frame = RR_Queue_Back(self->frame);
    int pops = RR_Queue_Size(self->stack) - frame->sp;
    while(pops > 0)
    {
        VM_Pop(self);
        pops -= 1;
    }
    self->pc = frame->pc;
    RR_Queue_PopB(self->frame);
}

static void
VM_Glb(VM* self, int address)
{
    RR_Value* value = RR_Queue_Get(self->stack, address);
    RR_Value_Inc(value);
    RR_Queue_PshB(self->stack, value);
}

static void
VM_Loc(VM* self, int address)
{
    Frame* frame = RR_Queue_Back(self->frame);
    RR_Value* value = RR_Queue_Get(self->stack, address + frame->sp);
    RR_Value_Inc(value);
    RR_Queue_PshB(self->stack, value);
}

static void
VM_Jmp(VM* self, int address)
{
    self->pc = address;
}

static void
VM_Ret(VM* self)
{
    Frame* frame = RR_Queue_Back(self->frame);
    self->pc = frame->pc;
    RR_Queue_PopB(self->frame);
}

static void
VM_Lod(VM* self)
{
    RR_Queue_PshB(self->stack, self->ret);
}

static void
VM_Sav(VM* self)
{
    RR_Value* value = RR_Queue_Back(self->stack);
    RR_Value_Inc(value);
    self->ret = value;
}

static void
VM_Psh(VM* self, int address)
{
    RR_Value* value = RR_Queue_Get(self->data, address);
    RR_Value* copy = RR_Value_Copy(value); // Copy, because constant.
    RR_Queue_PshB(self->stack, copy);
}

static void
VM_Mov(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    if(RR_Value_Subbing(a, b))
        RR_Value_Sub(a, b);
    else
    {
        RR_Type type = RR_Value_ToType(a);
        if(type == TYPE_NULL)
            Quit("vm: cannot move `%s` to type `null`", RR_Type_ToString(RR_Value_ToType(b)));
        RR_Type_Kill(RR_Value_ToType(a), RR_Value_Of(a));
        RR_Type_Copy(a, b);
    }
    VM_Pop(self);
}

static void
VM_Add(VM* self)
{
    char* operator = "+";
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    if(RR_Value_Pushing(a, b))
    {
        RR_Value_Inc(b);
        RR_Queue_PshB(RR_Value_ToQueue(a), b);
    }
    else
    if(RR_Value_ToType(a) == RR_Value_ToType(b))
        switch(RR_Value_ToType(a))
        {
        case TYPE_QUEUE:
            RR_Queue_Append(RR_Value_ToQueue(a), RR_Value_ToQueue(b));
            break;
        case TYPE_MAP:
            RR_Map_Append(RR_Value_ToMap(a), RR_Value_ToMap(b));
            break;
        case TYPE_STRING:
            RR_String_Appends(RR_Value_ToString(a), RR_String_Value(RR_Value_ToString(b)));
            break;
        case TYPE_NUMBER:
            *RR_Value_ToNumber(a) += *RR_Value_ToNumber(b);
            break;
        case TYPE_FUNCTION:
        case TYPE_CHAR:
        case TYPE_BOOL:
        case TYPE_NULL:
        case TYPE_FILE:
            VM_BadOperator(self, RR_Value_ToType(a), operator);
        }
    else
        VMTypeMatch(self, RR_Value_ToType(a), RR_Value_ToType(b), operator);
    VM_Pop(self);
}

static void
VM_Sub(VM* self)
{
    char* operator = "-";
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    if(RR_Value_Pushing(a, b))
    {
        RR_Value_Inc(b);
        RR_Queue_PshF(RR_Value_ToQueue(a), b);
    }
    else
    if(RR_Value_ToType(a) == RR_Value_ToType(b))
        switch(RR_Value_ToType(a))
        {
        case TYPE_QUEUE:
            RR_Queue_Prepend(RR_Value_ToQueue(a), RR_Value_ToQueue(b));
            break;
        case TYPE_NUMBER:
            *RR_Value_ToNumber(a) -= *RR_Value_ToNumber(b);
            break;
        case TYPE_FUNCTION:
        case TYPE_MAP:
        case TYPE_STRING:
        case TYPE_CHAR:
        case TYPE_BOOL:
        case TYPE_NULL:
        case TYPE_FILE:
            VM_BadOperator(self, RR_Value_ToType(a), operator);
        }
    else
        VMTypeMatch(self, RR_Value_ToType(a), RR_Value_ToType(b), operator);
    VM_Pop(self);
}

static void
VM_Mul(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeMatch(self, RR_Value_ToType(a), RR_Value_ToType(b), "*");
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_NUMBER);
    *RR_Value_ToNumber(a) *= *RR_Value_ToNumber(b);
    VM_Pop(self);
}

static void
VM_Div(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeMatch(self, RR_Value_ToType(a), RR_Value_ToType(b), "/");
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_NUMBER);
    *RR_Value_ToNumber(a) /= *RR_Value_ToNumber(b);
    VM_Pop(self);
}

static void
VM_Dll(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 3);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* c = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_STRING);
    VMTypeExpect(self, RR_Value_ToType(b), TYPE_STRING);
    VMTypeExpect(self, RR_Value_ToType(c), TYPE_NUMBER);
    typedef RR_Value* (*Call)();
    Call call;
    void* so = dlopen(RR_String_Value(RR_Value_ToString(a)), RTLD_NOW | RTLD_GLOBAL);
    if(so == NULL)
        Quit("vm: could not open shared object library `%s`\n", RR_String_Value(RR_Value_ToString(a)));
    *(void**)(&call) = dlsym(so, RR_String_Value(RR_Value_ToString(b)));
    if(call == NULL)
        Quit("vm: could not open shared object function `%s` from shared object library `%s`\n", RR_String_Value(RR_Value_ToString(b)), RR_String_Value(RR_Value_ToString(a)));
    int params = 3;
    int start = RR_Queue_Size(self->stack) - params;
    int args = *RR_Value_ToNumber(c);
    RR_Value* value = NULL;
    switch(args)
    {
    case 0:
        value = call(); // This is totally bonkers. Is there a way to pass an array through dlsym?
        break;
    case 1:
        value = call(
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 2:
        value = call(
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 3:
        value = call(
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 4:
        value = call(
            RR_Queue_Get(self->stack, start - 4),
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 5:
        value = call(
            RR_Queue_Get(self->stack, start - 5),
            RR_Queue_Get(self->stack, start - 4),
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 6:
        value = call(
            RR_Queue_Get(self->stack, start - 6),
            RR_Queue_Get(self->stack, start - 5),
            RR_Queue_Get(self->stack, start - 4),
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 7:
        value = call(
            RR_Queue_Get(self->stack, start - 7),
            RR_Queue_Get(self->stack, start - 6),
            RR_Queue_Get(self->stack, start - 5),
            RR_Queue_Get(self->stack, start - 4),
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 8:
        value = call(
            RR_Queue_Get(self->stack, start - 8),
            RR_Queue_Get(self->stack, start - 7),
            RR_Queue_Get(self->stack, start - 6),
            RR_Queue_Get(self->stack, start - 5),
            RR_Queue_Get(self->stack, start - 4),
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    case 9:
        value = call(
            RR_Queue_Get(self->stack, start - 9),
            RR_Queue_Get(self->stack, start - 8),
            RR_Queue_Get(self->stack, start - 7),
            RR_Queue_Get(self->stack, start - 6),
            RR_Queue_Get(self->stack, start - 5),
            RR_Queue_Get(self->stack, start - 4),
            RR_Queue_Get(self->stack, start - 3),
            RR_Queue_Get(self->stack, start - 2),
            RR_Queue_Get(self->stack, start - 1));
        break;
    default:
        Quit("only 9 arguments max are supported for native functions calls");
    }
    for(int i = 0; i < args + params; i++)
        VM_Pop(self);
    RR_Value_Print(value);
    RR_Queue_PshB(self->stack, value);
}

static void
VM_Vrt(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_FUNCTION);
    VMTypeExpect(self, RR_Value_ToType(b), TYPE_NUMBER);
    VM_ArgMatch(self, RR_Function_Size(RR_Value_ToFunction(a)), *RR_Value_ToNumber(b));
    int spds = *RR_Value_ToNumber(b);
    int pc = RR_Function_Address(RR_Value_ToFunction(a));
    VM_Pop(self);
    VM_Pop(self);
    int sp = RR_Queue_Size(self->stack) - spds;
    RR_Queue_PshB(self->frame, Frame_Init(self->pc, sp));
    self->pc = pc;
}

static void
VM_RangedSort(VM* self, RR_Queue* queue, RR_Value* compare, int left, int right)
{
    if(left >= right)
        return;
    RR_Queue_Swap(queue, left, (left + right) / 2);
    int last = left;
    for(int i = left + 1; i <= right; i++)
    {
        RR_Value* a = RR_Queue_Get(queue, i);
        RR_Value* b = RR_Queue_Get(queue, left);
        RR_Value_Inc(a);
        RR_Value_Inc(b);
        RR_Value_Inc(compare);
        RR_Queue_PshB(self->stack, a);
        RR_Queue_PshB(self->stack, b);
        RR_Queue_PshB(self->stack, compare);
        RR_Queue_PshB(self->stack, RR_Value_NewNumber(2));
        VM_Vrt(self);
        VM_Run(self, true);
        VMTypeExpect(self, RR_Value_ToType(self->ret), TYPE_BOOL);
        if(*RR_Value_ToBool(self->ret))
             RR_Queue_Swap(queue, ++last, i);
        RR_Value_Kill(self->ret);
    }
   RR_Queue_Swap(queue, left, last);
   VM_RangedSort(self, queue, compare, left, last - 1);
   VM_RangedSort(self, queue, compare, last + 1, right);
}

static void
VM_Sort(VM* self, RR_Queue* queue, RR_Value* compare)
{
    VMTypeExpect(self, RR_Value_ToType(compare), TYPE_FUNCTION);
    VM_ArgMatch(self, RR_Function_Size(RR_Value_ToFunction(compare)), 2);
    VM_RangedSort(self, queue, compare, 0, RR_Queue_Size(queue) - 1);
}

static void
VM_Srt(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_QUEUE);
    VMTypeExpect(self, RR_Value_ToType(b), TYPE_FUNCTION);
    VM_Sort(self, RR_Value_ToQueue(a), b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNull());
}

static void
VM_Lst(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeMatch(self, RR_Value_ToType(a), RR_Value_ToType(b), "<");
    bool boolean = RR_Value_LessThan(a, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Lte(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeMatch(self, RR_Value_ToType(a), RR_Value_ToType(b), "<=");
    bool boolean = RR_Value_LessThanEqualTo(a, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Grt(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeMatch(self, RR_Value_ToType(a), RR_Value_ToType(b), ">");
    bool boolean = RR_Value_GreaterThan(a, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Gte(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeMatch(self, RR_Value_ToType(a), RR_Value_ToType(b), ">=");
    bool boolean = RR_Value_GreaterThanEqualTo(a, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_And(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeMatch(self, RR_Value_ToType(a), RR_Value_ToType(b), "&&");
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_BOOL);
    *RR_Value_ToBool(a) = *RR_Value_ToBool(a) && *RR_Value_ToBool(b);
    VM_Pop(self);
}

static void
VM_Lor(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeMatch(self, RR_Value_ToType(a), RR_Value_ToType(b), "||");
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_BOOL);
    *RR_Value_ToBool(a) = *RR_Value_ToBool(a) || *RR_Value_ToBool(b);
    VM_Pop(self);
}

static void
VM_Eql(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    bool boolean = RR_Value_Equal(a, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Neq(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    bool boolean = !RR_Value_Equal(a, b);
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Spd(VM* self)
{
    self->spds += 1;
}

static void
VM_Not(VM* self)
{
    RR_Value* value = RR_Queue_Back(self->stack);
    VMTypeExpect(self, RR_Value_ToType(value), TYPE_BOOL);
    *RR_Value_ToBool(value) = !*RR_Value_ToBool(value);
}

static void
VM_Brf(VM* self, int address)
{
    RR_Value* value = RR_Queue_Back(self->stack);
    VMTypeExpect(self, RR_Value_ToType(value), TYPE_BOOL);
    if(*RR_Value_ToBool(value) == false)
        self->pc = address;
    VM_Pop(self);
}

static void
VM_Prt(VM* self)
{
    RR_Value* value = RR_Queue_Back(self->stack);
    RR_String* print = RR_Value_Sprint(value, false, 0);
    puts(RR_String_Value(print));
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNumber(RR_String_Size(print)));
    RR_String_Kill(print);
}

static void
VM_Len(VM* self)
{
    RR_Value* value = RR_Queue_Back(self->stack);
    int len = RR_Value_Len(value);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNumber(len));
}

static void
VM_Psb(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_QUEUE);
    RR_Queue_PshB(RR_Value_ToQueue(a), b);
    RR_Value_Inc(b);
    VM_Pop(self);
}

static void
VM_Psf(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_QUEUE);
    RR_Queue_PshF(RR_Value_ToQueue(a), b);
    RR_Value_Inc(b);
    VM_Pop(self);
}

static void
VM_Ins(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 3);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* c = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_MAP);
    if(RR_Value_ToType(b)== TYPE_CHAR)
        VMTypeBadIndex(self, RR_Value_ToType(a), RR_Value_ToType(b));
    else
    if(RR_Value_ToType(b) == TYPE_STRING)
    {
        RR_Map_Set(RR_Value_ToMap(a), RR_String_Copy(RR_Value_ToString(b)), c); // Keys are not reference counted.
        RR_Value_Inc(c);
    }
    else
        VMTypeBad(self, RR_Value_ToType(b));
    VM_Pop(self);
    VM_Pop(self);
}

static void
VM_Ref(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    int refs = RR_Value_Refs(a);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNumber(refs));
}

static RR_Value*
VM_IndexRR_Queue(VM* self, RR_Value* queue, RR_Value* index)
{
    RR_Value* value = RR_Queue_Get(RR_Value_ToQueue(queue), *RR_Value_ToNumber(index));
    if(value == NULL)
        VM_OutOfBounds(self, RR_Value_ToType(queue), *RR_Value_ToNumber(index));
    RR_Value_Inc(value);
    return value;
}

static RR_Value*
VM_IndexRR_String(VM* self, RR_Value* queue, RR_Value* index)
{
    RR_Char* character = RR_Char_Init(queue, *RR_Value_ToNumber(index));
    if(character == NULL)
        VM_OutOfBounds(self, RR_Value_ToType(queue), *RR_Value_ToNumber(index));
    RR_Value* value = RR_Value_NewChar(character);
    RR_Value_Inc(queue);
    return value;
}

static RR_Value*
VM_Index(VM* self, RR_Value* storage, RR_Value* index)
{
    if(RR_Value_ToType(storage) == TYPE_QUEUE)
        return VM_IndexRR_Queue(self, storage, index);
    else
    if(RR_Value_ToType(storage) == TYPE_STRING)
        return VM_IndexRR_String(self, storage, index);
    VMTypeBadIndex(self, RR_Value_ToType(storage), RR_Value_ToType(index));
    return NULL;
}

static RR_Value*
VM_Lookup(VM* self, RR_Value* map, RR_Value* index)
{
    VMTypeExpect(self, RR_Value_ToType(map), TYPE_MAP);
    RR_Value* value = RR_Map_Get(RR_Value_ToMap(map), RR_String_Value(RR_Value_ToString(index)));
    if(value != NULL)
        RR_Value_Inc(value);
    return value;
}

static void
VM_Get(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    RR_Value* value = NULL;
    if(RR_Value_ToType(b) == TYPE_NUMBER)
        value = VM_Index(self, a, b);
    else
    if(RR_Value_ToType(b) == TYPE_STRING)
        value = VM_Lookup(self, a, b);
    else
        VMTypeBadIndex(self, RR_Value_ToType(a), RR_Value_ToType(b));
    VM_Pop(self);
    VM_Pop(self);
    if(value == NULL)
        RR_Queue_PshB(self->stack, RR_Value_NewNull());
    else
        RR_Queue_PshB(self->stack, value);
}

static void
VM_Fmt(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_STRING);
    VMTypeExpect(self, RR_Value_ToType(b), TYPE_QUEUE);
    RR_String* formatted = RR_String_Init("");
    int index = 0;
    char* ref = RR_String_Value(RR_Value_ToString(a));
    for(char* c = ref; *c; c++)
    {
        if(*c == '{')
        {
            char next = *(c + 1);
            if(next == '}')
            {
                RR_Value* value = RR_Queue_Get(RR_Value_ToQueue(b), index);
                if(value == NULL)
                    VM_OutOfBounds(self, RR_Value_ToType(b), index);
                RR_String_Append(formatted, RR_Value_Sprint(value, false, 0));
                index += 1;
                c += 1;
                continue;
            }
        }
        RR_String_PshB(formatted, *c);
    }
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewString(formatted));
}

static void
VM_Typ(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    RR_Type type = RR_Value_ToType(a);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewString(RR_String_Init(RR_Type_ToString(type))));
}

static void
VM_Del(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    if(RR_Value_ToType(b) == TYPE_NUMBER)
    {
        if(RR_Value_ToType(a) == TYPE_QUEUE)
        {
            bool success = RR_Queue_Del(RR_Value_ToQueue(a), *RR_Value_ToNumber(b));
            if(success == false)
                VM_OutOfBounds(self, RR_Value_ToType(a), *RR_Value_ToNumber(b));
        }
        else
        if(RR_Value_ToType(a) == TYPE_STRING)
        {
            bool success = RR_String_Del(RR_Value_ToString(a), *RR_Value_ToNumber(b));
            if(success == false)
                VM_OutOfBounds(self, RR_Value_ToType(a), *RR_Value_ToNumber(b));
        }
        else
            VMTypeBadIndex(self, RR_Value_ToType(a), RR_Value_ToType(b));
    }
    else
    if(RR_Value_ToType(b) == TYPE_STRING)
    {
        if(RR_Value_ToType(a) != TYPE_MAP)
            VMTypeBadIndex(self, RR_Value_ToType(a), RR_Value_ToType(b));
        RR_Map_Del(RR_Value_ToMap(a), RR_String_Value(RR_Value_ToString(b)));
    }
    else
        VMTypeBad(self, RR_Value_ToType(a));
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNull());
}

static void
VM_Mem(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    bool boolean = a == b;
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Opn(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_STRING);
    VMTypeExpect(self, RR_Value_ToType(b), TYPE_STRING);
    RR_File* file = RR_File_Init(RR_String_Copy(RR_Value_ToString(a)), RR_String_Copy(RR_Value_ToString(b)));
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewFile(file));
}

static void
VM_Red(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_FILE);
    VMTypeExpect(self, RR_Value_ToType(b), TYPE_NUMBER);
    RR_String* buffer = RR_String_Init("");
    if(RR_File_File(RR_Value_ToFile(a)))
    {
        RR_String_Resize(buffer, *RR_Value_ToNumber(b));
        int size = fread(RR_String_Value(buffer), sizeof(char), *RR_Value_ToNumber(b), RR_File_File(RR_Value_ToFile(a)));
        int diff = *RR_Value_ToNumber(b) - size;
        while(diff)
        {
            RR_String_PopB(buffer);
            diff -= 1;
        }
    }
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewString(buffer));
}

static void
VM_Wrt(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 2);
    RR_Value* b = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_FILE);
    VMTypeExpect(self, RR_Value_ToType(b), TYPE_STRING);
    size_t bytes = 0;
    if(RR_File_File(RR_Value_ToFile(a)))
        bytes = fwrite(RR_String_Value(RR_Value_ToString(b)), sizeof(char), RR_String_Size(RR_Value_ToString(b)), RR_File_File(RR_Value_ToFile(a)));
    VM_Pop(self);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNumber(bytes));
}

static void
VM_God(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_FILE);
    bool boolean = RR_File_File(RR_Value_ToFile(a)) != NULL;
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewBool(boolean));
}

static void
VM_Key(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_MAP);
    RR_Value* queue = RR_Map_Key(RR_Value_ToMap(a));
    VM_Pop(self);
    RR_Queue_PshB(self->stack, queue);
}

static void
VM_Ext(VM* self)
{
    RR_Value* a = RR_Queue_Get(self->stack, RR_Queue_Size(self->stack) - 1);
    VMTypeExpect(self, RR_Value_ToType(a), TYPE_NUMBER);
    double* number = RR_Value_ToNumber(a);
    exit(*number);
    VM_Pop(self);
    RR_Queue_PshB(self->stack, RR_Value_NewNull());
}

static int
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
        case OPCODE_EXT: VM_Ext(self); break;
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

static Args
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
            if(RR_Equals(arg, "-d")) self.dump = true;
            if(RR_Equals(arg, "-h")) self.help = true;
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
    Args args = Args_Parse(argc, argv);
    if(args.entry)
    {
        int ret = 0;
        RR_String* entry = RR_String_Init(args.entry);
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
        RR_String_Kill(entry);
        return ret;
    }
    else
    if(args.help)
        Args_Help();
    else
        Quit("usage: rr -h");
}
