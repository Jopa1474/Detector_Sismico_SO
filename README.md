# Detector_Sismico_SO
Proyecto Grupal 2 de Principios de Sistemas Operativos

Sistema experimental de detección y caracterización de actividad sísmica con un MPU-9250,
driver propio de kernel, biblioteca estática `libseismic.a` y procesamiento distribuido en
3 nodos con OpenMPI y mensajes cifrados con AES-256-GCM.

## Estructura

```
driver/        Módulo de kernel (/dev/seismic0)            -> entrega final
lib/           Biblioteca estática libseismic.a            -> entrega final
cluster/
  common/      common.h (contrato) y crypto.c (AES-256-GCM sobre MPI)
  coordinator/ coordinador: fuente de datos, preprocesamiento, fusión de resultados
  nodes/       node.c (loop común), stalta.c, fft.c, stats.c
  main.c       un solo ejecutable; el rank MPI decide el rol
tools/         gen_data.c: datos sintéticos para probar sin hardware
hardware/      diagramas y esquemáticos
docs/          documentación IEEE
data/          datos generados (no se versionan)
```

| Rank | Rol | Archivo |
|------|-----|---------|
| 0 | Coordinador | `cluster/coordinator/coordinator.c` |
| 1 | Nodo 1 – STA/LTA | `cluster/nodes/stalta.c` |
| 2 | Nodo 2 – FFT | `cluster/nodes/fft.c` |
| 3 | Nodo 3 – Estadísticas | `cluster/nodes/stats.c` |

## Dependencias (Ubuntu/Debian, en las 3 máquinas)

```bash
sudo apt install build-essential openmpi-bin libopenmpi-dev libssl-dev valgrind
```

## Uso en una sola máquina (desarrollo)

```bash
make            # compila bin/seismic_cluster y bin/gen_data
make key        # genera cluster.key (llave AES-256 compartida)
make data       # genera data/sample.csv (reposo, vibración leve y "sismo" simulado)
make run        # coordinador + 3 nodos en esta máquina; guarda results.csv
make run ARGS=--realtime   # avanza a la velocidad real del sensor (1 ventana/s)
make valgrind   # revisa accesos inválidos y fugas en nuestro código
```

Para ver que los mensajes viajan cifrados:

```bash
SEISMIC_DEBUG_CRYPTO=1 make run
```

## Uso en las 3 máquinas físicas

Guía detallada con solución de problemas: [docs/cluster_setup.md](docs/cluster_setup.md).

1. Las 3 máquinas en la misma red, con Linux nativo y las mismas versiones de OpenMPI.
2. SSH sin contraseña desde la máquina 1 hacia las otras dos (`ssh-keygen` + `ssh-copy-id`).
3. Clonar el repo **en la misma ruta** en las 3 (p. ej. `~/Detector_Sismico_SO`) y correr `make` en cada una.
4. Generar la llave una sola vez y copiarla: `make key && scp cluster.key usuario@maquina2:~/Detector_Sismico_SO/` (igual para la 3).
5. Copiar también `data/sample.csv` solo en la máquina 1 (el coordinador es el único que lo lee).
6. `cp cluster/hostfile.example cluster/hostfile` y poner las IPs reales.
7. Verificar con `make check` y `make hello-cluster` (debe mostrar 3 máquinas distintas).
8. `make run-cluster` (agregar `ARGS=--realtime` para la demo con el monitor del sistema abierto).

Si `mpirun` se queda pegado entre máquinas, casi siempre es el firewall o que elige la
interfaz de red equivocada: probar con `MPIFLAGS="--mca btl_tcp_if_include <interfaz>"`.

## Protocolo

Cada mensaje MPI va como `[IV 12 B | texto cifrado | tag GCM 16 B]`. El IV es aleatorio por
mensaje y el tag autentica también origen, destino y tag MPI. Si un nodo recibe un
mensaje alterado o cifrado con otra llave, se aborta todo el clúster.