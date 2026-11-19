/*
 * Projeto 2 - Custo - SO 2026.2 - Grupo "Desce dai, Gugu!"
 * Experimento 3: Quicksort com tres niveis de bitwise.
 *
 * Algoritmo (o MESMO nos tres niveis): quicksort recursivo com particao de Lomuto, pivo no
 * elemento do meio, recursao sobre a parte menor (profundidade no maximo ~log2 N).
 * Estrutura, fluxo de controle e quantidade de comparacoes sao identicos; muda SO a
 * implementacao de tres operacoes (truques do "Bit Twiddling Hacks"):
 *
 *   operacao        nivel 0: nenhum        nivel 1: moderado        nivel 2: alto
 *   comparar a < b  a < b                  a < b                    XOR do bit de sinal + comparacao unsigned
 *   trocar x, y     variavel temporaria    XOR (t = x^y; x^=t; y^=t)  XOR
 *   ponto medio     lo + (hi-lo)/2         lo + ((hi-lo)>>1)        lo + ((hi-lo)>>1)
 *
 * Os tres niveis usam a mesma particao, o mesmo pivo, os mesmos limites e o mesmo fluxo
 * do Quicksort. O nivel alto compara corretamente toda a faixa int32: inverter o bit de
 * sinal mapeia a ordem signed para a ordem unsigned, sem subtracao nem risco de overflow.
 *
 * Metricas impressas por execucao:
 *   tempo     : so a ordenacao (ns e ms). Copia do vetor, verificacao e limpeza ficam fora.
 *   espaco    : bytes do vetor, pico de pilha (aprox.), profundidade maxima de recursao,
 *               aumento de memoria residente (RSS) e page faults durante a ordenacao.
 *   corretude : % de elementos na posicao certa (contra o vetor ordenado de referencia),
 *               se o vetor ficou ordenado, se ainda e uma permutacao dos mesmos elementos,
 *               numero de INVERSOES (pares fora de ordem; mede a distancia ate a ordem certa) e
 *               % de pares adjacentes em ordem. Cuidado: "posicao certa" pode cair a 0% por um
 *               deslocamento unico, enquanto "adjacentes" fica perto de 100%; olhem as tres.
 *
 * Compilar (com condicoes.h na mesma pasta):  gcc -O2 -o quick quicksort.c
 * Uso:
 *   ./quick <N> [reps=5] [extremas=100] [ordem=r] [entrada=v] [arquivo.csv]
 *     N           tamanho do vetor
 *     reps        repeticoes por nivel (padrao 5)
 *     extremas    % das chaves sorteadas em todo o int32 (-2^31..2^31-1); as demais ficam em
 *                 +-32768 (0..100; padrao 100). Controla a distribuicao dos valores de entrada.
 *     ordem       r = a ordem dos niveis gira a cada repeticao (dilui o vies de ordem; padrao)
 *                 f = ordem fixa (nenhum, moderado, alto) em todas as repeticoes
 *     entrada     v = a entrada MUDA a cada repeticao (mesma para os 3 niveis dentro dela); a
 *                     todos os niveis recebem os mesmos dados dentro de cada repeticao (padrao)
 *                 f = a mesma entrada em todas as repeticoes (mede so o ruido do sistema)
 *   Exemplos:  ./quick 100000      ./quick 100000 5 0      ./quick 300000 5 10 f
 *
 * A entrada usa sementes fixas (reprodutivel): rodar de novo com os mesmos parametros gera os
 * mesmos vetores. Dentro de uma repeticao, os 3 niveis ordenam exatamente o mesmo vetor.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "hardware.h"
#include "condicoes.h"

/* ---------- gerador pseudoaleatorio (semente fixa) ---------- */
static uint64_t estado = 0x9E3779B97F4A7C15ULL;
static uint32_t rnd32(void) {
    estado ^= estado >> 12; estado ^= estado << 25; estado ^= estado >> 27;
    return (uint32_t)((estado * 0x2545F4914F6CDD1DULL) >> 32);
}

