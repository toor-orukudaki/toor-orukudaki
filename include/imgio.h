#ifndef IMGIO_H
#define IMGIO_H
#include "bmp.h"

/* Charge un BMP ou un PNG (détecté par le contenu du fichier) en RGB 8 bits. */
BmpStatus img_load(const char *path, Bmp *img);
/* Enregistre en PNG si path finit par .png, sinon en BMP. */
BmpStatus img_save(const char *path, const Bmp *img);

#endif