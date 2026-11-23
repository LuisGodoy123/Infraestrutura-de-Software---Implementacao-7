#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

#define ENDERECO_MAXIMO 65535
#define TAMANHO_PAGINA 256
#define NUM_PAGINAS 256
#define NUM_FRAMES 128
#define TAMANHO_TLB 16
#define ARQUIVO_BACKING_STORE "BACKING_STORE.bin"
#define ARQUIVO_SAIDA "correct.txt"

static signed char memoria[NUM_FRAMES * TAMANHO_PAGINA];
static int tabela_paginas[NUM_PAGINAS];
static int pagina_do_frame[NUM_FRAMES];
static unsigned long uso_frame[NUM_FRAMES];
static unsigned long relogio;
static int proximo_frame_livre;
static int proximo_fifo;
static int usa_lru;
static int tlb_pagina[TAMANHO_TLB];
static int tlb_frame[TAMANHO_TLB];
static unsigned long tlb_uso[TAMANHO_TLB];
static int tlb_acerto[TAMANHO_TLB];
static int tlb_proximo;
static int tlb_usa_lru;

struct argumento_busca {
    int slot;
    int pagina;
};
static long faltas_pagina;
static long acertos_tlb;
static FILE *backing_store;

static int algoritmo_valido(const char *texto)
{
    return strcmp(texto, "fifo") == 0 || strcmp(texto, "lru") == 0;
}

static int interpretar_endereco(const char *linha, long *saida)
{
    char *fim;
    long valor;

    while (isspace((unsigned char)*linha))
        linha++;
    if (!isdigit((unsigned char)*linha))
        return 0;
    valor = strtol(linha, &fim, 10);
    while (isspace((unsigned char)*fim))
        fim++;
    if (*fim != '\0' || valor > ENDERECO_MAXIMO)
        return 0;
    *saida = valor;
    return 1;
}

static int validar_arquivo_enderecos(const char *caminho)
{
    FILE *arquivo = fopen(caminho, "r");
    char linha[64];
    long valor;
    long contagem = 0;

    if (arquivo == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo de enderecos '%s'\n", caminho);
        return 0;
    }
    while (fgets(linha, sizeof linha, arquivo) != NULL) {
        contagem++;
        if ((strchr(linha, '\n') == NULL && !feof(arquivo)) || !interpretar_endereco(linha, &valor)) {
            fprintf(stderr, "Erro: linha %ld de '%s' fora do formato (esperado um inteiro de 0 a %d)\n",
                    contagem, caminho, ENDERECO_MAXIMO);
            fclose(arquivo);
            return 0;
        }
    }
    fclose(arquivo);
    if (contagem == 0) {
        fprintf(stderr, "Erro: o arquivo '%s' nao contem enderecos\n", caminho);
        return 0;
    }
    return 1;
}

static int carregar_pagina(int pagina, int frame)
{
    if (fseek(backing_store, (long)pagina * TAMANHO_PAGINA, SEEK_SET) != 0)
        return 0;
    return fread(&memoria[frame * TAMANHO_PAGINA], 1, TAMANHO_PAGINA, backing_store) == TAMANHO_PAGINA;
}

static int frame_lru(void)
{
    int frame, vitima = 0;

    for (frame = 1; frame < NUM_FRAMES; frame++)
        if (uso_frame[frame] < uso_frame[vitima])
            vitima = frame;
    return vitima;
}

static int selecionar_frame(void)
{
    int frame, i;

    if (proximo_frame_livre < NUM_FRAMES)
        return proximo_frame_livre++;
    if (usa_lru) {
        frame = frame_lru();
    } else {
        frame = proximo_fifo;
        proximo_fifo = (proximo_fifo + 1) % NUM_FRAMES;
    }
    tabela_paginas[pagina_do_frame[frame]] = -1;
    for (i = 0; i < TAMANHO_TLB; i++)
        if (tlb_pagina[i] == pagina_do_frame[frame]) {
            tlb_pagina[i] = -1;
            tlb_uso[i] = 0;
        }
    return frame;
}

static void *tlb_buscar_slot(void *parametro)
{
    struct argumento_busca *argumento = parametro;

    tlb_acerto[argumento->slot] = tlb_pagina[argumento->slot] == argumento->pagina;
    return NULL;
}

static int tlb_consultar(int pagina)
{
    pthread_t threads[TAMANHO_TLB];
    struct argumento_busca argumentos[TAMANHO_TLB];
    int iniciada[TAMANHO_TLB];
    int i;

    for (i = 0; i < TAMANHO_TLB; i++) {
        argumentos[i].slot = i;
        argumentos[i].pagina = pagina;
        iniciada[i] = pthread_create(&threads[i], NULL, tlb_buscar_slot, &argumentos[i]) == 0;
        if (!iniciada[i])
            tlb_buscar_slot(&argumentos[i]);
    }
    for (i = 0; i < TAMANHO_TLB; i++)
        if (iniciada[i])
            pthread_join(threads[i], NULL);
    for (i = 0; i < TAMANHO_TLB; i++)
        if (tlb_acerto[i])
            return i;
    return -1;
}

