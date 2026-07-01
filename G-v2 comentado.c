/*---------------------g-v2.l--------------------*/

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "g-v2.tab.h" // Inclui definições de tokens e yylval geradas pelo Bison .

#define YY_USER_ACTION do { \
    yylloc.first_line = yylineno; \    // Sincroniza a linha inicial do token para rastreio de erros .
    yylloc.last_line = yylineno; \     // Define a linha final do token com base no contador do Flex .
    yylloc.first_column = 1; \         // Inicializa a coluna inicial (fixada em 1 neste projeto) .
    yylloc.last_column = 1; \          // Inicializa a coluna final para a estrutura de localização .
} while (0);                           // Macro executada automaticamente antes de cada ação do analisador .

static char *str_buffer = NULL;       // Ponteiro para o buffer que acumula caracteres de strings .
static size_t str_len = 0;            // Armazena o tamanho atual da string dentro do buffer .
static size_t str_cap = 0;            // Controla a capacidade total de memória alocada para o buffer 
.
/* Aloca memória e duplica a string fornecida, encerrando o programa em caso de falha. */
/* Retorna char* para entregar o ponteiro da nova cópia; o parâmetro const char *s é a string original a ser clonada. */
static char *dup_text(const char *s) {
    size_t len = strlen(s);                          // Calcula o comprimento da string original
    char *copy = (char *)malloc(len + 1);            // Aloca espaço para a cópia (incluindo o '\0')
    if (!copy) {                                     // Verifica se a alocação de memória falhou
        fprintf(stderr, "ERRO: FALHA DE MEMORIA\n"); // Exibe mensagem de erro na saída de erro padrão
        exit(1);                                     // Finaliza o compilador por erro fatal de sistema
    }
    /* Copia o conteúdo de s para o novo bloco; o +1 garante a cópia do terminador nulo. */
    memcpy(copy, s, len + 1);
    return copy;                                     // Retorna o endereço da nova string duplicada
}

/* Imprime uma mensagem de erro léxico com o número da linha atual e encerra a compilação. */
/* Tipo void pois a função interrompe a execução via exit; o parâmetro const char *msg é a descrição do erro. */
static void lexer_error(const char *msg) {
    /* Exibe o erro formatado usando yylineno, que rastreia as linhas lidas pelo Flex. */
    fprintf(stderr, "ERRO: %s LINHA %d\n", msg, yylineno);
    exit(1);                                         // Interrompe imediatamente o processo de compilação
}

/* Reinicializa o buffer de strings temporário para o processamento de uma nova cadeia de caracteres. */
/* Tipo void pois apenas redefine variáveis globais internas; não possui parâmetros pois atua sobre o buffer global. */
static void str_reset(void) {
    str_len = 0;                                     // Reseta o contador de tamanho atual da string
    if (str_buffer) {                                // Verifica se o buffer global já está alocado
        str_buffer = '\0';                        // Invalida a string anterior marcando o início como nulo
    }
}

/* Adiciona o fragmento de texto fornecido ao final do buffer de strings atual, gerenciando o redimensionamento. */
/* Tipo void pois atualiza o estado do buffer interno; o parâmetro const char *text é o texto a ser anexado. */
static void str_append(const char *text) {
    size_t n = strlen(text);                         // Obtém o tamanho do novo fragmento a ser anexado
    char *new_buf;                                   // Ponteiro auxiliar para redimensionamento seguro
    size_t new_cap;                                  // Variável para cálculo da nova capacidade de memória

    if (str_len + n + 1 > str_cap) {                 // Checa se o conteúdo excede a capacidade atual
        new_cap = str_cap ? str_cap : 64;            // Define capacidade inicial de 64 ou usa a existente
        while (new_cap < str_len + n + 1) {          // Garante que a nova capacidade comporte o texto
            new_cap *= 2;                            // Dobra o tamanho sucessivamente para eficiência
        }
                                                    /* Realoca o bloco de memória atual para o novo tamanho calculado. */
        new_buf = (char *)realloc(str_buffer, new_cap);
        if (!new_buf) {                              // Valida se o sistema conseguiu prover mais memória
            fprintf(stderr, "ERRO: FALHA DE MEMORIA\n");
            exit(1);                                 // Encerra por falha crítica de recursos
        }
        str_buffer = new_buf;                        // Atualiza o ponteiro global para o novo endereço
        str_cap = new_cap;                           // Registra o novo limite de armazenamento do buffer
    }
                                                /* Concatena o novo texto na posição correta e atualiza o marcador de fim de string. */
    memcpy(str_buffer + str_len, text, n + 1);
    str_len += n;                                    // Incrementa o tamanho ocupado no buffer global
}
%}

%option noyywrap nodefault yylineno                   // Opções: desativa yywrap, avisa falta de regras e conta linhas
%x COMENTARIO CADEIA                                  // Define condições de partida exclusivas para comentários e strings

ID      [A-Za-z_][A-Za-z0-9_]*                        // Expressão regular para nomes de identificadores
INTNUM  [1-9]+                                        // Expressão regular para literais numéricos inteiros
CARLIT  \'([^\\\'\n]|\\[ntr0\\\'])\'                  // Expressão regular para literais de caractere (com suporte a escape)

%%

"/*"                    { BEGIN(COMENTARIO); }        // Identifica o início de um comentário e muda o estado do scanner
<COMENTARIO>[^*\n]+     ;                             // No contexto de comentário, consome texto (exceto '*' e '\n')
<COMENTARIO>"*"+[^*/\n] ;                             // Consome asteriscos internos que não finalizam o bloco
<COMENTARIO>\n          ;                             // Permite que o bloco de comentário ocupe múltiplas linhas
<COMENTARIO>"*"+"/"     { BEGIN(INITIAL); }           // Reconhece o fim do comentário e retorna ao estado de análise normal
<COMENTARIO><<EOF>>     { lexer_error("COMENTARIO NAO TERMINA"); } // Reporta erro se o arquivo acabar com o comentário aberto

"\""                    { str_reset(); str_append("\""); BEGIN(CADEIA); } // Limpa buffer e inicia tratamento de string literal
<CADEIA>[^\\\"\n]+      { str_append(yytext); }       // Acumula caracteres normais da string no buffer temporário 
<CADEIA>\\[^\n]         { str_append(yytext); }       // Adiciona sequências de escape identificadas ao buffer
                                                     /* Finaliza a string, salva o lexema completo para a AST e retorna ao estado inicial de tokens. */
<CADEIA>"\""            {
                            str_append("\"");
                            yylval.lexeme = dup_text(str_buffer);
                            BEGIN(INITIAL);
                            return CADEIACARACTERES;
                         }

<CADEIA>\n              { lexer_error("CADEIA DE CARACTERES OCUPA MAIS DE UMA LINHA"); } // Erro para quebras de linha em strings
<CADEIA><<EOF>>         { lexer_error("CADEIA DE CARACTERES OCUPA MAIS DE UMA LINHA"); } // Erro para strings não fechadas no fim do arquivo

"global"                { return GLOBAL; }            // Retorna o token para a seção de variáveis globais
"funcao"                { return FUNCAO; }            // Retorna o token para a seção de declaração de funções
"principal"             { return PRINCIPAL; }         // Retorna o token que identifica o bloco principal do programa
"int"                   { return INT; }               // Retorna o token correspondente ao tipo de dado inteiro
"car"                   { return CAR; }               // Retorna o token correspondente ao tipo de dado caractere
"retorne"               { return RETORNE; }           // Retorna o token para o comando de retorno de função
"leia"                  { return LEIA; }              // Retorna o token para a instrução de entrada de dados
"escreva"               { return ESCREVA; }           // Retorna o token para a instrução de saída de dados
"novalinha"             { return NOVALINHA; }         // Retorna o token para o comando de quebra de linha na saída
"se"                    { return SE; }                // Retorna o token correspondente à condicional 'se'
"entao"                 { return ENTAO; }             // Retorna o token para início do bloco de execução do 'se'
"senao"                 { return SENAO; }             // Retorna o token para o bloco alternativo da condicional
"fimse"                 { return FIMSE; }             // Retorna o token que encerra a estrutura condicional
"enquanto"              { return ENQUANTO; }          // Retorna o token correspondente ao laço de repetição

"||"                    { return OU; }                // Identifica e retorna o token do operador lógico OU
"&"                     { return E; }                 // Identifica e retorna o token do operador lógico E
"=="                    { return IGUAL; }             // Identifica e retorna o token de comparação de igualdade
"!="                    { return DIFERENTE; }         // Identifica e retorna o token de comparação de diferença
">="                    { return MAIORIGUAL; }        // Identifica e retorna o token do operador maior ou igual
"<="                    { return MENORIGUAL; }        // Identifica e retorna o token do operador menor ou igual

{CARLIT}                { yylval.lexeme = dup_text(yytext); return CARCONST; }    // Captura constante caractere e retorna seu token
{INTNUM}                { yylval.lexeme = dup_text(yytext); return INTCONST; }    // Captura número inteiro e retorna seu token
{ID}                    { yylval.lexeme = dup_text(yytext); return IDENTIFICADOR; } // Captura identificador e retorna seu token

[ \t\r\n]+              ;                             // Descarta espaços, tabulações e quebras de linha fora de tokens

"{"                     { return '{'; }               // Delimitador para o início de um bloco de comandos
"}"                     { return '}'; }               // Delimitador para o fim de um bloco de comandos
"["                     { return '['; }               // Delimitador para seções globais, de funções ou índices
"]"                     { return ']'; }               // Delimitador para o fechamento de seções ou índices
"("                     { return '('; }               // Início de listas de parâmetros ou precedência em expressões
")"                     { return ')'; }               // Fim de listas de parâmetros ou precedência em expressões
":"                     { return ':'; }               // Caractere usado para separação em declarações de tipo
";"                     { return ';'; }               // Caractere terminador de instruções na gramática
","                     { return ','; }               // Separador de itens em listas de variáveis ou argumentos
"="                     { return '='; }               // Operador para atribuição de valores
"<"                     { return '<'; }               // Operador relacional de comparação 'menor que'
">"                     { return '>'; }               // Operador relacional de comparação 'maior que'
"+"                     { return '+'; }               // Operador aritmético para adição
"-"                     { return '-'; }               // Operador aritmético para subtração ou sinal unário
"*"                     { return '*'; }               // Operador aritmético para multiplicação
"/"                     { return '/'; }               // Operador aritmético para divisão
"!"                     { return '!'; }               // Operador unário de negação lógica

<<EOF>>                 { return 0; }                 // Sinaliza o fim do arquivo de entrada para o Bison

.                       { lexer_error("CARACTERE INVALIDO"); } // Regra para capturar e reportar qualquer símbolo não reconhecido

%%

/*---------------------ast.h--------------------*/

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

/*---------------------ast.c--------------------*/

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
/*---------------------g-v2.y--------------------*/

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"                                     // Inclui a definição da estrutura da árvore sintática
#include "semantic.h"                                 // Inclui as funções de verificação de tipos e escopo
#include "codegen.h"                                  // Inclui as funções para geração de código MIPS assembly

extern int yylex(void);                               // Declaração da função do analisador léxico (Flex)
extern int yylineno;                                  // Variável global que rastreia a linha atual no código-fonte
extern char *yytext;                                  // Ponteiro para o texto do último token lido
extern FILE *yyin;                                    // Ponteiro para o arquivo de entrada que está sendo processado


static ASTNode *g_root = NULL;                       // Ponteiro global que armazena a raiz da árvore sintática

/* Cria um nó da AST para operações binárias, adicionando os operandos esquerdo e direito como filhos. */
/* Retorna ASTNode* para integrar a árvore; kind define o operador, left/right os operandos e line a linha no código. [1, 2] */
static ASTNode *make_binary(ASTKind kind, ASTNode *left, ASTNode *right, int line) {
                                                    /* ast_new aloca nó da categoria 'kind' na linha 'line' sem texto (NULL). */
    ASTNode *node = ast_new(kind, line, NULL);       
    if (!node) {                                     // Verifica se a alocação do novo nó falhou
        return NULL;                                 // Retorna nulo interrompendo a construção
    }
                                                    /* ast_add_child anexa o nó 'left' no array de filhos do pai 'node'. */
    ast_add_child(node, left);                       
                                                    /* ast_add_child anexa o nó 'right' como segundo filho do pai 'node'. */
    ast_add_child(node, right);                      
    return node;                                     // Retorna o ponteiro do nó binário pronto
}

/* Cria um nó da AST para operações unárias, adicionando o operando fornecido como seu único filho. */
/* Retorna ASTNode* para o nó da expressão; kind indica o tipo de operação, child é o operando e line rastreia a origem. [1, 2] */
static ASTNode *make_unary(ASTKind kind, ASTNode *child, int line) {
                                                    /* ast_new cria nó para operador unário 'kind' na linha 'line'. */
    ASTNode *node = ast_new(kind, line, NULL);       
    if (!node) {                                     // Valida se houve sucesso na criação do nó
        return NULL;                                 // Aborta retornando nulo em caso de erro
    }
                                                    /* ast_add_child vincula o operando único 'child' como filho de 'node'. */
    ast_add_child(node, child);                      
    return node;                                     // Entrega o nó unário para compor a árvore
}

/* Concatena duas listas da AST, movendo todos os filhos do segundo nó para o final do primeiro. */
/* Retorna ASTNode* representando a lista unificada; a e b são os nós de lista (cabeçalhos) a serem mesclados. [1, 3] */
static ASTNode *merge_lists(ASTNode *a, ASTNode *b) {
    size_t i;                                        // Variável de iteração para percorrer os filhos

    if (!a) {                                        // Verifica se a lista de destino é inexistente
        return b;                                    // Retorna a lista 'b' como a lista principal
    }
    if (!b) {                                        // Verifica se a lista de origem está vazia
        return a;                                    // Retorna a lista 'a' sem modificações
    }
                                                    /* Itera sobre todos os nós filhos presentes no cabeçalho da lista 'b'. */
    for (i = 0; i < b->child_count; ++i) {           
                                                        /* ast_add_child insere o filho de 'b' no final do array de filhos de 'a'. */
        ast_add_child(a, b->children[i]);            
    }
    b->child_count = 0;                              // Zera contador de 'b' para não afetar filhos movidos
                                                    /* ast_free libera a memória do nó 'b' e seu array (já vazio de filhos válidos). */
    ast_free(b);                                     
    return a;                                        // Retorna a lista 'a' contendo todos os elementos fundidos
}

/* Cria um nó para declaração de variável ou vetor, configurando atributos de tamanho e liberando a string original. */
/* Retorna ASTNode* do tipo item de declaração; line indica a posição, name o identificador, e is_vector/vector_size as propriedades físicas. [1, 3] */
static ASTNode *make_decl_item(int line, char *name, int is_vector, int vector_size) {
                                                    /* ast_new instancia nó de declaração usando o nome 'name' vindo do lexer. */
    ASTNode *node = ast_new(AST_DECL_ITEM, line, name); 
    node->is_vector = is_vector;                     // Define flag: 1 para vetor, 0 para variável escalar
    node->vector_size = vector_size;                 // Armazena a capacidade do vetor (ou -1 se escalar)
    free(name);                                      // Libera lexema original após cópia interna no nó
    return node;                                     // Retorna o nó de item de declaração configurado
}

/* Cria um nó para parâmetro de função, registrando se é um vetor e limpando a memória do lexema temporário. */
/* Retorna ASTNode* do tipo parâmetro; line situa o erro, name identifica o parâmetro e is_vector define a forma de passagem. [1, 3] */
static ASTNode *make_param_item(int line, char *name, int is_vector) {
                                                    /* ast_new aloca nó de parâmetro formal com identificador 'name'. */
    ASTNode *node = ast_new(AST_PARAM, line, name);  
    node->is_vector = is_vector;                     // Indica se o parâmetro é um vetor (passagem por referência)
    node->vector_size = -1;                          // Define tamanho como indefinido para parâmetros de vetor
    free(name);                                      // Desaloca string temporária do analisador léxico
    return node;                                     // Retorna o nó para compor a lista de parâmetros
}

/* Cria um nó de lista vazio destinado a agrupar expressões, como em argumentos de chamadas de função. */
/* Retorna ASTNode* como container de lista; não possui parâmetros por ser uma inicialização de cabeçalho genérico. [1, 3] */
static ASTNode *make_expr_list(void) {
                                                    /* ast_new_list cria nó container de categoria AST_EXPR_LIST via ast_new. */
    return ast_new_list(AST_EXPR_LIST);              
}
/* Reporta erros sintáticos encontrados pelo Bison, exibindo a mensagem "ERRO SINTATICO" e a linha correspondente. */
/* Tipo void pois apenas emite o erro no stderr; o parâmetro const char *s contém a mensagem de erro gerada pelo parser. [1, 4] */
void yyerror(const char *s);
%}

%code requires {
#include "ast.h"                                     // Garante que o arquivo gerado conheça o tipo ASTNode
}

%define parse.error verbose                          // Ativa mensagens de erro sintático mais detalhadas
%locations                                            // Habilita o rastreio de localização (linha/coluna) dos tokens

%union {                                              // Define os tipos de valores que os símbolos podem assumir
    ASTNode *node;                                    // Valor para nós da Árvore Sintática Abstrata
    char *lexeme;                                     // Valor para strings de identificadores e literais
    int typev;                                        // Valor para tipos básicos (int ou car)
}

%start programa                                       // Define o símbolo não-terminal raiz da gramática

                                                    /* Define os símbolos terminais (tokens) para as palavras-reservadas e comandos de controle da linguagem G-V2. */
%token GLOBAL FUNCAO PRINCIPAL INT CAR RETORNE LEIA ESCREVA NOVALINHA SE ENTAO SENAO FIMSE ENQUANTO
%token OU E IGUAL DIFERENTE MAIORIGUAL MENORIGUAL    // Tokens para operadores lógicos e relacionais
%token <lexeme> IDENTIFICADOR CADEIACARACTERES CARCONST INTCONST // Tokens que carregam o texto do lexema

                                                    /* Associa símbolos não-terminais de alto nível (programa, seções e blocos) ao tipo de dado 'node' (ASTNode*) da união. */
%type <node> programa opt_global opt_func global_section func_section func_list func_decl principal_block bloco
                                                    /* Define que os não-terminais de listas de declarações, parâmetros e comandos retornam ponteiros para nós da AST. */
