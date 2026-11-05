/*
 * tokens.h - Token numbers shared by the lexer and (from Part 4) the parser.
 *
 * TEMPORARY FILE: in Part 4, Bison generates these same names in
 * parser.tab.h, and this file is deleted.
 *
 * A one-character token such as '+' or ';' is returned as its own character
 * code (0-255). Named tokens therefore start at 258, the same as in Bison
 * (0 = end of input, 256 = error, 257 = undefined).
 */
#ifndef TOKENS_H
#define TOKENS_H

enum {
    /* keywords */
    VAR = 258, IF, ELSE, WHILE, FOR, BREAK, CONTINUE,
    FUNC, RETURN, PRINT, PAIR, NIL,
    /* two-character operators */
    EQ, NE, LE, GE,
    /* tokens that carry a value */
    INTEGER, IDENTIFIER
};

/* the value that travels with a token */
typedef union {
    int   ival;     /* INTEGER: the number          */
    char *str;      /* IDENTIFIER: the name (malloc) */
} YYSTYPE;

extern YYSTYPE yylval;

#endif
