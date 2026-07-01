#include "symbol_table.h"

#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s) {
    size_t len;
    char *copy;

    if (!s) {
        return NULL;
    }
    len = strlen(s);
    copy = (char *)malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, s, len + 1);
    return copy;
}

static void symbol_free(Symbol *sym) {
    if (!sym) {
        return;
    }
    free(sym->name);
    free(sym->label);
    paraminfo_free(sym->params);
    free(sym);
}

void paraminfo_free(ParamInfo *list) {
    ParamInfo *next;
    while (list) {
        next = list->next;
        free(list->name);
        free(list);
        list = next;
    }
}

ParamInfo *paraminfo_new(const char *name, Gv1Type type, int is_vector, int vector_size) {
    ParamInfo *p = (ParamInfo *)calloc(1, sizeof(ParamInfo));
    if (!p) {
        return NULL;
    }
    p->name = dup_str(name);
    p->type = type;
    p->is_vector = is_vector;
    p->vector_size = vector_size;
    return p;
}

void paraminfo_append(ParamInfo **list, ParamInfo *node) {
    ParamInfo *cur;

    if (!list || !node) {
        return;
    }
    if (!*list) {
        *list = node;
        return;
    }
    cur = *list;
    while (cur->next) {
        cur = cur->next;
    }
    cur->next = node;
}

void symtab_init(SymbolTableStack *stack) {
    stack->top = NULL;
}

void symtab_push_scope(SymbolTableStack *stack) {
    Scope *scope = (Scope *)calloc(1, sizeof(Scope));
    if (!scope) {
        return;
    }
    scope->next = stack->top;
    stack->top = scope;
}

void symtab_pop_scope(SymbolTableStack *stack) {
    Scope *scope;
    Symbol *sym;
    Symbol *next;

    if (!stack->top) {
        return;
    }

    scope = stack->top;
    stack->top = scope->next;
    sym = scope->symbols;
    while (sym) {
        next = sym->next;
        symbol_free(sym);
        sym = next;
    }
    free(scope);
}

void symtab_free(SymbolTableStack *stack) {
    while (stack->top) {
        symtab_pop_scope(stack);
    }
}

Symbol *symtab_lookup_current(SymbolTableStack *stack, const char *name) {
    Symbol *sym;

    if (!stack->top) {
        return NULL;
    }
    sym = stack->top->symbols;
    while (sym) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
        sym = sym->next;
    }
    return NULL;
}

Symbol *symtab_lookup(SymbolTableStack *stack, const char *name) {
    Scope *scope = stack->top;
    Symbol *sym;

    while (scope) {
        sym = scope->symbols;
        while (sym) {
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
            sym = sym->next;
        }
        scope = scope->next;
    }
    return NULL;
}

static Symbol *declare_symbol(SymbolTableStack *stack, const char *name, SymbolKind kind, Gv1Type type, int is_vector, int vector_size, const char *label) {
    Symbol *sym;

    if (!stack->top || symtab_lookup_current(stack, name)) {
        return NULL;
    }
    sym = (Symbol *)calloc(1, sizeof(Symbol));
    if (!sym) {
        return NULL;
    }
    sym->name = dup_str(name);
    sym->kind = kind;
    sym->type = type;
    sym->is_vector = is_vector;
    sym->vector_size = vector_size;
    sym->label = dup_str(label);
    if (!sym->name || (label && !sym->label)) {
        symbol_free(sym);
        return NULL;
    }
    sym->next = stack->top->symbols;
    stack->top->symbols = sym;
    return sym;
}

Symbol *symtab_declare_var(SymbolTableStack *stack, const char *name, Gv1Type type, int is_vector, int vector_size, const char *label) {
    return declare_symbol(stack, name, SYM_VAR, type, is_vector, vector_size, label);
}

Symbol *symtab_declare_param(SymbolTableStack *stack, const char *name, Gv1Type type, int is_vector, int vector_size) {
    return declare_symbol(stack, name, SYM_PARAM, type, is_vector, vector_size, NULL);
}

Symbol *symtab_declare_func(SymbolTableStack *stack, const char *name, Gv1Type type, int param_count, ParamInfo *params) {
    Symbol *sym = declare_symbol(stack, name, SYM_FUNC, type, 0, -1, NULL);
    if (!sym) {
        return NULL;
    }
    sym->param_count = param_count;
    sym->params = params;
    return sym;
}
