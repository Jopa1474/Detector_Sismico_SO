#ifndef SEISMIC_COORDINATOR_H
#define SEISMIC_COORDINATOR_H

/* Corre el ciclo del coordinador (rank 0).
 * realtime=1 espera entre ventanas como si los datos llegaran del sensor.
 * results_path puede ser NULL para no guardar CSV de resultados. */
int run_coordinator(const char *data_path, int realtime, const char *results_path);

#endif
