/*
 * stalta.c — Nodo 1: detección de eventos STA/LTA. Responsable: Persona C.
 *
 * Idea:
 *   - STA = promedio de |mag| (o mag^2) en los últimos 0.5 s  (50 muestras)
 *   - LTA = promedio en los últimos 10 s                       (1000 muestras)
 *   - ratio = STA / LTA ; trigger = ratio > umbral (p. ej. 3.0)
 *
 * Como las ventanas se solapan 50 %, solo se agregan al historial las
 * últimas w->n_new muestras de cada ventana (WIN en la primera, HOP después).
 *
 * TODO (Persona C): implementar con un buffer circular de LTA_N muestras.
 */
#include "nodes.h"

#define STA_N (FS_HZ / 2)     /* 0.5 s */
#define LTA_N (FS_HZ * 10)    /* 10 s  */
#define TRIGGER_RATIO 3.0f

void stalta_init(void)
{
    /* TODO: reservar/limpiar el buffer circular de historial */
}

void stalta_process(const window_t *w, res_stalta_t *out)
{
    (void)w;
    /* TODO: agregar w->mag[WIN - w->n_new .. WIN-1] al historial,
     *       calcular STA y LTA y llenar out->ratio y out->trigger.
     *       Ojo: mientras el historial tenga menos de LTA_N muestras,
     *       no disparar (todavía no hay referencia de ruido de fondo). */
    out->ratio = 0.0f;
    out->trigger = 0;
}

void stalta_free(void)
{
    /* TODO: liberar memoria si se usó malloc */
}
