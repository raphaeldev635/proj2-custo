# Projeto 2 - Custo - SO 2026.2 - Grupo "Desce dai, Gugu!"
#
# Uso:
#   make                 compila (gcc -O2)
#   make run N=100000000 compila (se precisar) e roda uma vez com o N dado
#   make sudo N=...      roda com sudo (permite subir a prioridade)
#   make clean           apaga o executavel
#
# Para trocar a flag de otimizacao (use a MESMA em todas as maquinas):
#   make OPT=-O0
#   make OPT=-O3 run N=100000000

CC  ?= gcc
OPT ?= -O2
CFLAGS = $(OPT) -Wall -Wextra
N ?= 100000000

all: crivo

# recompila se o .c ou o .h mudarem, ou se a flag de otimizacao mudar
crivo: crivo.c condicoes.h .opt
	$(CC) $(CFLAGS) -o $@ crivo.c

# guarda a flag usada; so reescreve se mudou (forca recompilar)
.opt: FORCE
	@echo '$(OPT)' | cmp -s - $@ || echo '$(OPT)' > $@

run: crivo
	./crivo $(N)

sudo: crivo
	sudo ./crivo $(N)

clean:
	rm -f crivo .opt

FORCE:
.PHONY: all run sudo clean FORCE
