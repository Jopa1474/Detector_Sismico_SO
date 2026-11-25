/*
 * preprocess.c — Responsable: Persona B.
 *
 * Versión base: solo quita el componente estático con un promedio móvil
 * exponencial muy lento (deja pasar las vibraciones, elimina la gravedad).
 *
 * TODO (Persona B):
 *   - Calibración: offsets medidos en reposo por eje.
 *   - Filtro pasabanda 0.5–20 Hz (p. ej. Butterworth de 2.º orden como
 *     pasa-altos 0.5 Hz + pasa-bajos 20 Hz, en forma biquad).
 */
#include "preprocess.h"

static float mean_x, mean_y, mean_z;
static int   started;

/* alpha ~ 1/(FS * 2 s): sigue cambios muy lentos (gravedad / inclinación). */
#define ALPHA 0.005f

void preprocess_init(void)
{
    mean_x = mean_y = mean_z = 0.0f;
    started = 0;
}

void preprocess_sample(const sample_t *in, sample_t *out)
{
    if (!started) {
        mean_x = in->ax; mean_y = in->ay; mean_z = in->az;
        started = 1;
    }
    mean_x += ALPHA * (in->ax - mean_x);
    mean_y += ALPHA * (in->ay - mean_y);
    mean_z += ALPHA * (in->az - mean_z);

    out->t_ms = in->t_ms;
    out->ax = in->ax - mean_x;
    out->ay = in->ay - mean_y;
    out->az = in->az - mean_z;

    /* TODO (Persona B): aplicar aquí el filtro pasabanda 0.5–20 Hz. */
}
