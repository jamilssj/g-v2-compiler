#include "symbol_table.h"

#include <stdlib.h>
#include <string.h>
/* Cria uma cópia dinâmica da string fornecida para armazenamento persistente na tabela. */
/* Retorna char* para o ponteiro da nova área alocada; o parâmetro s é a string original a ser duplicada. */
static char *dup_str(const char *s) {
    size_t len;                                      // Declara variável para armazenar o comprimento da string
    char *copy;                                      // Declara ponteiro para o novo bloco de memória alocado

    if (!s) {                                        // Verifica se o ponteiro de entrada fornecido é nulo
        return NULL;                                 // Retorna nulo para evitar erros de processamento
    }
    len = strlen(s);                                 // Calcula o tamanho da string original usando strlen
    copy = (char *)malloc(len + 1);                  // Aloca memória dinâmica incluindo o terminador '\0'
    if (!copy) {                                     // Valida se a alocação de memória pelo malloc falhou
        return NULL;                                 // Retorna nulo caso o sistema não tenha memória livre
    }
    memcpy(copy, s, len + 1);                        // Copia len + 1 bytes da origem s para o destino copy
    return copy;                                     // Retorna o ponteiro para a nova string duplicada
}

/* Libera toda a memória associada a um símbolo, incluindo nome, rótulo e parâmetros. */
/* Tipo void pois realiza apenas a desalocação; o parâmetro sym é o ponteiro para a estrutura do símbolo a ser destruída. */
static void symbol_free(Symbol *sym) {
    if (!sym) {                                      // Verifica se o ponteiro do símbolo recebido é nulo
        return;                                      // Encerra a execução da função imediatamente
    }
    free(sym->name);                                 // Libera a memória alocada para a string do nome
    free(sym->label);                                // Libera a memória alocada para a string do rótulo assembly
    paraminfo_free(sym->params);                     // Chama função interna para limpar a lista de parâmetros
    free(sym);                                       // Desaloca a estrutura principal do símbolo do heap
}

/* Percorre e libera recursivamente uma lista ligada de informações de parâmetros. */
/* Tipo void pois limpa a memória da lista; o parâmetro list é o ponteiro para o início da lista de parâmetros. */
void paraminfo_free(ParamInfo *list) {
    ParamInfo *next;                                 // Declara ponteiro auxiliar para rastrear o próximo item
    while (list) {                                   // Itera enquanto houver elementos na lista ligada
        next = list->next;                           // Salva o endereço do próximo nó antes de liberar o atual
        free(list->name);                            // Libera a string do nome armazenada no nó atual
        free(list);                                  // Desaloca a estrutura de informação de parâmetro atual
        list = next;                                 // Avança para o próximo item da lista salva anteriormente
    }
}

/* Aloca e inicializa uma nova estrutura de informação de parâmetro para uma função. */
/* Retorna ParamInfo* para o novo nó criado; name, type, is_vector e vector_size definem as propriedades do parâmetro. */
ParamInfo *paraminfo_new(const char *name, Gv1Type type, int is_vector, int vector_size) {
                                                    /* calloc aloca memória e inicializa todos os bytes com zero para segurança. */
    ParamInfo *p = (ParamInfo *)calloc(1, sizeof(ParamInfo));
    if (!p) {                                        // Verifica se a alocação de memória dinâmica falhou
        return NULL;                                 // Retorna nulo em caso de erro crítico de recursos
    }
    p->name = dup_str(name);                         // Realiza cópia profunda do nome usando dup_str interno
    p->type = type;                                  // Define o tipo de dado (int ou car) do parâmetro
    p->is_vector = is_vector;                        // Atribui a flag indicando se é um parâmetro vetorial
    p->vector_size = vector_size;                    // Armazena a dimensão física se for um vetor
    return p;                                        // Retorna o ponteiro para o novo parâmetro inicializado
}

/* Adiciona um novo nó de parâmetro ao final da lista de parâmetros fornecida. */
/* Tipo void pois modifica a lista existente; **list permite alterar o ponteiro da cabeça e *node é o item a anexar. */
void paraminfo_append(ParamInfo **list, ParamInfo *node) {
    ParamInfo *cur;                                  // Declara ponteiro auxiliar para percorrer a lista

    if (!list || !node) {                            // Verifica integridade dos ponteiros de entrada
        return;                                      // Sai da função se os dados fornecidos forem inválidos
    }
    if (!*list) {                                    // Verifica se a lista de parâmetros está atualmente vazia
        *list = node;                                // Define o nó fornecido como o primeiro item da lista
        return;                                      // Finaliza a operação de anexação inicial
    }
    cur = *list;                                     // Inicia o ponteiro de navegação na cabeça da lista
    while (cur->next) {                              // Percorre a lista até encontrar o último elemento atual
        cur = cur->next;                             // Avança o ponteiro auxiliar para o próximo nó
    }
    cur->next = node;                                // Conecta o novo parâmetro ao campo 'next' do último nó
}

