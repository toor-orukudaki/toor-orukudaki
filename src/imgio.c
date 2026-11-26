#include "imgio.h"
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DIM 16384

static const uint8_t PNG_MAGIC[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

static int has_png_ext(const char *path) {
    size_t n = strlen(path);
    if (n < 4) return 0;
    const char *e = path + n - 4;
    return e[0] == '.' && (e[1] | 32) == 'p' && (e[2] | 32) == 'n' && (e[3] | 32) == 'g';
}

static BmpStatus png_load(const char *path, Bmp *img) {
    png_image im;
    memset(&im, 0, sizeof im);
    im.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&im, path)) {
        png_image_free(&im);
        return BMP_ERR_FORMAT;
    }
    if (im.width == 0 || im.height == 0 || im.width > MAX_DIM || im.height > MAX_DIM) {
        png_image_free(&im);
        return BMP_ERR_FORMAT;
    }
    im.format = PNG_FORMAT_RGB; /* force RGB 8 bits (alpha et 16 bits convertis) */
    BmpStatus st = bmp_create(img, im.width, im.height);
    if (st != BMP_OK) {
        png_image_free(&im);
        return st;
    }
    if (!png_image_finish_read(&im, NULL, img->data, 0, NULL)) {
        bmp_free(img);
        png_image_free(&im);
        return BMP_ERR_FORMAT;
    }
    png_image_free(&im);
    return BMP_OK;
}

static BmpStatus png_save(const char *path, const Bmp *img) {
    png_image im;
    memset(&im, 0, sizeof im);
    im.version = PNG_IMAGE_VERSION;
    im.width = img->width;
    im.height = img->height;
    im.format = PNG_FORMAT_RGB;
    int ok = png_image_write_to_file(&im, path, 0, img->data, 0, NULL);
    png_image_free(&im);
    return ok ? BMP_OK : BMP_ERR_IO;
}

BmpStatus img_load(const char *path, Bmp *img) {
    if (!path || !img) return BMP_ERR_ARG;
    uint8_t magic[8] = {0};
    FILE *f = fopen(path, "rb");
    if (!f) return BMP_ERR_IO;
    size_t got = fread(magic, 1, sizeof magic, f);
    fclose(f);
    if (got == sizeof magic && memcmp(magic, PNG_MAGIC, sizeof magic) == 0)
        return png_load(path, img);
    return bmp_load(path, img);
}

BmpStatus img_save(const char *path, const Bmp *img) {
    if (!path || !img || !img->data || !img->width || !img->height) return BMP_ERR_ARG;
    return has_png_ext(path) ? png_save(path, img) : bmp_save(path, img);
}