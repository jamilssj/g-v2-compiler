#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>

#include "symbol_table.h"

typedef struct {
    Gv1Type type;
    int is_vector;
} ValueType;

typedef struct {
    SymbolTableStack table;
    int has_error;
    int in_function;
    Gv1Type current_return_type;
} SemanticContext;

static ValueType value_invalid(void) {
    ValueType v;
    v.type = TYPE_INVALID;
    v.is_vector = 0;
    return v;
}

static ValueType value_scalar(Gv1Type type) {
    ValueType v;
    v.type = type;
    v.is_vector = 0;
    return v;
}

static ValueType value_vector(Gv1Type type) {
    ValueType v;
    v.type = type;
    v.is_vector = 1;
    return v;
}

static void semantic_error(SemanticContext *ctx, int line, const char *msg) {
    if (ctx->has_error) {
        return;
    }
    fprintf(stderr, "ERRO: %s LINHA %d\n", msg, line);
    ctx->has_error = 1;
}

static ValueType infer_expr(SemanticContext *ctx, ASTNode *node);
static void analyze_command(SemanticContext *ctx, ASTNode *cmd);

static int is_decl_list(ASTNode *node) {
    return node && node->kind == AST_DECL_LIST;
}

static int is_cmd_list(ASTNode *node) {
    return node && node->kind == AST_CMD_LIST;
}

static ASTNode *block_decl_list(ASTNode *block) {
    if (!block || block->kind != AST_BLOCK || block->child_count == 0) {
        return NULL;
    }
    return is_decl_list(block->children[0]) ? block->children[0] : NULL;
}

static ASTNode *block_cmd_list(ASTNode *block) {
    if (!block || block->kind != AST_BLOCK || block->child_count == 0) {
        return NULL;
    }
    if (is_decl_list(block->children[0])) {
        return block->child_count > 1 ? block->children[1] : NULL;
    }
    return block->children[0];
}

static ParamInfo *build_param_info(ASTNode *params) {
    ParamInfo *list = NULL;
    size_t i;

    if (!params || params->kind != AST_PARAM_LIST) {
        return NULL;
    }
    for (i = 0; i < params->child_count; ++i) {
        ASTNode *p = params->children[i];
        ParamInfo *info;
        if (!p || p->kind != AST_PARAM) {
            continue;
        }
        info = paraminfo_new(p->lexeme, p->type, p->is_vector, p->vector_size);
        if (!info) {
            paraminfo_free(list);
            return NULL;
        }
        paraminfo_append(&list, info);
    }
    return list;
}

static int declare_decl_items(SemanticContext *ctx, ASTNode *decls, int allow_vector_size) {
    size_t i;

    if (!decls || decls->kind != AST_DECL_LIST) {
        return 1;
    }

    for (i = 0; i < decls->child_count; ++i) {
        ASTNode *decl = decls->children[i];
        if (!decl || decl->kind != AST_DECL_ITEM) {
            continue;
        }
        if (!symtab_declare_var(&ctx->table, decl->lexeme, decl->type, decl->is_vector, allow_vector_size ? decl->vector_size : -1, NULL)) {
            semantic_error(ctx, decl->line, "IDENTIFICADOR REDECLARADO NO MESMO ESCOPO");
            return 0;
        }
    }
    return 1;
}

static int declare_params(SemanticContext *ctx, ASTNode *params) {
    size_t i;

    if (!params || params->kind != AST_PARAM_LIST) {
        return 1;
    }

    for (i = 0; i < params->child_count; ++i) {
        ASTNode *p = params->children[i];
        if (!p || p->kind != AST_PARAM) {
            continue;
        }
        if (!symtab_declare_param(&ctx->table, p->lexeme, p->type, p->is_vector, p->vector_size)) {
            semantic_error(ctx, p->line, "IDENTIFICADOR REDECLARADO NO MESMO ESCOPO");
            return 0;
        }
    }
    return 1;
}

