/*
 * symtab.h - Scoped symbol table used by the semantic pass.
 *
 * Each Scope is a small hash table that points to its parent scope. Lookup
 * walks outwards from the innermost scope, so an inner declaration shadows an
 * outer one. Functions live in their own flat table, because they can only
 * be declared at the top level.
 */
#ifndef SYMTAB_H
#define SYMTAB_H

#include "ast.h"

#define SCOPE_BUCKETS 32

typedef struct Symbol {
    char *name;
    SymKind kind;           /* SYM_GLOBAL or SYM_LOCAL                  */
    int slot;               /* index into globals[] or the frame        */
    int line;               /* line of the declaration                  */
    struct Symbol *next;    /* next symbol in the same bucket           */
} Symbol;

typedef struct Scope {
    Symbol *buckets[SCOPE_BUCKETS];
    struct Scope *parent;
    int depth;              /* 0 = global scope                         */
} Scope;

typedef struct {
    char *name;
    int arity;              /* number of parameters                     */
    int line;
} FuncSym;

typedef struct {
    Scope *current;         /* the innermost scope                      */
    FuncSym *funcs;
    int nfuncs, cap_funcs;
    int nglobals;           /* global slots handed out so far           */
    int in_function;        /* are we inside a function body?           */
    int next_local;         /* next free local slot in this function    */
} SymTable;

void    symtab_init(SymTable *st);
void    symtab_free(SymTable *st);
void    symtab_push_scope(SymTable *st);
void    symtab_pop_scope(SymTable *st);

/* Declare in the innermost scope. Returns NULL if already declared there. */
Symbol *symtab_declare(SymTable *st, const char *name, int line);
Symbol *symtab_lookup(SymTable *st, const char *name);
Symbol *symtab_lookup_current(SymTable *st, const char *name);

/* Returns the function's index, or -1 if that name is already taken. */
int     symtab_add_func(SymTable *st, const char *name, int arity, int line);
int     symtab_find_func(SymTable *st, const char *name);

#endif