%type <node> decl_list decl_line var_list param_list_opt param_list param_item cmd_list comando expr lvalue
                                                    /* Especifica que os resultados de todas as sub-expressões (aritméticas e lógicas) são armazenados como nós da árvore. */
%type <node> or_expr and_expr eq_expr desig_expr add_expr mul_expr un_expr prim_expr
%type <node> list_expr arg_list_opt                    // Define que estas regras retornam ponteiros para nós AST
%type <typev> tipo                                    // Define que esta regra retorna um valor inteiro de tipo

%%

/* Regra principal que define a estrutura hierárquica de um programa na linguagem G-V2. */
programa
    : opt_global opt_func PRINCIPAL principal_block {
        $$ = ast_new(AST_PROGRAM, @3.first_line, NULL);     // Cria o nó raiz da árvore sintática
        if ($1) {                                           // Verifica se a seção global foi definida
            ast_add_child($$, $1);                          // Anexa as variáveis globais ao programa
        }
        if ($2) {                                           // Verifica se existem funções declaradas
            ast_add_child($$, $2);                          // Anexa a lista de funções ao programa
        }
        ast_add_child($$, $4);                              // Anexa o bloco principal obrigatório
        g_root = $$;                                        // Armazena a raiz completa na variável global
      }
    ;

opt_global
    : /* empty */ { $$ = NULL; }                            // Seção global é opcional; retorna nulo se vazia
    | global_section { $$ = $1; }                           // Repassa o nó da seção global processada
    ;

opt_func
    : /* empty */ { $$ = NULL; }                            // Seção de funções é opcional; retorna nulo se vazia
    | func_section { $$ = $1; }                             // Repassa o nó da seção de funções processada
    ;

global_section
    : GLOBAL '[' decl_list ']' {                            // Define o bloco de variáveis globais entre colchetes
        $$ = ast_new(AST_GLOBAL_SECTION, @1.first_line, NULL); // Cria nó container para escopo global
        ast_add_child($$, $3);                              // Anexa a lista de declarações ao nó global
      }
    ;

func_section
    : FUNCAO '[' func_list ']' {                            // Define a lista de funções do programa
        $$ = $3;                                            // Repassa a lista de funções acumulada
      }
    ;

func_list
    : func_decl {                                           // Caso base: primeira função da lista
        $$ = ast_new_list(AST_FUNCTION_SECTION);            // Inicializa o nó de lista para as funções
        ast_add_child($$, $1);                              // Adiciona a primeira declaração à lista
      }
    | func_list func_decl {                                 // Caso recursivo para múltiplas funções
        ast_add_child($1, $2);                              // Anexa a nova função à lista existente
        $$ = $1;                                            // Mantém o ponteiro da lista atualizada
      }
    ;

/* Define a assinatura de uma função contendo identificador, parâmetros, tipo e corpo. */
func_decl
    : IDENTIFICADOR '(' param_list_opt ')' ':' tipo bloco {
        $$ = ast_new(AST_FUNCTION_DECL, @1.first_line, $1); // Cria nó de declaração com o nome da função
        $$->type = (Gv1Type)$6;                             // Define o tipo de retorno da sub-rotina
        ast_add_child($$, $3);                              // Anexa a lista de parâmetros formais
        ast_add_child($$, $7);                              // Anexa o bloco de comandos (corpo da função)
        free($1);                                           // Libera a memória temporária do identificador
      }
    ;

principal_block
    : bloco { $$ = $1; }                                    // O bloco principal segue a mesma regra de blocos
    ;

bloco
    : '[' decl_list ']' '{' cmd_list '}' {                  // Bloco completo com variáveis locais e comandos
        $$ = ast_new(AST_BLOCK, @1.first_line, NULL);       // Cria nó representativo de um novo escopo
        ast_add_child($$, $2);                              // Anexa a lista de declarações locais
        ast_add_child($$, $5);                              // Anexa a sequência de comandos do bloco
      }
    | '{' cmd_list '}' {                                    // Bloco simplificado contendo apenas comandos
        $$ = ast_new(AST_BLOCK, @1.first_line, NULL);       // Cria o nó de bloco para a árvore
        ast_add_child($$, $2);                              // Anexa a lista de comandos diretamente
      }
    ;

decl_list
    : decl_line { $$ = $1; }                                // Repassa a primeira linha de declarações
    | decl_list decl_line { $$ = merge_lists($1, $2); }     // Funde múltiplas linhas em uma única lista
    ;

decl_line
    : var_list ':' tipo ';' {                               // Associa um tipo a uma lista de variáveis
        size_t i;                                           // Variável de controle para o laço de iteração
                                                        /* Percorre os filhos para atribuir o tipo vindo da direita para todos os itens da linha. */
        for (i = 0; i < $1->child_count; ++i) {
            $1->children[i]->type = (Gv1Type)$3;            // Define o tipo de cada variável ou vetor
        }
        $$ = $1;                                            // Retorna a lista de itens agora tipados
      }
    ;
var_list
    : IDENTIFICADOR {                                // Caso de identificador simples (escalar)
                                                        /* Cria o nó para um item de declaração escalar sem tamanho definido. */
        ASTNode *item = make_decl_item(@1.first_line, $1, 0, -1);
        $$ = ast_new_list(AST_DECL_LIST);            // Inicializa a lista de declarações
        ast_add_child($$, item);                     // Adiciona a variável escalar à lista
      }
    | IDENTIFICADOR '[' INTCONST ']' {               // Caso de declaração de vetor com tamanho
                                                        /* Cria o nó para item de vetor, convertendo o texto do tamanho para inteiro. */
        ASTNode *item = make_decl_item(@1.first_line, $1, 1, atoi($3));
        $$ = ast_new_list(AST_DECL_LIST);            // Inicializa a lista de declarações
        ast_add_child($$, item);                     // Adiciona o vetor à lista
        free($3);                                    // Libera a string da constante inteira
      }
    | var_list ',' IDENTIFICADOR {                   // Adição de variável escalar à lista existente
                                                        /* Anexa uma nova variável simples ao final da lista de itens atual. */
        ast_add_child($1, make_decl_item(@3.first_line, $3, 0, -1));
        $$ = $1;                                     // Retorna a lista atualizada
      }
    | var_list ',' IDENTIFICADOR '[' INTCONST ']' {  // Adição de vetor à lista existente
                                                        /* Anexa um novo vetor com tamanho especificado à lista de itens atual. */
        ast_add_child($1, make_decl_item(@3.first_line, $3, 1, atoi($5)));
        $$ = $1;                                     // Retorna a lista atualizada
        free($5);                                    // Libera a memória do lexema do tamanho
      }
    ;

param_list_opt
    : /* empty */ { $$ = ast_new_list(AST_PARAM_LIST); } // Se vazio, cria uma lista de parâmetros sem filhos
    | param_list { $$ = $1; }                            // Repassa a lista de parâmetros preenchida
    ;

param_list
    : param_item {                                   // Primeiro parâmetro formal da função
        $$ = ast_new_list(AST_PARAM_LIST);           // Cria o nó container para os parâmetros
        ast_add_child($$, $1);                       // Insere o primeiro parâmetro na lista
      }
    | param_list ',' param_item {                    // Parâmetros subsequentes separados por vírgula
        ast_add_child($1, $3);                       // Adiciona o novo parâmetro à lista acumulada
        $$ = $1;                                     // Mantém a referência da lista
      }
    ;

param_item
    : IDENTIFICADOR ':' tipo {                       // Parâmetro do tipo escalar (ex: x: int)
        ASTNode *node = make_param_item(@1.first_line, $1, 0); // Cria nó de parâmetro escalar
        node->type = (Gv1Type)$3;                    // Define o tipo básico do parâmetro
        $$ = node;                                   // Retorna o nó do parâmetro configurado
      }
    | IDENTIFICADOR '[' ']' ':' tipo {               // Parâmetro do tipo vetor (ex: v[]: car)
        ASTNode *node = make_param_item(@1.first_line, $1, 1); // Cria nó de parâmetro de vetor
        node->type = (Gv1Type)$5;                    // Define o tipo dos elementos do vetor
        $$ = node;                                   // Retorna o nó do parâmetro de vetor
      }
    ;

tipo
    : INT { $$ = TYPE_INT; }                         // Mapeia a palavra-chave 'int' para o tipo interno inteiro
    | CAR { $$ = TYPE_CAR; }                         // Mapeia a palavra-chave 'car' para o tipo interno caractere
    ;

cmd_list
    : comando {                                      // Caso base: primeiro comando encontrado
        $$ = ast_new_list(AST_CMD_LIST);             // Cria o nó container para a sequência de comandos
        ast_add_child($$, $1);                       // Adiciona o comando inicial à lista
      }
    | cmd_list comando {                             // Caso recursivo: comandos adicionais
        ast_add_child($1, $2);                       // Concatena o novo comando à lista existente
        $$ = $1;                                     // Repassa a lista de comandos em crescimento
      }
    ;
comando
    : ';' { $$ = ast_new(AST_EMPTY_STMT, @1.first_line, NULL); }             // Comando vazio: cria nó para ponto e vírgula isolado
    | expr ';' {                                                            // Comando de expressão seguido de terminador
        $$ = ast_new(AST_EXPR_STMT, @1.first_line, NULL);                   // Cria o nó container para uma instrução de expressão
        ast_add_child($$, $1);                                              // Adiciona a subárvore da expressão como filha do comando
      }
    | RETORNE expr ';' {                                                    // Comando de retorno de valor em funções
        $$ = ast_new(AST_RETURN, @1.first_line, NULL);                      // Cria nó para representar a instrução retorne
        ast_add_child($$, $2);                                              // Anexa o resultado da expressão ao nó de retorno
      }
    | LEIA lvalue ';' {                                                     // Comando de entrada de dados via teclado
        $$ = ast_new(AST_READ, @1.first_line, NULL);                        // Cria nó para a instrução de leitura
        ast_add_child($$, $2);                                              // Associa o local de destino (variável/vetor) à leitura
      }
    | ESCREVA expr ';' {                                                    // Comando de saída para valores calculados
        $$ = ast_new(AST_WRITE_EXPR, @1.first_line, NULL);                  // Cria nó para escrita de resultado de expressão
        ast_add_child($$, $2);                                              // Anexa a expressão que será avaliada e impressa
      }
    | ESCREVA CADEIACARACTERES ';' {                                        // Comando de saída para mensagens literais
        $$ = ast_new(AST_WRITE_STR, @2.first_line, $2);                     // Cria nó para escrita de string constante
        free($2);                                                           // Libera a memória do lexema após cópia na AST
      }
    | NOVALINHA ';' { $$ = ast_new(AST_NEWLINE, @1.first_line, NULL); }     // Cria nó para o comando de quebra de linha
    | SE '(' expr ')' ENTAO comando FIMSE {                                 // Estrutura condicional simples (se-então)
        $$ = ast_new(AST_IF, @1.first_line, NULL);                          // Cria o nó representativo do condicional se
        ast_add_child($$, $3);                                              // Adiciona a condição (expressão lógica) como 1º filho
        ast_add_child($$, $6);                                              // Adiciona o comando a ser executado como 2º filho
      }
    | SE '(' expr ')' ENTAO comando SENAO comando FIMSE {                   // Estrutura condicional completa (se-senao)
        $$ = ast_new(AST_IF_ELSE, @1.first_line, NULL);                     // Cria o nó para o condicional com alternativa senao
        ast_add_child($$, $3);                                              // Adiciona a condição lógica do desvio
        ast_add_child($$, $6);                                              // Adiciona o bloco/comando do ramo verdadeiro
        ast_add_child($$, $8);                                              // Adiciona o bloco/comando do ramo falso
      }
    | ENQUANTO '(' expr ')' comando {                                       // Estrutura de repetição baseada em condição
        $$ = ast_new(AST_WHILE, @1.first_line, NULL);                       // Cria o nó para o laço enquanto
        ast_add_child($$, $3);                                              // Define a condição de permanência no laço
        ast_add_child($$, $5);                                              // Define o corpo de comandos da iteração
      }
    | bloco { $$ = $1; }                                                    // Permite o aninhamento de blocos como comandos
    ;

lvalue                                                                      // Define alvos de atribuição ou leitura (L-values)
    : IDENTIFICADOR {                                                       // Caso de uso simples de uma variável
        $$ = ast_new(AST_IDENTIFIER, @1.first_line, $1);                    // Cria nó identificador com o nome da variável
        free($1);                                                           // Limpa o lexema original após criar o nó
      }
    | IDENTIFICADOR '[' expr ']' {                                          // Caso de acesso a elemento indexado de vetor
        $$ = ast_new(AST_ARRAY_ACCESS, @1.first_line, $1);                  // Cria nó para acesso de array (vetor)
        ast_add_child($$, ast_new(AST_IDENTIFIER, @1.first_line, $1));      // Anexa o nome do vetor como base
        ast_add_child($$, $3);                                              // Anexa o índice resultante da expressão
        free($1);                                                           // Libera a memória do nome do vetor
      }
    ;

expr                                                                        // Regra principal para avaliação de expressões
    : lvalue '=' expr {                                                     // Operação de atribuição de valor a variável/vetor
        $$ = ast_new(AST_ASSIGN, @2.first_line, NULL);                      // Cria nó para o operador de atribuição
        ast_add_child($$, $1);                                              // Primeiro filho: o destino (lvalue)
        ast_add_child($$, $3);                                              // Segundo filho: o valor (expressão à direita)
      }
    | or_expr { $$ = $1; }                                                  // Repassa expressões lógicas para a hierarquia
    ;

or_expr                                                                     // Nível da precedência para o operador lógico OU
    : or_expr OU and_expr { $$ = make_binary(AST_OR, $1, $3, @2.first_line); } // Cria nó binário para operação '||'
    | and_expr { $$ = $1; }                                                 // Repassa para o próximo nível (E lógico)
    ;

and_expr                                                                    // Nível da precedência para o operador lógico E
    : and_expr E eq_expr { $$ = make_binary(AST_AND, $1, $3, @2.first_line); } // Cria nó binário para operação '&'
    | eq_expr { $$ = $1; }                                                  // Repassa para comparações de igualdade
    ;

eq_expr                                                                     // Nível da precedência para operadores de igualdade
    : eq_expr IGUAL desig_expr { $$ = make_binary(AST_EQ, $1, $3, @2.first_line); } // Cria nó para '=='
    | eq_expr DIFERENTE desig_expr { $$ = make_binary(AST_NE, $1, $3, @2.first_line); } // Cria nó para '!='
    | desig_expr { $$ = $1; }                                               // Repassa para desigualdades relacionais
    ;

/* Define os operadores relacionais de comparação com precedência sobre operações aritméticas. */
desig_expr
    : desig_expr '<' add_expr { $$ = make_binary(AST_LT, $1, $3, @2.first_line); } // Cria nó para 'menor que'
    | desig_expr '>' add_expr { $$ = make_binary(AST_GT, $1, $3, @2.first_line); } // Cria nó para 'maior que'
    | desig_expr MAIORIGUAL add_expr { $$ = make_binary(AST_GE, $1, $3, @2.first_line); } // Cria nó para '>='
    | desig_expr MENORIGUAL add_expr { $$ = make_binary(AST_LE, $1, $3, @2.first_line); } // Cria nó para '<='
    | add_expr { $$ = $1; }                                                 // Repassa para o nível de adição
    ;

/* Nível de precedência para operações de soma (+) e subtração (-). */
add_expr
    : add_expr '+' mul_expr { $$ = make_binary(AST_ADD, $1, $3, @2.first_line); } // Cria nó binário de soma
    | add_expr '-' mul_expr { $$ = make_binary(AST_SUB, $1, $3, @2.first_line); } // Cria nó binário de subtração
    | mul_expr { $$ = $1; }                                                 // Repassa para multiplicação/divisão
    ;

/* Nível de precedência para operações de multiplicação (*) e divisão (/). */
mul_expr
    : mul_expr '*' un_expr { $$ = make_binary(AST_MUL, $1, $3, @2.first_line); }  // Cria nó binário de multiplicação
    | mul_expr '/' un_expr { $$ = make_binary(AST_DIV, $1, $3, @2.first_line); }  // Cria nó binário de divisão
    | un_expr { $$ = $1; }                                                  // Repassa para operadores unários
    ;

un_expr
    : '-' prim_expr { $$ = make_unary(AST_NEG, $2, @1.first_line); }         // Cria nó para inversão de sinal
    | '!' prim_expr { $$ = make_unary(AST_NOT, $2, @1.first_line); }         // Cria nó para negação lógica (!)
    | prim_expr { $$ = $1; }                                                // Repassa para expressões primárias
    ;

prim_expr
    : IDENTIFICADOR '(' arg_list_opt ')' {                                  // Chamada de função com argumentos
        $$ = ast_new(AST_FUNC_CALL, @1.first_line, $1);                     // Cria nó de chamada com o nome
        ast_add_child($$, $3);                                              // Anexa a lista de argumentos reais
        free($1);                                                           // Libera o lexema temporário
      }
    | IDENTIFICADOR '[' expr ']' {                                          // Acesso a elemento de vetor
        $$ = ast_new(AST_ARRAY_ACCESS, @1.first_line, $1);                  // Cria nó de acesso indexado
        ast_add_child($$, ast_new(AST_IDENTIFIER, @1.first_line, $1));      // Define o identificador base do vetor
        ast_add_child($$, $3);                                              // Define a expressão do índice
        free($1);                                                           // Limpa memória do nome original
      }
    | IDENTIFICADOR {                                                       // Uso simples de variável (identificador)
        $$ = ast_new(AST_IDENTIFIER, @1.first_line, $1);                    // Cria nó identificador para a AST
        free($1);                                                           // Libera string do identificador
      }
    | CARCONST {                                                            // Uso de literal de caractere (ex: 'a')
        $$ = ast_new(AST_CHAR_LITERAL, @1.first_line, $1);                  // Cria nó de constante caractere
        free($1);                                                           // Libera o lexema da constante
      }
    | INTCONST {                                                            // Uso de literal inteiro (ex: 10)
        $$ = ast_new(AST_INT_LITERAL, @1.first_line, $1);                   // Cria nó de valor constante inteiro
        free($1);                                                           // Libera o texto da constante
      }
    | '(' expr ')' { $$ = $2; }                                             // Preserva prioridade com parênteses
    ;

