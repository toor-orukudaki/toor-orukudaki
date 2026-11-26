#ifndef WAV_H
#define WAV_H
#include <stddef.h>
#include <stdint.h>
#include "bmp.h"
#include "lsb.h"

/* Audio PCM 16 bits : s = échantillons entrelacés, n = total (tous canaux). */
typedef struct { uint16_t channels; uint32_t rate; size_t n; int16_t *s; } Wav;

BmpStatus wav_load(const char *path, Wav *w);
BmpStatus wav_save(const char *path, const Wav *w);
void      wav_free(Wav *w);
size_t    wav_capacity(const Wav *w);
/* Même principe que pour les images : 1 bit caché par échantillon (son LSB). */
LsbStatus wav_embed_keyed(Wav *w, const uint8_t key[LSB_KEY_LEN], const uint8_t *msg, size_t len);
LsbStatus wav_extract_keyed(const Wav *w, const uint8_t key[LSB_KEY_LEN],
                            uint8_t **msg, size_t *len);
#endif