%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "g-v2.tab.h"

#define YY_USER_ACTION do { \
    yylloc.first_line = yylineno; \
    yylloc.last_line = yylineno; \
    yylloc.first_column = 1; \
    yylloc.last_column = 1; \
} while (0);

static char *str_buffer = NULL;
static size_t str_len = 0;
static size_t str_cap = 0;

static char *dup_text(const char *s) {
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (!copy) {
        fprintf(stderr, "ERRO: FALHA DE MEMORIA\n");
        exit(1);
    }
    memcpy(copy, s, len + 1);
    return copy;
}

static void lexer_error(const char *msg) {
    fprintf(stderr, "ERRO: %s LINHA %d\n", msg, yylineno);
    exit(1);
}

static void str_reset(void) {
    str_len = 0;
    if (str_buffer) {
        str_buffer[0] = '\0';
    }
}

static void str_append(const char *text) {
    size_t n = strlen(text);
    char *new_buf;
    size_t new_cap;

    if (str_len + n + 1 > str_cap) {
        new_cap = str_cap ? str_cap : 64;
        while (new_cap < str_len + n + 1) {
            new_cap *= 2;
        }
        new_buf = (char *)realloc(str_buffer, new_cap);
        if (!new_buf) {
            fprintf(stderr, "ERRO: FALHA DE MEMORIA\n");
            exit(1);
        }
        str_buffer = new_buf;
        str_cap = new_cap;
    }
    memcpy(str_buffer + str_len, text, n + 1);
    str_len += n;
}
%}

%option noyywrap nodefault yylineno
%x COMENTARIO CADEIA

ID      [A-Za-z_][A-Za-z0-9_]*
INTNUM  [0-9]+
CARLIT  \'([^\\\'\n]|\\[ntr0\\\'])\'

%%

"/*"                    { BEGIN(COMENTARIO); }
<COMENTARIO>[^*\n]+     ;
<COMENTARIO>"*"+[^*/\n] ;
<COMENTARIO>\n          ;
<COMENTARIO>"*"+"/"     { BEGIN(INITIAL); }
<COMENTARIO><<EOF>>     { lexer_error("COMENTARIO NAO TERMINA"); }

"\""                    { str_reset(); str_append("\""); BEGIN(CADEIA); }
<CADEIA>[^\\\"\n]+      { str_append(yytext); }
<CADEIA>\\[^\n]         { str_append(yytext); }
<CADEIA>"\""            {
                            str_append("\"");
                            yylval.lexeme = dup_text(str_buffer);
                            BEGIN(INITIAL);
                            return CADEIACARACTERES;
                         }

<CADEIA>\n              { lexer_error("CADEIA DE CARACTERES OCUPA MAIS DE UMA LINHA"); }
<CADEIA><<EOF>>         { lexer_error("CADEIA DE CARACTERES OCUPA MAIS DE UMA LINHA"); }

"global"                { return GLOBAL; }
"funcao"                { return FUNCAO; }
"principal"             { return PRINCIPAL; }
"int"                   { return INT; }
"car"                   { return CAR; }
"retorne"               { return RETORNE; }
"leia"                  { return LEIA; }
"escreva"               { return ESCREVA; }
"novalinha"             { return NOVALINHA; }
"se"                    { return SE; }
"entao"                 { return ENTAO; }
"senao"                 { return SENAO; }
"fimse"                 { return FIMSE; }
"enquanto"              { return ENQUANTO; }

"||"                    { return OU; }
"&"                     { return E; }
"=="                    { return IGUAL; }
"!="                    { return DIFERENTE; }
">="                    { return MAIORIGUAL; }
"<="                    { return MENORIGUAL; }

{CARLIT}                { yylval.lexeme = dup_text(yytext); return CARCONST; }
{INTNUM}                { yylval.lexeme = dup_text(yytext); return INTCONST; }
{ID}                    { yylval.lexeme = dup_text(yytext); return IDENTIFICADOR; }

[ \t\r\n]+              ;

"{"                     { return '{'; }
"}"                     { return '}'; }
"["                     { return '['; }
"]"                     { return ']'; }
"("                     { return '('; }
")"                     { return ')'; }
":"                     { return ':'; }
";"                     { return ';'; }
","                     { return ','; }
"="                     { return '='; }
"<"                     { return '<'; }
">"                     { return '>'; }
"+"                     { return '+'; }
"-"                     { return '-'; }
"*"                     { return '*'; }
"/"                     { return '/'; }
"!"                     { return '!'; }

<<EOF>>                 { return 0; }

.                       { lexer_error("CARACTERE INVALIDO"); }

%%
