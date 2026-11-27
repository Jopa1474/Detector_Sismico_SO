/*
 * main.c — Punto de entrada del clúster. Un solo ejecutable para todos;
 * el rank MPI decide el rol:
 *   rank 0 -> coordinador, rank 1 -> STA/LTA, rank 2 -> FFT, rank 3 -> estadísticas
 *
 * Uso:
 *   mpirun -np 4 bin/seismic_cluster <datos.csv | /dev/seismic0> [--realtime]
 *          [--out resultados.csv] [--key cluster.key] [--max-windows N]
 */
#include "common.h"
#include "coordinator.h"
#include "crypto.h"
#include "nodes.h"

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *p)
{
    fprintf(stderr, "uso: mpirun -np %d %s <datos.csv | /dev/seismic0> [--realtime] [--out resultados.csv]\n"
                    "       [--key archivo] [--max-windows N]\n", N_RANKS, p);
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size != N_RANKS) {
        if (rank == 0)
            fprintf(stderr, "Se necesitan exactamente %d procesos (coordinador + 3 nodos), hay %d.\n",
                    N_RANKS, size);
        MPI_Finalize();
        return 1;
    }

    const char *data = NULL, *out = NULL, *key = NULL;
    int realtime = 0;
    unsigned max_windows = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--realtime")) realtime = 1;
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--key") && i + 1 < argc) key = argv[++i];
        else if (!strcmp(argv[i], "--max-windows") && i + 1 < argc) max_windows = (unsigned)strtoul(argv[++i], NULL, 10);
        else if (argv[i][0] != '-') data = argv[i];
    }

    /* Todos los procesos deben cargar la llave; si alguno falla, abortan todos. */
    int ok = (sec_init(key) == 0), all_ok = 0;
    MPI_Allreduce(&ok, &all_ok, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (!all_ok || (rank == 0 && !data)) {
        if (rank == 0 && !data) usage(argv[0]);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int rc = (rank == RANK_COORD) ? run_coordinator(data, realtime, out, max_windows) : run_node(rank);

    sec_cleanup();
    MPI_Finalize();
    return rc;
}
