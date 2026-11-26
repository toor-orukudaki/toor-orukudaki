#include <stdio.h>
#include <string.h>
#include "bmp.h"
#include "imgio.h"
#include "wav.h"
#include "detect.h"
#include "report.h"
#include "cli.h"

static int ends_wav(const char *p) {
    size_t n = strlen(p);
    return n >= 4 && p[n-4] == '.' && (p[n-3] | 32) == 'w' && (p[n-2] | 32) == 'a' && (p[n-1] | 32) == 'v';
}

int cli_version(void) {
    puts("stego 1.0 - outil educatif de steganographie et de detection");
    return 0;
}

int cli_help(void) {
    puts("STEGO - cacher et detecter des messages dans des images (BMP, PNG) et des sons (WAV)\n"
         "\n"
         "Cacher / extraire (mot de passe demande, ou variable STEGO_PASS) :\n"
         "  stego hide <in> <out> \"texte\"       cache un texte chiffre\n"
         "  stego hidefile <in> <out> <fichier>  cache un fichier chiffre\n"
         "  stego reveal <in> [dossier]          extrait le message ou le fichier\n"
         "\n"
         "Analyser :\n"
         "  stego info <fichier>                 taille et capacite\n"
         "  stego detect <fichier>               test du chi-carre et verdict\n"
         "  stego scan <fichiers...>             verdict de plusieurs fichiers\n"
         "  stego report <fichier> [rapport.md]  rapport Markdown (ecran si pas de sortie)\n"
         "  stego bitplane <in> <out> <0-7>      plan de bits d'une image\n"
         "\n"
         "Divers :\n"
         "  stego invert <in> <out>              inverse les couleurs (test)\n"
         "  stego help | version\n"
         "\n"
         "Exemples :\n"
         "  stego hide photo.png secret.png \"rendez-vous a 18h\"\n"
         "  stego scan *.png *.wav");
    return 0;
}

/* Charge et analyse un fichier. 0 = ok. */
static int analyse(const char *path, ReportData *d) {
    memset(d, 0, sizeof *d);
    d->path = path;
    d->is_wav = ends_wav(path);
    BmpStatus st;
    int rc;
    if (d->is_wav) {
        Wav w;
        st = wav_load(path, &w);
        if (st != BMP_OK) { fprintf(stderr, "Erreur (%s) : %s\n", path, bmp_strerror(st)); return -1; }
        d->channels = w.channels; d->rate = w.rate; d->samples = w.n;
        d->capacity = wav_capacity(&w);
        d->lsb_ones = report_lsb_ones_wav(&w);
        rc = det_wav(&w, &d->det);
        wav_free(&w);
    } else {
        Bmp img;
        st = img_load(path, &img);
        if (st != BMP_OK) { fprintf(stderr, "Erreur (%s) : %s\n", path, bmp_strerror(st)); return -1; }
        d->width = img.width; d->height = img.height;
        d->capacity = lsb_capacity(&img);
        d->lsb_ones = report_lsb_ones_image(&img);
        rc = det_image(&img, &d->det);
        bmp_free(&img);
    }
    if (rc != 0) { fprintf(stderr, "Erreur (%s) : analyse impossible\n", path); return -1; }
    if (report_sha256_file(path, d->sha256) != 0) strcpy(d->sha256, "inconnu");
    return 0;
}

int cmd_scan(int n, char **files) {
    int suspects = 0, errors = 0;
    for (int i = 0; i < n; i++) {
        ReportData d;
        if (analyse(files[i], &d) != 0) { errors++; continue; }
        double m = d.det.p_all > d.det.p_head ? d.det.p_all : d.det.p_head;
        if (m < 0) printf("%-13s p=n/a     %s\n", det_level_name(d.det.level), files[i]);
        else printf("%-13s p=%.4f  %s\n", det_level_name(d.det.level), m, files[i]);
        if (d.det.level > 0) suspects++;
    }
    printf("%d fichier(s) analyse(s), %d suspect(s), %d erreur(s)\n", n - errors, suspects, errors);
    return errors ? 1 : 0;
}

int cmd_report(int n, char **a) {
    if (n < 1 || n > 2) { fprintf(stderr, "Usage : stego report <fichier> [rapport.md]\n"); return 1; }
    ReportData d;
    if (analyse(a[0], &d) != 0) return 1;
    FILE *f = stdout;
    if (n == 2) {
        f = fopen(a[1], "wx"); /* n'ecrase jamais un fichier existant */
        if (!f) { fprintf(stderr, "Erreur : creation de %s impossible (existe deja ?)\n", a[1]); return 1; }
    }
    int rc = report_markdown(f, &d);
    if (n == 2 && fclose(f) != 0) rc = -1;
    if (rc != 0) { fprintf(stderr, "Erreur : ecriture du rapport\n"); return 1; }
    if (n == 2) printf("Rapport ecrit dans %s\n", a[1]);
    return 0;
}