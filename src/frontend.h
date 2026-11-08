/*
 * frontend.h - Interface of the compiler front end (lexer, later parser).
 */
#ifndef FRONTEND_H
#define FRONTEND_H

#include <stdio.h>
#include "ast.h"

/* Error reporting: prints "file:line: error: message" to stderr. */
void fe_begin(const char *filename);        /* set file name, reset count */
void fe_error(int line, const char *fmt, ...);
int  fe_error_count(void);

/* "VAR", "INTEGER", ... for a token number; "PUNCT" for '+' ';' etc. */
const char *fe_token_name(int tok);

/* Parser entry point (implemented in parser.y): the tree, or NULL on any error */
Node *parse_source(const char *src);

/* Lexer entry points (implemented in lexer.l) */
void lexer_begin(const char *src);
void lexer_end(void);
void lexer_dump_tokens(const char *src, FILE *out);

/* Read a whole file into a NUL-terminated heap buffer (NULL on failure). */
char *read_file(const char *path);

#endif
