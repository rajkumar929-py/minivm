#include <stdio.h>
#include <string.h>
#include "bytecode.h"
#include "vm.h"

/* print 2 + 3 * 4; */
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

/*
 * Globals: slot 0 = i, slot 1 = sum
 *   1: i = 1;
 *   2: sum = 0;
 *   3: while (i <= 5) {
 *   4:     sum = sum + i;
 *   5:     i = i + 1;
 *      }
 *   6: print sum;
 */
static void build_loop(Chunk *c)
{
    enum { I = 0, SUM = 1 };
    c->nglobals = 2;

    /* line 1: i = 1 */
    chunk_mark_stmt(c, chunk_emit_op(c, OP_PUSH, 1));
    chunk_emit_i32(c, 1, 1);
    chunk_emit_op(c, OP_STORE, 1); chunk_emit_i32(c, I, 1);

    /* line 2: sum = 0 */
    chunk_mark_stmt(c, chunk_emit_op(c, OP_PUSH, 2));
    chunk_emit_i32(c, 0, 2);
    chunk_emit_op(c, OP_STORE, 2); chunk_emit_i32(c, SUM, 2);

    /* line 3: loop test.  Remember where it starts: we jump back here. */
    int loop_start = chunk_emit_op(c, OP_LOAD, 3);
    chunk_mark_stmt(c, loop_start);
    chunk_emit_i32(c, I, 3);
    chunk_emit_op(c, OP_PUSH, 3);  chunk_emit_i32(c, 5, 3);
    chunk_emit_op(c, OP_LE, 3);
    chunk_emit_op(c, OP_JZ, 3);
    int exit_operand = c->count;          /* where JZ's target will go */
    chunk_emit_i32(c, -1, 3);             /* placeholder: exit not known yet */

    /* line 4: sum = sum + i */
    chunk_mark_stmt(c, chunk_emit_op(c, OP_LOAD, 4));
    chunk_emit_i32(c, SUM, 4);
    chunk_emit_op(c, OP_LOAD, 4);  chunk_emit_i32(c, I, 4);
    chunk_emit_op(c, OP_ADD, 4);
    chunk_emit_op(c, OP_STORE, 4); chunk_emit_i32(c, SUM, 4);

    /* line 5: i = i + 1 */
    chunk_mark_stmt(c, chunk_emit_op(c, OP_LOAD, 5));
    chunk_emit_i32(c, I, 5);
    chunk_emit_op(c, OP_PUSH, 5);  chunk_emit_i32(c, 1, 5);
    chunk_emit_op(c, OP_ADD, 5);
    chunk_emit_op(c, OP_STORE, 5); chunk_emit_i32(c, I, 5);
    chunk_emit_op(c, OP_JMP, 5);   chunk_emit_i32(c, loop_start, 5);

    /* line 6: print sum.  NOW we know where the loop exits to. */
    int loop_exit = chunk_emit_op(c, OP_LOAD, 6);
    chunk_mark_stmt(c, loop_exit);
    chunk_emit_i32(c, SUM, 6);
    chunk_emit_op(c, OP_PRINT, 6);
    chunk_emit_op(c, OP_HALT, 6);

    chunk_patch_i32(c, exit_operand, loop_exit);   /* back-patch */
}


int main(int argc, char **argv)
{
    const char *which = argc > 1 ? argv[1] : "demo";
    int trace = argc > 2 && strcmp(argv[2], "trace") == 0;

    Chunk c;
    chunk_init(&c);
    if      (strcmp(which, "demo") == 0)      build_demo(&c);
    else if (strcmp(which, "loop") == 0)      build_loop(&c);
    else {
        fprintf(stderr, "usage: %s [demo|loop] [trace]\n", argv[0]);
        return 2;
    }

    if (trace) disassemble_chunk(&c, which, stdout);

    VM *vm = vm_new(&c);
    vm->trace = trace;
    VMState st = vm_run(vm);

    if (st == VM_ERROR)
        fprintf(stderr, "runtime error: %s\n", vm->error);
    printf("[%s after %lld instructions, stack depth %d]\n",
           vm_state_name(st), vm->steps, vm->sp);

    int code = (st == VM_ERROR) ? 1 : 0;
    vm_free(vm);
    chunk_free(&c);
    return code;
}
