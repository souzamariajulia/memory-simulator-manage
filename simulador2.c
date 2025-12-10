#include <stdio.h>
#include <string.h>

#define NUM_PAGES   16
#define NUM_FRAMES  8
#define TRACE_LEN   25


#define DEBUG 0  


static const int PAGINAS_INICIAIS[NUM_FRAMES] = {3, 1, 0, 5, 4, 9, 2, 11};

static const int TIPO_ACESSO[TRACE_LEN] = {
//0 leitura R
//1 escruta
/*  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 */
    0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0
};
static const int PAG_ACESSO[TRACE_LEN] = {
    0, 1, 2, 6, 7, 1, 7, 6, 2, 3, 0, 4, 0, 6, 1, 8, 12, 8, 2, 15, 6, 0, 3, 5, 0
};


typedef struct {
    int presente;   /* 0/1 */
    int moldura;    /* índice do frame, se presente */
    int R;          /* bit referenciada */
    int M;          /* bit modificada */
    int last_use;   /* timestamp do último uso (para MRU) */
} PageEntry;

typedef struct {
    PageEntry page[NUM_PAGES];
    int frame_to_page[NUM_FRAMES]; 

    int hits, misses;
    int tempo;

    int ordem_circular[NUM_FRAMES]; 
    int idx_fifo;                   
    int clock_hand;                 

} Simulador;

typedef int (*EscolheVitimaFn)(Simulador *s);

typedef struct {
    const char *nome;
    EscolheVitimaFn choose;
    int reset_R_interval; 
} Algoritmo;


static void zeraR(Simulador *s) {
    for (int p = 0; p < NUM_PAGES; ++p)
        if (s->page[p].presente) s->page[p].R = 0;
}

static int buscaFrame(Simulador *s, int pagina) {
    if (s->page[pagina].presente) return s->page[pagina].moldura;
    return -1;
}

static void logAcesso(Simulador *s, int tipo, int pag, int hit, int vitima) {
#if DEBUG
    printf("[%02d] %s P%-2d  ->  %s", s->tempo, (tipo ? "M" : "R"), pag, (hit ? "HIT " : "MISS"));
    if (!hit) {
        if (vitima >= 0) printf("  (vitima=P%d)", vitima);
    }
    printf("\n");
#else
    (void)s; (void)tipo; (void)pag; (void)hit; (void)vitima;
#endif
}


static void carregarPaginaEmFrame(Simulador *s, int pag, int f, int tipo) {
    s->frame_to_page[f] = pag;
    s->page[pag].presente = 1;
    s->page[pag].moldura = f;
    s->page[pag].R = 1;
    if (tipo) s->page[pag].M = 1;
    s->page[pag].last_use = s->tempo;
}

static void removerPagina(Simulador *s, int vitima) {
    int f = s->page[vitima].moldura;
    s->page[vitima].presente = 0;
    s->page[vitima].moldura = -1;
    s->page[vitima].R = 0;
    s->page[vitima].M = 0;
    s->frame_to_page[f] = -1;
}

static void inicializaSimulador(Simulador *s) {
    memset(s, 0, sizeof(*s));

    /* Tabela de páginas limpa */
    for (int p = 0; p < NUM_PAGES; ++p) {
        s->page[p].presente = 0;
        s->page[p].moldura  = -1;
        s->page[p].R = 0;
        s->page[p].M = 0;
        s->page[p].last_use = -1;
    }

    /* Frames vazios */
    for (int f = 0; f < NUM_FRAMES; ++f) {
        s->frame_to_page[f] = -1;
        s->ordem_circular[f] = f; /* ordem fixa 0..7, associada aos frames */
    }

    s->hits = s->misses = 0;
    s->tempo = 0;
    s->idx_fifo = 0;
    s->clock_hand = 0;

    int t = 0;
    for (int f = 0; f < NUM_FRAMES; ++f) {
        int pag = PAGINAS_INICIAIS[f];
        s->frame_to_page[f] = pag;
        s->page[pag].presente = 1;
        s->page[pag].moldura = f;
        s->page[pag].R = 1;
        s->page[pag].M = 0;
        s->page[pag].last_use = ++t; 
    }
}

static int choose_FIFO(Simulador *s) {
    for (int k = 0; k < NUM_FRAMES; ++k) {
        int f = s->idx_fifo;
        int pag = s->frame_to_page[f];
        s->idx_fifo = (s->idx_fifo + 1) % NUM_FRAMES;
        if (pag >= 0 && s->page[pag].presente) return pag;
    }
    for (int f = 0; f < NUM_FRAMES; ++f) {
        int pag = s->frame_to_page[f];
        if (pag >= 0 && s->page[pag].presente) return pag;
    }
    return 0; 
}

static int choose_SC(Simulador *s) {
    while (1) {
        int f = s->idx_fifo;
        int pag = s->frame_to_page[f];
        s->idx_fifo = (s->idx_fifo + 1) % NUM_FRAMES;
        if (pag < 0 || !s->page[pag].presente) continue;
        if (s->page[pag].R == 1) {
            s->page[pag].R = 0; 
        } else {
            return pag;         
        }
    }
}

