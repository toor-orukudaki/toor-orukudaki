#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lsb.h"
#include "payload.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "ECHEC ligne %d : %s\n", __LINE__, #c); return 1; } } while (0)

int main(void) {
    Bmp a, o;
    uint8_t k1[LSB_KEY_LEN], k2[LSB_KEY_LEN], *out = NULL;
    size_t n = 0, total = 100 * 100 * 3;
    CHECK(lsb_derive_key("alpha", k1) == LSB_OK && lsb_derive_key("beta", k2) == LSB_OK);
    CHECK(bmp_create(&a, 100, 100) == BMP_OK && bmp_create(&o, 100, 100) == BMP_OK);
    for (size_t i = 0; i < total; i++) a.data[i] = (uint8_t)(i * 13 + 5);
    memcpy(o.data, a.data, total);

    const char *m = "Secret disperse";
    size_t ml = strlen(m);
    CHECK(lsb_embed_keyed(&a, k1, (const uint8_t *)m, ml) == LSB_OK);
    CHECK(lsb_extract_keyed(&a, k1, &out, &n) == LSB_OK && n == ml && !memcmp(out, m, ml));
    free(out);

    /* Mauvaise clé : échec, ou contenu différent */
    LsbStatus s = lsb_extract_keyed(&a, k2, &out, &n);
    if (s == LSB_OK) { CHECK(!(n == ml && !memcmp(out, m, ml))); free(out); }

    /* Dispersion : des canaux modifiés bien au-delà des (4+len)*8 premiers, et seul le LSB change */
    size_t late = 0;
    for (size_t i = 0; i < total; i++) {
        CHECK((a.data[i] >> 1) == (o.data[i] >> 1));
        if (i >= (4 + ml) * 8 && ((a.data[i] ^ o.data[i]) & 1)) late++;
    }
    CHECK(late > 20);

    /* Capacité dépassée (4x4 : 2 octets) */
    Bmp s4;
    uint8_t big[3] = {1, 2, 3};
    CHECK(bmp_create(&s4, 4, 4) == BMP_OK);
    CHECK(lsb_embed_keyed(&s4, k1, big, 3) == LSB_ERR_CAPACITY);

    /* Charge utile */
    uint8_t *pl = NULL;
    size_t pll = 0, dl = 0;
    char name[PL_MAX_NAME + 1];
    const uint8_t *d = NULL;
    CHECK(pl_pack("a.txt", (const uint8_t *)"xyz", 3, &pl, &pll) == 0);
    CHECK(pl_unpack(pl, pll, name, &d, &dl) == 0 && !strcmp(name, "a.txt") && dl == 3 && !memcmp(d, "xyz", 3));
    free(pl);
    CHECK(pl_pack("../x", (const uint8_t *)"y", 1, &pl, &pll) == 0);
    CHECK(pl_unpack(pl, pll, name, &d, &dl) == -1);
    free(pl);

    bmp_free(&a); bmp_free(&o); bmp_free(&s4);
    printf("OK : tous les tests KEYED passent\n");
    return 0;
}