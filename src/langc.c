/*
 * langc.c - Command-line driver for the compiler.
 *
 *   langc --tokens file.lang     print the token stream (Part 3)
 *
 * More flags arrive later: --ast (Part 4), --dis and --run (Part 6).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frontend.h"

static int usage(void)
{
    fprintf(stderr, "usage: langc --tokens file.lang\n");
    return 2;
}

int main(int argc, char **argv)
{
    int tokens = 0;
    const char *path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) tokens = 1;
        else if (argv[i][0] == '-')           return usage();
        else                                  path = argv[i];
    }
    if (!path || !tokens) return usage();

    char *src = read_file(path);
    if (!src) {
        fprintf(stderr, "langc: cannot read '%s'\n", path);
        return 2;
    }

    fe_begin(path);
    lexer_dump_tokens(src, stdout);
    free(src);

    return fe_error_count() > 0 ? 1 : 0;
}
