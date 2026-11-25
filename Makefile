# Makefile raíz — Detector_Sismico_SO
#
#   make            compila el clúster y las herramientas
#   make key        genera la llave AES-256 compartida (cluster.key)
#   make data       genera datos sintéticos en data/sample.csv
#   make run        corre los 4 procesos en esta máquina (pruebas)
#   make run-cluster  corre en las 3 máquinas usando cluster/hostfile
#   make valgrind   corre en local con valgrind (buscar fugas / accesos inválidos)
#   make clean

CC      := mpicc
HOSTCC  := gcc
CFLAGS  := -O2 -g -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE \
           -Icluster/common -Icluster/coordinator -Icluster/nodes
LDLIBS  := -lcrypto -lm

BIN     := bin
CLUSTER := $(BIN)/seismic_cluster
GEN     := $(BIN)/gen_data

SRCS := cluster/main.c \
        cluster/common/crypto.c \
        cluster/coordinator/coordinator.c \
        cluster/coordinator/source_csv.c \
        cluster/coordinator/preprocess.c \
        cluster/coordinator/fusion.c \
        cluster/nodes/node.c \
        cluster/nodes/stalta.c \
        cluster/nodes/fft.c \
        cluster/nodes/stats.c
OBJS := $(SRCS:%.c=build/%.o)

DATA     ?= data/sample.csv
KEY      ?= cluster.key
HOSTFILE ?= cluster/hostfile
MPIFLAGS ?=
ARGS     ?=

.PHONY: all key data run run-cluster valgrind clean

all: $(CLUSTER) $(GEN)

$(CLUSTER): $(OBJS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: %.c $(wildcard cluster/*/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(GEN): tools/gen_data.c | $(BIN)
	$(HOSTCC) -O2 -Wall -Wextra -std=c11 -o $@ $< -lm

$(BIN):
	mkdir -p $(BIN)

key:
	@if [ -f $(KEY) ]; then echo "$(KEY) ya existe (bórrela a mano si quiere otra)"; \
	else openssl rand -hex 32 | tr -d '\n' > $(KEY) && chmod 600 $(KEY) && echo "Llave generada en $(KEY). Cópiela a las 3 máquinas (scp), NO la suba a git."; fi

data: $(GEN)
	$(GEN) $(DATA)

run: all
	mpirun $(MPIFLAGS) -np 4 --oversubscribe $(CLUSTER) $(DATA) --key $(KEY) --out results.csv $(ARGS)

run-cluster: all
	mpirun $(MPIFLAGS) -np 4 --hostfile $(HOSTFILE) $(CURDIR)/$(CLUSTER) $(CURDIR)/$(DATA) \
	       --key $(CURDIR)/$(KEY) --out $(CURDIR)/results.csv $(ARGS)

# OpenMPI reporta "fugas" propias en MPI_Init/MPI_Finalize y libpmix: son normales.
# Lo importante es que no aparezca ningún reporte con frames de nuestros .c
# (crypto.c, coordinator.c, node.c, stalta.c, ...).
valgrind: all
	mpirun $(MPIFLAGS) -np 4 --oversubscribe valgrind --leak-check=full -q \
	       $(CLUSTER) $(DATA) --key $(KEY) 2> valgrind.log; \
	echo "Reportes que apuntan a nuestro código:"; \
	grep -E "\((crypto|coordinator|source_csv|preprocess|fusion|node|stalta|fft|stats)\.c:" valgrind.log || echo "  ninguno :)"

clean:
	rm -rf build $(BIN) results.csv valgrind.log
