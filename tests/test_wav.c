#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wav.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "ECHEC ligne %d : %s\n", __LINE__, #c); return 1; } } while (0)

int main(void) {
    Wav w, r;
    w.channels = 1; w.rate = 8000; w.n = 9000;
    w.s = malloc(w.n * sizeof *w.s);
    CHECK(w.s != NULL);
    for (size_t i = 0; i < w.n; i++) w.s[i] = (int16_t)((int)(i * 37 % 2000) - 1000);
    int16_t *orig = malloc(w.n * sizeof *orig);
    CHECK(orig != NULL);
    memcpy(orig, w.s, w.n * sizeof *orig);

    /* Aller-retour WAV */
    CHECK(wav_save("build/t.wav", &w) == BMP_OK);
    CHECK(wav_load("build/t.wav", &r) == BMP_OK);
    CHECK(r.channels == 1 && r.rate == 8000 && r.n == w.n);
    CHECK(memcmp(r.s, w.s, w.n * sizeof *w.s) == 0);
    wav_free(&r);

    /* Cacher -> sauvegarder -> recharger -> extraire */
    uint8_t key[LSB_KEY_LEN], *out = NULL;
    size_t n = 0;
    const char *m = "Secret dans un WAV";
    CHECK(lsb_derive_key("pw", key) == LSB_OK);
    CHECK(wav_capacity(&w) == 9000 / 8 - 4);
    CHECK(wav_embed_keyed(&w, key, (const uint8_t *)m, strlen(m)) == LSB_OK);
    CHECK(wav_save("build/t2.wav", &w) == BMP_OK);
    CHECK(wav_load("build/t2.wav", &r) == BMP_OK);
    CHECK(wav_extract_keyed(&r, key, &out, &n) == LSB_OK);
    CHECK(n == strlen(m) && memcmp(out, m, n) == 0);
    free(out);
    wav_free(&r);

    /* Le son ne change que de +-1 par échantillon */
    for (size_t i = 0; i < w.n; i++) {
        int d = (int)w.s[i] - (int)orig[i];
        CHECK(d >= -1 && d <= 1);
    }

    /* Trop grand, fichier invalide, fichier absent */
    uint8_t *big = calloc(wav_capacity(&w) + 1, 1);
    CHECK(big && wav_embed_keyed(&w, key, big, wav_capacity(&w) + 1) == LSB_ERR_CAPACITY);
    free(big);
    FILE *f = fopen("build/bad.wav", "wb");
    CHECK(f != NULL);
    fputs("ceci n'est pas un fichier wav, juste du texte de remplissage pour depasser 44 octets", f);
    fclose(f);
    CHECK(wav_load("build/bad.wav", &r) == BMP_ERR_FORMAT);
    CHECK(wav_load("build/absent.wav", &r) == BMP_ERR_IO);

    free(orig);
    wav_free(&w);
    printf("OK : tous les tests WAV passent\n");
    return 0;
}