/*
 * codegen.c - Walk the annotated AST and emit bytecode into a Chunk.
 *
 * Layout of the generated code:
 *     [ top-level statements ] HALT       (functions go after HALT in Part 7)
 *
 * Two rules keep the stack balanced:
 *   - gen_expr leaves EXACTLY ONE new value on the stack
 *   - gen_stmt leaves the stack exactly as it found it
 *
 * Templates (L = label = an address):
 *   if (c) S1 else S2         while (c) S             for (i; c; u) S
 *       <c>                   top: <c>                    <i>
 *       JZ   Lelse                 JZ  Lexit         top: <c>
 *       <S1>                       <S>                    JZ  Lexit
 *       JMP  Lend                  JMP top                <S>
 *   Lelse: <S2>               Lexit:                next: <u>
 *   Lend:                                                 JMP top
 *                                                   Lexit:
 * A jump FORWARD is emitted with target -1 and BACK-PATCHED later.
 */

#include "codegen.h"
#include "frontend.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LOOPS 128      /* the semantic pass allows at most 100 nested loops */

typedef struct {
    int *at;               /* operand offsets of jumps still waiting for a target */
    int count, cap;
} PatchList;

typedef struct {
    PatchList breaks;      /* jumps to the loop's exit         */
    PatchList continues;   /* jumps to the loop's next round   */
} Loop;

typedef struct {
    Chunk *c;
    const char *filename;
    Loop loops[MAX_LOOPS]; /* the innermost loop is loops[nloops - 1] */
    int nloops;
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
/* A forward jump: the target isn't known yet, so emit -1.
   Returns the offset of the OPERAND, which is what gets patched later. */
static int emit_jump(CG *g, OpCode op, int line)
{
    return emit1(g, op, -1, line) + 1;
}

/* Back-patch a forward jump so that it lands HERE. */
static void patch_here(CG *g, int operand_at)
{
    chunk_patch_i32(g->c, operand_at, here(g));
}
/* ---- loops: break and continue ---- */
static void patch_push(PatchList *p, int operand_at)
{
    if (p->count == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 4;
        p->at = realloc(p->at, (size_t)p->cap * sizeof(int));
        if (!p->at) { perror("realloc"); exit(1); }
    }
    p->at[p->count++] = operand_at;
}

static void patch_all(CG *g, PatchList *p, int target)
{
    for (int i = 0; i < p->count; i++) chunk_patch_i32(g->c, p->at[i], target);
    free(p->at);
    memset(p, 0, sizeof *p);
}

static void loop_push(CG *g)
{
    if (g->nloops == MAX_LOOPS) {           /* can't happen: the semantic pass stops at 100 */
        fprintf(stderr, "internal error: loops nested too deeply\n");
        abort();
    }
    memset(&g->loops[g->nloops++], 0, sizeof(Loop));
}

/* The loop is finished: now both of its targets are known. */
static void loop_pop(CG *g, int exit_addr, int next_addr)
{
    Loop *l = &g->loops[--g->nloops];
    patch_all(g, &l->breaks, exit_addr);
    patch_all(g, &l->continues, next_addr);
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
static void gen_stmt(CG *g, Node *n);

static void gen_if(CG *g, Node *n)
{
    int start = here(g);
    gen_expr(g, n->a);
    chunk_mark_stmt(g->c, start);
    int jfalse = emit_jump(g, OP_JZ, n->line);      /* to Lelse (or Lend) */
    gen_stmt(g, n->b);
    if (n->c) {
        int jend = emit_jump(g, OP_JMP, n->line);   /* skip over the else */
        patch_here(g, jfalse);                      /* Lelse is here      */
        gen_stmt(g, n->c);
        patch_here(g, jend);                        /* Lend is here       */
    } else {
        patch_here(g, jfalse);
    }
}

static void gen_while(CG *g, Node *n)
{
    int top = here(g);
    gen_expr(g, n->a);
    chunk_mark_stmt(g->c, top);
    int jexit = emit_jump(g, OP_JZ, n->line);
    loop_push(g);
    gen_stmt(g, n->b);
    emit1(g, OP_JMP, top, n->line);     /* backward jump: top is already known */
    patch_here(g, jexit);
    loop_pop(g, here(g), top);          /* break -> exit, continue -> top */
}

static void gen_for(CG *g, Node *n)
{
    gen_stmt(g, n->a);                  /* init (gen_stmt ignores NULL) */
    int top = here(g);
    int jexit = -1;
    if (n->b) {                         /* no condition = loop forever */
        gen_expr(g, n->b);
        chunk_mark_stmt(g->c, top);
        jexit = emit_jump(g, OP_JZ, n->line);
    }
    loop_push(g);
    gen_stmt(g, n->d);                  /* body */
    int next = here(g);
    gen_stmt(g, n->c);                  /* update */
    emit1(g, OP_JMP, top, n->line);
    if (jexit >= 0) patch_here(g, jexit);
    loop_pop(g, here(g), next);         /* break -> exit, continue -> update */
}


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
    case N_IF:    gen_if(g, n);    return;
    case N_WHILE: gen_while(g, n); return;
    case N_FOR:   gen_for(g, n);   return;
    case N_BREAK:
        patch_push(&g->loops[g->nloops - 1].breaks, emit_jump(g, OP_JMP, n->line));
        break;
    case N_CONTINUE:
        patch_push(&g->loops[g->nloops - 1].continues, emit_jump(g, OP_JMP, n->line));
        break;

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
