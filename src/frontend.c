/*
 * frontend.c - Shared helpers for the front end: errors, token names, files.
 */
#include <stdarg.h>
#include <stdlib.h>
#include "frontend.h"
#include "parser.tab.h"



static const char *g_filename = "<input>";
static int g_errors;

void fe_begin(const char *filename)
{
    g_filename = filename ? filename : "<input>";
    g_errors = 0;
}

void fe_error(int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d: error: ", g_filename, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    g_errors++;
}

int fe_error_count(void)
{
    return g_errors;
}

const char *fe_token_name(int tok)
{
    switch (tok) {
    case VAR: return "VAR";           case IF: return "IF";
    case ELSE: return "ELSE";         case WHILE: return "WHILE";
    case FOR: return "FOR";           case BREAK: return "BREAK";
    case CONTINUE: return "CONTINUE"; case FUNC: return "FUNC";
    case RETURN: return "RETURN";     case PRINT: return "PRINT";
    case PAIR: return "PAIR";         case NIL: return "NIL";
    case EQ: return "EQ";             case NE: return "NE";
    case LE: return "LE";             case GE: return "GE";
    case INTEGER: return "INTEGER";   case IDENTIFIER: return "IDENTIFIER";
    default: return "PUNCT";
    }
}

char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}
