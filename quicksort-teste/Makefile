# Orquestracao multiplataforma: Linux e Windows (GNU Make + Python 3 + GCC/MinGW).
ifeq ($(OS),Windows_NT)
PYTHON ?= python
else
PYTHON ?= python3
endif
CC = gcc
OPT ?= -O2
CFLAGS = $(OPT) -Wall -Wextra -std=c11

N ?= 100000
REPS_ANALISE ?= 10
REPS_TERMINAL ?= 1
PCT ?= 100
ORD ?= r
ENT ?= v
TAMANHOS ?= 100 500 1000 5000 10000 50000 100000 250000 500000 750000 1000000 2500000 5000000 7500000 10000000
ARQ_TXT ?= evidencias/teste_01_txt_resultados_detalhados.txt
ARQ_CSV ?= evidencias/teste_02_csv_dados_estruturados.csv
ARQ_LOG ?= evidencias/registro_integral_execucao.log

.PHONY: all quick experimento-completo analise-txt analise-csv terminal-controle quick-run quick-todos quick-todos-csv compilar clean
.SILENT:

all: experimento-completo

quick: compilar

experimento-completo:
	@$(PYTHON) executar_experimento.py completo --cc "$(CC)" --cflags "$(CFLAGS)" --tamanhos "$(TAMANHOS)" --reps-analise $(REPS_ANALISE) --reps-terminal $(REPS_TERMINAL) --pct $(PCT) --ordem $(ORD) --entrada $(ENT) --saida-txt "$(ARQ_TXT)" --saida-csv "$(ARQ_CSV)" --evidencia "$(ARQ_LOG)"

analise-txt:
	@$(PYTHON) executar_experimento.py txt --cc "$(CC)" --cflags "$(CFLAGS)" --tamanhos "$(TAMANHOS)" --reps-analise $(REPS_ANALISE) --pct $(PCT) --ordem $(ORD) --entrada $(ENT) --saida-txt "$(ARQ_TXT)" --evidencia "$(ARQ_LOG)"

analise-csv:
	@$(PYTHON) executar_experimento.py csv --cc "$(CC)" --cflags "$(CFLAGS)" --tamanhos "$(TAMANHOS)" --reps-analise $(REPS_ANALISE) --pct $(PCT) --ordem $(ORD) --entrada $(ENT) --saida-csv "$(ARQ_CSV)" --evidencia "$(ARQ_LOG)"

terminal-controle:
	@$(PYTHON) executar_experimento.py terminal --cc "$(CC)" --cflags "$(CFLAGS)" --tamanhos "$(TAMANHOS)" --reps-terminal $(REPS_TERMINAL) --pct $(PCT) --ordem $(ORD) --entrada $(ENT) --evidencia "$(ARQ_LOG)"

quick-run:
	@$(PYTHON) executar_experimento.py terminal --cc "$(CC)" --cflags "$(CFLAGS)" --tamanhos "$(N)" --reps-terminal $(REPS_TERMINAL) --pct $(PCT) --ordem $(ORD) --entrada $(ENT) --evidencia "$(ARQ_LOG)"

quick-todos: terminal-controle
quick-todos-csv: analise-csv

compilar:
	@$(PYTHON) executar_experimento.py compilar --cc "$(CC)" --cflags "$(CFLAGS)"

clean:
	@$(PYTHON) executar_experimento.py limpar --saida-txt "$(ARQ_TXT)" --saida-csv "$(ARQ_CSV)" --evidencia "$(ARQ_LOG)"
