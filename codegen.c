#include "codegen.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol_table.h"

typedef struct {
    FILE *out;
    SymbolTableStack table;
    int var_counter;
    int str_counter;
    int label_counter;
    char *data_buf;
    size_t data_buf_len;
    size_t data_buf_cap;
    int text_opened;
} CodegenContext;

static void data_buf_printf(CodegenContext *ctx, const char *fmt, ...) {
    va_list args;
    va_list args2;
    int needed;
    char *new_buf;
    size_t new_cap;

    va_start(args, fmt);
    va_copy(args2, args);
    needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (needed < 0) return;

    if (ctx->data_buf_len + (size_t)needed + 1 > ctx->data_buf_cap) {
        new_cap = ctx->data_buf_cap ? ctx->data_buf_cap : 256;
        while (new_cap < ctx->data_buf_len + (size_t)needed + 1) new_cap *= 2;
        new_buf = (char *)realloc(ctx->data_buf, new_cap);
        if (!new_buf) return;
        ctx->data_buf = new_buf;
        ctx->data_buf_cap = new_cap;
    }
    vsnprintf(ctx->data_buf + ctx->data_buf_len, (size_t)needed + 1, fmt, args2);
    ctx->data_buf_len += (size_t)needed;
    va_end(args2);
}

static void flush_data(CodegenContext *ctx) {
    if (ctx->data_buf && ctx->data_buf_len > 0) {
        fprintf(ctx->out, ".data\n");
        fwrite(ctx->data_buf, 1, ctx->data_buf_len, ctx->out);
        free(ctx->data_buf);
        ctx->data_buf = NULL;
        ctx->data_buf_len = 0;
        ctx->data_buf_cap = 0;
    }
}

static char *make_label(const char *prefix, int id) {
    char buffer[64];
    int size = snprintf(buffer, sizeof(buffer), "%s_%d", prefix, id);
    char *result = (char *)malloc((size_t)size + 1);
    if (!result) {
        return NULL;
    }
    memcpy(result, buffer, (size_t)size + 1);
    return result;
}

static int char_literal_value(const char *lexeme) {
    if (!lexeme || lexeme[0] != '\'' || lexeme[1] == '\0') {
        return 0;
    }
    if (lexeme[1] == '\\') {
        switch (lexeme[2]) {
            case 'n': return '\n';
            case 't': return '\t';
            case 'r': return '\r';
            case '\\': return '\\';
            case '\'': return '\'';
            case '0': return '\0';
            default: return (unsigned char)lexeme[2];
        }
    }
    return (unsigned char)lexeme[1];
}

static void ensure_text(CodegenContext *ctx) {
    if (!ctx->text_opened) {
        fprintf(ctx->out, ".text\n");
        ctx->text_opened = 1;
    }
}

static ASTNode *block_decl_list(ASTNode *block) {
    if (!block || block->kind != AST_BLOCK || block->child_count == 0) {
        return NULL;
    }
    return block->children[0]->kind == AST_DECL_LIST ? block->children[0] : NULL;
}

static ASTNode *block_cmd_list(ASTNode *block) {
    if (!block || block->kind != AST_BLOCK || block->child_count == 0) {
        return NULL;
    }
    if (block->children[0]->kind == AST_DECL_LIST) {
        return block->child_count > 1 ? block->children[1] : NULL;
    }
    return block->children[0];
}

static void declare_decl_items(CodegenContext *ctx, ASTNode *decls) {
    size_t i;

    if (!decls || decls->kind != AST_DECL_LIST) {
        return;
    }
    for (i = 0; i < decls->child_count; ++i) {
        ASTNode *decl = decls->children[i];
        char *label;
        if (!decl || decl->kind != AST_DECL_ITEM) {
            continue;
        }
        label = make_label("gv2_var", ctx->var_counter++);
        if (!label) {
            continue;
        }
        if (decl->is_vector) {
            int bytes = (decl->vector_size > 0 ? decl->vector_size : 1) * 4;
            data_buf_printf(ctx, "%s: .space %d\n", label, bytes);
        } else {
            data_buf_printf(ctx, "%s: .word 0\n", label);
        }
        (void)symtab_declare_var(&ctx->table, decl->lexeme, decl->type, decl->is_vector, decl->vector_size, label);
        free(label);
    }
}

static void emit_expr(CodegenContext *ctx, ASTNode *node);
static void emit_command(CodegenContext *ctx, ASTNode *cmd);

