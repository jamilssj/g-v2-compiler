#include "ast.h"

#include <stdlib.h>
#include <string.h>
/* Aloca memória e duplica a string fornecida para garantir a preservação de lexemas na AST. */
/* Retorna char* para o ponteiro da nova cópia; o parâmetro s é a string de origem vinda do analisador. */
static char *dup_str(const char *s) {
    size_t len;                                      // Declara variável para o comprimento da string
    char *copy;                                      // Declara o ponteiro para o novo bloco de memória

    if (!s) {                                        // Verifica se a string de entrada é nula
        return NULL;                                 // Retorna nulo para evitar erro de processamento
    }
                                                /* strlen(s) calcula o tamanho da string s; len armazena o resultado. */
    len = strlen(s);                                 
                                                /* malloc aloca memória dinâmica; o +1 reserva espaço para o terminador nulo '\0'. */
    copy = (char *)malloc(len + 1);                  
    if (!copy) {                                     // Valida se o sistema conseguiu alocar a memória
        return NULL;                                 // Retorna nulo em caso de falha de recursos
    }
                                                /* memcpy copia len+1 bytes da origem s para o destino copy. */
    memcpy(copy, s, len + 1);                        
    return copy;                                     // Retorna o endereço da nova string duplicada
}

/* Cria e inicializa um novo nó da AST configurando seu tipo, linha de origem e lexema. */
/* Retorna ASTNode* como o novo objeto alocado; kind define a categoria do nó, line rastreia a posição no código e lexeme armazena o texto. */
ASTNode *ast_new(ASTKind kind, int line, const char *lexeme) {
                                                    /* calloc aloca memória para o nó e inicializa todos os seus bits com zero. */
    ASTNode *node = (ASTNode *)calloc(1, sizeof(ASTNode)); 
    if (!node) {                                     // Checa se a alocação do nó foi bem-sucedida
        return NULL;                                 // Aborta a criação se não houver memória livre
    }
    node->kind = kind;                               // Atribui a categoria gramatical ao nó
    node->line = line;                               // Guarda a linha para futuras mensagens de erro
    node->type = TYPE_INVALID;                       // Define o tipo semântico inicial como inválido
    node->lexeme = dup_str(lexeme);                  // Armazena uma cópia local do lexema identificado
    node->is_vector = 0;                             // Inicializa a flag de vetor como falso (escalar)
    node->vector_size = -1;                          // Define tamanho de vetor inexistente por padrão
    return node;                                     // Retorna o ponteiro para o nó AST configurado
}

/* Cria um nó da AST especializado para servir como container ou cabeçalho de uma lista de elementos. */
/* Retorna ASTNode* para o nó de lista; o parâmetro kind especifica a natureza da lista (ex: declarações ou comandos). */
ASTNode *ast_new_list(ASTKind kind) {
                                                    /* Reutiliza ast_new enviando linha 0 e lexema nulo para criar um nó container. */
    return ast_new(kind, 0, NULL);                   
}

/* Adiciona um nó filho a um nó pai, gerenciando dinamicamente a expansão do array de ponteiros de filhos. */
/* Tipo void pois modifica o pai in-place; parent é o nó que recebe o descendente e child é o nó a ser anexado. */
void ast_add_child(ASTNode *parent, ASTNode *child) {
    ASTNode **new_children;                          // Ponteiro auxiliar para expansão segura da lista
    size_t new_capacity;                             // Variável para cálculo da nova capacidade alocada

    if (!parent || !child) {                         // Verifica a integridade dos ponteiros recebidos
        return;                                      // Sai da função se algum dos nós for inválido
    }

                                                    /* Compara se o total de filhos atingiu o limite de capacidade atual do array. */
    if (parent->child_count == parent->child_capacity) {
                                                        /* Calcula nova capacidade: dobra a atual ou inicia com 4 se for a primeira. */
        new_capacity = parent->child_capacity ? parent->child_capacity * 2 : 4;
                                                        /* realloc redimensiona o bloco de memória do array de ponteiros de filhos. */
        new_children = (ASTNode **)realloc(parent->children, new_capacity * sizeof(ASTNode *));
        if (!new_children) {                         // Valida se o redimensionamento foi aceito pelo sistema
            return;                                  // Interrompe a adição se houver falha de memória
        }
        parent->children = new_children;             // Atualiza o endereço do array de filhos no pai
        parent->child_capacity = new_capacity;       // Registra o novo limite de armazenamento de filhos
    }

                                                    /* Anexa o ponteiro do filho na posição correta e incrementa o contador total. */
    parent->children[parent->child_count++] = child; 
}

/* Desaloca recursivamente a memória de um nó, de seus lexemas e de toda a sua árvore de filhos. */
/* Tipo void pois realiza apenas a limpeza da memória dinâmica; node é a raiz da subárvore a ser destruída. */
void ast_free(ASTNode *node) {
    size_t i;                                        // Variável de controle para percorrer os filhos

    if (!node) {                                     // Caso base: verifica se o nó atual existe
        return;                                      // Encerra a recursão se o nó for nulo
    }
                                                    /* Percorre o array de filhos para limpar a árvore de baixo para cima. */
    for (i = 0; i < node->child_count; ++i) {
        ast_free(node->children[i]);                 // Chamada recursiva para destruir cada subárvore filha
    }
                                                    /* free libera o bloco de memória previamente alocado no heap. */
    free(node->children);                            // Libera o array dinâmico de ponteiros de filhos
    free(node->lexeme);                              // Libera a memória da string do lexema duplicado
    free(node);                                      // Libera a estrutura principal do nó AST atual
}
