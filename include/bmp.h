#ifndef BMP_H
#define BMP_H
#include <stdint.h>
#include <stddef.h>

typedef enum {
    BMP_OK = 0, BMP_ERR_IO, BMP_ERR_FORMAT,
    BMP_ERR_UNSUPPORTED, BMP_ERR_MEMORY, BMP_ERR_ARG
} BmpStatus;

/* Image en mémoire : RGB, 3 octets/pixel, ligne du haut en premier. */
typedef struct {
    uint32_t width, height;
    uint8_t *data;
} Bmp;

BmpStatus   bmp_create(Bmp *img, uint32_t w, uint32_t h);
BmpStatus   bmp_load(const char *path, Bmp *img);
BmpStatus   bmp_save(const char *path, const Bmp *img);
void        bmp_free(Bmp *img);
uint8_t    *bmp_pixel(const Bmp *img, uint32_t x, uint32_t y); /* -> [R,G,B] */
const char *bmp_strerror(BmpStatus s);

#endif