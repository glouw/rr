#include <roman2.h>
#include <stdio.h>

void Other_Sub(RR_Value* a, RR_Value* b)
{
    puts("HELLO AGAIN FROM C!\n");
    RR_Value_Print(a);
    RR_Value_Print(b);
    double* da = RR_Value_ToNumber(a);
    double* db = RR_Value_ToNumber(b);
    *da *= 20;
    *db *= 20;
    printf("PRINTF %f %f\n", *da, *db);
}

void Other_Test(void)
{
    puts("HELLO TEST FROM C!\n");
}

void Other_Test2(void)
{
    puts("HELLO TEST2 FROM C!\n");
}
