#include "lsb.h"
#include <stdlib.h>

#define HDR_BYTES 4 /* taille du message, 4 octets little-endian */

static size_t total_bytes(const Bmp *img) {
    return (size_t)img->width * img->height * 3 / 8;
}

size_t lsb_capacity(const Bmp *img) {
    if (!img || !img->data) return 0;
    size_t t = total_bytes(img);
    return t > HDR_BYTES ? t - HDR_BYTES : 0;
}

/* Écrit 1 octet dans les LSB de 8 canaux consécutifs (bit de poids fort d'abord). */
static void put_byte(Bmp *img, size_t idx, uint8_t v) {
    for (int i = 0; i < 8; i++) {
        uint8_t *c = &img->data[idx * 8 + i];
        *c = (uint8_t)((*c & 0xFE) | ((v >> (7 - i)) & 1));
    }
}

static uint8_t get_byte(const Bmp *img, size_t idx) {
    uint8_t v = 0;
    for (int i = 0; i < 8; i++)
        v = (uint8_t)((v << 1) | (img->data[idx * 8 + i] & 1));
    return v;
}

const char *lsb_strerror(LsbStatus s) {
    switch (s) {
    case LSB_OK:           return "ok";
    case LSB_ERR_CAPACITY: return "message trop grand pour cette image";
    case LSB_ERR_NODATA:   return "aucun message valide trouve";
    case LSB_ERR_MEMORY:   return "memoire insuffisante";
    default:               return "argument invalide";
    }
}

LsbStatus lsb_embed(Bmp *img, const uint8_t *msg, size_t len) {
    if (!img || !img->data || (!msg && len)) return LSB_ERR_ARG;
    if (len > lsb_capacity(img)) return LSB_ERR_CAPACITY;
    for (int i = 0; i < HDR_BYTES; i++)
        put_byte(img, (size_t)i, (uint8_t)((len >> (8 * i)) & 0xFF));
    for (size_t i = 0; i < len; i++)
        put_byte(img, HDR_BYTES + i, msg[i]);
    return LSB_OK;
}

LsbStatus lsb_extract(const Bmp *img, uint8_t **msg, size_t *len) {
    if (!img || !img->data || !msg || !len) return LSB_ERR_ARG;
    *msg = NULL;
    *len = 0;
    if (total_bytes(img) < HDR_BYTES) return LSB_ERR_NODATA;
    size_t n = 0;
    for (int i = 0; i < HDR_BYTES; i++)
        n |= (size_t)get_byte(img, (size_t)i) << (8 * i);
    if (n > lsb_capacity(img)) return LSB_ERR_NODATA; /* taille incohérente */
    uint8_t *buf = malloc(n + 1);
    if (!buf) return LSB_ERR_MEMORY;
    for (size_t i = 0; i < n; i++) buf[i] = get_byte(img, HDR_BYTES + i);
    buf[n] = 0;
    *msg = buf;
    *len = n;
    return LSB_OK;
}

LsbStatus lsb_bitplane(const Bmp *img, unsigned bit, Bmp *out) {
    if (!img || !img->data || !out || bit > 7) return LSB_ERR_ARG;
    if (bmp_create(out, img->width, img->height) != BMP_OK) return LSB_ERR_MEMORY;
    size_t n = (size_t)img->width * img->height * 3;
    for (size_t i = 0; i < n; i++)
        out->data[i] = ((img->data[i] >> bit) & 1) ? 255 : 0;
    return LSB_OK;
}
#include <sodium.h>
#include <string.h>

/* Permutation pseudo-aléatoire des n canaux (Fisher-Yates partiel) : seuls
   les m premiers indices sont utiles. Libérer avec free(). */
