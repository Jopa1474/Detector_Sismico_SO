/*
 * common.h — Contrato compartido entre coordinador y nodos.
 *
 * IMPORTANTE: cualquier cambio a estas estructuras debe acordarse entre todos,
 * porque coordinador y nodos tienen que compilar exactamente la misma versión.
 */
#ifndef SEISMIC_COMMON_H
#define SEISMIC_COMMON_H

#include <stdint.h>

/* ---------------- Parámetros de adquisición ---------------- */
#define FS_HZ    100            /* frecuencia de muestreo (Hz)               */
#define WIN      200            /* muestras por ventana = 2 s                */
#define HOP      100            /* avance entre ventanas = 50 % solapamiento */
#define N_BANDS  4              /* bandas de energía que reporta el Nodo 2   */

/* ---------------- Roles (rank MPI) ---------------- */
enum {
    RANK_COORD  = 0,            /* servidor coordinador */
    RANK_STALTA = 1,            /* Nodo 1: detección STA/LTA */
    RANK_FFT    = 2,            /* Nodo 2: análisis frecuencial */
    RANK_STATS  = 3,            /* Nodo 3: métricas estadísticas */
    N_RANKS     = 4
};

/* ---------------- Tags de mensajes MPI ---------------- */
enum {
    TAG_WINDOW = 10,            /* coordinador -> nodo: ventana de datos   */
    TAG_RESULT = 20,            /* nodo -> coordinador: resultado          */
    TAG_STOP   = 99             /* coordinador -> nodo: terminar           */
};

/* Una muestra cruda del acelerómetro (en g). */
typedef struct {
    uint64_t t_ms;
    float ax, ay, az;
} sample_t;

/* Ventana que el coordinador envía (cifrada) a cada nodo. */
typedef struct {
    uint32_t id;                /* número de ventana, empieza en 1                 */
    uint32_t n_new;             /* muestras nuevas al final de la ventana:
                                   WIN en la primera ventana, HOP en las demás.
                                   El Nodo 1 lo usa para no duplicar historial.     */
    uint64_t t0_ms;             /* timestamp de la primera muestra                 */
    float ax[WIN], ay[WIN], az[WIN];   /* ya calibrados y filtrados (g)            */
    float mag[WIN];             /* magnitud de la aceleración dinámica (g)         */
} window_t;

/* Resultados que regresa cada nodo. Todos llevan el id de la ventana. */
typedef struct {
    uint32_t id;
    float    ratio;             /* STA/LTA */
    uint8_t  trigger;           /* 1 si se detectó evento */
} res_stalta_t;

typedef struct {
    uint32_t id;
    float    f_dom;             /* frecuencia dominante (Hz) */
    float    band_e[N_BANDS];   /* energía por banda */
} res_fft_t;

typedef struct {
    uint32_t id;
    float    rms;               /* aceleración RMS (g) */
    float    pga;               /* pico de aceleración (g) */
    float    std;               /* desviación estándar (g) */
} res_stats_t;

#endif /* SEISMIC_COMMON_H */
