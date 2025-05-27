#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Constantes: tamanho da página e da memória física (em bytes), máximo de processos
const int TAMANHO_PAGINA = 4096;      // 4 KB
const int TAMANHO_MEMORIA = 16384;    // 16 KB
const int MAX_PROCESSOS = 100;

// Definição dos algoritmos de substituição de páginas
#define FIFO 0
#define LRU 1
#define CLOCK 2
#define RANDOM 3
#define CUSTOM 4

// Estrutura que representa uma página na tabela de páginas do processo
typedef struct {
    int presente;       // página está na memória física?
    int frame;          // índice do frame na memória física onde a página está
    int modificada;     // página foi modificada (bit de modificação)
    int referenciada;   // página foi referenciada (bit de referência)
    int tempo_carga;    // tempo em que a página foi carregada na memória
    int ultimo_acesso;  // último tempo em que a página foi acessada
} Pagina;

// Estrutura que representa um processo com suas páginas
typedef struct {
    int pid;            // identificador do processo
    int tamanho;        // tamanho do processo em bytes
    int num_paginas;    // número de páginas que o processo usa
    Pagina *tabela_paginas; // tabela de páginas do processo
} Processo;

// Informações do frame na memória física
typedef struct {
    int pid;            // PID do processo que possui a página no frame
    int pagina;         // número da página do processo que está no frame
    int tempo_carga;    // tempo em que a página foi carregada
    int ultimo_acesso;  // último acesso à página
    int referenciada;   // bit de referência da página
} FrameInfo;

// Estrutura da memória física, que possui vários frames
typedef struct {
    int num_frames;     // número total de frames disponíveis
    FrameInfo *frames;  // array com informações de cada frame
} MemoriaFisica;

// Estrutura principal que representa o simulador de paginação
typedef struct {
    int tempo_atual;            // contador de tempo para simulação
    int tamanho_pagina;         // tamanho da página (em bytes)
    int tamanho_memoria_fisica; // tamanho total da memória física (em bytes)
    int num_processos;          // quantidade de processos no simulador
    Processo *processos;        // array de processos
    MemoriaFisica memoria;      // memória física do sistema
    int total_acessos;          // total de acessos realizados
    int page_faults;            // total de falhas de página
    int algoritmo;              // algoritmo de substituição de página usado
} Simulador;

// ================== PROTÓTIPOS DAS FUNÇÕES ==================
// Declaração das funções para organização

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
int substituir_pagina_lru(Simulador *sim);

// ================== IMPLEMENTAÇÃO DAS FUNÇÕES ==================

// Inicializa o simulador configurando os parâmetros básicos e alocando memória
Simulador* inicializar_simulador(int tam_pagina, int tam_memoria) {
    Simulador *sim = malloc(sizeof(Simulador));
    if (!sim) {
        fprintf(stderr, "Erro ao alocar memória para o simulador.\n");
        exit(1);
    }

    sim->tempo_atual = 0;
    sim->tamanho_pagina = tam_pagina;
    sim->tamanho_memoria_fisica = tam_memoria;

    // Calcula quantos frames cabem na memória física
    int num_frames = tam_memoria / tam_pagina;
    sim->memoria.num_frames = num_frames;

    // Inicializa os frames da memória física como vazios
    sim->memoria.frames = calloc(num_frames, sizeof(FrameInfo));
    for (int i = 0; i < num_frames; i++) {
        sim->memoria.frames[i].pid = -1;  // -1 indica frame livre
    }

    sim->num_processos = 0;
    sim->processos = NULL;  // ainda não há processos
    sim->total_acessos = 0;
    sim->page_faults = 0;
    sim->algoritmo = FIFO;  // algoritmo padrão é FIFO

    return sim;
}

