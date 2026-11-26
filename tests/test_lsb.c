#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lsb.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "ECHEC ligne %d : %s\n", __LINE__, #c); return 1; } } while (0)

int main(void) {
    Bmp a, orig, s, p;
    size_t total = 64 * 64 * 3;
    CHECK(bmp_create(&a, 64, 64) == BMP_OK);
    CHECK(bmp_create(&orig, 64, 64) == BMP_OK);
    for (size_t i = 0; i < total; i++) a.data[i] = (uint8_t)(i * 7 + 3);
    memcpy(orig.data, a.data, total);

    const char *m = "Bonjour STEGO";
    CHECK(lsb_capacity(&a) == total / 8 - 4);
    CHECK(lsb_embed(&a, (const uint8_t *)m, strlen(m)) == LSB_OK);

    uint8_t *out = NULL;
    size_t n = 0;
    CHECK(lsb_extract(&a, &out, &n) == LSB_OK);
    CHECK(n == strlen(m) && memcmp(out, m, n) == 0);
    free(out);

    /* Chaque canal change d'au plus 1 : l'image reste visuellement identique */
    for (size_t i = 0; i < total; i++) {
        int d = (int)a.data[i] - (int)orig.data[i];
        CHECK(d >= -1 && d <= 1);
    }

    /* Message trop grand (image 4x4 : capacité 2 octets) */
    uint8_t big[3] = {1, 2, 3};
    CHECK(bmp_create(&s, 4, 4) == BMP_OK);
    CHECK(lsb_capacity(&s) == 2);
    CHECK(lsb_embed(&s, big, 3) == LSB_ERR_CAPACITY);

    /* Image sans message : taille lue = 0xFFFFFFFF -> refusée */
    memset(s.data, 0xFF, 48);
    CHECK(lsb_extract(&s, &out, &n) == LSB_ERR_NODATA);

    /* Plan de bits */
    CHECK(lsb_bitplane(&a, 0, &p) == LSB_OK);
    CHECK(p.data[0] == 0 || p.data[0] == 255);
    CHECK(lsb_bitplane(&a, 8, &p) == LSB_ERR_ARG);

    bmp_free(&a); bmp_free(&orig); bmp_free(&s); bmp_free(&p);
    printf("OK : tous les tests LSB passent\n");
    return 0;
}