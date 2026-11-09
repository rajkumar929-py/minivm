/*
 * semantic.c - Name resolution and static checks.
 *
 * Two passes over the top level:
 *   pass 1: register every function signature (this is what makes recursion
 *           and calling a later function work)
 *   pass 2: walk all statements and function bodies in file order,
 *           resolving names through the scoped symbol table.
 */
#include "semantic.h"
#include "symtab.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef struct {
    SymTable st;
    const char *filename;
    int errors;
    int loop_depth;
    int nalloc_sites;
} Sema;

static void sema_error(Sema *s, int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d: semantic error: ", s->filename, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    s->errors++;
}

static void check_stmt(Sema *s, Node *n);

static void check_expr(Sema *s, Node *n)
{
    if (!n) return;
    switch (n->kind) {
    case N_INT:
    case N_NIL:
        break;
    case N_IDENT: {
        Symbol *sym = symtab_lookup(&s->st, n->name);
        if (!sym) {
            sema_error(s, n->line, "use of undeclared variable '%s'", n->name);
            return;
        }
        n->sym_kind = sym->kind;
        n->slot = sym->slot;
        break;
    }
    case N_BINARY:
        check_expr(s, n->a);
        check_expr(s, n->b);
        break;
    case N_UNARY:
    case N_FIELD:
        check_expr(s, n->a);
        break;
    default:
        sema_error(s, n->line, "internal: unexpected node in expression");
    }
}

static void declare_var(Sema *s, Node *n)
{
    /* The initial value is checked BEFORE the name is declared, so in
       "var x = x;" the right-hand x is an outer x, or an error. */
    check_expr(s, n->a);
    Symbol *sym = symtab_declare(&s->st, n->name, n->line);
    if (!sym) {
        Symbol *prev = symtab_lookup_current(&s->st, n->name);
        sema_error(s, n->line, "redeclaration of '%s' (previously declared on line %d)",
                   n->name, prev ? prev->line : 0);
        return;
    }
    n->sym_kind = sym->kind;
    n->slot = sym->slot;
}

static void resolve_assign(Sema *s, Node *n)
{
    check_expr(s, n->a);
    Symbol *sym = symtab_lookup(&s->st, n->name);
    if (!sym) {
        sema_error(s, n->line, "assignment to undeclared variable '%s'", n->name);
        return;
    }
    n->sym_kind = sym->kind;
    n->slot = sym->slot;
}

static void check_block(Sema *s, Node *n)
{
    symtab_push_scope(&s->st);
    for (int i = 0; i < n->list.count; i++) check_stmt(s, n->list.items[i]);
    symtab_pop_scope(&s->st);
}

#define MAX_LOOP_NESTING 100   /* codegen will keep a fixed-size loop stack */

static void check_stmt(Sema *s, Node *n)
{
    if (!n) return;
    if ((n->kind == N_WHILE || n->kind == N_FOR) && s->loop_depth >= MAX_LOOP_NESTING) {
        sema_error(s, n->line, "loops nested deeper than %d levels", MAX_LOOP_NESTING);
        return;
    }
    switch (n->kind) {
    case N_VAR_DECL:     declare_var(s, n); break;
    case N_ASSIGN:       resolve_assign(s, n); break;
    case N_FIELD_ASSIGN: check_expr(s, n->a); check_expr(s, n->b); break;
    case N_PRINT:        check_expr(s, n->a); break;
    case N_BLOCK:        check_block(s, n); break;
    case N_IF:
        check_expr(s, n->a);
        /* a lone statement branch gets its own scope, like a block */
        symtab_push_scope(&s->st); check_stmt(s, n->b); symtab_pop_scope(&s->st);
        if (n->c) { symtab_push_scope(&s->st); check_stmt(s, n->c); symtab_pop_scope(&s->st); }
        break;
    case N_WHILE:
        check_expr(s, n->a);
        s->loop_depth++;
        symtab_push_scope(&s->st); check_stmt(s, n->b); symtab_pop_scope(&s->st);
        s->loop_depth--;
        break;
    case N_FOR:
        symtab_push_scope(&s->st);          /* the scope of the loop variable */
        if (n->a) check_stmt(s, n->a);
        check_expr(s, n->b);
        if (n->c) check_stmt(s, n->c);
        s->loop_depth++;
        symtab_push_scope(&s->st); check_stmt(s, n->d); symtab_pop_scope(&s->st);
        s->loop_depth--;
        symtab_pop_scope(&s->st);
        break;
    case N_BREAK:
    case N_CONTINUE:
        if (s->loop_depth == 0)
            sema_error(s, n->line, "'%s' outside of a loop",
                       n->kind == N_BREAK ? "break" : "continue");
        break;
    default:
        sema_error(s, n->line, "internal: unexpected node in statement");
    }
}

SemaResult semantic_analyze(Node *program, const char *filename)
{
    Sema s;
    memset(&s, 0, sizeof s);
    s.filename = filename ? filename : "<input>";
    symtab_init(&s.st);

    /* pass 2: bodies and top-level statements, in order */
    for (int i = 0; i < program->list.count; i++)
        check_stmt(&s, program->list.items[i]);

    SemaResult r;
    r.nglobals = s.st.nglobals;
    r.nfuncs = s.st.nfuncs;
    r.nalloc_sites = s.nalloc_sites;
    r.errors = s.errors;
    symtab_free(&s.st);
    return r;
}
