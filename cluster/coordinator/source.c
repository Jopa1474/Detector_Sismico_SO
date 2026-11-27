/*
 * source.c — Elige la fuente de datos (CSV o dispositivo) y reparte las llamadas.
 */
#include "source.h"
#include "source_backends.h"

#include <string.h>

static enum { SRC_NONE, SRC_CSV, SRC_DEVICE } kind = SRC_NONE;

int source_open(const char *spec)
{
    if (!spec) return -1;
    if (!strncmp(spec, "/dev/", 5) || !strcmp(spec, "device")) {
        if (device_open(!strcmp(spec, "device") ? NULL : spec) != 0) return -1;
        kind = SRC_DEVICE;
    } else {
        if (csv_open(spec) != 0) return -1;
        kind = SRC_CSV;
    }
    return 0;
}

int source_read(sample_t *s)
{
    switch (kind) {
    case SRC_CSV:    return csv_read(s);
    case SRC_DEVICE: return device_read(s);
    default:         return -1;
    }
}

int source_is_live(void)
{
    return kind == SRC_DEVICE;
}

void source_actuate(int servo_deg, int alarm)
{
    if (kind == SRC_DEVICE) device_actuate(servo_deg, alarm);
}

void source_close(void)
{
    if (kind == SRC_CSV)    csv_close();
    if (kind == SRC_DEVICE) device_close();
    kind = SRC_NONE;
}
