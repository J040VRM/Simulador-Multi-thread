#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

// Função auxiliar para buscar o índice do processo no vetor
int buscar_indice_processo(Simulador *sim, int pid) {
    for (int i = 0; i < sim->num_processos; i++) {
        if (sim->processos[i].pid == pid) {
            return i;
        }
    }
    return -1; // não encontrado
}

// Tradução de endereço virtual para físico com tratamento de page fault
int traduzir_endereco(Simulador *sim, int pid, int endereco_virtual) {
    int indice_proc = buscar_indice_processo(sim, pid);
    if (indice_proc == -1) {
        fprintf(stderr, "Erro: processo %d não encontrado!\n", pid);
        return -1;
    }

    Processo *proc = &sim->processos[indice_proc];
    int pagina = endereco_virtual / sim->tamanho_pagina;
    int deslocamento = endereco_virtual % sim->tamanho_pagina;

    if (pagina >= proc->num_paginas) {
        fprintf(stderr, "Erro: página %d fora do limite do processo %d.\n", pagina, pid);
        return -1;
    }

    Pagina *pag = &proc->tabela_paginas[pagina];

    if (!pag->presente) {
        printf("Tempo t=%d: [PAGE FAULT] Página %d do Processo %d não está na memória!\n",
               sim->tempo_atual, pagina, pid);
        sim->page_faults++;

        int frame = carregar_pagina(sim, pid, pagina); // <- aqui deve retornar o frame
        pag->frame = frame; // segurança caso carregar_pagina não atualize
        pag->presente = 1;
        pag->tempo_carga = sim->tempo_atual;
    }

    // Atualiza estatísticas e tempo de acesso
    pag->ultimo_acesso = sim->tempo_atual;
    sim->total_acessos++;

    int endereco_fisico = pag->frame * sim->tamanho_pagina + deslocamento;
    printf("Tempo t=%d: Endereço Virtual (P%d): %d -> Página: %d -> Frame: %d -> Endereço Físico: %d\n",
           sim->tempo_atual, pid, endereco_virtual, pagina, pag->frame, endereco_fisico);

    return endereco_fisico;
}
//Funcoes auxiliares

int encontrar_frame_livre(MemoriaFisica *mem) {
    for (int i = 0; i < mem->num_frames; i++) {
        if (mem->frames[i].pid == -1) {
            return i;
        }
    }
    return -1; // Nenhum frame livre
}

int substituir_pagina_fifo(Simulador *sim) {
    int mais_antigo = 0;
    for (int i = 1; i < sim->memoria.num_frames; i++) {
        if (sim->memoria.frames[i].tempo_carga < sim->memoria.frames[mais_antigo].tempo_carga) {
            mais_antigo = i;
        }
    }
    return mais_antigo;
}

int substituir_pagina_random(Simulador *sim) {
    return rand() % sim->memoria.num_frames;
}

void liberar_frame(MemoriaFisica *mem, int frame) {
    mem->frames[frame].pid = -1;
    mem->frames[frame].pagina = -1;
    mem->frames[frame].tempo_carga = -1;
    mem->frames[frame].ultimo_acesso = -1;
    mem->frames[frame].referenciada = 0;
}


int carregar_pagina(Simulador *sim, int pid, int num_pagina) {
    int frame = encontrar_frame_livre(&sim->memoria);
    if (frame == -1) {
        // Substituição: decide algoritmo
        frame = (sim->algoritmo == FIFO)
                    ? substituir_pagina_fifo(sim)
                    : substituir_pagina_random(sim);

        // Libera página antiga
        FrameInfo antigo = sim->memoria.frames[frame];
        int indice_antigo = buscar_indice_processo(sim, antigo.pid);
        if (indice_antigo != -1) {
            sim->processos[indice_antigo].tabela_paginas[antigo.pagina].presente = 0;
        }
    }

    // Localiza o processo atual
    int indice_proc = buscar_indice_processo(sim, pid);
    if (indice_proc == -1) {
        fprintf(stderr, "Erro ao carregar: processo %d não encontrado.\n", pid);
        return -1;
    }

    // Atualiza frame com nova página
    sim->memoria.frames[frame].pid = pid;
    sim->memoria.frames[frame].pagina = num_pagina;
    sim->memoria.frames[frame].tempo_carga = sim->tempo_atual;
    sim->memoria.frames[frame].ultimo_acesso = sim->tempo_atual;
    sim->memoria.frames[frame].referenciada = 1;

    // Atualiza tabela de páginas
    Pagina *pag = &sim->processos[indice_proc].tabela_paginas[num_pagina];
    pag->presente = 1;
    pag->frame = frame;
    pag->tempo_carga = sim->tempo_atual;
    pag->ultimo_acesso = sim->tempo_atual;
    pag->referenciada = 1;

    return frame;
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
