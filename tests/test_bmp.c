#include <stdio.h>
#include <string.h>
#include "bmp.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "ECHEC ligne %d : %s\n", __LINE__, #c); return 1; } } while (0)

int main(void) {
    /* 101 px de large : 303 octets/ligne -> teste le padding */
    Bmp a, b;
    CHECK(bmp_create(&a, 101, 60) == BMP_OK);
    for (uint32_t y = 0; y < a.height; y++)
        for (uint32_t x = 0; x < a.width; x++) {
            uint8_t *p = bmp_pixel(&a, x, y);
            p[0] = (uint8_t)(x * 255 / (a.width - 1));
            p[1] = (uint8_t)(y * 255 / (a.height - 1));
            p[2] = (uint8_t)((x + y) & 255);
        }
    CHECK(bmp_save("build/sample.bmp", &a) == BMP_OK);
    CHECK(bmp_load("build/sample.bmp", &b) == BMP_OK);
    CHECK(a.width == b.width && a.height == b.height);
    CHECK(memcmp(a.data, b.data, (size_t)a.width * a.height * 3) == 0);
    CHECK(bmp_pixel(&b, 101, 0) == NULL); /* hors limites */
    bmp_free(&a);
    bmp_free(&b);

    /* Un fichier qui n'est pas un BMP doit être refusé */
    FILE *f = fopen("build/bad.bmp", "wb");
    CHECK(f != NULL);
    fputs("ceci n'est pas un BMP, juste du texte de remplissage pour depasser 54 octets", f);
    fclose(f);
    CHECK(bmp_load("build/bad.bmp", &b) == BMP_ERR_FORMAT);

    printf("OK : tous les tests BMP passent\n");
    return 0;
}