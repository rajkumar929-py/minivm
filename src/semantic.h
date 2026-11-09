/*
 * semantic.h - The semantic pass.
 *
 * Walks the AST with a scoped symbol table and:
 *   - rejects undeclared variables and functions
 *   - rejects redeclaration in the same scope
 *   - checks call arity, and where break, continue and return appear
 *   - ANNOTATES the tree: every variable gets (sym_kind, slot), every call
 *     gets func_index, every pair() gets alloc_site, every function nlocals.
 *
 * The code generator relies entirely on these annotations.
 */
#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"

typedef struct {
    int nglobals;       /* global slots the program needs */
    int nfuncs;         /* number of functions            */
    int nalloc_sites;   /* number of pair() sites         */
    int errors;
} SemaResult;

SemaResult semantic_analyze(Node *program, const char *filename);

#endif
