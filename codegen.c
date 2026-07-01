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