static uint32_t *positions(size_t n, size_t m, const uint8_t *key) {
    uint32_t *perm = malloc(n * sizeof *perm);
    uint8_t *ks = malloc(m * 4);
    if (!perm || !ks) { free(perm); free(ks); return NULL; }
    for (size_t i = 0; i < n; i++) perm[i] = (uint32_t)i;
    randombytes_buf_deterministic(ks, m * 4, key);
    for (size_t i = 0; i < m; i++) {
        uint32_t r = (uint32_t)ks[4*i] | (uint32_t)ks[4*i+1] << 8 |
                     (uint32_t)ks[4*i+2] << 16 | (uint32_t)ks[4*i+3] << 24;
        size_t j = i + (size_t)(((uint64_t)r * (n - i)) >> 32);
        uint32_t t = perm[i]; perm[i] = perm[j]; perm[j] = t;
    }
    sodium_memzero(ks, m * 4);
    free(ks);
    return perm;
}

static void put_at(Bmp *img, const uint32_t *pos, size_t idx, uint8_t v) {
    for (int i = 0; i < 8; i++) {
        uint8_t *c = &img->data[pos[idx * 8 + i]];
        *c = (uint8_t)((*c & 0xFE) | ((v >> (7 - i)) & 1));
    }
}

static uint8_t get_at(const Bmp *img, const uint32_t *pos, size_t idx) {
    uint8_t v = 0;
    for (int i = 0; i < 8; i++)
        v = (uint8_t)((v << 1) | (img->data[pos[idx * 8 + i]] & 1));
    return v;
}

LsbStatus lsb_derive_key(const char *pw, uint8_t key[LSB_KEY_LEN]) {
    if (!pw || !key || sodium_init() < 0) return LSB_ERR_ARG;
    static const char label[] = "STEGO-POSITIONS-v1"; /* sel fixe : les positions doivent être relisibles */
    uint8_t salt[crypto_pwhash_SALTBYTES];
    crypto_generichash(salt, sizeof salt, (const uint8_t *)label, strlen(label), NULL, 0);
    if (crypto_pwhash(key, LSB_KEY_LEN, pw, strlen(pw), salt,
                      crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
                      crypto_pwhash_ALG_ARGON2ID13) != 0)
        return LSB_ERR_MEMORY;
    return LSB_OK;
}

LsbStatus lsb_embed_keyed(Bmp *img, const uint8_t key[LSB_KEY_LEN],
                          const uint8_t *msg, size_t len) {
    if (!img || !img->data || !key || (!msg && len)) return LSB_ERR_ARG;
    if (len > lsb_capacity(img)) return LSB_ERR_CAPACITY;
    size_t n = (size_t)img->width * img->height * 3;
    uint32_t *pos = positions(n, (HDR_BYTES + len) * 8, key);
    if (!pos) return LSB_ERR_MEMORY;
    for (int i = 0; i < HDR_BYTES; i++)
        put_at(img, pos, (size_t)i, (uint8_t)((len >> (8 * i)) & 0xFF));
    for (size_t i = 0; i < len; i++) put_at(img, pos, HDR_BYTES + i, msg[i]);
    free(pos);
    return LSB_OK;
}

LsbStatus lsb_extract_keyed(const Bmp *img, const uint8_t key[LSB_KEY_LEN],
                            uint8_t **msg, size_t *len) {
    if (!img || !img->data || !key || !msg || !len) return LSB_ERR_ARG;
    *msg = NULL;
    *len = 0;
    if (total_bytes(img) < HDR_BYTES) return LSB_ERR_NODATA;
    size_t n = (size_t)img->width * img->height * 3, sz = 0;
    uint32_t *pos = positions(n, HDR_BYTES * 8, key);
    if (!pos) return LSB_ERR_MEMORY;
    for (int i = 0; i < HDR_BYTES; i++) sz |= (size_t)get_at(img, pos, (size_t)i) << (8 * i);
    free(pos);
    if (sz > lsb_capacity(img)) return LSB_ERR_NODATA; /* mauvaise clé ou pas de message */
    pos = positions(n, (HDR_BYTES + sz) * 8, key);
    uint8_t *buf = malloc(sz + 1);
    if (!pos || !buf) { free(pos); free(buf); return LSB_ERR_MEMORY; }
    for (size_t i = 0; i < sz; i++) buf[i] = get_at(img, pos, HDR_BYTES + i);
    buf[sz] = 0;
    free(pos);
    *msg = buf;
    *len = sz;
    return LSB_OK;
}