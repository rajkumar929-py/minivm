/*
 * symtab.c - Scoped symbol table implementation.
 */
#include "symtab.h"
#include <stdlib.h>
#include <string.h>

/* djb2 string hash: turns a name into a bucket number */
static unsigned hash(const char *s)
{
    unsigned h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h % SCOPE_BUCKETS;
}

void symtab_init(SymTable *st)
{
    memset(st, 0, sizeof *st);
    symtab_push_scope(st);          /* the global scope, depth 0 */
}

void symtab_push_scope(SymTable *st)
{
    Scope *s = calloc(1, sizeof *s);
    if (!s) abort();
    s->parent = st->current;
    s->depth = st->current ? st->current->depth + 1 : 0;
    st->current = s;
}

void symtab_pop_scope(SymTable *st)
{
    Scope *s = st->current;
    if (!s) return;
    for (int i = 0; i < SCOPE_BUCKETS; i++) {
        Symbol *sym = s->buckets[i];
        while (sym) {
            Symbol *next = sym->next;
            free(sym->name);
            free(sym);
            sym = next;
        }
    }
    st->current = s->parent;
    free(s);
}

void symtab_free(SymTable *st)
{
    while (st->current) symtab_pop_scope(st);
    for (int i = 0; i < st->nfuncs; i++) free(st->funcs[i].name);
    free(st->funcs);
    memset(st, 0, sizeof *st);
}

Symbol *symtab_lookup_current(SymTable *st, const char *name)
{
    for (Symbol *s = st->current->buckets[hash(name)]; s; s = s->next)
        if (strcmp(s->name, name) == 0) return s;
    return NULL;
}

Symbol *symtab_declare(SymTable *st, const char *name, int line)
{
    if (symtab_lookup_current(st, name)) return NULL;   /* redeclaration */

    Symbol *s = calloc(1, sizeof *s);
    if (!s) abort();
    s->name = strdup(name);
    s->line = line;

    /* This is the slot allocation. */
    if (st->in_function) {
        s->kind = SYM_LOCAL;
        s->slot = st->next_local++;
    } else {
        /* Every declaration outside a function gets its own global slot, even
           inside a block. Scopes only limit where the NAME can be seen. */
        s->kind = SYM_GLOBAL;
        s->slot = st->nglobals++;
    }

    /* push onto the front of the bucket's linked list */
    unsigned h = hash(name);
    s->next = st->current->buckets[h];
    st->current->buckets[h] = s;
    return s;
}

Symbol *symtab_lookup(SymTable *st, const char *name)
{
    for (Scope *sc = st->current; sc; sc = sc->parent)
        for (Symbol *s = sc->buckets[hash(name)]; s; s = s->next)
            if (strcmp(s->name, name) == 0) return s;
    return NULL;
}

int symtab_find_func(SymTable *st, const char *name)
{
    for (int i = 0; i < st->nfuncs; i++)
        if (strcmp(st->funcs[i].name, name) == 0) return i;
    return -1;
}

int symtab_add_func(SymTable *st, const char *name, int arity, int line)
{
    if (symtab_find_func(st, name) >= 0) return -1;
    if (st->nfuncs == st->cap_funcs) {
        st->cap_funcs = st->cap_funcs ? st->cap_funcs * 2 : 8;
        st->funcs = realloc(st->funcs, st->cap_funcs * sizeof(FuncSym));
        if (!st->funcs) abort();
    }
    st->funcs[st->nfuncs].name = strdup(name);
    st->funcs[st->nfuncs].arity = arity;
    st->funcs[st->nfuncs].line = line;
    return st->nfuncs++;
}