static ValueType infer_lvalue(SemanticContext *ctx, ASTNode *node, int allow_vector) {
    Symbol *sym;
    ValueType idx_type;

    if (!node || ctx->has_error) {
        return value_invalid();
    }

    switch (node->kind) {
        case AST_IDENTIFIER:
            sym = symtab_lookup(&ctx->table, node->lexeme);
            if (!sym) {
                semantic_error(ctx, node->line, "IDENTIFICADOR NAO DECLARADO");
                return value_invalid();
            }
            if (sym->kind == SYM_FUNC) {
                semantic_error(ctx, node->line, "USO INVALIDO DE FUNCAO COMO VARIAVEL");
                return value_invalid();
            }
            node->type = sym->type;
            node->is_vector = sym->is_vector;
            return sym->is_vector ? value_vector(sym->type) : value_scalar(sym->type);
        case AST_ARRAY_ACCESS:
            if (node->child_count < 2) {
                semantic_error(ctx, node->line, "ACESSO A VETOR INVALIDO");
                return value_invalid();
            }
            sym = symtab_lookup(&ctx->table, node->children[0]->lexeme);
            if (!sym || sym->kind == SYM_FUNC) {
                semantic_error(ctx, node->line, "IDENTIFICADOR NAO DECLARADO");
                return value_invalid();
            }
            if (!sym->is_vector) {
                semantic_error(ctx, node->line, "IDENTIFICADOR NAO E VETOR");
                return value_invalid();
            }
            idx_type = infer_expr(ctx, node->children[1]);
            if (ctx->has_error) {
                return value_invalid();
            }
            if (idx_type.is_vector || idx_type.type != TYPE_INT) {
                semantic_error(ctx, node->line, "INDICE DE VETOR DEVE SER INT");
                return value_invalid();
            }
            node->type = sym->type;
            node->is_vector = 0;
            return value_scalar(sym->type);
        default:
            if (!allow_vector) {
                semantic_error(ctx, node->line, "LADO ESQUERDO INVALIDO");
            }
            return value_invalid();
    }
}

static ValueType infer_call(SemanticContext *ctx, ASTNode *node) {
    Symbol *sym;
    ASTNode *args;
    size_t i;
    size_t expected;

    sym = symtab_lookup(&ctx->table, node->lexeme);
    if (!sym || sym->kind != SYM_FUNC) {
        semantic_error(ctx, node->line, "FUNCAO NAO DECLARADA");
        return value_invalid();
    }

    args = node->child_count > 0 ? node->children[0] : NULL;
    expected = (size_t)sym->param_count;
    if (expected != (args ? args->child_count : 0)) {
        semantic_error(ctx, node->line, "NUMERO DE ARGUMENTOS INCOMPATIVEL");
        return value_invalid();
    }

    for (i = 0; i < expected; ++i) {
        ParamInfo *formal = sym->params;
        ASTNode *actual;
        ValueType actual_type;
        size_t j;

        for (j = 0; j < i && formal; ++j) {
            formal = formal->next;
        }
        if (!formal) {
            break;
        }
        actual = args->children[i];
        actual_type = infer_expr(ctx, actual);
        if (ctx->has_error) {
            return value_invalid();
        }
        if (formal->is_vector) {
            if (!actual_type.is_vector || actual_type.type != formal->type) {
                semantic_error(ctx, actual->line, "TIPO DE ARGUMENTO INCOMPATIVEL");
                return value_invalid();
            }
        } else {
            if (actual_type.is_vector || actual_type.type != formal->type) {
                semantic_error(ctx, actual->line, "TIPO DE ARGUMENTO INCOMPATIVEL");
                return value_invalid();
            }
        }
    }

    node->type = sym->type;
    node->is_vector = 0;
    return value_scalar(sym->type);
}

