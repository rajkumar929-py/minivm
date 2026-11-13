/*
 * vm.c - Stack-based bytecode interpreter.
 *
 * A classic fetch-decode-execute loop with a switch on the opcode byte.
 */
#include "vm.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

const char *vm_state_name(VMState s)
{
    switch (s) {
    case VM_READY:   return "READY";
    case VM_RUNNING: return "RUNNING";
    case VM_HALTED:  return "HALTED";
    case VM_ERROR:   return "ERROR";
    }
    return "?";
}

VM *vm_new(Chunk *chunk)
{
    VM *vm = calloc(1, sizeof *vm);
    if (!vm) return NULL;
    vm->chunk = chunk;
    vm->nglobals = chunk->nglobals;
    vm->globals = calloc((size_t)(chunk->nglobals ? chunk->nglobals : 1), sizeof(Value));
    if (!vm->globals) { free(vm); return NULL; }
    vm->out = stdout;
    vm->state = VM_READY;
    return vm;
}

void vm_free(VM *vm)
{
    if (!vm) return;
    free(vm->globals);
    free(vm);
}

void vm_push(VM *vm, Value v)
{
    if (vm->sp < VM_STACK_MAX) vm->stack[vm->sp++] = v;
}

Value vm_pop(VM *vm)
{
    return vm->sp > 0 ? vm->stack[--vm->sp] : NIL_VAL;
}

