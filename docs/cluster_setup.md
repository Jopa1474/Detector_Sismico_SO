# Guía: armar el clúster de 3 máquinas

Responsable: Persona A. Tiempo estimado: 1–2 horas la primera vez.

En esta guía, **máquina 1** es la que corre el coordinador (rank 0) y el Nodo 1 (rank 1),
y las **máquinas 2 y 3** corren los nodos 2 y 3.

## 0. Requisitos

- Las 3 con **Linux nativo** (arranque dual o USB con persistencia). Ni VM ni WSL.
- Las 3 en la **misma red**. Las redes de la U a veces bloquean el tráfico entre equipos:
  si da problemas, usen el hotspot de un celular o un router propio.
- Mismo usuario en las 3 facilita todo (p. ej. `so`), aunque no es obligatorio.

## 1. Instalar dependencias (en las 3)

```bash
sudo apt update
sudo apt install build-essential openmpi-bin libopenmpi-dev libssl-dev \
                 openssh-server valgrind git
sudo systemctl enable --now ssh
mpirun --version        # anotar: debe dar EXACTAMENTE la misma versión en las 3
```

Si las versiones no coinciden (p. ej. Ubuntu 22.04 vs 24.04), lo más fácil es que las 3
tengan la misma versión de Ubuntu.

## 2. Averiguar las IPs (en las 3)

```bash
hostname -I     # la primera suele ser la de la red local, p. ej. 192.168.1.11
ip -br addr     # muestra el nombre de la interfaz (wlp2s0, enp3s0, ...) para el paso 8
```

## 3. SSH sin contraseña (solo en la máquina 1)

MPI arranca los procesos remotos por SSH, así que la máquina 1 debe poder entrar a las
otras dos sin que le pidan contraseña.

```bash
ssh-keygen -t ed25519            # Enter a todo, sin passphrase
ssh-copy-id usuario@IP_MAQUINA_2
ssh-copy-id usuario@IP_MAQUINA_3
ssh usuario@IP_MAQUINA_2 hostname   # debe responder sin pedir contraseña
```

Si el usuario es distinto en cada máquina, agreguen en `~/.ssh/config` de la máquina 1:

```
Host 192.168.1.11
    User usuario_de_la_2
Host 192.168.1.12
    User usuario_de_la_3
```

## 4. Clonar y compilar (en las 3, en la MISMA ruta)

MPI busca el ejecutable en la misma ruta en todas las máquinas.

```bash
cd ~
git clone <url-del-repo> Detector_Sismico_SO
cd Detector_Sismico_SO
git checkout develop
make
```

## 5. Llave compartida (generar en la 1, copiar a las otras)

```bash
make key
scp cluster.key usuario@IP_MAQUINA_2:~/Detector_Sismico_SO/
scp cluster.key usuario@IP_MAQUINA_3:~/Detector_Sismico_SO/
```

## 6. Hostfile (solo en la máquina 1)

```bash
cp cluster/hostfile.example cluster/hostfile
nano cluster/hostfile      # poner las IPs reales; la 1 con slots=2
```

## 7. Verificar

```bash
make check            # en la máquina 1 revisa también las otras dos por SSH
make hello-cluster    # prueba real: versiones, llave, latencia y eco cifrado
```

`make hello-cluster` debe terminar con `RESULTADO: clúster listo` y mostrar
**3 máquinas distintas**. Guarden captura de esto, sirve como evidencia.

## 8. Correr el sistema

```bash
make data                          # en la máquina 1
make run-cluster ARGS=--realtime   # con el monitor del sistema abierto en las 3
```

## Problemas comunes

| Síntoma | Causa probable | Solución |
|---|---|---|
| `mpirun` se queda pegado sin imprimir nada | firewall o MPI eligió otra interfaz de red | `sudo ufw disable` en las 3 (solo para la demo) y `make hello-cluster MPIFLAGS="--mca btl_tcp_if_include wlp2s0 --mca oob_tcp_if_include wlp2s0"` con su interfaz |
| `Permission denied (publickey)` | falta `ssh-copy-id` o usuario distinto | paso 3 |
| `orted: command not found` | en la otra máquina falta OpenMPI o no está en el PATH de sesiones no interactivas | instalar `openmpi-bin` ahí; revisar con `ssh IP which orted` |
| `execvp error ... seismic_cluster` | el repo no está en la misma ruta o falta `make` allá | paso 4 |
| `no se pudo abrir la llave` | falta `cluster.key` en alguna máquina | paso 5 |
| `autenticación fallida` | la llave no es la misma en todas | volver a copiar la de la máquina 1; `make check` muestra la huella |
| `There are not enough slots` | hostfile con menos de 4 slots | la 1 debe tener `slots=2` |
| Versiones distintas en el paso 2 de `hello` | Ubuntu distinto en alguna máquina | misma versión de Ubuntu en las 3 |