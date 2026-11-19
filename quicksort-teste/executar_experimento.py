#!/usr/bin/env python3
"""Orquestrador multiplataforma do experimento (Linux e Windows com GCC/MinGW)."""

import argparse
import datetime as dt
import os
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TAMANHOS_PADRAO = "100 500 1000 5000 10000 50000 100000 250000 500000 750000 1000000 2500000 5000000 7500000 10000000"


def caminho_absoluto(valor):
    p = Path(valor)
    return p if p.is_absolute() else ROOT / p


def parser_args():
    p = argparse.ArgumentParser(
        description="Executa as passagens TXT, CSV e/ou controle no terminal e registra a evidencia integral."
    )
    p.add_argument("modo", choices=["completo", "txt", "csv", "terminal", "compilar", "limpar"])
    p.add_argument("--tamanhos", default=os.environ.get("TAMANHOS", TAMANHOS_PADRAO))
    p.add_argument("--reps-analise", type=int, default=int(os.environ.get("REPS_ANALISE", "10")))
    p.add_argument("--reps-terminal", type=int, default=int(os.environ.get("REPS_TERMINAL", "1")))
    p.add_argument("--pct", type=int, default=int(os.environ.get("PCT", "100")))
    p.add_argument("--ordem", choices=["r", "f"], default=os.environ.get("ORD", "r"))
    p.add_argument("--entrada", choices=["v", "f"], default=os.environ.get("ENT", "v"))
    p.add_argument("--saida-txt", default="evidencias/teste_01_txt_resultados_detalhados.txt")
    p.add_argument("--saida-csv", default="evidencias/teste_02_csv_dados_estruturados.csv")
    p.add_argument("--evidencia", default="evidencias/registro_integral_execucao.log")
    p.add_argument("--cc", default=os.environ.get("CC", "gcc"))
    p.add_argument("--cflags", default=os.environ.get("CFLAGS", "-O2 -Wall -Wextra -std=c11"))
    return p


class Registro:
    def __init__(self, arquivo_log):
        arquivo_log.parent.mkdir(parents=True, exist_ok=True)
        self.log = arquivo_log.open("w", encoding="utf-8", newline="")

    def emitir(self, texto="", arquivo_txt=None, mostrar=True):
        linha = texto if texto.endswith("\n") else texto + "\n"
        self.log.write(linha)
        self.log.flush()
        if arquivo_txt is not None:
            arquivo_txt.write(linha)
            arquivo_txt.flush()
        if mostrar:
            sys.stdout.write(linha)
            sys.stdout.flush()

    def emitir_bruto(self, texto, arquivo_txt=None, mostrar=True):
        if not texto:
            return
        self.log.write(texto)
        self.log.flush()
        if arquivo_txt is not None:
            arquivo_txt.write(texto)
            arquivo_txt.flush()
        if mostrar:
            sys.stdout.write(texto)
            sys.stdout.flush()

    def fechar(self):
        self.log.close()


def compilar(args):
    nome = "quick.exe" if os.name == "nt" else "quick"
    binario = ROOT / nome
    try:
        compilador = shlex.split(args.cc, posix=True)
        flags = shlex.split(args.cflags, posix=True)
    except ValueError as exc:
        raise RuntimeError("Nao foi possivel interpretar CC/CFLAGS: {}".format(exc))
    if not compilador:
        raise RuntimeError("CC nao pode ser vazio.")
    comando = compilador + flags + ["-o", str(binario), str(ROOT / "quicksort.c")]
    if os.name == "nt":
        # hardware.h consulta fabricante/modelo do equipamento no Registro do Windows.
        comando.append("-ladvapi32")
    resultado = subprocess.run(comando, cwd=str(ROOT), stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, encoding="utf-8",
                               errors="replace")
    if resultado.returncode != 0:
        detalhe = resultado.stdout or "Sem diagnostico do compilador."
        raise RuntimeError("Falha na compilacao (codigo {}).\nComando: {}\n{}".format(
            resultado.returncode, " ".join(comando), detalhe))
    return binario, comando, resultado.stdout


