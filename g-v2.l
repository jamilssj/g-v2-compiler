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