/* Inicializa a pilha de tabelas de símbolos definindo o topo como vazio. */
/* Tipo void pois apenas zera o ponteiro de controle; o parâmetro stack é a estrutura da pilha a ser inicializada. */
void symtab_init(SymbolTableStack *stack) {
    stack->top = NULL;                               // Define o topo da pilha como nulo (inicialmente vazia)
}
/* Cria um novo nível de escopo e o empilha no topo da pilha de símbolos. */
/* Tipo void pois altera o estado da pilha global; o parâmetro stack é a pilha onde o novo escopo será inserido. */
void symtab_push_scope(SymbolTableStack *stack) {
                                                    /* Aloca memória para a estrutura Scope, zerando os campos automaticamente. */
    Scope *scope = (Scope *)calloc(1, sizeof(Scope));
    if (!scope) {                                    // Valida se a alocação do novo escopo foi bem-sucedida
        return;                                      // Aborta se não houver memória disponível no sistema
    }
    scope->next = stack->top;                        // Aponta o novo escopo para o antigo topo da pilha
    stack->top = scope;                              // Atualiza o topo da pilha para o novo escopo criado
}

/* Remove o escopo atual do topo da pilha, liberando todos os símbolos contidos nele. */
/* Tipo void pois realiza a destruição do nível; o parâmetro stack identifica a pilha de onde o escopo será retirado. */
void symtab_pop_scope(SymbolTableStack *stack) {
    Scope *scope;                                    // Ponteiro auxiliar para o escopo a ser removido
    Symbol *sym;                                     // Ponteiro para navegar na lista de símbolos do escopo
    Symbol *next;                                    // Ponteiro auxiliar para desalocação segura de símbolos

    if (!stack->top) {                               // Verifica se a pilha já está vazia
        return;                                      // Sai da função se não houver escopo para remover
    }

    scope = stack->top;                              // Captura o endereço do escopo atual no topo
    stack->top = scope->next;                        // Faz o topo da pilha apontar para o escopo pai
    sym = scope->symbols;                            // Inicia a limpeza pela lista de símbolos do escopo
    while (sym) {                                    // Itera sobre cada símbolo registrado neste nível
        next = sym->next;                            // Salva o próximo símbolo antes de destruir o atual
        symbol_free(sym);                            // Chama a função interna para liberar o registro do símbolo
        sym = next;                                  // Avança para o próximo item da lista ligada
    }
    free(scope);                                     // Libera a estrutura de cabeçalho do escopo desalocado
}

/* Desaloca toda a pilha de tabelas de símbolos, limpando todos os escopos remanescentes. */
/* Tipo void porque apenas realiza a limpeza da memória; o parâmetro stack aponta para a estrutura que gerencia a pilha de escopos. */
void symtab_free(SymbolTableStack *stack) {
                                                    /* Executa o desempilhamento sucessivo até que o ponteiro do topo seja nulo. */
    while (stack->top) {                             
        symtab_pop_scope(stack);                     // Remove e limpa o escopo atual em cada iteração
    }
}

/* Pesquisa um identificador apenas no escopo atual para verificar redeclarações locais. */
/* Retorna Symbol* para fornecer os dados do símbolo encontrado; stack identifica a pilha e name é a string do identificador buscado. */
Symbol *symtab_lookup_current(SymbolTableStack *stack, const char *name) {
    Symbol *sym;                                     // Ponteiro para iterar pelos símbolos do nível atual

    if (!stack->top) {                               // Checa se existe algum escopo ativo na pilha
        return NULL;                                 // Retorna nulo se não houver escopo para pesquisar
    }
    sym = stack->top->symbols;                       // Inicia busca na lista de símbolos do topo da pilha
    while (sym) {                                    // Percorre a lista ligada de símbolos do escopo atual
                                                        /* Compara o nome armazenado com o nome buscado (retorna 0 se idênticos). */
        if (strcmp(sym->name, name) == 0) {          
            return sym;                              // Retorna o ponteiro do símbolo se encontrado localmente
        }
        sym = sym->next;                             // Avança para o próximo símbolo na lista do escopo
    }
    return NULL;                                     // Retorna nulo se o nome não existir no escopo atual
}