arg_list_opt
    : /* empty */ { $$ = make_expr_list(); }                                // Retorna lista vazia se não há argumentos
    | list_expr { $$ = $1; }                                                // Retorna a lista de expressões processada
    ;

list_expr
    : expr {                                                                // Primeiro argumento da lista
        $$ = make_expr_list();                                              // Inicializa o container de expressões
        ast_add_child($$, $1);                                              // Adiciona a primeira expressão
      }
    | list_expr ',' expr {                                                  // Argumentos adicionais na lista
        ast_add_child($1, $3);                                              // Concatena nova expressão à lista
        $$ = $1;                                                            // Mantém o ponteiro da lista atual
      }
    ;
%%

/* Emite uma notificação de erro sintático para o usuário, identificando a linha exata onde a gramática foi violada. */
/* Tipo void por ser uma função de saída sem retorno; o parâmetro s é a string de erro padrão enviada automaticamente pelo Bison. */
void yyerror(const char *s) {
    (void)s;                                                            // Silencia o aviso de parâmetro não utilizado
                                                                        // Exibe o erro sintático indicando a linha onde
                                                                        // a sequência de tokens falhou
    fprintf(stderr, "ERRO: ERRO SINTATICO LINHA %d\n", yylineno);
}

int main(int argc, char **argv) {
    int parse_result;                                                   // Variável para armazenar o status do parser

    if (argc != 2) {                                                    // Valida se o usuário passou o arquivo de entrada
                                                                        // Exibe mensagem de instrução de uso correto caso
                                                                        // falte o argumento do arquivo .g
        fprintf(stderr, "ERRO: USO: ./g-v2 arquivo.g\n");
        return 1;                                                       // Encerra o programa com código de erro
    }

    yyin = fopen(argv[1], "r");                                         // Tenta abrir o arquivo fonte para leitura
    if (!yyin) {                                                        // Verifica se o ponteiro do arquivo é nulo
        fprintf(stderr, "ERRO: NAO FOI POSSIVEL ABRIR ARQUIVO\n");      // Notifica falha de acesso ao sistema de arquivos
        return 1;                                                       // Finaliza por erro de abertura de arquivo
    }

    parse_result = yyparse();                                           // Inicia o processo de análise sintática (Bison)
    fclose(yyin);                                                       // Fecha o arquivo de entrada após o processamento
                                                                        // Verifica se houve erro sintático ou se a árvore
                                                                        // não foi gerada corretamente
    if (parse_result != 0 || !g_root) {
        ast_free(g_root);                                               // Limpa qualquer nó da árvore parcial alocado
        return 1;                                                       // Retorna erro de análise sintática
    }

    if (!semantic_check(g_root)) {                                      // Dispara a validação semântica da árvore abstrata
        ast_free(g_root);                                               // Libera a memória da AST se houver erro de tipos
        return 1;                                                       // Interrompe por erro de lógica/semântica
    }

                                                                        // Tenta gerar o arquivo .asm a partir do caminho
                                                                        // do arquivo de entrada original
    if (!codegen_generate(g_root, argv[1])) {
        fprintf(stderr, "ERRO: FALHA NA GERACAO DE CODIGO\n");          // Reporta falha crítica no gerador de código
        ast_free(g_root);                                               // Garante a limpeza da memória antes de sair
        return 1;                                                       // Retorna código de erro na fase de backend
    }

    ast_free(g_root);                                                   // Libera a árvore após a compilação bem-sucedida
    return 0;                                                           // Finaliza o compilador com sucesso absoluto
}

/*---------------------symbol_table.h--------------------*/

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

/*---------------------symbol_table.c--------------------*/

#include "symbol_table.h"

#include <stdlib.h>
#include <string.h>
/* Cria uma cópia dinâmica da string fornecida para armazenamento persistente na tabela. */
/* Retorna char* para o ponteiro da nova área alocada; o parâmetro s é a string original a ser duplicada. */
static char *dup_str(const char *s) {
    size_t len;                                      // Declara variável para armazenar o comprimento da string
    char *copy;                                      // Declara ponteiro para o novo bloco de memória alocado

    if (!s) {                                        // Verifica se o ponteiro de entrada fornecido é nulo
        return NULL;                                 // Retorna nulo para evitar erros de processamento
    }
    len = strlen(s);                                 // Calcula o tamanho da string original usando strlen
    copy = (char *)malloc(len + 1);                  // Aloca memória dinâmica incluindo o terminador '\0'
    if (!copy) {                                     // Valida se a alocação de memória pelo malloc falhou
        return NULL;                                 // Retorna nulo caso o sistema não tenha memória livre
    }
    memcpy(copy, s, len + 1);                        // Copia len + 1 bytes da origem s para o destino copy
    return copy;                                     // Retorna o ponteiro para a nova string duplicada
}

/* Libera toda a memória associada a um símbolo, incluindo nome, rótulo e parâmetros. */
/* Tipo void pois realiza apenas a desalocação; o parâmetro sym é o ponteiro para a estrutura do símbolo a ser destruída. */
static void symbol_free(Symbol *sym) {
    if (!sym) {                                      // Verifica se o ponteiro do símbolo recebido é nulo
        return;                                      // Encerra a execução da função imediatamente
    }
    free(sym->name);                                 // Libera a memória alocada para a string do nome
    free(sym->label);                                // Libera a memória alocada para a string do rótulo assembly
    paraminfo_free(sym->params);                     // Chama função interna para limpar a lista de parâmetros
    free(sym);                                       // Desaloca a estrutura principal do símbolo do heap
}

/* Percorre e libera recursivamente uma lista ligada de informações de parâmetros. */
/* Tipo void pois limpa a memória da lista; o parâmetro list é o ponteiro para o início da lista de parâmetros. */
void paraminfo_free(ParamInfo *list) {
    ParamInfo *next;                                 // Declara ponteiro auxiliar para rastrear o próximo item
    while (list) {                                   // Itera enquanto houver elementos na lista ligada
        next = list->next;                           // Salva o endereço do próximo nó antes de liberar o atual
        free(list->name);                            // Libera a string do nome armazenada no nó atual
        free(list);                                  // Desaloca a estrutura de informação de parâmetro atual
        list = next;                                 // Avança para o próximo item da lista salva anteriormente
    }
}

/* Aloca e inicializa uma nova estrutura de informação de parâmetro para uma função. */
/* Retorna ParamInfo* para o novo nó criado; name, type, is_vector e vector_size definem as propriedades do parâmetro. */
ParamInfo *paraminfo_new(const char *name, Gv1Type type, int is_vector, int vector_size) {
                                                    /* calloc aloca memória e inicializa todos os bytes com zero para segurança. */
    ParamInfo *p = (ParamInfo *)calloc(1, sizeof(ParamInfo));
    if (!p) {                                        // Verifica se a alocação de memória dinâmica falhou
        return NULL;                                 // Retorna nulo em caso de erro crítico de recursos
    }
    p->name = dup_str(name);                         // Realiza cópia profunda do nome usando dup_str interno
    p->type = type;                                  // Define o tipo de dado (int ou car) do parâmetro
    p->is_vector = is_vector;                        // Atribui a flag indicando se é um parâmetro vetorial
    p->vector_size = vector_size;                    // Armazena a dimensão física se for um vetor
    return p;                                        // Retorna o ponteiro para o novo parâmetro inicializado
}

/* Adiciona um novo nó de parâmetro ao final da lista de parâmetros fornecida. */
/* Tipo void pois modifica a lista existente; **list permite alterar o ponteiro da cabeça e *node é o item a anexar. */
void paraminfo_append(ParamInfo **list, ParamInfo *node) {
    ParamInfo *cur;                                  // Declara ponteiro auxiliar para percorrer a lista

    if (!list || !node) {                            // Verifica integridade dos ponteiros de entrada
        return;                                      // Sai da função se os dados fornecidos forem inválidos
    }
    if (!*list) {                                    // Verifica se a lista de parâmetros está atualmente vazia
        *list = node;                                // Define o nó fornecido como o primeiro item da lista
        return;                                      // Finaliza a operação de anexação inicial
    }
    cur = *list;                                     // Inicia o ponteiro de navegação na cabeça da lista
    while (cur->next) {                              // Percorre a lista até encontrar o último elemento atual
        cur = cur->next;                             // Avança o ponteiro auxiliar para o próximo nó
    }
    cur->next = node;                                // Conecta o novo parâmetro ao campo 'next' do último nó
}

/* Inicializa a pilha de tabelas de símbolos definindo o topo como vazio. */
/* Tipo void pois apenas zera o ponteiro de controle; o parâmetro stack é a estrutura da pilha a ser inicializada. */
void symtab_init(SymbolTableStack *stack) {
    stack->top = NULL;                               // Define o topo da pilha como nulo (inicialmente vazia)
}
/* Cria um novo nível de escopo e o empilha no topo da pilha de símbolos. */
/* Tipo void pois altera o estado da pilha global; o parâmetro stack é a pilha onde o novo escopo será inserido. */
void symtab_push_scope(SymbolTableStack *stack) {
                                                    /* Aloca memória para a estrutura Scope, zerando os campos automaticamente. */
    Scope *scope = (Scope *)calloc(1, sizeof(Scope));
    if (!scope) {                                    // Valida se a alocação do novo escopo foi bem-sucedida
        return;                                      // Aborta se não houver memória disponível no sistema
    }
    scope->next = stack->top;                        // Aponta o novo escopo para o antigo topo da pilha
    stack->top = scope;                              // Atualiza o topo da pilha para o novo escopo criado
}

/* Remove o escopo atual do topo da pilha, liberando todos os símbolos contidos nele. */
/* Tipo void pois realiza a destruição do nível; o parâmetro stack identifica a pilha de onde o escopo será retirado. */
void symtab_pop_scope(SymbolTableStack *stack) {
    Scope *scope;                                    // Ponteiro auxiliar para o escopo a ser removido
    Symbol *sym;                                     // Ponteiro para navegar na lista de símbolos do escopo
    Symbol *next;                                    // Ponteiro auxiliar para desalocação segura de símbolos

    if (!stack->top) {                               // Verifica se a pilha já está vazia
        return;                                      // Sai da função se não houver escopo para remover
    }

    scope = stack->top;                              // Captura o endereço do escopo atual no topo
    stack->top = scope->next;                        // Faz o topo da pilha apontar para o escopo pai
    sym = scope->symbols;                            // Inicia a limpeza pela lista de símbolos do escopo
    while (sym) {                                    // Itera sobre cada símbolo registrado neste nível
        next = sym->next;                            // Salva o próximo símbolo antes de destruir o atual
        symbol_free(sym);                            // Chama a função interna para liberar o registro do símbolo
        sym = next;                                  // Avança para o próximo item da lista ligada
    }
    free(scope);                                     // Libera a estrutura de cabeçalho do escopo desalocado
}

/* Desaloca toda a pilha de tabelas de símbolos, limpando todos os escopos remanescentes. */
/* Tipo void porque apenas realiza a limpeza da memória; o parâmetro stack aponta para a estrutura que gerencia a pilha de escopos. */
void symtab_free(SymbolTableStack *stack) {
                                                    /* Executa o desempilhamento sucessivo até que o ponteiro do topo seja nulo. */
    while (stack->top) {                             
        symtab_pop_scope(stack);                     // Remove e limpa o escopo atual em cada iteração
    }
}

/* Pesquisa um identificador apenas no escopo atual para verificar redeclarações locais. */
/* Retorna Symbol* para fornecer os dados do símbolo encontrado; stack identifica a pilha e name é a string do identificador buscado. */
Symbol *symtab_lookup_current(SymbolTableStack *stack, const char *name) {
    Symbol *sym;                                     // Ponteiro para iterar pelos símbolos do nível atual

    if (!stack->top) {                               // Checa se existe algum escopo ativo na pilha
        return NULL;                                 // Retorna nulo se não houver escopo para pesquisar
    }
    sym = stack->top->symbols;                       // Inicia busca na lista de símbolos do topo da pilha
    while (sym) {                                    // Percorre a lista ligada de símbolos do escopo atual
                                                        /* Compara o nome armazenado com o nome buscado (retorna 0 se idênticos). */
        if (strcmp(sym->name, name) == 0) {          
            return sym;                              // Retorna o ponteiro do símbolo se encontrado localmente
        }
        sym = sym->next;                             // Avança para o próximo símbolo na lista do escopo
    }
    return NULL;                                     // Retorna nulo se o nome não existir no escopo atual
}

/* Busca um símbolo percorrendo a pilha do escopo atual até o global seguindo as regras de visibilidade. */
/* Retorna Symbol* para acessar o registro visível no escopo; stack provê a hierarquia de busca e name é o nome a ser resolvido. */
Symbol *symtab_lookup(SymbolTableStack *stack, const char *name) {
    Scope *scope = stack->top;                       // Inicia a navegação a partir do escopo mais interno
    Symbol *sym;                                     // Ponteiro auxiliar para busca interna de símbolos

                                                    /* Itera através da hierarquia de escopos, do topo em direção à base (global). */
    while (scope) {                                  
        sym = scope->symbols;                        // Obtém a lista de símbolos do nível de escopo atual
        while (sym) {                                // Pesquisa exaustiva dentro do nível atual
                                                            /* Verifica se o lexema do símbolo atual coincide com o nome buscado. */
            if (strcmp(sym->name, name) == 0) {      
                return sym;                          // Retorna o símbolo mais próximo (respeitando o sombreamento)
            }
            sym = sym->next;                         // Passa para o próximo símbolo do mesmo nível
        }
        scope = scope->next;                         // Sobe para o escopo pai (envolvente) para continuar a busca
    }
    return NULL;                                     // Retorna nulo se o nome não for achado em nenhum escopo
}
/* Função interna: aloca e inicializa um símbolo no topo da pilha. Parâmetros: stack (alvo), name (lexema), kind (categoria), type (tipo), is_vector (flag), vector_size (tamanho), label (rótulo assembly). */
/* Retorna Symbol* para o novo objeto; stack indica o destino, name o identificador, kind/type a categoria e dados, e label o rótulo assembly. */
static Symbol *declare_symbol(SymbolTableStack *stack, const char *name, SymbolKind kind, Gv1Type type, int is_vector, int vector_size, const char *label) {
    Symbol *sym;                                     // Ponteiro local para o novo registro de símbolo

                                                    /* Verifica se existe um escopo ativo e se o identificador já foi declarado no nível atual. */
    if (!stack->top || symtab_lookup_current(stack, name)) {
        return NULL;                                 // Retorna nulo para evitar redeclaração no mesmo escopo
    }
    sym = (Symbol *)calloc(1, sizeof(Symbol));       // Aloca memória zerada para a estrutura do símbolo
    if (!sym) {                                      // Valida se o sistema conseguiu prover a memória
        return NULL;                                 // Aborta em caso de falha crítica de recursos
    }
    sym->name = dup_str(name);                       // Armazena uma cópia permanente do nome (identificador)
    sym->kind = kind;                                // Define a categoria (variável, parâmetro ou função)
    sym->type = type;                                // Define o tipo básico associado (int ou car)
    sym->is_vector = is_vector;                      // Atribui a flag indicando se o símbolo é um vetor
    sym->vector_size = vector_size;                  // Registra a dimensão física para vetores escalares
    sym->label = dup_str(label);                     // Salva o rótulo MIPS (usado para variáveis globais)
                                                    /* Valida se as duplicações de strings obrigatórias foram processadas com sucesso. */
    if (!sym->name || (label && !sym->label)) {
        symbol_free(sym);                            // Libera a memória parcial em caso de erro na string
        return NULL;                                 // Retorna nulo por falha de alocação de texto
    }
    sym->next = stack->top->symbols;                 // Conecta o novo símbolo ao início da lista do escopo
    stack->top->symbols = sym;                       // Atualiza a cabeça da lista de símbolos do escopo
    return sym;                                      // Retorna o ponteiro para o símbolo recém-registrado
}

/* Cria uma nova entrada de variável na tabela de símbolos do escopo atual. */
/* Retorna Symbol* para o registro da variável; stack é a pilha e os demais campos definem as propriedades físicas e lógicas da variável. */
Symbol *symtab_declare_var(SymbolTableStack *stack, const char *name, Gv1Type type, int is_vector, int vector_size, const char *label) {
                                                    /* Invoca o declarador genérico especificando a categoria como variável (SYM_VAR). */
    return declare_symbol(stack, name, SYM_VAR, type, is_vector, vector_size, label);
}

/* Registra um novo parâmetro de função no escopo atual da tabela de símbolos. */
/* Retorna Symbol* para o parâmetro; stack situa a inserção e name/type/is_vector especificam a interface formal da sub-rotina. */
Symbol *symtab_declare_param(SymbolTableStack *stack, const char *name, Gv1Type type, int is_vector, int vector_size) {
                                                    /* Invoca o declarador genérico configurando a categoria como parâmetro (SYM_PARAM). */
    return declare_symbol(stack, name, SYM_PARAM, type, is_vector, vector_size, NULL);
}

/* Declara uma função, armazenando seu tipo de retorno e a lista detalhada de seus parâmetros. */
/* Retorna Symbol* para a função; stack é a pilha, name/type o nome e retorno, e param_count/params detalham a assinatura. */
Symbol *symtab_declare_func(SymbolTableStack *stack, const char *name, Gv1Type type, int param_count, ParamInfo *params) {
                                                    /* Cria inicialmente o símbolo da função como um tipo escalar (is_vector=0). */
    Symbol *sym = declare_symbol(stack, name, SYM_FUNC, type, 0, -1, NULL);
    if (!sym) {                                      // Verifica se houve falha na criação do símbolo base
        return NULL;                                 // Retorna nulo caso o nome já esteja em uso no escopo
    }
    sym->param_count = param_count;                  // Registra a aridade (número de argumentos) da função
    sym->params = params;                            // Associa a lista ligada de metadados dos parâmetros
    return sym;                                      // Retorna o registro completo da assinatura da função
}

/*---------------------semantic.h--------------------*/

#ifndef GV_SEMANTIC_H
#define GV_SEMANTIC_H

#include "ast.h"

int semantic_check(ASTNode *root);

#endif

/*---------------------semantic.c--------------------*/

#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>

#include "symbol_table.h"                            // Interface para manipulação da pilha de tabelas de símbolos