def executar_comando(comando, env, registro, mostrar=True, arquivo_txt=None):
    processo = subprocess.Popen(
        comando, cwd=str(ROOT), env=env, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
        bufsize=1
    )
    assert processo.stdout is not None
    for linha in processo.stdout:
        registro.emitir_bruto(linha, arquivo_txt=arquivo_txt, mostrar=mostrar)
    codigo = processo.wait()
    registro.emitir("[processo] codigo_de_saida={}".format(codigo),
                    arquivo_txt=arquivo_txt, mostrar=mostrar)
    if codigo != 0:
        raise RuntimeError("O processo terminou com codigo {}: {}".format(codigo, comando))


def executar_fase(nome, tamanhos, reps, args, binario, registro, env, arquivo_txt=None,
                  csv_path=None, mostrar_detalhes=True):
    registro.emitir("", arquivo_txt=arquivo_txt)
    registro.emitir("================ FASE: {} ================".format(nome), arquivo_txt=arquivo_txt)
    total = len(tamanhos)
    for indice, n in enumerate(tamanhos, start=1):
        registro.emitir(
            "[progresso] {} | teste {}/{} | N={} | {} repeticao(oes) por nivel (3 niveis)".format(
                nome, indice, total, n, reps
            ),
            arquivo_txt=arquivo_txt,
        )
        comando = [str(binario), str(n), str(reps), str(args.pct), args.ordem, args.entrada]
        if csv_path is not None:
            comando.append(str(csv_path))
        executar_comando(comando, env, registro, mostrar=mostrar_detalhes,
                         arquivo_txt=arquivo_txt)


