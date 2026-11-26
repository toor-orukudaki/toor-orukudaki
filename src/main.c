#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sodium.h>
#include "bmp.h"
#include "imgio.h"
#include "lsb.h"
#include "stegocrypt.h"
#include "payload.h"
#include "wav.h"
#include "detect.h"
#include "cli.h"

/* Un support = une image OU un fichier audio, choisi selon l'extension. */
typedef struct { Bmp img; Wav wav; int is_wav; } Media;

static int usage(void) {
    fprintf(stderr,
        "Usage (image BMP/PNG ou son WAV) :\n"
        "  stego info <fichier>\n"
        "  stego hide <in> <out> \"texte\"\n"
        "  stego hidefile <in> <out> <fichier>\n"
        "  stego reveal <in> [dossier_sortie]\n"
        "  stego detect <fichier>\n"
        "  stego scan <fichiers...>\n"
        "  stego report <fichier> [rapport.md]\n"
        "  stego help\n"
        "Images seulement :\n"
        "  stego invert <in> <out>\n"
        "  stego bitplane <in> <out> <0-7>\n"
        "Mot de passe demande ; pour les scripts : variable STEGO_PASS.\n");
    return 1;
}

static int ends_wav(const char *p) {
    size_t n = strlen(p);
    return n >= 4 && p[n-4] == '.' && (p[n-3] | 32) == 'w' && (p[n-2] | 32) == 'a' && (p[n-1] | 32) == 'v';
}

static int media_load(const char *path, Media *m) {
    memset(m, 0, sizeof *m);
    m->is_wav = ends_wav(path);
    BmpStatus st = m->is_wav ? wav_load(path, &m->wav) : img_load(path, &m->img);
    if (st != BMP_OK) { fprintf(stderr, "Erreur : %s\n", bmp_strerror(st)); return -1; }
    return 0;
}
static void media_free(Media *m) { bmp_free(&m->img); wav_free(&m->wav); }
static size_t media_cap(const Media *m) { return m->is_wav ? wav_capacity(&m->wav) : lsb_capacity(&m->img); }
static LsbStatus media_embed(Media *m, const uint8_t *k, const uint8_t *d, size_t n) {
    return m->is_wav ? wav_embed_keyed(&m->wav, k, d, n) : lsb_embed_keyed(&m->img, k, d, n);
}
static LsbStatus media_extract(const Media *m, const uint8_t *k, uint8_t **d, size_t *n) {
    return m->is_wav ? wav_extract_keyed(&m->wav, k, d, n) : lsb_extract_keyed(&m->img, k, d, n);
}
static int media_save(const Media *m, const char *path) {
    BmpStatus st = m->is_wav ? wav_save(path, &m->wav) : img_save(path, &m->img);
    if (st != BMP_OK) { fprintf(stderr, "Erreur : %s\n", bmp_strerror(st)); return 1; }
    return 0;
}

/* Saisie sans écho dans le terminal. */
static int read_hidden(const char *prompt, char *buf, size_t n) {
    struct termios old, tmp;
    int tty = isatty(STDIN_FILENO);
    fputs(prompt, stderr);
    if (tty) {
        tcgetattr(STDIN_FILENO, &old);
        tmp = old;
        tmp.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &tmp);
    }
    char *r = fgets(buf, (int)n, stdin);
    if (tty) { tcsetattr(STDIN_FILENO, TCSANOW, &old); fputc('\n', stderr); }
    if (!r) return -1;
    buf[strcspn(buf, "\r\n")] = 0;
    return 0;
}

static int get_password(char *buf, size_t n, int confirm) {
    const char *env = getenv("STEGO_PASS");
    if (env) {
        if (strlen(env) >= n) return -1;
        memcpy(buf, env, strlen(env) + 1);
        return buf[0] ? 0 : -1;
    }
    if (read_hidden("Mot de passe : ", buf, n) != 0) return -1;
    if (confirm) {
        char again[256];
        int ok = read_hidden("Confirmer     : ", again, sizeof again) == 0 && !strcmp(buf, again);
        sodium_memzero(again, sizeof again);
        if (!ok) { fprintf(stderr, "Erreur : les mots de passe different.\n"); return -1; }
    }
    return buf[0] ? 0 : -1;
}

