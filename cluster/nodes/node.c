/*
 * node.c — Loop común de los nodos de procesamiento (ranks 1..3).
 * No hace falta modificarlo: cada quien trabaja en su archivo de algoritmo.
 */
#include "nodes.h"
#include "crypto.h"

#include <stdio.h>
#include <unistd.h>

int run_node(int rank)
{
    static window_t w;
    char host[64] = "?";
    gethostname(host, sizeof host);

    switch (rank) {
    case RANK_STALTA: stalta_init(); break;
    case RANK_FFT:    fft_init();    break;
    case RANK_STATS:  stats_init();  break;
    default: return 1;
    }
    printf("[nodo %d] listo en host '%s'\n", rank, host);
    fflush(stdout);

    unsigned long processed = 0;
    for (;;) {
        MPI_Status st;
        long n = sec_recv(&w, sizeof w, RANK_COORD, MPI_ANY_TAG, &st);
        if (st.MPI_TAG == TAG_STOP) break;
        if (n != (long)sizeof w) {
            /* Falló la autenticación GCM o el tamaño: no se puede confiar en el
             * canal. Se detiene todo el clúster en vez de dejar al coordinador
             * esperando un resultado que nunca llega. */
            fprintf(stderr, "[nodo %d] ventana inválida o no autenticada: abortando clúster\n", rank);
            MPI_Abort(MPI_COMM_WORLD, 2);
        }

        switch (rank) {
        case RANK_STALTA: {
            res_stalta_t r = {0};
            stalta_process(&w, &r);
            r.id = w.id;
            sec_send(&r, sizeof r, RANK_COORD, TAG_RESULT);
            break;
        }
        case RANK_FFT: {
            res_fft_t r = {0};
            fft_process(&w, &r);
            r.id = w.id;
            sec_send(&r, sizeof r, RANK_COORD, TAG_RESULT);
            break;
        }
        case RANK_STATS: {
            res_stats_t r = {0};
            stats_process(&w, &r);
            r.id = w.id;
            sec_send(&r, sizeof r, RANK_COORD, TAG_RESULT);
            break;
        }
        }
        processed++;
    }

    switch (rank) {
    case RANK_STALTA: stalta_free(); break;
    case RANK_FFT:    fft_free();    break;
    case RANK_STATS:  stats_free();  break;
    }
    printf("[nodo %d] fin: %lu ventanas procesadas\n", rank, processed);
    return 0;
}
