/*
 * source.h — Fuente de muestras del acelerómetro y salida a los actuadores.
 *
 * El coordinador no sabe de dónde vienen los datos. source_open() elige:
 *   - "/dev/..." o "device"  -> el sensor real, por medio de libseismic.a
 *                               (o la biblioteca simulada, según cómo se enlace)
 *   - cualquier otro texto   -> un archivo CSV generado con tools/gen_data
 */
#ifndef SEISMIC_SOURCE_H
#define SEISMIC_SOURCE_H

#include "common.h"

/* Abre la fuente. Devuelve 0 si ok. */
int  source_open(const char *spec);

/* Lee una muestra. Devuelve 1 si leyó, 0 si se acabaron los datos, -1 si error. */
int  source_read(sample_t *s);

/* 1 si los datos llegan en tiempo real (sensor): el coordinador no debe esperar. */
int  source_is_live(void);

/* Manda el ángulo del servo y el estado de la alarma al hardware.
 * Con CSV no hace nada. Solo envía lo que cambió. */
void source_actuate(int servo_deg, int alarm);

void source_close(void);

#endif
