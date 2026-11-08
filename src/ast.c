/*
 * ast.c - Build, free and print AST nodes.
 *
 * What a, b, c, d and list mean for each kind:
 *   N_VAR_DECL     name, a = initial value (may be NULL)
 *   N_ASSIGN       name, a = value
 *   N_FIELD_ASSIGN a = the pair, b = value, field = 0 left / 1 right
 *   N_IF           a = condition, b = then, c = else (may be NULL)
 *   N_WHILE        a = condition, b = body
 *   N_FOR          a = init, b = condition, c = update (each may be NULL), d = body
 *   N_PRINT        a = value
 *   N_EXPR_STMT    a = the call
 *   N_FUNC_DECL    name, params/nparams, a = body block
 *   N_RETURN       a = value (may be NULL)
 *   N_BINARY       op, a = left side, b = right side
 *   N_UNARY        op, a = operand
 *   N_CALL         name, list = arguments
 *   N_PAIR         a = left, b = right
 *   N_FIELD        a = the pair, field = 0 left / 1 right
 *   N_BLOCK, N_PROGRAM   list = statements
 */
#include "ast.h"
#include <stdlib.h>

static void *xcalloc(size_t n, size_t sz)
{
    void *p = calloc(n, sz);
    if (!p) { perror("calloc"); exit(1); }
    return p;
}

Node *ast_new(NodeKind kind, int line)
{
    Node *n = xcalloc(1, sizeof *n);    /* every field starts as 0 / NULL */
    n->kind = kind;
    n->line = line;
    return n;
}

Node *ast_int(int value, int line)
{
    Node *n = ast_new(N_INT, line);
    n->ival = value;
    return n;
}

Node *ast_ident(char *name, int line)
{
    Node *n = ast_new(N_IDENT, line);
    n->name = name;
    return n;
}

Node *ast_binary(AstOp op, Node *l, Node *r, int line)
{
    Node *n = ast_new(N_BINARY, line);
    n->op = op;
    n->a = l;
    n->b = r;
    return n;
}

Node *ast_unary(AstOp op, Node *e, int line)
{
    Node *n = ast_new(N_UNARY, line);
    n->op = op;
    n->a = e;
    return n;
}

void ast_list_push(NodeList *list, Node *n)
{
    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 4;
        list->items = realloc(list->items, list->cap * sizeof(Node *));
        if (!list->items) { perror("realloc"); exit(1); }
    }
    list->items[list->count++] = n;
}

void ast_free(Node *n)
{
    if (!n) return;
    ast_free(n->a);
    ast_free(n->b);
    ast_free(n->c);
    ast_free(n->d);
    for (int i = 0; i < n->list.count; i++)
        ast_free(n->list.items[i]);
    free(n->list.items);
    for (int i = 0; i < n->nparams; i++)
        free(n->params[i]);
    free(n->params);
    free(n->name);
    free(n);
}
const char *ast_op_str(AstOp op)
{
    switch (op) {
    case OP_ADD_: return "+";
    case OP_SUB_: return "-";
    case OP_MUL_: return "*";
    case OP_DIV_: return "/";
    case OP_MOD_: return "%";
    case OP_LT_:  return "<";
    case OP_GT_:  return ">";
    case OP_LE_:  return "<=";
    case OP_GE_:  return ">=";
    case OP_EQ_:  return "==";
    case OP_NE_:  return "!=";
    case OP_NEG_: return "-";
    case OP_POS_: return "+";
    }
    return "?";
}

static void pad(FILE *out, int indent)
{
    for (int i = 0; i < indent; i++) fputs("  ", out);
}

static const char *field_name(int f) { return f ? "right" : "left"; }

