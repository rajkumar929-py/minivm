#include <stdio.h>
#include "bytecode.h"

/* hand-compiled:   print 2 + 3 * 4;   (line 1) */
static void build_demo(Chunk *c)
{
    int start = chunk_emit_op(c, OP_PUSH, 1);
    chunk_emit_i32(c, 2, 1);
    chunk_mark_stmt(c, start);

    chunk_emit_op(c, OP_PUSH, 1);
    chunk_emit_i32(c, 3, 1);
    chunk_emit_op(c, OP_PUSH, 1);
    chunk_emit_i32(c, 4, 1);
    chunk_emit_op(c, OP_MUL, 1);
    chunk_emit_op(c, OP_ADD, 1);
    chunk_emit_op(c, OP_PRINT, 1);
    chunk_emit_op(c, OP_HALT, 2);
}

int main(void)
{
    Chunk c;
    chunk_init(&c);
    build_demo(&c);

    disassemble_chunk(&c, "demo", stdout);

    printf("\nraw bytes:");
    for (int i = 0; i < c.count; i++) printf(" %02X", c.code[i]);
    printf("\n");

    chunk_free(&c);
    return 0;
}
