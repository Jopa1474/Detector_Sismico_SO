/*
 * preprocess.h — Calibración, componente estático y filtro pasabanda.
 * Responsable: Persona B.
 */
#ifndef SEISMIC_PREPROCESS_H
#define SEISMIC_PREPROCESS_H

#include "common.h"

/* Se llama una vez antes de procesar. */
void preprocess_init(void);

/* Recibe una muestra cruda y la deja calibrada y filtrada (0.5–20 Hz) en out.
 * Se llama muestra por muestra, en orden, así que puede guardar estado
 * (p. ej. los coeficientes y memoria de un filtro IIR). */
void preprocess_sample(const sample_t *in, sample_t *out);

#endif