static int choose_Clock(Simulador *s) {
    while (1) {
        int f = s->clock_hand;
        int pag = s->frame_to_page[f];
        if (pag >= 0 && s->page[pag].presente) {
            if (s->page[pag].R == 1) {
                s->page[pag].R = 0;
                s->clock_hand = (s->clock_hand + 1) % NUM_FRAMES;
            } else {
                int v = pag;
                s->clock_hand = (s->clock_hand + 1) % NUM_FRAMES;
                return v;
            }
        } else {
            s->clock_hand = (s->clock_hand + 1) % NUM_FRAMES;
        }
    }
}

static int choose_MRU(Simulador *s) {
    int vitima = -1, maior = -1000000;
    for (int p = 0; p < NUM_PAGES; ++p) {
        if (s->page[p].presente && s->page[p].last_use > maior) {
            maior = s->page[p].last_use;
            vitima = p;
        }
    }
    if (vitima < 0) vitima = 0; 
    return vitima;
}

static int choose_NUR(Simulador *s) {
    int candidatos[4][NUM_PAGES];
    int cnt[4] = {0,0,0,0};
    
    for (int p = 0; p < NUM_PAGES; ++p) {
        if (!s->page[p].presente) continue;
        int r = s->page[p].R ? 1 : 0;
        int m = s->page[p].M ? 1 : 0;
        int cl;
        if (r==0 && m==0) cl = 0;       
        else if (r==0 && m==1) cl = 1;  
        else if (r==1 && m==0) cl = 2;  
        else cl = 3;                    
        candidatos[cl][cnt[cl]++] = p;
    }
    for (int i = 0; i < 4; ++i) if (cnt[i] > 0) return candidatos[i][0];

    
    for (int p = 0; p < NUM_PAGES; ++p) if (s->page[p].presente) return p;
    return 0;
}


static void simular(const Algoritmo *alg, Simulador *s) {
    inicializaSimulador(s);

#if DEBUG
    printf("=== %s ===\n", alg->nome);
#endif

    for (int i = 0; i < TRACE_LEN; ++i) {
        s->tempo++;

        
        if (alg->reset_R_interval > 0 && (s->tempo % alg->reset_R_interval == 0)) {
            zeraR(s);
        }

        int tipo = TIPO_ACESSO[i];  
        int pag  = PAG_ACESSO[i];

        int f = buscaFrame(s, pag);
        if (f != -1) {
            
            s->hits++;
            s->page[pag].R = 1;
            if (tipo) s->page[pag].M = 1;
            s->page[pag].last_use = s->tempo;
            logAcesso(s, tipo, pag, 1, -1);
        } else {
           
            s->misses++;

            
            int vitima = alg->choose(s);
            int frame_livre = s->page[vitima].moldura;

            
            removerPagina(s, vitima);
            carregarPaginaEmFrame(s, pag, frame_livre, tipo);

            logAcesso(s, tipo, pag, 0, vitima);
        }
    }

    
}


int main(void) {

    const Algoritmo algoritmos[] = {
        {"FIFO",            choose_FIFO,   0},
        {"Segunda Chance",  choose_SC,     0},
        {"Relogio",         choose_Clock,  0},
        {"MRU",             choose_MRU,    0},
        {"NUR",             choose_NUR,    8}
    };
    const int N = (int)(sizeof(algoritmos)/sizeof(algoritmos[0]));


    printf("========================================\n");
    printf("      CONFIGURACOES DO SISTEMA\n");
    printf("========================================\n");
    printf("Memoria Fisica: 32 KB\n");
    printf(" - 8 molduras de 4 KB cada\n");
    printf("Memoria Virtual: 64 KB\n");
    printf(" - 16 paginas de 4 KB cada\n");
    printf("Tamanho da pagina/moldura: 4 KB\n");
    printf("Numero de acessos no trace: %d\n", TRACE_LEN);
    printf("\n");


    int hits[N], misses[N];
    double taxa[N];

    for (int i = 0; i < N; ++i) {
        Simulador s;
        simular(&algoritmos[i], &s);
        hits[i] = s.hits;
        misses[i] = s.misses;
        taxa[i] = (double)s.hits / TRACE_LEN * 100.0;
    }

    printf("\n========================================\n");
    printf("        RELATORIO COMPARATIVO\n");
    printf("========================================\n");
    printf("%-15s  %-7s  %-7s  %-10s\n", "Algoritmo", "Hits", "Misses", "Taxa(%)");
    printf("----------------------------------------\n");

    for (int i = 0; i < N; ++i) {
        printf("%-15s  %-7d  %-7d  %-10.2f\n",
               algoritmos[i].nome, hits[i], misses[i], taxa[i]);
    }
    printf("\n");


    int melhor = 0, pior = 0;
    for (int i = 1; i < N; ++i) {
        if (taxa[i] > taxa[melhor]) melhor = i;
        if (taxa[i] < taxa[pior])   pior = i;
    }

    printf("\n========================================\n");
    printf("           DESEMPENHO FINAL\n");
    printf("========================================\n");
    printf("Melhor algoritmo: %s (%.2f%% de acerto)\n",
           algoritmos[melhor].nome, taxa[melhor]);
    printf("Pior algoritmo:   %s (%.2f%% de acerto)\n",
           algoritmos[pior].nome, taxa[pior]);

    return 0;
}
