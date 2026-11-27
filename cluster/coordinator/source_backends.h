/*
 * source_backends.h — Uso interno de source.c: las dos fuentes posibles.
 * El resto del coordinador solo usa source.h.
 */
#ifndef SEISMIC_SOURCE_BACKENDS_H
#define SEISMIC_SOURCE_BACKENDS_H

#include "common.h"

/* source_csv.c */
int  csv_open(const char *path);
int  csv_read(sample_t *s);
void csv_close(void);

/* source_device.c */
int  device_open(const char *path);
int  device_read(sample_t *s);
void device_actuate(int servo_deg, int alarm);
void device_close(void);

#endif