/* Busca um símbolo percorrendo a pilha do escopo atual até o global seguindo as regras de visibilidade. */
/* Retorna Symbol* para acessar o registro visível no escopo; stack provê a hierarquia de busca e name é o nome a ser resolvido. */
Symbol *symtab_lookup(SymbolTableStack *stack, const char *name) {
    Scope *scope = stack->top;                       // Inicia a navegação a partir do escopo mais interno
    Symbol *sym;                                     // Ponteiro auxiliar para busca interna de símbolos

                                                    /* Itera através da hierarquia de escopos, do topo em direção à base (global). */
    while (scope) {                                  
        sym = scope->symbols;                        // Obtém a lista de símbolos do nível de escopo atual
        while (sym) {                                // Pesquisa exaustiva dentro do nível atual
                                                            /* Verifica se o lexema do símbolo atual coincide com o nome buscado. */
            if (strcmp(sym->name, name) == 0) {      
                return sym;                          // Retorna o símbolo mais próximo (respeitando o sombreamento)
            }
            sym = sym->next;                         // Passa para o próximo símbolo do mesmo nível
        }
        scope = scope->next;                         // Sobe para o escopo pai (envolvente) para continuar a busca
    }
    return NULL;                                     // Retorna nulo se o nome não for achado em nenhum escopo
}
/* Função interna: aloca e inicializa um símbolo no topo da pilha. Parâmetros: stack (alvo), name (lexema), kind (categoria), type (tipo), is_vector (flag), vector_size (tamanho), label (rótulo assembly). */
/* Retorna Symbol* para o novo objeto; stack indica o destino, name o identificador, kind/type a categoria e dados, e label o rótulo assembly. */
static Symbol *declare_symbol(SymbolTableStack *stack, const char *name, SymbolKind kind, Gv1Type type, int is_vector, int vector_size, const char *label) {
    Symbol *sym;                                     // Ponteiro local para o novo registro de símbolo

                                                    /* Verifica se existe um escopo ativo e se o identificador já foi declarado no nível atual. */
    if (!stack->top || symtab_lookup_current(stack, name)) {
        return NULL;                                 // Retorna nulo para evitar redeclaração no mesmo escopo
    }
    sym = (Symbol *)calloc(1, sizeof(Symbol));       // Aloca memória zerada para a estrutura do símbolo
    if (!sym) {                                      // Valida se o sistema conseguiu prover a memória
        return NULL;                                 // Aborta em caso de falha crítica de recursos
    }
    sym->name = dup_str(name);                       // Armazena uma cópia permanente do nome (identificador)
    sym->kind = kind;                                // Define a categoria (variável, parâmetro ou função)
    sym->type = type;                                // Define o tipo básico associado (int ou car)
    sym->is_vector = is_vector;                      // Atribui a flag indicando se o símbolo é um vetor
    sym->vector_size = vector_size;                  // Registra a dimensão física para vetores escalares
    sym->label = dup_str(label);                     // Salva o rótulo MIPS (usado para variáveis globais)
                                                    /* Valida se as duplicações de strings obrigatórias foram processadas com sucesso. */
    if (!sym->name || (label && !sym->label)) {
        symbol_free(sym);                            // Libera a memória parcial em caso de erro na string
        return NULL;                                 // Retorna nulo por falha de alocação de texto
    }
    sym->next = stack->top->symbols;                 // Conecta o novo símbolo ao início da lista do escopo
    stack->top->symbols = sym;                       // Atualiza a cabeça da lista de símbolos do escopo
    return sym;                                      // Retorna o ponteiro para o símbolo recém-registrado
}

/* Cria uma nova entrada de variável na tabela de símbolos do escopo atual. */
/* Retorna Symbol* para o registro da variável; stack é a pilha e os demais campos definem as propriedades físicas e lógicas da variável. */
Symbol *symtab_declare_var(SymbolTableStack *stack, const char *name, Gv1Type type, int is_vector, int vector_size, const char *label) {
                                                    /* Invoca o declarador genérico especificando a categoria como variável (SYM_VAR). */
    return declare_symbol(stack, name, SYM_VAR, type, is_vector, vector_size, label);
}

/* Registra um novo parâmetro de função no escopo atual da tabela de símbolos. */
/* Retorna Symbol* para o parâmetro; stack situa a inserção e name/type/is_vector especificam a interface formal da sub-rotina. */
Symbol *symtab_declare_param(SymbolTableStack *stack, const char *name, Gv1Type type, int is_vector, int vector_size) {
                                                    /* Invoca o declarador genérico configurando a categoria como parâmetro (SYM_PARAM). */
    return declare_symbol(stack, name, SYM_PARAM, type, is_vector, vector_size, NULL);
}

/* Declara uma função, armazenando seu tipo de retorno e a lista detalhada de seus parâmetros. */
/* Retorna Symbol* para a função; stack é a pilha, name/type o nome e retorno, e param_count/params detalham a assinatura. */
Symbol *symtab_declare_func(SymbolTableStack *stack, const char *name, Gv1Type type, int param_count, ParamInfo *params) {
                                                    /* Cria inicialmente o símbolo da função como um tipo escalar (is_vector=0). */
    Symbol *sym = declare_symbol(stack, name, SYM_FUNC, type, 0, -1, NULL);
    if (!sym) {                                      // Verifica se houve falha na criação do símbolo base
        return NULL;                                 // Retorna nulo caso o nome já esteja em uso no escopo
    }
    sym->param_count = param_count;                  // Registra a aridade (número de argumentos) da função
    sym->params = params;                            // Associa a lista ligada de metadados dos parâmetros
    return sym;                                      // Retorna o registro completo da assinatura da função
}
