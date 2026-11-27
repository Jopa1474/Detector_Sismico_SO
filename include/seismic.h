/*
 * seismic.h — Contrato de la BIBLIOTECA libseismic.a (lo que ve el coordinador).
 *
 * La biblioteca es la ÚNICA parte del proyecto que habla con /dev/seismic0.
 * El coordinador solo usa estas funciones; nunca hace open/read/ioctl directo.
 *
 * Convención de retorno (igual que las llamadas al sistema):
 *   0 = éxito, -1 = error y errno indica la causa.
 *
 * Hay dos implementaciones con exactamente estas funciones:
 *   - lib/        -> libseismic.a real, habla con el driver (Persona B)
 *   - lib/mock/   -> libseismic_mock.a, genera datos simulados (para probar
 *                    el coordinador sin driver ni hardware)
 */
#ifndef SEISMIC_H
#define SEISMIC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Una muestra ya convertida a unidades físicas. */
typedef struct {
    uint64_t t_ms;      /* milisegundos desde la primera muestra leída (empieza en 0) */
    float    ax, ay, az;/* aceleración en g (incluye la gravedad, sin filtrar) */
    int      overrun;   /* 1 si el driver perdió muestras justo antes de esta */
} seismic_reading_t;

/* Estado del dispositivo. */
typedef struct {
    unsigned rate_hz;
    int      simulated;       /* 1 si los datos vienen de una fuente simulada */
    uint64_t samples_total;
    uint64_t samples_dropped;
} seismic_status_t;

/* Abre el dispositivo. path = NULL usa "/dev/seismic0".
 * Errores típicos: ENOENT (driver no cargado), EBUSY (ya lo usa otro proceso),
 * EACCES (falta permiso sobre /dev/seismic0). */
int  seismic_open(const char *path);

/* Lee la siguiente muestra. Se bloquea hasta que haya una (100 por segundo).
 * Devuelve -1 con errno = EINTR si una señal interrumpió la espera. */
int  seismic_read_sample(seismic_reading_t *out);

/* Mueve el servo. deg se recorta al rango 0..180. */
int  seismic_set_servo(int deg);

/* Enciende (on != 0) o apaga (on == 0) el buzzer. */
int  seismic_set_alarm(int on);

/* Consulta el estado del driver. */
int  seismic_get_status(seismic_status_t *out);

/* Apaga la alarma, pone el servo en 0 y cierra el dispositivo. */
void seismic_close(void);

#ifdef __cplusplus
}
#endif

#endif /* SEISMIC_H */
