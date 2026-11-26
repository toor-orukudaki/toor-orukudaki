#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "report.h"

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "ECHEC ligne %d : %s\n", __LINE__, #c); return 1; } } while (0)

int main(void) {
    /* SHA-256 de "abc" : valeur de référence du standard */
    FILE *f = fopen("build/abc.txt", "wb");
    CHECK(f != NULL);
    fputs("abc", f);
    fclose(f);
    char h[65];
    CHECK(report_sha256_file("build/abc.txt", h) == 0);
    CHECK(strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    CHECK(report_sha256_file("build/absent.txt", h) == -1);

    /* Proportion de LSB à 1 */
    Bmp img;
    CHECK(bmp_create(&img, 2, 2) == BMP_OK);
    for (size_t i = 0; i < 12; i++) img.data[i] = (uint8_t)i;      /* 6 impairs sur 12 */
    CHECK(report_lsb_ones_image(&img) == 0.5);
    for (size_t i = 0; i < 12; i++) img.data[i] = 255;
    CHECK(report_lsb_ones_image(&img) == 1.0);
    bmp_free(&img);
    CHECK(report_lsb_ones_image(NULL) < 0);

    /* Rapport Markdown */
    ReportData d;
    memset(&d, 0, sizeof d);
    d.path = "photo.png"; d.width = 10; d.height = 20; d.capacity = 123; d.lsb_ones = 0.5;
    strcpy(d.sha256, h);
    d.det.p_all = 0.9; d.det.p_head = 0.95; d.det.p_tail = 0.001; d.det.sequential = 1; d.det.level = 2;
    FILE *t = tmpfile();
    CHECK(t != NULL);
    CHECK(report_markdown(t, &d) == 0);
    rewind(t);
    char buf[4096] = {0};
    size_t n = fread(buf, 1, sizeof buf - 1, t);
    fclose(t);
    CHECK(n > 200);
    CHECK(strstr(buf, "# Rapport d'analyse STEGO") && strstr(buf, "photo.png"));
    CHECK(strstr(buf, "10 x 20") && strstr(buf, "ba7816bf") && strstr(buf, "TRES SUSPECT"));
    CHECK(strstr(buf, "0.9500") && strstr(buf, "oui (message"));

    d.det.p_all = -1; d.det.level = 0; d.det.sequential = 0;
    t = tmpfile();
    CHECK(t && report_markdown(t, &d) == 0);
    rewind(t);
    memset(buf, 0, sizeof buf);
    CHECK(fread(buf, 1, sizeof buf - 1, t) > 100);
    fclose(t);
    CHECK(strstr(buf, "non calculable") && strstr(buf, "RAS"));

    CHECK(report_markdown(NULL, &d) == -1);
    printf("OK : tous les tests REPORT passent\n");
    return 0;
}