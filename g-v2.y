%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "semantic.h"
#include "codegen.h"

extern int yylex(void);
extern int yylineno;
extern char *yytext;
extern FILE *yyin;

static ASTNode *g_root = NULL;

static ASTNode *make_binary(ASTKind kind, ASTNode *left, ASTNode *right, int line) {
    ASTNode *node = ast_new(kind, line, NULL);
    if (!node) {
        return NULL;
    }
    ast_add_child(node, left);
    ast_add_child(node, right);
    return node;
}

static ASTNode *make_unary(ASTKind kind, ASTNode *child, int line) {
    ASTNode *node = ast_new(kind, line, NULL);
    if (!node) {
        return NULL;
    }
    ast_add_child(node, child);
    return node;
}

static ASTNode *merge_lists(ASTNode *a, ASTNode *b) {
    size_t i;

    if (!a) {
        return b;
    }
    if (!b) {
        return a;
    }
    for (i = 0; i < b->child_count; ++i) {
        ast_add_child(a, b->children[i]);
    }
    b->child_count = 0;
    ast_free(b);
    return a;
}

static ASTNode *make_decl_item(int line, char *name, int is_vector, int vector_size) {
    ASTNode *node = ast_new(AST_DECL_ITEM, line, name);
    node->is_vector = is_vector;
    node->vector_size = vector_size;
    free(name);
    return node;
}

static ASTNode *make_param_item(int line, char *name, int is_vector) {
    ASTNode *node = ast_new(AST_PARAM, line, name);
    node->is_vector = is_vector;
    node->vector_size = -1;
    free(name);
    return node;
}

static ASTNode *make_expr_list(void) {
    return ast_new_list(AST_EXPR_LIST);
}

void yyerror(const char *s);
%}

%code requires {
#include "ast.h"
}

%define parse.error verbose
%locations

%union {
    ASTNode *node;
    char *lexeme;
    int typev;
}

%start programa

%token GLOBAL FUNCAO PRINCIPAL INT CAR RETORNE LEIA ESCREVA NOVALINHA SE ENTAO SENAO FIMSE ENQUANTO
%token OU E IGUAL DIFERENTE MAIORIGUAL MENORIGUAL
%token <lexeme> IDENTIFICADOR CADEIACARACTERES CARCONST INTCONST

%type <node> programa opt_global opt_func global_section func_section func_list func_decl principal_block bloco
%type <node> decl_list decl_line var_list param_list_opt param_list param_item cmd_list comando expr lvalue
%type <node> or_expr and_expr eq_expr desig_expr add_expr mul_expr un_expr prim_expr
%type <node> list_expr arg_list_opt
%type <typev> tipo

%%

programa
    : opt_global opt_func PRINCIPAL principal_block {
        $$ = ast_new(AST_PROGRAM, @3.first_line, NULL);
        if ($1) {
            ast_add_child($$, $1);
        }
        if ($2) {
            ast_add_child($$, $2);
        }
        ast_add_child($$, $4);
        g_root = $$;
      }
    ;

opt_global
    : /* empty */ { $$ = NULL; }
    | global_section { $$ = $1; }
    ;

opt_func
    : /* empty */ { $$ = NULL; }
    | func_section { $$ = $1; }
    ;

global_section
    : GLOBAL '[' decl_list ']' {
        $$ = ast_new(AST_GLOBAL_SECTION, @1.first_line, NULL);
        ast_add_child($$, $3);
      }
    ;

func_section
    : FUNCAO '[' func_list ']' {
        $$ = $3;
      }
    ;

func_list
    : func_decl {
        $$ = ast_new_list(AST_FUNCTION_SECTION);
        ast_add_child($$, $1);
      }
    | func_list func_decl {
        ast_add_child($1, $2);
        $$ = $1;
      }
    ;

func_decl
    : IDENTIFICADOR '(' param_list_opt ')' ':' tipo bloco {
        $$ = ast_new(AST_FUNCTION_DECL, @1.first_line, $1);
        $$->type = (Gv1Type)$6;
        ast_add_child($$, $3);
        ast_add_child($$, $7);
        free($1);
      }
    ;

principal_block
    : bloco { $$ = $1; }
    ;

bloco
    : '[' decl_list ']' '{' cmd_list '}' {
        $$ = ast_new(AST_BLOCK, @1.first_line, NULL);
        ast_add_child($$, $2);
        ast_add_child($$, $5);
      }
    | '{' cmd_list '}' {
        $$ = ast_new(AST_BLOCK, @1.first_line, NULL);
        ast_add_child($$, $2);
      }
    ;

decl_list
    : decl_line { $$ = $1; }
    | decl_list decl_line { $$ = merge_lists($1, $2); }
    ;

