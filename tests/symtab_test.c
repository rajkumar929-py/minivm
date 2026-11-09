/* symtab_test.c - check scopes, shadowing and slot numbers */
#include <assert.h>
#include <stdio.h>
#include "symtab.h"

int main(void)
{
    SymTable st;
    symtab_init(&st);

    /* a global gets slot 0; declaring it twice in one scope fails */
    Symbol *outer = symtab_declare(&st, "x", 1);
    assert(outer && outer->kind == SYM_GLOBAL && outer->slot == 0);
    assert(symtab_declare(&st, "x", 2) == NULL);

    /* a new scope may shadow it, with a different slot */
    symtab_push_scope(&st);
    Symbol *inner = symtab_declare(&st, "x", 3);
    assert(inner && inner->slot == 1);
    assert(symtab_lookup(&st, "x") == inner);

    /* when the scope ends, the outer x is visible again */
    symtab_pop_scope(&st);
    assert(symtab_lookup(&st, "x") == outer);
    assert(symtab_lookup(&st, "nope") == NULL);

    /* inside a function, slots are locals and start from 0 */
    st.in_function = 1;
    symtab_push_scope(&st);
    Symbol *a = symtab_declare(&st, "a", 4);
    Symbol *b = symtab_declare(&st, "b", 4);
    assert(a->kind == SYM_LOCAL && a->slot == 0 && b->slot == 1);
    symtab_pop_scope(&st);
    st.in_function = 0;

    /* functions: a duplicate name is refused */
    assert(symtab_add_func(&st, "f", 2, 5) == 0);
    assert(symtab_add_func(&st, "f", 1, 6) == -1);
    assert(symtab_find_func(&st, "f") == 0);

    symtab_free(&st);
    puts("all symtab checks passed");
    return 0;
}
