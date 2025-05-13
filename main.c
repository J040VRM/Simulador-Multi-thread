#include <stdio.h>
#include <stdlib.h>

// Constantes do simulador
const int TAMANHO_PAGINA = 4096;      // 4 KB
const int TAMANHO_MEMORIA = 16384;    // 16 KB (4 frames)
const int MAX_PROCESSOS = 100;

// Definições de algoritmos
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
    int tamanho_pagina;            // Tamanho da página
    int tamanho_memoria_fisica;    // Tamanho total da memória
    int num_processos;             // Quantidade de processos
    Processo *processos;           // Array de processos
    MemoriaFisica memoria;         // A memória em si

    // Estatísticas
    int total_acessos;
    int page_faults;

    // Algoritmo de substituição
    int algoritmo;                 // FIFO, LRU, etc.
} Simulador;


// Inicializa o simulador
Simulador* inicializar_simulador(int tam_pagina, int tam_memoria) {
    Simulador *sim = malloc(sizeof(Simulador));
    if (!sim) {
        fprintf(stderr, "Erro ao alocar memória para o simulador.\n");
        exit(1);
    }

    sim->tempo_atual = 0;
    sim->tamanho_pagina = tam_pagina;
    sim->tamanho_memoria_fisica = tam_memoria;

    int num_frames = tam_memoria / tam_pagina;
    sim->memoria.num_frames = num_frames;

    sim->memoria.frames = calloc(num_frames, sizeof(FrameInfo));
    if (!sim->memoria.frames) {
        fprintf(stderr, "Erro ao alocar memória para os frames.\n");
        exit(1);
    }

    for (int i = 0; i < num_frames; i++) {
        sim->memoria.frames[i].pid = -1;
        sim->memoria.frames[i].pagina = -1;
        sim->memoria.frames[i].tempo_carga = -1;
        sim->memoria.frames[i].ultimo_acesso = -1;
        sim->memoria.frames[i].referenciada = 0;
    }

    sim->num_processos = 0;
    sim->processos = NULL;

    sim->total_acessos = 0;
    sim->page_faults = 0;
    sim->algoritmo = FIFO;

    return sim;
}

// Adiciona um novo processo ao simulador
void adicionar_processo(Simulador *sim, int pid, int tamanho) {
    sim->processos = realloc(sim->processos, (sim->num_processos + 1) * sizeof(Processo));
    if (!sim->processos) {
        fprintf(stderr, "Erro ao alocar espaço para processos.\n");
        exit(1);
    }

    Processo *p = &sim->processos[sim->num_processos];
    p->pid = pid;
    p->tamanho = tamanho;

    p->num_paginas = (tamanho + sim->tamanho_pagina - 1) / sim->tamanho_pagina;

    p->tabela_paginas = malloc(p->num_paginas * sizeof(Pagina));
    if (!p->tabela_paginas) {
        fprintf(stderr, "Erro ao alocar tabela de páginas para processo %d.\n", pid);
        exit(1);
    }

    for (int i = 0; i < p->num_paginas; i++) {
        p->tabela_paginas[i].presente = 0;
        p->tabela_paginas[i].frame = -1;
        p->tabela_paginas[i].modificada = 0;
        p->tabela_paginas[i].referenciada = 0;
        p->tabela_paginas[i].tempo_carga = -1;
        p->tabela_paginas[i].ultimo_acesso = -1;
    }

    sim->num_processos++;
}

// Função principal de teste
int main() {
    Simulador *sim = inicializar_simulador(TAMANHO_PAGINA, TAMANHO_MEMORIA);

    printf("Simulador inicializado:\n");
    printf("- Tamanho da página: %d bytes\n", sim->tamanho_pagina);
    printf("- Tamanho da memória física: %d bytes\n", sim->tamanho_memoria_fisica);
    printf("- Frames disponíveis: %d\n\n", sim->memoria.num_frames);

    adicionar_processo(sim, 1, 10000);  // 3 páginas
    adicionar_processo(sim, 2, 16000);  // 4 páginas
    adicionar_processo(sim, 3, 8192);   // 2 páginas

    printf("Processos adicionados: %d\n", sim->num_processos);
    for (int i = 0; i < sim->num_processos; i++) {
        Processo p = sim->processos[i];
        printf("\nProcesso PID=%d | Tamanho=%d | Páginas=%d\n",
               p.pid, p.tamanho, p.num_paginas);
        for (int j = 0; j < p.num_paginas; j++) {
            Pagina pg = p.tabela_paginas[j];
            printf("  Página %d: presente=%d frame=%d modificada=%d ref=%d\n",
                   j, pg.presente, pg.frame, pg.modificada, pg.referenciada);
        }
    }

    // Liberação de memória
    for (int i = 0; i < sim->num_processos; i++) {
        free(sim->processos[i].tabela_paginas);
    }
    free(sim->processos);
    free(sim->memoria.frames);
    free(sim);

    return 0;
}
