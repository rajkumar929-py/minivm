/*
 * bytecode.h - The instruction set and the Chunk that holds a program.
 *
 * Encoding: 1-byte opcode, then 0, 1 or 2 operands.
 * Each operand is a 4-byte little-endian int32.
 */
#ifndef BYTECODE_H
#define BYTECODE_H

#include <stdint.h>
#include <stdio.h>

typedef enum {
    /* stack */
    OP_NOP      = 0x00,
    OP_PUSH     = 0x01,   /* PUSH imm   [] -> [imm]                 */
    OP_POP      = 0x02,   /* [a] -> []                              */
    OP_DUP      = 0x03,   /* [a] -> [a a]                           */
    OP_PUSH_NIL = 0x04,   /* [] -> [nil]                            */
    /* arithmetic / comparison: pop b, pop a, push (a op b) */
    OP_ADD      = 0x10,
    OP_SUB      = 0x11,
    OP_MUL      = 0x12,
    OP_DIV      = 0x13,
    OP_CMP      = 0x14,   /* a < b                                  */
    OP_MOD      = 0x15,
    OP_EQ       = 0x16,
    OP_NE       = 0x17,
    OP_GT       = 0x18,
    OP_LE       = 0x19,
    OP_GE       = 0x1A,
    OP_NEG      = 0x1B,   /* [a] -> [-a]                            */
    /* control flow */
    OP_JMP      = 0x20,   /* JMP addr                               */
    OP_JZ       = 0x21,   /* JZ addr  : pop c; if !c goto addr      */
    OP_JNZ      = 0x22,   /* JNZ addr : pop c; if  c goto addr      */
    /* variables */
    OP_STORE    = 0x30,   /* STORE idx : globals[idx] = pop         */
    OP_LOAD     = 0x31,   /* LOAD idx  : push globals[idx]          */
    OP_LOADL    = 0x32,   /* LOADL idx : push stack[fp + idx]       */
    OP_STOREL   = 0x33,   /* STOREL idx: stack[fp + idx] = pop      */
    /* functions */
    OP_CALL     = 0x40,   /* CALL addr argc                         */
    OP_RET      = 0x41,   /* RET                                    */
    OP_ENTER    = 0x42,   /* ENTER n : make room for n locals       */
    /* heap */
    OP_NEWPAIR  = 0x50,   /* [a b] -> [pair(a,b)]                   */
    OP_GETL     = 0x51,   /* [p] -> [p.left]                        */
    OP_GETR     = 0x52,   /* [p] -> [p.right]                       */
    OP_SETL     = 0x53,   /* [p v] -> []   p.left = v               */
    OP_SETR     = 0x54,   /* [p v] -> []   p.right = v              */
    /* I/O */
    OP_PRINT    = 0x60,   /* [a] -> []  print a                     */
    OP_HALT     = 0xFF
} OpCode;

/* static facts about each opcode */
typedef struct {
    const char *name;
    int noperands;          /* how many 4-byte operands follow       */
    int pops, pushes;       /* stack effect (-1 = depends)           */
} OpInfo;

/* per-byte flags */
enum {
    BC_INSTR_START = 1 << 0,    /* this byte is an opcode            */
    BC_STMT        = 1 << 1     /* first instruction of a statement  */
};

typedef struct {
    uint8_t *code;          /* the instruction bytes                 */
    int *lines;             /* lines[i] = source line of code[i]     */
    uint8_t *flags;         /* flags[i] = BC_* bits for code[i]      */
    int count, cap;         /* bytes used, bytes allocated           */
    int nglobals;           /* number of global variable slots       */
} Chunk;

extern const OpInfo op_info[256];

void chunk_init(Chunk *c);
void chunk_free(Chunk *c);
int  chunk_emit_op(Chunk *c, OpCode op, int line);   /* returns its offset */
void chunk_emit_i32(Chunk *c, int32_t v, int line);
void chunk_patch_i32(Chunk *c, int offset, int32_t v);
int32_t chunk_read_i32(const Chunk *c, int offset);
void chunk_mark_stmt(Chunk *c, int offset);

int  is_valid_opcode(uint8_t op);
int  instr_length(const Chunk *c, int offset);

int  disassemble_instr(const Chunk *c, int offset, FILE *out);   /* returns next offset */
void disassemble_chunk(const Chunk *c, const char *name, FILE *out);

#endif
