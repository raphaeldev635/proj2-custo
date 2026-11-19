/*
 * condicoes.h (versao 2) - utilitario para medir sempre nas mesmas condicoes.
 * Header unico (basta #include). Linux completo; Windows e macOS parcial.
 * Tudo que nao for possivel e avisado na tela; nada aqui interrompe o programa
 * (as unicas saidas fatais sao falta de memoria em cond_alloc).
 *
 * IMPORTANTE (Linux): defina _GNU_SOURCE ANTES de qualquer #include no seu .c
 *   #define _GNU_SOURCE
 *
 * API (compativel com a versao 1):
 *   cond_preparar();   uma vez, no inicio do main
 *   cond_limpar();     antes de CADA medicao (fora do trecho cronometrado)
 *   cond_ns();         relogio monotonico em nanossegundos
 * Novidades da versao 2:
 *   cond_rss_kb()      memoria residente do processo (KiB), -1 se indisponivel
 *   cond_minflt()      page faults menores acumulados, -1 se indisponivel
 *   cond_freq_khz()    frequencia atual do nucleo fixado (kHz), -1 se indisponivel
 *   cond_alloc(n) / cond_liberar(p, n)
 *                      memoria direto do SO (mmap): paginas sempre novas e zeradas,
 *                      devolvidas ao SO no free. Torna espaco/page faults comparaveis.
 *
 * cond_preparar() faz:
 *   - fixa o processo em UM nucleo (evita o nucleo 0 se houver outros: recebe mais
 *     interrupcoes). Para escolher: CRIVO_CPU=3 ./programa
 *   - tenta subir a prioridade (nice -10; no Linux so com sudo)
 *   - desliga huge pages transparentes SO PARA ESTE PROCESSO (prctl), para que o
 *     tamanho de pagina (e portanto os page faults) seja o mesmo em todas as execucoes
 *   - aquece a CPU ~300 ms
 *   - calibra o custo de ler o relogio
 *   - imprime um diagnostico: governador de frequencia, frequencia, turbo, carga do
 *     sistema e tamanhos de cache (anotem isso no relatorio)
 * cond_limpar() faz:
 *   - expulsa os caches: escreve 1x e le 2x um buffer maior que o L3 (a leitura
 *     dupla garante que linhas "sujas" ja foram gravadas antes da medicao, e nao
 *     durante ela). O buffer e reaproveitado entre chamadas.
 *   - devolve memoria livre do alocador ao SO (malloc_trim)
 *
 * NAO consegue (precisa de root/BIOS, faca a mao se quiser):
 *   - limpar o page cache do SO:  sudo sh -c 'sync; echo 3 > /proc/sys/vm/drop_caches'
 *   - travar a frequencia:        sudo cpupower frequency-set -g performance
 *   - desligar turbo, fechar outros programas, ligar na tomada
 * Nao usa prioridade de tempo real (SCHED_FIFO) de proposito: poderia travar a maquina.
 */
#ifndef CONDICOES_H
#define CONDICOES_H
#define CONDICOES_VERSAO 2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#if defined(_WIN32)
  #include <windows.h>
#else
  #include <unistd.h>
  #include <sys/mman.h>
  #include <sys/resource.h>
  #ifndef MAP_ANONYMOUS
    #define MAP_ANONYMOUS MAP_ANON
  #endif
  #if defined(__linux__)
    #include <sched.h>
    #include <malloc.h>
    #include <sys/prctl.h>
  #endif
#endif

static volatile uint64_t cond_sink;   /* impede o compilador de eliminar leituras */
static int cond_cpu_fixado = -1;

/* ---------- relogio monotonico em nanossegundos ---------- */
static inline uint64_t cond_ns(void) {
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

/* ---------- utilitarios (Linux) ---------- */
#if defined(__linux__)
static inline long cond_ler_long(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    long v = -1;
    if (fscanf(f, "%ld", &v) != 1) v = -1;
    fclose(f);
    return v;
}
static inline int cond_ler_texto(const char *path, char *buf, size_t n) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    if (!fgets(buf, (int)n, f)) { fclose(f); return 0; }
    fclose(f);
    size_t l = strlen(buf);
    while (l && (buf[l - 1] == '\n' || buf[l - 1] == '\r')) buf[--l] = 0;
    return 1;
}
#endif