/* ---------- as tres operacoes, em suas versoes ---------- */
#define MENOR_NORMAL(a, b)  ((a) < (b))
#define MENOR_BITS(a, b)    ((((uint32_t)(a) ^ 0x80000000u)) < (((uint32_t)(b) ^ 0x80000000u)))
#define TROCA_TEMP(x, y)    do { int32_t t_ = *(x); *(x) = *(y); *(y) = t_; } while (0)
#define TROCA_XOR(x, y)     do { int32_t t_ = *(x) ^ *(y); *(x) ^= t_; *(y) ^= t_; } while (0)
#define MEIO_DIV(lo, hi)    ((lo) + ((hi) - (lo)) / 2)
#define MEIO_SHIFT(lo, hi)  ((lo) + (((hi) - (lo)) >> 1))

/* ---------- instrumentacao de espaco (pilha) ---------- */
static int prof_max = 0;
static uintptr_t pilha_base = 0, pilha_pico = 0;

/* ---------- gerador das tres versoes do quicksort ---------- */
#define GERA_QS(NOME, MENOR, TROCA, MEIO)                                             \
static void NOME(int32_t *a, long lo, long hi, int d) {                               \
    while (lo < hi) {                                                                 \
        if (d > prof_max) {                                                           \
            prof_max = d;                                                             \
            uintptr_t f = (uintptr_t)__builtin_frame_address(0);                      \
            pilha_pico = pilha_base > f ? pilha_base - f : f - pilha_base;            \
        }                                                                             \
        long m = MEIO(lo, hi);                                                        \
        TROCA(&a[m], &a[hi]);                                                         \
        int32_t p = a[hi];                                                            \
        long i = lo;                                                                  \
        for (long j = lo; j < hi; j++)                                                \
            if (MENOR(a[j], p)) { TROCA(&a[i], &a[j]); i++; }                         \
        TROCA(&a[i], &a[hi]);                                                         \
        if (i - lo < hi - i) { NOME(a, lo, i - 1, d + 1); lo = i + 1; }               \
        else                 { NOME(a, i + 1, hi, d + 1); hi = i - 1; }               \
    }                                                                                 \
}

GERA_QS(qs_nenhum,   MENOR_NORMAL, TROCA_TEMP, MEIO_DIV)
GERA_QS(qs_moderado, MENOR_NORMAL, TROCA_XOR,  MEIO_SHIFT)
GERA_QS(qs_alto,     MENOR_BITS,   TROCA_XOR,  MEIO_SHIFT)

#define GERA_WRAP(NOME, QS)                                                           \
static void NOME(int32_t *a, long n) {                                                \
    prof_max = 0; pilha_pico = 0;                                                     \
    pilha_base = (uintptr_t)__builtin_frame_address(0);                               \
    if (n > 1) QS(a, 0, n - 1, 1);                                                    \
}
GERA_WRAP(ordena_nenhum,   qs_nenhum)
GERA_WRAP(ordena_moderado, qs_moderado)
GERA_WRAP(ordena_alto,     qs_alto)

typedef void (*ordena_fn)(int32_t *, long);
static const char *NOMES[3] = {"nenhum", "moderado", "alto"};
static const ordena_fn ORDENA[3] = {ordena_nenhum, ordena_moderado, ordena_alto};

static int cmp_int32(const void *x, const void *y) {
    int32_t a = *(const int32_t *)x, b = *(const int32_t *)y;
    return (a > b) - (a < b);
}
static int cmp_u64(const void *x, const void *y) {
    uint64_t a = *(const uint64_t *)x, b = *(const uint64_t *)y;
    return (a > b) - (a < b);
}

