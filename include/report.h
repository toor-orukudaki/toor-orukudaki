#ifndef REPORT_H
#define REPORT_H
#include <stdio.h>
#include "detect.h"

typedef struct {
    const char *path;
    int is_wav;
    unsigned width, height;          /* image */
    unsigned channels, rate;         /* son */
    size_t samples;                  /* son */
    size_t capacity;                 /* octets de message cachables */
    double lsb_ones;                 /* proportion de LSB à 1 (-1 = inconnue) */
    char sha256[65];
    DetReport det;
} ReportData;

/* SHA-256 d'un fichier, en hexadécimal. 0 = ok. */
int    report_sha256_file(const char *path, char out[65]);
double report_lsb_ones_image(const Bmp *img);
double report_lsb_ones_wav(const Wav *w);
/* Rapport au format Markdown. 0 = ok. */
int    report_markdown(FILE *f, const ReportData *d);

#endif