#include "report.h"
#include <sodium.h>
#include <string.h>
#include <time.h>

int report_sha256_file(const char *path, char out[65]) {
    if (!path || !out || sodium_init() < 0) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    crypto_hash_sha256_state st;
    crypto_hash_sha256_init(&st);
    unsigned char buf[4096], h[crypto_hash_sha256_BYTES];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) crypto_hash_sha256_update(&st, buf, n);
    int bad = ferror(f);
    fclose(f);
    if (bad) return -1;
    crypto_hash_sha256_final(&st, h);
    sodium_bin2hex(out, 65, h, sizeof h);
    return 0;
}

double report_lsb_ones_image(const Bmp *img) {
    if (!img || !img->data) return -1.0;
    size_t n = (size_t)img->width * img->height * 3, ones = 0;
    for (size_t i = 0; i < n; i++) ones += img->data[i] & 1u;
    return n ? (double)ones / (double)n : -1.0;
}

double report_lsb_ones_wav(const Wav *w) {
    if (!w || !w->s || !w->n) return -1.0;
    size_t ones = 0;
    for (size_t i = 0; i < w->n; i++) ones += (uint16_t)w->s[i] & 1u;
    return (double)ones / (double)w->n;
}

static void pstr(char *b, size_t n, double p) {
    if (p < 0) snprintf(b, n, "non calculable");
    else snprintf(b, n, "%.4f", p);
}

int report_markdown(FILE *f, const ReportData *d) {
    if (!f || !d) return -1;
    char t[32] = "date inconnue", p1[32], p2[32], p3[32];
    time_t now = time(NULL);
    struct tm *g = gmtime(&now);
    if (g) strftime(t, sizeof t, "%Y-%m-%d %H:%M UTC", g);
    pstr(p1, sizeof p1, d->det.p_all);
    pstr(p2, sizeof p2, d->det.p_head);
    pstr(p3, sizeof p3, d->det.p_tail);

    fprintf(f, "# Rapport d'analyse STEGO\n\n");
    fprintf(f, "- Date : %s\n- Fichier : `%s`\n", t, d->path ? d->path : "?");
    if (d->is_wav)
        fprintf(f, "- Type : son WAV, %u canal(aux), %u Hz, %zu echantillons\n",
                d->channels, d->rate, d->samples);
    else
        fprintf(f, "- Type : image, %u x %u pixels\n", d->width, d->height);
    fprintf(f, "- SHA-256 : `%s`\n- Capacite LSB : %zu octets\n\n", d->sha256, d->capacity);

    fprintf(f, "## Test du chi-carre (paires de valeurs)\n\n");
    fprintf(f, "| Zone analysee | p-value |\n|---|---|\n");
    fprintf(f, "| Tout le support | %s |\n| Premiers 25 %% | %s |\n| Derniers 25 %% | %s |\n\n", p1, p2, p3);
    if (d->lsb_ones >= 0)
        fprintf(f, "- Proportion de bits de poids faible a 1 : %.4f (proche de 0.5 = bits aleatoires)\n", d->lsb_ones);
    fprintf(f, "- Signature sequentielle : %s\n\n", d->det.sequential ? "oui (message ecrit au debut du support)" : "non");

    fprintf(f, "## Verdict : %s\n\n", det_level_name(d->det.level));
    if (d->det.level == 0)
        fprintf(f, "Aucun signe d'histogramme egalise. Cela ne prouve pas l'absence de message : un message court disperse par une cle n'est pas visible avec ce test.\n\n");
    else if (d->det.level == 1)
        fprintf(f, "L'histogramme est en partie egalise, ce qui peut indiquer des bits aleatoires inseres. Comparer avec une copie d'origine si elle existe.\n\n");
    else
        fprintf(f, "Les paires de valeurs sont fortement egalisees : fort indice de bits aleatoires inseres. Un faux positif reste possible (support bruite ou deja traite).\n\n");

    fprintf(f, "## Limites\n\n");
    fprintf(f, "- Methode statistique simple (chi-carre), adaptee aux supports remplis ou aux ecritures sequentielles.\n");
    fprintf(f, "- Outil educatif : un verdict RAS n'est pas une garantie.\n");
    return ferror(f) ? -1 : 0;
}