/* gera a entrada (semente fixa por 'semente'), o gabarito ordenado e os checksums */
static void gerar(int32_t *in, int32_t *ref, long n, int pct, uint64_t semente, uint64_t *soma, uint64_t *xr) {
    estado = 0x9E3779B97F4A7C15ULL + semente * 0xD1B54A32D192ED03ULL;
    if (!estado) estado = 1;
    uint64_t s = 0, x = 0;
    for (long i = 0; i < n; i++) {
        uint32_t r = rnd32(), q = rnd32();
        if ((int)(q % 100) < pct) in[i] = (int32_t)r;          /* chave extrema: todo o int32 */
        else in[i] = (int32_t)(r << 16) >> 16;                 /* chave comum: +-32768 */
        s += (uint64_t)(uint32_t)in[i];
        x ^= (uint64_t)(uint32_t)in[i];
    }
    memcpy(ref, in, (size_t)n * 4);
    qsort(ref, (size_t)n, 4, cmp_int32);                       /* gabarito exato (fora do tempo) */
    *soma = s;
    *xr = x;
}

/* conta inversoes (pares fora de ordem) com merge sort exato; fora do tempo */
static uint64_t conta_inversoes(const int32_t *src, int32_t *a, int32_t *b, long n) {
    memcpy(a, src, (size_t)n * 4);
    uint64_t inv = 0;
    for (long larg = 1; larg < n; larg *= 2) {
        for (long lo = 0; lo < n; lo += 2 * larg) {
            long mid = lo + larg < n ? lo + larg : n;
            long hi = lo + 2 * larg < n ? lo + 2 * larg : n;
            long i = lo, j = mid, k = lo;
            while (i < mid && j < hi) {
                if (a[i] <= a[j]) b[k++] = a[i++];
                else { b[k++] = a[j++]; inv += (uint64_t)(mid - i); }
            }
            while (i < mid) b[k++] = a[i++];
            while (j < hi) b[k++] = a[j++];
        }
        int32_t *t = a; a = b; b = t;
    }
    return inv;
}

