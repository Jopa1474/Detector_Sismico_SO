# libseismic.a

Biblioteca estática que es la **única** parte del proyecto que habla con el driver
(`/dev/seismic0`). El coordinador solo usa sus funciones.

## Contratos

- **Lo que la biblioteca ofrece:** [`include/seismic.h`](../include/seismic.h)
  (`seismic_open`, `seismic_read_sample`, `seismic_set_servo`, `seismic_set_alarm`,
  `seismic_get_status`, `seismic_close`).
- **Cómo habla con el driver:** [`include/seismic_ioctl.h`](../include/seismic_ioctl.h)
  (formato de `read()` y comandos `ioctl`).

## Qué falta (biblioteca real)

- `lib/seismic.c` que implemente `seismic.h` usando `open`, `read` e `ioctl` sobre el driver:
  - leer varias muestras por `read()` y entregarlas de una en una,
  - convertir cuentas a g (`valor / SEISMIC_ACCEL_LSB_PER_G`) y `t_ns` a ms desde la primera muestra,
  - marcar `overrun` cuando la muestra traiga `SEISMIC_SF_OVERRUN`.
- `lib/Makefile` que genere `lib/libseismic.a` (`ar rcs`). El Makefile raíz la usa con
  `make SEISMIC_LIB=real`.
- Un programa de prueba (por ejemplo `lib/test_seismic.c`) que imprima muestras y mueva el servo.

## Biblioteca simulada

`lib/mock/seismic_mock.c` implementa las mismas funciones con datos simulados. Es la que
se enlaza por defecto (`SEISMIC_LIB=mock`) mientras no exista la real.
