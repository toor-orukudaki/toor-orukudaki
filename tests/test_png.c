#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imgio.h"
#include "lsb.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "ECHEC ligne %d : %s\n", __LINE__, #c); return 1; } } while (0)

int main(void) {
    Bmp a, b;
    size_t total = 50 * 40 * 3;
    CHECK(bmp_create(&a, 50, 40) == BMP_OK);
    for (size_t i = 0; i < total; i++) a.data[i] = (uint8_t)(i * 11 + 7);

    /* Aller-retour PNG : les pixels doivent être identiques */
    CHECK(img_save("build/t.png", &a) == BMP_OK);
    CHECK(img_load("build/t.png", &b) == BMP_OK);
    CHECK(b.width == 50 && b.height == 40 && memcmp(a.data, b.data, total) == 0);
    bmp_free(&b);

    /* Chaîne complète : cacher -> PNG -> recharger -> extraire */
    uint8_t key[LSB_KEY_LEN], *out = NULL;
    size_t n = 0;
    const char *m = "Secret dans un PNG";
    CHECK(lsb_derive_key("pw", key) == LSB_OK);
    CHECK(lsb_embed_keyed(&a, key, (const uint8_t *)m, strlen(m)) == LSB_OK);
    CHECK(img_save("build/t2.png", &a) == BMP_OK);
    CHECK(img_load("build/t2.png", &b) == BMP_OK);
    CHECK(lsb_extract_keyed(&b, key, &out, &n) == LSB_OK);
    CHECK(n == strlen(m) && memcmp(out, m, n) == 0);
    free(out);
    bmp_free(&b);

    /* Le BMP passe toujours par la même fonction */
    CHECK(img_save("build/t3.bmp", &a) == BMP_OK);
    CHECK(img_load("build/t3.bmp", &b) == BMP_OK && memcmp(a.data, b.data, total) == 0);
    bmp_free(&b);

    /* Fichiers invalides : refusés sans crash */
    FILE *f = fopen("build/bad.png", "wb");
    CHECK(f != NULL);
    fputs("pas une image du tout, juste du texte de remplissage pour depasser 54 octets..", f);
    fclose(f);
    CHECK(img_load("build/bad.png", &b) != BMP_OK);
    static const uint8_t magic_only[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    f = fopen("build/trunc.png", "wb");
    CHECK(f != NULL);
    fwrite(magic_only, 1, 8, f);
    fclose(f);
    CHECK(img_load("build/trunc.png", &b) != BMP_OK);
    CHECK(img_load("build/inexistant.png", &b) == BMP_ERR_IO);

    bmp_free(&a);
    printf("OK : tous les tests PNG passent\n");
    return 0;
}