typedef struct {                                     // Define a estrutura para representação de tipos avaliados
    Gv1Type type;                                    // Armazena o tipo básico do dado (inteiro ou caractere)
    int is_vector;                                   // Flag booleana que indica se o valor é um escalar ou vetor
} ValueType;                                         // Tipo utilizado para propagação de tipos em expressões

typedef struct {                                     // Estrutura que mantém o estado global da análise semântica
    SymbolTableStack table;                          // Pilha de escopos para busca e registro de identificadores
    int has_error;                                   // Flag de controle para evitar a emissão de erros duplicados
    int in_function;                                 // Indica se o analisador está dentro do corpo de uma função
    Gv1Type current_return_type;                     // Tipo de retorno da função atual para validação do retorne
} SemanticContext;                                   // Contexto completo para verificação de regras da linguagem

/* Retorna uma estrutura de tipo marcada como inválida para sinalizar erros em expressões. */
/* Retorna ValueType para propagação interna; não possui parâmetros pois gera um estado de erro padrão. */
static ValueType value_invalid(void) {
    ValueType v;                                     // Declara uma variável temporária para o tipo de valor
    v.type = TYPE_INVALID;                           // Define o tipo como inválido para sinalizar erros de análise
    v.is_vector = 0;                                 // Define que o valor não é um vetor por padrão
    return v;                                        // Retorna a estrutura de valor inicializada como inválida
}

/* Cria uma representação de tipo para um valor escalar (não vetor) baseado no tipo básico fornecido. */
/* Retorna ValueType configurado como escalar; o parâmetro type define se o valor é inteiro ou caractere. */
static ValueType value_scalar(Gv1Type type) {
    ValueType v;                                     // Declara a estrutura para armazenar o tipo escalar
    v.type = type;                                   // Atribui o tipo básico (int ou car) recebido via parâmetro
    v.is_vector = 0;                                 // Garante que a flag de identificação de vetor esteja desativada
    return v;                                        // Retorna a representação do valor escalar configurado
}

/* Cria uma representação de tipo para um identificador que representa um vetor. */
/* Retorna ValueType com a flag de vetor ativa; o parâmetro type especifica o tipo base dos elementos. */
static ValueType value_vector(Gv1Type type) {
    ValueType v;                                     // Declara a estrutura para representar um tipo vetorial
    v.type = type;                                   // Define o tipo básico dos elementos contidos no vetor
    v.is_vector = 1;                                 // Ativa a flag indicando que este valor trata-se de um vetor
    return v;                                        // Retorna a estrutura configurada com as propriedades de vetor
}

/* Exibe uma mensagem de erro semântico formatada e interrompe a emissão de erros subsequentes. */
/* Tipo void pois apenas realiza saída e altera estado; ctx gerencia a flag de erro, line indica a posição e msg a descrição. */
static void semantic_error(SemanticContext *ctx, int line, const char *msg) {
    if (ctx->has_error) {                            // Checa se um erro já foi emitido anteriormente
        return;                                      // Aborta a função para evitar mensagens de erro em cascata
    }
                                                    /* Imprime a mensagem de erro formatada na saída de erro padrão do sistema. */
    fprintf(stderr, "ERRO: %s LINHA %d\n", msg, line);
    ctx->has_error = 1;                              // Ativa a flag global de erro para interromper novas emissões
}

static ValueType infer_expr(SemanticContext *ctx, ASTNode *node); // Protótipo para inferência de tipos em expressões
static void analyze_command(SemanticContext *ctx, ASTNode *cmd);  // Protótipo para análise semântica de comandos

/* Verifica se um nó específico da AST corresponde à categoria de lista de declarações. */
/* Retorna int como booleano; o parâmetro node é o ponteiro para o nó da AST a ser testado. */
static int is_decl_list(ASTNode *node) {
                                                    /* Retorna verdadeiro se o nó não for nulo e seu tipo for uma lista de declarações. */
    return node && node->kind == AST_DECL_LIST;
}

/* Verifica se um nó específico da AST corresponde à categoria de lista de comandos. */
/* Retorna int como booleano; o parâmetro node é o ponteiro para o nó da AST a ser testado. */
static int is_cmd_list(ASTNode *node) {
                                                    /* Retorna verdadeiro se o nó existir e for categorizado como lista de comandos. */
    return node && node->kind == AST_CMD_LIST;
}

/* Identifica e extrai o nó de lista de declarações contido dentro de um nó de bloco. */
/* Retorna ASTNode* para o sub-nó encontrado ou NULL; o parâmetro block é o nó pai do tipo bloco. */
static ASTNode *block_decl_list(ASTNode *block) {
                                                    /* Valida se o nó é um bloco válido e se possui pelo menos um filho. */
    if (!block || block->kind != AST_BLOCK || block->child_count == 0) {
        return NULL;                                 // Retorna nulo se o bloco for inválido ou estiver vazio
    }
                                                    /* Retorna o primeiro filho se for uma lista de declarações, caso contrário retorna nulo. */
    return is_decl_list(block->children) ? block->children : NULL;
}

/* Identifica e extrai o nó de lista de comandos contido dentro de um nó de bloco. */
/* Retorna ASTNode* para o sub-nó encontrado ou NULL; o parâmetro block é o nó pai do tipo bloco. */
static ASTNode *block_cmd_list(ASTNode *block) {
                                                    /* Verifica a integridade do nó e se ele contém descendentes para analisar. */
    if (!block || block->kind != AST_BLOCK || block->child_count == 0) {
        return NULL;                                 // Retorna nulo para blocos inexistentes ou sem filhos
    }
    if (is_decl_list(block->children)) {          // Checa se o primeiro filho é a seção de declarações
                                                        /* Se houver declarações, a lista de comandos deve ser o segundo filho (índice 1). */
        return block->child_count > 1 ? block->children[1] : NULL;
    }
    return block->children;                       // Se não houver declarações, a lista de comandos é o primeiro filho
}
/* Transforma a subárvore de parâmetros da AST em uma lista ligada compatível com a Tabela de Símbolos. */
/* Retorna ParamInfo* para armazenamento na assinatura da função; params é o nó da AST com os dados originais. */
static ParamInfo *build_param_info(ASTNode *params) {
    ParamInfo *list = NULL;                          // Inicializa a cabeça da lista de parâmetros como nula
    size_t i;                                        // Variável de iteração para percorrer os nós da AST

    if (!params || params->kind != AST_PARAM_LIST) { // Valida se o nó recebido é uma lista de parâmetros válida
        return NULL;                                 // Aborta retornando nulo se a estrutura for incompatível
    }
    for (i = 0; i < params->child_count; ++i) {      // Itera sobre cada filho do nó da lista de parâmetros
        ASTNode *p = params->children[i];            // Obtém o ponteiro para o i-ésimo nó de parâmetro
        ParamInfo *info;                             // Ponteiro para armazenar a nova estrutura de informação
        if (!p || p->kind != AST_PARAM) {            // Ignora nós que não representam parâmetros individuais
            continue;                                // Salta para a próxima iteração do laço
        }
                                                        /* paraminfo_new cria metadado: lexema (nome), tipo, flag vetor e tamanho físico. */
        info = paraminfo_new(p->lexeme, p->type, p->is_vector, p->vector_size);
        if (!info) {                                 // Checa se a alocação do novo metadado falhou
            paraminfo_free(list);                    // Libera a lista parcial construída em caso de erro
            return NULL;                             // Retorna nulo por falha crítica de recursos
        }
        paraminfo_append(&list, info);               // Anexa o novo registro ao final da lista vinculada
    }
    return list;                                     // Retorna a lista completa de informações dos parâmetros
}

/* Percorre itens declarados (variáveis/vetores) e os registra no escopo atual da Tabela de Símbolos. */
/* Retorna int indicando sucesso ou contagem; ctx é o estado da análise, decls a lista de nós e allow_vector_size controla a validação física. */
static int declare_decl_items(SemanticContext *ctx, ASTNode *decls, int allow_vector_size) {
    size_t i;                                        // Índice para percorrer os itens de declaração

    if (!decls || decls->kind != AST_DECL_LIST) {    // Verifica se o nó é uma lista de declarações válida
        return 1;                                    // Retorna sucesso (nada a declarar) se a lista for vazia
    }

    for (i = 0; i < decls->child_count; ++i) {       // Percorre todos os itens contidos na lista de declarações
        ASTNode *decl = decls->children[i];          // Acessa o nó de declaração atual da iteração
        if (!decl || decl->kind != AST_DECL_ITEM) {  // Filtra apenas nós categorizados como itens de declaração
            continue;                                // Pula nós inválidos ou irrelevantes para declaração
        }
                                                        /* symtab_declare_var insere na tabela: lexema, tipo e tamanho; o rótulo é nulo. */
        if (!symtab_declare_var(&ctx->table, decl->lexeme, decl->type, decl->is_vector, allow_vector_size ? decl->vector_size : -1, NULL)) {
                                                            /* semantic_error emite aviso caso o identificador já exista no escopo atual. */
            semantic_error(ctx, decl->line, "IDENTIFICADOR REDECLARADO NO MESMO ESCOPO");
            return 0;                                // Interrompe a análise retornando falha (0)
        }
    }
    return 1;                                        // Retorna sucesso (1) após registrar todos os itens
}