static void emit_array_access_address(CodegenContext *ctx, ASTNode *node) {
    Symbol *sym;

    if (!node || node->kind != AST_ARRAY_ACCESS || node->child_count < 2) {
        fprintf(ctx->out, "move $v0, $zero\n");
        return;
    }
    sym = symtab_lookup(&ctx->table, node->children[0]->lexeme);
    emit_expr(ctx, node->children[1]);
    fprintf(ctx->out, "move $t4, $v0\n");
    if (sym && sym->label) {
        if (sym->kind == SYM_PARAM && sym->is_vector) {
            fprintf(ctx->out, "lw $t5, %s\n", sym->label);
        } else {
            fprintf(ctx->out, "la $t5, %s\n", sym->label);
        }
        fprintf(ctx->out, "sll $t4, $t4, 2\n");
        fprintf(ctx->out, "addu $v0, $t5, $t4\n");
    } else {
        fprintf(ctx->out, "move $v0, $zero\n");
    }
}

static void emit_expr(CodegenContext *ctx, ASTNode *node) {
    Symbol *sym;
    char *l1;
    char *l2;

    if (!node) {
        fprintf(ctx->out, "li $v0, 0\n");
        return;
    }

    switch (node->kind) {
        case AST_IDENTIFIER:
            sym = symtab_lookup(&ctx->table, node->lexeme);
            if (sym && sym->label) {
                if (sym->is_vector) {
                    if (sym->kind == SYM_PARAM) {
                        fprintf(ctx->out, "lw $v0, %s\n", sym->label);
                    } else {
                        fprintf(ctx->out, "la $v0, %s\n", sym->label);
                    }
                } else {
                    fprintf(ctx->out, "lw $v0, %s\n", sym->label);
                }
            } else {
                fprintf(ctx->out, "li $v0, 0\n");
            }
            break;
        case AST_ARRAY_ACCESS:
            emit_array_access_address(ctx, node);
            fprintf(ctx->out, "lw $v0, 0($v0)\n");
            break;
        case AST_INT_LITERAL:
            fprintf(ctx->out, "li $v0, %s\n", node->lexeme ? node->lexeme : "0");
            break;
        case AST_CHAR_LITERAL:
            fprintf(ctx->out, "li $v0, %d\n", char_literal_value(node->lexeme));
            break;
        case AST_STRING_LITERAL:
            l1 = make_label("gv2_str", ctx->str_counter++);
            data_buf_printf(ctx, "%s: .asciiz %s\n", l1, node->lexeme ? node->lexeme : "\"\"");
            ensure_text(ctx);
            fprintf(ctx->out, "la $v0, %s\n", l1);
            free(l1);
            break;
        case AST_FUNC_CALL: {
            size_t arg_count = 0;
            size_t ai;
            if (node->child_count > 0 && node->children[0] && node->children[0]->kind == AST_EXPR_LIST) {
                arg_count = node->children[0]->child_count;
            }
            if (arg_count > 0) {
                fprintf(ctx->out, "addiu $sp, $sp, %d\n", (int)(-((int)arg_count * 4)));
                for (ai = 0; ai < arg_count; ++ai) {
                    emit_expr(ctx, node->children[0]->children[ai]);
                    fprintf(ctx->out, "sw $v0, %d($sp)\n", (int)(ai * 4));
                }
            }
            fprintf(ctx->out, "jal %s\n", node->lexeme ? node->lexeme : "func");
            if (arg_count > 0) {
                fprintf(ctx->out, "addiu $sp, $sp, %d\n", (int)arg_count * 4);
            }
            break;
        }
        case AST_ASSIGN:
            if (node->children[0]->kind == AST_ARRAY_ACCESS) {
                emit_array_access_address(ctx, node->children[0]);
                fprintf(ctx->out, "move $t2, $v0\n");
                emit_expr(ctx, node->children[1]);
                fprintf(ctx->out, "sw $v0, 0($t2)\n");
            } else {
                emit_expr(ctx, node->children[1]);
                sym = symtab_lookup(&ctx->table, node->children[0]->lexeme);
                if (sym && sym->label) {
                    fprintf(ctx->out, "sw $v0, %s\n", sym->label);
                }
            }
            break;
        case AST_NEG:
            emit_expr(ctx, node->children[0]);
            fprintf(ctx->out, "subu $v0, $zero, $v0\n");
            break;
        case AST_NOT:
            emit_expr(ctx, node->children[0]);
            fprintf(ctx->out, "seq $v0, $v0, $zero\n");
            break;
        case AST_OR:
            emit_expr(ctx, node->children[0]);
            fprintf(ctx->out, "sne $t0, $v0, $zero\n");
            fprintf(ctx->out, "addiu $sp, $sp, -4\n");
            fprintf(ctx->out, "sw $t0, 0($sp)\n");
            emit_expr(ctx, node->children[1]);
            fprintf(ctx->out, "sne $t1, $v0, $zero\n");
            fprintf(ctx->out, "lw $t0, 0($sp)\n");
            fprintf(ctx->out, "addiu $sp, $sp, 4\n");
            fprintf(ctx->out, "or $v0, $t0, $t1\n");
            break;
        case AST_AND:
            emit_expr(ctx, node->children[0]);
            fprintf(ctx->out, "sne $t0, $v0, $zero\n");
            fprintf(ctx->out, "addiu $sp, $sp, -4\n");
            fprintf(ctx->out, "sw $t0, 0($sp)\n");
            emit_expr(ctx, node->children[1]);
            fprintf(ctx->out, "sne $t1, $v0, $zero\n");
            fprintf(ctx->out, "lw $t0, 0($sp)\n");
            fprintf(ctx->out, "addiu $sp, $sp, 4\n");
            fprintf(ctx->out, "and $v0, $t0, $t1\n");
            break;
        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
        case AST_EQ:
        case AST_NE:
        case AST_LT:
        case AST_GT:
        case AST_GE:
        case AST_LE:
            emit_expr(ctx, node->children[0]);
            fprintf(ctx->out, "move $t0, $v0\n");
            fprintf(ctx->out, "addiu $sp, $sp, -4\n");
            fprintf(ctx->out, "sw $t0, 0($sp)\n");
            emit_expr(ctx, node->children[1]);
            fprintf(ctx->out, "lw $t0, 0($sp)\n");
            fprintf(ctx->out, "addiu $sp, $sp, 4\n");
            switch (node->kind) {
                case AST_ADD: fprintf(ctx->out, "addu $v0, $t0, $v0\n"); break;
                case AST_SUB: fprintf(ctx->out, "subu $v0, $t0, $v0\n"); break;
                case AST_MUL: fprintf(ctx->out, "mul $v0, $t0, $v0\n"); break;
                case AST_DIV: fprintf(ctx->out, "div $t0, $v0\nmflo $v0\n"); break;
                case AST_EQ: fprintf(ctx->out, "seq $v0, $t0, $v0\n"); break;
                case AST_NE: fprintf(ctx->out, "sne $v0, $t0, $v0\n"); break;
                case AST_LT: fprintf(ctx->out, "slt $v0, $t0, $v0\n"); break;
                case AST_GT: fprintf(ctx->out, "sgt $v0, $t0, $v0\n"); break;
                case AST_GE: fprintf(ctx->out, "sge $v0, $t0, $v0\n"); break;
                case AST_LE: fprintf(ctx->out, "sle $v0, $t0, $v0\n"); break;
                default: break;
            }
            break;
        default:
            l1 = make_label("gv2_true", ctx->label_counter++);
            l2 = make_label("gv2_end", ctx->label_counter++);
            emit_expr(ctx, node->children[0]);
            fprintf(ctx->out, "bne $v0, $zero, %s\n", l1);
            fprintf(ctx->out, "li $v0, 0\n");
            fprintf(ctx->out, "j %s\n", l2);
            fprintf(ctx->out, "%s:\n", l1);
            fprintf(ctx->out, "li $v0, 1\n");
            fprintf(ctx->out, "%s:\n", l2);
            free(l1);
            free(l2);
            break;
    }
}