static size_t max_data(const Media *m, size_t namelen) {
    size_t cap = media_cap(m), ov = SC_OVERHEAD + 1 + namelen;
    return cap > ov ? cap - ov : 0;
}

static uint8_t *read_file(const char *path, size_t max, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "Erreur : lecture de %s impossible\n", path); return NULL; }
    long sz = (fseek(f, 0, SEEK_END) == 0) ? ftell(f) : -1;
    if (sz < 0 || (size_t)sz > max) {
        fprintf(stderr, "Erreur : fichier trop grand pour ce support (max %zu octets)\n", max);
        fclose(f);
        return NULL;
    }
    rewind(f);
    uint8_t *buf = malloc((size_t)sz + 1);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fprintf(stderr, "Erreur : lecture de %s impossible\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *len = (size_t)sz;
    return buf;
}

static int do_hide(Media *m, const char *out_path, const char *name,
                   const uint8_t *data, size_t len) {
    char pw[256];
    uint8_t key[LSB_KEY_LEN], *pl = NULL, *blob = NULL;
    size_t pll = 0, bl = 0;
    int rc = 1;
    ScStatus cs;
    LsbStatus ls;

    memset(key, 0, sizeof key);
    if (get_password(pw, sizeof pw, 1) != 0) goto end;
    if (pl_pack(name, data, len, &pl, &pll) != 0) {
        fprintf(stderr, "Erreur : nom de fichier trop long\n");
        goto end;
    }
    if ((cs = sc_seal(pw, pl, pll, &blob, &bl)) != SC_OK) {
        fprintf(stderr, "Erreur : %s\n", sc_strerror(cs));
        goto end;
    }
    if ((ls = lsb_derive_key(pw, key)) != LSB_OK ||
        (ls = media_embed(m, key, blob, bl)) != LSB_OK) {
        fprintf(stderr, "Erreur : %s\n", lsb_strerror(ls));
        goto end;
    }
    if ((rc = media_save(m, out_path)) == 0)
        printf("%zu octets chiffres caches dans %s\n", bl, out_path);
end:
    sodium_memzero(pw, sizeof pw);
    sodium_memzero(key, sizeof key);
    if (pl) { sodium_memzero(pl, pll); free(pl); }
    free(blob);
    return rc;
}

static int cmd_hidefile(Media *m, const char *out_path, const char *path) {
    const char *base = path, *p;
    if ((p = strrchr(base, '/'))) base = p + 1;
    if ((p = strrchr(base, '\\'))) base = p + 1;
    if (!base[0]) { fprintf(stderr, "Erreur : nom de fichier invalide\n"); return 1; }
    size_t len = 0;
    uint8_t *data = read_file(path, max_data(m, strlen(base)), &len);
    if (!data) return 1;
    int rc = do_hide(m, out_path, base, data, len);
    free(data);
    return rc;
}

static int cmd_reveal(const Media *m, const char *outdir) {
    char pw[256], name[PL_MAX_NAME + 1];
    uint8_t key[LSB_KEY_LEN], *blob = NULL, *pl = NULL;
    size_t bl = 0, pll = 0, dl = 0;
    const uint8_t *data = NULL;
    int rc = 1;
    ScStatus cs;
    LsbStatus ls;

    memset(key, 0, sizeof key);
    if (get_password(pw, sizeof pw, 0) != 0) goto end;
    if ((ls = lsb_derive_key(pw, key)) != LSB_OK ||
        (ls = media_extract(m, key, &blob, &bl)) != LSB_OK) {
        fprintf(stderr, "Erreur : %s\n", lsb_strerror(ls));
        goto end;
    }
    if ((cs = sc_open(pw, blob, bl, &pl, &pll)) != SC_OK) {
        fprintf(stderr, "Erreur : %s\n", sc_strerror(cs));
        goto end;
    }
    if (pl_unpack(pl, pll, name, &data, &dl) != 0) {
        fprintf(stderr, "Erreur : charge utile invalide\n");
        goto end;
    }
    if (!name[0]) {
        fwrite(data, 1, dl, stdout);
        putchar('\n');
        rc = 0;
    } else {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", outdir ? outdir : ".", name);
        FILE *f = fopen(path, "wbx"); /* x : n'écrase jamais un fichier existant */
        if (!f) fprintf(stderr, "Erreur : creation de %s impossible (existe deja ?)\n", path);
        else {
            int bad = (fwrite(data, 1, dl, f) != dl);
            if (fclose(f) != 0) bad = 1;
            if (bad) fprintf(stderr, "Erreur : ecriture de %s\n", path);
            else { printf("Fichier extrait : %s (%zu octets)\n", path, dl); rc = 0; }
        }
    }
end:
    sodium_memzero(pw, sizeof pw);
    sodium_memzero(key, sizeof key);
    if (pl) { sodium_memzero(pl, pll + 1); free(pl); }
    free(blob);
    return rc;
}

