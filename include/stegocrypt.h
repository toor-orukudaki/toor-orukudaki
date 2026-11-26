#ifndef STEGOCRYPT_H
#define STEGOCRYPT_H
#include <stddef.h>
#include <stdint.h>

/* Conteneur : "STG1" | version(1) | sel(16) | nonce(24) | clen(4, LE) | chiffré+tag(clen) */
#define SC_HEADER_SIZE 49
#define SC_OVERHEAD    (SC_HEADER_SIZE + 16) /* en-tête + tag d'authentification */

typedef enum {
    SC_OK = 0, SC_ERR_ARG, SC_ERR_INIT, SC_ERR_MEMORY,
    SC_ERR_KDF, SC_ERR_FORMAT, SC_ERR_AUTH
} ScStatus;

/* *out est alloué : à libérer avec free(). */
ScStatus    sc_seal(const char *password, const uint8_t *msg, size_t len,
                    uint8_t **out, size_t *out_len);
/* *msg est alloué (len+1, terminé par 0) : à effacer (sodium_memzero) puis free(). */
ScStatus    sc_open(const char *password, const uint8_t *in, size_t in_len,
                    uint8_t **msg, size_t *len);
const char *sc_strerror(ScStatus s);

#endif