#!/usr/bin/env python3
"""The GPL source offer for one build: the complete source of the commit, beside its package.

  gn_source.py TAG SITE_DIR OUT_DIR

Writes OUT_DIR/TAG-source.tar.gz (`git archive` of HEAD: firmware, tools, web, tests, .github and every gn_* file,
everything the build reads from this repository) and puts a copy, with SOURCE.txt, in the installer site's
`firmware/` folder next to the package and the licence texts. The site's pages credit upstream with a link to
github.com/hugelton/Felucca; that link stays, and a link to this build's own source is added after it, so the person
who gets the package gets its source from the same place (GPL-3.0 section 6(d)). Fails unless the archive holds the
files a rebuild needs and at least one page got the link.
"""
import hashlib
import io
import os
import subprocess
import sys
import tarfile

UPSTREAM_LINK = 'github.com/hugelton/Felucca</a>'
NEEDED = ["build.sh", "BUILDING.md", "LICENSE", "LICENSING.md", "FORK.md", "firmware/src/felucca.c",
          "firmware/app.ld", "tools/build.py", "web/make_site.py", ".github/workflows/firmware.yml"]


def main(tag, site, out):
    name = f"{tag}-source.tar.gz"
    path = os.path.join(out, name)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    # gzip without a timestamp and a fixed mtime from the commit: the same commit gives the same bytes
    subprocess.check_call(["git", "-c", "tar.tar.gz.command=gzip -cn", "archive", "--format=tar.gz",
                           f"--prefix={tag}-source/", "-o", path, commit])
    data = open(path, "rb").read()
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as t:
        names = {n.split("/", 1)[1] for n in t.getnames() if "/" in n}
    missing = [n for n in NEEDED if n not in names]
    if missing:
        sys.exit(f"gn_source: the archive lacks {', '.join(missing)}")
    sha = hashlib.sha256(data).hexdigest()

    fw = os.path.join(site, "firmware")
    os.makedirs(fw, exist_ok=True)
    with open(os.path.join(fw, name), "wb") as f:
        f.write(data)
    with open(os.path.join(fw, "SOURCE.txt"), "w") as f:
        f.write(f"""This firmware is free software under the GNU General Public License, version 3 only (LICENSE).

Its complete corresponding source is {name}, in this folder
(SHA-256 {sha}), commit {commit}.
It is Felucca by Leo Kuroshita / Hugelton Instruments (https://github.com/hugelton/Felucca), with the changes
listed in FORK.md. BUILDING.md and FORK.md say how to build it; the three JieLi AC79 SDK files the package
holds (Apache-2.0) come from https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK, as LICENSING.md describes.
The licences of every part are in LICENSE, LICENSING.md and LICENSES/.
""")

    linked = 0
    for root, _, files in os.walk(site):
        for fn in files:
            if not fn.endswith(".html"):
                continue
            p = os.path.join(root, fn)
            html = open(p, encoding="utf-8").read()
            if UPSTREAM_LINK not in html:
                continue
            rel = os.path.relpath(os.path.join(fw, name), root).replace(os.sep, "/")
            src = os.path.relpath(os.path.join(fw, "SOURCE.txt"), root).replace(os.sep, "/")
            add = (f'{UPSTREAM_LINK} &middot; this build\'s source: <a href="{rel}" style="color:inherit">'
                   f'{name}</a> (<a href="{src}" style="color:inherit">about</a>)')
            open(p, "w", encoding="utf-8").write(html.replace(UPSTREAM_LINK, add, 1))
            linked += 1
            print(f"gn_source: linked from {os.path.relpath(p, site)}")
    if not linked:
        sys.exit("gn_source: no page of the site links upstream's source; the offer would be unlinked")
    print(f"gn_source: {name} {len(data)} B sha256 {sha}")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
