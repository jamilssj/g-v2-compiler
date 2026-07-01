#ifndef GV_AST_H                                     // Proteção contra inclusões múltiplas
#define GV_AST_H                                     // Define o símbolo do cabeçalho

#include <stddef.h>                                  // Inclusão para tipos básicos como size_t

typedef enum {                                       // Definição dos tipos de dados da linguagem
    TYPE_INVALID = 0,                                // Representa um tipo inválido ou erro
    TYPE_INT,                                        // Tipo de dado numérico inteiro
    TYPE_CAR                                         // Tipo de dado caractere
} Gv1Type;                                           // Nome da enumeração de tipos básicos

typedef enum {                                       // Definição de todas as categorias de nós da AST
    AST_PROGRAM = 0,                                 // Nó raiz que representa o programa completo
    AST_GLOBAL_SECTION,                              // Container para declarações globais
    AST_FUNCTION_SECTION,                            // Container para a lista de funções
    AST_FUNCTION_DECL,                               // Declaração individual de uma função
    AST_PARAM_LIST,                                  // Lista de parâmetros formais
    AST_PARAM,                                       // Representa um parâmetro individual
    AST_BLOCK,                                       // Bloco de código delimitado
    AST_DECL_LIST,                                   // Lista de declarações de variáveis
    AST_DECL_ITEM,                                   // Item de declaração (variável ou vetor)
    AST_CMD_LIST,                                    // Lista sequencial de comandos
    AST_EXPR_LIST,                                   // Lista de expressões (argumentos)
    AST_EMPTY_STMT,                                  // Comando vazio (apenas ponto e vírgula)
    AST_EXPR_STMT,                                   // Expressão usada como comando
    AST_READ,                                        // Comando de entrada de dados (leia)
    AST_WRITE_EXPR,                                  // Comando de saída de expressão (escreva)
    AST_WRITE_STR,                                   // Comando de saída de string literal
    AST_NEWLINE,                                     // Comando para nova linha (novalinha)
    AST_RETURN,                                      // Comando de retorno de função
    AST_IF,                                          // Estrutura condicional simples (se)
    AST_IF_ELSE,                                     // Estrutura condicional completa (se-senao)
    AST_WHILE,                                       // Estrutura de repetição (enquanto)
    AST_ASSIGN,                                      // Operação de atribuição de valor
    AST_OR,                                          // Operador lógico binário OU
    AST_AND,                                         // Operador lógico binário E
    AST_EQ,                                          // Operador de igualdade relacional
    AST_NE,                                          // Operador de diferença relacional
    AST_LT,                                          // Operador menor que
    AST_GT,                                          // Operador maior que
    AST_GE,                                          // Operador maior ou igual
    AST_LE,                                          // Operador menor ou igual
    AST_ADD,                                         // Operação aritmética de soma
    AST_SUB,                                         // Operação aritmética de subtração
    AST_MUL,                                         // Operação aritmética de multiplicação
    AST_DIV,                                         // Operação aritmética de divisão
    AST_NEG,                                         // Operação unária de inversão de sinal
    AST_NOT,                                         // Operação unária de negação lógica
    AST_IDENTIFIER,                                  // Nó de identificador (nome de variável)
    AST_INT_LITERAL,                                 // Valor constante inteiro
    AST_CHAR_LITERAL,                                // Valor constante caractere
    AST_STRING_LITERAL,                              // Cadeia de caracteres literal
    AST_FUNC_CALL,                                   // Chamada de sub-rotina/função
    AST_ARRAY_ACCESS                                 // Acesso a elemento de vetor por índice
} ASTKind;                                           // Nome da enumeração de tipos de nós

typedef struct ASTNode {                             // Estrutura principal de um nó da árvore
    ASTKind kind;                                    // Categoria específica do nó atual
    int line;                                        // Linha correspondente no código-fonte
    char *lexeme;                                    // Texto original (nome ou valor literal)
    Gv1Type type;                                    // Tipo de dado (atribuído no semântico)
    int is_vector;                                   // Flag: 1 se for vetor, 0 se escalar
    int vector_size;                                 // Tamanho definido para vetores
    struct ASTNode **children;                       // Array de ponteiros para nós filhos
    size_t child_count;                              // Número de filhos anexados no momento
    size_t child_capacity;                           // Capacidade de memória alocada para filhos
} ASTNode;                                           // Nome da estrutura do nó da AST

ASTNode *ast_new(ASTKind kind, int line, const char *lexeme);
ASTNode *ast_new_list(ASTKind kind);
void ast_add_child(ASTNode *parent, ASTNode *child);
void ast_free(ASTNode *node);

#endif
