#!/usr/bin/env python3
"""Interface web locale pour STEGO. Ecoute uniquement sur 127.0.0.1."""
import io
import os
import re
import subprocess
import tempfile
from pathlib import Path

from flask import Flask, abort, jsonify, render_template_string, request, send_file
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


@app.post("/capacity")
def capacity():
    """Place disponible dans un support (pour l'indicateur de la page)."""
    try:
        with tempfile.TemporaryDirectory() as tmp:
            src = save_upload("cover", tmp)
            p = run_stego(["info", str(src)])
            m = re.search(r"max : (\d+) octets", p.stdout)
            if p.returncode != 0 or not m:
                raise UserError(p.stderr.strip() or "Analyse impossible.")
            return jsonify(max=int(m.group(1)))
    except UserError as e:
        return jsonify(error=str(e)), 400


@app.post("/hide")
def hide():
    try:
        pw = get_password()
        if request.form.get("password2") != pw:
            raise UserError("Les deux mots de passe sont differents.")
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


PAGE = r"""<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>STEGO</title>
<style>
:root{--bg:#0b1020;--card:rgba(20,27,50,.86);--fg:#e8ecf7;--mut:#93a0c0;--acc:#7c8cff;--acc2:#5eead4;--bd:#2b3558}
*{box-sizing:border-box}
body{margin:0;color:var(--fg);font:16px/1.5 system-ui,sans-serif;min-height:100vh;
background:radial-gradient(ellipse at top,#18235a 0,var(--bg) 62%) fixed}
body::before{content:"";position:fixed;inset:0;z-index:-1;animation:tw 4s ease-in-out infinite alternate;
background-size:260px 220px;background-image:
radial-gradient(1.5px 1.5px at 20px 30px,#fff,transparent),radial-gradient(1px 1px at 90px 120px,#bfe9ff,transparent),
radial-gradient(1.5px 1.5px at 160px 60px,#fff,transparent),radial-gradient(1px 1px at 230px 180px,#fff,transparent),
radial-gradient(1px 1px at 130px 200px,#ffe9bf,transparent)}
@keyframes tw{from{opacity:.45}to{opacity:1}}
main{max-width:660px;margin:0 auto;padding:28px 16px 60px}
h1{margin:0;font-size:2.3rem;background:linear-gradient(90deg,var(--acc),var(--acc2));-webkit-background-clip:text;background-clip:text;color:transparent}
.sub{color:var(--mut);margin:4px 0 0}
nav{display:flex;gap:8px;margin:22px 0}
nav button{flex:1;padding:11px;border:1px solid var(--bd);background:var(--card);color:var(--fg);border-radius:10px;cursor:pointer;font:inherit;transition:.15s}
nav button:hover{border-color:var(--acc)}
nav button.on{background:linear-gradient(135deg,var(--acc),#5b6cf0);border-color:var(--acc);font-weight:600}
section{display:none;background:var(--card);backdrop-filter:blur(6px);border:1px solid var(--bd);border-radius:14px;padding:20px}
section.on{display:block}
label{display:block;margin:16px 0 6px;font-weight:600}
label.r{display:inline;font-weight:400;margin-right:16px}
input,textarea{width:100%;padding:10px;border:1px solid var(--bd);border-radius:10px;background:#0e1530;color:var(--fg);font:inherit}
input[type=radio]{width:auto}
.drop{position:relative;border:2px dashed var(--bd);border-radius:12px;padding:22px;text-align:center;color:var(--mut);transition:.15s}
.drop:hover{border-color:var(--acc);color:var(--fg)}
.drop input{position:absolute;inset:0;width:100%;height:100%;opacity:0;cursor:pointer}
.pv{margin-top:10px;text-align:center}.pv img{max-width:100%;max-height:200px;border-radius:10px;display:block;margin:0 auto 6px}
.pv audio{width:100%;margin-bottom:6px}
.pw{display:flex;gap:6px}.pw input{flex:1}
.ico{padding:0 12px;border:1px solid var(--bd);background:#0e1530;color:var(--fg);border-radius:10px;cursor:pointer;font:inherit}
.go{margin-top:20px;width:100%;padding:12px;background:linear-gradient(135deg,var(--acc),var(--acc2));color:#0b1020;border:0;border-radius:10px;font:inherit;font-weight:700;cursor:pointer}
.go:disabled{opacity:.45;cursor:not-allowed}
.ghost{margin-top:10px;padding:8px 14px;border:1px solid var(--bd);background:transparent;color:var(--fg);border-radius:10px;cursor:pointer;font:inherit}
.err{background:#3b1520;border:1px solid #7a2b3d;color:#ffb3c0;padding:10px 14px;border-radius:10px;margin-bottom:16px}
.out{white-space:pre-wrap;word-break:break-word;background:#0e1530;border:1px solid var(--bd);border-radius:10px;padding:12px;margin-top:16px}
.badge{display:inline-block;padding:3px 14px;border-radius:99px;color:#fff;font-weight:700}
.RAS{background:#2f9e44}.SUSPECT{background:#e8590c}.TRES{background:#c92a2a}
.bar{height:8px;background:#0e1530;border-radius:99px;overflow:hidden;margin-top:14px}
.bar i{display:block;height:100%;width:0;background:var(--acc2);transition:.25s}.bar i.over{background:#ff6b81}
small{color:var(--mut)}
#toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%) translateY(80px);background:#1c2750;border:1px solid var(--acc);
padding:10px 18px;border-radius:99px;opacity:0;transition:.3s;pointer-events:none}
#toast.show{transform:translateX(-50%);opacity:1}
</style></head><body><main>
<h1>🛰️ STEGO</h1>
<p class="sub">Cache tes messages dans une image ou un son. Rien ne quitte ton ordinateur.</p>
<nav>
<button id="b-hide" onclick="tab('hide')">🔒 Cacher</button>
<button id="b-reveal" onclick="tab('reveal')">🔓 Extraire</button>
<button id="b-analyse" onclick="tab('analyse')">🔍 Analyser</button>
</nav>
{% if error %}<div class="err">⚠️ {{ error }}</div>{% endif %}

<section id="s-hide"><form method="post" action="/hide" enctype="multipart/form-data" onsubmit="return busy(this)">
<label>1. Image ou son de depart <small>(BMP, PNG, WAV)</small></label>
<div class="drop"><input type="file" name="cover" id="cover" accept=".bmp,.png,.wav" required onchange="picked(this,'pv-cover');capa()">📂 Glisse un fichier ici ou clique</div>
<div class="pv" id="pv-cover"></div>
<label>2. Que veux-tu cacher ?</label>
<input type="radio" id="mt" name="mode" value="text" checked onchange="mode()"><label class="r" for="mt">Un texte</label>
<input type="radio" id="mf" name="mode" value="file" onchange="mode()"><label class="r" for="mf">Un fichier</label>
<div id="m-text"><textarea name="text" id="txt" rows="4" placeholder="Ton message secret" oninput="check()"></textarea></div>
<div id="m-file" hidden><div class="drop"><input type="file" name="secret_file" id="secret" onchange="picked(this,'pv-secret');check()">📎 Glisse le fichier a cacher</div><div class="pv" id="pv-secret"></div></div>
<div class="bar" id="cap" hidden><i id="bar"></i></div><small id="captxt"></small>
<label>3. Mot de passe</label>
<div class="pw"><input type="password" class="pwf" id="pw" name="password" required autocomplete="new-password">
<button type="button" class="ico" title="Afficher / masquer" onclick="togglePw()">👁</button>
<button type="button" class="ico" title="Generer un mot de passe fort" onclick="genPw()">🎲</button></div>
<label>Confirme-le <small>(une faute de frappe = message perdu)</small></label>
<input type="password" class="pwf" id="pw2" name="password2" required autocomplete="new-password">
<button class="go" id="gohide">🔒 Cacher et telecharger</button>
<p><small>Le fichier obtenu ressemble a l'original. Garde le mot de passe : sans lui, rien ne se retrouve.</small></p>
</form></section>

<section id="s-reveal"><form method="post" action="/reveal" enctype="multipart/form-data" onsubmit="return busy(this)">
<label>Fichier qui contient un message</label>
<div class="drop"><input type="file" name="stego_file" accept=".bmp,.png,.wav" required onchange="picked(this,'pv-reveal')">📂 Glisse un fichier ici ou clique</div>
<div class="pv" id="pv-reveal"></div>
<label>Mot de passe</label>
<div class="pw"><input type="password" class="pwf" name="password" required>
<button type="button" class="ico" title="Afficher / masquer" onclick="togglePw()">👁</button></div>
<button class="go">🔓 Extraire</button>
{% if revealed is not none %}<div class="out" id="res">{{ revealed }}</div>
<button type="button" class="ghost" onclick="copyOut()">📋 Copier le message</button>{% endif %}
</form></section>

<section id="s-analyse"><form method="post" action="/analyse" enctype="multipart/form-data" onsubmit="return busy(this)">
<label>Fichier a analyser</label>
<div class="drop"><input type="file" name="target" accept=".bmp,.png,.wav" required onchange="picked(this,'pv-analyse')">📂 Glisse un fichier ici ou clique</div>
<div class="pv" id="pv-analyse"></div>
<button class="go">🔍 Analyser</button>
{% if report %}
{% set icon = {'RAS':'✅','SUSPECT':'⚠️','TRES':'🚨'}.get(verdict.split()[0] if verdict else '', '') %}
<p>Verdict : <span class="badge {{ verdict.split()[0] if verdict else '' }}">{{ icon }} {{ verdict }}</span></p>
<div class="out">{{ report }}</div>
{% endif %}
</form></section>
<div id="toast"></div>

<script>
let maxBytes=null;const urls={};const $=id=>document.getElementById(id);
function toast(m){const t=$('toast');t.textContent=m;t.classList.add('show');setTimeout(()=>t.classList.remove('show'),2600)}
function tab(n){for(const t of ['hide','reveal','analyse']){
$('s-'+t).classList.toggle('on',t==n);$('b-'+t).classList.toggle('on',t==n)}}
function mode(){const f=$('mf').checked;$('m-file').hidden=!f;$('m-text').hidden=f;check()}
function fmt(n){return n<1024?n+' o':n<1048576?(n/1024).toFixed(1)+' Ko':(n/1048576).toFixed(1)+' Mo'}
function picked(inp,pv){
const box=$(pv);if(urls[pv]){URL.revokeObjectURL(urls[pv]);delete urls[pv]}
box.textContent='';const f=inp.files[0];if(!f)return;
const url=URL.createObjectURL(f);urls[pv]=url;const n=f.name.toLowerCase();let el=null;
if(/\.(png|bmp)$/.test(n)){el=document.createElement('img');el.src=url}
else if(/\.wav$/.test(n)){el=document.createElement('audio');el.controls=true;el.src=url}
if(el)box.appendChild(el);
const s=document.createElement('small');s.textContent=f.name+' · '+fmt(f.size);box.appendChild(s)}
async function capa(){
maxBytes=null;const f=$('cover').files[0];$('captxt').textContent='';
if(!f){$('cap').hidden=true;check();return}
const fd=new FormData();fd.append('cover',f);
try{const r=await fetch('/capacity',{method:'POST',body:fd});const j=await r.json();
if(r.ok){maxBytes=j.max}else{$('captxt').textContent='⚠️ '+j.error}}catch(e){}
check()}
function need(){if($('mf').checked){const f=$('secret').files[0];return f?f.size+f.name.length:0}
return new TextEncoder().encode($('txt').value).length}
function check(){const b=$('gohide');
if(maxBytes===null){b.disabled=false;return}
const n=need(),over=n>maxBytes,pct=maxBytes?Math.min(100,Math.round(100*n/maxBytes)):100;
$('cap').hidden=false;$('bar').style.width=pct+'%';$('bar').className=over?'over':'';
$('captxt').textContent='Place utilisee : '+fmt(n)+' sur '+fmt(maxBytes)+(over?' : trop gros, choisis un support plus grand':'');
b.disabled=over}
function togglePw(){for(const e of document.querySelectorAll('.pwf'))e.type=e.type=='password'?'text':'password'}
function genPw(){const A='abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789';
const r=crypto.getRandomValues(new Uint32Array(16));
const p=Array.from(r,x=>A[x%A.length]).join('').match(/.{4}/g).join('-');
$('pw').value=p;$('pw2').value=p;for(const e of document.querySelectorAll('.pwf'))e.type='text';
if(navigator.clipboard)navigator.clipboard.writeText(p).then(()=>toast('Mot de passe genere et copie : garde-le !'),()=>toast('Mot de passe genere : note-le !'));
else toast('Mot de passe genere : note-le !')}
function copyOut(){const t=$('res').textContent;
if(navigator.clipboard)navigator.clipboard.writeText(t).then(()=>toast('Copie !'),()=>toast('Copie impossible'))}
function busy(f){const p=f.querySelector('#pw'),q=f.querySelector('#pw2');
if(p&&q&&p.value!==q.value){toast('Les deux mots de passe sont differents');return false}
const b=f.querySelector('.go'),t=b.textContent;b.textContent='⏳ Un instant...';b.disabled=true;
setTimeout(()=>{b.textContent=t;b.disabled=false;check()},4000);return true}
tab('{{ tab }}');mode();
</script></main></body></html>
"""

if __name__ == "__main__":
    print(f"STEGO web : ouvre http://localhost:{PORT} dans ton navigateur (Ctrl+C pour arreter)")
    app.run(host="127.0.0.1", port=PORT)