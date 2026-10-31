#include <stdio.h>
#include "value.h"

int main(void)
{
    Value a = INT_VAL(7), z = INT_VAL(0), n = NIL_VAL;
    printf("sizeof(Value) = %zu bytes\n", sizeof(Value));
    printf("truthy: 7=%d 0=%d nil=%d\n", value_truthy(a), value_truthy(z), value_truthy(n));
    printf("equal:  7==7 %d   0==nil %d\n", values_equal(a, INT_VAL(7)), values_equal(z, n));
    return 0;
}