static inline int cond_ncpu(void) {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
#endif
}

/* ---------- fixar em um nucleo ---------- */
static inline void cond_fixar_cpu(void) {
#if defined(__linux__)
    int cpu = sched_getcpu();
    const char *env = getenv("CRIVO_CPU");
    if (env) cpu = atoi(env);
    else if (cpu == 0 && cond_ncpu() > 1) cpu = 1;   /* nucleo 0 recebe mais interrupcoes */
    if (cpu < 0) cpu = 0;
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(cpu, &s);
    if (sched_setaffinity(0, sizeof(s), &s) == 0) {
        cond_cpu_fixado = cpu;
        printf("[cond] processo fixado no nucleo %d\n", cpu);
    } else {
        printf("[cond] AVISO: nao foi possivel fixar o nucleo %d\n", cpu);
    }
#elif defined(_WIN32)
    DWORD cpu = GetCurrentProcessorNumber();
    if (cpu < 64 && SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << cpu)) {
        cond_cpu_fixado = (int)cpu;
        printf("[cond] thread fixada no nucleo %lu\n", (unsigned long)cpu);
    } else {
        printf("[cond] AVISO: nao foi possivel fixar o nucleo\n");
    }
#else
    printf("[cond] AVISO: fixar nucleo nao suportado neste SO\n");
#endif
}

/* ---------- prioridade ---------- */
static inline void cond_prioridade(void) {
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

/* ---------- huge pages transparentes: desligar so neste processo ---------- */
static inline void cond_desligar_thp(void) {
#if defined(__linux__) && defined(PR_SET_THP_DISABLE)
    if (prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0) == 0)
        printf("[cond] huge pages transparentes desligadas neste processo (paginas de 4 KiB)\n");
    else
        printf("[cond] AVISO: nao foi possivel desligar huge pages neste processo\n");
#else
    printf("[cond] AVISO: huge pages nao controlaveis neste SO\n");
#endif
}

/* ---------- aquecer a CPU ---------- */
static inline void cond_aquecer_cpu(int ms) {
    uint64_t fim = cond_ns() + (uint64_t)ms * 1000000ULL;
    uint64_t x = 88172645463325252ULL;
    while (cond_ns() < fim)
        for (int i = 0; i < 100000; i++) {   /* trabalho de CPU sem usar memoria */
            x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        }
    cond_sink = x;
    printf("[cond] CPU aquecida por %d ms\n", ms);
}

/* ---------- custo de ler o relogio ---------- */
static inline void cond_calibrar_relogio(void) {
    uint64_t min = ~0ULL;
    for (int i = 0; i < 10000; i++) {
        uint64_t a = cond_ns(), b = cond_ns();
        uint64_t d = b - a;
        if (d < min) min = d;
    }
    printf("[cond] menor intervalo entre duas leituras do relogio: %llu ns "
           "(tempos proximos disso sao ruido)\n", (unsigned long long)min);
}

/* ---------- leituras de estado ---------- */
static inline long cond_freq_khz(void) {
#if defined(__linux__)
    if (cond_cpu_fixado < 0) return -1;
    char p[160];
    snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", cond_cpu_fixado);
    return cond_ler_long(p);
#else
    return -1;
#endif
}

static inline long cond_rss_kb(void) {
#if defined(__linux__)
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) return -1;
    long total = 0, res = 0;
    if (fscanf(f, "%ld %ld", &total, &res) != 2) { fclose(f); return -1; }
    fclose(f);
    return res * (sysconf(_SC_PAGESIZE) / 1024);
#else
    return -1;
#endif
}

static inline long cond_minflt(void) {
#if !defined(_WIN32)
    struct rusage r;
    if (getrusage(RUSAGE_SELF, &r) != 0) return -1;
    return (long)r.ru_minflt;
#else
    return -1;
#endif
}

