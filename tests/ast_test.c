/* ast_test.c - build the tree for "print 2 + 3 * 4;" by hand and print it */
#include <stdio.h>
#include "ast.h"

int main(void)
{
    /* 3 * 4 first: it sits lower in the tree, so it is evaluated first */
    Node *mul = ast_binary(OP_MUL_, ast_int(3, 1), ast_int(4, 1), 1);
    Node *add = ast_binary(OP_ADD_, ast_int(2, 1), mul, 1);

    Node *print = ast_new(N_PRINT, 1);
    print->a = add;

    Node *prog = ast_new(N_PROGRAM, 1);
    ast_list_push(&prog->list, print);

    ast_print(stdout, prog, 0);
    ast_free(prog);                 /* frees the whole tree */
    return 0;
}