/* Registra os parâmetros de uma função como símbolos locais no novo escopo criado para o corpo. */
/* Retorna int com o total de parâmetros processados; ctx é o estado e params o nó da AST contendo as definições formais. */
static int declare_params(SemanticContext *ctx, ASTNode *params) {
    size_t i;                                        // Variável de controle para o laço de parâmetros

    if (!params || params->kind != AST_PARAM_LIST) { // Valida a integridade do nó de lista de parâmetros
        return 1;                                    // Sucesso por vacuidade se não houver parâmetros
    }

    for (i = 0; i < params->child_count; ++i) {      // Itera pelos parâmetros formais definidos na assinatura
        ASTNode *p = params->children[i];            // Obtém o nó descritor do i-ésimo parâmetro
        if (!p || p->kind != AST_PARAM) {            // Assegura que o nó é realmente um item de parâmetro
            continue;                                // Ignora elementos inesperados na estrutura da árvore
        }
                                                        /* symtab_declare_param registra o parâmetro como símbolo local: nome, tipo e dimensões. */
        if (!symtab_declare_param(&ctx->table, p->lexeme, p->type, p->is_vector, p->vector_size)) {
                                                            /* Emite erro semântico se houver conflito de nomes (parâmetros duplicados). */
            semantic_error(ctx, p->line, "IDENTIFICADOR REDECLARADO NO MESMO ESCOPO");
            return 0;                                // Aborta com falha se encontrar erro de redeclaração
        }
    }
    return 1;                                        // Confirma sucesso após processar toda a lista formal
}
/* Infere o tipo de um L-value (variável ou acesso a vetor), validando declaração e índices. */
/* Retorna ValueType com o tipo e status de vetor; ctx provê o contexto, node é o nó do identificador/vetor e allow_vector define se o vetor inteiro é um alvo válido. */
static ValueType infer_lvalue(SemanticContext *ctx, ASTNode *node, int allow_vector) {
    Symbol *sym;                                              // Ponteiro para o registro do símbolo na tabela
    ValueType idx_type;                                       // Armazena o tipo resultante da expressão de índice

    if (!node || ctx->has_error) {                            // Verifica integridade do nó e estado de erro prévio
        return value_invalid();                               // Aborta retornando tipo inválido se houver falha
    }

    switch (node->kind) {                                     // Avalia a categoria do nó para determinar o tipo
        case AST_IDENTIFIER:                                  // Caso o nó seja um identificador simples
                                                            /* Busca o nome do identificador na pilha de escopos da tabela de símbolos. */
            sym = symtab_lookup(&ctx->table, node->lexeme);
            if (!sym) {                                       // Se o identificador não foi encontrado em nenhum escopo
                                                                /* Emite erro de variável não declarada e interrompe a análise semântica. */
                semantic_error(ctx, node->line, "IDENTIFICADOR NAO DECLARADO");
                return value_invalid();                       // Retorna sinalizador de erro na inferência
            }
            if (sym->kind == SYM_FUNC) {                      // Bloqueia o uso de nomes de funções em atribuições
                                                                /* Reporta tentativa inválida de usar uma função como destino de valor. */
                semantic_error(ctx, node->line, "USO INVALIDO DE FUNCAO COMO VARIAVEL");
                return value_invalid();                       // Sinaliza falha semântica no uso do identificador
            }
            node->type = sym->type;                           // Salva o tipo básico (int/car) no nó da árvore
            node->is_vector = sym->is_vector;                 // Registra no nó se o identificador é um vetor
                                                            /* Devolve a representação de vetor ou escalar conforme a declaração original. */
            return sym->is_vector ? value_vector(sym->type) : value_scalar(sym->type);
        case AST_ARRAY_ACCESS:                                // Caso o nó represente um acesso a elemento de vetor
            if (node->child_count < 2) {                      // Garante existência de base e índice na árvore
                                                                /* Emite erro para estrutura de acesso a vetor corrompida ou incompleta. */
                semantic_error(ctx, node->line, "ACESSO A VETOR INVALIDO");
                return value_invalid();                       // Interrompe a análise deste nó
            }
                                                            /* Resolve o símbolo base do acesso (primeiro filho) na tabela de símbolos. */
            sym = symtab_lookup(&ctx->table, node->children->lexeme);
            if (!sym || sym->kind == SYM_FUNC) {              // Valida se a base existe e não é uma função
                                                                /* Reporta falha caso o identificador base do acesso não esteja declarado. */
                semantic_error(ctx, node->line, "IDENTIFICADOR NAO DECLARADO");
                return value_invalid();                       // Retorna erro de inferência para o lvalue
            }
            if (!sym->is_vector) {                            // Verifica se o símbolo base é realmente um vetor
                                                                /* Reporta erro semântico ao tentar indexar uma variável escalar. */
                semantic_error(ctx, node->line, "IDENTIFICADOR NAO E VETOR");
                return value_invalid();                       // Sinaliza incompatibilidade de estrutura
            }
                                                            /* Realiza a inferência de tipo para a expressão fornecida como índice. */
            idx_type = infer_expr(ctx, node->children[1]);
            if (ctx->has_error) {                             // Checa se a análise da expressão do índice falhou
                return value_invalid();                       // Propaga o erro de inferência para cima
            }
                                                            /* Valida se o índice é um escalar do tipo inteiro (regra da linguagem G-V2). */
            if (idx_type.is_vector || idx_type.type != TYPE_INT) {
                                                                /* Emite erro caso o índice do vetor não resulte em um valor inteiro simples. */
                semantic_error(ctx, node->line, "INDICE DE VETOR DEVE SER INT");
                return value_invalid();                       // Retorna tipo inválido por erro de indexação
            }
            node->type = sym->type;                           // Define o tipo do nó como o tipo base do vetor
            node->is_vector = 0;                              // O resultado do acesso indexado é um escalar
            return value_scalar(sym->type);                   // Retorna o tipo escalar resultante da indexação
        default:                                              // Caso o nó não seja um identificador ou acesso válido
            if (!allow_vector) {                              // Verifica se o contexto exige um L-value estrito
                                                                /* Reporta erro se o lado esquerdo não for uma variável ou elemento de vetor. */
                semantic_error(ctx, node->line, "LADO ESQUERDO INVALIDO");
            }
            return value_invalid();                           // Retorna falha na inferência do L-value
    }
}
/* Valida uma chamada de função, checando existência, número de argumentos e compatibilidade de tipos. */
/* Retorna ValueType contendo o tipo de retorno da função; ctx permite busca na tabela e node contém os dados da chamada. */
static ValueType infer_call(SemanticContext *ctx, ASTNode *node) {
    Symbol *sym;                                              // Ponteiro para o registro da função na tabela de símbolos
    ASTNode *args;                                            // Armazena o nó que contém a lista de argumentos reais
    size_t i;                                                 // Variável de controle para o laço de verificação de argumentos
    size_t expected;                                          // Armazena a quantidade de parâmetros formais esperada

                                                    /* Busca o identificador da função na pilha de escopos da tabela de símbolos. */
    sym = symtab_lookup(&ctx->table, node->lexeme);
    if (!sym || sym->kind != SYM_FUNC) {                      // Valida se o símbolo existe e se é de fato uma função
                                                        /* Reporta erro caso o nome da função não tenha sido declarado previamente. */
        semantic_error(ctx, node->line, "FUNCAO NAO DECLARADA");
        return value_invalid();                               // Retorna tipo inválido para sinalizar falha na inferência
    }

                                                    /* Obtém a lista de argumentos reais (o primeiro filho do nó de chamada). */
    args = node->child_count > 0 ? node->children : NULL;
    expected = (size_t)sym->param_count;                      // Recupera o número de parâmetros registrados na declaração
    if (expected != (args ? args->child_count : 0)) {         // Compara a quantidade fornecida com a esperada
                                                        /* Emite erro caso o número de argumentos na chamada seja diferente da assinatura. */
        semantic_error(ctx, node->line, "NUMERO DE ARGUMENTOS INCOMPATIVEL");
        return value_invalid();                               // Interrompe a análise da chamada por erro de aridade
    }

    for (i = 0; i < expected; ++i) {                          // Itera para validar individualmente cada argumento real
        ParamInfo *formal = sym->params;                      // Reinicia o ponteiro na lista de parâmetros formais
        ASTNode *actual;                                      // Ponteiro para o nó da AST do argumento real atual
        ValueType actual_type;                                // Variável para o tipo resultante da inferência do argumento
        size_t j;                                             // Contador auxiliar para navegação na lista ligada

                                                        /* Percorre a lista ligada de parâmetros formais até alcançar a posição 'i'. */
        for (j = 0; j < i && formal; ++j) {
            formal = formal->next;                            // Avança para o próximo descritor de parâmetro na lista
        }
        if (!formal) {                                        // Proteção de segurança contra fim inesperado da lista
            break;                                            // Sai do laço se houver inconsistência na contagem
        }
        actual = args->children[i];                           // Acessa o i-ésimo argumento real da lista na AST
        actual_type = infer_expr(ctx, actual);                // Invoca inferência recursiva para o valor do argumento
        if (ctx->has_error) {                                 // Verifica se a análise da expressão do argumento falhou
            return value_invalid();                           // Propaga o erro de inferência para o nível superior
        }
        if (formal->is_vector) {                              // Caso o parâmetro formal exija a passagem de um vetor
                                                            /* Valida se o argumento real é um vetor e se possui o mesmo tipo base. */
            if (!actual_type.is_vector || actual_type.type != formal->type) {
                                                              /* Reporta incompatibilidade para parâmetros que
                                                              devem receber vetores. */
                semantic_error(ctx, actual->line, "TIPO DE ARGUMENTO INCOMPATIVEL");
                return value_invalid();                       // Sinaliza erro semântico no argumento vetorial
            }
        } else {                                              // Caso o parâmetro formal seja uma variável escalar
                                                            /* Valida se o argumento real também é escalar e possui tipo compatível. */
            if (actual_type.is_vector || actual_type.type != formal->type) {
                                                              /* Reporta erro quando um vetor é passado onde se espera um valor escalar. */
                semantic_error(ctx, actual->line, "TIPO DE ARGUMENTO INCOMPATIVEL");
                return value_invalid();                       // Sinaliza erro semântico no argumento escalar
            }
        }
    }

    node->type = sym->type;                                   // Define o tipo do nó de chamada como o de retorno da função
    node->is_vector = 0;                                      // Funções em G-V2 retornam obrigatoriamente valores escalares
    return value_scalar(sym->type);                           // Retorna a representação do tipo de retorno para a expressão
}
/* Percorre recursivamente uma expressão para determinar seu tipo resultante e validar operações. */
/* Retorna ValueType com o tipo computado da expressão; ctx mantém o estado da análise e node é a raiz da subárvore da expressão. */
static ValueType infer_expr(SemanticContext *ctx, ASTNode *node) {
    ValueType left;                                           // Armazena o tipo do operando à esquerda (atribuição)
    ValueType right;                                          // Armazena o tipo do operando à direita (atribuição)
    ValueType a;                                              // Armazena o tipo do primeiro operando aritmético
    ValueType b;                                              // Armazena o tipo do segundo operando aritmético
    Symbol *sym;                                              // Ponteiro para o registro do símbolo buscado

    if (!node || ctx->has_error) {                            // Verifica se o nó é nulo ou se já houve erro prévio
        return value_invalid();                               // Retorna tipo inválido para interromper a análise
    }

    switch (node->kind) {                                     // Avalia a categoria do nó para determinar o tipo
        case AST_IDENTIFIER:                                  // Caso o nó seja um identificador (variável/vetor)
            sym = symtab_lookup(&ctx->table, node->lexeme);   // Busca o nome na pilha de escopos da tabela
            if (!sym) {                                       // Se o identificador não foi declarado anteriormente
                semantic_error(ctx, node->line, "IDENTIFICADOR NAO DECLARADO");
                return value_invalid();                       // Retorna erro de inferência para o identificador
            }
            if (sym->kind == SYM_FUNC) {                      // Bloqueia o uso de nomes de funções em expressões
                semantic_error(ctx, node->line, "FUNCAO USADA COMO EXPRESSAO");
                return value_invalid();                       // Reporta falha no uso semântico do nome
            }
            node->type = sym->type;                           // Atribui o tipo básico do símbolo ao nó da AST
            node->is_vector = sym->is_vector;                 // Registra no nó se o símbolo é um vetor
                                                                /* Retorna a representação de vetor ou escalar conforme a declaração. */
            return sym->is_vector ? value_vector(sym->type) : value_scalar(sym->type);
        case AST_INT_LITERAL:                                 // Caso o nó represente um número inteiro literal
            node->type = TYPE_INT;                            // Define o tipo do nó como inteiro (TYPE_INT)
            node->is_vector = 0;                              // Constantes numéricas são sempre escalares
            return value_scalar(TYPE_INT);                    // Retorna representação de valor escalar inteiro
        case AST_CHAR_LITERAL:                                // Caso o nó represente um caractere literal
            node->type = TYPE_CAR;                            // Define o tipo do nó como caractere (TYPE_CAR)
            node->is_vector = 0;                              // Literais de caractere não são vetores
            return value_scalar(TYPE_CAR);                    // Retorna representação de valor escalar caractere
        case AST_STRING_LITERAL:                              // Caso o nó seja uma string (cadeia de caracteres)
            semantic_error(ctx, node->line, "CADEIA NAO E EXPRESSAO");
            return value_invalid();                           // Emite erro: strings não são permitidas em expressões
        case AST_ARRAY_ACCESS:                                // Caso o nó represente acesso a índice de vetor
            return infer_lvalue(ctx, node, 0);                // Delega inferência para a lógica de L-values
        case AST_FUNC_CALL:                                   // Caso o nó represente uma chamada de função
            return infer_call(ctx, node);                     // Delega inferência para a lógica de chamadas
        case AST_ASSIGN:                                      // Caso o nó represente uma operação de atribuição
            if (node->child_count < 2) {                      // Valida se a árvore de atribuição está completa
                semantic_error(ctx, node->line, "ATRIBUICAO INVALIDA");
                return value_invalid();                       // Interrompe a análise por erro de estrutura
            }
            left = infer_lvalue(ctx, node->children, 1);   // Infere o destino permitindo atribuição entre vetores
            right = infer_expr(ctx, node->children[1]);       // Infere recursivamente o tipo do valor atribuído
            if (ctx->has_error) {                             // Verifica se a análise dos operandos gerou erros
                return value_invalid();                       // Propaga a falha de inferência para cima
            }
                                                            /* Valida se os tipos básicos e a dimensionalidade (vetor/escalar) coincidem. */
            if (left.type != right.type || left.is_vector != right.is_vector) {
                semantic_error(ctx, node->line, "TIPOS INCOMPATIVEIS");
                return value_invalid();                       // Reporta erro de tipagem na atribuição
            }
            node->type = left.type;                           // O tipo do nó passa a ser o tipo do operando esquerdo
            node->is_vector = left.is_vector;                 // Preserva a informação de vetor no nó
            return left;                                      // Retorna o tipo validado da operação
        case AST_ADD:                                         // Operações aritméticas: Adição
        case AST_SUB:                                         // Operações aritméticas: Subtração
        case AST_MUL:                                         // Operações aritméticas: Multiplicação
        case AST_DIV:                                         // Operações aritméticas: Divisão
            a = infer_expr(ctx, node->children);           // Infere o tipo do primeiro operando aritmético
            b = infer_expr(ctx, node->children[1]);           // Infere o tipo do segundo operando aritmético
            if (ctx->has_error) {                             // Checa se a inferência dos operandos falhou
                return value_invalid();                       // Propaga erro para interromper a expressão
            }
                                                            /* Operações aritméticas em G-V2 exigem operandos escalares do tipo inteiro. */
            if (a.is_vector || b.is_vector || a.type != TYPE_INT || b.type != TYPE_INT) {
                semantic_error(ctx, node->line, "OPERACAO ARITMETICA EXIGE INT");
                return value_invalid();                       // Reporta erro de tipo nos operandos aritméticos
            }
            node->type = TYPE_INT;                            // Define o resultado da aritmética como inteiro
            node->is_vector = 0;                              // O resultado de cálculos aritméticos é um escalar
            return value_scalar(TYPE_INT);                    // Retorna sucesso com o tipo escalar inteiro
    case AST_OR:                                                        // Início do caso para operação lógica OU
    case AST_AND:                                                       // Início do caso para operação lógica E
        a = infer_expr(ctx, node->children);                         // Chama recursão para inferir o 1º operando
        b = infer_expr(ctx, node->children[1]);                         // Chama recursão para inferir o 2º operando
        if (ctx->has_error) {                                           // Verifica se houve erro na análise dos filhos
            return value_invalid();                                     // Se houve erro prévio, retorna inválido
        }
                                                                        // Operação lógica não permite vetores
                                                                        // e exige que ambos os operandos sejam INT
        if (a.is_vector || b.is_vector || a.type != TYPE_INT || b.type != TYPE_INT) {
            semantic_error(ctx, node->line, "OPERACAO LOGICA EXIGE INT"); // Reporta erro semântico de tipo incompatível
            return value_invalid();                                     // Retorna tipo inválido após detectar erro
        }
        node->type = TYPE_INT;                                          // Atribui o tipo inteiro ao nó da árvore
        node->is_vector = 0;                                            // Define explicitamente que o nó é um escalar
        return value_scalar(TYPE_INT);                                  // Retorna tipo escalar inteiro com sucesso
    case AST_EQ:                                                        // Início do caso para comparação: igual
    case AST_NE:                                                        // Início do caso para comparação: diferente
    case AST_LT:                                                        // Início do caso para comparação: menor que
    case AST_GT:                                                        // Início do caso para comparação: maior que
    case AST_GE:                                                        // Início do caso para comparação: maior igual
    case AST_LE:                                                        // Início do caso para comparação: menor igual
        a = infer_expr(ctx, node->children);                         // Obtém o tipo da expressão à esquerda
        b = infer_expr(ctx, node->children[1]);                         // Obtém o tipo da expressão à direita
        if (ctx->has_error) {                                           // Checa se a inferência dos filhos falhou
            return value_invalid();                                     // Aborta a análise se já houver erro
        }
                                                                        // Relacionais exigem escalares e tipos
                                                                        // base idênticos (ex: int com int)
        if (a.is_vector || b.is_vector || a.type != b.type) {
            semantic_error(ctx, node->line, "OPERACAO RELACIONAL EXIGE TIPOS IGUAIS");
            return value_invalid();                                     // Retorna inválido por incompatibilidade
        }
        node->type = TYPE_INT;                                          // Operações relacionais resultam em lógico (int)
        node->is_vector = 0;                                            // Define o resultado como um valor escalar
        return value_scalar(TYPE_INT);                                  // Retorna o ValueType correspondente a int
    case AST_NEG:                                                       // Início do caso: menos unário (negativo)
    case AST_NOT:                                                       // Início do caso: negação lógica unária (!)
        a = infer_expr(ctx, node->children);                         // Infere o tipo do único operando existente
        if (ctx->has_error) {                                           // Verifica se a subexpressão é válida
            return value_invalid();                                     // Interrompe a análise em caso de erro
        }
        if (a.is_vector || a.type != TYPE_INT) {                        // Operadores unários exigem escalar do tipo INT
            semantic_error(ctx, node->line, "OPERACAO UNARIA EXIGE INT");// Emite erro caso seja vetor ou tipo CAR
            return value_invalid();                                     // Retorna tipo inválido por erro de unário
        }
        node->type = TYPE_INT;                                          // Configura o tipo do nó como inteiro
        node->is_vector = 0;                                            // Garante que o resultado seja tratado como escalar
        return value_scalar(TYPE_INT);                                  // Retorna a representação de valor escalar int
    default:                                                            // Caso o tipo de nó não seja tratado aqui
        return value_invalid();                                         // Retorna inválido por segurança
    }
}
/* Analisa um bloco de comandos, gerenciando a criação e destruição de escopos locais. */
/* Tipo void pois realiza validações de estado; ctx gerencia a pilha de símbolos, block é o nó do bloco e push_scope indica se deve criar novo escopo. */
static void analyze_block(SemanticContext *ctx, ASTNode *block, int push_scope) {
    ASTNode *decls;                                                     // Declara ponteiro para o nó de declarações
    ASTNode *cmds;                                                      // Declara ponteiro para o nó de comandos
    size_t i;                                                           // Variável de controle para iteração em listas

                                                                        // Valida o nó do bloco e o estado de erro do contexto
    if (!block || block->kind != AST_BLOCK || ctx->has_error) {
        return;                                                         // Aborta a análise se o bloco for nulo ou inválido
    }

    if (push_scope) {                                                   // Verifica se o bloco exige criação de novo escopo
        symtab_push_scope(&ctx->table);                                 // Adiciona novo nível de escopo na tabela
    }

    decls = block_decl_list(block);                                     // Identifica a subárvore de declarações do bloco
    cmds = block_cmd_list(block);                                       // Identifica a subárvore de comandos do bloco
    if (!declare_decl_items(ctx, decls, 1)) {                           // Processa a inserção das variáveis no escopo
        if (push_scope) {                                               // Caso ocorra erro na declaração e o escopo foi aberto
            symtab_pop_scope(&ctx->table);                              // Remove o escopo atual para limpeza
        }
        return;                                                         // Encerra a análise do bloco por erro prévio
    }

    if (cmds && is_cmd_list(cmds)) {                                    // Garante que existe uma lista de comandos válida
        for (i = 0; i < cmds->child_count; ++i) {                       // Percorre cada comando individual dentro da lista
            analyze_command(ctx, cmds->children[i]);                    // Realiza análise semântica no comando atual
            if (ctx->has_error) {                                       // Checa se o comando analisado gerou algum erro
                break;                                                  // Interrompe o processamento dos comandos restantes
            }
        }
    }

    if (push_scope) {                                                   // Verifica se o escopo deve ser fechado ao fim
        symtab_pop_scope(&ctx->table);                                  // Desempilha o escopo local da função/bloco
    }
}

/* Analisa a declaração de uma função, registrando sua assinatura e processando seu corpo. */
/* Tipo void pois atualiza a tabela de símbolos e valida o bloco; ctx é o contexto semântico e func é o nó da declaração. */
static void analyze_function(SemanticContext *ctx, ASTNode *func) {
    ASTNode *params;                                                    // Ponteiro para o nó da lista de parâmetros
    ASTNode *body;                                                      // Ponteiro para o nó do bloco do corpo da função
    ParamInfo *plist;                                                   // Ponteiro para estrutura de metadados de parâmetros
    Symbol *sym;                                                        // Ponteiro para o símbolo da função na tabela

                                                                        // Verifica se o nó é uma declaração de função válida
    if (!func || func->kind != AST_FUNCTION_DECL || ctx->has_error) {
        return;                                                         // Retorna se o nó for nulo ou houver erro no contexto
    }

                                                                        // Atribui o primeiro filho ao nó de parâmetros se houver
    params = func->child_count > 0 ? func->children : NULL;
                                                                        // Atribui o segundo filho ao bloco de comandos se houver
    body = func->child_count > 1 ? func->children[1] : NULL;
    plist = build_param_info(params);                                   // Gera lista de tipos para validar chamadas futuras
                                                                        // Valida se a construção da lista de parâmetros falhou
    if (params && params->kind == AST_PARAM_LIST && params->child_count != 0 && !plist) {
        semantic_error(ctx, func->line, "FALHA INTERNA NA LISTA DE PARAMETROS"); // Registra erro interno de processamento
        return;                                                         // Aborta análise da função
    }

                                                                        // Tenta declarar a função e sua assinatura na tabela
    sym = symtab_declare_func(&ctx->table, func->lexeme, func->type, (int)(params ? params->child_count : 0), plist);
    if (!sym) {                                                         // Verifica se a declaração da função falhou
        paraminfo_free(plist);                                          // Libera memória dos parâmetros em caso de erro
        semantic_error(ctx, func->line, "IDENTIFICADOR REDECLARADO NO MESMO ESCOPO"); // Reporta erro de duplicidade
        return;                                                         // Interrompe análise por erro de declaração
    }

    symtab_push_scope(&ctx->table);                                     // Abre escopo local para os parâmetros da função
    if (!declare_params(ctx, params)) {                                 // Insere os parâmetros formais como variáveis locais
        symtab_pop_scope(&ctx->table);                                  // Fecha escopo em caso de erro na declaração
        return;                                                         // Retorna se parâmetros forem inválidos
    }

    ctx->in_function = 1;                                               // Seta flag indicando análise dentro de função
    ctx->current_return_type = func->type;                              // Define o tipo de retorno esperado para os "return"
    analyze_block(ctx, body, 0);                                        // Analisa o corpo da função (bloco de comandos)
    ctx->in_function = 0;                                               // Reseta a flag de contexto de função
    symtab_pop_scope(&ctx->table);                                      // Remove escopo local ao finalizar a função
}
/* Realiza a checagem semântica de comandos (atribuição, controle de fluxo, E/S, retorno). */
/* Tipo void pois emite erros e valida lógica; ctx fornece o ambiente e cmd representa a instrução específica a ser verificada. */
static void analyze_command(SemanticContext *ctx, ASTNode *cmd) {
    ValueType cond;                                                     // Armazena o tipo resultante da condição (IF/WHILE)
    ValueType ret;                                                      // Armazena o tipo resultante de expressões de retorno/leitura

    if (!cmd || ctx->has_error) {                                       // Valida o nó e verifica se já existe erro no contexto
        return;                                                         // Interrompe a análise caso o comando seja nulo ou inválido
    }

    switch (cmd->kind) {                                                // Avalia o tipo de comando no nó da AST
        case AST_EMPTY_STMT:                                            // Caso o comando seja vazio (apenas ponto e vírgula)
            break;                                                      // Não requer verificação adicional
        case AST_EXPR_STMT:                                             // Caso o comando seja apenas uma expressão
            if (cmd->child_count > 0) {                                 // Verifica se há uma expressão associada ao comando
                (void)infer_expr(ctx, cmd->children);                // Realiza a inferência de tipo para validar a expressão
            }
            break;                                                      // Finaliza análise do comando de expressão
        case AST_READ: {                                                // Caso para o comando 'leia'
            ret = infer_lvalue(ctx, cmd->children, 0);               // Verifica se o alvo da leitura é um lvalue válido
            if (ctx->has_error) {                                       // Checa se houve falha na identificação da variável
                break;                                                  // Interrompe se o identificador for inexistente ou inválido
            }
            if (ret.is_vector) {                                        // O comando 'leia' não pode receber um vetor inteiro
                semantic_error(ctx, cmd->line, "LEIA EXIGE VARIAVEL ESCALAR"); // Reporta erro por tentativa de ler para vetor
            }
            break;                                                      // Finaliza análise do comando de leitura
        }
        case AST_WRITE_EXPR:                                            // Caso para o comando 'escreva' com expressão
            if (cmd->child_count > 0) {                                 // Garante que existe um operando para escrita
                ret = infer_expr(ctx, cmd->children);                // Avalia semanticamente a expressão de escrita
                if (!ctx->has_error && ret.is_vector) {                 // Verifica se o resultado da expressão é um vetor
                    semantic_error(ctx, cmd->line, "ESCRITA EXIGE EXPRESSAO ESCALAR"); // Reporta erro de escrita inválida
                }
            }
            break;                                                      // Finaliza análise de escrita de expressão
        case AST_WRITE_STR:                                             // Caso para escrita de literal de string direto
        case AST_NEWLINE:                                               // Caso para o comando 'novalinha'
            break;                                                      // Comandos que não exigem checagem de tipos
        case AST_RETURN:                                                // Caso para o comando 'retorne'
            if (!ctx->in_function) {                                    // Verifica se o retorno está dentro de uma função
                semantic_error(ctx, cmd->line, "RETORNO FORA DE FUNCAO"); // Reporta erro de retorno em escopo global
                break;                                                  // Interrompe a verificação do retorno
            }
            if (cmd->child_count == 0) {                                // Verifica se o comando possui expressão de retorno
                semantic_error(ctx, cmd->line, "RETORNO SEM EXPRESSAO"); // Reporta erro se faltar o valor de retorno
                break;                                                  // Aborta análise deste comando
            }
            ret = infer_expr(ctx, cmd->children);                    // Determina o tipo do valor que está sendo retornado
            if (ctx->has_error) {                                       // Checa erros na expressão que compõe o retorno
                break;                                                  // Para a análise se a expressão for inválida
            }
                                                                        // Valida se o tipo retornado condiz com a assinatura
            if (ret.is_vector || ret.type != ctx->current_return_type) {
                semantic_error(ctx, cmd->line, "TIPO DE RETORNO INCOMPATIVEL"); // Reporta erro de tipo de retorno
            }
            break;                                                      // Finaliza validação do retorno
        case AST_IF:                                                    // Caso para comando condicional SE
        case AST_IF_ELSE:                                               // Caso para comando condicional SE-SENAO
            cond = infer_expr(ctx, cmd->children);                   // Avalia a expressão dentro dos parênteses do SE
            if (ctx->has_error) {                                       // Verifica se a condição é válida
                break;                                                  // Aborta se a expressão de controle falhar
            }
            if (cond.is_vector || cond.type != TYPE_INT) {              // Condições exigem escalar do tipo inteiro
                semantic_error(ctx, cmd->line, "EXPRESSAO DE CONTROLE DEVE SER INT"); // Reporta erro de tipo booleano
                break;                                                  // Interrompe análise do bloco condicional
            }
            analyze_command(ctx, cmd->children[1]);                     // Analisa o comando/bloco do braço 'ENTAO'
                                                                        // Se for um IF-ELSE, analisa o terceiro filho (braço SENAO)
            if (!ctx->has_error && cmd->kind == AST_IF_ELSE && cmd->child_count > 2) {
                analyze_command(ctx, cmd->children[2]);                 // Realiza a análise semântica do braço SENAO
            }
            break;                                                      // Finaliza análise da estrutura condicional
        case AST_WHILE:                                                 // Caso para o laço de repetição ENQUANTO
            cond = infer_expr(ctx, cmd->children);                   // Avalia a expressão de controle do laço
            if (ctx->has_error) {                                       // Checa validade da expressão de controle
                break;                                                  // Aborta análise do laço em caso de erro
            }
            if (cond.is_vector || cond.type != TYPE_INT) {              // O controle do laço deve resultar em um inteiro escalar
                semantic_error(ctx, cmd->line, "EXPRESSAO DE CONTROLE DEVE SER INT"); // Reporta erro de controle inválido
                break;                                                  // Interrompe processamento do laço
            }
            analyze_command(ctx, cmd->children[1]);                     // Analisa recursivamente o corpo do comando enquanto
            break;                                                      // Finaliza análise do laço
        case AST_BLOCK:                                                 // Caso o comando seja um sub-bloco delimitado
            analyze_block(ctx, cmd, 0);                                 // Chama análise de bloco sem forçar novo escopo aqui
            break;                                                      // Finaliza análise do bloco
        default:                                                        // Caso encontre um tipo de nó não esperado
            break;                                                      // Ignora e continua o processamento
    }
}
/* Percorre e analisa todas as funções declaradas na seção de funções do programa. */
/* Tipo void para processamento iterativo; ctx é o contexto global e section contém a lista de funções da AST. */
static void analyze_function_section(SemanticContext *ctx, ASTNode *section) {
    size_t i;                                                           // Índice para percorrer a lista de funções

    if (!section || section->kind != AST_FUNCTION_SECTION) {            // Valida se o nó da secção existe e é correto
        return;                                                         // Aborta caso o nó seja nulo ou inválido
    }
    for (i = 0; i < section->child_count; ++i) {                        // Itera sobre cada filho (declaração de função)
        analyze_function(ctx, section->children[i]);                    // Executa a análise semântica da função atual
        if (ctx->has_error) {                                           // Verifica se a análise da função gerou erro
            break;                                                      // Interrompe o processamento em caso de falha
        }
    }
}

