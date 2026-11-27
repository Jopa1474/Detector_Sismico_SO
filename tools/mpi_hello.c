/*
 * mpi_hello.c - Prueba del clúster antes de correr el sistema completo.
 * Responsable: Persona A.
 *
 * Verifica, en este orden:
 *   1. Que los 4 procesos arrancan y en qué máquina quedó cada uno.
 *   2. Que todas las máquinas tienen la misma versión de OpenMPI.
 *   3. Que todas cargan la misma llave AES (si no, el descifrado falla).
 *   4. Latencia de mensajes sin cifrar (1 byte, ida y vuelta).
 *   5. Mensajes cifrados del tamaño real de una ventana: ida, vuelta y
 *      comparación byte por byte de que llegó lo mismo que se envió.
 *
 * Uso:
 *   mpirun -np 4 bin/mpi_hello --key cluster.key
 *   mpirun -np 4 --hostfile cluster/hostfile $PWD/bin/mpi_hello --key $PWD/cluster.key
 */
#include "common.h"
#include "crypto.h"

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N_PING   200          /* repeticiones para medir latencia */
#define TAG_PING 1
#define TAG_ECHO 2
#define NAME_LEN 64
#define VER_LEN  128

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const char *key = NULL;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--key") && i + 1 < argc) key = argv[++i];

    /* ---------- 1 y 2: host y versión de cada proceso ---------- */
    char host[NAME_LEN] = {0};
    gethostname(host, sizeof host - 1);

    char ver[VER_LEN] = {0};
    int vlen = 0;
    char full[MPI_MAX_LIBRARY_VERSION_STRING];
    MPI_Get_library_version(full, &vlen);
    /* Solo la primera línea (p. ej. "Open MPI v4.1.6, ...") */
    snprintf(ver, sizeof ver, "%.*s", VER_LEN - 1, full);
    char *nl = strchr(ver, '\n');
    if (nl) *nl = '\0';
    char *comma = strchr(ver, ',');
    if (comma) *comma = '\0';

    char *all_hosts = NULL, *all_vers = NULL;
    if (rank == 0) {
        all_hosts = calloc((size_t)size, NAME_LEN);
        all_vers  = calloc((size_t)size, VER_LEN);
    }
    MPI_Gather(host, NAME_LEN, MPI_CHAR, all_hosts, NAME_LEN, MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Gather(ver,  VER_LEN,  MPI_CHAR, all_vers,  VER_LEN,  MPI_CHAR, 0, MPI_COMM_WORLD);

    int fails = 0;
    if (rank == 0) {
        static const char *roles[] = { "coordinador", "nodo 1 STA/LTA", "nodo 2 FFT", "nodo 3 stats" };
        printf("== 1. Procesos (%d) ==\n", size);
        for (int r = 0; r < size; r++)
            printf("  rank %d  %-16s en '%s'  (%s)\n", r, r < 4 ? roles[r] : "extra",
                   all_hosts + r * NAME_LEN, all_vers + r * VER_LEN);

        if (size != N_RANKS) {
            printf("  [FALLA] se esperaban %d procesos\n", N_RANKS);
            fails++;
        }

        /* ¿cuántas máquinas distintas? */
        int distinct = 0;
        for (int r = 0; r < size; r++) {
            int seen = 0;
            for (int q = 0; q < r; q++)
                if (!strcmp(all_hosts + r * NAME_LEN, all_hosts + q * NAME_LEN)) seen = 1;
            if (!seen) distinct++;
        }
        printf("  máquinas distintas: %d %s\n", distinct,
               distinct >= 3 ? "[OK]" : "(para la demo deben ser 3; en local es normal ver 1)");

        printf("== 2. Versión de OpenMPI ==\n");
        int same = 1;
        for (int r = 1; r < size; r++)
            if (strcmp(all_vers, all_vers + r * VER_LEN)) same = 0;
        printf("  %s\n", same ? "[OK] todas iguales" : "[FALLA] versiones distintas: instalar la misma en todas");
        if (!same) fails++;
    }

    /* ---------- 3: llave ---------- */
    int ok = (sec_init(key) == 0), all_ok = 0;
    MPI_Allreduce(&ok, &all_ok, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (rank == 0) {
        printf("== 3. Llave AES ==\n");
        printf("  %s\n", all_ok ? "[OK] todos los procesos cargaron una llave"
                                : "[FALLA] algún proceso no encontró la llave (ver mensajes arriba)");
    }
    if (!all_ok) {
        free(all_hosts); free(all_vers);
        MPI_Finalize();
        return 1;
    }

    /* ---------- 4: latencia sin cifrar ---------- */
    if (rank == 0) printf("== 4. Latencia sin cifrar (1 B, ida y vuelta, promedio de %d) ==\n", N_PING);
    for (int r = 1; r < size; r++) {
        char b = 'x';
        if (rank == 0) {
            double t0 = MPI_Wtime();
            for (int i = 0; i < N_PING; i++) {
                MPI_Send(&b, 1, MPI_CHAR, r, TAG_PING, MPI_COMM_WORLD);
                MPI_Recv(&b, 1, MPI_CHAR, r, TAG_PING, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            double us = (MPI_Wtime() - t0) / N_PING * 1e6;
            printf("  0 <-> %d : %8.1f µs\n", r, us);
        } else if (rank == r) {
            for (int i = 0; i < N_PING; i++) {
                MPI_Recv(&b, 1, MPI_CHAR, 0, TAG_PING, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(&b, 1, MPI_CHAR, 0, TAG_PING, MPI_COMM_WORLD);
            }
        }
    }

    /* ---------- 5: eco cifrado con el tamaño real de una ventana ---------- */
    window_t *tx = calloc(1, sizeof *tx), *rx = calloc(1, sizeof *rx);
    if (rank == 0) printf("== 5. Eco cifrado AES-256-GCM (%zu B por mensaje) ==\n", sizeof(window_t));
    for (int r = 1; r < size; r++) {
        if (rank == 0) {
            /* patrón conocido para comparar al regreso */
            tx->id = 1234u + (uint32_t)r;
            for (int i = 0; i < WIN; i++) tx->mag[i] = (float)i * 0.001f * (float)r;

            double t0 = MPI_Wtime();
            int good = sec_send(tx, sizeof *tx, r, TAG_ECHO) == 0 &&
                       sec_recv(rx, sizeof *rx, r, TAG_ECHO, NULL) == (long)sizeof *rx &&
                       memcmp(tx, rx, sizeof *tx) == 0;
            double ms = (MPI_Wtime() - t0) * 1e3;
            printf("  0 <-> %d : %s  (%.2f ms)\n", r,
                   good ? "[OK] llegó idéntico" : "[FALLA] llave distinta o datos alterados", ms);
            if (!good) fails++;
        } else if (rank == r) {
            long n = sec_recv(rx, sizeof *rx, 0, TAG_ECHO, NULL);
            if (n == (long)sizeof *rx) {
                sec_send(rx, sizeof *rx, 0, TAG_ECHO);
            } else {
                /* Devolver algo para que el coordinador no se quede esperando. */
                memset(rx, 0, sizeof *rx);
                sec_send(rx, sizeof *rx, 0, TAG_ECHO);
            }
        }
    }

    if (rank == 0)
        printf("\n%s\n", fails ? "RESULTADO: hay fallas, revisar arriba."
                               : "RESULTADO: clúster listo para 'make run-cluster'.");

    free(tx); free(rx);
    free(all_hosts); free(all_vers);
    sec_cleanup();
    MPI_Finalize();
    return fails ? 1 : 0;
}