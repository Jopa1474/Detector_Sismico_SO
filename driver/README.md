# Driver (módulo de kernel)

Módulo de kernel en C llamado `seismic` que crea `/dev/seismic0` (verificar con `lsmod`).

## Contrato

Todo lo que el driver debe cumplir está en
[`include/seismic_ioctl.h`](../include/seismic_ioctl.h): formato de las muestras de `read()`,
comandos `ioctl` (servo, alarma, info, vaciar buffer), comportamiento con buffer lleno,
un solo proceso a la vez, y apagar la alarma y el servo en `release()`.

## Plan

1. Núcleo con **fuente simulada** (no necesita hardware): registro del dispositivo,
   `open`/`read`/`ioctl`/`release`, FIFO y un temporizador que genera muestras a 100 Hz
   con `SEISMIC_SF_SIMULATED`.
2. Parte que habla con el hardware, según lo que decida el profe:
   - Arduino por USB: driver USB propio (sin `cdc_acm`) y un protocolo binario con el firmware.
   - Raspberry Pi: lectura del MPU-9250 por I²C y servo/buzzer por GPIO/PWM.
