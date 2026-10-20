# Building Gnomad

Gnomad builds the way Felucca does. The toolchain, the SDK files and `./build.sh` are all explained in Felucca's
[BUILDING.md](BUILDING.md), and you need everything it lists. This page only adds the Gnomad-specific steps.

The released package was built by continuous integration on Linux x86-64. That CI workflow isn't included in
this repository (see "CI" below), but every script it calls is here, in `.github/ci/`.

## 1. Prerequisites

Follow [BUILDING.md](BUILDING.md), "Prerequisites". You need Python 3 with Pillow and fontTools, the JieLi
toolchain (`tools/get_toolchain.sh`), and the three JieLi AC79 SDK files. On macOS you also need Docker. The
release was built with Pillow 12.3.0. `.github/actions/jieli-deps/action.yml` shows how CI fetched and pinned
the toolchain and the SDK.

## 2. The build's name

The name the synth shows comes from `build/gen/gn_name.h`. Release 12 was built with this:

```
mkdir -p build/gen
printf '#define FELUCCA_VERSION "%s"\n' "GNOMAD V1 115e11" > build/gen/gn_name.h
```

The `FELUCCA ` prefix that the editor's INFO reply carries is added by `firmware/src/editor.c`. Felucca's
installer page checks for it. For a build of your own, use a name of your own, for example
`GNOMAD V1 mine`. INFO has room for 24 characters, prefix included.

## 3. Build

```
./build.sh
python3 .github/ci/build_info.py --engines     # checks the shipping configuration as compiled, prints sizes
```

Don't set any `FELUCCA_*` variable. The release used the defaults in `firmware/src/felucca.c`, and Gnomad's
engines are on by default (`firmware/src/gn_engines.h`). The output is `build/felucca.fwsc` (the app plus a
loader built here) and `build/felucca.bin`.

## 4. The package to install (with Felucca's released update loader)

The released Gnomad package isn't `build/felucca.fwsc`. It's the Gnomad app packed with the **update loader
from Felucca 1.0.1's published package**, so every install runs Felucca's own released loader:

```
mkdir -p build/upstream
curl -fsSL -o build/upstream/felucca-1.0.1.fwsc \
  https://github.com/hugelton/Felucca/releases/download/v1.0.1/felucca-1.0.1.fwsc
python3 .github/ci/released_package.py build/upstream/felucca-1.0.1.fwsc \
  7ff22a3532e75b881de0f879b5103387fb666980766f1768d8b8012e067e8fae 609849 \
  e671adf0ce6e4f69ac8514087c8833b1969fa4c470645786f19968e90b9a630f \
  build/gnomad-mine.fwsc build/felucca-1.0.1-loader-ota.bin
cmp build/loader/ota.bin build/felucca-1.0.1-loader-ota.bin   # the loader built here is the released one
```

`released_package.py` checks the published package's size and SHA-256, reads it the way the device does
(`tools/gn_package_check.py`), extracts the loader and checks it against the pinned SHA-256. Then it packs it
with your app.

We haven't confirmed that you get a byte-identical copy of the released `.fwsc`. The package carries the
build's name and depends on the toolchain and Pillow versions. The source here is exactly what release 12 was
built from.

## 5. Tests

```
tests/run_tests.sh          # Felucca's host tests (see BUILDING.md; CI runs them on a GN_ENGINES 0 build)
tests/run_fork_tests.sh     # Gnomad's tests (needs the package from step 4)
tests/gn_skin_tests.sh      # the Advanced skin's tests (need the base's sources; see FORK.md)
```

FORK.md, section "CI", describes exactly which build each runner is used against.

## CI

The release's workflow (`.github/workflows/firmware.yml`) is in the release's source tarball, not in this
repository. **If you add CI to a fork of this repository, use GitHub-hosted runners** (for example
`runs-on: ubuntu-24.04`). Never use a self-hosted runner on a public repository, because a pull request
could run code on that machine. The steps are the ones above, in that order. Workflows also need the
package-reading and budget scripts in `.github/ci/`, which are included.
