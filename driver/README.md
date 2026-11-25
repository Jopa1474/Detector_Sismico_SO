# Driver (módulo de kernel)

Pendiente para la entrega final.

- Módulo de kernel en C que expone `/dev/seismic0` (verificar con `lsmod`).
- Lectura del MPU-9250 por I²C y control del servo (PWM) y el buzzer (GPIO).
- Pendiente confirmar con el profe si se puede usar el subsistema I²C del kernel
  o si hay que hacer I²C/PWM a mano sobre GPIO ("no usar ningún driver del sistema").