/* Processa as declarações de variáveis globais, inserindo-as no escopo base da tabela. */
/* Tipo void pois registra os símbolos globais; ctx provê o estado e section é o nó que contém as declarações iniciais. */
static void analyze_global_section(SemanticContext *ctx, ASTNode *section) {
    if (!section || section->kind != AST_GLOBAL_SECTION) {              // Verifica se a secção global é válida
        return;                                                         // Ignora se o nó não for uma secção global
    }
    if (section->child_count > 0) {                                     // Checa se existem variáveis declaradas
                                                                        // Tenta declarar os itens globais na tabela
        (void)declare_decl_items(ctx, section->children, 1);
    }
}

int semantic_check(ASTNode *root) {
    SemanticContext ctx;                                                // Declara o contexto local da análise
    size_t i;                                                           // Variável para iteração sobre as secções

    if (!root || root->kind != AST_PROGRAM) {                           // Valida a integridade da raiz da árvore
        fprintf(stderr, "ERRO: AST INVALIDA LINHA 0\n");                // Reporta erro crítico se a AST for nula
        return 0;                                                       // Retorna falha imediata na verificação
    }

    symtab_init(&ctx.table);                                            // Prepara a pilha da tabela de símbolos
    ctx.has_error = 0;                                                  // Inicializa a flag de erro como falsa
    ctx.in_function = 0;                                                // Define que a análise começa fora de funções
    ctx.current_return_type = TYPE_INVALID;                             // Reseta o tipo de retorno esperado

    symtab_push_scope(&ctx.table);                                      // Cria o escopo base para variáveis globais

    for (i = 0; i < root->child_count; ++i) {                           // Percorre os ramos principais do programa
        ASTNode *child = root->children[i];                             // Seleciona a secção atual para análise
        if (!child) {                                                   // Verifica se o ponteiro do filho é válido
            continue;                                                   // Salta caso encontre um nó nulo
        }
        if (child->kind == AST_GLOBAL_SECTION) {                        // Identifica secção de variáveis globais
            analyze_global_section(&ctx, child);                        // Processa a declaração de símbolos globais
        } else if (child->kind == AST_FUNCTION_SECTION) {               // Identifica secção de definições de funções
            analyze_function_section(&ctx, child);                      // Analisa as funções declaradas
        } else if (child->kind == AST_BLOCK) {                          // Identifica o bloco principal (principal)
            analyze_block(&ctx, child, 1);                              // Analisa o corpo do programa principal
        }
        if (ctx.has_error) {                                            // Monitoriza se algum erro semântico ocorreu
            break;                                                      // Para o processo ao detetar a primeira falha
        }
    }

    symtab_free(&ctx.table);                                            // Liberta a memória da tabela de símbolos
    return ctx.has_error ? 0 : 1;                                       // Retorna sucesso (1) ou erro (0) da análise
}

/*---------------------codegen.h--------------------*/

#ifndef GV1_CODEGEN_H
#define GV1_CODEGEN_H

#include "ast.h"

int codegen_generate(ASTNode *root, const char *input_path);

#endif

/*---------------------codegen.c--------------------*/

#include "codegen.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol_table.h"                            // Interface para gerenciar símbolos e endereços na memória

typedef struct {                                     // Agrupa o estado necessário para emitir o código assembly
    FILE *out;                                       // Arquivo de destino onde o código MIPS será gravado
    SymbolTableStack table;                          // Pilha de escopos para calcular deslocamentos na pilha
    int var_counter;                                 // Gerador de IDs únicos para variáveis internas temporárias
    int str_counter;                                 // Gerador de índices sequenciais para literais de string
    int label_counter;                               // Gerador de rótulos únicos para saltos de controle (if/while)
    char *data_buf;                                  // Armazena temporariamente as definições da seção .data
    size_t data_buf_len;                             // Comprimento atual dos dados acumulados no buffer de dados
    size_t data_buf_cap;                             // Capacidade total de memória alocada para o buffer de dados
    int text_opened;                                 // Flag que evita a escrita redundante do cabeçalho .text
} CodegenContext;                                    // Estrutura que mantém o contexto global da geração de código

/* Adiciona texto formatado ao buffer temporário da seção de dados (.data). /
/* void: não retorna valor; CodegenContext* ctx: gerencia o estado; char* fmt: formato do texto; ...: valores variáveis. */
static void data_buf_printf(CodegenContext *ctx, const char *fmt, ...) {
    va_list args;                                                       // Declara lista para argumentos variáveis
    va_list args2;                                                      // Declara segunda lista para cópia segura
    int needed;                                                         // Armazena o tamanho necessário do buffer
    char *new_buf;                                                      // Ponteiro para o novo buffer realocado
    size_t new_cap;                                                     // Armazena a nova capacidade calculada


    va_start(args, fmt);                                                // Inicia o processamento dos argumentos
    va_copy(args2, args);                                               // Copia argumentos para uso posterior
    needed = vsnprintf(NULL, 0, fmt, args);                             // Calcula o tamanho da string formatada
    va_end(args);                                                       // Encerra o uso da primeira lista
    if (needed < 0) return;                                             // Aborta se houver erro na formatação

                                                                        // Verifica se o novo texto cabe no buffer
    if (ctx->data_buf_len + (size_t)needed + 1 > ctx->data_buf_cap) {
        new_cap = ctx->data_buf_cap ? ctx->data_buf_cap : 256;          // Define 256 como capacidade inicial
                                                                        // Dobra capacidade até suportar o texto
        while (new_cap < ctx->data_buf_len + (size_t)needed + 1) new_cap *= 2;
        new_buf = (char *)realloc(ctx->data_buf, new_cap);              // Tenta redimensionar o buffer atual
        if (!new_buf) return;                                           // Retorna se a alocação de memória falhar
        ctx->data_buf = new_buf;                                        // Atualiza o ponteiro do buffer no contexto
        ctx->data_buf_cap = new_cap;                                    // Atualiza o registro da capacidade total
    }
                                                                        // Escreve a string no fim do buffer atual
    vsnprintf(ctx->data_buf + ctx->data_buf_len, (size_t)needed + 1, fmt, args2);
    ctx->data_buf_len += (size_t)needed;                                // Incrementa o contador de bytes ocupados
    va_end(args2);                                                      // Finaliza o uso da segunda lista

}

/* Escreve o conteúdo acumulado no buffer de dados para o arquivo assembly final. /
/ void: não retorna valor; CodegenContext* ctx: contém o arquivo de saída e o buffer de dados a ser liberado. */
static void flush_data(CodegenContext ctx) {
                                                                        // Verifica se existem dados para escrever
    if (ctx->data_buf && ctx->data_buf_len > 0) {
        fprintf(ctx->out, ".data\n");                                   // Escreve diretiva da seção de dados
        fwrite(ctx->data_buf, 1, ctx->data_buf_len, ctx->out);          // Grava o conteúdo do buffer no arquivo
        free(ctx->data_buf);                                            // Libera a memória alocada para o buffer
        ctx->data_buf = NULL;                                           // Reseta o ponteiro para nulo
        ctx->data_buf_len = 0;                                          // Zera o contador de tamanho do buffer
        ctx->data_buf_cap = 0;                                          // Zera o registro de capacidade total
    }
}

}
/* Constrói uma string de rótulo única combinando um texto de prefixo e um número sequencial. /
/ char: retorna o rótulo alocado; char* prefix: base do nome; int id: contador para garantir unicidade. */
static char *make_label(const char *prefix, int id) {
    char buffer[64];                                                    // Buffer local para formatação do rótulo
    int size = snprintf(buffer, sizeof(buffer), "%s_%d", prefix, id);   // Gera string formatada (ex: prefix_1)
    char result = (char )malloc((size_t)size + 1);                      // Aloca memória para a string resultante
    if (!result) {                                                      // Valida se a alocação foi bem-sucedida
        return NULL;                                                    // Retorna nulo em caso de falta de memória
    }
    memcpy(result, buffer, (size_t)size + 1);                           // Copia o rótulo do buffer para o heap
    return result;                                                      // Retorna o endereço da nova string
}

/* Converte a representação em string de um caractere literal para seu valor ASCII inteiro. /
/ int: retorna o valor ASCII; char lexeme: a string original contendo as aspas e possíveis escapes. */
static int char_literal_value(const char *lexeme) {
                                                                        // Valida se o literal é nulo ou malformado
    if (!lexeme || lexeme[0] != '\'' || lexeme[1] == '\0') {
        return 0;                                                       // Retorna 0 para entradas inválidas
    }
    if (lexeme[1] == '\') {                                            // Verifica se o caractere possui escape (\)
        switch (lexeme[2]) {                                            // Analisa qual o tipo de escape presente
            case 'n': return '\n';                                      // Retorna o valor de nova linha
            case 't': return '\t';                                      // Retorna o valor de tabulação
            case 'r': return '\r';                                      // Retorna o valor de retorno de carro
            case '\': return '\';                                     // Retorna o valor da barra invertida
            case ''': return ''';                                     // Retorna o valor da aspa simples
            case '0': return '\0';                                      // Retorna o valor do caractere nulo
            default: return (unsigned char)lexeme[2];                   // Retorna o próprio caractere após a barra
        }
    }
    return (unsigned char)lexeme[1];                                    // Retorna o valor ASCII do caractere puro
}
/* Emite a diretiva de início de código (.text) no assembly, caso ainda não tenha sido feita. */
/* void: não retorna valor; CodegenContext* ctx: monitora se a seção de texto já foi aberta no arquivo. */
static void ensure_text(CodegenContext *ctx) {
    if (!ctx->text_opened) {                                            // Verifica se a diretiva .text já foi emitida
        fprintf(ctx->out, ".text\n");                                   // Escreve o marcador de início de código no ficheiro
        ctx->text_opened = 1;                                           // Atualiza o estado para evitar múltiplas aberturas
    }
}

/* Busca e retorna o nó contendo a lista de declarações de um bloco da AST. */
/* ASTNode*: retorna o nó de declarações ou NULL; ASTNode* block: o nó de bloco a ser verificado. */
static ASTNode *block_decl_list(ASTNode *block) {
                                                                        // Valida a integridade do nó e se é um bloco válido
    if (!block || block->kind != AST_BLOCK || block->child_count == 0) {
        return NULL;                                                    // Retorna nulo se o bloco não tiver filhos
    }
                                                                        // Checa se o primeiro filho é a lista de declarações
    return block->children->kind == AST_DECL_LIST ? block->children : NULL;
}

/* Busca e retorna o nó contendo a lista de comandos de um bloco da AST. */
/* ASTNode*: retorna o nó de comandos ou NULL; ASTNode* block: o nó de bloco a ser verificado. */
static ASTNode *block_cmd_list(ASTNode *block) {
                                                                        // Verifica se o nó existe e corresponde a um bloco
    if (!block || block->kind != AST_BLOCK || block->child_count == 0) {
        return NULL;                                                    // Retorna nulo para blocos nulos ou sem conteúdo
    }
    if (block->children->kind == AST_DECL_LIST) {                    // Identifica se o primeiro ramo contém declarações
                                                                        // Se houver declarações, os comandos são o segundo ramo
        return block->child_count > 1 ? block->children[1] : NULL;
    }
    return block->children;                                          // Sem declarações, comandos ocupam o primeiro ramo
}

/* Processa declarações de variáveis, gerando espaço em memória e atualizando a tabela de símbolos. */
/* void: não retorna valor; CodegenContext* ctx: estado da geração; ASTNode* decls: subárvore de declarações. */
static void declare_decl_items(CodegenContext *ctx, ASTNode *decls) {
    size_t i;                                                           // Variável de controle para iterar sobre a lista

    if (!decls || decls->kind != AST_DECL_LIST) {                       // Confirma se o nó recebido é de fato uma lista
        return;                                                         // Aborta a função se a subárvore for inválida
    }
    for (i = 0; i < decls->child_count; ++i) {                          // Percorre cada item declarado individualmente
        ASTNode *decl = decls->children[i];                             // Recupera o nó do item de declaração atual
        char *label;                                                    // Armazena o rótulo assembly único da variável
        if (!decl || decl->kind != AST_DECL_ITEM) {                     // Valida se o filho é uma declaração de item
            continue;                                                   // Ignora nós malformados e prossegue
        }
                                                                        // Gera nome de rótulo único incrementando o contador
        label = make_label("gv2_var", ctx->var_counter++);
        if (!label) {                                                   // Verifica falha na geração/alocação do rótulo
            continue;                                                   // Passa para o próximo caso ocorra erro
        }
        if (decl->is_vector) {                                          // Verifica se o item declarado é do tipo vetor
                                                                        // Calcula bytes totais (tamanho * 4 bytes/word)
            int bytes = (decl->vector_size > 0 ? decl->vector_size : 1) * 4;
                                                                        // Reserva bloco de memória na secção de dados
            data_buf_printf(ctx, "%s: .space %d\n", label, bytes);
        } else {                                                        // Caso para declaração de variável escalar simples
            data_buf_printf(ctx, "%s: .word 0\n", label);               // Aloca uma word (4 bytes) inicializada com zero
        }
                                                                        // Insere os metadados da variável na tabela de símbolos
        (void)symtab_declare_var(&ctx->table, decl->lexeme, decl->type, decl->is_vector, decl->vector_size, label);
        free(label);                                                    // Liberta a memória da string do rótulo temporário
    }
}

static void emit_expr(CodegenContext *ctx, ASTNode *node);
static void emit_command(CodegenContext *ctx, ASTNode *cmd);
/* Gera código MIPS para calcular o endereço efetivo de um elemento de vetor e colocá-lo em $a0. */
/* void: não retorna valor; CodegenContext* ctx: estado da geração; ASTNode* node: nó de acesso ao array. */

