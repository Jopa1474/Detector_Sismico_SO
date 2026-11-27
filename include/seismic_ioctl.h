/*
 * seismic_ioctl.h — Contrato entre el DRIVER (módulo de kernel) y la BIBLIOTECA.
 *
 * Este archivo lo incluyen los dos lados:
 *   - el driver:     #include "../include/seismic_ioctl.h"   (código de kernel)
 *   - libseismic.a:  #include "seismic_ioctl.h"               (espacio de usuario)
 * Por eso solo usa tipos de <linux/types.h> (__u64, __s16, ...), que existen en
 * ambos lados con el mismo tamaño. Cualquier cambio aquí se acuerda entre todos.
 *
 * ------------------------------------------------------------------------------
 * LO QUE EL DRIVER DEBE CUMPLIR
 * ------------------------------------------------------------------------------
 *  Dispositivo
 *   - Módulo llamado "seismic" (aparece en lsmod) que crea /dev/seismic0.
 *   - Un solo proceso a la vez: un segundo open() devuelve -EBUSY.
 *   - Toma muestras del MPU-9250 a SEISMIC_RATE_HZ y las guarda en un buffer
 *     circular (FIFO). Si el buffer se llena, descarta la muestra MÁS VIEJA,
 *     suma 1 a samples_dropped y marca SEISMIC_SF_OVERRUN en la siguiente.
 *   - Sin punto flotante en el kernel: las aceleraciones van en cuentas crudas
 *     del sensor; la conversión a g la hace la biblioteca.
 *
 *  read(fd, buf, count)
 *   - Entrega solo muestras completas: floor(count / sizeof(struct seismic_sample))
 *     como máximo, o las que haya. Devuelve los bytes copiados.
 *   - count < sizeof(struct seismic_sample)  -> -EINVAL.
 *   - Sin muestras: se bloquea hasta que llegue al menos una.
 *     Con O_NONBLOCK devuelve -EAGAIN. Si llega una señal, -ERESTARTSYS.
 *
 *  ioctl(fd, cmd, arg)
 *   - SEISMIC_IOC_SET_SERVO: arg apunta a un __u32 de 0 a 180 (grados).
 *                            Fuera de rango -> -EINVAL.
 *   - SEISMIC_IOC_SET_ALARM: arg apunta a un __u32; 0 = apagada, otro = encendida.
 *   - SEISMIC_IOC_GET_INFO:  llena una struct seismic_info.
 *   - SEISMIC_IOC_FLUSH:     vacía el FIFO (sin argumento).
 *   - Comando desconocido   -> -ENOTTY.
 *
 *  release() (al cerrar el archivo, también si el proceso muere con Ctrl+C)
 *   - Apaga la alarma y lleva el servo a 0 grados. Así el buzzer nunca queda
 *     sonando aunque el coordinador termine de golpe.
 * ------------------------------------------------------------------------------
 */
#ifndef SEISMIC_IOCTL_H
#define SEISMIC_IOCTL_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define SEISMIC_DEVICE_PATH     "/dev/seismic0"
#define SEISMIC_RATE_HZ         100     /* muestras por segundo */
#define SEISMIC_ACCEL_LSB_PER_G 16384   /* MPU-9250 configurado en ±2 g */
#define SEISMIC_SERVO_MAX_DEG   180

/* Una muestra, tal como la entrega read(). 16 bytes, sin relleno. */
struct seismic_sample {
	__u64 t_ns;          /* ktime_get_ns() al tomar la muestra (reloj monótono) */
	__s16 ax, ay, az;    /* aceleración cruda; en g = valor / SEISMIC_ACCEL_LSB_PER_G */
	__u16 flags;         /* combinación de SEISMIC_SF_* */
};

#define SEISMIC_SF_SIMULATED 0x0001  /* la muestra la generó el driver, no el sensor */
#define SEISMIC_SF_OVERRUN   0x0002  /* se perdieron muestras justo antes de esta */

/* Estado del driver, para SEISMIC_IOC_GET_INFO. */
struct seismic_info {
	__u32 rate_hz;          /* frecuencia de muestreo real */
	__u32 lsb_per_g;        /* cuentas por g */
	__u32 fifo_capacity;    /* muestras que caben en el buffer */
	__u32 source;           /* SEISMIC_SRC_* */
	__u64 samples_total;    /* muestras tomadas desde que se cargó el módulo */
	__u64 samples_dropped;  /* muestras descartadas por buffer lleno */
};

#define SEISMIC_SRC_SIMULATED 0  /* fuente simulada dentro del driver (para pruebas) */
#define SEISMIC_SRC_HARDWARE  1  /* MPU-9250 real */

#define SEISMIC_IOC_MAGIC 0xE7

#define SEISMIC_IOC_SET_SERVO _IOW(SEISMIC_IOC_MAGIC, 1, __u32)
#define SEISMIC_IOC_SET_ALARM _IOW(SEISMIC_IOC_MAGIC, 2, __u32)
#define SEISMIC_IOC_GET_INFO  _IOR(SEISMIC_IOC_MAGIC, 3, struct seismic_info)
#define SEISMIC_IOC_FLUSH     _IO(SEISMIC_IOC_MAGIC, 4)

#endif /* SEISMIC_IOCTL_H */
