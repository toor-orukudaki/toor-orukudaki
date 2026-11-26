#!/usr/bin/env bash
# Démo complète de STEGO : cacher, extraire, détecter, rapporter.
set -euo pipefail
cd "$(dirname "$0")"

make -s
make -s test > /dev/null          # crée aussi build/sample.bmp (image de départ)
S=./build/stego
D=build/demo
rm -rf "$D" && mkdir -p "$D/out"
export STEGO_PASS="demo-pass-123"

echo; echo "== 1. Image de depart"
$S info build/sample.bmp

echo; echo "== 2. Cacher un texte chiffre, puis le retrouver"
$S hide build/sample.bmp "$D/texte.bmp" "Rendez-vous a 18h"
$S reveal "$D/texte.bmp"

echo; echo "== 3. Cacher un fichier (2000 octets aleatoires)"
head -c 2000 /dev/urandom > "$D/plein.bin"
$S hidefile build/sample.bmp "$D/plein.bmp" "$D/plein.bin"
$S reveal "$D/plein.bmp" "$D/out"
cmp "$D/plein.bin" "$D/out/plein.bin" && echo "Fichier retrouve : identique a l'original"

echo; echo "== 4. Mauvais mot de passe : refuse"
STEGO_PASS=faux $S reveal "$D/texte.bmp" || echo "(refus attendu)"

echo; echo "== 5. Detection : image propre, message court, support rempli"
$S scan build/sample.bmp "$D/texte.bmp" "$D/plein.bmp"

echo; echo "== 6. Rapport automatique"
$S report "$D/plein.bmp" "$D/rapport.md"
head -n 14 "$D/rapport.md"

echo; echo "== 7. Meme chose en PNG"
$S invert build/sample.bmp "$D/image.png"
$S hide "$D/image.png" "$D/image_secret.png" "Message dans un PNG"
$S reveal "$D/image_secret.png"

echo; echo "Demo terminee. Fichiers dans $D/"
