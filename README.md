# STEGO

Outil **educatif et defensif** de steganographie en C : il cache des messages et des fichiers
chiffres dans des images (BMP, PNG) et des sons (WAV), et contient un detecteur qui repere les
supports suspects. Projet de cybersecurite realise a la Faculte des Sciences de Rabat.

## Fonctionnalites

- Support : BMP 24 bits, PNG, WAV PCM 16 bits.
- Chiffrement avec mot de passe : Argon2id + XChaCha20-Poly1305 (libsodium), integrite verifiee.
- Les bits sont disperses dans tout le support selon une cle derivee du mot de passe.
- On peut cacher un texte ou n'importe quel fichier (nom conserve, jamais ecrase a l'extraction).
- Detecteur : test du chi-carre sur les paires de valeurs, verdict RAS / SUSPECT / TRES SUSPECT.
- Rapport automatique au format Markdown avec empreinte SHA-256.

## Installation (Ubuntu ou WSL)

    sudo apt install build-essential make libsodium-dev libpng-dev
    make test
    make

Le programme est ensuite dans `build/stego`. Pour voir une demonstration complete :

    bash demo.sh

## Utilisation

    stego hide photo.png secret.png "rendez-vous a 18h"     # cacher un texte
    stego hidefile photo.png secret.png document.pdf        # cacher un fichier
    stego reveal secret.png [dossier]                       # extraire
    stego info photo.png                                    # capacite
    stego detect secret.png                                 # analyser un fichier
    stego scan *.png *.wav                                  # analyser plusieurs fichiers
    stego report secret.png rapport.md                      # rapport Markdown
    stego bitplane photo.png plan0.png 0                    # visualiser un plan de bits

Le mot de passe est demande sans affichage. Pour les scripts : variable `STEGO_PASS`.

## Fonctionnement

1. Le message est emballe avec son nom : `longueur du nom | nom | donnees`.
2. Il est chiffre dans un conteneur : `STG1 | version | sel (16) | nonce (24) | taille (4) | chiffre + tag`.
3. Une cle derivee du mot de passe (Argon2id) melange les positions des canaux de couleur (ou des
   echantillons audio) ; chaque bit du conteneur remplace le bit de poids faible d'une position.
4. A l'extraction, sans le bon mot de passe, ni les positions ni le message ne sont retrouvables.

## Detecteur

L'insertion de bits aleatoires egalise les effectifs des paires de valeurs (2k, 2k+1) : c'est ce
que mesure le test du chi-carre. Trois analyses sont faites : tout le support, les 25 % du debut
et les 25 % de la fin (signature d'une ecriture sequentielle).

## Limites connues

- Le detecteur voit surtout les supports bien remplis (environ 80 % et plus) ou les ecritures
  sequentielles : un message court et disperse reste invisible pour lui.
- Un meme mot de passe donne les memes positions sur des supports de meme taille.
- Les formats avec perte (JPEG, MP3) detruisent le message : ils ne sont pas pris en charge.
- Le PNG est converti en RGB 8 bits : la transparence est perdue.
- Outil d'apprentissage : il n'a pas ete audite et ne remplace pas un vrai outil de securite.

## Structure

    include/   en-tetes          src/      code (bmp, lsb, stegocrypt, payload, imgio, wav, detect, report, main)
    tests/     tests unitaires   Makefile  make test = tous les tests avec ASan et UBSan

## Ethique

Ce projet sert a apprendre, proteger et detecter. Utilise-le uniquement sur des fichiers qui
t'appartiennent ou avec l'accord de leur proprietaire.

## Licence

MIT (voir le fichier LICENSE).

## Interface web

    sudo apt install python3-flask
    python3 web/app.py

Puis ouvre http://localhost:5000 : trois onglets (Cacher, Extraire, Analyser). Le serveur
n'ecoute que sur ton ordinateur (127.0.0.1) ; les fichiers sont traites dans un dossier
temporaire efface juste apres.
