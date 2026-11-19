/*
 * hardware.h - coleta multiplataforma de metadados da maquina.
 * Suporte principal: Linux e Windows (GCC/MinGW-w64).
 * Campos nao expostos pelo sistema operacional aparecem como "indisponivel".
 */
#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0601
  #endif
  #include <windows.h>
  #include <winreg.h>
#else
  #include <unistd.h>
  #include <sys/utsname.h>
#endif

#define HW_TAM 512

static void hw_limpar_fim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
}

#if defined(__linux__)
static int hw_ler_primeira_linha(const char *caminho, char *dest, size_t cap) {
    FILE *f = fopen(caminho, "r");
    if (!f) return 0;
    int ok = fgets(dest, (int)cap, f) != NULL;
    fclose(f);
    if (ok) hw_limpar_fim(dest);
    return ok && dest[0] != '\0';
}
#endif

static void hw_formatar_bytes(uint64_t bytes, char *dest, size_t cap) {
    if (bytes >= (uint64_t)1024 * 1024 * 1024)
        snprintf(dest, cap, "%.2f GiB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024u * 1024u)
        snprintf(dest, cap, "%.2f MiB", (double)bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024u)
        snprintf(dest, cap, "%.2f KiB", (double)bytes / 1024.0);
    else
        snprintf(dest, cap, "%llu B", (unsigned long long)bytes);
}

static void hw_timestamp(char *dest, size_t cap) {
    time_t agora = time(NULL);
    struct tm tmv;
#if defined(_WIN32)
    if (localtime_s(&tmv, &agora) != 0) {
        snprintf(dest, cap, "indisponivel");
        return;
    }
#else
    if (!localtime_r(&agora, &tmv)) {
        snprintf(dest, cap, "indisponivel");
        return;
    }
#endif
    if (!strftime(dest, cap, "%Y-%m-%d %H:%M:%S", &tmv))
        snprintf(dest, cap, "indisponivel");
}

#if defined(__linux__)
static int hw_ler_cpu(char *dest, size_t cap) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) return 0;
    char linha[1024];
    int ok = 0;
    while (fgets(linha, sizeof linha, f)) {
        const char *chaves[] = {"model name", "Hardware", "Model"};
        for (size_t i = 0; i < sizeof chaves / sizeof chaves[0]; i++) {
            size_t n = strlen(chaves[i]);
            if (strncmp(linha, chaves[i], n) == 0) {
                char *p = strchr(linha, ':');
                if (p) {
                    p++;
                    while (*p == ' ' || *p == '\t') p++;
                    snprintf(dest, cap, "%s", p);
                    hw_limpar_fim(dest);
                    ok = dest[0] != '\0';
                    break;
                }
            }
        }
        if (ok) break;
    }
    fclose(f);
    return ok;
}

static int hw_ler_os(char *dest, size_t cap) {
    FILE *f = fopen("/etc/os-release", "r");
    if (f) {
        char linha[1024];
        while (fgets(linha, sizeof linha, f)) {
            if (strncmp(linha, "PRETTY_NAME=", 12) == 0) {
                char *p = linha + 12;
                hw_limpar_fim(p);
                size_t n = strlen(p);
                if (n >= 2 && ((p[0] == '"' && p[n - 1] == '"') || (p[0] == '\'' && p[n - 1] == '\''))) {
                    p[n - 1] = '\0';
                    p++;
                }
                snprintf(dest, cap, "%.*s", (int)(cap - 1), p);
                fclose(f);
                return dest[0] != '\0';
            }
        }
        fclose(f);
    }
    struct utsname u;
    if (uname(&u) == 0) {
        snprintf(dest, cap, "%s %s", u.sysname, u.release);
        return 1;
    }
    return 0;
}

static uint64_t hw_parse_cache_size(const char *s) {
    char *fim = NULL;
    double v = strtod(s, &fim);
    if (fim == s || v <= 0.0) return 0;
    while (*fim == ' ' || *fim == '\t') fim++;
    if (*fim == 'K' || *fim == 'k') v *= 1024.0;
    else if (*fim == 'M' || *fim == 'm') v *= 1024.0 * 1024.0;
    else if (*fim == 'G' || *fim == 'g') v *= 1024.0 * 1024.0 * 1024.0;
    return (uint64_t)v;
}

