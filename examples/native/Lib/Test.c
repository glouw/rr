//! -lSDL2 -lSDL2_ttf -lSDL2_mixer

#include <roman2.h>
#include <stdio.h>

void Test_Add(RR_Value* a, RR_Value* b)
{
    puts("HELLO FROM C!\n");
    RR_Value_Print(a);
    RR_Value_Print(b);
    double* da = RR_Value_ToNumber(a);
    double* db = RR_Value_ToNumber(b);
    *da *= 20;
    *db *= 20;
    printf("PRINTF %f %f\n", *da, *db);
}
