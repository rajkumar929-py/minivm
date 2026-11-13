/*
 * langc.c - Command-line driver for the compiler.
 *
 *   langc --tokens file.lang     print the token stream           (Part 3)
 *   langc --ast    file.lang     print the syntax tree            (Part 4)
 *   langc --sema   file.lang     run the semantic pass, print the
 *                                annotated tree                   (Part 5)
 *   langc --dis    file.lang     compile, print the bytecode      (Part 6)
 *   langc --run    file.lang     compile and run on the VM        (Part 6)
 *   langc --trace  file.lang     run, printing every instruction  (Part 6)
 *
 * Exit status: 0 ok, 1 errors in the program, 2 bad command line or file,
 *              3 runtime error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "frontend.h"
#include "semantic.h"
#include "vm.h"

static int usage(void)
{
    fprintf(stderr, "usage: langc (--tokens | --ast | --sema | --dis | --run | --trace) file.lang\n");
    return 2;
}

/* Run a compiled chunk. Returns 0 if it halted normally, 3 on a runtime error. */
static int run_chunk(Chunk *chunk, int trace)
{
    VM *vm = vm_new(chunk);
    if (!vm) { fprintf(stderr, "langc: out of memory\n"); return 3; }
    vm->trace = trace;
    VMState st = vm_run(vm);
    if (st == VM_ERROR) fprintf(stderr, "runtime error: %s\n", vm->error);
    vm_free(vm);
    return st == VM_ERROR ? 3 : 0;
}

int main(int argc, char **argv)
{
    int tokens = 0, ast = 0, sema = 0, dis = 0, run = 0, trace = 0;
    const char *path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0)     tokens = 1;
        else if (strcmp(argv[i], "--ast") == 0)   ast = 1;
        else if (strcmp(argv[i], "--sema") == 0)  sema = 1;
        else if (strcmp(argv[i], "--dis") == 0)   dis = 1;
        else if (strcmp(argv[i], "--run") == 0)   run = 1;
        else if (strcmp(argv[i], "--trace") == 0) run = trace = 1;
        else if (argv[i][0] == '-')               return usage();
        else                                      path = argv[i];
    }
    if (!path || (!tokens && !ast && !sema && !dis && !run)) return usage();

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
    int rc = (fe_error_count() > 0 || sema_errors > 0) ? 1 : 0;

    if (rc == 0 && (dis || run)) {
        Chunk chunk;
        if (!compile_source(src, path, &chunk)) {
            rc = 1;
        } else {
            if (dis) disassemble_chunk(&chunk, path, stdout);
            if (run) rc = run_chunk(&chunk, trace);
            chunk_free(&chunk);
        }
    }
    free(src);
    return rc;
}
