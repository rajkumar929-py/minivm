/*
 * parser.y - Bison grammar for the "lang" language: tokens in, AST out.
 *
 * Precedence is built into the grammar: one rule per level, lowest first.
 *
 *   expression -> equality    == !=          (lowest: done last)
 *              -> comparison  < > <= >=
 *              -> term        + -
 *              -> factor      * / %
 *              -> unary       -x +x
 *              -> postfix     p.left  p.right
 *              -> primary     42  nil  x  f(..)  pair(..)  (expr)   (highest)
 *
 * Each binary level is LEFT-recursive (term: term '+' factor), which makes
 * the operators left-associative: 10 - 3 - 2 means (10 - 3) - 2.
 */
%code requires {
#include "ast.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frontend.h"

int yylex(void);
void yyerror(const char *msg);

static Node *g_root;    /* the finished tree, set by the 'program' rule */

static int field_index(const char *name, int line);

%}

%locations
%define parse.error verbose

%union {
    int       ival;
    char     *str;
    Node     *node;
    NodeList  list;
    struct { char **names; int count; } params;   /* function parameters */

}

%token VAR "var" IF "if" ELSE "else" WHILE "while" FOR "for"
%token BREAK "break" CONTINUE "continue" FUNC "func" RETURN "return"
%token PRINT "print" PAIR "pair" NIL "nil"
%token EQ "==" NE "!=" LE "<=" GE ">="
%token <ival> INTEGER "integer"
%token <str>  IDENTIFIER "identifier"

/* dangling else: an 'else' binds to the nearest 'if' (see if_stmt) */
%precedence LOWER_THAN_ELSE
%precedence ELSE

%type <node> item statement var_decl simple_assign block func_decl
%type <node> if_stmt while_stmt for_stmt for_init opt_simple_assign opt_expr
%type <node> expression equality comparison term factor unary postfix primary call
%type <list> items statement_list args arg_list
%type <params> param_list param_list_opt


%start program

%%

program
    : items                         { g_root = ast_new(N_PROGRAM, 1); g_root->list = $1; }
    ;

items
    : %empty                        { memset(&$$, 0, sizeof $$); }
        | items item                    { $$ = $1; ast_list_push(&$$, $2); }
    ;

item
    : statement                     { $$ = $1; }
    | func_decl                     { $$ = $1; }
    ;

func_decl
    : FUNC IDENTIFIER '(' param_list_opt ')' block
        {
            $$ = ast_new(N_FUNC_DECL, @1.first_line);
            $$->name = $2;
            $$->params = $4.names;
            $$->nparams = $4.count;
            $$->a = $6;
        }
    ;

param_list_opt
    : %empty                        { $$.names = NULL; $$.count = 0; }
    | param_list                    { $$ = $1; }
    ;

param_list
    : IDENTIFIER
        { $$.names = malloc(sizeof(char *)); $$.names[0] = $1; $$.count = 1; }
    | param_list ',' IDENTIFIER
        {
            $$ = $1;
            $$.names = realloc($$.names, ($$.count + 1) * sizeof(char *));
            $$.names[$$.count++] = $3;
        }
    ;


statement
    : var_decl                      { $$ = $1; }
    | simple_assign ';'             { $$ = $1; }
    | postfix '.' IDENTIFIER '=' expression ';'
        {
            $$ = ast_new(N_FIELD_ASSIGN, @1.first_line);
            $$->a = $1;
            $$->b = $5;
            $$->field = field_index($3, @3.first_line);
            free($3);
        }

    | if_stmt                       { $$ = $1; }
    | while_stmt                    { $$ = $1; }
    | for_stmt                      { $$ = $1; }
    | block                         { $$ = $1; }
    | PRINT expression ';'          { $$ = ast_new(N_PRINT, @1.first_line); $$->a = $2; }
    | call ';'                      { $$ = ast_new(N_EXPR_STMT, @1.first_line); $$->a = $1; }
    | RETURN ';'                    { $$ = ast_new(N_RETURN, @1.first_line); }
    | RETURN expression ';'         { $$ = ast_new(N_RETURN, @1.first_line); $$->a = $2; }
    | BREAK ';'                     { $$ = ast_new(N_BREAK, @1.first_line); }
    | CONTINUE ';'                  { $$ = ast_new(N_CONTINUE, @1.first_line); }

    ;

var_decl
    : VAR IDENTIFIER ';'
        { $$ = ast_new(N_VAR_DECL, @1.first_line); $$->name = $2; }
    | VAR IDENTIFIER '=' expression ';'
        { $$ = ast_new(N_VAR_DECL, @1.first_line); $$->name = $2; $$->a = $4; }
    ;

simple_assign
    : IDENTIFIER '=' expression
        { $$ = ast_new(N_ASSIGN, @1.first_line); $$->name = $1; $$->a = $3; }
    ;
block
    : '{' statement_list '}'        { $$ = ast_new(N_BLOCK, @1.first_line); $$->list = $2; }
    ;

statement_list
    : %empty                        { memset(&$$, 0, sizeof $$); }
    | statement_list statement      { $$ = $1; ast_list_push(&$$, $2); }
    ;

