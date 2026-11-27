#!/usr/bin/env bash
# check_node.sh - Revisa que esta máquina esté lista para ser parte del clúster.
# Responsable: Persona A.
#
# Uso (desde la raíz del repo, en CADA una de las 3 máquinas):
#   bash scripts/check_node.sh
#
# En la máquina 1 (la del coordinador), si existe cluster/hostfile, además
# revisa por SSH que las otras máquinas tengan el repo, el binario y la llave.

cd "$(dirname "$0")/.." || exit 1
REPO="$(pwd)"
FAILS=0

ok()   { printf '  \033[32m[OK]\033[0m    %s\n' "$1"; }
bad()  { printf '  \033[31m[FALTA]\033[0m %s\n' "$1"; FAILS=$((FAILS + 1)); }
warn() { printf '  \033[33m[OJO]\033[0m   %s\n' "$1"; }

echo "== Máquina: $(hostname)  |  repo: $REPO"

echo "== Sistema"
if grep -qi microsoft /proc/version 2>/dev/null; then
    bad "esto es WSL: el enunciado pide Linux nativo"
else
    VIRT="$(systemd-detect-virt 2>/dev/null)"
    VIRT="${VIRT:-none}"
    if [ "$VIRT" = "none" ]; then ok "Linux nativo"; else bad "máquina virtual detectada ($VIRT): el enunciado pide Linux sin VMs"; fi
fi

echo "== Herramientas"
for cmd in gcc make mpicc mpirun openssl ssh; do
    if command -v "$cmd" >/dev/null 2>&1; then ok "$cmd"; else bad "$cmd  (sudo apt install build-essential openmpi-bin libopenmpi-dev openssl openssh-client)"; fi
done
command -v valgrind >/dev/null 2>&1 && ok "valgrind" || warn "valgrind no está (sudo apt install valgrind)"

if command -v mpirun >/dev/null 2>&1; then
    echo "  versión OpenMPI: $(mpirun --version 2>/dev/null | head -1)   <- debe ser igual en las 3"
fi

if echo '#include <openssl/evp.h>' | gcc -E -x c - >/dev/null 2>&1; then
    ok "headers de OpenSSL (libssl-dev)"
else
    bad "headers de OpenSSL (sudo apt install libssl-dev)"
fi

echo "== Servidor SSH (las otras máquinas entran a esta por SSH)"
if systemctl is-active --quiet ssh 2>/dev/null || systemctl is-active --quiet sshd 2>/dev/null; then
    ok "sshd corriendo"
else
    warn "sshd no está activo (sudo apt install openssh-server && sudo systemctl enable --now ssh). Necesario en las máquinas 2 y 3."
fi

echo "== Proyecto"
[ -x bin/seismic_cluster ] && ok "bin/seismic_cluster compilado" || bad "bin/seismic_cluster (correr 'make')"
[ -x bin/mpi_hello ]       && ok "bin/mpi_hello compilado"       || bad "bin/mpi_hello (correr 'make')"

if [ -f cluster.key ]; then
    LEN=$(tr -d '\n' < cluster.key | wc -c)
    if [ "$LEN" -eq 64 ]; then
        ok "cluster.key (huella: $(sha256sum cluster.key | cut -c1-12)  <- debe ser igual en las 3)"
    else
        bad "cluster.key tiene $LEN caracteres, deben ser 64"
    fi
else
    bad "cluster.key (en la máquina 1: 'make key'; en las demás: copiarla con scp)"
fi

if [ -f cluster/hostfile ]; then
    echo "== Revisión remota según cluster/hostfile"
    while read -r H; do
        if ssh -n -o BatchMode=yes -o ConnectTimeout=5 "$H" true 2>/dev/null; then
            ok "$H: SSH sin contraseña"
            REMOTE=$(ssh -n -o BatchMode=yes "$H" "cd '$REPO' 2>/dev/null && echo repo; test -x '$REPO/bin/seismic_cluster' && echo bin; test -f '$REPO/cluster.key' && sha256sum '$REPO/cluster.key' | cut -c1-12" 2>/dev/null)
            echo "$REMOTE" | grep -q '^repo$' && ok "$H: repo en $REPO" || bad "$H: no existe $REPO (clonar en la MISMA ruta)"
            echo "$REMOTE" | grep -q '^bin$'  && ok "$H: binario compilado" || bad "$H: falta 'make' en esa máquina"
            RKEY=$(echo "$REMOTE" | tail -1)
            LKEY=$(sha256sum cluster.key 2>/dev/null | cut -c1-12)
            if [ -n "$LKEY" ] && [ "$RKEY" = "$LKEY" ]; then ok "$H: misma llave"; else bad "$H: llave distinta o ausente"; fi
        else
            bad "$H: SSH sin contraseña no funciona (ssh-copy-id usuario@$H)"
        fi
    done < <(grep -vE '^\s*(#|$)' cluster/hostfile | awk '{print $1}' | sort -u)
else
    warn "no hay cluster/hostfile (solo hace falta en la máquina 1: cp cluster/hostfile.example cluster/hostfile)"
fi

echo
if [ "$FAILS" -eq 0 ]; then
    echo "Todo listo en esta máquina."
else
    echo "Hay $FAILS cosa(s) pendiente(s)."
fi