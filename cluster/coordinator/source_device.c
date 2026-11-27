/*
 * source_device.c — Fuente de datos desde el sensor, por medio de libseismic.
 *
 * Solo usa las funciones de seismic.h; nunca toca /dev/seismic0 directamente.
 * Se enlaza contra libseismic.a (real) o libseismic_mock.a (simulada).
 */
#include "source_backends.h"
#include "seismic.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/* El ángulo cambia ±1° por ventana solo por el ruido; sin esta zona muerta el
 * servo físico estaría vibrando todo el tiempo. Solo se mueve con cambios >= 3°
 * (o para volver a 0). */
#define SERVO_DEADBAND_DEG 3

static int last_servo = -1;   /* -1 = todavía no se ha enviado nada */
static int last_alarm = -1;
static unsigned long overruns;

int device_open(const char *path)
{
    if (seismic_open(path) != 0) {
        fprintf(stderr, "[source] no se pudo abrir %s: %s\n",
                path ? path : "/dev/seismic0", strerror(errno));
        if (errno == ENOENT)
            fprintf(stderr, "[source] ¿está cargado el driver? (sudo insmod seismic.ko)\n");
        else if (errno == EACCES)
            fprintf(stderr, "[source] falta permiso sobre el dispositivo (sudo chmod 666 /dev/seismic0)\n");
        else if (errno == EBUSY)
            fprintf(stderr, "[source] otro proceso tiene abierto el dispositivo\n");
        return -1;
    }

    seismic_status_t st;
    if (seismic_get_status(&st) == 0)
        printf("[source] dispositivo abierto: %u Hz, fuente %s\n",
               st.rate_hz, st.simulated ? "SIMULADA" : "MPU-9250");

    last_servo = last_alarm = -1;
    overruns = 0;
    return 0;
}

int device_read(sample_t *s)
{
    seismic_reading_t r;
    if (seismic_read_sample(&r) != 0) {
        if (errno == EINTR) return 0;          /* señal: terminar ordenadamente */
        fprintf(stderr, "[source] error leyendo el sensor: %s\n", strerror(errno));
        return -1;
    }
    if (r.overrun && (++overruns == 1 || overruns % 100 == 0))
        fprintf(stderr, "[source] aviso: el driver perdió muestras (%lu veces)\n", overruns);

    s->t_ms = r.t_ms;
    s->ax = r.ax;
    s->ay = r.ay;
    s->az = r.az;
    return 1;
}

void device_actuate(int servo_deg, int alarm)
{
    /* Solo se envía lo que cambió: evita una llamada al driver por ventana. */
    int diff = servo_deg - last_servo;
    if (diff < 0) diff = -diff;
    if (last_servo < 0 || diff >= SERVO_DEADBAND_DEG || (servo_deg == 0 && last_servo != 0)) {
        if (seismic_set_servo(servo_deg) == 0) last_servo = servo_deg;
        else fprintf(stderr, "[source] no se pudo mover el servo: %s\n", strerror(errno));
    }
    if (alarm != last_alarm) {
        if (seismic_set_alarm(alarm) == 0) last_alarm = alarm;
        else fprintf(stderr, "[source] no se pudo cambiar la alarma: %s\n", strerror(errno));
    }
}

void device_close(void)
{
    seismic_close();   /* apaga la alarma y lleva el servo a 0 */
}
