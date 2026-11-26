#!/usr/bin/env python3
"""Interface web locale pour STEGO. Ecoute uniquement sur 127.0.0.1."""
import io
import os
import subprocess
import tempfile
from pathlib import Path

from flask import Flask, abort, render_template_string, request, send_file
from werkzeug.utils import secure_filename

PORT = 5000
ROOT = Path(__file__).resolve().parent.parent
STEGO = ROOT / "build" / "stego"
ALLOWED = {".bmp", ".png", ".wav"}
HOSTS = {f"127.0.0.1:{PORT}", f"localhost:{PORT}"}

app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = 32 * 1024 * 1024  # 32 Mo maximum


class UserError(Exception):
    """Erreur a afficher a l'utilisateur."""


@app.before_request
def only_local():
    # Protege contre les pages externes qui viseraient ce serveur local.
    if request.host not in HOSTS:
        abort(403)


def run_stego(args, password=None):
    if not STEGO.exists():
        raise UserError("Programme introuvable : lance d'abord « make » dans le dossier du projet.")
    env = {"PATH": os.environ.get("PATH", "/usr/bin:/bin")}
    if password is not None:
        env["STEGO_PASS"] = password  # jamais dans la ligne de commande
    try:
        return subprocess.run([str(STEGO), *args], env=env, capture_output=True,
                              text=True, errors="replace", timeout=60)
    except subprocess.TimeoutExpired:
        raise UserError("Le traitement a pris trop de temps.")


def get_password():
    pw = request.form.get("password", "")
    if not pw:
        raise UserError("Entre un mot de passe.")
    if len(pw.encode()) > 200 or any(c in pw for c in "\r\n\0"):
        raise UserError("Mot de passe invalide (200 octets maximum, sans retour a la ligne).")
    return pw


def save_upload(field, tmp):
    f = request.files.get(field)
    if not f or not f.filename:
        raise UserError("Choisis un fichier image (BMP, PNG) ou son (WAV).")
    ext = Path(f.filename).suffix.lower()
    if ext not in ALLOWED:
        raise UserError("Formats acceptes : BMP, PNG, WAV.")
    path = Path(tmp) / ("in" + ext)  # nom fixe : le nom envoye n'est jamais utilise
    f.save(path)
    return path


def page(**ctx):
    for k, v in (("tab", "hide"), ("error", None), ("revealed", None), ("report", None), ("verdict", "")):
        ctx.setdefault(k, v)
    return render_template_string(PAGE, **ctx)


@app.get("/")
def index():
    return page()


@app.post("/hide")
def hide():
    try:
        pw = get_password()
        with tempfile.TemporaryDirectory() as tmp:
            src = save_upload("cover", tmp)
            out = Path(tmp) / ("out" + src.suffix)
            if request.form.get("mode") == "file":
                up = request.files.get("secret_file")
                name = secure_filename(up.filename) if up and up.filename else ""
                if not name:
                    raise UserError("Choisis le fichier a cacher.")
                pdir = Path(tmp) / "p"
                pdir.mkdir()
                up.save(pdir / name)
                args = ["hidefile", str(src), str(out), str(pdir / name)]
            else:
                text = request.form.get("text", "")
                if not text:
                    raise UserError("Ecris le message a cacher.")
                if len(text) > 10000 or "\0" in text:
                    raise UserError("Message invalide ou trop long (10 000 caracteres maximum).")
                args = ["hide", str(src), str(out), text]
            p = run_stego(args, pw)
            if p.returncode != 0 or not out.exists():
                raise UserError(p.stderr.strip() or "Echec du traitement.")
            data = out.read_bytes()
        return send_file(io.BytesIO(data), as_attachment=True, download_name="cache" + src.suffix)
    except UserError as e:
        return page(tab="hide", error=str(e))


@app.post("/reveal")
def reveal():
    try:
        pw = get_password()
        with tempfile.TemporaryDirectory() as tmp:
            src = save_upload("stego_file", tmp)
            outdir = Path(tmp) / "out"
            outdir.mkdir()
            p = run_stego(["reveal", str(src), str(outdir)], pw)
            if p.returncode != 0:
                raise UserError(p.stderr.strip() or "Echec de l'extraction.")
            files = sorted(outdir.iterdir())
            if files:
                data = files[0].read_bytes()
                return send_file(io.BytesIO(data), as_attachment=True, download_name=files[0].name)
            return page(tab="reveal", revealed=p.stdout.rstrip("\n"))
    except UserError as e:
        return page(tab="reveal", error=str(e))


@app.post("/analyse")
def analyse():
    try:
        with tempfile.TemporaryDirectory() as tmp:
            src = save_upload("target", tmp)
            p = run_stego(["report", str(src)])
            if p.returncode != 0:
                raise UserError(p.stderr.strip() or "Analyse impossible.")
            shown = secure_filename(request.files["target"].filename) or "fichier"
            report = p.stdout.replace(str(src), shown)
        verdict = next((l.split(":", 1)[1].strip() for l in report.splitlines()
                        if l.startswith("## Verdict")), "")
        return page(tab="analyse", report=report, verdict=verdict)
    except UserError as e:
        return page(tab="analyse", error=str(e))


