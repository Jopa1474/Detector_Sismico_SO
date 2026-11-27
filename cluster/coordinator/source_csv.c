/*
 * source_csv.c — Lee muestras de un CSV:  t_ms,ax,ay,az   (aceleraciones en g)
 * Las líneas que empiezan con '#' o con texto (encabezado) se ignoran.
 */
#include "source_backends.h"

#include <stdio.h>
#include <stdlib.h>

static FILE *g_f = NULL;

int csv_open(const char *path)
{
    g_f = fopen(path, "r");
    if (!g_f) {
        perror("[source] fopen");
        return -1;
    }
    return 0;
}

int csv_read(sample_t *s)
{
    char line[256];
    while (g_f && fgets(line, sizeof line, g_f)) {
        unsigned long long t;
        float x, y, z;
        if (sscanf(line, "%llu,%f,%f,%f", &t, &x, &y, &z) == 4) {
            s->t_ms = t;
            s->ax = x; s->ay = y; s->az = z;
            return 1;
        }
        /* línea vacía, comentario o encabezado: se salta */
    }
    return 0;
}

void csv_close(void)
{
    if (g_f) fclose(g_f);
    g_f = NULL;
}
