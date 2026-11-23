#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_ADDRESS 65535
#define PAGE_SIZE 256
#define NUM_PAGES 256
#define NUM_FRAMES 128
#define TLB_SIZE 16
#define BACKING_STORE_FILE "BACKING_STORE.bin"
#define OUTPUT_FILE "correct.txt"

static signed char memory[NUM_FRAMES * PAGE_SIZE];
static int page_table[NUM_PAGES];
static int frame_page[NUM_FRAMES];
static unsigned long frame_used[NUM_FRAMES];
static unsigned long tick;
static int next_frame;
static int fifo_next;
static int use_lru;
static int tlb_page[TLB_SIZE];
static int tlb_frame[TLB_SIZE];
static int tlb_next;
static long page_faults;
static long tlb_hits;
static FILE *backing_store;

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

static int load_page(int page, int frame)
{
    if (fseek(backing_store, (long)page * PAGE_SIZE, SEEK_SET) != 0)
        return 0;
    return fread(&memory[frame * PAGE_SIZE], 1, PAGE_SIZE, backing_store) == PAGE_SIZE;
}

static int lru_frame(void)
{
    int frame, victim = 0;

    for (frame = 1; frame < NUM_FRAMES; frame++)
        if (frame_used[frame] < frame_used[victim])
            victim = frame;
    return victim;
}

static int select_frame(void)
{
    int frame, i;

    if (next_frame < NUM_FRAMES)
        return next_frame++;
    if (use_lru) {
        frame = lru_frame();
    } else {
        frame = fifo_next;
        fifo_next = (fifo_next + 1) % NUM_FRAMES;
    }
    page_table[frame_page[frame]] = -1;
    for (i = 0; i < TLB_SIZE; i++)
        if (tlb_page[i] == frame_page[frame])
            tlb_page[i] = -1;
    return frame;
}

static int tlb_lookup(int page)
{
    int i;

    for (i = 0; i < TLB_SIZE; i++)
        if (tlb_page[i] == page)
            return i;
    return -1;
}

static int tlb_insert(int page, int frame)
{
    int slot = tlb_next;

    tlb_next = (tlb_next + 1) % TLB_SIZE;
    tlb_page[slot] = page;
    tlb_frame[slot] = frame;
    return slot;
}

static int translate_address(long address, int *tlb_slot, int *physical, signed char *value)
{
    int page = address / PAGE_SIZE;
    int offset = address % PAGE_SIZE;
    int frame, slot;

    slot = tlb_lookup(page);
    if (slot >= 0) {
        tlb_hits++;
        frame = tlb_frame[slot];
    } else {
        if (page_table[page] == -1) {
            page_faults++;
            frame = select_frame();
            if (!load_page(page, frame))
                return 0;
            page_table[page] = frame;
            frame_page[frame] = page;
        }
        frame = page_table[page];
        slot = tlb_insert(page, frame);
    }
    frame_used[frame] = ++tick;
    *tlb_slot = slot;
    *physical = frame * PAGE_SIZE + offset;
    *value = memory[*physical];
    return 1;
}

int main(int argc, char **argv)
{
    FILE *f, *out;
    char line[64];
    long address, total = 0;
    int physical, slot, i;
    signed char value;

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
    backing_store = fopen(BACKING_STORE_FILE, "rb");
    if (backing_store == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir '%s'\n", BACKING_STORE_FILE);
        return 1;
    }
    use_lru = strcmp(argv[2], "lru") == 0;
    for (i = 0; i < NUM_PAGES; i++)
        page_table[i] = -1;
    for (i = 0; i < TLB_SIZE; i++)
        tlb_page[i] = -1;
    out = fopen(OUTPUT_FILE, "w");
    if (out == NULL) {
        fprintf(stderr, "Erro: nao foi possivel criar '%s'\n", OUTPUT_FILE);
        return 1;
    }
    f = fopen(argv[1], "r");
    while (fgets(line, sizeof line, f) != NULL) {
        parse_address(line, &address);
        if (!translate_address(address, &slot, &physical, &value)) {
            fprintf(stderr, "Erro: falha ao ler '%s'\n", BACKING_STORE_FILE);
            return 1;
        }
        fprintf(out, "Virtual address: %ld TLB: %d Physical address: %d Value: %d\n",
                address, slot, physical, value);
        total++;
    }
    fprintf(out, "Number of Translated Addresses = %ld\n", total);
    fprintf(out, "Page Faults = %ld\n", page_faults);
    fprintf(out, "Page Fault Rate = %.3f\n", (double)page_faults / total);
    fprintf(out, "TLB Hits = %ld\n", tlb_hits);
    fprintf(out, "TLB Hit Rate = %.3f\n", (double)tlb_hits / total);
    fclose(out);
    fclose(f);
    fclose(backing_store);
    return 0;
}
