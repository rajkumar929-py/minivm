/*
 * langc.c - Command-line driver for the compiler.
 *
 *   langc --tokens file.lang     print the token stream           (Part 3)
 *   langc --ast    file.lang     print the syntax tree            (Part 4)
 *   langc --sema   file.lang     run the semantic pass, print the
 *                                annotated tree                   (Part 5)
 *
 * Exit status: 0 ok, 1 errors in the program, 2 bad command line or file.
 * More flags arrive later: --dis and --run (Part 6).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frontend.h"
#include "semantic.h"

static int usage(void)
{
    fprintf(stderr, "usage: langc (--tokens | --ast | --sema) file.lang\n");
    return 2;
}

int main(int argc, char **argv)
{
    int tokens = 0, ast = 0, sema = 0;
    const char *path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0)    tokens = 1;
        else if (strcmp(argv[i], "--ast") == 0)  ast = 1;
        else if (strcmp(argv[i], "--sema") == 0) sema = 1;
        else if (argv[i][0] == '-')              return usage();
        else                                     path = argv[i];
    }
    if (!path || (!tokens && !ast && !sema)) return usage();

    char *src = read_file(path);
    if (!src) {
        fprintf(stderr, "langc: cannot read '%s'\n", path);
        return 2;
    }

    fe_begin(path);
    if (tokens) lexer_dump_tokens(src, stdout);

    int sema_errors = 0;
    if (ast || sema) {
        Node *tree = parse_source(src);
        if (tree) {
            if (sema) {
                SemaResult r = semantic_analyze(tree, path);
                sema_errors = r.errors;
                if (r.errors == 0) {
                    ast_print(stdout, tree, 0);
                    printf("globals=%d functions=%d alloc_sites=%d\n",
                           r.nglobals, r.nfuncs, r.nalloc_sites);
                }
            } else {
                ast_print(stdout, tree, 0);
            }
            ast_free(tree);
        }
    }
    free(src);

    return (fe_error_count() > 0 || sema_errors > 0) ? 1 : 0;
}
