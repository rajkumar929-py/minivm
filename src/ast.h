/*
 * ast.h - The Abstract Syntax Tree (AST) for the "lang" language.
 *
 * The parser (parser.y) builds this tree. Later passes read it:
 * semantic.c checks it (Part 5) and codegen.c turns it into bytecode (Part 6).
 */
#ifndef AST_H
#define AST_H

#include <stdio.h>

typedef enum {
    /* statements */
    N_PROGRAM,      /* list = top-level statements and functions   */
    N_BLOCK,        /* { ... }            list = statements         */
    N_VAR_DECL,     /* var name = a;      (a may be NULL)           */
    N_ASSIGN,       /* name = a;                                    */
    N_FIELD_ASSIGN, /* a.left = b;  or  a.right = b;                */
    N_IF,           /* if (a) b else c    (c may be NULL)           */
    N_WHILE,        /* while (a) b                                  */
    N_FOR,          /* for (a; b; c) d    (a, b, c may be NULL)     */
    N_BREAK,
    N_CONTINUE,
    N_PRINT,        /* print a;                                     */
    N_EXPR_STMT,    /* f(x);  a call used as a statement            */
    N_FUNC_DECL,    /* func name(params) a                          */
    N_RETURN,       /* return a;          (a may be NULL)           */
    /* expressions */
    N_INT,          /* 42                 ival                      */
    N_NIL,          /* nil                                          */
    N_IDENT,        /* x                  name                      */
    N_BINARY,       /* a op b                                       */
    N_UNARY,        /* op a                                         */
    N_CALL,         /* name(list...)                                */
    N_PAIR,         /* pair(a, b)                                   */
    N_FIELD         /* a.left  or  a.right                          */
} NodeKind;

/* Operators. The trailing _ avoids a clash with the opcodes in bytecode.h. */
typedef enum {
    OP_ADD_, OP_SUB_, OP_MUL_, OP_DIV_, OP_MOD_,
    OP_LT_, OP_GT_, OP_LE_, OP_GE_, OP_EQ_, OP_NE_,
    OP_NEG_, OP_POS_
} AstOp;

typedef struct Node Node;

/* A growable array of child nodes. All zero = empty list. */
typedef struct {
    Node **items;
    int count;
    int cap;
} NodeList;

struct Node {
    NodeKind kind;
    int line;               /* source line, for error messages        */

    Node *a, *b, *c, *d;    /* children; meaning depends on kind      */
    NodeList list;          /* block statements, call arguments       */

    char *name;             /* variable / function name (owned)       */
    char **params;          /* N_FUNC_DECL parameter names (owned)    */
    int nparams;
    int ival;               /* N_INT value                            */
    AstOp op;               /* N_BINARY / N_UNARY                     */
    int field;              /* N_FIELD / N_FIELD_ASSIGN: 0 left, 1 right */
};

Node *ast_new(NodeKind kind, int line);
Node *ast_int(int value, int line);
Node *ast_ident(char *name, int line);
Node *ast_binary(AstOp op, Node *l, Node *r, int line);
Node *ast_unary(AstOp op, Node *e, int line);
void  ast_list_push(NodeList *list, Node *n);
void  ast_free(Node *n);
void  ast_print(FILE *out, const Node *n, int indent);
const char *ast_op_str(AstOp op);

#endif
