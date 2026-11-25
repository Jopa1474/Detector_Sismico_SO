/*
 * fusion.h — Combina los resultados de los 3 nodos.
 * Responsable: Persona A.
 */
#ifndef SEISMIC_FUSION_H
#define SEISMIC_FUSION_H

#include "common.h"

typedef enum {
    LVL_NORMAL = 0, LVL_BAJA, LVL_MODERADA, LVL_ELEVADA, LVL_MUY_ELEVADA
} level_t;

typedef struct {
    float   iav;        /* Índice de Actividad Vibratoria 0–100 */
    level_t level;
    int     alarm;      /* 1 = buzzer encendido */
    int     servo_deg;  /* 0–180, para cuando exista el driver */
} fusion_out_t;

const char *level_name(level_t l);

void fusion_init(void);
void fusion_update(const res_stalta_t *a, const res_fft_t *f, const res_stats_t *s,
                   fusion_out_t *out);

#endif