decl_line
    : var_list ':' tipo ';' {
        size_t i;
        for (i = 0; i < $1->child_count; ++i) {
            $1->children[i]->type = (Gv1Type)$3;
        }
        $$ = $1;
      }
    ;

var_list
    : IDENTIFICADOR {
        ASTNode *item = make_decl_item(@1.first_line, $1, 0, -1);
        $$ = ast_new_list(AST_DECL_LIST);
        ast_add_child($$, item);
      }
    | IDENTIFICADOR '[' INTCONST ']' {
        ASTNode *item = make_decl_item(@1.first_line, $1, 1, atoi($3));
        $$ = ast_new_list(AST_DECL_LIST);
        ast_add_child($$, item);
        free($3);
      }
    | var_list ',' IDENTIFICADOR {
        ast_add_child($1, make_decl_item(@3.first_line, $3, 0, -1));
        $$ = $1;
      }
    | var_list ',' IDENTIFICADOR '[' INTCONST ']' {
        ast_add_child($1, make_decl_item(@3.first_line, $3, 1, atoi($5)));
        $$ = $1;
        free($5);
      }
    ;

param_list_opt
    : /* empty */ { $$ = ast_new_list(AST_PARAM_LIST); }
    | param_list { $$ = $1; }
    ;

param_list
    : param_item {
        $$ = ast_new_list(AST_PARAM_LIST);
        ast_add_child($$, $1);
      }
    | param_list ',' param_item {
        ast_add_child($1, $3);
        $$ = $1;
      }
    ;

param_item
    : IDENTIFICADOR ':' tipo {
        ASTNode *node = make_param_item(@1.first_line, $1, 0);
        node->type = (Gv1Type)$3;
        $$ = node;
      }
    | IDENTIFICADOR '[' ']' ':' tipo {
        ASTNode *node = make_param_item(@1.first_line, $1, 1);
        node->type = (Gv1Type)$5;
        $$ = node;
      }
    ;

tipo
    : INT { $$ = TYPE_INT; }
    | CAR { $$ = TYPE_CAR; }
    ;

cmd_list
    : comando {
        $$ = ast_new_list(AST_CMD_LIST);
        ast_add_child($$, $1);
      }
    | cmd_list comando {
        ast_add_child($1, $2);
        $$ = $1;
      }
    ;

comando
    : ';' { $$ = ast_new(AST_EMPTY_STMT, @1.first_line, NULL); }
    | expr ';' {
        $$ = ast_new(AST_EXPR_STMT, @1.first_line, NULL);
        ast_add_child($$, $1);
      }
    | RETORNE expr ';' {
        $$ = ast_new(AST_RETURN, @1.first_line, NULL);
        ast_add_child($$, $2);
      }
    | LEIA lvalue ';' {
        $$ = ast_new(AST_READ, @1.first_line, NULL);
        ast_add_child($$, $2);
      }
    | ESCREVA expr ';' {
        $$ = ast_new(AST_WRITE_EXPR, @1.first_line, NULL);
        ast_add_child($$, $2);
      }
    | ESCREVA CADEIACARACTERES ';' {
        $$ = ast_new(AST_WRITE_STR, @2.first_line, $2);
        free($2);
      }
    | NOVALINHA ';' { $$ = ast_new(AST_NEWLINE, @1.first_line, NULL); }
    | SE '(' expr ')' ENTAO comando FIMSE {
        $$ = ast_new(AST_IF, @1.first_line, NULL);
        ast_add_child($$, $3);
        ast_add_child($$, $6);
      }
    | SE '(' expr ')' ENTAO comando SENAO comando FIMSE {
        $$ = ast_new(AST_IF_ELSE, @1.first_line, NULL);
        ast_add_child($$, $3);
        ast_add_child($$, $6);
        ast_add_child($$, $8);
      }
    | ENQUANTO '(' expr ')' comando {
        $$ = ast_new(AST_WHILE, @1.first_line, NULL);
        ast_add_child($$, $3);
        ast_add_child($$, $5);
      }
    | bloco { $$ = $1; }
    ;

lvalue
    : IDENTIFICADOR {
        $$ = ast_new(AST_IDENTIFIER, @1.first_line, $1);
        free($1);
      }
    | IDENTIFICADOR '[' expr ']' {
        $$ = ast_new(AST_ARRAY_ACCESS, @1.first_line, $1);
        ast_add_child($$, ast_new(AST_IDENTIFIER, @1.first_line, $1));
        ast_add_child($$, $3);
        free($1);
      }
    ;

