/*
 * fusion.c — Responsable: Persona A.
 *
 * Versión base funcional, con umbrales de ejemplo.
 * TODO (Persona A):
 *   - Calibrar RMS_REF y los umbrales con datos reales del MPU-9250.
 *   - Decidir si el IAV también pondera PGA / energía por bandas.
 */
#include "fusion.h"

#define RMS_REF        0.05f   /* g que corresponden a IAV = 100 (experimental) */
#define ALARM_ON_IAV   40.0f   /* encender si IAV >= esto y STA/LTA disparó     */
#define ALARM_OFF_IAV  25.0f   /* apagar si IAV < esto (histéresis)             */
#define PERSIST        2       /* ventanas consecutivas para cambiar de estado   */

static int alarm_state, on_count, off_count;

const char *level_name(level_t l)
{
    static const char *n[] = { "NORMAL", "BAJA", "MODERADA", "ELEVADA", "MUY_ELEVADA" };
    return (l >= LVL_NORMAL && l <= LVL_MUY_ELEVADA) ? n[l] : "?";
}

void fusion_init(void)
{
    alarm_state = on_count = off_count = 0;
}

void fusion_update(const res_stalta_t *a, const res_fft_t *f, const res_stats_t *s,
                   fusion_out_t *out)
{
    (void)f; /* TODO: usar f_dom / band_e si se decide ponderar por frecuencia */

    float iav = 100.0f * s->rms / RMS_REF;
    if (iav < 0.0f)   iav = 0.0f;
    if (iav > 100.0f) iav = 100.0f;
    out->iav = iav;

    /* 5 niveles iguales de 20 puntos */
    int lvl = (int)(iav / 20.0f);
    if (lvl > LVL_MUY_ELEVADA) lvl = LVL_MUY_ELEVADA;
    out->level = (level_t)lvl;

    /* Alarma con persistencia e histéresis */
    if (!alarm_state) {
        on_count = (a->trigger && iav >= ALARM_ON_IAV) ? on_count + 1 : 0;
        if (on_count >= PERSIST) { alarm_state = 1; off_count = 0; }
    } else {
        off_count = (iav < ALARM_OFF_IAV) ? off_count + 1 : 0;
        if (off_count >= PERSIST) { alarm_state = 0; on_count = 0; }
    }
    out->alarm = alarm_state;

    out->servo_deg = (int)(iav * 180.0f / 100.0f + 0.5f);
}