static void emit_block(CodegenContext *ctx, ASTNode *block, int push_scope) {
    ASTNode *decls;
    ASTNode *cmds;
    size_t i;

    if (!block || block->kind != AST_BLOCK) {
        return;
    }

    if (push_scope) {
        symtab_push_scope(&ctx->table);
    }
    decls = block_decl_list(block);
    cmds = block_cmd_list(block);
    declare_decl_items(ctx, decls);
    if (cmds && cmds->kind == AST_CMD_LIST) {
        for (i = 0; i < cmds->child_count; ++i) {
            emit_command(ctx, cmds->children[i]);
        }
    }
    if (push_scope) {
        symtab_pop_scope(&ctx->table);
    }
}

static void emit_command(CodegenContext *ctx, ASTNode *cmd) {
    Symbol *sym;
    char *l1;
    char *l2;

    if (!cmd) {
        return;
    }

    switch (cmd->kind) {
        case AST_EMPTY_STMT:
            break;
        case AST_EXPR_STMT:
            emit_expr(ctx, cmd->children[0]);
            break;
        case AST_READ:
            if (cmd->child_count > 0 && cmd->children[0]->kind == AST_ARRAY_ACCESS) {
                emit_array_access_address(ctx, cmd->children[0]);
                fprintf(ctx->out, "move $t2, $v0\n");
                fprintf(ctx->out, "li $v0, 5\nsyscall\n");
                fprintf(ctx->out, "sw $v0, 0($t2)\n");
            } else {
                sym = symtab_lookup(&ctx->table, cmd->children[0]->lexeme);
                if (sym && sym->label) {
                    if (sym->type == TYPE_CAR) {
                        fprintf(ctx->out, "li $v0, 12\nsyscall\n");
                    } else {
                        fprintf(ctx->out, "li $v0, 5\nsyscall\n");
                    }
                    fprintf(ctx->out, "sw $v0, %s\n", sym->label);
                }
            }
            break;
        case AST_WRITE_EXPR:
            emit_expr(ctx, cmd->children[0]);
            if (cmd->children[0] && cmd->children[0]->type == TYPE_CAR) {
                fprintf(ctx->out, "move $a0, $v0\nli $v0, 11\nsyscall\n");
            } else {
                fprintf(ctx->out, "move $a0, $v0\nli $v0, 1\nsyscall\n");
            }
            break;
        case AST_WRITE_STR:
            l1 = make_label("gv2_str", ctx->str_counter++);
            data_buf_printf(ctx, "%s: .asciiz %s\n", l1, cmd->lexeme ? cmd->lexeme : "\"\"");
            ensure_text(ctx);
            fprintf(ctx->out, "la $a0, %s\nli $v0, 4\nsyscall\n", l1);
            free(l1);
            break;
        case AST_NEWLINE:
            ensure_text(ctx);
            fprintf(ctx->out, "li $a0, 10\nli $v0, 11\nsyscall\n");
            break;
        case AST_RETURN:
            if (cmd->child_count > 0) {
                emit_expr(ctx, cmd->children[0]);
            }
            fprintf(ctx->out, "jr $ra\n");
            break;
        case AST_IF:
            l1 = make_label("gv2_if_end", ctx->label_counter++);
            emit_expr(ctx, cmd->children[0]);
            fprintf(ctx->out, "beq $v0, $zero, %s\n", l1);
            emit_command(ctx, cmd->children[1]);
            fprintf(ctx->out, "%s:\n", l1);
            free(l1);
            break;
        case AST_IF_ELSE:
            l1 = make_label("gv2_else", ctx->label_counter++);
            l2 = make_label("gv2_if_end", ctx->label_counter++);
            emit_expr(ctx, cmd->children[0]);
            fprintf(ctx->out, "beq $v0, $zero, %s\n", l1);
            emit_command(ctx, cmd->children[1]);
            fprintf(ctx->out, "j %s\n", l2);
            fprintf(ctx->out, "%s:\n", l1);
            emit_command(ctx, cmd->children[2]);
            fprintf(ctx->out, "%s:\n", l2);
            free(l1);
            free(l2);
            break;
        case AST_WHILE:
            l1 = make_label("gv2_while_begin", ctx->label_counter++);
            l2 = make_label("gv2_while_end", ctx->label_counter++);
            fprintf(ctx->out, "%s:\n", l1);
            emit_expr(ctx, cmd->children[0]);
            fprintf(ctx->out, "beq $v0, $zero, %s\n", l2);
            emit_command(ctx, cmd->children[1]);
            fprintf(ctx->out, "j %s\n", l1);
            fprintf(ctx->out, "%s:\n", l2);
            free(l1);
            free(l2);
            break;
        case AST_BLOCK:
            emit_block(ctx, cmd, 1);
            break;
        default:
            break;
    }
}

