#include "bmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HDR_SIZE 54
#define MAX_DIM  16384

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void wr16(uint8_t *p, uint16_t v) { p[0] = v & 255; p[1] = v >> 8; }
static void wr32(uint8_t *p, uint32_t v) {
    p[0] = v & 255; p[1] = (v >> 8) & 255; p[2] = (v >> 16) & 255; p[3] = v >> 24;
}
/* Chaque ligne BMP est alignée sur 4 octets. */
static size_t row_size(uint32_t w) { return ((size_t)w * 3 + 3) / 4 * 4; }

const char *bmp_strerror(BmpStatus s) {
    switch (s) {
    case BMP_OK:              return "ok";
    case BMP_ERR_IO:          return "erreur d'entrée/sortie";
    case BMP_ERR_FORMAT:      return "fichier BMP invalide";
    case BMP_ERR_UNSUPPORTED: return "BMP non supporté (24 bits non compressé requis)";
    case BMP_ERR_MEMORY:      return "mémoire insuffisante";
    default:                  return "argument invalide";
    }
}

BmpStatus bmp_create(Bmp *img, uint32_t w, uint32_t h) {
    if (!img) return BMP_ERR_ARG;
    img->width = img->height = 0;
    img->data = NULL;
    if (w == 0 || h == 0 || w > MAX_DIM || h > MAX_DIM) return BMP_ERR_ARG;
    img->data = calloc((size_t)w * h * 3, 1);
    if (!img->data) return BMP_ERR_MEMORY;
    img->width = w;
    img->height = h;
    return BMP_OK;
}

void bmp_free(Bmp *img) {
    if (!img) return;
    free(img->data);
    img->data = NULL;
    img->width = img->height = 0;
}

uint8_t *bmp_pixel(const Bmp *img, uint32_t x, uint32_t y) {
    if (!img || !img->data || x >= img->width || y >= img->height) return NULL;
    return img->data + ((size_t)y * img->width + x) * 3;
}

BmpStatus bmp_load(const char *path, Bmp *img) {
    if (!path || !img) return BMP_ERR_ARG;
    img->width = img->height = 0;
    img->data = NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return BMP_ERR_IO;

    uint8_t hdr[HDR_SIZE];
    BmpStatus st = BMP_OK;
    uint8_t *row = NULL;

    if (fread(hdr, 1, HDR_SIZE, f) != HDR_SIZE || hdr[0] != 'B' || hdr[1] != 'M') {
        st = BMP_ERR_FORMAT; goto end;
    }
    uint32_t offset = rd32(hdr + 10), dib = rd32(hdr + 14);
    int64_t  w = (int32_t)rd32(hdr + 18), h = (int32_t)rd32(hdr + 22);
    uint16_t bpp = rd16(hdr + 28);
    uint32_t comp = rd32(hdr + 30);

    if (dib < 40 || bpp != 24 || comp != 0) { st = BMP_ERR_UNSUPPORTED; goto end; }
    int top_down = (h < 0);
    if (top_down) h = -h;
    if (w <= 0 || h <= 0 || w > MAX_DIM || h > MAX_DIM || offset < HDR_SIZE) {
        st = BMP_ERR_FORMAT; goto end;
    }
    if (fseek(f, (long)offset, SEEK_SET) != 0) { st = BMP_ERR_IO; goto end; }

    st = bmp_create(img, (uint32_t)w, (uint32_t)h);
    if (st != BMP_OK) goto end;

    size_t rs = row_size(img->width);
    row = malloc(rs);
    if (!row) { st = BMP_ERR_MEMORY; goto end; }

    for (uint32_t r = 0; r < img->height; r++) {
        if (fread(row, 1, rs, f) != rs) { st = BMP_ERR_FORMAT; goto end; }
        uint32_t y = top_down ? r : img->height - 1 - r; /* BMP : bas -> haut */
        for (uint32_t x = 0; x < img->width; x++) {
            uint8_t *p = bmp_pixel(img, x, y);
            p[0] = row[x * 3 + 2]; /* R */
            p[1] = row[x * 3 + 1]; /* G */
            p[2] = row[x * 3];     /* B */
        }
    }
end:
    free(row);
    fclose(f);
    if (st != BMP_OK) bmp_free(img);
    return st;
}

BmpStatus bmp_save(const char *path, const Bmp *img) {
    if (!path || !img || !img->data || !img->width || !img->height) return BMP_ERR_ARG;
    size_t rs = row_size(img->width);
    uint64_t total = HDR_SIZE + (uint64_t)rs * img->height;
    if (total > UINT32_MAX) return BMP_ERR_ARG;

    uint8_t *row = calloc(rs, 1); /* le padding reste à 0 */
    if (!row) return BMP_ERR_MEMORY;
    FILE *f = fopen(path, "wb");
    if (!f) { free(row); return BMP_ERR_IO; }

    uint8_t hdr[HDR_SIZE];
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B'; hdr[1] = 'M';
    wr32(hdr + 2, (uint32_t)total);
    wr32(hdr + 10, HDR_SIZE);
    wr32(hdr + 14, 40);
    wr32(hdr + 18, img->width);
    wr32(hdr + 22, img->height);
    wr16(hdr + 26, 1);
    wr16(hdr + 28, 24);
    wr32(hdr + 34, (uint32_t)(rs * img->height));
    wr32(hdr + 38, 2835);
    wr32(hdr + 42, 2835);

    BmpStatus st = BMP_OK;
    if (fwrite(hdr, 1, HDR_SIZE, f) != HDR_SIZE) st = BMP_ERR_IO;
    for (uint32_t r = 0; r < img->height && st == BMP_OK; r++) {
        uint32_t y = img->height - 1 - r;
        for (uint32_t x = 0; x < img->width; x++) {
            const uint8_t *p = bmp_pixel(img, x, y);
            row[x * 3] = p[2]; row[x * 3 + 1] = p[1]; row[x * 3 + 2] = p[0];
        }
        if (fwrite(row, 1, rs, f) != rs) st = BMP_ERR_IO;
    }
    if (fclose(f) != 0 && st == BMP_OK) st = BMP_ERR_IO;
    free(row);
    return st;
}