/*
 * codegen.h - Code generation: annotated AST -> bytecode.
 */
#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "bytecode.h"
#include "semantic.h"

/* Emit bytecode for an analysed program into chunk. Returns 1 on success. */
int codegen_program(Node *program, const SemaResult *sema, Chunk *chunk,
                    const char *filename);

/* The whole compiler: parse + semantic pass + codegen. Returns 1 on success.
   On failure the errors are already printed and out is left empty. */
int compile_source(const char *src, const char *filename, Chunk *out);

#endif