static void emit_function_params(CodegenContext *ctx, ASTNode *params) {
    size_t i;

    if (!params || params->kind != AST_PARAM_LIST) {
        return;
    }
    for (i = 0; i < params->child_count; ++i) {
        ASTNode *p = params->children[i];
        char *label;
        if (!p || p->kind != AST_PARAM) {
            continue;
        }
        label = make_label("gv2_param", ctx->var_counter++);
        if (!label) {
            continue;
        }
        data_buf_printf(ctx, "%s: .word 0\n", label);
        (void)symtab_declare_param(&ctx->table, p->lexeme, p->type, p->is_vector, p->vector_size);
        {
            Symbol *s = symtab_lookup(&ctx->table, p->lexeme);
            if (s) { free(s->label); s->label = label; label = NULL; }
        }
        free(label);
    }
}

static void emit_function(ASTNode *func, CodegenContext *ctx) {
    ASTNode *params;
    ASTNode *body;
    size_t pi;

    if (!func || func->kind != AST_FUNCTION_DECL) {
        return;
    }
    params = func->child_count > 0 ? func->children[0] : NULL;
    body = func->child_count > 1 ? func->children[1] : NULL;

    ensure_text(ctx);
    fprintf(ctx->out, "\n%s:\n", func->lexeme ? func->lexeme : "func");
    symtab_push_scope(&ctx->table);
    emit_function_params(ctx, params);
    if (params && params->kind == AST_PARAM_LIST && params->child_count > 0) {
        for (pi = 0; pi < params->child_count; ++pi) {
            ASTNode *p = params->children[pi];
            Symbol *sym = symtab_lookup(&ctx->table, p->lexeme);
            if (sym && sym->label) {
                fprintf(ctx->out, "lw $t3, %d($sp)\n", (int)(pi * 4));
                fprintf(ctx->out, "sw $t3, %s\n", sym->label);
            }
        }
    }
    emit_block(ctx, body, 0);
    fprintf(ctx->out, "jr $ra\n");
    symtab_pop_scope(&ctx->table);
}