expr
    : lvalue '=' expr {
        $$ = ast_new(AST_ASSIGN, @2.first_line, NULL);
        ast_add_child($$, $1);
        ast_add_child($$, $3);
      }
    | or_expr { $$ = $1; }
    ;

or_expr
    : or_expr OU and_expr { $$ = make_binary(AST_OR, $1, $3, @2.first_line); }
    | and_expr { $$ = $1; }
    ;

and_expr
    : and_expr E eq_expr { $$ = make_binary(AST_AND, $1, $3, @2.first_line); }
    | eq_expr { $$ = $1; }
    ;

eq_expr
    : eq_expr IGUAL desig_expr { $$ = make_binary(AST_EQ, $1, $3, @2.first_line); }
    | eq_expr DIFERENTE desig_expr { $$ = make_binary(AST_NE, $1, $3, @2.first_line); }
    | desig_expr { $$ = $1; }
    ;

desig_expr
    : desig_expr '<' add_expr { $$ = make_binary(AST_LT, $1, $3, @2.first_line); }
    | desig_expr '>' add_expr { $$ = make_binary(AST_GT, $1, $3, @2.first_line); }
    | desig_expr MAIORIGUAL add_expr { $$ = make_binary(AST_GE, $1, $3, @2.first_line); }
    | desig_expr MENORIGUAL add_expr { $$ = make_binary(AST_LE, $1, $3, @2.first_line); }
    | add_expr { $$ = $1; }
    ;

add_expr
    : add_expr '+' mul_expr { $$ = make_binary(AST_ADD, $1, $3, @2.first_line); }
    | add_expr '-' mul_expr { $$ = make_binary(AST_SUB, $1, $3, @2.first_line); }
    | mul_expr { $$ = $1; }
    ;

mul_expr
    : mul_expr '*' un_expr { $$ = make_binary(AST_MUL, $1, $3, @2.first_line); }
    | mul_expr '/' un_expr { $$ = make_binary(AST_DIV, $1, $3, @2.first_line); }
    | un_expr { $$ = $1; }
    ;

un_expr
    : '-' prim_expr { $$ = make_unary(AST_NEG, $2, @1.first_line); }
    | '!' prim_expr { $$ = make_unary(AST_NOT, $2, @1.first_line); }
    | prim_expr { $$ = $1; }
    ;

prim_expr
    : IDENTIFICADOR '(' arg_list_opt ')' {
        $$ = ast_new(AST_FUNC_CALL, @1.first_line, $1);
        ast_add_child($$, $3);
        free($1);
      }
    | IDENTIFICADOR '[' expr ']' {
        $$ = ast_new(AST_ARRAY_ACCESS, @1.first_line, $1);
        ast_add_child($$, ast_new(AST_IDENTIFIER, @1.first_line, $1));
        ast_add_child($$, $3);
        free($1);
      }
    | IDENTIFICADOR {
        $$ = ast_new(AST_IDENTIFIER, @1.first_line, $1);
        free($1);
      }
    | CARCONST {
        $$ = ast_new(AST_CHAR_LITERAL, @1.first_line, $1);
        free($1);
      }
    | INTCONST {
        $$ = ast_new(AST_INT_LITERAL, @1.first_line, $1);
        free($1);
      }
    | '(' expr ')' { $$ = $2; }
    ;

arg_list_opt
    : /* empty */ { $$ = make_expr_list(); }
    | list_expr { $$ = $1; }
    ;

list_expr
    : expr {
        $$ = make_expr_list();
        ast_add_child($$, $1);
      }
    | list_expr ',' expr {
        ast_add_child($1, $3);
        $$ = $1;
      }
    ;

%%

void yyerror(const char *s) {
    (void)s;
    fprintf(stderr, "ERRO: ERRO SINTATICO LINHA %d\n", yylineno);
}

int main(int argc, char **argv) {
    int parse_result;

    if (argc != 2) {
        fprintf(stderr, "ERRO: USO: ./g-v2 arquivo.g\n");
        return 1;
    }

    yyin = fopen(argv[1], "r");
    if (!yyin) {
        fprintf(stderr, "ERRO: NAO FOI POSSIVEL ABRIR ARQUIVO\n");
        return 1;
    }

    parse_result = yyparse();
    fclose(yyin);
    if (parse_result != 0 || !g_root) {
        ast_free(g_root);
        return 1;
    }

    if (!semantic_check(g_root)) {
        ast_free(g_root);
        return 1;
    }

    if (!codegen_generate(g_root, argv[1])) {
        fprintf(stderr, "ERRO: FALHA NA GERACAO DE CODIGO\n");
        ast_free(g_root);
        return 1;
    }

    ast_free(g_root);
    return 0;
}