def principal():
    args = parser_args().parse_args()
    saida_txt = caminho_absoluto(args.saida_txt)
    saida_csv = caminho_absoluto(args.saida_csv)
    evidencia = caminho_absoluto(args.evidencia)
    binario = ROOT / ("quick.exe" if os.name == "nt" else "quick")

    if args.modo == "limpar":
        for p in (binario, saida_txt, saida_csv, evidencia):
            try:
                if p.is_file():
                    p.unlink()
                    print("[limpeza] removido: {}".format(p))
            except OSError as exc:
                print("[limpeza] nao foi possivel remover {}: {}".format(p, exc), file=sys.stderr)
                return 1
        return 0

    tamanhos = []
    try:
        for item in args.tamanhos.replace(",", " ").split():
            n = int(item)
            if n < 1:
                raise ValueError("cada tamanho N deve ser >= 1")
            tamanhos.append(n)
        if not tamanhos:
            raise ValueError("informe ao menos um tamanho em --tamanhos")
        if args.reps_analise < 1 or args.reps_analise > 1000:
            raise ValueError("--reps-analise deve estar entre 1 e 1000")
        if args.reps_terminal < 1 or args.reps_terminal > 1000:
            raise ValueError("--reps-terminal deve estar entre 1 e 1000")
        if args.pct < 0 or args.pct > 100:
            raise ValueError("--pct deve estar entre 0 e 100")
    except ValueError as exc:
        print("[erro] {}".format(exc), file=sys.stderr)
        return 2

    try:
        binario, comando_compilacao, saida_compilador = compilar(args)
    except RuntimeError as exc:
        print("[erro] {}".format(exc), file=sys.stderr)
        return 1

    # Recolhe o cabeçalho uma unica vez; processos filhos nao o repetem.
    info = subprocess.run([str(binario), "--hardware"], cwd=str(ROOT),
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, encoding="utf-8", errors="replace")
    if info.returncode != 0 or not info.stdout.strip():
        print("[erro] nao foi possivel coletar informacoes da maquina: {}".format(info.stdout),
              file=sys.stderr)
        return 1
    cabecalho = info.stdout
    if not cabecalho.endswith("\n"):
        cabecalho += "\n"

    if args.modo == "compilar":
        print("[compilacao] concluida: {}".format(binario.name))
        if saida_compilador.strip():
            print(saida_compilador, end="")
        return 0

    saida_txt.parent.mkdir(parents=True, exist_ok=True)
    saida_csv.parent.mkdir(parents=True, exist_ok=True)
    registro = Registro(evidencia)
    arquivo_txt = None
    inicio = dt.datetime.now().astimezone()
    ambiente = os.environ.copy()
    ambiente["CRIVO_SEM_CABECALHO"] = "1"

    try:
        # A evidencia mestre comeca pelo cabecalho de hardware e registra a saida
        # completa do orquestrador e de todos os processos filhos.
        registro.emitir_bruto(cabecalho, mostrar=True)
        registro.emitir("Inicio da execucao: {}".format(inicio.isoformat(timespec="seconds")))
        registro.emitir("Sistema de execucao: {}".format("Windows" if os.name == "nt" else "Linux/Unix"))
        registro.emitir("Comando de compilacao: {}".format(" ".join(comando_compilacao)))
        registro.emitir("[compilacao] concluida: {}".format(binario.name))
        if saida_compilador.strip():
            registro.emitir_bruto(saida_compilador)
        registro.emitir("Parametros: tamanhos={} | reps_analise={} | reps_terminal={} | PCT={} | ORD={} | ENT={}".format(
            ",".join(str(x) for x in tamanhos), args.reps_analise,
            args.reps_terminal, args.pct, args.ordem, args.entrada
        ))

        if args.modo in ("completo", "txt"):
            arquivo_txt = saida_txt.open("w", encoding="utf-8", newline="")
            arquivo_txt.write(cabecalho)
            arquivo_txt.flush()
            executar_fase("TXT - RESULTADOS DETALHADOS", tamanhos, args.reps_analise,
                          args, binario, registro, ambiente, arquivo_txt=arquivo_txt,
                          mostrar_detalhes=False)
            arquivo_txt.close()
            arquivo_txt = None
            registro.emitir("[arquivo] TXT detalhado salvo em: {}".format(saida_txt))

        if args.modo in ("completo", "csv"):
            # Metadados comentados preservam o CSV como tabela: leitores podem
            # ignorar as linhas de comentario (por exemplo pandas read_csv(comment='#')).
            with saida_csv.open("w", encoding="utf-8") as fcsv:
                fcsv.write("# Metadados do experimento; linhas iniciadas por # nao sao registros de dados.\n")
                for linha in cabecalho.splitlines():
                    fcsv.write("# {}\n".format(linha))
                fcsv.flush()
            executar_fase("CSV - DADOS ESTRUTURADOS", tamanhos, args.reps_analise,
                          args, binario, registro, ambiente, csv_path=saida_csv,
                          mostrar_detalhes=False)
            registro.emitir("[arquivo] CSV estruturado salvo em: {}".format(saida_csv))

        if args.modo in ("completo", "terminal"):
            executar_fase("CONTROLE VISUAL NO TERMINAL", tamanhos, args.reps_terminal,
                          args, binario, registro, ambiente, mostrar_detalhes=True)

        fim = dt.datetime.now().astimezone()
        registro.emitir("")
        registro.emitir("========== EXECUCAO FINALIZADA ==========")
        registro.emitir("Inicio: {}".format(inicio.isoformat(timespec="seconds")))
        registro.emitir("Fim: {}".format(fim.isoformat(timespec="seconds")))
        registro.emitir("Evidencia integral: {}".format(evidencia))
        if args.modo in ("completo", "txt"):
            registro.emitir("TXT detalhado: {}".format(saida_txt))
        if args.modo in ("completo", "csv"):
            registro.emitir("CSV estruturado: {}".format(saida_csv))
        return 0
    except KeyboardInterrupt:
        registro.emitir("[INTERRUPCAO] execucao interrompida pelo usuario.")
        return 130
    except (OSError, RuntimeError) as exc:
        registro.emitir("[ERRO] {}".format(exc))
        return 1
    except Exception as exc:
        registro.emitir("[ERRO INESPERADO] {}: {}".format(type(exc).__name__, exc))
        return 1
    finally:
        if arquivo_txt is not None and not arquivo_txt.closed:
            arquivo_txt.close()
        registro.fechar()


if __name__ == "__main__":
    sys.exit(principal())