static ValueType infer_expr(SemanticContext *ctx, ASTNode *node) {
    ValueType left;
    ValueType right;
    ValueType a;
    ValueType b;
    Symbol *sym;

    if (!node || ctx->has_error) {
        return value_invalid();
    }

    switch (node->kind) {
        case AST_IDENTIFIER:
            sym = symtab_lookup(&ctx->table, node->lexeme);
            if (!sym) {
                semantic_error(ctx, node->line, "IDENTIFICADOR NAO DECLARADO");
                return value_invalid();
            }
            if (sym->kind == SYM_FUNC) {
                semantic_error(ctx, node->line, "FUNCAO USADA COMO EXPRESSAO");
                return value_invalid();
            }
            node->type = sym->type;
            node->is_vector = sym->is_vector;
            return sym->is_vector ? value_vector(sym->type) : value_scalar(sym->type);
        case AST_INT_LITERAL:
            node->type = TYPE_INT;
            node->is_vector = 0;
            return value_scalar(TYPE_INT);
        case AST_CHAR_LITERAL:
            node->type = TYPE_CAR;
            node->is_vector = 0;
            return value_scalar(TYPE_CAR);
        case AST_STRING_LITERAL:
            semantic_error(ctx, node->line, "CADEIA NAO E EXPRESSAO");
            return value_invalid();
        case AST_ARRAY_ACCESS:
            return infer_lvalue(ctx, node, 0);
        case AST_FUNC_CALL:
            return infer_call(ctx, node);
        case AST_ASSIGN:
            if (node->child_count < 2) {
                semantic_error(ctx, node->line, "ATRIBUICAO INVALIDA");
                return value_invalid();
            }
            left = infer_lvalue(ctx, node->children[0], 1);
            right = infer_expr(ctx, node->children[1]);
            if (ctx->has_error) {
                return value_invalid();
            }
            if (left.type != right.type || left.is_vector != right.is_vector) {
                semantic_error(ctx, node->line, "TIPOS INCOMPATIVEIS");
                return value_invalid();
            }
            node->type = left.type;
            node->is_vector = left.is_vector;
            return left;
        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
            a = infer_expr(ctx, node->children[0]);
            b = infer_expr(ctx, node->children[1]);
            if (ctx->has_error) {
                return value_invalid();
            }
            if (a.is_vector || b.is_vector || a.type != TYPE_INT || b.type != TYPE_INT) {
                semantic_error(ctx, node->line, "OPERACAO ARITMETICA EXIGE INT");
                return value_invalid();
            }
            node->type = TYPE_INT;
            node->is_vector = 0;
            return value_scalar(TYPE_INT);
        case AST_OR:
        case AST_AND:
            a = infer_expr(ctx, node->children[0]);
            b = infer_expr(ctx, node->children[1]);
            if (ctx->has_error) {
                return value_invalid();
            }
            if (a.is_vector || b.is_vector || a.type != TYPE_INT || b.type != TYPE_INT) {
                semantic_error(ctx, node->line, "OPERACAO LOGICA EXIGE INT");
                return value_invalid();
            }
            node->type = TYPE_INT;
            node->is_vector = 0;
            return value_scalar(TYPE_INT);
        case AST_EQ:
        case AST_NE:
        case AST_LT:
        case AST_GT:
        case AST_GE:
        case AST_LE:
            a = infer_expr(ctx, node->children[0]);
            b = infer_expr(ctx, node->children[1]);
            if (ctx->has_error) {
                return value_invalid();
            }
            if (a.is_vector || b.is_vector || a.type != b.type) {
                semantic_error(ctx, node->line, "OPERACAO RELACIONAL EXIGE TIPOS IGUAIS");
                return value_invalid();
            }
            node->type = TYPE_INT;
            node->is_vector = 0;
            return value_scalar(TYPE_INT);
        case AST_NEG:
        case AST_NOT:
            a = infer_expr(ctx, node->children[0]);
            if (ctx->has_error) {
                return value_invalid();
            }
            if (a.is_vector || a.type != TYPE_INT) {
                semantic_error(ctx, node->line, "OPERACAO UNARIA EXIGE INT");
                return value_invalid();
            }
            node->type = TYPE_INT;
            node->is_vector = 0;
            return value_scalar(TYPE_INT);
        default:
            return value_invalid();
    }
}

static void analyze_block(SemanticContext *ctx, ASTNode *block, int push_scope) {
    ASTNode *decls;
    ASTNode *cmds;
    size_t i;

    if (!block || block->kind != AST_BLOCK || ctx->has_error) {
        return;
    }

    if (push_scope) {
        symtab_push_scope(&ctx->table);
    }

    decls = block_decl_list(block);
    cmds = block_cmd_list(block);
    if (!declare_decl_items(ctx, decls, 1)) {
        if (push_scope) {
            symtab_pop_scope(&ctx->table);
        }
        return;
    }

    if (cmds && is_cmd_list(cmds)) {
        for (i = 0; i < cmds->child_count; ++i) {
            analyze_command(ctx, cmds->children[i]);
            if (ctx->has_error) {
                break;
            }
        }
    }

    if (push_scope) {
        symtab_pop_scope(&ctx->table);
    }
}

static void analyze_function(SemanticContext *ctx, ASTNode *func) {
    ASTNode *params;
    ASTNode *body;
    ParamInfo *plist;
    Symbol *sym;

    if (!func || func->kind != AST_FUNCTION_DECL || ctx->has_error) {
        return;
    }

    params = func->child_count > 0 ? func->children[0] : NULL;
    body = func->child_count > 1 ? func->children[1] : NULL;
    plist = build_param_info(params);
    if (params && params->kind == AST_PARAM_LIST && params->child_count != 0 && !plist) {
        semantic_error(ctx, func->line, "FALHA INTERNA NA LISTA DE PARAMETROS");
        return;
    }

    sym = symtab_declare_func(&ctx->table, func->lexeme, func->type, (int)(params ? params->child_count : 0), plist);
    if (!sym) {
        paraminfo_free(plist);
        semantic_error(ctx, func->line, "IDENTIFICADOR REDECLARADO NO MESMO ESCOPO");
        return;
    }

    symtab_push_scope(&ctx->table);
    if (!declare_params(ctx, params)) {
        symtab_pop_scope(&ctx->table);
        return;
    }

    ctx->in_function = 1;
    ctx->current_return_type = func->type;
    analyze_block(ctx, body, 0);
    ctx->in_function = 0;
    symtab_pop_scope(&ctx->table);
}