/* record a runtime error: message, location, and state = ERROR */
static void runtime_error(VM *vm, int instr_pc, const char *fmt, ...)
{
    char msg[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    int line = (instr_pc >= 0 && instr_pc < vm->chunk->count) ? vm->chunk->lines[instr_pc] : 0;
    snprintf(vm->error, sizeof vm->error, "line %d (pc %d): %s", line, instr_pc, msg);
    vm->error_pc = instr_pc;
    vm->state = VM_ERROR;
}

static const char *type_name(Value v)
{
    switch (v.type) {
    case VAL_NIL: return "nil";
    case VAL_INT: return "int";
    case VAL_OBJ: return "object";
    }
    return "?";
}
/* Execute ONE instruction at vm->pc.
 * Returns 1 to keep going, 0 to stop (HALT or runtime error). */
static int execute_one(VM *vm)
{
    Chunk *c = vm->chunk;
    int ip = vm->pc;                    /* address of this instruction */
    uint8_t op = c->code[ip];

    /* ---- decode ---- */
    if (!is_valid_opcode(op)) { runtime_error(vm, ip, "invalid opcode 0x%02X", op); return 0; }
    int len = 1 + 4 * op_info[op].noperands;
    if (ip + len > c->count) { runtime_error(vm, ip, "truncated instruction"); return 0; }
    int32_t a1 = op_info[op].noperands >= 1 ? chunk_read_i32(c, ip + 1) : 0;
    int32_t a2 = op_info[op].noperands >= 2 ? chunk_read_i32(c, ip + 5) : 0;

    /* ---- safety checks from the op_info table ---- */
    if (op_info[op].pops > 0 && vm->sp < op_info[op].pops) {
        runtime_error(vm, ip, "stack underflow in %s", op_info[op].name); return 0;
    }
    if (op_info[op].pushes > 0 && vm->sp + op_info[op].pushes > VM_STACK_MAX) {
        runtime_error(vm, ip, "stack overflow"); return 0;
    }

    if (vm->trace) {
        fprintf(stderr, "[trace] ");
        disassemble_instr(c, ip, stderr);
    }

    vm->pc = ip + len;                  /* default: fall through to the next instruction */
    vm->steps++;

#define PUSH(v)  (vm->stack[vm->sp++] = (v))
#define POP()    (vm->stack[--vm->sp])
#define PEEK(n)  (vm->stack[vm->sp - 1 - (n)])

    /* ---- execute ---- */
    switch (op) {
    case OP_NOP: break;
    case OP_PUSH: PUSH(INT_VAL(a1)); break;
    case OP_PUSH_NIL: PUSH(NIL_VAL); break;
    case OP_POP: vm->sp--; break;
    case OP_DUP: { Value v = PEEK(0); PUSH(v); break; }

    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD:
    case OP_CMP: case OP_GT: case OP_LE: case OP_GE: {
        Value b = POP(), a = POP();
        if (!IS_INT(a) || !IS_INT(b)) {
            runtime_error(vm, ip, "operands of %s must be integers (got %s and %s)",
                          op_info[op].name, type_name(a), type_name(b));
            return 0;
        }
        int32_t x = AS_INT(a), y = AS_INT(b), r = 0;
        switch (op) {
        /* unsigned arithmetic wraps around safely; signed overflow is undefined in C */
        case OP_ADD: r = (int32_t)((uint32_t)x + (uint32_t)y); break;
        case OP_SUB: r = (int32_t)((uint32_t)x - (uint32_t)y); break;
        case OP_MUL: r = (int32_t)((uint32_t)x * (uint32_t)y); break;
        case OP_DIV:
        case OP_MOD:
            if (y == 0) { runtime_error(vm, ip, "division by zero"); return 0; }
            if (x == INT32_MIN && y == -1) r = (op == OP_DIV) ? INT32_MIN : 0;
            else r = (op == OP_DIV) ? x / y : x % y;
            break;
        case OP_CMP: r = x < y;  break;
        case OP_GT:  r = x > y;  break;
        case OP_LE:  r = x <= y; break;
        case OP_GE:  r = x >= y; break;
        default: break;
        }
        PUSH(INT_VAL(r));
        break;
    }
    case OP_EQ: case OP_NE: {
        Value b = POP(), a = POP();
        int eq = values_equal(a, b);
        PUSH(INT_VAL(op == OP_EQ ? eq : !eq));
        break;
    }
    case OP_NEG: {
        Value a = POP();
        if (!IS_INT(a)) { runtime_error(vm, ip, "operand of NEG must be an integer"); return 0; }
        PUSH(INT_VAL((int32_t)(0u - (uint32_t)AS_INT(a))));
        break;
    }

    case OP_JMP: vm->pc = a1; break;
    case OP_JZ:  { Value v = POP(); if (!value_truthy(v)) vm->pc = a1; break; }
    case OP_JNZ: { Value v = POP(); if (value_truthy(v))  vm->pc = a1; break; }

    case OP_LOAD:
    case OP_STORE:
        if (a1 < 0 || a1 >= vm->nglobals) { runtime_error(vm, ip, "bad global index %d", a1); return 0; }
        if (op == OP_LOAD) PUSH(vm->globals[a1]);
        else vm->globals[a1] = POP();
        break;

    case OP_LOADL:
    case OP_STOREL: {
        if (vm->nframes == 0) { runtime_error(vm, ip, "%s outside a function", op_info[op].name); return 0; }
        int slot = vm->frames[vm->nframes - 1].base + a1;     /* frame pointer + index */
        int limit = (op == OP_STOREL) ? vm->sp - 1 : vm->sp;  /* STOREL's value is on top */
        if (a1 < 0 || slot >= limit) { runtime_error(vm, ip, "bad local slot %d", a1); return 0; }
        if (op == OP_LOADL) PUSH(vm->stack[slot]);
        else vm->stack[slot] = POP();
        break;
    }

    case OP_CALL: {             /* CALL addr argc: the arguments are already pushed */
        if (vm->nframes == VM_FRAMES_MAX) {
            runtime_error(vm, ip, "call stack overflow (recursion deeper than %d)", VM_FRAMES_MAX);
            return 0;
        }
        if (a2 < 0 || a2 > vm->sp) { runtime_error(vm, ip, "bad argument count %d", a2); return 0; }
        if (a1 < 0 || a1 >= c->count) { runtime_error(vm, ip, "call to invalid address %d", a1); return 0; }
        CallFrame *f = &vm->frames[vm->nframes++];
        f->ret_addr = ip + len;         /* come back to the instruction after CALL */
        f->base = vm->sp - a2;          /* the arguments become locals 0..argc-1  */
        vm->pc = a1;
        break;
    }
    case OP_ENTER:              /* ENTER n: make room for n more locals, all nil */
        if (a1 < 0 || vm->sp + a1 > VM_STACK_MAX) { runtime_error(vm, ip, "stack overflow"); return 0; }
        for (int i = 0; i < a1; i++) PUSH(NIL_VAL);
        break;
    case OP_RET: {
        if (vm->nframes == 0) { runtime_error(vm, ip, "RET outside a function"); return 0; }
        if (vm->sp < 1) { runtime_error(vm, ip, "stack underflow in RET"); return 0; }
        Value result = POP();
        CallFrame *f = &vm->frames[--vm->nframes];
        vm->sp = f->base;               /* throw away arguments, locals, leftovers */
        PUSH(result);                   /* the call's value, where the arguments were */
        vm->pc = f->ret_addr;
        break;
    }

    case OP_PRINT: {
        Value v = POP();
        value_print(vm->out, v);
        fputc('\n', vm->out);
        fflush(vm->out);
        break;
    }
    case OP_HALT:
        vm->pc = ip;                    /* stay on HALT */
        vm->state = VM_HALTED;
        return 0;

    default:                            /* heap (Part 8) */
        runtime_error(vm, ip, "%s is not implemented yet", op_info[op].name);
        return 0;
    }
#undef PUSH
#undef POP
#undef PEEK
    return 1;
}

VMState vm_run(VM *vm)
{
    if (vm->state == VM_HALTED || vm->state == VM_ERROR) return vm->state;
    vm->state = VM_RUNNING;
    for (;;) {
        if (vm->pc < 0 || vm->pc >= vm->chunk->count) {
            runtime_error(vm, vm->pc, "program counter out of range");
            break;
        }
        if (!execute_one(vm)) break;
    }
    return vm->state;
}
