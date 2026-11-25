/*
 * nodes.h — Interfaz de los algoritmos de cada nodo.
 *
 * Cada nodo recibe la ventana completa y llena su estructura de resultado.
 * El campo id del resultado lo llena node.c, no hace falta tocarlo.
 */
#ifndef SEISMIC_NODES_H
#define SEISMIC_NODES_H

#include "common.h"

/* Nodo 1 — STA/LTA (Persona C). Guarda historial entre ventanas. */
void stalta_init(void);
void stalta_process(const window_t *w, res_stalta_t *out);
void stalta_free(void);

/* Nodo 2 — FFT (Persona C). */
void fft_init(void);
void fft_process(const window_t *w, res_fft_t *out);
void fft_free(void);

/* Nodo 3 — Estadísticas (Persona B). */
void stats_init(void);
void stats_process(const window_t *w, res_stats_t *out);
void stats_free(void);

/* Loop genérico de un nodo: recibe ventanas hasta TAG_STOP. */
int run_node(int rank);

#endif