static void analyze_command(SemanticContext *ctx, ASTNode *cmd) {
    ValueType cond;
    ValueType ret;

    if (!cmd || ctx->has_error) {
        return;
    }

    switch (cmd->kind) {
        case AST_EMPTY_STMT:
            break;
        case AST_EXPR_STMT:
            if (cmd->child_count > 0) {
                (void)infer_expr(ctx, cmd->children[0]);
            }
            break;
        case AST_READ: {
            ret = infer_lvalue(ctx, cmd->children[0], 0);
            if (ctx->has_error) {
                break;
            }
            if (ret.is_vector) {
                semantic_error(ctx, cmd->line, "LEIA EXIGE VARIAVEL ESCALAR");
            }
            break;
        }
        case AST_WRITE_EXPR:
            if (cmd->child_count > 0) {
                ret = infer_expr(ctx, cmd->children[0]);
                if (!ctx->has_error && ret.is_vector) {
                    semantic_error(ctx, cmd->line, "ESCRITA EXIGE EXPRESSAO ESCALAR");
                }
            }
            break;
        case AST_WRITE_STR:
        case AST_NEWLINE:
            break;
        case AST_RETURN:
            if (!ctx->in_function) {
                semantic_error(ctx, cmd->line, "RETORNO FORA DE FUNCAO");
                break;
            }
            if (cmd->child_count == 0) {
                semantic_error(ctx, cmd->line, "RETORNO SEM EXPRESSAO");
                break;
            }
            ret = infer_expr(ctx, cmd->children[0]);
            if (ctx->has_error) {
                break;
            }
            if (ret.is_vector || ret.type != ctx->current_return_type) {
                semantic_error(ctx, cmd->line, "TIPO DE RETORNO INCOMPATIVEL");
            }
            break;
        case AST_IF:
        case AST_IF_ELSE:
            cond = infer_expr(ctx, cmd->children[0]);
            if (ctx->has_error) {
                break;
            }
            if (cond.is_vector || cond.type != TYPE_INT) {
                semantic_error(ctx, cmd->line, "EXPRESSAO DE CONTROLE DEVE SER INT");
                break;
            }
            analyze_command(ctx, cmd->children[1]);
            if (!ctx->has_error && cmd->kind == AST_IF_ELSE && cmd->child_count > 2) {
                analyze_command(ctx, cmd->children[2]);
            }
            break;
        case AST_WHILE:
            cond = infer_expr(ctx, cmd->children[0]);
            if (ctx->has_error) {
                break;
            }
            if (cond.is_vector || cond.type != TYPE_INT) {
                semantic_error(ctx, cmd->line, "EXPRESSAO DE CONTROLE DEVE SER INT");
                break;
            }
            analyze_command(ctx, cmd->children[1]);
            break;
        case AST_BLOCK:
            analyze_block(ctx, cmd, 0);
            break;
        default:
            break;
    }
}

static void analyze_function_section(SemanticContext *ctx, ASTNode *section) {
    size_t i;

    if (!section || section->kind != AST_FUNCTION_SECTION) {
        return;
    }
    for (i = 0; i < section->child_count; ++i) {
        analyze_function(ctx, section->children[i]);
        if (ctx->has_error) {
            break;
        }
    }
}

static void analyze_global_section(SemanticContext *ctx, ASTNode *section) {
    if (!section || section->kind != AST_GLOBAL_SECTION) {
        return;
    }
    if (section->child_count > 0) {
        (void)declare_decl_items(ctx, section->children[0], 1);
    }
}

int semantic_check(ASTNode *root) {
    SemanticContext ctx;
    size_t i;

    if (!root || root->kind != AST_PROGRAM) {
        fprintf(stderr, "ERRO: AST INVALIDA LINHA 0\n");
        return 0;
    }

    symtab_init(&ctx.table);
    ctx.has_error = 0;
    ctx.in_function = 0;
    ctx.current_return_type = TYPE_INVALID;

    symtab_push_scope(&ctx.table);

    for (i = 0; i < root->child_count; ++i) {
        ASTNode *child = root->children[i];
        if (!child) {
            continue;
        }
        if (child->kind == AST_GLOBAL_SECTION) {
            analyze_global_section(&ctx, child);
        } else if (child->kind == AST_FUNCTION_SECTION) {
            analyze_function_section(&ctx, child);
        } else if (child->kind == AST_BLOCK) {
            analyze_block(&ctx, child, 1);
        }
        if (ctx.has_error) {
            break;
        }
    }

    symtab_free(&ctx.table);
    return ctx.has_error ? 0 : 1;
}