int main(int argc, char **argv) {
    /* Linha a linha mesmo quando a saida e redirecionada pelo orquestrador Python. */
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    if (argc == 2 && strcmp(argv[1], "--hardware") == 0) {
        hardware_imprimir(stdout);
        return 0;
    }

    if (argc < 2) {
        fprintf(stderr, "uso: %s <N> [reps=5] [extremas=100] [ordem=r|f] [entrada=v|f] [arquivo.csv]\n", argv[0]);
        return 1;
    }
    long n = atol(argv[1]);
    int reps = argc > 2 ? atoi(argv[2]) : 5;
    int pct = argc > 3 ? atoi(argv[3]) : 100;
    char ordem = argc > 4 ? argv[4][0] : 'r';
    char entrada = argc > 5 ? argv[5][0] : 'v';
    if (n < 1) { fprintf(stderr, "N deve ser >= 1\n"); return 1; }
    if (reps < 1 || reps > 1000) { fprintf(stderr, "reps deve estar entre 1 e 1000\n"); return 1; }
    if (pct < 0 || pct > 100) { fprintf(stderr, "extremas deve estar entre 0 e 100\n"); return 1; }
    if (ordem != 'r' && ordem != 'f') { fprintf(stderr, "ordem deve ser r ou f\n"); return 1; }
    if (entrada != 'v' && entrada != 'f') { fprintf(stderr, "entrada deve ser v ou f\n"); return 1; }

    FILE *csv = NULL;
    if (argc > 6) {
        csv = fopen(argv[6], "a+");
        if (!csv) { perror("nao foi possivel abrir CSV"); return 1; }

        /* O CSV pode comecar com metadados em linhas comentadas (# ...). */
        int tem_cabecalho = 0;
        const char *prefixo_csv = "N,tentativa,nivel,";
        char linha_csv[512];
        rewind(csv);
        while (fgets(linha_csv, sizeof linha_csv, csv)) {
            if (strncmp(linha_csv, prefixo_csv, strlen(prefixo_csv)) == 0) {
                tem_cabecalho = 1;
                break;
            }
        }
        if (fseek(csv, 0, SEEK_END) != 0) {
            perror("falha ao posicionar CSV");
            fclose(csv);
            return 1;
        }
        if (!tem_cabecalho) {
            fprintf(csv, "N,tentativa,nivel,tempo_ns,tempo_ms,frequencia_mhz,vetor_bytes,pilha_pico_bytes,profundidade_max,rss_delta_kib,page_faults,posicao_certa,percentual_posicao_certa,ordenado,permutacao,inversoes,percentual_inversoes,pares_adjacentes_percentual,chaves_extremas_pct,ordem,entrada\n");
            fflush(csv);
        }
    }

    /* No uso direto, cada processo mostra seu cabecalho uma vez.
       O orquestrador central imprime o cabecalho geral e suprime as copias filhas. */
    if (!getenv("CRIVO_SEM_CABECALHO"))
        hardware_imprimir(stdout);

    cond_preparar();

    int32_t *in = (int32_t *)malloc((size_t)n * 4);
    int32_t *ref = (int32_t *)malloc((size_t)n * 4);
    int32_t *bufA = (int32_t *)malloc((size_t)n * 4);
    int32_t *bufB = (int32_t *)malloc((size_t)n * 4);
    int32_t *w = (int32_t *)cond_alloc((size_t)n * 4);        /* vetor de trabalho (mmap) */
    uint64_t (*tempos)[3] = (uint64_t (*)[3])malloc((size_t)reps * sizeof(uint64_t[3]));
    double (*corr)[3] = (double (*)[3])malloc((size_t)reps * sizeof(double[3]));
    if (!in || !ref || !bufA || !bufB || !tempos || !corr) { fprintf(stderr, "sem memoria\n"); return 1; }

    printf("[exp] quicksort | N=%ld | reps=%d | chaves_extremas=%d%% | ordem=%s | entrada=%s\n", n, reps, pct,
           ordem == 'r' ? "rotativa" : "fixa", entrada == 'v' ? "varia a cada repeticao" : "a mesma em todas");
    uint64_t soma_in = 0, xor_in = 0;
    for (int r = 0; r < reps; r++) {
        if (r == 0 || entrada == 'v') gerar(in, ref, n, pct, (uint64_t)(entrada == 'v' ? r : 0), &soma_in, &xor_in);
        int ini = (ordem == 'r') ? (r % 3) : 0;
        for (int j = 0; j < 3; j++) {
            int nv = (ini + j) % 3;
            memcpy(w, in, (size_t)n * 4);                      /* copia limpa (fora do tempo) */
            cond_limpar();                                     /* fora do tempo */

            long rss0 = cond_rss_kb(), flt0 = cond_minflt();
            uint64_t t0 = cond_ns();
            ORDENA[nv](w, n);
            uint64_t t1 = cond_ns();
            long rss1 = cond_rss_kb(), flt1 = cond_minflt();
            long freq = cond_freq_khz();
            int prof = prof_max;
            uintptr_t pilha = pilha_pico;

            /* verificacao (fora do tempo) */
            uint64_t certos = 0, adj = 0, soma_w = 0, xor_w = 0;
            for (long i = 0; i < n; i++) {
                if (w[i] == ref[i]) certos++;
                if (i + 1 < n && w[i] <= w[i + 1]) adj++;
                soma_w += (uint64_t)(uint32_t)w[i];
                xor_w ^= (uint64_t)(uint32_t)w[i];
            }
            int permuta = (soma_w == soma_in && xor_w == xor_in);
            double pct_pos = 100.0 * (double)certos / (double)n;
            double pct_adj = n > 1 ? 100.0 * (double)adj / (double)(n - 1) : 100.0;
            int ordenado = (n > 1) ? (adj == (uint64_t)(n - 1)) : 1;
            uint64_t inv = conta_inversoes(w, bufA, bufB, n);
            double pares = (double)n * (double)(n - 1) / 2.0;
            double pct_inv = pares > 0 ? 100.0 * (double)inv / pares : 0.0;

            uint64_t dt = t1 - t0;
            tempos[r][nv] = dt;
            corr[r][nv] = pct_pos;

            printf("rep=%d | nivel=%s | N=%ld | ordem_da_rep=%s,%s,%s\n", r + 1, NOMES[nv], n,
                   NOMES[ini], NOMES[(ini + 1) % 3], NOMES[(ini + 2) % 3]);
            printf("  tempo=%llu ns (%.4f ms)", (unsigned long long)dt, dt / 1e6);
            if (freq > 0) printf(" | freq=%ld MHz", freq / 1000);
            printf("\n");
            printf("  espaco: vetor=%ld B | pilha_pico=%llu B | profundidade_max=%d", n * 4,
                   (unsigned long long)pilha, prof);
            if (rss0 >= 0 && rss1 >= 0) printf(" | rss_delta=%ld KiB", rss1 - rss0); else printf(" | rss_delta=n/d");
            if (flt0 >= 0 && flt1 >= 0) printf(" | page_faults=%ld\n", flt1 - flt0); else printf(" | page_faults=n/d\n");
            printf("  corretude: posicao_certa=%llu/%ld (%.4f%%) | ordenado=%s | permutacao=%s\n",
                   (unsigned long long)certos, n, pct_pos, ordenado ? "sim" : "NAO", permuta ? "ok" : "NAO");
            printf("             inversoes=%llu (%.4f%% dos pares) | pares_adjacentes_em_ordem=%.4f%%\n",
                   (unsigned long long)inv, pct_inv, pct_adj);
            if (csv) {
                fprintf(csv, "%ld,%d,%s,%llu,%.6f,%ld,%ld,%llu,%d,%ld,%ld,%llu,%.6f,%s,%s,%llu,%.8f,%.6f,%d,%c,%c\n",
                        n, r + 1, NOMES[nv], (unsigned long long)dt, dt / 1e6,
                        freq > 0 ? freq / 1000 : 0, n * 4, (unsigned long long)pilha, prof,
                        (rss0 >= 0 && rss1 >= 0) ? rss1 - rss0 : -1,
                        (flt0 >= 0 && flt1 >= 0) ? flt1 - flt0 : -1,
                        (unsigned long long)certos, pct_pos, ordenado ? "sim" : "nao",
                        permuta ? "ok" : "nao", (unsigned long long)inv, pct_inv, pct_adj,
                        pct, ordem, entrada);
            }
        }
    }

    /* resumo por nivel */
    printf("\n[resumo] N=%ld | reps=%d | chaves_extremas=%d%%\n", n, reps, pct);
    for (int nv = 0; nv < 3; nv++) {
        uint64_t *v = (uint64_t *)malloc((size_t)reps * sizeof(uint64_t));
        if (!v) break;
        double soma = 0, somac = 0;
        for (int r = 0; r < reps; r++) { v[r] = tempos[r][nv]; soma += (double)v[r]; somac += corr[r][nv]; }
        qsort(v, (size_t)reps, sizeof(uint64_t), cmp_u64);
        double med = (reps % 2) ? (double)v[reps / 2] : ((double)v[reps / 2 - 1] + (double)v[reps / 2]) / 2.0;
        printf("  nivel=%-8s | media=%.1f ns | mediana=%.1f ns | min=%llu ns | max=%llu ns | posicao_certa_media=%.4f%%\n",
               NOMES[nv], soma / reps, med, (unsigned long long)v[0], (unsigned long long)v[reps - 1], somac / reps);
        free(v);
    }

    if (csv) fclose(csv);
    cond_liberar(w, (size_t)n * 4);
    free(in); free(ref); free(bufA); free(bufB); free(tempos); free(corr);
    return 0;
}
