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

int main(int argc, char **argv)
{
    const char *which = argc > 1 ? argv[1] : "demo";
    int trace = argc > 2 && strcmp(argv[2], "trace") == 0;

    Chunk c;
    chunk_init(&c);
    if      (strcmp(which, "demo") == 0)      build_demo(&c);
    else {
        fprintf(stderr, "usage: %s [demo] [trace]\n", argv[0]);
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