static void emit_array_access_address(CodegenContext *ctx, ASTNode *node) {
    Symbol *sym;                                                        // Declara ponteiro para o símbolo do vetor

                                                                        // Valida se o nó é um acesso a array com filhos
    if (!node || node->kind != AST_ARRAY_ACCESS || node->child_count < 2) {
        fprintf(ctx->out, "move $v0, $zero\n");                         // Se inválido, carrega endereço nulo em $v0
        return;                                                         // Interrompe a geração para este nó
    }
                                                                        // Procura o identificador do vetor na tabela
    sym = symtab_lookup(&ctx->table, node->children->lexeme);
    emit_expr(ctx, node->children[1]);                                   // Gera código para avaliar a expressão do índice
    fprintf(ctx->out, "move $t4, $v0\n");                               // Move o resultado do índice para o registador $t4
    if (sym && sym->label) {                                            // Verifica se o símbolo existe e possui rótulo
        if (sym->kind == SYM_PARAM && sym->is_vector) {                 // Caso o vetor seja um parâmetro (referência)
            fprintf(ctx->out, "lw $t5, %s\n", sym->label);              // Carrega o endereço base guardado no parâmetro
        } else {                                                        // Caso seja uma variável global ou local direta
            fprintf(ctx->out, "la $t5, %s\n", sym->label);              // Carrega o endereço inicial do rótulo em $t5
        }
        fprintf(ctx->out, "sll $t4, $t4, 2\n");                         // Multiplica o índice por 4 (offset de word)
        fprintf(ctx->out, "addu $v0, $t5, $t4\n");                      // Soma base e deslocamento, guarda em $v0
    } else {                                                            // Caso o identificador não tenha sido encontrado
        fprintf(ctx->out, "move $v0, $zero\n");                         // Define o endereço resultante como zero
    }
}

/* Emite instruções assembly para avaliar uma expressão, armazenando o resultado final no acumulador $s0. */
/* void: não retorna valor; CodegenContext* ctx: estado da geração; ASTNode* node: subárvore da expressão. */
static void emit_expr(CodegenContext *ctx, ASTNode *node) {
    Symbol *sym;                                                        // Ponteiro para metadados de identificadores
    char *l1;                                                           // Rótulo para literais de string
    char *l2;                                                           // Rótulo auxiliar (não utilizado neste trecho)

    if (!node) {                                                        // Verifica se o nó da expressão é nulo
        fprintf(ctx->out, "li $v0, 0\n");                               // Carrega valor padrão zero em $v0
        return;                                                         // Finaliza processamento do nó nulo
    }

    switch (node->kind) {                                               // Seleciona a ação baseada no tipo de nó AST
        case AST_IDENTIFIER:                                            // Caso o nó seja um identificador simples
                                                                        // Procura o nome da variável na tabela atual
            sym = symtab_lookup(&ctx->table, node->lexeme);
            if (sym && sym->label) {                                    // Se encontrado e com rótulo assembly válido
                if (sym->is_vector) {                                   // Se o identificador referir-se a um vetor
                    if (sym->kind == SYM_PARAM) {                       // Se for um vetor passado como parâmetro
                        fprintf(ctx->out, "lw $v0, %s\n", sym->label);  // Carrega o endereço base que o parâmetro contém
                    } else {                                            // Se for um vetor local ou global fixo
                        fprintf(ctx->out, "la $v0, %s\n", sym->label);  // Carrega o endereço inicial do rótulo
                    }
                } else {                                                // Caso seja uma variável escalar simples
                    fprintf(ctx->out, "lw $v0, %s\n", sym->label);      // Carrega o valor da variável para $v0
                }
            } else {                                                    // Caso o símbolo não esteja definido
                fprintf(ctx->out, "li $v0, 0\n");                       // Carrega zero por segurança/erro
            }
            break;                                                      // Finaliza tratamento de identificador
        case AST_ARRAY_ACCESS:                                          // Caso seja acesso indexado: arr[expr]
            emit_array_access_address(ctx, node);                       // Obtém o endereço do elemento e coloca em $v0
            fprintf(ctx->out, "lw $v0, 0($v0)\n");                      // Carrega o valor contido nesse endereço
            break;                                                      // Finaliza acesso a array
        case AST_INT_LITERAL:                                           // Caso seja uma constante inteira
                                                                        // Carrega o valor imediato do lexema em $v0
            fprintf(ctx->out, "li $v0, %s\n", node->lexeme ? node->lexeme : "0");
            break;                                                      // Finaliza literal inteiro
        case AST_CHAR_LITERAL:                                          // Caso seja um carácter (ex: 'a')
                                                                        // Converte o carácter e carrega o valor ASCII
            fprintf(ctx->out, "li $v0, %d\n", char_literal_value(node->lexeme));
            break;                                                      // Finaliza literal de carácter
        case AST_STRING_LITERAL:                                        // Caso seja uma string literal ("...")
                                                                        // Cria rótulo único para a string na seção .data
            l1 = make_label("gv2_str", ctx->str_counter++);
                                                                        // Define a string no buffer de dados do assembly
            data_buf_printf(ctx, "%s: .asciiz %s\n", l1, node->lexeme ? node->lexeme : "\"\"");
            ensure_text(ctx);                                           // Garante o retorno à seção de código (.text)
            fprintf(ctx->out, "la $v0, %s\n", l1);                      // Carrega o endereço da string em $v0
            free(l1);                                                   // Liberta a memória da string do rótulo
            break;                           
        case AST_FUNC_CALL: {                                           // Início do tratamento de chamada de função
            size_t arg_count = 0;                                       // Variável para contar o número de argumentos
            size_t ai;                                                  // Índice para iterar sobre a lista de argumentos
                                                                        // Verifica se existem argumentos na chamada
            if (node->child_count > 0 && node->children && node->children->kind == AST_EXPR_LIST) {
                arg_count = node->children->child_count;             // Obtém a quantidade total de argumentos
            }
            if (arg_count > 0) {                                        // Caso existam argumentos, prepara a pilha
                                                                        // Aloca espaço na pilha (decrementa $sp) 
                                                                        // multiplicando o n.º de argumentos por 4 bytes
                fprintf(ctx->out, "addiu $sp, $sp, %d\n", (int)(-((int)arg_count * 4)));
                for (ai = 0; ai < arg_count; ++ai) {                    // Percorre cada argumento da lista
                    emit_expr(ctx, node->children->children[ai]);    // Gera código para avaliar o argumento atual
                    fprintf(ctx->out, "sw $v0, %d($sp)\n", (int)(ai * 4)); // Salva o valor resultante ($v0) na pilha
                }
            }
                                                                        // Realiza o salto jal para o rótulo da função
            fprintf(ctx->out, "jal %s\n", node->lexeme ? node->lexeme : "func");
            if (arg_count > 0) {                                        // Após o retorno, limpa os argumentos da pilha
                fprintf(ctx->out, "addiu $sp, $sp, %d\n", (int)arg_count * 4); // Desaloca o espaço dos parâmetros (soma $sp)
            }
            break;                                                      // Finaliza tratamento de chamada de função
        }
        case AST_ASSIGN:                                                // Início do tratamento de comando de atribuição
            if (node->children->kind == AST_ARRAY_ACCESS) {          // Verifica se o destino é um elemento de vetor
                emit_array_access_address(ctx, node->children);      // Obtém o endereço do elemento e guarda em $v0
                fprintf(ctx->out, "move $t2, $v0\n");                   // Move o endereço destino para $t2
                emit_expr(ctx, node->children[1]);                      // Gera código para avaliar o valor a ser atribuído
                fprintf(ctx->out, "sw $v0, 0($t2)\n");                  // Grava o valor ($v0) no endereço apontado por $t2
            } else {                                                    // Caso o destino seja uma variável escalar
                emit_expr(ctx, node->children[1]);                      // Avalia a expressão do lado direito
                                                                        // Procura o rótulo da variável na tabela
                sym = symtab_lookup(&ctx->table, node->children->lexeme);
                if (sym && sym->label) {                                // Se a variável for encontrada com um rótulo válido
                    fprintf(ctx->out, "sw $v0, %s\n", sym->label);      // Armazena o resultado direto na memória da variável
                }
            }
            break;                                                      // Finaliza tratamento de atribuição
        case AST_NEG:                                                   // Início do tratamento de negação aritmética (-)
            emit_expr(ctx, node->children);                          // Avalia o operando da negação
            fprintf(ctx->out, "subu $v0, $zero, $v0\n");                // Subtrai de zero ($v0 = 0 - $v0) para inverter sinal
            break;                                                      // Finaliza negação aritmética
        case AST_NOT:                                                   // Início do tratamento de negação lógica (!)
            emit_expr(ctx, node->children);                          // Avalia o operando booleano
            fprintf(ctx->out, "seq $v0, $v0, $zero\n");                 // $v0 = 1 se operando era 0 (falso), senão 0
            break;                                                      // Finaliza negação lógica
        case AST_OR:                                                    // Início do tratamento de operação OU lógico
            emit_expr(ctx, node->children);                          // Avalia o primeiro operando (esquerda)
            fprintf(ctx->out, "sne $t0, $v0, $zero\n");                 // Normaliza: $t0 = 1 se $v0 != 0, senão 0
            fprintf(ctx->out, "addiu $sp, $sp, -4\n");                  // Reserva espaço na pilha para o operando 1
            fprintf(ctx->out, "sw $t0, 0($sp)\n");                      // Empilha o resultado booleano do 1º operando
            emit_expr(ctx, node->children[1]);                          // Avalia o segundo operando (direita)
            fprintf(ctx->out, "sne $t1, $v0, $zero\n");                 // Normaliza: $t1 = 1 se $v0 != 0, senão 0
            fprintf(ctx->out, "lw $t0, 0($sp)\n");                      // Recupera o valor do 1º operando da pilha
            fprintf(ctx->out, "addiu $sp, $sp, 4\n");                   // Restaura o ponteiro da pilha ($sp)
            fprintf(ctx->out, "or $v0, $t0, $t1\n");                    // Realiza a operação OR bit a bit entre os booleanos
            break;                                                      // Finaliza operação OR
        case AST_AND:                                                   // Início do tratamento de operação E lógico
            emit_expr(ctx, node->children);                          // Avalia o primeiro operando (esquerda)
            fprintf(ctx->out, "sne $t0, $v0, $zero\n");                 // Normaliza: $t0 = 1 se $v0 != 0, senão 0
            fprintf(ctx->out, "addiu $sp, $sp, -4\n");                  // Aloca espaço na pilha para preservação
            fprintf(ctx->out, "sw $t0, 0($sp)\n");                      // Salva o booleano do 1º operando na pilha
            emit_expr(ctx, node->children[1]);                          // Avalia o segundo operando (direita)
            fprintf(ctx->out, "sne $t1, $v0, $zero\n");                 // Normaliza: $t1 = 1 se $v0 != 0, senão 0
            fprintf(ctx->out, "lw $t0, 0($sp)\n");                      // Retira o resultado do 1º operando da pilha
            fprintf(ctx->out, "addiu $sp, $sp, 4\n");                   // Desaloca o espaço da pilha utilizado
            fprintf(ctx->out, "and $v0, $t0, $t1\n");                   // Realiza a operação AND bit a bit entre eles
            break;                                                      // Finaliza operação AND
        case AST_ADD:                                                   // Início do caso para operação de soma (+)
        case AST_SUB:                                                   // Início do caso para operação de subtração (-)
        case AST_MUL:                                                   // Início do caso para operação de multiplicação (*)
        case AST_DIV:                                                   // Início do caso para operação de divisão (/)
        case AST_EQ:                                                    // Início do caso para comparação: igual (==)
        case AST_NE:                                                    // Início do caso para comparação: diferente (!=)
        case AST_LT:                                                    // Início do caso para comparação: menor que (<)
        case AST_GT:                                                    // Início do caso para comparação: maior que (>)
        case AST_GE:                                                    // Início do caso para comparação: maior igual (>=)
        case AST_LE:                                                    // Início do caso para comparação: menor igual (<=)
            emit_expr(ctx, node->children);                          // Avalia o operando esquerdo e guarda em $v0
            fprintf(ctx->out, "move $t0, $v0\n");                       // Move o resultado da esquerda para o registo $t0
            fprintf(ctx->out, "addiu $sp, $sp, -4\n");                  // Aloca espaço na pilha para o operando
            fprintf(ctx->out, "sw $t0, 0($sp)\n");                      // Salva o valor da esquerda na pilha
            emit_expr(ctx, node->children[1]);                          // Avalia o operando direito e guarda em $v0
            fprintf(ctx->out, "lw $t0, 0($sp)\n");                      // Recupera o valor da esquerda da pilha para $t0
            fprintf(ctx->out, "addiu $sp, $sp, 4\n");                   // Liberta o espaço da pilha (restaura $sp)
            switch (node->kind) {                                       // Seleciona instrução com base no tipo de nó
                case AST_ADD: fprintf(ctx->out, "addu $v0, $t0, $v0\n"); break; // Soma sem overflow ($v0 = $t0 + $v0)
                case AST_SUB: fprintf(ctx->out, "subu $v0, $t0, $v0\n"); break; // Subtrai sem overflow ($v0 = $t0 - $v0)
                case AST_MUL: fprintf(ctx->out, "mul $v0, $t0, $v0\n"); break;  // Multiplica os valores ($v0 = $t0 * $v0)
                                                                        // Divide os valores e move o quociente (mflo)
                                                                        // para o registo de resultado $v0
                case AST_DIV: fprintf(ctx->out, "div $t0, $v0\nmflo $v0\n"); break; 
                case AST_EQ: fprintf(ctx->out, "seq $v0, $t0, $v0\n"); break;   // $v0 = 1 se forem iguais, senão 0
                case AST_NE: fprintf(ctx->out, "sne $v0, $t0, $v0\n"); break;   // $v0 = 1 se forem diferentes, senão 0
                case AST_LT: fprintf(ctx->out, "slt $v0, $t0, $v0\n"); break;   // $v0 = 1 se esquerda < direita, senão 0
                case AST_GT: fprintf(ctx->out, "sgt $v0, $t0, $v0\n"); break;   // $v0 = 1 se esquerda > direita, senão 0
                case AST_GE: fprintf(ctx->out, "sge $v0, $t0, $v0\n"); break;   // $v0 = 1 se esquerda >= direita, senão 0
                case AST_LE: fprintf(ctx->out, "sle $v0, $t0, $v0\n"); break;   // $v0 = 1 se esquerda <= direita, senão 0
                default: break;                                         // Caso padrão por integridade
            }
            break;                                                      // Finaliza o processamento binário
        default:                                                        // Caso para literais ou booleanos simples
            l1 = make_label("gv2_true", ctx->label_counter++);          // Gera um rótulo para o ramo verdadeiro
            l2 = make_label("gv2_end", ctx->label_counter++);           // Gera um rótulo para o ponto de saída
            emit_expr(ctx, node->children);                          // Avalia a expressão que serve de condição
            fprintf(ctx->out, "bne $v0, $zero, %s\n", l1);              // Salta para o rótulo 'true' se $v0 for != 0
            fprintf(ctx->out, "li $v0, 0\n");                           // Carrega 0 (falso) no acumulador
            fprintf(ctx->out, "j %s\n", l2);                            // Salta incondicionalmente para o fim
            fprintf(ctx->out, "%s:\n", l1);                             // Imprime a definição do rótulo verdadeiro
            fprintf(ctx->out, "li $v0, 1\n");                           // Carrega 1 (verdadeiro) no acumulador
            fprintf(ctx->out, "%s:\n", l2);                             // Imprime a definição do rótulo final
            free(l1);                                                   // Liberta a memória da string do rótulo 1
            free(l2);                                                   // Liberta a memória da string do rótulo 2
            break;                                                      // Encerra o tratamento padrão da expressão
    }
}

