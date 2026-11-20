#include <stdio.h>
#include <string.h>

static int valid_algorithm(const char *s)
{
    return strcmp(s, "fifo") == 0 || strcmp(s, "lru") == 0;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Erro: numero de argumentos incorreto\n");
        fprintf(stderr, "Uso: %s <arquivo_enderecos> <fifo|lru> <fifo|lru>\n", argv[0]);
        return 1;
    }
    if (!valid_algorithm(argv[2])) {
        fprintf(stderr, "Erro: algoritmo de substituicao de pagina deve ser fifo ou lru\n");
        return 1;
    }
    if (!valid_algorithm(argv[3])) {
        fprintf(stderr, "Erro: algoritmo de substituicao da TLB deve ser fifo ou lru\n");
        return 1;
    }
    return 0;
}
