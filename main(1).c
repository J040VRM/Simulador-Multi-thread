#include <stdio.h>
#include <stdlib.h>

// Definições para facilitar legibilidade de algoritmos
#define FIFO 0
#define LRU 1
#define CLOCK 2
#define RANDOM 3
#define CUSTOM 4

// Representa uma entrada da tabela de páginas de um processo
typedef struct {
    int presente;         // 1 se está na memória, 0 se não
    int frame;            // Índice do frame onde está (ou -1)
    int modificada;       // 1 se foi modificada
    int referenciada;     // 1 se foi acessada recentemente (usado no CLOCK)
    int tempo_carga;      // Momento em que foi carregada
    int ultimo_acesso;    // Último tempo de acesso (usado no LRU)
} Pagina;

// Representa um processo com sua tabela de páginas
typedef struct {
    int pid;                  // Identificador do processo
    int tamanho;              // Tamanho total do processo (bytes)
    int num_paginas;          // Quantidade de páginas do processo
    Pagina *tabela_paginas;   // Array de páginas
} Processo;

// Representa um frame na memória física
typedef struct {
    int pid;            // Processo que ocupa o frame
    int pagina;         // Página daquele processo
    int tempo_carga;    // Tempo que foi carregado (FIFO)
    int ultimo_acesso;  // Último acesso (LRU)
    int referenciada;   // Bit de referência (CLOCK)
} FrameInfo;

// Memória física composta por frames
typedef struct {
    int num_frames;     // Quantidade total de frames
    FrameInfo *frames;  // Array de frames
} MemoriaFisica;

// Estrutura geral da simulação
typedef struct {
    int tempo_atual;               // Tempo atual da simulação
    int tamanho_pagina;            // Tamanho da página (ex: 4096 bytes)
    int tamanho_memoria_fisica;    // Tamanho total da memória (ex: 16384 bytes)
    int num_processos;             // Quantidade de processos
    Processo *processos;           // Array de processos
    MemoriaFisica memoria;         // A memória em si

    // Estatísticas
    int total_acessos;
    int page_faults;

    // Algoritmo de substituição
    int algoritmo;                 // FIFO, LRU, etc.
} Simulador;
