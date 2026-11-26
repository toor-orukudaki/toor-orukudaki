#include "stegocrypt.h"
#include <sodium.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC     "STG1"
#define VERSION   1
#define SALT_LEN  crypto_pwhash_SALTBYTES                        /* 16 */
#define NONCE_LEN crypto_aead_xchacha20poly1305_ietf_NPUBBYTES   /* 24 */
#define KEY_LEN   crypto_aead_xchacha20poly1305_ietf_KEYBYTES    /* 32 */
#define TAG_LEN   crypto_aead_xchacha20poly1305_ietf_ABYTES      /* 16 */
#define AAD_LEN   5                                              /* magic + version */
#define MAX_PLAIN (64u * 1024u * 1024u)

static void wr32(uint8_t *p, uint32_t v) {
    p[0] = v & 255; p[1] = (v >> 8) & 255; p[2] = (v >> 16) & 255; p[3] = v >> 24;
}
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int derive_key(uint8_t *key, const char *pw, const uint8_t *salt) {
    return crypto_pwhash(key, KEY_LEN, pw, strlen(pw), salt,
                         crypto_pwhash_OPSLIMIT_INTERACTIVE,
                         crypto_pwhash_MEMLIMIT_INTERACTIVE,
                         crypto_pwhash_ALG_ARGON2ID13);
}

const char *sc_strerror(ScStatus s) {
    switch (s) {
    case SC_OK:         return "ok";
    case SC_ERR_INIT:   return "initialisation libsodium impossible";
    case SC_ERR_MEMORY: return "memoire insuffisante";
    case SC_ERR_KDF:    return "derivation de cle impossible (memoire ?)";
    case SC_ERR_FORMAT: return "aucun message STEGO valide dans cette image";
    case SC_ERR_AUTH:   return "mot de passe incorrect ou donnees alterees";
    default:            return "argument invalide";
    }
}

ScStatus sc_seal(const char *pw, const uint8_t *msg, size_t len,
                 uint8_t **out, size_t *out_len) {
    if (!pw || !out || !out_len || (!msg && len) || len > MAX_PLAIN) return SC_ERR_ARG;
    *out = NULL;
    *out_len = 0;
    if (sodium_init() < 0) return SC_ERR_INIT;

    size_t clen = len + TAG_LEN, total = SC_HEADER_SIZE + clen;
    uint8_t *buf = malloc(total);
    if (!buf) return SC_ERR_MEMORY;

    uint8_t *salt = buf + AAD_LEN, *nonce = salt + SALT_LEN;
    memcpy(buf, MAGIC, 4);
    buf[4] = VERSION;
    randombytes_buf(salt, SALT_LEN);
    randombytes_buf(nonce, NONCE_LEN);
    wr32(nonce + NONCE_LEN, (uint32_t)clen);

    uint8_t key[KEY_LEN];
    if (derive_key(key, pw, salt) != 0) { free(buf); return SC_ERR_KDF; }

    static const uint8_t empty = 0;
    unsigned long long outl = 0;
    crypto_aead_xchacha20poly1305_ietf_encrypt(buf + SC_HEADER_SIZE, &outl,
        len ? msg : &empty, len, buf, AAD_LEN, NULL, nonce, key);
    sodium_memzero(key, sizeof key);

    *out = buf;
    *out_len = total;
    return SC_OK;
}

ScStatus sc_open(const char *pw, const uint8_t *in, size_t in_len,
                 uint8_t **msg, size_t *len) {
    if (!pw || !in || !msg || !len) return SC_ERR_ARG;
    *msg = NULL;
    *len = 0;
    if (in_len < SC_OVERHEAD || memcmp(in, MAGIC, 4) != 0 || in[4] != VERSION)
        return SC_ERR_FORMAT;
    uint32_t clen = rd32(in + SC_HEADER_SIZE - 4);
    if (clen < TAG_LEN || (size_t)clen != in_len - SC_HEADER_SIZE) return SC_ERR_FORMAT;
    if (sodium_init() < 0) return SC_ERR_INIT;

    const uint8_t *salt = in + AAD_LEN, *nonce = salt + SALT_LEN;
    uint8_t key[KEY_LEN];
    if (derive_key(key, pw, salt) != 0) return SC_ERR_KDF;

    size_t plen = clen - TAG_LEN;
    uint8_t *buf = malloc(plen + 1);
    if (!buf) { sodium_memzero(key, sizeof key); return SC_ERR_MEMORY; }

    unsigned long long outl = 0;
    int r = crypto_aead_xchacha20poly1305_ietf_decrypt(buf, &outl, NULL,
        in + SC_HEADER_SIZE, clen, in, AAD_LEN, nonce, key);
    sodium_memzero(key, sizeof key);
    if (r != 0) { sodium_memzero(buf, plen + 1); free(buf); return SC_ERR_AUTH; }

    buf[plen] = 0;
    *msg = buf;
    *len = plen;
    return SC_OK;
}