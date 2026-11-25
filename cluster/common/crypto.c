/*
 * crypto.c — Implementación de sec_send / sec_recv con AES-256-GCM.
 *
 * Variable de entorno útil para la demo:
 *   SEISMIC_DEBUG_CRYPTO=1  -> imprime los primeros bytes de cada mensaje
 *                              cifrado, como evidencia de que viaja cifrado.
 */
#include "crypto.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>

static unsigned char g_key[32];
static int g_ready = 0;
static int g_debug = 0;

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = tolower(c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

int sec_init(const char *key_path)
{
    if (!key_path) key_path = getenv("SEISMIC_KEY");
    if (!key_path) key_path = "cluster.key";

    FILE *f = fopen(key_path, "r");
    if (!f) {
        fprintf(stderr, "[crypto] no se pudo abrir la llave '%s' (¿corrió 'make key' y la copió a todos los nodos?)\n", key_path);
        return -1;
    }
    char hex[65] = {0};
    size_t n = fread(hex, 1, 64, f);
    fclose(f);
    if (n != 64) {
        fprintf(stderr, "[crypto] la llave debe tener 64 caracteres hex (32 bytes)\n");
        return -1;
    }
    for (int i = 0; i < 32; i++) {
        int hi = hexval(hex[2 * i]), lo = hexval(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            fprintf(stderr, "[crypto] la llave contiene caracteres no hexadecimales\n");
            OPENSSL_cleanse(hex, sizeof hex);
            return -1;
        }
        g_key[i] = (unsigned char)((hi << 4) | lo);
    }
    OPENSSL_cleanse(hex, sizeof hex);

    const char *dbg = getenv("SEISMIC_DEBUG_CRYPTO");
    g_debug = dbg && dbg[0] == '1';
    g_ready = 1;
    return 0;
}

void sec_cleanup(void)
{
    OPENSSL_cleanse(g_key, sizeof g_key);
    g_ready = 0;
}

/* AAD = origen | destino | tag (enteros de 32 bits) */
static void build_aad(unsigned char aad[12], int src, int dst, int tag)
{
    uint32_t v[3] = { (uint32_t)src, (uint32_t)dst, (uint32_t)tag };
    memcpy(aad, v, sizeof v);
}

static void debug_dump(const char *dir, int a, int b, int tag, const unsigned char *p, size_t n)
{
    if (!g_debug) return;
    fprintf(stderr, "[crypto] %s %d->%d tag=%d %zuB cifrado: ", dir, a, b, tag, n);
    for (size_t i = 0; i < n && i < 16; i++) fprintf(stderr, "%02x", p[i]);
    fprintf(stderr, "...\n");
}

int sec_send(const void *buf, size_t len, int dest, int tag)
{
    if (!g_ready) return -1;

    int me;
    MPI_Comm_rank(MPI_COMM_WORLD, &me);

    size_t total = SEC_IV_LEN + len + SEC_TAG_LEN;
    unsigned char *msg = malloc(total);
    if (!msg) return -1;

    unsigned char *iv = msg;
    unsigned char *ct = msg + SEC_IV_LEN;
    unsigned char *gtag = msg + SEC_IV_LEN + len;
    unsigned char aad[12];
    build_aad(aad, me, dest, tag);

    int ok = 0, outl = 0, fin = 0;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx
        && RAND_bytes(iv, SEC_IV_LEN) == 1                       /* IV aleatorio por mensaje */
        && EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, SEC_IV_LEN, NULL) == 1
        && EVP_EncryptInit_ex(ctx, NULL, NULL, g_key, iv) == 1
        && EVP_EncryptUpdate(ctx, NULL, &outl, aad, sizeof aad) == 1
        && EVP_EncryptUpdate(ctx, ct, &outl, buf, (int)len) == 1
        && EVP_EncryptFinal_ex(ctx, ct + outl, &fin) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, SEC_TAG_LEN, gtag) == 1)
        ok = 1;
    EVP_CIPHER_CTX_free(ctx);

    int rc = -1;
    if (ok) {
        debug_dump("TX", me, dest, tag, ct, len);
        rc = (MPI_Send(msg, (int)total, MPI_BYTE, dest, tag, MPI_COMM_WORLD) == MPI_SUCCESS) ? 0 : -1;
    } else {
        fprintf(stderr, "[crypto] error cifrando mensaje\n");
    }
    OPENSSL_cleanse(msg, total);
    free(msg);
    return rc;
}

long sec_recv(void *buf, size_t maxlen, int src, int tag, MPI_Status *st_out)
{
    if (!g_ready) return -1;

    int me;
    MPI_Comm_rank(MPI_COMM_WORLD, &me);

    /* Probe primero para conocer origen, tag y tamaño reales. */
    MPI_Status st;
    MPI_Probe(src, tag, MPI_COMM_WORLD, &st);
    int total = 0;
    MPI_Get_count(&st, MPI_BYTE, &total);

    unsigned char *msg = malloc(total > 0 ? (size_t)total : 1);
    if (!msg) return -1;
    MPI_Recv(msg, total, MPI_BYTE, st.MPI_SOURCE, st.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (st_out) *st_out = st;

    if (total < SEC_OVERHEAD || (size_t)(total - SEC_OVERHEAD) > maxlen) {
        fprintf(stderr, "[crypto] mensaje de tamaño inesperado (%d B)\n", total);
        free(msg);
        return -1;
    }
    size_t len = (size_t)total - SEC_OVERHEAD;
    unsigned char *iv = msg;
    unsigned char *ct = msg + SEC_IV_LEN;
    unsigned char *gtag = msg + SEC_IV_LEN + len;
    unsigned char aad[12];
    build_aad(aad, st.MPI_SOURCE, me, st.MPI_TAG);
    debug_dump("RX", st.MPI_SOURCE, me, st.MPI_TAG, ct, len);

    int ok = 0, outl = 0, fin = 0;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (ctx
        && EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, SEC_IV_LEN, NULL) == 1
        && EVP_DecryptInit_ex(ctx, NULL, NULL, g_key, iv) == 1
        && EVP_DecryptUpdate(ctx, NULL, &outl, aad, sizeof aad) == 1
        && EVP_DecryptUpdate(ctx, buf, &outl, ct, (int)len) == 1
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, SEC_TAG_LEN, gtag) == 1
        && EVP_DecryptFinal_ex(ctx, (unsigned char *)buf + outl, &fin) == 1)   /* verifica el tag */
        ok = 1;
    EVP_CIPHER_CTX_free(ctx);
    free(msg);

    if (!ok) {
        fprintf(stderr, "[crypto] autenticación fallida (mensaje alterado o llave distinta)\n");
        memset(buf, 0, len);
        return -1;
    }
    return (long)len;
}
