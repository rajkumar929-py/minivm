/*
 * langc.c - Command-line driver for the compiler.
 *
 *   langc --tokens file.lang     print the token stream (Part 3)
 *   langc --ast    file.lang     print the syntax tree  (Part 4)
 *
 * Exit status: 0 ok, 1 errors in the program, 2 bad command line or file.
 * More flags arrive later: --dis and --run (Part 6).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frontend.h"

static int usage(void)
{
    fprintf(stderr, "usage: langc (--tokens | --ast) file.lang\n");
    return 2;
}

int main(int argc, char **argv)
{
    int tokens = 0, ast = 0;
    const char *path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0)   tokens = 1;
        else if (strcmp(argv[i], "--ast") == 0) ast = 1;
        else if (argv[i][0] == '-')             return usage();
        else                                    path = argv[i];
    }
    if (!path || (!tokens && !ast)) return usage();

    char *src = read_file(path);
    if (!src) {
        fprintf(stderr, "langc: cannot read '%s'\n", path);
        return 2;
    }

    fe_begin(path);
    if (tokens) lexer_dump_tokens(src, stdout);

    if (ast) {
        Node *tree = parse_source(src);
        if (tree) {
            ast_print(stdout, tree, 0);
            ast_free(tree);
        }
    }
    free(src);

    return fe_error_count() > 0 ? 1 : 0;
}