static int tlb_slot_lru(void)
{
    int i, vitima = 0;

    for (i = 1; i < TAMANHO_TLB; i++)
        if (tlb_uso[i] < tlb_uso[vitima])
            vitima = i;
    return vitima;
}

static int tlb_inserir(int pagina, int frame)
{
    int slot;

    if (tlb_usa_lru) {
        slot = tlb_slot_lru();
    } else {
        slot = tlb_proximo;
        tlb_proximo = (tlb_proximo + 1) % TAMANHO_TLB;
    }
    tlb_pagina[slot] = pagina;
    tlb_frame[slot] = frame;
    return slot;
}

static int traduzir_endereco(long endereco, int *slot_tlb, int *fisico, signed char *valor)
{
    int pagina = endereco / TAMANHO_PAGINA;
    int deslocamento = endereco % TAMANHO_PAGINA;
    int frame, slot;

    slot = tlb_consultar(pagina);
    if (slot >= 0) {
        acertos_tlb++;
        frame = tlb_frame[slot];
    } else {
        if (tabela_paginas[pagina] == -1) {
            faltas_pagina++;
            frame = selecionar_frame();
            if (!carregar_pagina(pagina, frame))
                return 0;
            tabela_paginas[pagina] = frame;
            pagina_do_frame[frame] = pagina;
        }
        frame = tabela_paginas[pagina];
        slot = tlb_inserir(pagina, frame);
    }
    uso_frame[frame] = ++relogio;
    tlb_uso[slot] = relogio;
    *slot_tlb = slot;
    *fisico = frame * TAMANHO_PAGINA + deslocamento;
    *valor = memoria[*fisico];
    return 1;
}

int main(int argc, char **argv)
{
    FILE *arquivo_enderecos, *saida;
    char linha[64];
    long endereco, total = 0;
    int fisico, slot, i;
    signed char valor;

    if (argc != 4) {
        fprintf(stderr, "Erro: numero de argumentos incorreto\n");
        fprintf(stderr, "Uso: %s <arquivo_enderecos> <fifo|lru> <fifo|lru>\n", argv[0]);
        return 1;
    }
    if (!algoritmo_valido(argv[2])) {
        fprintf(stderr, "Erro: algoritmo de substituicao de pagina deve ser fifo ou lru\n");
        return 1;
    }
    if (!algoritmo_valido(argv[3])) {
        fprintf(stderr, "Erro: algoritmo de substituicao da TLB deve ser fifo ou lru\n");
        return 1;
    }
    if (!validar_arquivo_enderecos(argv[1]))
        return 1;
    backing_store = fopen(ARQUIVO_BACKING_STORE, "rb");
    if (backing_store == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir '%s'\n", ARQUIVO_BACKING_STORE);
        return 1;
    }
    usa_lru = strcmp(argv[2], "lru") == 0;
    tlb_usa_lru = strcmp(argv[3], "lru") == 0;
    for (i = 0; i < NUM_PAGINAS; i++)
        tabela_paginas[i] = -1;
    for (i = 0; i < TAMANHO_TLB; i++)
        tlb_pagina[i] = -1;
    saida = fopen(ARQUIVO_SAIDA, "w");
    if (saida == NULL) {
        fprintf(stderr, "Erro: nao foi possivel criar '%s'\n", ARQUIVO_SAIDA);
        return 1;
    }
    arquivo_enderecos = fopen(argv[1], "r");
    while (fgets(linha, sizeof linha, arquivo_enderecos) != NULL) {
        interpretar_endereco(linha, &endereco);
        if (!traduzir_endereco(endereco, &slot, &fisico, &valor)) {
            fprintf(stderr, "Erro: falha ao ler '%s'\n", ARQUIVO_BACKING_STORE);
            return 1;
        }
        fprintf(saida, "Virtual address: %ld TLB: %d Physical address: %d Value: %d\n",
                endereco, slot, fisico, valor);
        total++;
    }
    fprintf(saida, "Number of Translated Addresses = %ld\n", total);
    fprintf(saida, "Page Faults = %ld\n", faltas_pagina);
    fprintf(saida, "Page Fault Rate = %.3f\n", (double)faltas_pagina / total);
    fprintf(saida, "TLB Hits = %ld\n", acertos_tlb);
    fprintf(saida, "TLB Hit Rate = %.3f\n", (double)acertos_tlb / total);
    fclose(saida);
    fclose(arquivo_enderecos);
    fclose(backing_store);
    return 0;
}