static void emit_global_section(CodegenContext *ctx, ASTNode *section) {
    if (!section || section->kind != AST_GLOBAL_SECTION || section->child_count == 0) {
        return;
    }
    declare_decl_items(ctx, section->children[0]);
}

static void emit_function_section(CodegenContext *ctx, ASTNode *section) {
    size_t i;

    if (!section || section->kind != AST_FUNCTION_SECTION) {
        return;
    }
    for (i = 0; i < section->child_count; ++i) {
        emit_function(section->children[i], ctx);
    }
}

static void emit_main(CodegenContext *ctx, ASTNode *block) {
    ensure_text(ctx);
    fprintf(ctx->out, "\n.globl main\nmain:\n");
    symtab_push_scope(&ctx->table);
    emit_block(ctx, block, 0);
    fprintf(ctx->out, "li $v0, 10\nsyscall\n");
    symtab_pop_scope(&ctx->table);
}

static char *output_path_from_input(const char *input_path) {
    size_t len = strlen(input_path);
    char *out = (char *)malloc(len + 5);
    if (!out) {
        return NULL;
    }
    memcpy(out, input_path, len + 1);
    strcat(out, ".asm");
    return out;
}

int codegen_generate(ASTNode *root, const char *input_path) {
    CodegenContext ctx;
    char *out_path;
    size_t i;

    if (!root || root->kind != AST_PROGRAM) {
        return 0;
    }

    out_path = output_path_from_input(input_path);
    if (!out_path) {
        return 0;
    }

    ctx.out = fopen(out_path, "w");
    free(out_path);
    if (!ctx.out) {
        return 0;
    }

    symtab_init(&ctx.table);
    ctx.var_counter = 0;
    ctx.str_counter = 0;
    ctx.label_counter = 0;
    ctx.data_buf = NULL;
    ctx.data_buf_len = 0;
    ctx.data_buf_cap = 0;
    ctx.text_opened = 0;

    symtab_push_scope(&ctx.table);

    for (i = 0; i < root->child_count; ++i) {
        ASTNode *child = root->children[i];
        if (!child) {
            continue;
        }
        if (child->kind == AST_GLOBAL_SECTION) {
            emit_global_section(&ctx, child);
        } else if (child->kind == AST_FUNCTION_SECTION) {
            emit_function_section(&ctx, child);
        } else if (child->kind == AST_BLOCK) {
            emit_main(&ctx, child);
        }
    }

    flush_data(&ctx);
    symtab_free(&ctx.table);
    fclose(ctx.out);
    return 1;
}
