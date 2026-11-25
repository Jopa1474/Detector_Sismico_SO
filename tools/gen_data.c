/*
 * gen_data.c — Genera datos sintéticos del acelerómetro para probar el clúster
 * sin el hardware. Responsable de mejorarlo: Persona B.
 *
 * Salida CSV: t_ms,ax,ay,az  (g, 100 Hz). El eje Z incluye 1 g de gravedad.
 *
 * Escenario por defecto (60 s):
 *    0–20 s  reposo (solo ruido)
 *   20–30 s  vibración leve (8 Hz, p. ej. golpecitos en la mesa)
 *   35–50 s  "sismo" simulado (1–5 Hz, envolvente que sube y baja)
 *   50–60 s  reposo
 *
 * Uso: gen_data [salida.csv] [segundos] [semilla]
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define FS 100
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Ruido gaussiano (Box–Muller) */
static double gauss(void)
{
    double u1 = (rand() + 1.0) / (RAND_MAX + 2.0);
    double u2 = (rand() + 1.0) / (RAND_MAX + 2.0);
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

/* Envolvente suave entre a y b (0 fuera, 1 en el centro) */
static double env(double t, double a, double b)
{
    if (t < a || t > b) return 0.0;
    return sin(M_PI * (t - a) / (b - a));
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "data/sample.csv";
    double dur = argc > 2 ? atof(argv[2]) : 60.0;
    unsigned seed = argc > 3 ? (unsigned)atoi(argv[3]) : 42u;
    srand(seed);

    FILE *f = fopen(path, "w");
    if (!f) { perror("fopen"); return 1; }
    fprintf(f, "t_ms,ax,ay,az\n");

    const double noise = 0.002;   /* g */
    long n = (long)(dur * FS);
    for (long i = 0; i < n; i++) {
        double t = (double)i / FS;
        double ax = noise * gauss(), ay = noise * gauss(), az = 1.0 + noise * gauss();

        /* vibración leve */
        double e1 = (t >= 20 && t < 30) ? 1.0 : 0.0;
        ax += e1 * 0.008 * sin(2 * M_PI * 8.0 * t);
        az += e1 * 0.005 * sin(2 * M_PI * 8.0 * t + 1.0);

        /* sismo simulado */
        double e2 = env(t, 35, 50);
        ax += e2 * (0.06 * sin(2 * M_PI * 2.0 * t) + 0.02 * sin(2 * M_PI * 4.5 * t));
        ay += e2 * (0.04 * sin(2 * M_PI * 1.3 * t + 0.5));
        az += e2 * (0.03 * sin(2 * M_PI * 3.0 * t + 2.0));

        fprintf(f, "%ld,%.6f,%.6f,%.6f\n", (long)(t * 1000.0 + 0.5), ax, ay, az);
    }
    fclose(f);
    printf("Generadas %ld muestras (%.0f s) en %s\n", n, dur, path);
    return 0;
}
