/*
 * coordinator.c — Servidor coordinador (rank 0). Responsable: Persona A.
 *
 * Flujo por ventana:
 *   1. Lee HOP muestras nuevas de la fuente y las preprocesa.
 *   2. Arma la ventana de WIN muestras (50 % de solapamiento).
 *   3. La envía cifrada a los 3 nodos.
 *   4. Recibe los 3 resultados (cifrados) y verifica que sean de la misma ventana.
 *   5. Calcula IAV / nivel / alarma, lo reporta y lo manda al servo y al buzzer.
 *
 * La fuente puede ser un CSV o el sensor (ver source.h). Con el sensor los datos
 * llegan solos a 100 Hz, así que no se espera entre ventanas, y el ciclo termina
 * al llegar a max_windows (0 = sin límite, hasta Ctrl+C).
 */
#include "coordinator.h"
#include "common.h"
#include "crypto.h"
#include "fusion.h"
#include "preprocess.h"
#include "source.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void send_stop_all(void)
{
    uint32_t zero = 0;
    for (int r = 1; r < N_RANKS; r++)
        sec_send(&zero, sizeof zero, r, TAG_STOP);
}

/* Lee y preprocesa una muestra. 1 = ok, 0 = fin de datos. */
static int next_sample(sample_t *out)
{
    sample_t raw;
    int rc = source_read(&raw);
    if (rc != 1) return 0;
    preprocess_sample(&raw, out);
    return 1;
}

static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

int run_coordinator(const char *data_path, int realtime, const char *results_path,
                    unsigned max_windows)
{
    if (source_open(data_path) != 0) {
        send_stop_all();
        return 1;
    }
    preprocess_init();
    fusion_init();

    FILE *log = results_path ? fopen(results_path, "w") : NULL;
    if (log)
        fprintf(log, "id,t0_ms,sta_lta,trigger,f_dom,rms,pga,std,iav,nivel,alarma,servo_deg,ms\n");

    static window_t w;          /* static: ~3 KB, evita usar mucho stack */
    static sample_t buf[WIN];
    memset(&w, 0, sizeof w);

    /* Llenar la primera ventana completa */
    int have = 0;
    while (have < WIN && next_sample(&buf[have])) have++;
    if (have < WIN) {
        fprintf(stderr, "[coord] datos insuficientes para una ventana (%d/%d)\n", have, WIN);
        goto done;
    }

    uint32_t id = 0;
    uint32_t n_new = WIN;
    const int live = source_is_live();
    if (live) realtime = 0;     /* el sensor ya marca el ritmo */
    printf("[coord] procesando '%s' (ventanas de %d muestras, avance %d)%s\n",
           data_path, WIN, HOP, live ? " desde el sensor" : realtime ? " en tiempo real" : "");
    fflush(stdout);

    for (;;) {
        /* --- 2. Armar ventana --- */
        w.id = ++id;
        w.n_new = n_new;
        w.t0_ms = buf[0].t_ms;
        for (int i = 0; i < WIN; i++) {
            w.ax[i] = buf[i].ax;
            w.ay[i] = buf[i].ay;
            w.az[i] = buf[i].az;
            w.mag[i] = sqrtf(buf[i].ax * buf[i].ax + buf[i].ay * buf[i].ay + buf[i].az * buf[i].az);
        }

        double t_start = MPI_Wtime();

        /* --- 3. Enviar a los 3 nodos --- */
        for (int r = 1; r < N_RANKS; r++)
            if (sec_send(&w, sizeof w, r, TAG_WINDOW) != 0) goto done;

        /* --- 4. Recibir resultados --- */
        res_stalta_t ra;
        res_fft_t    rf;
        res_stats_t  rs;
        if (sec_recv(&ra, sizeof ra, RANK_STALTA, TAG_RESULT, NULL) != (long)sizeof ra ||
            sec_recv(&rf, sizeof rf, RANK_FFT,    TAG_RESULT, NULL) != (long)sizeof rf ||
            sec_recv(&rs, sizeof rs, RANK_STATS,  TAG_RESULT, NULL) != (long)sizeof rs) {
            fprintf(stderr, "[coord] error recibiendo resultados de la ventana %u: abortando\n", w.id);
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
        if (ra.id != w.id || rf.id != w.id || rs.id != w.id) {
            fprintf(stderr, "[coord] resultados desincronizados (%u/%u/%u vs %u)\n",
                    ra.id, rf.id, rs.id, w.id);
            goto done;
        }

        double ms = (MPI_Wtime() - t_start) * 1000.0;

        /* --- 5. Fusionar y reportar --- */
        fusion_out_t fo;
        fusion_update(&ra, &rf, &rs, &fo);

        printf("[w %4u] t=%6.1fs | STA/LTA=%5.2f trig=%d | f_dom=%5.2fHz | RMS=%.4fg PGA=%.4fg | "
               "IAV=%5.1f %-11s servo=%3d° alarma=%s | %.2f ms\n",
               w.id, w.t0_ms / 1000.0, ra.ratio, ra.trigger, rf.f_dom, rs.rms, rs.pga,
               fo.iav, level_name(fo.level), fo.servo_deg, fo.alarm ? "ON " : "OFF", ms);
        fflush(stdout);
        if (log)
            fprintf(log, "%u,%llu,%.4f,%d,%.4f,%.6f,%.6f,%.6f,%.2f,%s,%d,%d,%.3f\n",
                    w.id, (unsigned long long)w.t0_ms, ra.ratio, ra.trigger, rf.f_dom,
                    rs.rms, rs.pga, rs.std, fo.iav, level_name(fo.level), fo.alarm,
                    fo.servo_deg, ms);

        /* Servo y buzzer (con CSV no hace nada). */
        source_actuate(fo.servo_deg, fo.alarm);

        if (max_windows && id >= max_windows) break;

        /* --- 1. Desplazar HOP muestras y leer las nuevas --- */
        memmove(buf, buf + HOP, (WIN - HOP) * sizeof(sample_t));
        int got = 0;
        while (got < HOP && next_sample(&buf[WIN - HOP + got])) got++;
        if (got < HOP) break;          /* fin de datos */
        n_new = HOP;

        if (realtime) sleep_ms(HOP * 1000 / FS_HZ);
    }

    printf("[coord] fin: %u ventanas procesadas\n", id);

done:
    send_stop_all();
    source_actuate(0, 0);       /* dejar el hardware en reposo */
    if (log) fclose(log);
    source_close();
    return 0;
}
