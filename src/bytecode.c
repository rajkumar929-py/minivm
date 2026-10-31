/*
 * bytecode.c - Chunk management and the disassembler.
 */
#include "bytecode.h"
#include <stdlib.h>
#include <string.h>

const OpInfo op_info[256] = {
    [OP_NOP]      = { "NOP",      0, 0, 0 },
    [OP_PUSH]     = { "PUSH",     1, 0, 1 },
    [OP_POP]      = { "POP",      0, 1, 0 },
    [OP_DUP]      = { "DUP",      0, 1, 2 },
    [OP_PUSH_NIL] = { "PUSH_NIL", 0, 0, 1 },
    [OP_ADD]      = { "ADD",      0, 2, 1 },
    [OP_SUB]      = { "SUB",      0, 2, 1 },
    [OP_MUL]      = { "MUL",      0, 2, 1 },
    [OP_DIV]      = { "DIV",      0, 2, 1 },
    [OP_CMP]      = { "CMP",      0, 2, 1 },
    [OP_MOD]      = { "MOD",      0, 2, 1 },
    [OP_EQ]       = { "EQ",       0, 2, 1 },
    [OP_NE]       = { "NE",       0, 2, 1 },
    [OP_GT]       = { "GT",       0, 2, 1 },
    [OP_LE]       = { "LE",       0, 2, 1 },
    [OP_GE]       = { "GE",       0, 2, 1 },
    [OP_NEG]      = { "NEG",      0, 1, 1 },
    [OP_JMP]      = { "JMP",      1, 0, 0 },
    [OP_JZ]       = { "JZ",       1, 1, 0 },
    [OP_JNZ]      = { "JNZ",      1, 1, 0 },
    [OP_STORE]    = { "STORE",    1, 1, 0 },
    [OP_LOAD]     = { "LOAD",     1, 0, 1 },
    [OP_LOADL]    = { "LOADL",    1, 0, 1 },
    [OP_STOREL]   = { "STOREL",   1, 1, 0 },
    [OP_CALL]     = { "CALL",     2, -1, 0 },
    [OP_RET]      = { "RET",      0, -1, 1 },
    [OP_ENTER]    = { "ENTER",    1, 0, -1 },
    [OP_NEWPAIR]  = { "NEWPAIR",  0, 2, 1 },
    [OP_GETL]     = { "GETL",     0, 1, 1 },
    [OP_GETR]     = { "GETR",     0, 1, 1 },
    [OP_SETL]     = { "SETL",     0, 2, 0 },
    [OP_SETR]     = { "SETR",     0, 2, 0 },
    [OP_PRINT]    = { "PRINT",    0, 1, 0 },
    [OP_HALT]     = { "HALT",     0, 0, 0 },
};

int is_valid_opcode(uint8_t op) { return op_info[op].name != NULL; }

void chunk_init(Chunk *c) { memset(c, 0, sizeof *c); }

void chunk_free(Chunk *c)
{
    free(c->code);
    free(c->lines);
    free(c->flags);
    memset(c, 0, sizeof *c);
}

/* append one byte, growing the three parallel arrays when full */
static void emit_byte(Chunk *c, uint8_t b, int line, uint8_t flags)
{
    if (c->count == c->cap) {
        c->cap = c->cap ? c->cap * 2 : 64;
        c->code  = realloc(c->code,  (size_t)c->cap);
        c->lines = realloc(c->lines, (size_t)c->cap * sizeof(int));
        c->flags = realloc(c->flags, (size_t)c->cap);
        if (!c->code || !c->lines || !c->flags) { perror("realloc"); exit(1); }
    }
    c->code[c->count]  = b;
    c->lines[c->count] = line;
    c->flags[c->count] = flags;
    c->count++;
}

int chunk_emit_op(Chunk *c, OpCode op, int line)
{
    int at = c->count;
    emit_byte(c, (uint8_t)op, line, BC_INSTR_START);
    return at;
}

/* operands are little-endian: lowest byte first */
void chunk_emit_i32(Chunk *c, int32_t v, int line)
{
    uint32_t u = (uint32_t)v;
    for (int i = 0; i < 4; i++) emit_byte(c, (uint8_t)(u >> (8 * i)), line, 0);
}

void chunk_patch_i32(Chunk *c, int offset, int32_t v)
{
    uint32_t u = (uint32_t)v;
    for (int i = 0; i < 4; i++) c->code[offset + i] = (uint8_t)(u >> (8 * i));
}

int32_t chunk_read_i32(const Chunk *c, int offset)
{
    uint32_t u = 0;
    for (int i = 0; i < 4; i++) u |= (uint32_t)c->code[offset + i] << (8 * i);
    return (int32_t)u;
}

void chunk_mark_stmt(Chunk *c, int offset)
{
    if (offset >= 0 && offset < c->count) c->flags[offset] |= BC_STMT;
}

int instr_length(const Chunk *c, int offset)
{
    return 1 + 4 * op_info[c->code[offset]].noperands;
}
