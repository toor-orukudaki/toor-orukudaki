#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "detect.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "ECHEC ligne %d : %s\n", __LINE__, #c); return 1; } } while (0)

static uint32_t st = 12345;
static uint32_t rnd(void) { st = st * 1664525u + 1013904223u; return st >> 8; }

/* Support "propre" : valeurs presque toujours paires (histogramme de paires très inégal). */
static void make_cover(Bmp *b, uint32_t w, uint32_t h) {
    bmp_create(b, w, h);
    for (size_t i = 0; i < (size_t)w * h * 3; i++)
        b->data[i] = (uint8_t)(2 * (rnd() % 128) + (rnd() % 10 == 0));
}

int main(void) {
    /* 1. Valeurs de référence du chi-carré */
    uint32_t hh[4] = {10, 10, 10, 10};
    int df = 0;
    CHECK(det_chi2_p(hh, 4, &df) > 0.99 && df == 1);   /* paires parfaitement égales */
    uint32_t hs[4] = {100, 0, 100, 0};
    CHECK(det_chi2_p(hs, 4, &df) < 1e-6);

    Bmp a, b;
    DetReport r;
    uint8_t key[LSB_KEY_LEN];
    CHECK(lsb_derive_key("pw", key) == LSB_OK);

    /* 2. Support propre : pas suspect */
    make_cover(&a, 150, 150);
    CHECK(det_image(&a, &r) == 0);
    CHECK(r.p_all < 0.05 && r.level == 0 && !r.sequential);

    /* 3. Message séquentiel à 40 % de la capacité : signature détectée */
    size_t cap = lsb_capacity(&a), len = cap * 4 / 10;
    uint8_t *msg = malloc(cap);
    CHECK(msg != NULL);
    for (size_t i = 0; i < cap; i++) msg[i] = (uint8_t)(rnd() >> 3);
    CHECK(lsb_embed(&a, msg, len) == LSB_OK);
    CHECK(det_image(&a, &r) == 0);
    CHECK(r.p_head >= 0.05 && r.p_tail < 0.01 && r.sequential && r.level == 2);
    bmp_free(&a);

    /* 4. Support rempli à 100 % avec la clé : tout le support devient suspect */
    make_cover(&b, 150, 150);
    CHECK(lsb_embed_keyed(&b, key, msg, cap) == LSB_OK);
    CHECK(det_image(&b, &r) == 0);
    CHECK(r.p_all >= 0.05 && r.level >= 1);
    bmp_free(&b);

    /* 5. Son : propre puis rempli de bits aléatoires */
    Wav w;
    w.channels = 1; w.rate = 8000; w.n = 60000;
    w.s = malloc(w.n * sizeof *w.s);
    CHECK(w.s != NULL);
    for (size_t i = 0; i < w.n; i++) {
        int v = (int)(rnd() % 400) - 200;
        w.s[i] = (int16_t)(v * 2);
    }
    CHECK(det_wav(&w, &r) == 0 && r.level == 0);
    for (size_t i = 0; i < w.n; i++) w.s[i] = (int16_t)((w.s[i] & ~1) | (int)((rnd() >> 5) & 1));
    CHECK(det_wav(&w, &r) == 0 && r.p_all >= 0.05 && r.level >= 1);
    free(w.s);
    free(msg);

    /* 6. Cas limites */
    CHECK(det_image(NULL, &r) == -1);
    CHECK(det_wav(NULL, &r) == -1);

    printf("OK : tous les tests DETECT passent\n");
    return 0;
}