/*
 * fft.c - Nodo 2: analisis frecuencial.
 * Responsable: Persona C.
 */

#include "nodes.h"
#include <math.h>

#define FFT_N 256
#define PI 3.14159265358979323846

// Ventana de Hann y factores de rotacion para la FFT
static double hann[WIN];
static double tw_re[FFT_N / 2];
static double tw_im[FFT_N / 2];

void fft_init(void)
{
    // Precalcular la ventana de Hann
    for (int i = 0; i < WIN; i++) {
        hann[i] = 0.5 * (1.0 - cos(2.0 * PI * i / (WIN - 1)));
    }

    // Precalcular los factores de rotacion
    for (int k = 0; k < FFT_N / 2; k++) {

        double angle = -2.0 * PI * k / FFT_N;

        tw_re[k] = cos(angle);
        tw_im[k] = sin(angle);
    }
}

static void fft_radix2(double re[], double im[]){

    // Reordenamiento por inversion de bits
    for (int i = 1, j = 0; i < FFT_N; i++) {

        int bit = FFT_N >> 1;
        
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }

        j ^= bit;

        if (i < j) {
            double temp = re[i];
            re[i] = re[j];
            re[j] = temp;
            temp = im[i];
            im[i] = im[j];
            im[j] = temp;
        }
    }

    // Mariposas del algoritmo Cooley-Tukey
    for (int len = 2; len <= FFT_N; len <<= 1){

        int half = len / 2;
        int step = FFT_N / len;

        for (int i = 0; i < FFT_N; i += len){

            for (int j = 0; j < half; j++){

                int a = i + j;
                int b = a + half;
                int k = j * step;

                double tr = tw_re[k] * re[b] - tw_im[k] * im[b];
                double ti = tw_re[k] * im[b] + tw_im[k] * re[b];

                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
            }
        }
    }
}

void fft_process(const window_t *w, res_fft_t *out){

    out->id = w->id;
    out->f_dom = 0.0f;

    for (int b = 0; b < N_BANDS; b++)
        out->band_e[b] = 0.0f;

    double power[FFT_N / 2 + 1] = {0};

    const float *axes[] = {w->ax, w->ay, w->az};

    // FFT para cada eje y suma de las potencias
    for (int axis = 0; axis < 3; axis++){

        double re[FFT_N] = {0};
        double im[FFT_N] = {0};
        double mean = 0.0;

        // Aplicar ventana de Hann y copiar a re[]
        for (int i = 0; i < WIN && i < FFT_N; i++){

            if (!isfinite(axes[axis][i]))
                return; // Salir si hay NaN o Inf

            mean += axes[axis][i];
        }

        mean /= WIN;

        // Eliminar DC y aplicar Hann
        for (int i = 0; i < WIN; i++){
            re[i] = (axes[axis][i] - mean) * hann[i];
        }

        // Ejecutar la FFT de 256 puntos
        fft_radix2(re, im);

        // Acumular la potencia de los tres ejes
        for (int k = 1; k <= FFT_N / 2; k++){
            power[k] += re[k] * re[k] + im[k] * im[k];
        }
    }

    double max_power = 0.0;

    // Frecuencia dominante y energia por bandas
    for (int k = 1; k <= FFT_N / 2; k++) {

        double f = (double)k * FS_HZ / FFT_N;
        double p = power[k];

        if (p > max_power) {
            max_power = p;
            out->f_dom = (float)f;
        }

        if (f >= 0.5 && f < 2.0)
            out->band_e[0] += (float)p;

        else if (f >= 2.0 && f < 5.0)
            out->band_e[1] += (float)p;

        else if (f >= 5.0 && f < 10.0)
            out->band_e[2] += (float)p;

        else if (f >= 10.0 && f <= 20.0)
            out->band_e[3] += (float)p;
    }
}

void fft_free(void)
{
    // No se utilizo memoria dinamica.
}