if_stmt
    : IF '(' expression ')' statement %prec LOWER_THAN_ELSE
        { $$ = ast_new(N_IF, @1.first_line); $$->a = $3; $$->b = $5; }
    | IF '(' expression ')' statement ELSE statement
        { $$ = ast_new(N_IF, @1.first_line); $$->a = $3; $$->b = $5; $$->c = $7; }
    ;

while_stmt
    : WHILE '(' expression ')' statement
        { $$ = ast_new(N_WHILE, @1.first_line); $$->a = $3; $$->b = $5; }
    ;

for_stmt
    : FOR '(' for_init ';' opt_expr ';' opt_simple_assign ')' statement
        {
            $$ = ast_new(N_FOR, @1.first_line);
            $$->a = $3; $$->b = $5; $$->c = $7; $$->d = $9;
        }
    ;

for_init
    : %empty                        { $$ = NULL; }
    | VAR IDENTIFIER '=' expression
        { $$ = ast_new(N_VAR_DECL, @1.first_line); $$->name = $2; $$->a = $4; }
    | simple_assign                 { $$ = $1; }
    ;

opt_simple_assign
    : %empty                        { $$ = NULL; }
    | simple_assign                 { $$ = $1; }
    ;

opt_expr
    : %empty                        { $$ = NULL; }
    | expression                    { $$ = $1; }
    ;


expression
    : equality                      { $$ = $1; }
    ;

equality
    : comparison                    { $$ = $1; }
    | equality EQ comparison        { $$ = ast_binary(OP_EQ_, $1, $3, @2.first_line); }
    | equality NE comparison        { $$ = ast_binary(OP_NE_, $1, $3, @2.first_line); }
    ;

comparison
    : term                          { $$ = $1; }
    | comparison '<' term           { $$ = ast_binary(OP_LT_, $1, $3, @2.first_line); }
    | comparison '>' term           { $$ = ast_binary(OP_GT_, $1, $3, @2.first_line); }
    | comparison LE term            { $$ = ast_binary(OP_LE_, $1, $3, @2.first_line); }
    | comparison GE term            { $$ = ast_binary(OP_GE_, $1, $3, @2.first_line); }
    ;

term
    : factor                        { $$ = $1; }
    | term '+' factor               { $$ = ast_binary(OP_ADD_, $1, $3, @2.first_line); }
    | term '-' factor               { $$ = ast_binary(OP_SUB_, $1, $3, @2.first_line); }
    ;

factor
    : unary                         { $$ = $1; }
    | factor '*' unary              { $$ = ast_binary(OP_MUL_, $1, $3, @2.first_line); }
    | factor '/' unary              { $$ = ast_binary(OP_DIV_, $1, $3, @2.first_line); }
    | factor '%' unary              { $$ = ast_binary(OP_MOD_, $1, $3, @2.first_line); }
    ;

unary
    : '-' unary                     { $$ = ast_unary(OP_NEG_, $2, @1.first_line); }
    | '+' unary                     { $$ = ast_unary(OP_POS_, $2, @1.first_line); }
    | postfix                       { $$ = $1; }
    ;

postfix
    : primary                       { $$ = $1; }
    | postfix '.' IDENTIFIER
        {
            $$ = ast_new(N_FIELD, @2.first_line);
            $$->a = $1;
            $$->field = field_index($3, @3.first_line);
            free($3);
        }
    ;

primary
    : INTEGER                       { $$ = ast_int($1, @1.first_line); }
    | NIL                           { $$ = ast_new(N_NIL, @1.first_line); }
    | IDENTIFIER                    { $$ = ast_ident($1, @1.first_line); }
    | call                          { $$ = $1; }
    | PAIR '(' expression ',' expression ')'
        { $$ = ast_new(N_PAIR, @1.first_line); $$->a = $3; $$->b = $5; }

    | '(' expression ')'            { $$ = $2; }
    ;

call
    : IDENTIFIER '(' args ')'
        { $$ = ast_new(N_CALL, @1.first_line); $$->name = $1; $$->list = $3; }
    ;

args
    : %empty                        { memset(&$$, 0, sizeof $$); }
    | arg_list                      { $$ = $1; }
    ;

arg_list
    : expression                    { memset(&$$, 0, sizeof $$); ast_list_push(&$$, $1); }
    | arg_list ',' expression       { $$ = $1; ast_list_push(&$$, $3); }
    ;


%%


/* p.left -> 0, p.right -> 1. Pairs have no other fields. */
static int field_index(const char *name, int line)
{
    if (strcmp(name, "left") == 0)  return 0;
    if (strcmp(name, "right") == 0) return 1;
    fe_error(line, "unknown field '%s' (pairs only have 'left' and 'right')", name);
    return 0;
}

/* Bison calls this on a syntax error. yylloc is the location of the bad token. */
void yyerror(const char *msg)
{
    fe_error(yylloc.first_line, "%s", msg);
}

/* Parse a whole source string. Returns the tree, or NULL if there was any error. */
Node *parse_source(const char *src)
{
    g_root = NULL;
    lexer_begin(src);
    int rc = yyparse();
    lexer_end();

    if (rc != 0 || fe_error_count() > 0) {
        ast_free(g_root);           /* safe on NULL */
        g_root = NULL;
    }
    return g_root;
}
