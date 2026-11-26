Jour 1 (Phase 1) : lecture/écriture BMP 24 bits en C, accès pixel RGB, CLI (info, invert), test OK.
Fichiers : include/bmp.h, src/bmp.c, src/main.c, tests/test_bmp.c, build.ps1 (pas de make sous Windows).
API : bmp_create/load/save/free, bmp_pixel, bmp_strerror (codes BmpStatus). Binaire : build/stego_tool.exe.
Env : Windows, gcc MinGW ; stego.exe bloqué par Windows, d'où stego_tool.exe + Unblock-File.


Prochain jour : plans de bits (extraire/visualiser le bit 0 d'un canal) puis début LSB (Phase 2).


Jour 2 (Phase 1 fin + Phase 2) : LSB séquentiel (1 bit/canal), texte caché/extrait, plans de bits, tests OK (ASan/UBSan).
Fichiers : include/{bmp,lsb}.h, src/{bmp,lsb,main}.c, tests/test_{bmp,lsb}.c, Makefile (build.ps1 abandonné).
Env : WSL Ubuntu (user toor), projet dans /mnt/c/Users/amine/Documents/Amine Project ; binaire build/stego ; make test / make.
Format provisoire : 4 octets taille (little-endian) + message, sans chiffrement ni intégrité.


Prochain jour : Phase 3 début : chiffrement libsodium (mot de passe) et en-tête propre.

Jour 3 (Phase 3) : chiffrement Argon2id + XChaCha20-Poly1305, conteneur STG1 (en-tête, sel, nonce, taille, tag), mot de passe sans écho.
Fichiers ajoutés : include/stegocrypt.h, src/stegocrypt.c, tests/test_crypt.c ; modifiés : src/main.c (hide/reveal chiffrés), Makefile (-lsodium).
API : sc_seal / sc_open / sc_strerror (ScStatus). Surcoût : 65 octets. STEGO_PASS pour les scripts.
Env : WSL Ubuntu, make test / make, binaire build/stego.


Prochain jour : Phase 4 début : cacher un fichier quelconque (nom + taille dans le conteneur), puis bits pseudo-aléatoires selon la clé.

Jour 4 (Phase 4) : fichiers quelconques (charge utile nom|données), bits LSB à positions pseudo-aléatoires selon la clé (Argon2id), contrôle de capacité.
Fichiers ajoutés : include/payload.h, src/payload.c, tests/test_keyed.c ; modifiés : include/lsb.h, src/lsb.c (+keyed), src/main.c (hidefile, reveal [dossier]), Makefile.
API : lsb_derive_key / lsb_embed_keyed / lsb_extract_keyed, pl_pack / pl_unpack ; lsb.c dépend de libsodium.
Env : WSL Ubuntu, make test / make, binaire build/stego, STEGO_PASS pour les scripts.


Prochain jour : Phase 5 : support PNG (décodage/encodage), puis WAV.


Jour 5 (Phase 5) : support PNG via libpng (API simplifiée), détection par signature à la lecture, extension à l'écriture.
Fichiers ajoutés : include/imgio.h, src/imgio.c (img_load / img_save), tests/test_png.c ; modifiés : src/main.c (bmp_load/save -> img_load/save), Makefile (-lpng).
Dépendance : sudo apt install libpng-dev. Formats : BMP 24 bits et PNG (converti en RGB 8 bits).
Env : WSL Ubuntu, make test / make, binaire build/stego, STEGO_PASS pour les scripts.


Prochain jour : Phase 5 suite : WAV 16 bits (lecture/écriture, LSB dans les échantillons).

Jour 6A (fin Phase 5) : support WAV PCM 16 bits, LSB à positions pseudo-aléatoires sur les échantillons (réutilise lsb_*_keyed).
Fichiers ajoutés : include/wav.h, src/wav.c, tests/test_wav.c ; modifiés : src/main.c (type Media image/son), Makefile (boucle de tests).
Formats : BMP 24 bits, PNG, WAV 16 bits ; Phase 5 terminée.
Env : WSL Ubuntu, make test / make, binaire build/stego, STEGO_PASS pour les scripts.


Prochain : 6B détecteur (chi-carré, plans de bits, score de suspicion).

Jour 6C (Phase 7 partielle) : commandes help, version, scan (plusieurs fichiers), report (Markdown + SHA-256).
Fichiers ajoutés : include/cli.h, include/report.h, src/report.c, src/cli_extra.c, tests/test_report.c ; modifiés : src/main.c (aiguillage, usage), Makefile.
Test de référence : SHA-256("abc") ; rapport refuse d'écraser un fichier existant.
Env : WSL Ubuntu, make test / make, binaire build/stego.


Prochain : 6D README, démo, mise en ligne GitHub (.gitignore, licence).
