#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SUSPEITOS 10
#define HASH_SIZE 37   // número primo pequeno para buckets da hash

// -----------------------------
// STRUCTS
// -----------------------------

// Nó da árvore de salas (mapa da mansão)
typedef struct Sala {
    char nome[60];
    char pista[120];     // pista associada (string vazia se não houver)
    struct Sala* esquerda;
    struct Sala* direita;
} Sala;

// Nó da BST de pistas coletadas
typedef struct PistaNode {
    char pista[120];
    struct PistaNode* esquerda;
    struct PistaNode* direita;
} PistaNode;

// Nó para encadeamento na tabela hash (pista -> suspeito)
typedef struct HashNode {
    char pista[120];
    char suspeito[60];
    struct HashNode* proximo;
} HashNode;

// Estrutura de contagem de pistas por suspeito (facilita verificação final)
typedef struct {
    char nome[60];
    int contador;
} SuspeitoCount;


// -----------------------------
// FUNÇÕES AUXILIARES
// -----------------------------

// Cria dinamicamente uma sala (cômodo) com nome e pista opcional
Sala* criarSala(const char* nome, const char* pista) {
    Sala* s = (Sala*) malloc(sizeof(Sala));
    if (!s) { perror("malloc"); exit(1); }
    strncpy(s->nome, nome, sizeof(s->nome)-1);
    s->nome[sizeof(s->nome)-1] = '\0';
    if (pista != NULL) strncpy(s->pista, pista, sizeof(s->pista)-1);
    else s->pista[0] = '\0';
    s->pista[sizeof(s->pista)-1] = '\0';
    s->esquerda = s->direita = NULL;
    return s;
}

// Cria um nó da BST para pista
PistaNode* criarPistaNode(const char* pista) {
    PistaNode* n = (PistaNode*) malloc(sizeof(PistaNode));
    if (!n) { perror("malloc"); exit(1); }
    strncpy(n->pista, pista, sizeof(n->pista)-1);
    n->pista[sizeof(n->pista)-1] = '\0';
    n->esquerda = n->direita = NULL;
    return n;
}

// Função de comparação de strings usada no BST (caso-insensível opcional)
int cmpPista(const char* a, const char* b) {
    return strcmp(a, b);
}

// Insere pista na BST (evita duplicatas). Retorna raiz atualizada.
PistaNode* inserirPistaBST(PistaNode* raiz, const char* pista) {
    if (pista == NULL || pista[0] == '\0') return raiz; // não insere vazias
    if (raiz == NULL) return criarPistaNode(pista);

    int cmp = cmpPista(pista, raiz->pista);
    if (cmp < 0) raiz->esquerda = inserirPistaBST(raiz->esquerda, pista);
    else if (cmp > 0) raiz->direita = inserirPistaBST(raiz->direita, pista);
    // se igual: já está inserida -> não duplicar
    return raiz;
}

// Mostra pistas em ordem alfabética (in-order)
void exibirPistasInOrder(PistaNode* raiz) {
    if (!raiz) return;
    exibirPistasInOrder(raiz->esquerda);
    printf(" - %s\n", raiz->pista);
    exibirPistasInOrder(raiz->direita);
}

// -----------------------------
// TABELA HASH SIMPLES (ENCAD. SEPARADO)
// -----------------------------

// Hash simples djb2 (string -> índice)
unsigned long hash_djb2(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + (unsigned char)c; /* hash * 33 + c */
    return hash;
}

// Inserir par (pista -> suspeito) na tabela hash (substitui se já existir)
void inserirNaHash(HashNode* tabela[], const char* pista, const char* suspeito) {
    if (!pista || pista[0] == '\0') return;
    unsigned long h = hash_djb2(pista) % HASH_SIZE;

    // procurar se já existe -> atualizar
    HashNode* cur = tabela[h];
    while (cur) {
        if (strcmp(cur->pista, pista) == 0) {
            // substituir valor
            strncpy(cur->suspeito, suspeito, sizeof(cur->suspeito)-1);
            cur->suspeito[sizeof(cur->suspeito)-1] = '\0';
            return;
        }
        cur = cur->proximo;
    }

    // não existe -> inserir no início da lista do bucket
    HashNode* novo = (HashNode*) malloc(sizeof(HashNode));
    if (!novo) { perror("malloc"); exit(1); }
    strncpy(novo->pista, pista, sizeof(novo->pista)-1);
    novo->pista[sizeof(novo->pista)-1] = '\0';
    strncpy(novo->suspeito, suspeito, sizeof(novo->suspeito)-1);
    novo->suspeito[sizeof(novo->suspeito)-1] = '\0';
    novo->proximo = tabela[h];
    tabela[h] = novo;
}

