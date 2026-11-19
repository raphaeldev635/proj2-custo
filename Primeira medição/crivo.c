/*
 * Projeto 2 - Custo - SO 2026.2 - Grupo "Desce dai, Gugu!"
 * Crivo de Eratostenes em 3 niveis de bitwise. UMA execucao por chamada.
 *
 * Compilar (use a MESMA flag em todas as maquinas/condicoes):
 *   gcc -O2 -o crivo crivo.c
 *
 * Uso:
 *   ./crivo <N>   -> roda os 3 niveis, um de cada, sempre na ordem
 *                    nenhum -> moderado -> alto, e mostra no terminal
 *
 * Exemplo:
 *   ./crivo 100000000
 *
 * Niveis:
 *   0 nenhum   : vetor de bytes (1 byte por numero), sem operacoes bitwise
 *   1 moderado : vetor de bits (1 bit por numero), com shifts e mascaras
 *   2 alto     : vetor de bits (todos os numeros), limpeza por palavra de 64 bits
 *                com mascaras para primos pequenos + popcount para contar
 *
 * Compilar junto com condicoes.h (mesma pasta). Antes de cada nivel o programa
 * limpa caches e fixa o nucleo (veja condicoes.h); isso NAO entra no tempo medido.
 *
 * O tempo inclui alocacao, inicializacao, crivo e contagem (igual nos 3 niveis).
 * Repeticoes: rodem o comando quantas vezes quiserem.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "condicoes.h"

/* pi(N) conhecido (valores de referencia matematicos) */
static long esperado(uint64_t n) {
    switch (n) {
        case 10ULL:         return 4;
        case 1000ULL:       return 168;
        case 100000ULL:     return 9592;
        case 1000000ULL:    return 78498;
        case 10000000ULL:   return 664579;
        default:            return -1; /* sem referencia */
    }
}


/* ---------- Nivel 0 - NENHUM: bytes ---------- */
static long crivo_nenhum(uint64_t n) {
    uint8_t *c = malloc(n + 1);
    if (!c) { fprintf(stderr, "sem memoria\n"); exit(1); }
    memset(c, 1, n + 1);
    c[0] = 0;
    if (n >= 1) c[1] = 0;
    for (uint64_t i = 2; i * i <= n; i++)
        if (c[i])
            for (uint64_t j = i * i; j <= n; j += i)
                c[j] = 0;
    long cont = 0;
    for (uint64_t i = 0; i <= n; i++)
        if (c[i]) cont++;
    free(c);
    return cont;
}

/* ---------- Nivel 1 - MODERADO: 1 bit por numero ---------- */
#define TESTA(b, i) ((b)[(i) >> 6] & (1ULL << ((i) & 63)))
#define LIMPA(b, i) ((b)[(i) >> 6] &= ~(1ULL << ((i) & 63)))

static long crivo_moderado(uint64_t n) {
    uint64_t palavras = (n >> 6) + 1;
    uint64_t *b = malloc(palavras * sizeof(uint64_t));
    if (!b) { fprintf(stderr, "sem memoria\n"); exit(1); }
    memset(b, 0xFF, palavras * sizeof(uint64_t));
    LIMPA(b, 0);
    if (n >= 1) LIMPA(b, 1);
    for (uint64_t i = 2; i * i <= n; i++)
        if (TESTA(b, i))
            for (uint64_t j = i * i; j <= n; j += i)
                LIMPA(b, j);
    long cont = 0;
    for (uint64_t i = 0; i <= n; i++)
        if (TESTA(b, i)) cont++;
    free(b);
    return cont;
}

/* ---------- Nivel 2 - ALTO: todos os numeros, operacoes por palavra ---------- */
/* 1 bit por numero (igual ao moderado), mas:
 *  - primos < 64: limpa 64 numeros de uma vez com mascara pre-calculada
 *  - primos >= 64: limpa bit a bit (no maximo 1 multiplo por palavra)
 *  - contagem com popcount por palavra
 */
static long crivo_alto(uint64_t n) {
    uint64_t bits = n + 1;
    uint64_t palavras = (bits + 63) >> 6;
    uint64_t *b = malloc(palavras * sizeof(uint64_t));
    if (!b) { fprintf(stderr, "sem memoria\n"); exit(1); }
    memset(b, 0xFF, palavras * sizeof(uint64_t));
    if (bits & 63)                           /* zera bits alem de n */
        b[palavras - 1] = (1ULL << (bits & 63)) - 1ULL;
    LIMPA(b, 0);
    if (n >= 1) LIMPA(b, 1);

    for (uint64_t p = 2; p * p <= n; p++) {
        if (!TESTA(b, p)) continue;
        uint64_t ini = p * p;
        if (p < 64) {
            /* mascaras: o padrao de multiplos de p se repete a cada p palavras */
            uint64_t masc[64];
            for (uint64_t r = 0; r < p; r++) {
                uint64_t m = 0;
                for (uint64_t k = 0; k < 64; k++)
                    if (((r * 64 + k) % p) == 0) m |= (1ULL << k);
                masc[r] = m;
            }
            uint64_t w0 = ini >> 6;
            /* primeira palavra: bit a bit (nao pode limpar o proprio p) */
            for (uint64_t j = ini; j <= n && (j >> 6) == w0; j += p)
                LIMPA(b, j);
            uint64_t r = (w0 + 1) % p;
            for (uint64_t w = w0 + 1; w < palavras; w++) {
                b[w] &= ~masc[r];
                if (++r == p) r = 0;
            }
        } else {
            for (uint64_t j = ini; j <= n; j += p)
                LIMPA(b, j);
        }
    }
    long cont = 0;
    for (uint64_t w = 0; w < palavras; w++)
        cont += __builtin_popcountll(b[w]);
    free(b);
    return cont;
}

typedef long (*crivo_fn)(uint64_t);

static void rodar(int nivel, uint64_t n) {
    static const char *nomes[] = {"nenhum", "moderado", "alto"};
    static crivo_fn fns[] = {crivo_nenhum, crivo_moderado, crivo_alto};
    long esp = esperado(n);

    cond_limpar();                 /* fora do trecho cronometrado */
    uint64_t t0 = cond_ns();
    long achados = fns[nivel](n);
    uint64_t t1 = cond_ns();
    uint64_t dt = t1 - t0;

    printf("nivel=%s | N=%llu | tempo=%llu ns (%.4f ms) | primos=%ld",
           nomes[nivel], (unsigned long long)n,
           (unsigned long long)dt, dt / 1e6, achados);
    if (esp < 0) {
        printf(" | corretude=sem referencia para este N\n");
    } else {
        long dif = achados > esp ? achados - esp : esp - achados;
        double pct = 100.0 * (1.0 - (double)dif / (double)esp);
        if (pct < 0) pct = 0;
        printf(" | esperado=%ld | corretude=%.4f%%\n", esp, pct);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "uso: %s <N>\n", argv[0]);
        return 1;
    }
    uint64_t n = strtoull(argv[1], NULL, 10);
    cond_preparar();
    for (int nivel = 0; nivel < 3; nivel++) rodar(nivel, n);
    return 0;
}
