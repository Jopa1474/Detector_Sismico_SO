/*
 * seismic_mock.c — Implementación SIMULADA de seismic.h (libseismic_mock.a).
 *
 * Sirve para probar el coordinador sin driver ni hardware. Tiene exactamente
 * las mismas funciones que la biblioteca real, así que el coordinador no cambia:
 * solo se enlaza contra esta en vez de libseismic.a.
 *
 * Genera un escenario de 60 s que se repite: reposo, vibración leve (8 Hz),
 * reposo y un "sismo" de 1–5 Hz (el mismo patrón que tools/gen_data).
 * Las órdenes de servo y alarma se imprimen en stderr cuando cambian.
 *
 * Variables de entorno:
 *   SEISMIC_MOCK_FAST=1   no esperar 10 ms entre muestras (pruebas rápidas)
 */
#include "seismic.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RATE_HZ 100
#define LOOP_S  60.0

static int      is_open;
static int      fast;
static uint64_t n;              /* muestras entregadas */
static int      servo_deg = -1; /* -1 = todavía sin orden */
static int      alarm_on  = -1;
static struct timespec next_tick;
static unsigned rng = 12345u;

/* Ruido gaussiano reproducible (Box–Muller sobre un LCG propio). */
static double urand(void)
{
    rng = rng * 1103515245u + 12345u;
    return ((rng >> 8) + 1.0) / 16777218.0;
}
static double gauss(void)
{
    return sqrt(-2.0 * log(urand())) * cos(2.0 * M_PI * urand());
}
static double env(double t, double a, double b)
{
    return (t < a || t > b) ? 0.0 : sin(M_PI * (t - a) / (b - a));
}

int seismic_open(const char *path)
{
    (void)path;
    if (is_open) { errno = EBUSY; return -1; }
    const char *f = getenv("SEISMIC_MOCK_FAST");
    fast = f && f[0] == '1';
    n = 0;
    servo_deg = alarm_on = -1;
    clock_gettime(CLOCK_MONOTONIC, &next_tick);
    is_open = 1;
    fprintf(stderr, "[mock] dispositivo simulado abierto (%s)\n", fast ? "rápido" : "100 Hz reales");
    return 0;
}

int seismic_read_sample(seismic_reading_t *out)
{
    if (!is_open) { errno = EBADF; return -1; }
    if (!out)     { errno = EINVAL; return -1; }

    if (!fast) {
        /* Esperar al siguiente múltiplo de 10 ms, como haría el driver. */
        next_tick.tv_nsec += 1000000000L / RATE_HZ;
        if (next_tick.tv_nsec >= 1000000000L) { next_tick.tv_nsec -= 1000000000L; next_tick.tv_sec++; }
        int rc;
        while ((rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_tick, NULL)) == EINTR)
            ;
        (void)rc;
    }

    double t_abs = (double)n / RATE_HZ;
    double t = fmod(t_abs, LOOP_S);
    double ax = 0.002 * gauss(), ay = 0.002 * gauss(), az = 1.0 + 0.002 * gauss();

    double e1 = (t >= 20 && t < 30) ? 1.0 : 0.0;           /* vibración leve */
    ax += e1 * 0.008 * sin(2 * M_PI * 8.0 * t);
    az += e1 * 0.005 * sin(2 * M_PI * 8.0 * t + 1.0);

    double e2 = env(t, 35, 50);                             /* sismo simulado */
    ax += e2 * (0.06 * sin(2 * M_PI * 2.0 * t) + 0.02 * sin(2 * M_PI * 4.5 * t));
    ay += e2 * (0.04 * sin(2 * M_PI * 1.3 * t + 0.5));
    az += e2 * (0.03 * sin(2 * M_PI * 3.0 * t + 2.0));

    out->t_ms = (uint64_t)(n * 1000u / RATE_HZ);
    out->ax = (float)ax;
    out->ay = (float)ay;
    out->az = (float)az;
    out->overrun = 0;
    n++;
    return 0;
}

int seismic_set_servo(int deg)
{
    if (!is_open) { errno = EBADF; return -1; }
    if (deg < 0) deg = 0;
    if (deg > 180) deg = 180;
    if (deg != servo_deg)
        fprintf(stderr, "[mock] servo -> %3d°\n", deg);
    servo_deg = deg;
    return 0;
}

int seismic_set_alarm(int on)
{
    if (!is_open) { errno = EBADF; return -1; }
    on = on ? 1 : 0;
    if (on != alarm_on)
        fprintf(stderr, "[mock] ALARMA %s\n", on ? "ENCENDIDA" : "apagada");
    alarm_on = on;
    return 0;
}

int seismic_get_status(seismic_status_t *out)
{
    if (!is_open) { errno = EBADF; return -1; }
    if (!out)     { errno = EINVAL; return -1; }
    out->rate_hz = RATE_HZ;
    out->simulated = 1;
    out->samples_total = n;
    out->samples_dropped = 0;
    return 0;
}

void seismic_close(void)
{
    if (!is_open) return;
    seismic_set_alarm(0);
    seismic_set_servo(0);
    is_open = 0;
    fprintf(stderr, "[mock] dispositivo cerrado tras %llu muestras\n", (unsigned long long)n);
}
