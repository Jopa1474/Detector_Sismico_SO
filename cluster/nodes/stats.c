/*
 * stats.c — Nodo 3: métricas de intensidad.
 *
 * Sobre w->mag (magnitud de la aceleración dinámica, en g):
 *   - rms = sqrt( (1/N) * sum(mag^2) )      energía promedio de la ventana
 *   - pga = max |mag|                        pico de aceleración (Peak Ground Acceleration)
 *   - std = sqrt( (1/N) * sum((mag - media)^2) )   dispersión alrededor de la media
 *
 * Se acumula en double para no perder precisión al sumar 200 valores pequeños.
 */
#include "nodes.h"

#include <math.h>

void stats_init(void)
{
    /* No guarda estado entre ventanas. */
}

void stats_process(const window_t *w, res_stats_t *out)
{
    double sum = 0.0, sum_sq = 0.0, peak = 0.0;

    for (int i = 0; i < WIN; i++) {
        double v = w->mag[i];
        double a = fabs(v);
        sum    += v;
        sum_sq += v * v;
        if (a > peak) peak = a;
    }

    double mean = sum / WIN;
    double var  = sum_sq / WIN - mean * mean;   /* E[x^2] - E[x]^2 */
    if (var < 0.0) var = 0.0;                    /* por redondeo puede quedar -1e-18 */

    out->rms = (float)sqrt(sum_sq / WIN);
    out->pga = (float)peak;
    out->std = (float)sqrt(var);
}

void stats_free(void)
{
}