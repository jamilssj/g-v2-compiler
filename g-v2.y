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
