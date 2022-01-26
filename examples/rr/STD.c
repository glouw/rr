#include <roman2.h>
#include <unistd.h>
#include <string.h>

RR_Value*
STD_GetCWD(void)
{
    char buffer[4096] = { '\0' };
    getcwd(buffer, sizeof(buffer));
    strcat(buffer, "/");
    return RR_Value_NewString(RR_String_Init(buffer));
}

RR_Value*
STD_RealPath(RR_Value* path)
{
    char* real = realpath(RR_String_Value(RR_Value_ToString(path)), NULL);
    if(real == NULL)
        return RR_Value_NewNull();
    else
        return RR_Value_NewString(RR_String_Init(real));
}
