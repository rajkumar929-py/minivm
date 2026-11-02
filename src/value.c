/*
 * value.c - Printing values.
 */
#include "value.h"

void value_print(FILE *out, Value v)
{
    switch (v.type) {
    case VAL_NIL: fprintf(out, "nil"); break;
    case VAL_INT: fprintf(out, "%d", AS_INT(v)); break;
    case VAL_OBJ: fprintf(out, "<object>"); break;   /* Part 8 prints pairs */
    }
}
