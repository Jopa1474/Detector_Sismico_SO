
/*
 * stalta.c - Nodo 1: deteccion STA/LTA.
 * Responsable: Persona C.
 */

#include "nodes.h"
#include <math.h>
#include <string.h>

#define STA_N (FS_HZ / 2)
#define LTA_N (FS_HZ * 10)
#define TRIGGER_RATIO 3.0
#define EPSILON 1e-12

// Historial circular de energia de las muestras.
static double history[LTA_N];
static unsigned pos, count;
static double sum_sta, sum_lta;

void stalta_init(void){
    memset(history, 0, sizeof history);

    pos = 0;
    count = 0;
    sum_sta = 0.0;
    sum_lta = 0.0;
}

void stalta_process(const window_t *w, res_stalta_t *out){
    
    out->id = w->id;
    out->ratio = 0.0f;
    out->trigger = 0;

    // Validar la cantidad de muestras nuevas.
    if (w->n_new == 0 || w->n_new > WIN)
        return;

    double peak = 0.0;

    // Procesar cada muestra nueva.
    for (unsigned i = WIN - w->n_new; i < WIN; i++){

        double mag = w->mag[i];

        if (!isfinite(mag)){
            stalta_init(); // Reiniciar si hay NaN o Inf.
            peak = 0.0;
            continue;
    
        }

        double energy = mag * mag;

        // Retirar la muestra antigua del STA
        if (count >= STA_N){
            unsigned old = (pos + LTA_N - STA_N) % LTA_N;
            sum_sta -= history[old];
        }

        // Retirar la muestra antigua del LTA
        if (count >= LTA_N){
            sum_lta -= history[pos];
        }

        // Agregar la nueva muestra al historial y a las sumas
        history[pos] = energy;
        sum_sta += energy;
        sum_lta += energy;
        pos = (pos + 1) % LTA_N;

        if (count < LTA_N)
            count++;
        
        //  Esperar 10 segundos para tener historial

        if (count >= LTA_N){

            double sta = sum_sta / STA_N;
            double lta = sum_lta / LTA_N;

            double ratio = (lta > EPSILON) ? sta / lta : 0.0;

            // Conservar el mayor ratio de la ventana
            if (ratio > peak)
                peak = ratio;
            
        }
    }
    out->ratio = (float)peak;
    out->trigger = (peak > TRIGGER_RATIO);

}

void stalta_free(void)
{
    stalta_init();
}