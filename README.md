# Implementação 6 - Memória Virtual

Simulador de gerenciador de memória virtual em C (*Designing a Virtual Memory Manager*, Silberschatz, 10ª ed.), com as modificações da disciplina Infraestrutura de Software.

- Endereço lógico de 16 bits: página de 256 bytes (`página = endereço / 256`, `offset = endereço % 256`).
- Memória física de **128 frames**, preenchidos do 0 ao 127; com a memória cheia, aplica-se `fifo` ou `lru`.
- **TLB de 16 entradas**, com substituição `fifo` ou `lru`; a busca usa **uma thread (pthread) por entrada**.
- Páginas lidas do `BACKING_STORE.bin` (bytes lidos como `signed char`).

## Ambiente

Desenvolvido no Windows 11, compilado e testado no WSL Ubuntu (gcc 15.2, GNU Make 4.4.1). Roda em Linux, Unix e macOS.

## Arquivos

| Arquivo | Conteúdo |
|---|---|
| `Makefile` | `make` gera o executável `vm`; `make clean` remove `vm` e `main.o`. |
| `main.c` | Todo o código (funções abaixo). |
| `evidencias.log` | Comandos e saídas reais dos testes de cada etapa do desenvolvimento. |
| `README.md` | Este arquivo. |

Funções de `main.c`:

- `algoritmo_valido`, `interpretar_endereco`, `validar_arquivo_enderecos`: validação de argumentos e do arquivo de endereços.
- `carregar_pagina`: lê uma página de 256 bytes do `BACKING_STORE.bin` para um frame.
- `frame_lru`, `selecionar_frame`: escolhem o frame (livre, FIFO ou LRU) em um page fault.
- `tlb_buscar_slot`, `tlb_consultar`: busca na TLB com uma thread por slot, resultado em `tlb_acerto[slot]`.
- `tlb_slot_lru`, `tlb_inserir`: escolhem o slot (FIFO ou LRU) e inserem a entrada na TLB.
- `traduzir_endereco`: TLB, tabela de páginas, page fault e cálculo do endereço físico e do valor.
- `main`: lê os endereços e grava o `correct.txt` com as estatísticas.

## Compilar

```
make
make clean
```

## Executar

```
./vm <arquivo_de_enderecos> <fifo|lru> <fifo|lru>
```

O 2º argumento é o algoritmo de substituição de **página** e o 3º o da **TLB**. Exemplo:

```
./vm addresses.txt lru fifo
```

O `BACKING_STORE.bin` (fornecido pelo professor) e o arquivo de endereços ficam fora do repositório. O programa abre `BACKING_STORE.bin` no **diretório atual** e grava o `correct.txt` nele.

## Saída

`correct.txt`, uma linha por endereço, seguida das estatísticas:

```
Virtual address: 16916 TLB: 0 Physical address: 20 Value: 0
...
Number of Translated Addresses = 1000
Page Faults = 539
Page Fault Rate = 0.539
TLB Hits = 54
TLB Hit Rate = 0.054
```

`TLB: N` é o índice (0 a 15) do slot em que a entrada ficou, seja por hit, seja por inserção após um miss.

## Tratamento de erros

O programa escreve a mensagem em `stderr`, sai com código 1 e **não cria** `correct.txt` quando:

1. o número de argumentos não é exatamente 3, ou o 2º/3º não é `fifo` ou `lru`;
2. o arquivo de endereços não existe, não abre, está vazio ou tem linha fora do formato (um inteiro de 0 a 65535 por linha, sem linhas em branco).

Também encerra com erro se o `BACKING_STORE.bin` não puder ser aberto.

## Decisões de implementação

- No LRU de página, todo acesso conta como uso, inclusive os que são hit na TLB.
- Quando uma página sai da memória, a entrada dela na TLB é invalidada.
- No LRU da TLB, um slot livre ou invalidado é usado primeiro (menor índice).
- Se um `pthread_create` falhar, o slot correspondente é conferido na thread principal.

## Testar

Com o `addresses.txt` e o `correct.txt` do professor (que usa `lru` na página e `fifo` na TLB) e o `BACKING_STORE.bin` no mesmo diretório do `vm`:

```
make
./vm addresses.txt lru fifo
cmp correct.txt correct_professor.txt
```

(`correct_professor.txt` é a cópia do `correct.txt` do professor, guardada com outro nome para não ser sobrescrita.) O `cmp` não deve imprimir nada. Todos os testes realizados estão em `evidencias.log`.
