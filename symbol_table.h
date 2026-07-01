#ifndef GV_SYMBOL_TABLE_H
#define GV_SYMBOL_TABLE_H

#include "ast.h"

typedef enum {
    SYM_VAR = 0,
    SYM_PARAM,
    SYM_FUNC
} SymbolKind;

typedef struct ParamInfo {
    char *name;
    Gv1Type type;
    int is_vector;
    int vector_size;
    struct ParamInfo *next;
} ParamInfo;

typedef struct Symbol {
    char *name;
    SymbolKind kind;
    Gv1Type type;
    int is_vector;
    int vector_size;
    int param_count;
    ParamInfo *params;
    char *label;
    struct Symbol *next;
} Symbol;

typedef struct Scope {
    Symbol *symbols;
    struct Scope *next;
} Scope;

typedef struct SymbolTableStack {
    Scope *top;
} SymbolTableStack;

void symtab_init(SymbolTableStack *stack);
void symtab_push_scope(SymbolTableStack *stack);
void symtab_pop_scope(SymbolTableStack *stack);
void symtab_free(SymbolTableStack *stack);

Symbol *symtab_lookup(SymbolTableStack *stack, const char *name);
Symbol *symtab_lookup_current(SymbolTableStack *stack, const char *name);

Symbol *symtab_declare_var(SymbolTableStack *stack, const char *name, Gv1Type type, int is_vector, int vector_size, const char *label);
Symbol *symtab_declare_param(SymbolTableStack *stack, const char *name, Gv1Type type, int is_vector, int vector_size);
Symbol *symtab_declare_func(SymbolTableStack *stack, const char *name, Gv1Type type, int param_count, ParamInfo *params);

ParamInfo *paraminfo_new(const char *name, Gv1Type type, int is_vector, int vector_size);
void paraminfo_append(ParamInfo **list, ParamInfo *node);
void paraminfo_free(ParamInfo *list);

#endif
