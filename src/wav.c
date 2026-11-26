#include "wav.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE (64u * 1024u * 1024u)

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void wr16(uint8_t *p, uint16_t v) { p[0] = v & 255; p[1] = v >> 8; }
static void wr32(uint8_t *p, uint32_t v) {
    p[0] = v & 255; p[1] = (v >> 8) & 255; p[2] = (v >> 16) & 255; p[3] = v >> 24;
}

void wav_free(Wav *w) {
    if (!w) return;
    free(w->s);
    w->s = NULL;
    w->n = 0;
}

BmpStatus wav_load(const char *path, Wav *w) {
    if (!path || !w) return BMP_ERR_ARG;
    memset(w, 0, sizeof *w);
    uint8_t *buf = NULL;
    const uint8_t *data = NULL;
    size_t size, pos = 12, dlen = 0;
    int have_fmt = 0;
    BmpStatus st = BMP_OK;

    FILE *f = fopen(path, "rb");
    if (!f) return BMP_ERR_IO;
    long sz = (fseek(f, 0, SEEK_END) == 0) ? ftell(f) : -1;
    if (sz < 44 || (unsigned long)sz > MAX_FILE) { st = BMP_ERR_FORMAT; goto end; }
    size = (size_t)sz;
    rewind(f);
    buf = malloc(size);
    if (!buf) { st = BMP_ERR_MEMORY; goto end; }
    if (fread(buf, 1, size, f) != size) { st = BMP_ERR_IO; goto end; }
    if (memcmp(buf, "RIFF", 4) || memcmp(buf + 8, "WAVE", 4)) { st = BMP_ERR_FORMAT; goto end; }

    while (pos + 8 <= size) {
        uint32_t cl = rd32(buf + pos + 4);
        const uint8_t *c = buf + pos + 8;
        size_t avail = size - pos - 8;
        if (!memcmp(buf + pos, "fmt ", 4)) {
            if (cl < 16 || avail < 16) { st = BMP_ERR_FORMAT; goto end; }
            if (rd16(c) != 1 || rd16(c + 14) != 16) { st = BMP_ERR_UNSUPPORTED; goto end; }
            w->channels = rd16(c + 2);
            w->rate = rd32(c + 4);
            have_fmt = 1;
        } else if (!memcmp(buf + pos, "data", 4)) {
            dlen = cl > avail ? avail : cl;
            data = c;
            break;
        }
        if (cl > avail) break;
        pos += 8 + (size_t)cl + (cl & 1);
    }
    if (!have_fmt || !data || w->channels == 0 || w->channels > 8) { st = BMP_ERR_FORMAT; goto end; }
    w->n = dlen / 2 / w->channels * w->channels;
    if (w->n == 0) { st = BMP_ERR_FORMAT; goto end; }
    w->s = malloc(w->n * sizeof *w->s);
    if (!w->s) { st = BMP_ERR_MEMORY; goto end; }
    for (size_t i = 0; i < w->n; i++) w->s[i] = (int16_t)rd16(data + 2 * i);
end:
    free(buf);
    fclose(f);
    if (st != BMP_OK) wav_free(w);
    return st;
}

BmpStatus wav_save(const char *path, const Wav *w) {
    if (!path || !w || !w->s || !w->n || !w->channels) return BMP_ERR_ARG;
    uint64_t dl = (uint64_t)w->n * 2;
    if (dl > UINT32_MAX - 36) return BMP_ERR_ARG;
    uint8_t h[44];
    memcpy(h, "RIFF", 4);
    wr32(h + 4, (uint32_t)(36 + dl));
    memcpy(h + 8, "WAVEfmt ", 8);
    wr32(h + 16, 16);
    wr16(h + 20, 1);
    wr16(h + 22, w->channels);
    wr32(h + 24, w->rate);
    wr32(h + 28, w->rate * w->channels * 2);
    wr16(h + 32, (uint16_t)(w->channels * 2));
    wr16(h + 34, 16);
    memcpy(h + 36, "data", 4);
    wr32(h + 40, (uint32_t)dl);

    uint8_t *b = malloc((size_t)dl);
    if (!b) return BMP_ERR_MEMORY;
    for (size_t i = 0; i < w->n; i++) wr16(b + 2 * i, (uint16_t)w->s[i]);
    FILE *f = fopen(path, "wb");
    if (!f) { free(b); return BMP_ERR_IO; }
    BmpStatus st = BMP_OK;
    if (fwrite(h, 1, 44, f) != 44 || fwrite(b, 1, (size_t)dl, f) != (size_t)dl) st = BMP_ERR_IO;
    if (fclose(f) != 0 && st == BMP_OK) st = BMP_ERR_IO;
    free(b);
    return st;
}

/* Capacité identique à lsb_capacity() sur n/3*3 canaux. */
size_t wav_capacity(const Wav *w) {
    if (!w || !w->s) return 0;
    size_t t = (w->n / 3 * 3) / 8;
    return t > 4 ? t - 4 : 0;
}

/* Réutilise le code LSB des images : les octets de poids faible des échantillons
   sont vus comme une "image" de (n/3) x 1 pixels. */
LsbStatus wav_embed_keyed(Wav *w, const uint8_t key[LSB_KEY_LEN], const uint8_t *msg, size_t len) {
    if (!w || !w->s) return LSB_ERR_ARG;
    size_t m = w->n / 3 * 3;
    uint8_t *t = malloc(m ? m : 1);
    if (!t) return LSB_ERR_MEMORY;
    for (size_t i = 0; i < m; i++) t[i] = (uint8_t)((uint16_t)w->s[i] & 0xFF);
    Bmp v = {(uint32_t)(m / 3), 1, t};
    LsbStatus st = lsb_embed_keyed(&v, key, msg, len);
    if (st == LSB_OK)
        for (size_t i = 0; i < m; i++) {
            uint16_t u = (uint16_t)w->s[i];
            u = (uint16_t)((u & 0xFFFEu) | (t[i] & 1u));
            w->s[i] = (int16_t)u;
        }
    free(t);
    return st;
}

LsbStatus wav_extract_keyed(const Wav *w, const uint8_t key[LSB_KEY_LEN],
                            uint8_t **msg, size_t *len) {
    if (!w || !w->s) return LSB_ERR_ARG;
    size_t m = w->n / 3 * 3;
    uint8_t *t = malloc(m ? m : 1);
    if (!t) return LSB_ERR_MEMORY;
    for (size_t i = 0; i < m; i++) t[i] = (uint8_t)((uint16_t)w->s[i] & 0xFF);
    Bmp v = {(uint32_t)(m / 3), 1, t};
    LsbStatus st = lsb_extract_keyed(&v, key, msg, len);
    free(t);
    return st;
}