// Encontrar suspeito associado a uma pista (retorna NULL se não achar)
const char* encontrarSuspeito(HashNode* tabela[], const char* pista) {
    if (!pista || pista[0] == '\0') return NULL;
    unsigned long h = hash_djb2(pista) % HASH_SIZE;
    HashNode* cur = tabela[h];
    while (cur) {
        if (strcmp(cur->pista, pista) == 0) return cur->suspeito;
        cur = cur->proximo;
    }
    return NULL;
}

// Limpar tabela hash da memória
void liberarTabelaHash(HashNode* tabela[]) {
    for (int i = 0; i < HASH_SIZE; i++) {
        HashNode* cur = tabela[i];
        while (cur) {
            HashNode* tmp = cur;
            cur = cur->proximo;
            free(tmp);
        }
        tabela[i] = NULL;
    }
}

// -----------------------------
// CONTADORES DE SUSPEITOS
// -----------------------------

// Procura índice do suspeito na lista; se não existir e houver espaço, adiciona.
int idxSuspeito(SuspeitoCount list[], int *n, const char* nome) {
    for (int i = 0; i < *n; i++) {
        if (strcmp(list[i].nome, nome) == 0) return i;
    }
    // adicionar novo, se houver espaço
    if (*n < MAX_SUSPEITOS) {
        strncpy(list[*n].nome, nome, sizeof(list[*n].nome)-1);
        list[*n].nome[sizeof(list[*n].nome)-1] = '\0';
        list[*n].contador = 0;
        (*n)++;
        return (*n) - 1;
    }
    return -1;
}

// -----------------------------
// EXPLORAÇÃO DA MANSÃO (INTERATIVA)
// -----------------------------
// Ao entrar em uma sala, se existir pista, insere a pista na BST e
// consulta a hash para identificar o suspeito e incrementar contador.
// -----------------------------
void explorarSalas(Sala* atual, PistaNode** pistasColetadas, HashNode* tabelaHash[], SuspeitoCount suspeitos[], int *numSuspeitos) {
    char opc;

    while (1) {
        printf("\nVocê entrou em: %s\n", atual->nome);

        // coleta automática da pista caso exista
        if (atual->pista[0] != '\0') {
            printf("Pista encontrada: \"%s\"\n", atual->pista);
            // inserir na BST (evita duplicatas)
            *pistasColetadas = inserirPistaBST(*pistasColetadas, atual->pista);
            // procurar suspeito associado a essa pista na hash
            const char* suspeito = encontrarSuspeito(tabelaHash, atual->pista);
            if (suspeito != NULL) {
                int idx = idxSuspeito(suspeitos, numSuspeitos, suspeito);
                if (idx >= 0) {
                    suspeitos[idx].contador++;
                    printf("  (Esta pista aponta para o suspeito: %s) [total: %d]\n", suspeitos[idx].nome, suspeitos[idx].contador);
                }
            } else {
                printf("  (Nenhum suspeito associado a esta pista no banco de dados.)\n");
            }
        } else {
            printf("Nenhuma pista nesta sala.\n");
        }

        // Opções de navegação
        printf("\nOpções:\n");
        if (atual->esquerda) printf("  (e) Ir para a esquerda -> %s\n", atual->esquerda->nome);
        if (atual->direita)  printf("  (d) Ir para a direita -> %s\n", atual->direita->nome);
        printf("  (s) Sair e ir ao julgamento\n");
        printf("Escolha: ");
        scanf(" %c", &opc);

        if (opc == 'e' && atual->esquerda) {
            atual = atual->esquerda;
        } else if (opc == 'd' && atual->direita) {
            atual = atual->direita;
        } else if (opc == 's') {
            printf("\nEncerrando exploração. Indo ao julgamento...\n");
            return;
        } else {
            printf("Opção inválida. Tente novamente.\n");
        }
    }
}

// -----------------------------
// VERIFICAÇÃO FINAL DO SUSPEITO
// - exige pelo menos 2 pistas apontando para o mesmo suspeito
// -----------------------------
void verificarSuspeitoFinal(SuspeitoCount suspeitos[], int numSuspeitos, const char* acusado) {
    if (acusado == NULL || acusado[0] == '\0') {
        printf("Entrada inválida.\n");
        return;
    }

    // procurar acusado na lista de suspeitos e verificar contador
    for (int i = 0; i < numSuspeitos; i++) {
        if (strcmp(suspeitos[i].nome, acusado) == 0) {
            printf("\nSuspeito acusado: %s\nPistas que apontam para ele: %d\n", acusado, suspeitos[i].contador);
            if (suspeitos[i].contador >= 2) {
                printf("Resultado: Há evidências suficientes! Acusação válida.\n");
            } else {
                printf("Resultado: Pistas insuficientes para sustentar a acusação.\n");
            }
            return;
        }
    }
    printf("\nO suspeito \"%s\" não possui pistas associadas coletadas durante a exploração.\n", acusado);
}

