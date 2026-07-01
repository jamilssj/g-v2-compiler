#ifndef GV_AST_H
#define GV_AST_H

#include <stddef.h>

typedef enum {
    TYPE_INVALID = 0,
    TYPE_INT,
    TYPE_CAR
} Gv1Type;

typedef enum {
    AST_PROGRAM = 0,
    AST_GLOBAL_SECTION,
    AST_FUNCTION_SECTION,
    AST_FUNCTION_DECL,
    AST_PARAM_LIST,
    AST_PARAM,
    AST_BLOCK,
    AST_DECL_LIST,
    AST_DECL_ITEM,
    AST_CMD_LIST,
    AST_EXPR_LIST,
    AST_EMPTY_STMT,
    AST_EXPR_STMT,
    AST_READ,
    AST_WRITE_EXPR,
    AST_WRITE_STR,
    AST_NEWLINE,
    AST_RETURN,
    AST_IF,
    AST_IF_ELSE,
    AST_WHILE,
    AST_ASSIGN,
    AST_OR,
    AST_AND,
    AST_EQ,
    AST_NE,
    AST_LT,
    AST_GT,
    AST_GE,
    AST_LE,
    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_NEG,
    AST_NOT,
    AST_IDENTIFIER,
    AST_INT_LITERAL,
    AST_CHAR_LITERAL,
    AST_STRING_LITERAL,
    AST_FUNC_CALL,
    AST_ARRAY_ACCESS
} ASTKind;

typedef struct ASTNode {
    ASTKind kind;
    int line;
    char *lexeme;
    Gv1Type type;
    int is_vector;
    int vector_size;
    struct ASTNode **children;
    size_t child_count;
    size_t child_capacity;
} ASTNode;

ASTNode *ast_new(ASTKind kind, int line, const char *lexeme);
ASTNode *ast_new_list(ASTKind kind);
void ast_add_child(ASTNode *parent, ASTNode *child);
void ast_free(ASTNode *node);

#endif
