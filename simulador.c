#include <stdio.h>
#include <stdlib.h>

#define NUM_PAGES 16
#define NUM_FRAMES 8
#define TRACE_LEN 25

typedef struct {
    int presente;
    int moldura;
    int R; 
    int M; 
    int tempo; 
} Pagina;

int tipoAcesso[TRACE_LEN]  = {0,0,1,0,1,1,0,0,0,0,1,0,0,1,0,0,0,1,0,0,0,1,0,0,0};
int paginaAcessada[TRACE_LEN] = {0,1,2,6,7,1,7,6,2,3,0,4,0,6,1,8,12,8,2,15,6,0,3,5,0};


int paginasIniciais[NUM_FRAMES] = {3,1,0,5,4,9,2,11};

int buscaPagina(Pagina *memoria, int pagina) {
    if (memoria[pagina].presente)
        return memoria[pagina].moldura;
    return -1;
}

void inicializa(Pagina *memoria, int *framesLivres) {
    for (int i = 0; i < NUM_PAGES; i++) {
        memoria[i].presente = 0;
        memoria[i].R = 0;
        memoria[i].M = 0;
        memoria[i].tempo = 0;
    }
    for (int i = 0; i < NUM_FRAMES; i++) {
        memoria[paginasIniciais[i]].presente = 1;
        memoria[paginasIniciais[i]].moldura = i;
        memoria[paginasIniciais[i]].R = 1;
        memoria[paginasIniciais[i]].M = 0;
        memoria[paginasIniciais[i]].tempo = 0;
        framesLivres[i] = paginasIniciais[i];
    }
}

void simulaFIFO() {
    Pagina memoria[NUM_PAGES];
    int frames[NUM_FRAMES];
    int fila[NUM_FRAMES];
    int posFila = 0;

    inicializa(memoria, frames);
    for (int i = 0; i < NUM_FRAMES; i++) fila[i] = paginasIniciais[i];

    int hits = 0, misses = 0;

    for (int t = 0; t < TRACE_LEN; t++) {
        int tipo = tipoAcesso[t];
        int pag = paginaAcessada[t];

        int frame = buscaPagina(memoria, pag);
        if (frame != -1) {
            hits++;
            memoria[pag].R = 1;
            if (tipo) memoria[pag].M = 1;
        } else {
            misses++;
            // substitui a mais antiga
            int vitima = fila[posFila];
            posFila = (posFila + 1) % NUM_FRAMES;

            memoria[vitima].presente = 0;
            int molduraLivre = memoria[vitima].moldura;

            fila[(posFila + NUM_FRAMES - 1) % NUM_FRAMES] = pag;

            memoria[pag].presente = 1;
            memoria[pag].moldura = molduraLivre;
            memoria[pag].R = 1;
            memoria[pag].M = tipo;
        }
    }

    printf("FIFO: hits=%d, misses=%d\n", hits, misses);
}

void simulaSegundaChance() {
    Pagina memoria[NUM_PAGES];
    int frames[NUM_FRAMES];
    int fila[NUM_FRAMES];
    int posFila = 0;

    inicializa(memoria, frames);
    for (int i = 0; i < NUM_FRAMES; i++) fila[i] = paginasIniciais[i];

    int hits = 0, misses = 0;

    for (int t = 0; t < TRACE_LEN; t++) {
        int tipo = tipoAcesso[t];
        int pag = paginaAcessada[t];

        int frame = buscaPagina(memoria, pag);
        if (frame != -1) {
            hits++;
            memoria[pag].R = 1;
            if (tipo) memoria[pag].M = 1;
        } else {
            misses++;
            // procura vítima com R=0
            while (1) {
                int candidato = fila[posFila];
                if (memoria[candidato].R == 1) {
                    memoria[candidato].R = 0;
                    posFila = (posFila + 1) % NUM_FRAMES;
                } else {
                    int molduraLivre = memoria[candidato].moldura;
                    memoria[candidato].presente = 0;
                    fila[posFila] = pag;
                    memoria[pag].presente = 1;
                    memoria[pag].moldura = molduraLivre;
                    memoria[pag].R = 1;
                    memoria[pag].M = tipo;
                    posFila = (posFila + 1) % NUM_FRAMES;
                    break;
                }
            }
        }
    }

    printf("Segunda Chance: hits=%d, misses=%d\n", hits, misses);
}

void simulaRelogio() {
    Pagina memoria[NUM_PAGES];
    int ponteiro = 0;
    int frames[NUM_FRAMES];

    inicializa(memoria, frames);

    int hits = 0, misses = 0;

    for (int t = 0; t < TRACE_LEN; t++) {
        int tipo = tipoAcesso[t];
        int pag = paginaAcessada[t];
        int frame = buscaPagina(memoria, pag);

        if (frame != -1) {
            hits++;
            memoria[pag].R = 1;
            if (tipo) memoria[pag].M = 1;
        } else {
            misses++;
            while (1) {
                int candidato = frames[ponteiro];
                if (memoria[candidato].R == 1) {
                    memoria[candidato].R = 0;
                    ponteiro = (ponteiro + 1) % NUM_FRAMES;
                } else {
                    int molduraLivre = memoria[candidato].moldura;
                    memoria[candidato].presente = 0;
                    frames[ponteiro] = pag;
                    memoria[pag].presente = 1;
                    memoria[pag].moldura = molduraLivre;
                    memoria[pag].R = 1;
                    memoria[pag].M = tipo;
                    ponteiro = (ponteiro + 1) % NUM_FRAMES;
                    break;
                }
            }
        }
    }

    printf("Relogio: hits=%d, misses=%d\n", hits, misses);
}

void simulaMRU() {
    Pagina memoria[NUM_PAGES];
    int frames[NUM_FRAMES];
    inicializa(memoria, frames);

    int hits = 0, misses = 0;
    int tempo = 0;

    for (int t = 0; t < TRACE_LEN; t++) {
        int tipo = tipoAcesso[t];
        int pag = paginaAcessada[t];
        tempo++;

        int frame = buscaPagina(memoria, pag);
        if (frame != -1) {
            hits++;
            memoria[pag].R = 1;
            if (tipo) memoria[pag].M = 1;
            memoria[pag].tempo = tempo;
        } else {
            misses++;
            // procura a mais recente (maior tempo)
            int vitima = -1, maior = -1;
            for (int i = 0; i < NUM_PAGES; i++) {
                if (memoria[i].presente && memoria[i].tempo > maior) {
                    vitima = i;
                    maior = memoria[i].tempo;
                }
            }
            int molduraLivre = memoria[vitima].moldura;
            memoria[vitima].presente = 0;
            memoria[pag].presente = 1;
            memoria[pag].moldura = molduraLivre;
            memoria[pag].R = 1;
            memoria[pag].M = tipo;
            memoria[pag].tempo = tempo;
        }
    }

    printf("MRU: hits=%d, misses=%d\n", hits, misses);
}

int main() {
    printf("Simulador de Substituicao de Paginas (C)\n");
    simulaFIFO();
    simulaSegundaChance();
    simulaRelogio();
    simulaMRU();
    return 0;
}