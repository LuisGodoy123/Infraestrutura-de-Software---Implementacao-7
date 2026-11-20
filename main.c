#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_ADDRESS 65535

static int valid_algorithm(const char *s)
{
    return strcmp(s, "fifo") == 0 || strcmp(s, "lru") == 0;
}

static int parse_address(const char *line, long *out)
{
    char *end;
    long v;

    while (isspace((unsigned char)*line))
        line++;
    if (!isdigit((unsigned char)*line))
        return 0;
    v = strtol(line, &end, 10);
    while (isspace((unsigned char)*end))
        end++;
    if (*end != '\0' || v > MAX_ADDRESS)
        return 0;
    *out = v;
    return 1;
}

static int validate_address_file(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[64];
    long v;
    long count = 0;

    if (f == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo de enderecos '%s'\n", path);
        return 0;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        count++;
        if ((strchr(line, '\n') == NULL && !feof(f)) || !parse_address(line, &v)) {
            fprintf(stderr, "Erro: linha %ld de '%s' fora do formato (esperado um inteiro de 0 a %d)\n",
                    count, path, MAX_ADDRESS);
            fclose(f);
            return 0;
        }
    }
    fclose(f);
    if (count == 0) {
        fprintf(stderr, "Erro: o arquivo '%s' nao contem enderecos\n", path);
        return 0;
    }
    return 1;
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
    if (!validate_address_file(argv[1]))
        return 1;
    return 0;
}