/* Traduz um bloco da AST (declarações e comandos) para MIPS, gerenciando o escopo se necessário. */
/* static void: sem retorno; CodegenContext* ctx: contexto da geração; ASTNode* block: nó do bloco; int push_scope: flag para criar novo escopo. */
static void emit_block(CodegenContext *ctx, ASTNode *block, int push_scope) {
    ASTNode *decls;                                                     // Ponteiro para o nó da lista de declarações
    ASTNode *cmds;                                                      // Ponteiro para o nó da lista de comandos
    size_t i;                                                           // Índice para percorrer a lista de comandos

    if (!block || block->kind != AST_BLOCK) {                           // Valida se o nó existe e é um bloco
        return;                                                         // Aborta caso o nó seja nulo ou inválido
    }

    if (push_scope) {                                                   // Verifica se o bloco exige abertura de escopo
        symtab_push_scope(&ctx->table);                                 // Cria um novo nível na tabela de símbolos
    }
    decls = block_decl_list(block);                                     // Recupera as declarações locais do bloco
    cmds = block_cmd_list(block);                                       // Recupera a sequência de comandos do bloco
    declare_decl_items(ctx, decls);                                     // Gera espaço e símbolos para as declarações
    if (cmds && cmds->kind == AST_CMD_LIST) {                           // Verifica se existe uma lista de comandos válida
        for (i = 0; i < cmds->child_count; ++i) {                       // Itera sobre cada comando individual da lista
            emit_command(ctx, cmds->children[i]);                       // Gera código assembly para o comando atual
        }
    }
    if (push_scope) {                                                   // Verifica se o escopo deve ser fechado
        symtab_pop_scope(&ctx->table);                                  // Remove o nível atual da tabela de símbolos
    }
}
/* Direciona a geração de código para diferentes tipos de comandos como SE, ENQUANTO e atribuições. */
/* static void: sem retorno; CodegenContext* ctx: contexto da geração; ASTNode* cmd: nó do comando a ser processado. */
static void emit_command(CodegenContext *ctx, ASTNode *cmd) {
    Symbol *sym;                                                        // Ponteiro para metadados de símbolos
    char *l1;                                                           // Ponteiro para string de rótulo temporário
    char *l2;                                                           // Segundo ponteiro para rótulo auxiliar

    if (!cmd) {                                                         // Verifica se o nó do comando é nulo
        return;                                                         // Retorna imediatamente se não houver comando
    }

    switch (cmd->kind) {                                                // Seleciona a ação pelo tipo de nó da AST
        case AST_EMPTY_STMT:                                            // Caso de comando vazio (apenas ';')
            break;                                                      // Nenhuma instrução é gerada
        case AST_EXPR_STMT:                                             // Caso de comando composto por expressão
            emit_expr(ctx, cmd->children);                           // Gera código para avaliar a expressão
            break;                                                      // Finaliza tratamento de instrução de expressão
        case AST_READ:                                                  // Início do tratamento do comando 'leia'
                                                                        // Verifica se o alvo da leitura é um array
            if (cmd->child_count > 0 && cmd->children->kind == AST_ARRAY_ACCESS) {
                emit_array_access_address(ctx, cmd->children);       // Obtém endereço do elemento em $v0
                fprintf(ctx->out, "move $t2, $v0\n");                   // Salva o endereço destino no registrador $t2
                fprintf(ctx->out, "li $v0, 5\nsyscall\n");              // Syscall 5: lê inteiro do teclado para $v0
                fprintf(ctx->out, "sw $v0, 0($t2)\n");                  // Armazena o valor lido no endereço em $t2
            } else {                                                    // Caso o alvo seja uma variável simples
                                                                        // Procura a variável na tabela de símbolos
                sym = symtab_lookup(&ctx->table, cmd->children->lexeme);
                if (sym && sym->label) {                                // Se a variável existe e tem rótulo válido
                    if (sym->type == TYPE_CAR) {                        // Verifica se o tipo da variável é caractere
                        fprintf(ctx->out, "li $v0, 12\nsyscall\n");     // Syscall 12: lê um único caractere para $v0
                    } else {                                            // Caso contrário, trata como inteiro
                        fprintf(ctx->out, "li $v0, 5\nsyscall\n");      // Syscall 5: lê um número inteiro para $v0
                    }
                    fprintf(ctx->out, "sw $v0, %s\n", sym->label);      // Salva o valor lido na memória da variável
                }
            }
            break;                                                      // Finaliza tratamento de leitura
        case AST_WRITE_EXPR:                                            // Caso de comando 'escreva' com expressão
            emit_expr(ctx, cmd->children);                           // Avalia a expressão e deixa o valor em $v0
            if (cmd->children && cmd->children->type == TYPE_CAR) { // Checa se o tipo resultante é caractere
                                                                        // Syscall 11: imprime o caractere em $a0
                fprintf(ctx->out, "move $a0, $v0\nli $v0, 11\nsyscall\n");
            } else {                                                    // Caso o resultado seja do tipo inteiro
                                                                        // Syscall 1: imprime o valor inteiro em $a0
                fprintf(ctx->out, "move $a0, $v0\nli $v0, 1\nsyscall\n");
            }
            break;                                                      // Finaliza tratamento de escrita de expressão
        case AST_WRITE_STR:                                             // Caso de 'escreva' com string literal
            l1 = make_label("gv2_str", ctx->str_counter++);             // Cria um rótulo único para a string literal
                                                                        // Armazena a string no buffer da seção .data
            data_buf_printf(ctx, "%s: .asciiz %s\n", l1, cmd->lexeme ? cmd->lexeme : "\"\"");
            ensure_text(ctx);                                           // Garante retorno à seção de código .text
                                                                        // Syscall 4: imprime string a partir de $a0
            fprintf(ctx->out, "la $a0, %s\nli $v0, 4\nsyscall\n", l1);
            free(l1);                                                   // Libera memória do rótulo temporário
            break;                                                      // Finaliza tratamento de escrita de string
        case AST_NEWLINE:                                               // Caso do comando 'novalinha'
            ensure_text(ctx);                                           // Certifica-se de estar na seção .text
                                                                        // Syscall 11: imprime o caractere '\n' (ASCII 10)
            fprintf(ctx->out, "li $a0, 10\nli $v0, 11\nsyscall\n");
            break;                                                      // Finaliza tratamento de nova linha
        case AST_RETURN:                                                // Caso do comando de retorno de função
            if (cmd->child_count > 0) {                                 // Verifica se há um valor de retorno opcional
                emit_expr(ctx, cmd->children);                       // Avalia o valor e o coloca no acumulador $v0
            }
            fprintf(ctx->out, "jr $ra\n");                              // Salta para o endereço de retorno em $ra
            break;                                                      // Finaliza tratamento de retorno
        case AST_IF:                                                    // Caso do comando condicional 'se' simples
            l1 = make_label("gv2_if_end", ctx->label_counter++);        // Gera rótulo para pular o corpo do IF
            emit_expr(ctx, cmd->children);                           // Avalia a expressão da condição lógica
            fprintf(ctx->out, "beq $v0, $zero, %s\n", l1);              // Se $v0 for 0 (falso), desvia para o fim
            emit_command(ctx, cmd->children[1]);                        // Gera código para o comando dentro do IF
            fprintf(ctx->out, "%s:\n", l1);                             // Define o rótulo de destino do desvio (fim)
            free(l1);                                                   // Libera a memória da string do rótulo
            break;                                                      // Finaliza tratamento de comando condicional
        case AST_IF_ELSE:                                               // Início do tratamento de condicional SE-SENAO
            l1 = make_label("gv2_else", ctx->label_counter++);          // Gera rótulo para o início do bloco SENAO
            l2 = make_label("gv2_if_end", ctx->label_counter++);        // Gera rótulo para o fim de toda a estrutura SE
            emit_expr(ctx, cmd->children);                           // Avalia a expressão condicional no assembly
            fprintf(ctx->out, "beq $v0, $zero, %s\n", l1);              // Salta para o SENAO se a condição for zero (falsa)
            emit_command(ctx, cmd->children[1]);                        // Gera código para os comandos do braço ENTAO
            fprintf(ctx->out, "j %s\n", l2);                            // Salta para o fim para evitar o bloco SENAO
            fprintf(ctx->out, "%s:\n", l1);                             // Emite a definição do rótulo para o bloco SENAO
            emit_command(ctx, cmd->children[2]);                        // Gera código para os comandos do braço SENAO
            fprintf(ctx->out, "%s:\n", l2);                             // Emite a definição do rótulo de encerramento
            free(l1);                                                   // Liberta a memória do primeiro rótulo
            free(l2);                                                   // Liberta a memória do segundo rótulo
            break;                                                      // Finaliza tratamento de SE-SENAO
        case AST_WHILE:                                                 // Início do tratamento do laço ENQUANTO
            l1 = make_label("gv2_while_begin", ctx->label_counter++);   // Cria rótulo para marcar o início do laço
            l2 = make_label("gv2_while_end", ctx->label_counter++);     // Cria rótulo para marcar o fim do laço
            fprintf(ctx->out, "%s:\n", l1);                             // Define o ponto de retorno para repetição
            emit_expr(ctx, cmd->children);                           // Avalia a condição de permanência no laço
            fprintf(ctx->out, "beq $v0, $zero, %s\n", l2);              // Sai do laço se a condição for falsa (zero)
            emit_command(ctx, cmd->children[1]);                        // Gera código para o corpo de comandos do laço
            fprintf(ctx->out, "j %s\n", l1);                            // Salta de volta para reavaliar a condição
            fprintf(ctx->out, "%s:\n", l2);                             // Define o ponto de saída do laço no assembly
            free(l1);                                                   // Desaloca o rótulo de início
            free(l2);                                                   // Desaloca o rótulo de fim
            break;                                                      // Finaliza tratamento de ENQUANTO
        case AST_BLOCK:                                                 // Caso o comando seja um bloco aninhado
            emit_block(ctx, cmd, 1);                                    // Processa o bloco forçando novo escopo local
            break;                                                      // Finaliza tratamento de bloco
        default:                                                        // Caso padrão para nós não reconhecidos
            break;                                                      // Ignora sem realizar operações
    }
}

/* Registra os parâmetros formais de uma função na tabela de símbolos para cálculo de deslocamentos. */
/* static void: sem retorno; CodegenContext* ctx: contexto da geração; ASTNode* params: nó com a lista de parâmetros. */
static void emit_function_params(CodegenContext *ctx, ASTNode *params) {
    size_t i;                                                           // Índice para percorrer a lista de parâmetros

    if (!params || params->kind != AST_PARAM_LIST) {                    // Valida se existe uma lista de parâmetros válida
        return;                                                         // Aborta se não houver parâmetros a processar
    }
    for (i = 0; i < params->child_count; ++i) {                         // Itera sobre cada parâmetro da assinatura
        ASTNode *p = params->children[i];                               // Recupera o nó do parâmetro individual
        char *label;                                                    // Armazena o rótulo único para o parâmetro
        if (!p || p->kind != AST_PARAM) {                               // Valida se o nó é um parâmetro válido
            continue;                                                   // Ignora itens inválidos na lista
        }
                                                                        // Cria nome de rótulo assembly para o parâmetro
        label = make_label("gv2_param", ctx->var_counter++);
        if (!label) {                                                   // Verifica se a alocação do rótulo falhou
            continue;                                                   // Avança para o próximo se houver erro
        }
        data_buf_printf(ctx, "%s: .word 0\n", label);                   // Aloca espaço (word) no buffer de dados
                                                                        // Insere metadados do parâmetro na tabela
        (void)symtab_declare_param(&ctx->table, p->lexeme, p->type, p->is_vector, p->vector_size);
        {
                                                                        // Recupera o símbolo recém-criado na tabela
            Symbol *s = symtab_lookup(&ctx->table, p->lexeme);
            if (s) {                                                    // Se o símbolo foi encontrado com sucesso
                free(s->label);                                         // Liberta rótulos genéricos pré-existentes
                s->label = label;                                       // Associa o novo rótulo assembly ao símbolo
                label = NULL;                                           // Anula ponteiro para não libertar label em uso
            }
        }
        free(label);                                                    // Liberta memória se label não foi aproveitado
    }
}
/* Gera o código completo de uma função, incluindo rótulo, prólogo, corpo e epílogo de retorno. */
/* static void: sem retorno; ASTNode* func: nó da declaração da função; CodegenContext* ctx: contexto da geração. */
static void emit_function(ASTNode *func, CodegenContext *ctx) {
    ASTNode *params;                                                    // Ponteiro para a subárvore de parâmetros
    ASTNode *body;                                                      // Ponteiro para a subárvore do corpo da função
    size_t pi;                                                          // Índice para iterar na lista de parâmetros

    if (!func || func->kind != AST_FUNCTION_DECL) {                     // Valida se o nó é uma declaração de função
        return;                                                         // Aborta se o nó for nulo ou tipo incorreto
    }
                                                                        // Atribui o primeiro filho aos parâmetros
    params = func->child_count > 0 ? func->children : NULL;
                                                                        // Atribui o segundo filho ao bloco de código
    body = func->child_count > 1 ? func->children[1] : NULL;

    ensure_text(ctx);                                                   // Garante que o código será escrito em .text
                                                                        // Imprime o rótulo da função no ficheiro assembly
    fprintf(ctx->out, "\n%s:\n", func->lexeme ? func->lexeme : "func");
    symtab_push_scope(&ctx->table);                                     // Inicia o escopo local para a função
    emit_function_params(ctx, params);                                  // Processa e regista os parâmetros formais
                                                                        // Verifica se há parâmetros para carregar da pilha
    if (params && params->kind == AST_PARAM_LIST && params->child_count > 0) {
        for (pi = 0; pi < params->child_count; ++pi) {                  // Percorre a lista de parâmetros recebidos
            ASTNode *p = params->children[pi];                          // Obtém o nó do parâmetro individual
                                                                        // Localiza o símbolo correspondente na tabela
            Symbol *sym = symtab_lookup(&ctx->table, p->lexeme);
            if (sym && sym->label) {                                    // Se o símbolo existe e possui rótulo definido
                                                                        // Carrega o valor da pilha ($sp) para $t3
                fprintf(ctx->out, "lw $t3, %d($sp)\n", (int)(pi * 4));
                                                                        // Salva o valor no rótulo de memória do parâmetro
                fprintf(ctx->out, "sw $t3, %s\n", sym->label);
            }
        }
    }
    emit_block(ctx, body, 0);                                           // Gera o código assembly para o bloco da função
    fprintf(ctx->out, "jr $ra\n");                                      // Emite instrução de retorno ao chamador
    symtab_pop_scope(&ctx->table);                                      // Encerra o escopo local da função
}

/* Processa a seção global do programa para alocar espaço para variáveis acessíveis em todo o código. */
/* static void: sem retorno; CodegenContext* ctx: contexto da geração; ASTNode* section: nó da seção global. */
static void emit_global_section(CodegenContext *ctx, ASTNode *section) {
                                                                        // Valida a seção global e se possui conteúdo
    if (!section || section->kind != AST_GLOBAL_SECTION || section->child_count == 0) {
        return;                                                         // Sai se a seção global estiver vazia ou inválida
    }
    declare_decl_items(ctx, section->children);                      // Processa e aloca as variáveis globais em .data
}

/* Percorre a lista de funções declaradas no programa e dispara a geração de código para cada uma. */
/* static void: sem retorno; CodegenContext* ctx: contexto da geração; ASTNode* section: nó contendo a lista de funções. */
static void emit_function_section(CodegenContext *ctx, ASTNode *section) {
    size_t i;                                                           // Variável de controle para o laço iterativo

    if (!section || section->kind != AST_FUNCTION_SECTION) {            // Valida o nó da seção de funções
        return;                                                         // Retorna se o nó não for uma seção válida
    }
    for (i = 0; i < section->child_count; ++i) {                        // Itera sobre cada declaração de função na AST
        emit_function(section->children[i], ctx);                       // Chama a geração de código para a função atual
    }
}

/* Gera o ponto de entrada principal do programa MIPS (.globl main) e chama a execução do bloco principal. */
/* static void: sem retorno; CodegenContext* ctx: contexto da geração; ASTNode* block: nó do bloco principal. */
static void emit_main(CodegenContext *ctx, ASTNode *block) {
    ensure_text(ctx);                                                   // Garante que o ponto de entrada está em .text
                                                                        // Define 'main' como global e emite o rótulo
    fprintf(ctx->out, "\n.globl main\nmain:\n");
    symtab_push_scope(&ctx->table);                                     // Abre o escopo global para o programa principal
    emit_block(ctx, block, 0);                                          // Traduz os comandos do bloco principal
                                                                        // Emite syscall para encerrar o programa (exit 10)
    fprintf(ctx->out, "li $v0, 10\nsyscall\n");
    symtab_pop_scope(&ctx->table);                                      // Fecha o escopo principal da tabela
}
/* Gera o nome do arquivo de saída assembly a partir do caminho do arquivo de entrada original. */
/* char*: retorna string alocada do caminho; const char* input_path: caminho do arquivo-fonte original. */
static char *output_path_from_input(const char *input_path) {
    size_t len = strlen(input_path);                                    // Obtém o comprimento do caminho original
    char *out = (char *)malloc(len + 5);                                // Aloca memória para o caminho + ".asm\0"
    if (!out) {                                                         // Verifica se houve falha na alocação
        return NULL;                                                    // Retorna nulo se não houver memória livre
    }
    memcpy(out, input_path, len + 1);                                   // Copia a string original incluindo o terminador
    strcat(out, ".asm");                                                // Adiciona a extensão assembly ao novo caminho
    return out;                                                         // Devolve o endereço da nova string alocada
}

int codegen_generate(ASTNode *root, const char *input_path) {
    CodegenContext ctx;                                                 // Estrutura que mantém o estado da geração
    char *out_path;                                                     // Ponteiro para o nome do ficheiro de saída
    size_t i;                                                           // Variável de controle para percorrer a árvore

    if (!root || root->kind != AST_PROGRAM) {                           // Valida a integridade da árvore abstrata
        return 0;                                                       // Aborta se o nó raiz for inválido ou nulo
    }

    out_path = output_path_from_input(input_path);                      // Constrói o nome do ficheiro .asm resultante
    if (!out_path) {                                                    // Verifica se a criação do caminho falhou
        return 0;                                                       // Interrompe se não conseguir definir a saída
    }

    ctx.out = fopen(out_path, "w");                                     // Cria/Abre o ficheiro assembly para escrita
    free(out_path);                                                     // Liberta a string do caminho após a abertura
    if (!ctx.out) {                                                     // Verifica se a abertura do ficheiro falhou
        return 0;                                                       // Retorna erro se o sistema negar a escrita
    }

    symtab_init(&ctx.table);                                            // Inicializa a tabela de símbolos do contexto
    ctx.var_counter = 0;                                                // Zera o contador global de rótulos de variáveis
    ctx.str_counter = 0;                                                // Zera o contador global de rótulos de strings
    ctx.label_counter = 0;                                              // Zera o contador global de rótulos de desvio
    ctx.data_buf = NULL;                                                // Garante que o buffer de dados começa nulo
    ctx.data_buf_len = 0;                                               // Define o tamanho inicial do buffer como zero
    ctx.data_buf_cap = 0;                                               // Define a capacidade inicial do buffer como zero
    ctx.text_opened = 0;                                                // Marca que a secção de código (.text) está fechada

    symtab_push_scope(&ctx.table);                                      // Abre o escopo base para os símbolos globais

    for (i = 0; i < root->child_count; ++i) {                           // Itera sobre as secções principais da árvore
        ASTNode *child = root->children[i];                             // Recupera a subárvore da secção atual
        if (!child) {                                                   // Verifica se o ponteiro da subárvore é válido
            continue;                                                   // Salta o processamento se o nó for nulo
        }
        if (child->kind == AST_GLOBAL_SECTION) {                        // Identifica secção de declarações globais
            emit_global_section(&ctx, child);                           // Processa variáveis globais e reserva memória
        } else if (child->kind == AST_FUNCTION_SECTION) {               // Identifica secção de definições de funções
            emit_function_section(&ctx, child);                         // Gera o código assembly para cada função
        } else if (child->kind == AST_BLOCK) {                          // Identifica o bloco de comandos principal
            emit_main(&ctx, child);                                     // Gera o ponto de entrada do programa (main)
        }
    }

    flush_data(&ctx);                                                   // Escreve os dados globais e limpa o buffer
    symtab_free(&ctx.table);                                            // Liberta a memória de toda a tabela de símbolos
    fclose(ctx.out);                                                    // Fecha o ficheiro assembly e guarda as alterações
    return 1;                                                           // Finaliza com sucesso a geração de código
}

/*---------------------fim--------------------*/
