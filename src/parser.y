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
 *              -> primary     42  nil  x  (expr)   (highest: done first)
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
%}

%locations
%define parse.error verbose

%union {
    int       ival;
    char     *str;
    Node     *node;
    NodeList  list;
}

%token VAR "var" IF "if" ELSE "else" WHILE "while" FOR "for"
%token BREAK "break" CONTINUE "continue" FUNC "func" RETURN "return"
%token PRINT "print" PAIR "pair" NIL "nil"
%token EQ "==" NE "!=" LE "<=" GE ">="
%token <ival> INTEGER "integer"
%token <str>  IDENTIFIER "identifier"

%type <node> statement var_decl simple_assign
%type <node> expression equality comparison term factor unary primary
%type <list> items

%start program

%%

program
    : items                         { g_root = ast_new(N_PROGRAM, 1); g_root->list = $1; }
    ;

items
    : %empty                        { memset(&$$, 0, sizeof $$); }
    | items statement               { $$ = $1; ast_list_push(&$$, $2); }
    ;

statement
    : var_decl                      { $$ = $1; }
    | simple_assign ';'             { $$ = $1; }
    | PRINT expression ';'          { $$ = ast_new(N_PRINT, @1.first_line); $$->a = $2; }
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
    | primary                       { $$ = $1; }
    ;

primary
    : INTEGER                       { $$ = ast_int($1, @1.first_line); }
    | NIL                           { $$ = ast_new(N_NIL, @1.first_line); }
    | IDENTIFIER                    { $$ = ast_ident($1, @1.first_line); }
    | '(' expression ')'            { $$ = $2; }
    ;

%%

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