// -----------------------------
// FUNÇÃO MAIN: monta mansão, preenche hash com relações pista->suspeito,
// inicia exploração e realiza julgamento.
// -----------------------------
int main() {
    // --- montar mansão (árvore fixa) ---
    Sala* hall = criarSala("Hall de Entrada", "Pegadas de lama");
    Sala* salaEstar = criarSala("Sala de Estar", "Foto rasgada");
    Sala* cozinha = criarSala("Cozinha", "Copo quebrado");
    Sala* biblioteca = criarSala("Biblioteca", "Livro arrancado da estante");
    Sala* jardim = criarSala("Jardim", "Fósforo queimado");
    Sala* adega = criarSala("Adega", "Cheiro de perfume forte");
    Sala* escritorio = criarSala("Escritório", "Bilhete amassado");

    // montar ligações
    hall->esquerda = salaEstar;
    hall->direita = cozinha;

    salaEstar->esquerda = biblioteca;
    salaEstar->direita = jardim;

    cozinha->esquerda = adega;
    cozinha->direita = escritorio;

    // --- tabela hash inicializada ---
    HashNode* tabelaHash[HASH_SIZE];
    for (int i = 0; i < HASH_SIZE; i++) tabelaHash[i] = NULL;

    // --- preencher hash com associações pista -> suspeito (pré-definidas) ---
    // Ex.: "Pegadas de lama" -> "Sr. Verde"
    inserirNaHash(tabelaHash, "Pegadas de lama", "Sr. Verde");
    inserirNaHash(tabelaHash, "Foto rasgada", "Sra. Azul");
    inserirNaHash(tabelaHash, "Copo quebrado", "Sr. Vermelho");
    inserirNaHash(tabelaHash, "Livro arrancado da estante", "Sra. Azul");
    inserirNaHash(tabelaHash, "Fósforo queimado", "Sr. Verde");
    inserirNaHash(tabelaHash, "Cheiro de perfume forte", "Sra. Amarela");
    inserirNaHash(tabelaHash, "Bilhete amassado", "Sr. Vermelho");

    // --- estruturas de coleta ---
    PistaNode* pistasColetadas = NULL;
    SuspeitoCount suspeitos[MAX_SUSPEITOS];
    int numSuspeitos = 0;

    printf("=== Detective Quest - Capítulo Final (Julgamento) ===\n");
    printf("Explore a mansão, colete pistas e acuse o culpado!\n");

    explorarSalas(hall, &pistasColetadas, tabelaHash, suspeitos, &numSuspeitos);

    // --- Ao final, exibir pistas coletadas ---
    printf("\n========================================\n");
    printf("PISTAS COLETADAS (ordem alfabética):\n");
    if (pistasColetadas == NULL) printf("Nenhuma pista coletada.\n");
    else exibirPistasInOrder(pistasColetadas);

    // --- mostrar resumo por suspeito ---
    printf("\nRESUMO DE PISTAS POR SUSPEITO:\n");
    if (numSuspeitos == 0) {
        printf("Nenhum suspeito identificado a partir das pistas coletadas.\n");
    } else {
        for (int i = 0; i < numSuspeitos; i++) {
            printf(" - %s : %d pista(s)\n", suspeitos[i].nome, suspeitos[i].contador);
        }
    }

    // --- fase de acusação ---
    char acusado[60];
    printf("\nDigite o nome do suspeito que deseja acusar (ex: \"Sr. Verde\"): ");
    // limpar newline pendente antes de fgets
    getchar();
    fgets(acusado, sizeof(acusado), stdin);
    acusado[strcspn(acusado, "\n")] = '\0';

    verificarSuspeitoFinal(suspeitos, numSuspeitos, acusado);

    // liberar memória da tabela hash
    liberarTabelaHash(tabelaHash);

    // Observação: liberação completa da BST e das salas (malloc) não mostrada aqui por brevidade,
    // mas pode ser adicionada com funções recursivas para free() caso deseje.
    printf("\nFim do jogo. Obrigado por jogar!\n");

    return 0;
}