static void hw_linux_caches(uint64_t cache[4][3]) {
    memset(cache, 0, sizeof(uint64_t) * 12);
    for (int i = 0; i < 16; i++) {
        char p[256], valor[128];
        int nivel = 0;
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu0/cache/index%d/level", i);
        if (!hw_ler_primeira_linha(p, valor, sizeof valor)) continue;
        nivel = atoi(valor);
        if (nivel < 1 || nivel > 3) continue;
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu0/cache/index%d/type", i);
        if (!hw_ler_primeira_linha(p, valor, sizeof valor)) continue;
        int tipo = strcmp(valor, "Data") == 0 ? 0 :
                   strcmp(valor, "Instruction") == 0 ? 1 :
                   strcmp(valor, "Unified") == 0 ? 2 : -1;
        if (tipo < 0) continue;
        snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu0/cache/index%d/size", i);
        if (!hw_ler_primeira_linha(p, valor, sizeof valor)) continue;
        uint64_t bytes = hw_parse_cache_size(valor);
        if (bytes > cache[nivel][tipo]) cache[nivel][tipo] = bytes;
    }
}
#endif

#if defined(_WIN32)
static int hw_registro(HKEY raiz, const char *chave, const char *nome, char *dest, size_t cap) {
    DWORD tipo = 0;
    DWORD bytes = (DWORD)cap;
    LONG r = RegGetValueA(raiz, chave, nome, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
                          &tipo, dest, &bytes);
    if (r != ERROR_SUCCESS || !dest[0]) return 0;
    hw_limpar_fim(dest);
    return dest[0] != '\0';
}

