#include "ast.h"

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

ASTNode *ast_new(ASTKind kind, int line, const char *lexeme) {
    ASTNode *node = (ASTNode *)calloc(1, sizeof(ASTNode));
    if (!node) {
        return NULL;
    }
    node->kind = kind;
    node->line = line;
    node->type = TYPE_INVALID;
    node->lexeme = dup_str(lexeme);
    node->is_vector = 0;
    node->vector_size = -1;
    return node;
}

ASTNode *ast_new_list(ASTKind kind) {
    return ast_new(kind, 0, NULL);
}

void ast_add_child(ASTNode *parent, ASTNode *child) {
    ASTNode **new_children;
    size_t new_capacity;

    if (!parent || !child) {
        return;
    }

    if (parent->child_count == parent->child_capacity) {
        new_capacity = parent->child_capacity ? parent->child_capacity * 2 : 4;
        new_children = (ASTNode **)realloc(parent->children, new_capacity * sizeof(ASTNode *));
        if (!new_children) {
            return;
        }
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }

    parent->children[parent->child_count++] = child;
}

void ast_free(ASTNode *node) {
    size_t i;

    if (!node) {
        return;
    }
    for (i = 0; i < node->child_count; ++i) {
        ast_free(node->children[i]);
    }
    free(node->children);
    free(node->lexeme);
    free(node);
}
