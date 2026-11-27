# Makefile raíz — Detector_Sismico_SO
#
#   make            compila el clúster y las herramientas
#   make key        genera la llave AES-256 compartida (cluster.key)
#   make data       genera datos sintéticos en data/sample.csv
#   make run        corre los 4 procesos en esta máquina (pruebas)
#   make run-cluster  corre en las 3 máquinas usando cluster/hostfile
#   make hello      prueba MPI + cifrado en esta máquina
#   make hello-cluster  prueba MPI + cifrado entre las 3 máquinas
#   make check      revisa que esta máquina tenga todo lo necesario
#   make run-device lee del sensor (/dev/seismic0) en esta máquina
#
# Biblioteca del sensor (por defecto la simulada, hasta que exista la real):
#   make SEISMIC_LIB=mock   enlaza lib/mock (datos simulados, no necesita driver)
#   make SEISMIC_LIB=real   enlaza lib/libseismic.a (la construye lib/Makefile)
# Para leer del sensor en el clúster:  make run-cluster DATA=/dev/seismic0
#   make valgrind   corre en local con valgrind (buscar fugas / accesos inválidos)
#   make clean

CC      := mpicc
HOSTCC  := gcc
CFLAGS  := -O2 -g -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE \
           -Iinclude -Icluster/common -Icluster/coordinator -Icluster/nodes
LDLIBS  := -lcrypto -lm

BIN     := bin
CLUSTER := $(BIN)/seismic_cluster
GEN     := $(BIN)/gen_data
HELLO   := $(BIN)/mpi_hello

SEISMIC_LIB ?= mock
MOCK_LIB    := build/lib/libseismic_mock.a
REAL_LIB    := lib/libseismic.a
LIBSEISMIC  := $(if $(filter real,$(SEISMIC_LIB)),$(REAL_LIB),$(MOCK_LIB))
LIBSTAMP    := build/.lib-$(SEISMIC_LIB)

SRCS := cluster/main.c \
        cluster/common/crypto.c \
        cluster/coordinator/coordinator.c \
        cluster/coordinator/source.c \
        cluster/coordinator/source_csv.c \
        cluster/coordinator/source_device.c \
        cluster/coordinator/preprocess.c \
        cluster/coordinator/fusion.c \
        cluster/nodes/node.c \
        cluster/nodes/stalta.c \
        cluster/nodes/fft.c \
        cluster/nodes/stats.c
OBJS := $(SRCS:%.c=build/%.o)

DATA     ?= data/sample.csv
DEVICE   ?= /dev/seismic0
KEY      ?= cluster.key
HOSTFILE ?= cluster/hostfile
MPIFLAGS ?=
ARGS     ?=

.PHONY: all key data run run-device run-cluster hello hello-cluster check valgrind clean FORCE

all: $(CLUSTER) $(GEN) $(HELLO)

$(CLUSTER): $(OBJS) $(LIBSEISMIC) $(LIBSTAMP) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBSEISMIC) $(LDLIBS)

# Marca de qué biblioteca se enlazó: al cambiar SEISMIC_LIB se vuelve a enlazar.
$(LIBSTAMP):
	@mkdir -p build && rm -f build/.lib-* && touch $@

# Biblioteca simulada (lib/mock): no necesita driver ni hardware.
$(MOCK_LIB): lib/mock/seismic_mock.c include/seismic.h
	@mkdir -p $(dir $@)
	$(HOSTCC) -O2 -g -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -Iinclude -c $< -o build/lib/seismic_mock.o
	ar rcs $@ build/lib/seismic_mock.o

# Biblioteca real: la construye su propio Makefile en lib/.
$(REAL_LIB): FORCE
	@test -f lib/Makefile || { echo "Falta lib/Makefile (biblioteca real). Mientras tanto use SEISMIC_LIB=mock."; exit 1; }
	$(MAKE) -C lib

build/%.o: %.c $(wildcard cluster/*/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(GEN): tools/gen_data.c | $(BIN)
	$(HOSTCC) -O2 -Wall -Wextra -std=c11 -o $@ $< -lm

$(HELLO): tools/mpi_hello.c cluster/common/crypto.c $(wildcard cluster/common/*.h) | $(BIN)
	$(CC) $(CFLAGS) -o $@ tools/mpi_hello.c cluster/common/crypto.c $(LDLIBS)

$(BIN):
	mkdir -p $(BIN)

key:
	@if [ -f $(KEY) ]; then echo "$(KEY) ya existe (bórrela a mano si quiere otra)"; \
	else openssl rand -hex 32 | tr -d '\n' > $(KEY) && chmod 600 $(KEY) && echo "Llave generada en $(KEY). Cópiela a las 3 máquinas (scp), NO la suba a git."; fi

data: $(GEN)
	$(GEN) $(DATA)

run: all
	mpirun $(MPIFLAGS) -np 4 --oversubscribe $(CLUSTER) $(DATA) --key $(KEY) --out results.csv $(ARGS)

run-device: all
	mpirun $(MPIFLAGS) -np 4 --oversubscribe $(CLUSTER) $(DEVICE) --key $(KEY) --out results.csv $(ARGS)

run-cluster: all
	mpirun $(MPIFLAGS) -np 4 --hostfile $(HOSTFILE) $(CURDIR)/$(CLUSTER) $(abspath $(DATA)) \
	       --key $(CURDIR)/$(KEY) --out $(CURDIR)/results.csv $(ARGS)

hello: $(HELLO)
	mpirun $(MPIFLAGS) -np 4 --oversubscribe $(HELLO) --key $(KEY)

hello-cluster: $(HELLO)
	mpirun $(MPIFLAGS) -np 4 --hostfile $(HOSTFILE) $(CURDIR)/$(HELLO) --key $(CURDIR)/$(KEY)

check:
	@bash scripts/check_node.sh

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