#ifndef SEISMIC_COORDINATOR_H
#define SEISMIC_COORDINATOR_H

/* Corre el ciclo del coordinador (rank 0).
 * data_path: un CSV, o "/dev/seismic0" / "device" para leer del sensor.
 * realtime=1 espera entre ventanas como si los datos llegaran del sensor
 * (se ignora cuando la fuente ya es el sensor).
 * results_path puede ser NULL para no guardar CSV de resultados.
 * max_windows: detenerse tras esa cantidad de ventanas (0 = sin límite). */
int run_coordinator(const char *data_path, int realtime, const char *results_path,
                    unsigned max_windows);

#endif
