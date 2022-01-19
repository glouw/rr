#include <roman2.h>
#include <stdio.h>

void add(Value* a, Value* b)
{
    puts("HELLO FROM C!\n");
    Value_Println(a);
    Value_Println(b);
    double* da = Value_Number(a);
    double* db = Value_Number(b);
    *da *= 20;
    *db *= 20;
    printf("PRINTF %f %f\n", *da, *db);
}
