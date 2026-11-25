/*
 * source.h — Fuente de muestras del acelerómetro.
 *
 * Por ahora (taller) se lee un CSV generado con tools/gen_data.
 * Para la entrega final se agrega una implementación que use libseismic.a
 * (lee del driver /dev/seismic0) sin cambiar el resto del coordinador.
 */
#ifndef SEISMIC_SOURCE_H
#define SEISMIC_SOURCE_H

#include "common.h"

/* Abre la fuente. Devuelve 0 si ok. */
int  source_open(const char *path);

/* Lee una muestra. Devuelve 1 si leyó, 0 si se acabaron los datos, -1 si error. */
int  source_read(sample_t *s);

void source_close(void);

#endif
