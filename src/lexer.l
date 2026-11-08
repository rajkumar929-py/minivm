/*
 * lexer.l - Flex specification for the "lang" language.
 *
 * How flex chooses a rule when several match:
 *   1. The longest match wins       ("<=" beats "<", "variable" beats "var")
 *   2. On a tie, the rule listed FIRST wins
 *      -> keywords come before {ID}, so "var" is VAR, not an IDENTIFIER.
 */
%{
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "frontend.h"
#include "parser.tab.h"

static int comment_start_line;

/* runs before every action: record the line of the token for the parser (@1) */
#define YY_USER_ACTION  yylloc.first_line = yylloc.last_line = yylineno;
%}



%option noyywrap nounput noinput yylineno
%x COMMENT

DIGIT   [0-9]
ID      [A-Za-z_][A-Za-z0-9_]*

%%

"/*"                    { comment_start_line = yylineno; BEGIN(COMMENT); }
<COMMENT>"*/"           { BEGIN(INITIAL); }
<COMMENT>\n             { /* yylineno counts the newline */ }
<COMMENT>.              { /* skip comment text */ }
<COMMENT><<EOF>>        { fe_error(comment_start_line, "unterminated block comment");
                          BEGIN(INITIAL); return 0; }

"//".*                  { /* line comment */ }
[ \t\r\n]+              { /* whitespace */ }

"var"                   { return VAR; }
"if"                    { return IF; }
"else"                  { return ELSE; }
"while"                 { return WHILE; }
"for"                   { return FOR; }
"break"                 { return BREAK; }
"continue"              { return CONTINUE; }
"func"                  { return FUNC; }
"return"                { return RETURN; }
"print"                 { return PRINT; }
"pair"                  { return PAIR; }
"nil"                   { return NIL; }

"=="                    { return EQ; }
"!="                    { return NE; }
"<="                    { return LE; }
">="                    { return GE; }

[-+*/%<>=;(){},.]       { return yytext[0]; }

{DIGIT}+                {
                          errno = 0;
                          long v = strtol(yytext, NULL, 10);
                          if (errno == ERANGE || v > INT_MAX) {
                              fe_error(yylineno, "integer literal '%s' out of range", yytext);
                              v = 0;
                          }
                          yylval.ival = (int)v;
                          return INTEGER;
                        }

{ID}                    { yylval.str = strdup(yytext); return IDENTIFIER; }

.                       { fe_error(yylineno, "unexpected character '%s'", yytext); }

%%

/* Start scanning an in-memory string (the whole source file). */
void lexer_begin(const char *src)
{
    yylineno = 1;
    BEGIN(INITIAL);
    yy_scan_string(src);
}

void lexer_end(void)
{
    yy_delete_buffer(YY_CURRENT_BUFFER);
}

/* Print every token, one per line (langc --tokens). */
void lexer_dump_tokens(const char *src, FILE *out)
{
    lexer_begin(src);
    int tok;
    while ((tok = yylex()) != 0) {
        fprintf(out, "line %-3d %-12s '%s'", yylineno, fe_token_name(tok), yytext);
        if (tok == INTEGER) fprintf(out, "  value=%d", yylval.ival);
        fputc('\n', out);
        if (tok == IDENTIFIER) free(yylval.str);
    }
    lexer_end();
}
