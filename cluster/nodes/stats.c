/*
 * stats.c — Nodo 3: métricas de intensidad. Responsable: Persona B.
 *
 * Sobre w->mag (aceleración dinámica, g):
 *   - rms = sqrt( (1/N) * sum(mag^2) )
 *   - pga = max |mag|
 *   - std = desviación estándar de mag
 *
 * TODO (Persona B): implementar.
 */
#include "nodes.h"

void stats_init(void)
{
}

void stats_process(const window_t *w, res_stats_t *out)
{
    (void)w;
    out->rms = 0.0f;
    out->pga = 0.0f;
    out->std = 0.0f;
}

void stats_free(void)
{
}
