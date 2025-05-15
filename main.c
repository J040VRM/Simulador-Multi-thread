#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Constantes
const int TAMANHO_PAGINA = 4096;      // 4 KB
const int TAMANHO_MEMORIA = 16384;    // 16 KB
const int MAX_PROCESSOS = 100;

// Algoritmos
#define FIFO 0
#define LRU 1
#define CLOCK 2
#define RANDOM 3
#define CUSTOM 4

// Structs
typedef struct {
    int presente;
    int frame;
    int modificada;
    int referenciada;
    int tempo_carga;
    int ultimo_acesso;
} Pagina;

typedef struct {
    int pid;
    int tamanho;
    int num_paginas;
    Pagina *tabela_paginas;
} Processo;

typedef struct {
    int pid;
    int pagina;
    int tempo_carga;
    int ultimo_acesso;
    int referenciada;
} FrameInfo;

typedef struct {
    int num_frames;
    FrameInfo *frames;
} MemoriaFisica;

typedef struct {
    int tempo_atual;
    int tamanho_pagina;
    int tamanho_memoria_fisica;
    int num_processos;
    Processo *processos;
    MemoriaFisica memoria;
    int total_acessos;
    int page_faults;
    int algoritmo;
} Simulador;

// ================== PROTÓTIPOS ==================
Simulador* inicializar_simulador(int tam_pagina, int tam_memoria);
void adicionar_processo(Simulador *sim, int pid, int tamanho);
int buscar_indice_processo(Simulador *sim, int pid);
int traduzir_endereco(Simulador *sim, int pid, int endereco_virtual);
int encontrar_frame_livre(MemoriaFisica *mem);
int substituir_pagina_fifo(Simulador *sim);
int substituir_pagina_random(Simulador *sim);
void liberar_frame(MemoriaFisica *mem, int frame);
int carregar_pagina(Simulador *sim, int pid, int num_pagina);
void executar_simulacao(Simulador *sim, int acessos[][2], int n);
void exibir_memoria_fisica(Simulador *sim);
void exibir_estatisticas(Simulador *sim);

// ================== FUNÇÕES ==================

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

    for (int i = 0; i < num_frames; i++) {
        sim->memoria.frames[i].pid = -1;
    }

    sim->num_processos = 0;
    sim->processos = NULL;
    sim->total_acessos = 0;
    sim->page_faults = 0;
    sim->algoritmo = FIFO;

    return sim;
}

void adicionar_processo(Simulador *sim, int pid, int tamanho) {
    sim->processos = realloc(sim->processos, (sim->num_processos + 1) * sizeof(Processo));
    Processo *p = &sim->processos[sim->num_processos];
    p->pid = pid;
    p->tamanho = tamanho;
    p->num_paginas = (tamanho + sim->tamanho_pagina - 1) / sim->tamanho_pagina;
    p->tabela_paginas = malloc(p->num_paginas * sizeof(Pagina));

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

int buscar_indice_processo(Simulador *sim, int pid) {
    for (int i = 0; i < sim->num_processos; i++) {
        if (sim->processos[i].pid == pid)
            return i;
    }
    return -1;
}

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
        printf("Tempo t=%d: [PAGE FAULT] Página %d do Processo %d não está na memória!\n", sim->tempo_atual, pagina, pid);
        sim->page_faults++;

        int frame = carregar_pagina(sim, pid, pagina);
        pag->frame = frame;
        pag->presente = 1;
        pag->tempo_carga = sim->tempo_atual;
    }

    pag->ultimo_acesso = sim->tempo_atual;
    sim->total_acessos++;

    int endereco_fisico = pag->frame * sim->tamanho_pagina + deslocamento;
    printf("Tempo t=%d: Endereço Virtual (P%d): %d -> Página: %d -> Frame: %d -> Endereço Físico: %d\n",
           sim->tempo_atual, pid, endereco_virtual, pagina, pag->frame, endereco_fisico);

    return endereco_fisico;
}

