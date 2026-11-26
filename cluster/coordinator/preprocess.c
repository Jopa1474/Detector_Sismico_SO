/*
 * preprocess.c — Preprocesamiento de la señal en el coordinador.
 *
 * Por cada eje, en este orden:
 *   1. Componente estático: se resta un promedio móvil exponencial muy lento
 *      (gravedad, inclinación del sensor y offset de calibración). Se inicializa
 *      con la primera muestra, así el filtro siguiente no ve un "escalón" de 1 g
 *      al arrancar (que provocaría una falsa detección los primeros segundos).
 *   2. Pasa-altos Butterworth de 2.º orden a 0.5 Hz: quita derivas lentas.
 *   3. Pasa-bajos Butterworth de 2.º orden a 20 Hz: quita ruido de alta frecuencia.
 *
 * Los filtros son "biquads" (IIR de 2.º orden) con coeficientes del
 * Audio EQ Cookbook (R. Bristow-Johnson), forma directa II transpuesta.
 * Ecuación:  y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
 */
#include "preprocess.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define F_LOW   0.5      /* Hz, corte del pasa-altos */
#define F_HIGH  20.0     /* Hz, corte del pasa-bajos */
#define Q_BUTTER 0.70710678118654752   /* 1/sqrt(2): respuesta Butterworth (plana) */
#define ALPHA   0.005    /* promedio móvil: sigue cambios de ~2 s o más lentos */

typedef struct {
    double b0, b1, b2, a1, a2;  /* coeficientes normalizados (a0 = 1) */
    double z1, z2;              /* estado interno */
} biquad_t;

typedef struct {
    double   mean;              /* componente estático */
    biquad_t hp, lp;
} axis_t;

static axis_t axes[3];
static int    started;

static void biquad_design(biquad_t *f, int highpass, double fc)
{
    double w0 = 2.0 * M_PI * fc / FS_HZ;
    double c = cos(w0), alpha = sin(w0) / (2.0 * Q_BUTTER);
    double a0 = 1.0 + alpha;

    if (highpass) {
        f->b0 =  (1.0 + c) / 2.0;
        f->b1 = -(1.0 + c);
        f->b2 =  (1.0 + c) / 2.0;
    } else {
        f->b0 = (1.0 - c) / 2.0;
        f->b1 =  1.0 - c;
        f->b2 = (1.0 - c) / 2.0;
    }
    f->a1 = -2.0 * c;
    f->a2 = 1.0 - alpha;

    f->b0 /= a0; f->b1 /= a0; f->b2 /= a0;
    f->a1 /= a0; f->a2 /= a0;
    f->z1 = f->z2 = 0.0;
}

static double biquad_step(biquad_t *f, double x)
{
    double y = f->b0 * x + f->z1;
    f->z1 = f->b1 * x - f->a1 * y + f->z2;
    f->z2 = f->b2 * x - f->a2 * y;
    return y;
}

static float process_axis(axis_t *ax, float raw)
{
    ax->mean += ALPHA * (raw - ax->mean);
    double x = raw - ax->mean;
    x = biquad_step(&ax->hp, x);
    x = biquad_step(&ax->lp, x);
    return (float)x;
}

void preprocess_init(void)
{
    for (int i = 0; i < 3; i++) {
        axes[i].mean = 0.0;
        biquad_design(&axes[i].hp, 1, F_LOW);
        biquad_design(&axes[i].lp, 0, F_HIGH);
    }
    started = 0;
}

void preprocess_sample(const sample_t *in, sample_t *out)
{
    if (!started) {
        axes[0].mean = in->ax;
        axes[1].mean = in->ay;
        axes[2].mean = in->az;
        started = 1;
    }

    out->t_ms = in->t_ms;
    out->ax = process_axis(&axes[0], in->ax);
    out->ay = process_axis(&axes[1], in->ay);
    out->az = process_axis(&axes[2], in->az);
}