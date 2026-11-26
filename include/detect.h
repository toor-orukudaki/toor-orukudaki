#ifndef DETECT_H
#define DETECT_H
#include <stddef.h>
#include <stdint.h>
#include "bmp.h"
#include "wav.h"

/* p-values du test du chi-carré (paires de valeurs) : p élevé = histogramme
   "égalisé" par l'insertion de bits aléatoires = suspect. -1 = non calculable. */
typedef struct {
    double p_all;   /* tout le support */
    double p_head;  /* premiers 25 % */
    double p_tail;  /* derniers 25 % */
    int sequential; /* signature d'un message écrit de façon séquentielle */
    int level;      /* 0 = RAS, 1 = suspect, 2 = tres suspect */
} DetReport;

/* Test sur un histogramme de nbins cases ; df reçoit les degrés de liberté. */
double det_chi2_p(const uint32_t *hist, size_t nbins, int *df);
int det_image(const Bmp *img, DetReport *r);
int det_wav(const Wav *w, DetReport *r);
/* Analyse + affichage ; renvoie 0 si l'analyse a pu être faite. */
int det_print_image(const Bmp *img);
int det_print_wav(const Wav *w);
const char *det_level_name(int level);

#endif