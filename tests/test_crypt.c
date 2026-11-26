#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stegocrypt.h"
#include "lsb.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "ECHEC ligne %d : %s\n", __LINE__, #c); return 1; } } while (0)

int main(void) {
    const char *m = "Message secret STEGO", *pw = "motdepasse";
    size_t ml = strlen(m), blen = 0, olen = 0;
    uint8_t *blob = NULL, *out = NULL;

    CHECK(sc_seal(pw, (const uint8_t *)m, ml, &blob, &blen) == SC_OK);
    CHECK(blen == ml + SC_OVERHEAD);
    for (size_t i = 0; i + ml <= blen; i++) CHECK(memcmp(blob + i, m, ml) != 0); /* pas en clair */

    CHECK(sc_open(pw, blob, blen, &out, &olen) == SC_OK);
    CHECK(olen == ml && memcmp(out, m, ml) == 0);
    free(out);

    CHECK(sc_open("mauvais", blob, blen, &out, &olen) == SC_ERR_AUTH);
    blob[blen - 1] ^= 1; /* un bit modifié */
    CHECK(sc_open(pw, blob, blen, &out, &olen) == SC_ERR_AUTH);
    blob[blen - 1] ^= 1;
    CHECK(sc_open(pw, blob, blen - 1, &out, &olen) == SC_ERR_FORMAT);

    /* Chaîne complète : chiffrer -> cacher en LSB -> extraire -> déchiffrer */
    Bmp img;
    uint8_t *ex = NULL;
    size_t exl = 0;
    CHECK(bmp_create(&img, 100, 100) == BMP_OK);
    CHECK(lsb_embed(&img, blob, blen) == LSB_OK);
    CHECK(lsb_extract(&img, &ex, &exl) == LSB_OK);
    CHECK(sc_open(pw, ex, exl, &out, &olen) == SC_OK);
    CHECK(olen == ml && memcmp(out, m, ml) == 0);

    free(out); free(ex); free(blob); bmp_free(&img);
    printf("OK : tous les tests CRYPT passent\n");
    return 0;
}