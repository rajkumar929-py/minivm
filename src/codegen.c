/*
 * codegen.c - Walk the annotated AST and emit bytecode into a Chunk.
 *
 * Layout of the generated code:
 *     [ top-level statements ] HALT       (functions go after HALT in Part 7)
 *
 * Two rules keep the stack balanced:
 *   - gen_expr leaves EXACTLY ONE new value on the stack
 *   - gen_stmt leaves the stack exactly as it found it
 */
#include "codegen.h"
#include "frontend.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Chunk *c;
    const char *filename;
    int errors;
} CG;

static void cg_error(CG *g, int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d: codegen error: ", g->filename, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    g->errors++;
}

/* ---- emit helpers ---- */
static int here(CG *g) { return g->c->count; }

static int emit(CG *g, OpCode op, int line) { return chunk_emit_op(g->c, op, line); }

static int emit1(CG *g, OpCode op, int32_t operand, int line)
{
    int at = chunk_emit_op(g->c, op, line);
    chunk_emit_i32(g->c, operand, line);
    return at;
}

/* ---- expressions ---- */
static void gen_load(CG *g, SymKind k, int slot, int line)
{
    emit1(g, k == SYM_LOCAL ? OP_LOADL : OP_LOAD, slot, line);
}

static void gen_store(CG *g, SymKind k, int slot, int line)
{
    emit1(g, k == SYM_LOCAL ? OP_STOREL : OP_STORE, slot, line);
}

static OpCode binary_opcode(AstOp op)
{
    switch (op) {
    case OP_ADD_: return OP_ADD;
    case OP_SUB_: return OP_SUB;
    case OP_MUL_: return OP_MUL;
    case OP_DIV_: return OP_DIV;
    case OP_MOD_: return OP_MOD;
    case OP_LT_:  return OP_CMP;        /* CMP means "less than" */
    case OP_GT_:  return OP_GT;
    case OP_LE_:  return OP_LE;
    case OP_GE_:  return OP_GE;
    case OP_EQ_:  return OP_EQ;
    case OP_NE_:  return OP_NE;
    default:      return OP_NOP;        /* not a binary operator */
    }
}

static void gen_expr(CG *g, Node *n)
{
    switch (n->kind) {
    case N_INT:   emit1(g, OP_PUSH, n->ival, n->line); break;
    case N_NIL:   emit(g, OP_PUSH_NIL, n->line); break;
    case N_IDENT: gen_load(g, n->sym_kind, n->slot, n->line); break;
    case N_UNARY:
        gen_expr(g, n->a);
        if (n->op == OP_NEG_) emit(g, OP_NEG, n->line);   /* unary + does nothing */
        break;
    case N_BINARY:
        gen_expr(g, n->a);              /* left first, so it ends up BELOW the right */
        gen_expr(g, n->b);
        emit(g, binary_opcode(n->op), n->line);
        break;
    case N_PAIR:
        gen_expr(g, n->a);
        gen_expr(g, n->b);
        emit(g, OP_NEWPAIR, n->line);
        break;
    case N_FIELD:
        gen_expr(g, n->a);
        emit(g, n->field ? OP_GETR : OP_GETL, n->line);
        break;
    case N_CALL:
        cg_error(g, n->line, "function calls are not supported yet (Part 7)");
        break;
    default:
        cg_error(g, n->line, "internal: unexpected node in expression");
    }
}

/* ---- statements ---- */
static void gen_stmt(CG *g, Node *n)
{
    if (!n) return;
    int start = here(g);
    switch (n->kind) {
    case N_VAR_DECL:
        if (n->a) gen_expr(g, n->a);
        else emit(g, OP_PUSH_NIL, n->line);         /* "var x;" means x = nil */
        gen_store(g, n->sym_kind, n->slot, n->line);
        break;
    case N_ASSIGN:
        gen_expr(g, n->a);
        gen_store(g, n->sym_kind, n->slot, n->line);
        break;
    case N_FIELD_ASSIGN:
        gen_expr(g, n->a);
        gen_expr(g, n->b);
        emit(g, n->field ? OP_SETR : OP_SETL, n->line);
        break;
    case N_PRINT:
        gen_expr(g, n->a);
        emit(g, OP_PRINT, n->line);
        break;
    case N_EXPR_STMT:
        gen_expr(g, n->a);
        emit(g, OP_POP, n->line);                   /* throw the unused value away */
        break;
    case N_BLOCK:
        for (int i = 0; i < n->list.count; i++) gen_stmt(g, n->list.items[i]);
        return;                         /* the statements inside mark themselves */
    case N_FUNC_DECL:
    case N_RETURN:
        cg_error(g, n->line, "functions are not supported yet (Part 7)");
        return;
    default:
        cg_error(g, n->line, "internal: unexpected node in statement");
        return;
    }
    chunk_mark_stmt(g->c, start);       /* first instruction of this statement */
}

int codegen_program(Node *program, const SemaResult *sema, Chunk *chunk,
                    const char *filename)
{
    CG g;
    memset(&g, 0, sizeof g);
    g.c = chunk;
    g.filename = filename ? filename : "<input>";
    chunk->nglobals = sema->nglobals;

    for (int i = 0; i < program->list.count; i++) gen_stmt(&g, program->list.items[i]);

    int n = program->list.count;
    emit(&g, OP_HALT, n ? program->list.items[n - 1]->line : 1);
    return g.errors == 0;
}

int compile_source(const char *src, const char *filename, Chunk *out)
{
    chunk_init(out);
    fe_begin(filename);
    Node *tree = parse_source(src);
    if (!tree) return 0;                            /* syntax errors already printed */

    SemaResult sr = semantic_analyze(tree, filename);
    int ok = sr.errors == 0 && codegen_program(tree, &sr, out, filename);
    ast_free(tree);
    if (!ok) chunk_free(out);
    return ok;
}
