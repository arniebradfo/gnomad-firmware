#!/usr/bin/env python3
"""The folder the app installs from: one package, its source, its licences and a signed manifest, side by side.

  gn_release_site.py TAG PACKAGE SOURCE LICENCE_DIR OUT_DIR

Environment: GN_VERSION (the build's name, e.g. "GNOMAD V1 c40a23"), UPSTREAM_TAG, GN_TITLE (what a person reads,
e.g. "Gnomad V1"), GN_MIN_APP (oldest app that may install it), GN_SITE_BASE (the https folder the site will be
served from; empty: no public host yet), GN_MANIFEST_KEY (the Ed25519 private key, raw 32 bytes in base64, from
CI's secrets; empty: not signed).

Writes OUT_DIR/ with the package, the source archive, licences/ (index.html, LICENSE, LICENSING.md, LICENSES/) and
manifest.json in the app's format (gnomad-app InstallerKit FirmwareManifest, format 1). With a host and a key it
also writes manifest.json.sig: the Ed25519 signature of manifest.json's exact bytes, base64, which the app checks
against the public keys it carries. Without either, the manifest is a preview: its URLs point at
https://firmware.invalid/ (which never resolves) and there is no signature, so no app accepts it.
"""
import base64
import hashlib
import html
import re
import json
import os
import shutil
import sys

BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
PREVIEW_BASE = "https://firmware.invalid/gnomad/"


def product_of(raw):
    """the identity the package carries (FACTS-OTA section 3): one marker per block of the first 20"""
    return "".join(chr((raw[i * BLK + KEEP] - i - 1) & 0xFF) for i in range(BLOCKS) if raw[i * BLK + KEEP] != 0x7D)


def releases(path):
    """[(number, INFO)] from releases.txt, numbers strictly increasing"""
    out = []
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        m = re.fullmatch(r"(\d+) (FELUCCA .+)", line)
        if not m or (out and int(m[1]) <= out[-1][0]):
            sys.exit(f"gn_release_site: bad line in {path}: {line!r}")
        out.append((int(m[1]), m[2]))
    return out


def write_licence_index(lic):
    """licences/index.html: the manifest's licence_url is this folder and a static host lists no folders"""
    texts = sorted(n for n in os.listdir(os.path.join(lic, "LICENSES")) if n != "index.html")
    link = lambda href, text: f'<li><a href="{html.escape(href)}">{html.escape(text)}</a></li>'
    body = ('<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width">'
            '<title>Gnomad licences</title><h1>Gnomad licences</h1>'
            '<p>Gnomad is free software under the GNU General Public License, version 3 only. Its complete source '
            'is the source archive published beside the firmware.</p><ul>'
            + link("LICENSE", "LICENSE (GPL-3.0)") + link("LICENSING.md", "LICENSING.md (what is under which licence)")
            + "".join(link("LICENSES/" + n, n) for n in texts) + "</ul>\n")
    open(os.path.join(lic, "index.html"), "w", encoding="utf-8").write(body)


def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def main(tag, package, source, licences, out):
    env = os.environ
    version, upstream = env["GN_VERSION"], env["UPSTREAM_TAG"]
    title, min_app = env.get("GN_TITLE") or "Gnomad", env.get("GN_MIN_APP") or "0.1.0"
    base, key = env.get("GN_SITE_BASE", "").strip(), env.get("GN_MANIFEST_KEY", "").strip()
    if base and not (base.startswith("https://") and base.endswith("/")):
        sys.exit(f"gn_release_site: GN_SITE_BASE must be an https folder ending in '/': {base!r}")
    signing = bool(base and key)
    base = base or PREVIEW_BASE

    raw = open(package, "rb").read()
    product = product_of(raw)
    if product != "FM-1_900":
        sys.exit(f"gn_release_site: the package says {product!r}, not FM-1_900")

    os.makedirs(out, exist_ok=True)
    pkg_name, src_name = f"{tag}.fwsc", os.path.basename(source)
    shutil.copyfile(package, os.path.join(out, pkg_name))
    shutil.copyfile(source, os.path.join(out, src_name))
    lic = os.path.join(out, "licences")
    os.makedirs(lic, exist_ok=True)
    for f in ("LICENSE", "LICENSING.md"):
        shutil.copyfile(os.path.join(licences, f), os.path.join(lic, f))
    shutil.copytree(os.path.join(licences, "LICENSES"), os.path.join(lic, "LICENSES"), dirs_exist_ok=True)
    write_licence_index(lic)

    info = f"FELUCCA {version}"
    past = releases(os.path.join(os.path.dirname(os.path.abspath(__file__)), "releases.txt"))
    known = [n for n, i in past if i == info]
    if known:                                # a rebuild of a published release: its own number, the earlier ones
        number = known[0]
        past = [(n, i) for n, i in past if n < number]
    else:
        number = (past[-1][0] if past else 0) + 1
    entry = {
        "name": pkg_name,
        "url": base + pkg_name,
        "size": len(raw),
        "sha256": hashlib.sha256(raw).hexdigest(),
        "product": product,
        "info": info,
        "release": number,
        "supersedes": [i for _, i in past],
        "title": title,
        "based_on": f"Felucca {upstream.removeprefix('v')}",
        "minimum_app_version": min_app,
        "source_url": base + src_name,
        "source_sha256": sha(source),
        "licence_url": base + "licences/",
    }
    body = (json.dumps({"format": 1, "packages": [entry]}, indent=2) + "\n").encode()
    open(os.path.join(out, "manifest.json"), "wb").write(body)

    if signing:
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
        k = Ed25519PrivateKey.from_private_bytes(base64.b64decode(key))
        sig = k.sign(body)
        k.public_key().verify(sig, body)
        open(os.path.join(out, "manifest.json.sig"), "w").write(base64.b64encode(sig).decode() + "\n")
        from cryptography.hazmat.primitives import serialization as s
        pub = base64.b64encode(k.public_key().public_bytes(s.Encoding.Raw, s.PublicFormat.Raw)).decode()
        print(f"gn_release_site: signed with the key whose public half is {pub}")
    else:
        why = "no GN_SITE_BASE (no public host yet)" if base == PREVIEW_BASE else "no signing key"
        print(f"gn_release_site: PREVIEW, not signed: {why}; no app accepts this manifest")
    print(f"gn_release_site: {pkg_name} {entry['size']} B sha256 {entry['sha256']} info {entry['info']!r} release {number} supersedes {len(past)}")


if __name__ == "__main__":
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    main(*sys.argv[1:])
