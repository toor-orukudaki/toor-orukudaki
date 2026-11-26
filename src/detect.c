#include "detect.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MIN_EXPECTED 5.0
#define P_SUSPECT    0.05
#define P_STRONG     0.30
#define P_LOW        0.01

/* Probabilité Q(df/2, x/2) : queue de la loi du chi-carré. */
static double gammq(double a, double x) {
    double gln = lgamma(a);
    if (x <= 0) return 1.0;
    if (x < a + 1.0) { /* série */
        double ap = a, sum = 1.0 / a, del = sum;
        for (int n = 0; n < 1000; n++) {
            ap += 1.0;
            del *= x / ap;
            sum += del;
            if (fabs(del) < fabs(sum) * 1e-14) break;
        }
        return 1.0 - sum * exp(-x + a * log(x) - gln);
    }
    /* fraction continue (Lentz) */
    double b = x + 1.0 - a, c = 1.0 / 1e-300, d = 1.0 / b, h = d;
    for (int i = 1; i < 1000; i++) {
        double an = -i * (i - a);
        b += 2.0;
        d = an * d + b; if (fabs(d) < 1e-300) d = 1e-300;
        c = b + an / c; if (fabs(c) < 1e-300) c = 1e-300;
        d = 1.0 / d;
        double del = d * c;
        h *= del;
        if (fabs(del - 1.0) < 1e-14) break;
    }
    return exp(-x + a * log(x) - gln) * h;
}

static double chi2_sf(double chi2, int df) {
    if (df >= 200) { /* Wilson-Hilferty, très précis pour df grand */
        double k = 2.0 / (9.0 * df);
        double z = (cbrt(chi2 / df) - (1.0 - k)) / sqrt(k);
        return 0.5 * erfc(z / sqrt(2.0));
    }
    double p = gammq(df / 2.0, chi2 / 2.0);
    return p < 0 ? 0 : (p > 1 ? 1 : p);
}

double det_chi2_p(const uint32_t *hist, size_t nbins, int *df_out) {
    double chi2 = 0;
    int used = 0;
    for (size_t k = 0; k + 1 < nbins; k += 2) {
        double a = hist[k], b = hist[k + 1], e = (a + b) / 2.0;
        if (e >= MIN_EXPECTED) { chi2 += (a - e) * (a - e) / e; used++; }
    }
    if (df_out) *df_out = used - 1;
    if (used < 2) return -1.0;
    return chi2_sf(chi2, used - 1);
}

static int level_of(double p) { return p >= P_STRONG ? 2 : (p >= P_SUSPECT ? 1 : 0); }

static void finish(DetReport *r) {
    double m = r->p_all > r->p_head ? r->p_all : r->p_head;
    r->level = level_of(m);
    r->sequential = (r->p_head >= P_SUSPECT && r->p_tail >= 0 && r->p_tail < P_LOW);
    if (r->sequential && r->level < 2) r->level = 2;
}

int det_image(const Bmp *img, DetReport *r) {
    if (!img || !img->data || !r) return -1;
    size_t n = (size_t)img->width * img->height * 3;
    uint32_t *h = malloc(256 * sizeof *h);
    if (!h) return -1;
    size_t cut = n / 4;
    for (int part = 0; part < 3; part++) {
        size_t a = part == 0 ? 0 : (part == 1 ? 0 : n - cut);
        size_t b = part == 0 ? n : (part == 1 ? cut : n);
        for (int i = 0; i < 256; i++) h[i] = 0;
        for (size_t i = a; i < b; i++) h[img->data[i]]++;
        double p = det_chi2_p(h, 256, NULL);
        if (part == 0) r->p_all = p; else if (part == 1) r->p_head = p; else r->p_tail = p;
    }
    free(h);
    finish(r);
    return 0;
}

int det_wav(const Wav *w, DetReport *r) {
    if (!w || !w->s || !r) return -1;
    size_t n = w->n, cut = n / 4;
    uint32_t *h = malloc(65536 * sizeof *h);
    if (!h) return -1;
    for (int part = 0; part < 3; part++) {
        size_t a = part == 2 ? n - cut : 0, b = part == 1 ? cut : n;
        for (int i = 0; i < 65536; i++) h[i] = 0;
        for (size_t i = a; i < b; i++) h[(uint16_t)(w->s[i] + 32768)]++;
        double p = det_chi2_p(h, 65536, NULL);
        if (part == 0) r->p_all = p; else if (part == 1) r->p_head = p; else r->p_tail = p;
    }
    free(h);
    finish(r);
    return 0;
}

const char *det_level_name(int level) {
    return level >= 2 ? "TRES SUSPECT" : (level == 1 ? "SUSPECT" : "RAS");
}

static void show(const char *label, double p) {
    if (p < 0) printf("  %-16s : non calculable (trop peu de donnees)\n", label);
    else printf("  %-16s : p = %.4f\n", label, p);
}

static int print_report(const DetReport *r) {
    printf("Test du chi-carre (p eleve = histogramme egalise = suspect)\n");
    show("tout le support", r->p_all);
    show("premiers 25 %", r->p_head);
    show("derniers 25 %", r->p_tail);
    if (r->sequential) printf("  Signature : message ecrit de facon sequentielle au debut\n");
    printf("Verdict : %s\n", det_level_name(r->level));
    printf("Limite : un message court et disperse par une cle reste difficile a voir.\n");
    return 0;
}

int det_print_image(const Bmp *img) {
    DetReport r;
    if (det_image(img, &r) != 0) { fprintf(stderr, "Erreur : analyse impossible\n"); return 1; }
    return print_report(&r);
}

int det_print_wav(const Wav *w) {
    DetReport r;
    if (det_wav(w, &r) != 0) { fprintf(stderr, "Erreur : analyse impossible\n"); return 1; }
    return print_report(&r);
}