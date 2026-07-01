#ifndef GV_SYMBOL_TABLE_H                          // Proteção contra inclusões múltiplas
#define GV_SYMBOL_TABLE_H                           // Define o símbolo deste cabeçalho

#include "ast.h"                                    // Necessário para definições de tipos (Gv1Type)

typedef enum {                                      // Categorias de símbolos na tabela
    SYM_VAR = 0,                                    // Identificador de variável comum
    SYM_PARAM,                                      // Identificador de parâmetro de função
    SYM_FUNC                                        // Identificador de sub-rotina ou função
} SymbolKind;                                       // Nome do tipo da enumeração

typedef struct ParamInfo {                          // Informações detalhadas de parâmetros
    char *name;                                     // Nome do parâmetro formal
    Gv1Type type;                                   // Tipo de dado (int ou car)
    int is_vector;                                  // Flag: 1 para vetor, 0 para escalar
    int vector_size;                                // Tamanho definido para o vetor
    struct ParamInfo *next;                         // Próximo parâmetro na lista ligada
} ParamInfo;                                        // Nome da estrutura de parâmetros

typedef struct Symbol {                             // Registro principal de um identificador
    char *name;                                     // String com o nome (lexema) do símbolo
    SymbolKind kind;                                // Categoria: variável, parâmetro ou função
    Gv1Type type;                                   // Tipo básico de dado associado
    int is_vector;                                  // Indica se o identificador é um vetor
    int vector_size;                                // Dimensão física do vetor em memória
    int param_count;                                // Total de argumentos (apenas para funções)
    ParamInfo *params;                              // Cabeça da lista de parâmetros (funções)
    char *label;                                    // Rótulo assembly MIPS para acesso global
    struct Symbol *next;                            // Próximo símbolo na lista do escopo
} Symbol;                                           // Nome da estrutura do símbolo

typedef struct Scope {                              // Representação de um nível de escopo
    Symbol *symbols;                                // Lista de símbolos declarados no nível
    struct Scope *next;                             // Ponteiro para o escopo pai (anterior)
} Scope;                                            // Nome da estrutura de escopo

typedef struct SymbolTableStack {                   // Estrutura para gerenciamento da pilha
    Scope *top;                                     // Ponteiro para o escopo atual (topo)
} SymbolTableStack;                                 // Tipo para manipulação global da tabela
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