/* ---------- diagnostico do estado da maquina ---------- */
static inline void cond_diagnostico(void) {
#if defined(__linux__)
    char txt[128];
    if (cond_cpu_fixado >= 0) {
        char p[160];
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", cond_cpu_fixado);
        if (cond_ler_texto(p, txt, sizeof txt)) printf("[cond] governador de frequencia: %s\n", txt);
        else printf("[cond] governador de frequencia: indisponivel\n");
        long f = cond_freq_khz();
        if (f > 0) printf("[cond] frequencia atual do nucleo: %ld MHz\n", f / 1000);
    }
    long nt = cond_ler_long("/sys/devices/system/cpu/intel_pstate/no_turbo");
    if (nt == 0) printf("[cond] turbo: ligado\n");
    else if (nt == 1) printf("[cond] turbo: desligado\n");
    else {
        long bo = cond_ler_long("/sys/devices/system/cpu/cpufreq/boost");
        if (bo == 1) printf("[cond] turbo (boost): ligado\n");
        else if (bo == 0) printf("[cond] turbo (boost): desligado\n");
    }
    FILE *f = fopen("/proc/loadavg", "r");
    if (f) {
        double l1 = 0;
        if (fscanf(f, "%lf", &l1) == 1) {
            printf("[cond] carga do sistema (1 min): %.2f em %d nucleos%s\n", l1, cond_ncpu(),
                   l1 > 0.5 ? "  <-- AVISO: ha outros programas rodando, feche-os" : "");
        }
        fclose(f);
    }
  #if defined(_SC_LEVEL1_DCACHE_SIZE) && defined(_SC_LEVEL2_CACHE_SIZE) && defined(_SC_LEVEL3_CACHE_SIZE)
    long l1c = sysconf(_SC_LEVEL1_DCACHE_SIZE), l2c = sysconf(_SC_LEVEL2_CACHE_SIZE), l3c = sysconf(_SC_LEVEL3_CACHE_SIZE);
    printf("[cond] caches: L1d=%ld KiB | L2=%ld KiB | L3=%ld KiB\n", l1c / 1024, l2c / 1024, l3c / 1024);
  #endif
#else
    printf("[cond] diagnostico detalhado so no Linux\n");
#endif
}

/* ---------- uma vez, no inicio ---------- */
static inline void cond_preparar(void) {
    printf("[cond] condicoes.h versao %d\n", CONDICOES_VERSAO);
    cond_fixar_cpu();
    cond_prioridade();
    cond_desligar_thp();
    cond_aquecer_cpu(300);
    cond_calibrar_relogio();
    cond_diagnostico();
}

/* ---------- antes de cada medicao ---------- */
static inline void cond_limpar(void) {
    static unsigned char *buf = NULL;
    static size_t tam = 0;
    if (!buf) {
        tam = 64u * 1024u * 1024u;                 /* padrao: 64 MB */
#if defined(__linux__) && defined(_SC_LEVEL3_CACHE_SIZE)
        long l3 = sysconf(_SC_LEVEL3_CACHE_SIZE);
        if (l3 > 0 && (size_t)l3 * 2 > tam) tam = (size_t)l3 * 2;
#endif
        buf = (unsigned char *)malloc(tam);
        if (!buf) tam = 0;
    }
    if (buf) {
        volatile unsigned char *v = buf;
        uint64_t soma = 0;
        for (size_t i = 0; i < tam; i += 64) v[i] = (unsigned char)(i >> 6);        /* escreve */
        for (int p = 0; p < 2; p++)
            for (size_t i = 0; i < tam; i += 64) soma += v[i];                      /* le 2x */
        cond_sink = soma;
    }
#if defined(__linux__)
    malloc_trim(0);
#endif
}

/* ---------- memoria direto do SO (paginas novas e zeradas) ---------- */
static inline void *cond_alloc(size_t bytes) {
    void *p;
#if defined(_WIN32)
    p = calloc(1, bytes);
#else
    p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) p = NULL;
#endif
    if (!p) { fprintf(stderr, "sem memoria (%zu bytes)\n", bytes); exit(1); }
    return p;
}

static inline void cond_liberar(void *p, size_t bytes) {
#if defined(_WIN32)
    (void)bytes;
    free(p);
#else
    munmap(p, bytes);
#endif
}

#endif /* CONDICOES_H */
