# libseismic.a

Pendiente para la entrega final. Biblioteca estática que es la única que habla con el
driver (`/dev/seismic0`). Funciones previstas:

- `seismic_open()` / `seismic_close()`
- `seismic_read_sample(sample_t *s)`
- `seismic_set_servo(int deg)`
- `seismic_set_alarm(int on)`

El coordinador la usará mediante una nueva fuente (`cluster/coordinator/source_device.c`)
en lugar de `source_csv.c`.
