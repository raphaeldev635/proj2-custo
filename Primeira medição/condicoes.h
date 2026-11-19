/*
 * condicoes.h - utilitario para medir sempre nas mesmas condicoes.
 * Header unico (basta #include). Funciona em Linux (completo), Windows (MinGW/MSVC,
 * parcial) e macOS/outros (parcial). O que nao for possivel e avisado na tela.
 *
 * IMPORTANTE (Linux): defina _GNU_SOURCE ANTES de qualquer #include no seu .c
 *   #define _GNU_SOURCE
 *
 * Uso:
 *   cond_preparar();      // uma vez, no inicio do main
 *   cond_limpar();        // antes de CADA medicao (fora do trecho cronometrado)
 *   uint64_t t0 = cond_ns(); ... trabalho ... uint64_t t1 = cond_ns();
 *
 * O que faz (e o que NAO consegue fazer):
 *   cond_preparar():
 *     - fixa o processo em UM nucleo (evita migracao entre nucleos e caches)
 *     - tenta subir a prioridade (no Linux, so com sudo; senao avisa)
 *     - "aquece" a CPU ~300 ms para tirar do estado de economia de energia
 *   cond_limpar():
 *     - expulsa os caches da CPU escrevendo/lendo um buffer maior que o cache L3
 *     - devolve ao SO a memoria livre do alocador (malloc_trim, so Linux)
 *   NAO consegue (precisa de root/BIOS, faca a mao se quiser):
 *     - limpar o page cache do SO:  sudo sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches'
 *     - travar a frequencia:        sudo cpupower frequency-set -g performance
 *     - desligar turbo / fechar outros programas / ligar na tomada
 */
#ifndef CONDICOES_H
#define CONDICOES_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#if defined(_WIN32)
  #include <windows.h>
#elif defined(__linux__)
  #include <sched.h>
  #include <unistd.h>
  #include <malloc.h>
  #include <sys/resource.h>
#else
  #include <unistd.h>
  #include <sys/resource.h>
#endif

static volatile uint64_t cond_sink; /* impede o compilador de eliminar as leituras */

/* ---------- relogio monotonico em nanossegundos ---------- */
static uint64_t cond_ns(void) {
#if defined(_WIN32)
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (uint64_t)((double)c.QuadPart * 1e9 / (double)f.QuadPart);
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
#endif
}

/* ---------- fixar em um nucleo ---------- */
static void cond_fixar_cpu(void) {
#if defined(__linux__)
    int cpu = sched_getcpu();
    const char *env = getenv("CRIVO_CPU");   /* ex.: CRIVO_CPU=2 ./crivo 100000000 */
    if (env) cpu = atoi(env);
    if (cpu < 0) cpu = 0;
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    if (sched_setaffinity(0, sizeof(s), &s) == 0)
        printf("[cond] processo fixado no nucleo %d\n", cpu);
    else
        printf("[cond] AVISO: nao foi possivel fixar o nucleo\n");
#elif defined(_WIN32)
    DWORD cpu = GetCurrentProcessorNumber();
    if (cpu < 64 && SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << cpu))
        printf("[cond] thread fixada no nucleo %lu\n", (unsigned long)cpu);
    else
        printf("[cond] AVISO: nao foi possivel fixar o nucleo\n");
#else
    printf("[cond] AVISO: fixar nucleo nao suportado neste SO\n");
#endif
}

/* ---------- prioridade ---------- */
static void cond_prioridade(void) {
#if defined(_WIN32)
    if (SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
        printf("[cond] prioridade alta aplicada\n");
    else
        printf("[cond] AVISO: nao foi possivel subir a prioridade\n");
#else
    if (setpriority(PRIO_PROCESS, 0, -10) == 0)
        printf("[cond] prioridade alta aplicada (nice -10)\n");
    else
        printf("[cond] AVISO: sem permissao para subir prioridade (rode com sudo se quiser)\n");
#endif
}

/* ---------- aquecer a CPU ---------- */
static void cond_aquecer_cpu(int ms) {
    uint64_t fim = cond_ns() + (uint64_t)ms * 1000000ULL;
    uint64_t x = 88172645463325252ULL;
    while (cond_ns() < fim)
        for (int i = 0; i < 100000; i++) {   /* trabalho de CPU sem usar memoria */
            x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        }
    cond_sink = x;
    printf("[cond] CPU aquecida por %d ms\n", ms);
}

/* ---------- uma vez, no inicio ---------- */
static void cond_preparar(void) {
    cond_fixar_cpu();
    cond_prioridade();
    cond_aquecer_cpu(300);
}

/* ---------- antes de cada medicao ---------- */
static void cond_limpar(void) {
    size_t tam = 64u * 1024u * 1024u;            /* padrao: 64 MB */
#if defined(__linux__) && defined(_SC_LEVEL3_CACHE_SIZE)
    long l3 = sysconf(_SC_LEVEL3_CACHE_SIZE);
    if (l3 > 0 && (size_t)l3 * 2 > tam) tam = (size_t)l3 * 2;
#endif
    volatile uint8_t *buf = (volatile uint8_t *)malloc(tam);
    if (buf) {
        uint64_t soma = 0;
        for (size_t i = 0; i < tam; i += 64) buf[i] = (uint8_t)i;   /* escreve */
        for (size_t i = 0; i < tam; i += 64) soma += buf[i];        /* le */
        cond_sink = soma;
        free((void *)buf);
    }
#if defined(__linux__)
    malloc_trim(0);
#endif
}

#endif /* CONDICOES_H */
