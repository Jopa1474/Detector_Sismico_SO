/*
 * fft.c — Nodo 2: análisis frecuencial. Responsable: Persona C.
 *
 * Idea:
 *   - Tomar w->mag (200 muestras), aplicar ventana de Hann y rellenar
 *     con ceros hasta FFT_N = 256.
 *   - FFT radix-2 (iterativa, ~40 líneas) o FFTW si se agrega -lfftw3f.
 *   - Resolución: FS_HZ / FFT_N = 0.39 Hz por bin.
 *   - f_dom = frecuencia del bin con mayor magnitud (ignorando el bin 0).
 *   - band_e[] = energía (suma de |X|^2) en bandas, por ejemplo:
 *       [0.5–2), [2–5), [5–10), [10–20] Hz
 *
 * TODO (Persona C): implementar.
 */
#include "nodes.h"

#define FFT_N 256

void fft_init(void)
{
    /* TODO: precalcular ventana de Hann / twiddles si se desea */
}

void fft_process(const window_t *w, res_fft_t *out)
{
    (void)w;
    out->f_dom = 0.0f;
    for (int b = 0; b < N_BANDS; b++) out->band_e[b] = 0.0f;
}

void fft_free(void)
{
}