static void hw_windows_caches(uint64_t cache[4][3]) {
    memset(cache, 0, sizeof(uint64_t) * 12);
    DWORD bytes = 0;
    GetLogicalProcessorInformation(NULL, &bytes);
    if (!bytes) return;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION info =
        (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(bytes);
    if (!info) return;
    if (GetLogicalProcessorInformation(info, &bytes)) {
        DWORD qtd = bytes / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        for (DWORD i = 0; i < qtd; i++) {
            if (info[i].Relationship != RelationCache) continue;
            int nivel = (int)info[i].Cache.Level;
            if (nivel < 1 || nivel > 3) continue;
            int tipo = info[i].Cache.Type == CacheData ? 0 :
                       info[i].Cache.Type == CacheInstruction ? 1 :
                       info[i].Cache.Type == CacheUnified ? 2 : -1;
            if (tipo < 0) continue;
            uint64_t tam = (uint64_t)info[i].Cache.Size;
            if (tam > cache[nivel][tipo]) cache[nivel][tipo] = tam;
        }
    }
    free(info);
}
#endif

static void hardware_imprimir(FILE *out) {
    char data[64], hostname[HW_TAM] = "indisponivel";
    char fabricante[HW_TAM] = "indisponivel";
    char modelo[HW_TAM] = "indisponivel";
    char cpu[HW_TAM] = "indisponivel";
    char sistema[HW_TAM] = "indisponivel";
    char kernel[HW_TAM] = "indisponivel";
    char arquitetura[128] = "indisponivel";
    char memoria[64] = "indisponivel";
    uint64_t caches[4][3] = {{0}};
    int nucleos_logicos = 0;
    hw_timestamp(data, sizeof data);

#if defined(_WIN32)
    DWORD hn = (DWORD)sizeof hostname;
    if (!GetComputerNameA(hostname, &hn)) snprintf(hostname, sizeof hostname, "indisponivel");
    hw_registro(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemManufacturer", fabricante, sizeof fabricante);
    hw_registro(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemProductName", modelo, sizeof modelo);
    if (strcmp(modelo, "indisponivel") == 0)
        hw_registro(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", "SystemVersion", modelo, sizeof modelo);
    hw_registro(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString", cpu, sizeof cpu);
    char produto[256] = "", versao[128] = "", build[64] = "";
    hw_registro(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "ProductName", produto, sizeof produto);
    hw_registro(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "DisplayVersion", versao, sizeof versao);
    hw_registro(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", "CurrentBuild", build, sizeof build);
    if (produto[0]) snprintf(sistema, sizeof sistema, "%s%s%s%s%s", produto,
                              versao[0] ? " | versao " : "", versao,
                              build[0] ? " | build " : "", build);
    MEMORYSTATUSEX ms;
    memset(&ms, 0, sizeof ms);
    ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms)) hw_formatar_bytes((uint64_t)ms.ullTotalPhys, memoria, sizeof memoria);
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    nucleos_logicos = (int)si.dwNumberOfProcessors;
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: snprintf(arquitetura, sizeof arquitetura, "x64 (AMD64)"); break;
        case PROCESSOR_ARCHITECTURE_INTEL: snprintf(arquitetura, sizeof arquitetura, "x86 (Intel)"); break;
#ifdef PROCESSOR_ARCHITECTURE_ARM64
        case PROCESSOR_ARCHITECTURE_ARM64: snprintf(arquitetura, sizeof arquitetura, "ARM64"); break;
#endif
        case PROCESSOR_ARCHITECTURE_ARM: snprintf(arquitetura, sizeof arquitetura, "ARM"); break;
        default: snprintf(arquitetura, sizeof arquitetura, "arquitetura Windows codigo %u", (unsigned)si.wProcessorArchitecture); break;
    }
    hw_windows_caches(caches);
#else
    struct utsname u;
    if (gethostname(hostname, sizeof hostname - 1) != 0) snprintf(hostname, sizeof hostname, "indisponivel");
    hostname[sizeof hostname - 1] = '\0';
    if (uname(&u) == 0) {
        snprintf(arquitetura, sizeof arquitetura, "%s", u.machine);
        snprintf(kernel, sizeof kernel, "%s %s", u.sysname, u.release);
    }
    hw_ler_os(sistema, sizeof sistema);
    hw_ler_cpu(cpu, sizeof cpu);
    hw_ler_primeira_linha("/sys/class/dmi/id/sys_vendor", fabricante, sizeof fabricante);
    hw_ler_primeira_linha("/sys/class/dmi/id/product_name", modelo, sizeof modelo);
    if (strcmp(modelo, "indisponivel") == 0)
        hw_ler_primeira_linha("/sys/class/dmi/id/product_version", modelo, sizeof modelo);
    FILE *fmem = fopen("/proc/meminfo", "r");
    if (fmem) {
        char chave[64];
        unsigned long long kb = 0;
        while (fscanf(fmem, "%63s %llu", chave, &kb) == 2) {
            if (strcmp(chave, "MemTotal:") == 0) {
                hw_formatar_bytes((uint64_t)kb * 1024u, memoria, sizeof memoria);
                break;
            }
            int ch;
            while ((ch = fgetc(fmem)) != '\n' && ch != EOF) {}
        }
        fclose(fmem);
    }
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    nucleos_logicos = ncpu > 0 ? (int)ncpu : 0;
    hw_linux_caches(caches);
#endif

    fprintf(out, "============================================================\n");
    fprintf(out, "INFORMACOES DA MAQUINA - EXPERIMENTO QUICKSORT BITWISE\n");
    fprintf(out, "============================================================\n");
    fprintf(out, "Coleta em: %s (horario local)\n", data);
    fprintf(out, "Host: %s\n", hostname);
    fprintf(out, "Fabricante do equipamento: %s\n", fabricante);
    fprintf(out, "Modelo do equipamento: %s\n", modelo);
    fprintf(out, "Processador: %s\n", cpu);
    fprintf(out, "Processadores logicos: %s", nucleos_logicos > 0 ? "" : "indisponivel\n");
    if (nucleos_logicos > 0) fprintf(out, "%d\n", nucleos_logicos);
    fprintf(out, "Memoria RAM fisica total: %s\n", memoria);
    fprintf(out, "Sistema operacional: %s\n", sistema);
    fprintf(out, "Kernel/versao base: %s\n", kernel);
    fprintf(out, "Arquitetura: %s\n", arquitetura);

    for (int nivel = 1; nivel <= 3; nivel++) {
        char d[64] = "indisponivel", ins[64] = "indisponivel", un[64] = "indisponivel";
        if (caches[nivel][0]) hw_formatar_bytes(caches[nivel][0], d, sizeof d);
        if (caches[nivel][1]) hw_formatar_bytes(caches[nivel][1], ins, sizeof ins);
        if (caches[nivel][2]) hw_formatar_bytes(caches[nivel][2], un, sizeof un);
        if (nivel == 1)
            fprintf(out, "Cache L1 (dados/instrucoes/unificada, por instancia): %s / %s / %s\n", d, ins, un);
        else
            fprintf(out, "Cache L%d (dados/instrucoes/unificada, por instancia): %s / %s / %s\n", nivel, d, ins, un);
    }
#ifdef __VERSION__
    fprintf(out, "Compilador C: %s\n", __VERSION__);
#else
    fprintf(out, "Compilador C: identificacao indisponivel\n");
#endif
    fprintf(out, "Observacao: campos indisponiveis nao foram fornecidos pela API/sistema operacional.\n");
    fprintf(out, "============================================================\n");
}

#endif /* HARDWARE_H */
