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
