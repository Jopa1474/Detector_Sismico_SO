/*
 * crypto.h — Envío/recepción MPI cifrada con AES-256-GCM (OpenSSL).
 *
 * Formato de cada mensaje en la red:  [ IV (12 B) | texto cifrado (len B) | tag GCM (16 B) ]
 * Como AAD (datos autenticados, no cifrados) se usan origen, destino y tag MPI,
 * así un mensaje no puede redirigirse a otro nodo sin que falle la verificación.
 */
#ifndef SEISMIC_CRYPTO_H
#define SEISMIC_CRYPTO_H

#include <stddef.h>
#include <mpi.h>

#define SEC_IV_LEN   12
#define SEC_TAG_LEN  16
#define SEC_OVERHEAD (SEC_IV_LEN + SEC_TAG_LEN)

/* Carga la llave compartida (64 caracteres hex = 32 bytes).
 * Si key_path es NULL se usa $SEISMIC_KEY o "cluster.key". Devuelve 0 si ok. */
int  sec_init(const char *key_path);

/* Borra la llave de memoria. */
void sec_cleanup(void);

/* Cifra buf y lo envía con MPI_Send. Devuelve 0 si ok, -1 si error. */
int  sec_send(const void *buf, size_t len, int dest, int tag);

/* Recibe un mensaje (src/tag pueden ser MPI_ANY_SOURCE / MPI_ANY_TAG),
 * lo verifica y descifra en buf. st (opcional) recibe origen y tag reales.
 * Devuelve la cantidad de bytes descifrados, o -1 si falla la autenticación
 * o el mensaje no cabe en maxlen. */
long sec_recv(void *buf, size_t maxlen, int src, int tag, MPI_Status *st);

#endif /* SEISMIC_CRYPTO_H */
