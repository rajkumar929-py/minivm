/*
 * vm.h - Stack-based bytecode virtual machine.
 *
 * Machine state:
 *   pc        program counter (byte offset into chunk->code)
 *   stack/sp  operand stack; sp = number of values on it
 *   globals   global variables, one Value per slot
 *   state     READY -> RUNNING -> HALTED | ERROR
 */
#ifndef VM_H
#define VM_H

#include "bytecode.h"
#include "value.h"

#define VM_STACK_MAX 4096

typedef enum {
    VM_READY,       /* loaded, not yet started  */
    VM_RUNNING,     /* inside vm_run()          */
    VM_HALTED,      /* executed HALT            */
    VM_ERROR        /* runtime error            */
} VMState;

typedef struct {
    Chunk *chunk;
    int pc;

    Value stack[VM_STACK_MAX];
    int sp;                     /* number of values on the stack */

    Value *globals;
    int nglobals;

    VMState state;
    char error[256];            /* message when state == VM_ERROR */
    int error_pc;

    long long steps;            /* instructions executed          */
    FILE *out;                  /* where PRINT writes             */
    int trace;                  /* print every instruction        */
} VM;

VM     *vm_new(Chunk *chunk);
void    vm_free(VM *vm);
VMState vm_run(VM *vm);
const char *vm_state_name(VMState s);

void  vm_push(VM *vm, Value v);     /* C API, used by tests */
Value vm_pop(VM *vm);

#endif
