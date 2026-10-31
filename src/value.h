/*
 * value.h - Runtime values.
 *
 * A Value is a tagged union: a "type" tag plus the data.
 *   VAL_NIL - the nil literal
 *   VAL_INT - 32-bit signed integer (also used for booleans 0/1)
 *   VAL_OBJ - pointer to a heap object (filled in in Part 8)
 */
#ifndef VALUE_H
#define VALUE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Obj Obj;     /* defined in Part 8; a pointer is enough for now */

typedef enum { VAL_NIL, VAL_INT, VAL_OBJ } ValueType;

typedef struct {
    ValueType type;
    union {
        int32_t i;
        Obj *obj;
    } as;
} Value;

#define NIL_VAL         ((Value){ VAL_NIL, { .i = 0 } })
#define INT_VAL(v)      ((Value){ VAL_INT, { .i = (int32_t)(v) } })
#define OBJ_VAL(o)      ((Value){ VAL_OBJ, { .obj = (Obj *)(o) } })

#define IS_NIL(v)       ((v).type == VAL_NIL)
#define IS_INT(v)       ((v).type == VAL_INT)
#define IS_OBJ(v)       ((v).type == VAL_OBJ)
#define AS_INT(v)       ((v).as.i)
#define AS_OBJ(v)       ((v).as.obj)

static inline bool value_truthy(Value v)
{
    if (IS_NIL(v)) return false;
    if (IS_INT(v)) return AS_INT(v) != 0;
    return true;            /* any object reference is true */
}

static inline bool values_equal(Value a, Value b)
{
    if (a.type != b.type) return false;
    switch (a.type) {
    case VAL_NIL: return true;
    case VAL_INT: return AS_INT(a) == AS_INT(b);
    case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);   /* same object? */
    }
    return false;
}

#endif
