// gcc Math.c -o Math.so --shared -fpic

void Math_Add(double* self, double* other) { *self += *other; }
void Math_Sub(double* self, double* other) { *self -= *other; }
void Math_Div(double* self, double* other) { *self /= *other; }
void Math_Mul(double* self, double* other) { *self *= *other; }
