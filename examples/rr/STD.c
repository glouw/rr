#include <roman2.h>
#include <unistd.h>
#include <string.h>

RR_Value*
STD_GetCWD(void)
{
    RR_String* path = RR_String_Init("");
    RR_String_Resize(path, 4096);
    getcwd(RR_String_Begin(path), RR_String_Size(path));
    strcat(RR_String_Begin(path), "/");
    return RR_Value_NewString(path);
}
