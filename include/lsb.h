#ifndef LSB_H
#define LSB_H
#include "bmp.h"

#define LSB_KEY_LEN 32

typedef enum {
    LSB_OK = 0, LSB_ERR_ARG, LSB_ERR_CAPACITY, LSB_ERR_NODATA, LSB_ERR_MEMORY
} LsbStatus;

/* Octets de message utilisables (l'en-tête de 4 octets est déjà retiré). */
size_t      lsb_capacity(const Bmp *img);
LsbStatus   lsb_embed(Bmp *img, const uint8_t *msg, size_t len);   /* séquentiel */
/* *msg est alloué (len+1, terminé par 0) : à libérer avec free(). */
LsbStatus   lsb_extract(const Bmp *img, uint8_t **msg, size_t *len);
LsbStatus   lsb_bitplane(const Bmp *img, unsigned bit, Bmp *out);
/* Version à positions pseudo-aléatoires : la clé décide quels canaux sont utilisés. */
LsbStatus   lsb_derive_key(const char *password, uint8_t key[LSB_KEY_LEN]);
LsbStatus   lsb_embed_keyed(Bmp *img, const uint8_t key[LSB_KEY_LEN],
                            const uint8_t *msg, size_t len);
LsbStatus   lsb_extract_keyed(const Bmp *img, const uint8_t key[LSB_KEY_LEN],
                              uint8_t **msg, size_t *len);
const char *lsb_strerror(LsbStatus s);

#endif