// Adiciona um processo no simulador, criando sua tabela de páginas
void adicionar_processo(Simulador *sim, int pid, int tamanho) {
    // Realoca memória para armazenar mais um processo
    sim->processos = realloc(sim->processos, (sim->num_processos + 1) * sizeof(Processo));
    Processo *p = &sim->processos[sim->num_processos];
    p->pid = pid;
    p->tamanho = tamanho;

    // Calcula o número de páginas necessárias para o processo
    p->num_paginas = (tamanho + sim->tamanho_pagina - 1) / sim->tamanho_pagina;

    // Aloca a tabela de páginas para esse processo
    p->tabela_paginas = malloc(p->num_paginas * sizeof(Pagina));

    // Inicializa as páginas como não presentes na memória
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

// Busca o índice do processo no array, dado o PID
int buscar_indice_processo(Simulador *sim, int pid) {
    for (int i = 0; i < sim->num_processos; i++) {
        if (sim->processos[i].pid == pid)
            return i;
    }
    return -1; // não encontrado
}

// Traduz um endereço virtual em endereço físico, realizando page fault se necessário
int traduzir_endereco(Simulador *sim, int pid, int endereco_virtual) {
    int indice_proc = buscar_indice_processo(sim, pid);
    if (indice_proc == -1) {
        fprintf(stderr, "Erro: processo %d não encontrado!\n", pid);
        return -1;
    }

    Processo *proc = &sim->processos[indice_proc];
    int pagina = endereco_virtual / sim->tamanho_pagina;    // calcula página virtual
    int deslocamento = endereco_virtual % sim->tamanho_pagina; // deslocamento na página

    if (pagina >= proc->num_paginas) {
        fprintf(stderr, "Erro: página %d fora do limite do processo %d.\n", pagina, pid);
        return -1;
    }

    Pagina *pag = &proc->tabela_paginas[pagina];

    // Se a página não estiver na memória, ocorre page fault
    if (!pag->presente) {
        printf("Tempo t=%d: [PAGE FAULT] Página %d do Processo %d não está na memória!\n", sim->tempo_atual, pagina, pid);
        sim->page_faults++;

        // Carrega a página na memória física
        int frame = carregar_pagina(sim, pid, pagina);
        pag->frame = frame;
        pag->presente = 1;
        pag->tempo_carga = sim->tempo_atual;
    }

    // Atualiza tempo do último acesso à página
    pag->ultimo_acesso = sim->tempo_atual;
    sim->total_acessos++;

    // Calcula endereço físico final
    int endereco_fisico = pag->frame * sim->tamanho_pagina + deslocamento;

    // Imprime info da tradução de endereço
    printf("Tempo t=%d: Endereço Virtual (P%d): %d -> Página: %d -> Frame: %d -> Endereço Físico: %d\n",
           sim->tempo_atual, pid, endereco_virtual, pagina, pag->frame, endereco_fisico);

    return endereco_fisico;
}

// Procura um frame livre na memória física
int encontrar_frame_livre(MemoriaFisica *mem) {
    for (int i = 0; i < mem->num_frames; i++) {
        if (mem->frames[i].pid == -1)
            return i;  // encontrou frame livre
    }
    return -1; // não encontrou frame livre
}

// Escolhe frame para substituição pelo algoritmo FIFO (mais antigo tempo de carga)
int substituir_pagina_fifo(Simulador *sim) {
    int mais_antigo = 0;
    for (int i = 1; i < sim->memoria.num_frames; i++) {
        if (sim->memoria.frames[i].tempo_carga < sim->memoria.frames[mais_antigo].tempo_carga)
            mais_antigo = i;
    }
    return mais_antigo;
}

// Escolhe frame para substituição de forma aleatória
int substituir_pagina_random(Simulador *sim) {
    return rand() % sim->memoria.num_frames;
}

// Libera o frame, marcando como livre
void liberar_frame(MemoriaFisica *mem, int frame) {
    mem->frames[frame].pid = -1;
    mem->frames[frame].pagina = -1;
    mem->frames[frame].tempo_carga = -1;
    mem->frames[frame].ultimo_acesso = -1;
    mem->frames[frame].referenciada = 0;
}

// Carrega a página do processo na memória física (carregando em frame livre ou substituindo)
int carregar_pagina(Simulador *sim, int pid, int num_pagina) {
    // Procura frame livre
    int frame = encontrar_frame_livre(&sim->memoria);

    if (frame == -1) {
        // Se não há frame livre, escolhe um frame para substituir, segundo algoritmo
        switch (sim->algoritmo) {
            case FIFO:
                frame = substituir_pagina_fifo(sim);
                break;
            case RANDOM:
                frame = substituir_pagina_random(sim);
                break;
            case LRU:
                frame = substituir_pagina_lru(sim);
                break;
            default:
                fprintf(stderr, "Algoritmo desconhecido!\n");
                exit(1);
        }
        // Marca a página antiga como não presente na tabela do processo antigo
        FrameInfo antigo = sim->memoria.frames[frame];
        int indice_antigo = buscar_indice_processo(sim, antigo.pid);
        if (indice_antigo != -1)
            sim->processos[indice_antigo].tabela_paginas[antigo.pagina].presente = 0;
    }

    // Atualiza frame com o novo conteúdo da página do processo atual
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

    // Atualiza a tabela de páginas do processo para refletir que a página está na memória
    Pagina *pag = &sim->processos[indice_proc].tabela_paginas[num_pagina];
    pag->presente = 1;
    pag->frame = frame;
    pag->tempo_carga = sim->tempo_atual;
    pag->ultimo_acesso = sim->tempo_atual;
    pag->referenciada = 1;

    return frame;
}

// Executa a simulação lendo e processando uma sequência de acessos a endereços virtuais
void executar_simulacao(Simulador *sim, int acessos[][2], int n) {
    printf("\n======= INÍCIO DA SIMULAÇÃO =======\n");
    for (int i = 0; i < n; i++) {
        int pid = acessos[i][0];
        int endereco_virtual = acessos[i][1];
        traduzir_endereco(sim, pid, endereco_virtual);
        sim->tempo_atual++;  // incrementa o tempo a cada acesso
    }

    printf("\n======= FIM DA SIMULAÇÃO =======\n\n");
    exibir_estatisticas(sim);
}

// Exibe o estado atual da memória física
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

// Implementação do algoritmo LRU para substituição (substitui a página menos recentemente usada)
int substituir_pagina_lru(Simulador *sim) {
    int menos_recente = -1;
    for (int i = 0; i < sim->memoria.num_frames; i++) {
        if (sim->memoria.frames[i].pid == -1) continue; // ignora frames livres

        if (menos_recente == -1 || sim->memoria.frames[i].ultimo_acesso < sim->memoria.frames[menos_recente].ultimo_acesso) {
            menos_recente = i;
        }
    }
    return menos_recente;
}

// Exibe estatísticas da simulação (total de acessos e page faults)
void exibir_estatisticas(Simulador *sim) {
    printf("======== ESTATÍSTICAS ========\n");
    printf("Total de acessos: %d\n", sim->total_acessos);
    printf("Page faults: %d\n", sim->page_faults);
    printf("Taxa de page faults: %.2f%%\n", 100.0 * sim->page_faults / sim->total_acessos);
}

// Função principal que implementa um menu para interação com o usuário
int main() {
    srand(time(NULL));  // inicializa gerador de números aleatórios
    Simulador *sim = inicializar_simulador(TAMANHO_PAGINA, TAMANHO_MEMORIA);

    int opcao;
    do {
        printf("\n===== MENU DO SIMULADOR DE PAGINAÇÃO =====\n");
        printf("1. Selecionar algoritmo (FIFO, RANDOM, LRU)\n");
        printf("2. Adicionar processo\n");
        printf("3. Simular acessos\n");
        printf("4. Exibir memória física\n");
        printf("5. Estatísticas\n");
        printf("0. Sair\n");
        printf("Escolha: ");
        scanf("%d", &opcao);

        if (opcao == 1) {
            printf("Escolha algoritmo: 0=FIFO, 3=RANDOM, 1=LRU: ");
            scanf("%d", &sim->algoritmo);
        }

        else if (opcao == 2) {
            int pid, tam;
            printf("PID do processo: ");
            scanf("%d", &pid);
            printf("Tamanho do processo (em bytes): ");
            scanf("%d", &tam);
            adicionar_processo(sim, pid, tam);
        }

        else if (opcao == 3) {
            int qtd;
            printf("Quantos acessos deseja simular? ");
            scanf("%d", &qtd);

            int acessos[qtd][2];
            for (int i = 0; i < qtd; i++) {
                printf("Acesso %d - PID e Endereço Virtual: ", i + 1);
                scanf("%d %d", &acessos[i][0], &acessos[i][1]);
            }

            executar_simulacao(sim, acessos, qtd);
        }

        else if (opcao == 4) {
            exibir_memoria_fisica(sim);
        }

        else if (opcao == 5) {
            exibir_estatisticas(sim);
        }

    } while (opcao != 0);

    printf("Simulador finalizado.\n");
    return 0;
}