PAGE = """<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>STEGO</title>
<style>
:root{--bg:#f6f7f9;--card:#fff;--fg:#1c2330;--mut:#667085;--acc:#3b5bdb;--bd:#d9dde5}
@media(prefers-color-scheme:dark){:root{--bg:#12151c;--card:#1b2029;--fg:#e6e9ef;--mut:#98a2b3;--acc:#5c7cfa;--bd:#2c3441}}
body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.5 system-ui,sans-serif}
main{max-width:640px;margin:0 auto;padding:24px 16px}
h1{margin:0}.sub{color:var(--mut);margin:4px 0 0}
nav{display:flex;gap:8px;margin:20px 0}
nav button{flex:1;padding:10px;border:1px solid var(--bd);background:var(--card);color:var(--fg);border-radius:8px;cursor:pointer;font:inherit}
nav button.on{background:var(--acc);color:#fff;border-color:var(--acc)}
section{display:none;background:var(--card);border:1px solid var(--bd);border-radius:12px;padding:20px}
section.on{display:block}
label{display:block;margin:14px 0 4px;font-weight:600}
label.r{display:inline;font-weight:400;margin-right:16px}
input,textarea{width:100%;box-sizing:border-box;padding:9px;border:1px solid var(--bd);border-radius:8px;background:var(--bg);color:var(--fg);font:inherit}
input[type=radio]{width:auto}
.go{margin-top:18px;padding:11px 20px;background:var(--acc);color:#fff;border:0;border-radius:8px;font:inherit;font-weight:600;cursor:pointer}
.err{background:#fde8e8;color:#8a1c1c;padding:10px 14px;border-radius:8px;margin-bottom:16px}
.out{white-space:pre-wrap;word-break:break-word;background:var(--bg);border:1px solid var(--bd);border-radius:8px;padding:12px;margin-top:16px}
.badge{display:inline-block;padding:3px 14px;border-radius:99px;color:#fff;font-weight:700}
.RAS{background:#2f9e44}.SUSPECT{background:#e8590c}.TRES{background:#c92a2a}
small{color:var(--mut)}
</style></head><body><main>
<h1>STEGO</h1>
<p class="sub">Cacher et detecter des messages dans des images et des sons. Tout reste sur ton ordinateur.</p>
<nav>
<button id="b-hide" onclick="tab('hide')">Cacher</button>
<button id="b-reveal" onclick="tab('reveal')">Extraire</button>
<button id="b-analyse" onclick="tab('analyse')">Analyser</button>
</nav>
{% if error %}<div class="err">{{ error }}</div>{% endif %}

<section id="s-hide"><form method="post" action="/hide" enctype="multipart/form-data">
<label>Image ou son de depart <small>(BMP, PNG, WAV)</small></label>
<input type="file" name="cover" accept=".bmp,.png,.wav" required>
<label>Que veux-tu cacher ?</label>
<input type="radio" id="mt" name="mode" value="text" checked onchange="mode()"><label class="r" for="mt">Un texte</label>
<input type="radio" id="mf" name="mode" value="file" onchange="mode()"><label class="r" for="mf">Un fichier</label>
<div id="m-text"><textarea name="text" rows="4" placeholder="Ton message secret"></textarea></div>
<div id="m-file" hidden><input type="file" name="secret_file"></div>
<label>Mot de passe</label><input type="password" name="password" required>
<button class="go">Cacher et telecharger</button>
<p><small>Le fichier obtenu ressemble a l'original. Garde le mot de passe : sans lui, rien ne se retrouve.</small></p>
</form></section>

<section id="s-reveal"><form method="post" action="/reveal" enctype="multipart/form-data">
<label>Fichier qui contient un message</label>
<input type="file" name="stego_file" accept=".bmp,.png,.wav" required>
<label>Mot de passe</label><input type="password" name="password" required>
<button class="go">Extraire</button>
{% if revealed is not none %}<div class="out">{{ revealed }}</div>{% endif %}
</form></section>

<section id="s-analyse"><form method="post" action="/analyse" enctype="multipart/form-data">
<label>Fichier a analyser</label>
<input type="file" name="target" accept=".bmp,.png,.wav" required>
<button class="go">Analyser</button>
{% if report %}
<p>Verdict : <span class="badge {{ verdict.split()[0] if verdict else '' }}">{{ verdict }}</span></p>
<div class="out">{{ report }}</div>
{% endif %}
</form></section>

<script>
function tab(n){for(const t of ['hide','reveal','analyse']){
document.getElementById('s-'+t).classList.toggle('on',t==n);
document.getElementById('b-'+t).classList.toggle('on',t==n)}}
function mode(){const f=document.querySelector('input[name=mode]:checked').value=='file';
document.getElementById('m-file').hidden=!f;document.getElementById('m-text').hidden=f}
tab('{{ tab }}');mode();
</script></main></body></html>
"""

if __name__ == "__main__":
    print(f"STEGO web : ouvre http://localhost:{PORT} dans ton navigateur (Ctrl+C pour arreter)")
    app.run(host="127.0.0.1", port=PORT)