int main(int argc, char **argv) {
    if (argc >= 2 && (!strcmp(argv[1], "help") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) return cli_help();
    if (argc == 2 && !strcmp(argv[1], "version")) return cli_version();
    if (argc >= 3 && !strcmp(argv[1], "scan")) return cmd_scan(argc - 2, argv + 2);
    if (argc >= 3 && !strcmp(argv[1], "report")) return cmd_report(argc - 2, argv + 2);
    if (argc < 3) return usage();
    const char *cmd = argv[1];
    Media m;
    if (media_load(argv[2], &m) != 0) return 1;
    int rc = 0;

    if (!strcmp(cmd, "info") && argc == 3) {
        if (m.is_wav)
            printf("%s : %u canal(aux), %u Hz, %zu echantillons\n", argv[2],
                   (unsigned)m.wav.channels, (unsigned)m.wav.rate, m.wav.n);
        else
            printf("%s : %u x %u pixels\n", argv[2], m.img.width, m.img.height);
        printf("Capacite LSB : %zu octets (fichier ou texte max : %zu octets)\n",
               media_cap(&m), max_data(&m, 0));
    } else if (!strcmp(cmd, "detect") && argc == 3) {
        rc = m.is_wav ? det_print_wav(&m.wav) : det_print_image(&m.img);
    } else if (m.is_wav && (!strcmp(cmd, "invert") || !strcmp(cmd, "bitplane"))) {
        fprintf(stderr, "Erreur : commande reservee aux images\n");
        rc = 1;
    } else if (!strcmp(cmd, "invert") && argc == 4) {
        size_t n = (size_t)m.img.width * m.img.height * 3;
        for (size_t i = 0; i < n; i++) m.img.data[i] = 255 - m.img.data[i];
        rc = media_save(&m, argv[3]);
    } else if (!strcmp(cmd, "bitplane") && argc == 5) {
        long bit = strtol(argv[4], NULL, 10);
        Bmp out;
        LsbStatus ls = (bit >= 0 && bit <= 7) ? lsb_bitplane(&m.img, (unsigned)bit, &out)
                                              : LSB_ERR_ARG;
        if (ls != LSB_OK) { fprintf(stderr, "Erreur : %s\n", lsb_strerror(ls)); rc = 1; }
        else {
            BmpStatus st = img_save(argv[3], &out);
            if (st != BMP_OK) { fprintf(stderr, "Erreur : %s\n", bmp_strerror(st)); rc = 1; }
            bmp_free(&out);
        }
    } else if (!strcmp(cmd, "hide") && argc == 5) {
        rc = do_hide(&m, argv[3], "", (const uint8_t *)argv[4], strlen(argv[4]));
    } else if (!strcmp(cmd, "hidefile") && argc == 5) {
        rc = cmd_hidefile(&m, argv[3], argv[4]);
    } else if (!strcmp(cmd, "reveal") && (argc == 3 || argc == 4)) {
        rc = cmd_reveal(&m, argc == 4 ? argv[3] : NULL);
    } else {
        rc = usage();
    }
    media_free(&m);
    return rc;
}