int encontrar_frame_livre(MemoriaFisica *mem) {
    for (int i = 0; i < mem->num_frames; i++) {
        if (mem->frames[i].pid == -1)
            return i;
    }
    return -1;
}

int substituir_pagina_fifo(Simulador *sim) {
    int mais_antigo = 0;
    for (int i = 1; i < sim->memoria.num_frames; i++) {
        if (sim->memoria.frames[i].tempo_carga < sim->memoria.frames[mais_antigo].tempo_carga)
            mais_antigo = i;
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
        frame = (sim->algoritmo == FIFO)
                    ? substituir_pagina_fifo(sim)
                    : substituir_pagina_random(sim);

        FrameInfo antigo = sim->memoria.frames[frame];
        int indice_antigo = buscar_indice_processo(sim, antigo.pid);
        if (indice_antigo != -1)
            sim->processos[indice_antigo].tabela_paginas[antigo.pagina].presente = 0;
    }

    int indice_proc = buscar_indice_processo(sim, pid);
    if (indice_proc == -1) {
        fprintf(stderr, "Erro ao carregar: processo %d não encontrado.\n", pid);
        return -1;
    }

    sim->memoria.frames[frame].pid = pid;
    sim->memoria.frames[frame].pagina = num_pagina;
    sim->memoria.frames[frame].tempo_carga = sim->tempo_atual;
    sim->memoria.frames[frame].ultimo_acesso = sim->tempo_atual;
    sim->memoria.frames[frame].referenciada = 1;

    Pagina *pag = &sim->processos[indice_proc].tabela_paginas[num_pagina];
    pag->presente = 1;
    pag->frame = frame;
    pag->tempo_carga = sim->tempo_atual;
    pag->ultimo_acesso = sim->tempo_atual;
    pag->referenciada = 1;

    return frame;
}

void executar_simulacao(Simulador *sim, int acessos[][2], int n) {
    printf("\n======= INÍCIO DA SIMULAÇÃO =======\n");
    for (int i = 0; i < n; i++) {
        int pid = acessos[i][0];
        int endereco_virtual = acessos[i][1];
        traduzir_endereco(sim, pid, endereco_virtual);
        sim->tempo_atual++;
    }

    printf("\n======= FIM DA SIMULAÇÃO =======\n\n");
    exibir_estatisticas(sim);
}

void exibir_memoria_fisica(Simulador *sim) {
    printf("\nEstado da Memória Física:\n");
    for (int i = 0; i < sim->memoria.num_frames; i++) {
        FrameInfo f = sim->memoria.frames[i];
        if (f.pid == -1)
            printf("Frame %d: LIVRE\n", i);
        else
            printf("Frame %d: P%d - Página %d\n", i, f.pid, f.pagina);
    }
    printf("\n");
}

void exibir_estatisticas(Simulador *sim) {
    printf("======== ESTATÍSTICAS ========\n");
    printf("Total de acessos: %d\n", sim->total_acessos);
    printf("Page faults: %d\n", sim->page_faults);
    printf("Taxa de page faults: %.2f%%\n", 100.0 * sim->page_faults / sim->total_acessos);
}

int main() {
    srand(time(NULL));
    Simulador *sim = inicializar_simulador(TAMANHO_PAGINA, TAMANHO_MEMORIA);
    sim->algoritmo = FIFO;

    adicionar_processo(sim, 0, 16384);
    adicionar_processo(sim, 1, 16384);

    int acessos[][2] = {
        {0, 1000}, {0, 8000}, {0, 12000},
        {1, 500}, {1, 10000}, {0, 2000},
        {1, 12000}, {0, 4096}, {1, 4096},
        {0, 8192}, {1, 8192}
    };

    int n = sizeof(acessos)/sizeof(acessos[0]);
    executar_simulacao(sim, acessos, n);

    return 0;
}