void ast_print(FILE *out, const Node *n, int indent)
{
    if (!n) { pad(out, indent); fputs("(null)\n", out); return; }
    pad(out, indent);
    switch (n->kind) {
    case N_PROGRAM:
        fprintf(out, "Program\n");
        for (int i = 0; i < n->list.count; i++) ast_print(out, n->list.items[i], indent + 1);
        break;
    case N_BLOCK:
        fprintf(out, "Block (line %d)\n", n->line);
        for (int i = 0; i < n->list.count; i++) ast_print(out, n->list.items[i], indent + 1);
        break;
    case N_VAR_DECL:
        fprintf(out, "VarDecl %s (line %d)\n", n->name, n->line);
        if (n->a) ast_print(out, n->a, indent + 1);
        break;
    case N_ASSIGN:
        fprintf(out, "Assign %s (line %d)\n", n->name, n->line);
        ast_print(out, n->a, indent + 1);
        break;
    case N_FIELD_ASSIGN:
        fprintf(out, "FieldAssign .%s (line %d)\n", field_name(n->field), n->line);
        ast_print(out, n->a, indent + 1);
        ast_print(out, n->b, indent + 1);
        break;
    case N_IF:
        fprintf(out, "If (line %d)\n", n->line);
        ast_print(out, n->a, indent + 1);
        pad(out, indent); fputs("Then\n", out);
        ast_print(out, n->b, indent + 1);
        if (n->c) { pad(out, indent); fputs("Else\n", out); ast_print(out, n->c, indent + 1); }
        break;
    case N_WHILE:
        fprintf(out, "While (line %d)\n", n->line);
        ast_print(out, n->a, indent + 1);
        ast_print(out, n->b, indent + 1);
        break;
    case N_FOR:
        fprintf(out, "For (line %d)\n", n->line);
        pad(out, indent + 1); fputs("Init\n", out);
        if (n->a) ast_print(out, n->a, indent + 2);
        pad(out, indent + 1); fputs("Cond\n", out);
        if (n->b) ast_print(out, n->b, indent + 2);
        pad(out, indent + 1); fputs("Update\n", out);
        if (n->c) ast_print(out, n->c, indent + 2);
        ast_print(out, n->d, indent + 1);
        break;
    case N_BREAK:    fprintf(out, "Break (line %d)\n", n->line); break;
    case N_CONTINUE: fprintf(out, "Continue (line %d)\n", n->line); break;
    case N_PRINT:
        fprintf(out, "Print (line %d)\n", n->line);
        ast_print(out, n->a, indent + 1);
        break;
    case N_EXPR_STMT:
        fprintf(out, "ExprStmt (line %d)\n", n->line);
        ast_print(out, n->a, indent + 1);
        break;
    case N_FUNC_DECL:
        fprintf(out, "Func %s(", n->name);
        for (int i = 0; i < n->nparams; i++) fprintf(out, "%s%s", i ? ", " : "", n->params[i]);
        fprintf(out, ") (line %d)\n", n->line);
        ast_print(out, n->a, indent + 1);
        break;
    case N_RETURN:
        fprintf(out, "Return (line %d)\n", n->line);
        if (n->a) ast_print(out, n->a, indent + 1);
        break;
    case N_INT:   fprintf(out, "Int %d\n", n->ival); break;
    case N_NIL:   fprintf(out, "Nil\n"); break;
    case N_IDENT: fprintf(out, "Ident %s\n", n->name); break;
    case N_BINARY:
        fprintf(out, "Binary %s\n", ast_op_str(n->op));
        ast_print(out, n->a, indent + 1);
        ast_print(out, n->b, indent + 1);
        break;
    case N_UNARY:
        fprintf(out, "Unary %s\n", ast_op_str(n->op));
        ast_print(out, n->a, indent + 1);
        break;
    case N_CALL:
        fprintf(out, "Call %s\n", n->name);
        for (int i = 0; i < n->list.count; i++) ast_print(out, n->list.items[i], indent + 1);
        break;
    case N_PAIR:
        fprintf(out, "Pair (alloc site, line %d)\n", n->line);
        ast_print(out, n->a, indent + 1);
        ast_print(out, n->b, indent + 1);
        break;
    case N_FIELD:
        fprintf(out, "Field .%s\n", field_name(n->field));
        ast_print(out, n->a, indent + 1);
        break;
    }
}
