# Gnomad: the changes made to Felucca

Gnomad is a modified version of [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton
Instruments (GPL-3.0-only), the open firmware for the M-VAVE FM-1. This file is the record of the changes: what was
changed, in which files, and when (GPL-3.0 section 5(a)). The changes start from upstream's tag `v1.0.1` (commit
`20c275e`, released 2026-10-05) and were made from 2026-10-05 on; each section below names its round, its date where
it has one, and the files it touches. The changes are Copyright (C) 2026 Kyle Palaniuk and, like Felucca, licensed
under the GNU General Public License, version 3 only (`LICENSE`). The fork keeps taking upstream's tagged releases,
so these rules keep the two easy to merge. Remotes: `upstream` is hugelton/Felucca (fetch only), `origin` is the
development repository.

The fork's product name is **Gnomad**. Its own files use the prefix `gn_` (`tools/gn_*`,
`tests/gn_*`), and its SysEx tag (rule 4) is "GN", `47 4E`.

## Base: upstream's tags

The fork is built on upstream's **releases**, not on upstream's daily `main`. This base is upstream's
tag `v1.0.1` (commit `20c275e`, released 2026-10-05) plus only what makes it ours to build on:

1. CI (`.github/`), which builds the tag as upstream's `BUILDING.md` says and runs upstream's tests
   unchanged, then ours;
2. the package to install, made with upstream's released update loader, and its verifier;
3. the build's name, one constant;
4. this file and the tag-based sync.

`git diff v1.0.1 -- firmware/` is the build's name in `felucca.c` and, on `gn/skin-1.0`, the Advanced skin
(below: our files and guarded lines in the UI): nothing of ours runs in the update path, and in the start-up
path only `palette_set`'s few assignments when GNOMAD is the saved palette. The tag and its commit are pinned in
`.github/workflows/firmware.yml` (`UPSTREAM_TAG`, `UPSTREAM_COMMIT`), together with upstream's
published package for that tag (name, size, SHA-256 from the release page) and the SHA-256 of the
update loader inside it.

## Rules

1. **Upstream's interface stays untouched**: the editor protocol (`web/EDITOR_PROTOCOL.md`),
   parameter and preset numbers, the flash formats (settings, presets, projects, samples), the
   package format and the build interface (`build.sh`, the `FELUCCA_*` flags). Upstream's
   installer, editor and saved data must keep working with our firmware.
2. **Our additions live in new files**, and behind build flags (like `FELUCCA_*` in
   `firmware/src/felucca.c`) where possible. When an upstream file must change, keep the edit
   small and say why in the commit message.
3. **General features are offered to upstream.** If an addition would help every Felucca
   user, it is offered to hugelton/Felucca. Nothing is sent upstream automatically.
4. **Editor protocol**: upstream's commands, their numbers and the `INFO` reply are never
   reused, renumbered or extended by us (open upstream pull requests and other forks already
   contend for the next numbers). Our own commands travel under a separate SysEx header of
   ours (`F0 7D` plus a two-byte tag that is not `46 4C`), with their own numbering and their
   own hello message. Upstream's parser ignores frames it does not know, so its editor keeps
   working.
5. **The USB port name** was "Felucca" (`firmware/src/usb.c`) until 2026-10-11: the
   app now names itself "Gnomad Synth V1"; the update loader keeps "Felucca Update" (Branding, round G).
6. **Firmware is built only in CI**: `.github/workflows/firmware.yml` on x86-64 Linux
   runners, never on a developer's machine. Every push to `main` and every pull request builds the
   package and runs the host tests. The packages are on the run page as artifacts.

## CI

- **The runner.** The firmware jobs run on a self-hosted Linux runner (labels self-hosted, linux, x64, gnomad;
  since 2026-10-12): every `git worktree add` is preceded by `git worktree prune` and the target removed, and a last step
  (`if: always()`) prunes again, since a self-hosted checkout keeps `.git` between runs while the runner empties
  `RUNNER_TEMP`.
- **The commit it names.** A pull request builds its head itself, not GitHub's merge of it; every
  name, the build's name and every artifact name come from the commit checked out
  (`git rev-parse HEAD`), and the build fails if the tree differs from it before or after the build.
- **The build** is upstream's: `./build.sh` with Pillow (pinned) and fontTools, the JieLi toolchain
  and the three AC79 SDK files from `.github/actions/jieli-deps` (pinned by SHA-256 and commit), no
  `FELUCCA_*` variable set. `.github/ci/build_info.py` fails the run unless the options the update
  path depends on are, as compiled (JieLi's preprocessor on `felucca.c` with the build's flags):
  `FELUCCA_OTA 1`, `FELUCCA_OTA_DRYRUN 0` (an update commits), `FELUCCA_OTA_RAMONLY 0` (an
  interrupted install resumes at the next power-on), `FELUCCA_FLASH 1`, identity `FM-1_900`, and
  the build's name; it also prints the image size and, from the linker's symbols, `.data+.bss`,
  the pool and `.noinit` against their regions.
- **The name.** CI writes `build/gen/gn_name.h`: `#define FELUCCA_VERSION "<GN_NAME> <milestone>
  <commit>"`, which `felucca.c` includes when no `FELUCCA_VERSION` is given (a `--release` build passes its
  own). The product's name is `GN_NAME` in `firmware/src/gn_hooks.h`, the one place it is written (Easy's
  screens show it too); the milestone is `GN_MILESTONE` in the workflow (`V1` on this line since Branding round V1.6); the
  commit gets as many hex digits as fit, 6 at most and 4 at least, in the 24 characters the editor's
  `INFO` sends (`ed_str(.., 24)`): `FELUCCA GNOMAD EASY 1a2b` here, `FELUCCA GNOMAD M0 1a2b3c` on the base.
  It starts with the `FELUCCA ` that 1.0.1's installer page checks. The ABOUT page and the console show
  the part after `FELUCCA`.
- **How the synth starts and installs** stays upstream's bytes: the step of that name fails the run
  when any of `firmware/src/main.c`, `ota.c`, `ota_hw.c`, `usb.c`, `libc.c` (the loader builds from it),
  `firmware/crt0.S`, `firmware/app.ld`, `firmware/loader/`, `firmware/hal/`, `build.sh`,
  `tools/build.py`, `tools/fm1pkg_make.py` or `tools/fm1_install.py` differs from the pinned tag's, an
  added file among them too (before, the diff against the tag was only recorded). The one exception is the
  branding lines (Branding, rounds G and V1.6): `.github/ci/gn_protected.py` allows exactly those whole lines and
  nothing else; its own test runs first in the step.
- **Budgets.** The base (`GN_BASE_COMMIT`) is built in the same job and `.github/ci/gn_budget.py`
  compares the two; a figure over the allowance this file states for the milestone (the `gn-budget`
  lines under Easy's Budgets), or a milestone with no line, fails the run.
- **Tests.** Upstream's `tests/run_tests.sh`, unchanged, then ours, `tests/run_fork_tests.sh` (Gnomad engines: upstream's
  runs against this commit built with the engine swap off, `GN_ENGINES 0` in a worktree, since its tests describe
  upstream's engines; ours, and the target cost check of the shipping build, against the shipping build; each runner
  under `sh -x`, so `.github/ci/gn_test_summary.py` names every failing test at the end of its log, even one that prints
  no FAIL line): the
  package verifier against malformed packages, upstream's own update tests (`ota_test`,
  `ldr_test`) on the package to install, the `INFO` length, and the ABOUT page drawn with the
  build's name passing upstream's layout lint; then the skin's, `tests/gn_skin_tests.sh` (below), with
  the base's sources checked out by `GN_BASE_COMMIT`.
- **The package to install** is `gnomad-<milestone>-<commit>-released-loader.fwsc`: this app with
  the update loader of upstream's released package (installing a package runs that package's own
  loader, so it is always upstream's released one). `.github/ci/released_package.py` downloads the
  released package, checks its published size and SHA-256, reads it as the device does
  (`tools/gn_package_check.py`: header and list CRCs, entry types and the exact type/name pairs,
  bounds, overlaps, data CRCs, the loader checks of `ota.c`, the bytes the device would select),
  extracts the loader the device would select, checks it against the pinned SHA-256, and packs it
  with this app as `tools/build.py` packs the normal package; the result is read back the same way.
  Not uploaded: `build/felucca.fwsc` and `build/loader/ota.bin`, whose update loader was built here.
- **Artifacts**, every name starting with `gnomad-<milestone>-<commit>`: the package to install,
  its installer page as a folder (`-installer-site`: serve it with `python3 -m http.server` and
  open it on localhost), the app, upstream's released loader as pinned, the generated headers
  (`build/gen`), the screens (upstream's renders of every screen, and the ABOUT pages with the
  build's name), the skin's screens (`-skin-screens`: GNOMAD's screens, their 2x copies and the
  labelled sheet), a manifest (commit, base, name, options, sizes, our `firmware/` changes, SHA-256
  values) and the logs. The job summary lists each with its SHA-256.
  A run red only on the shipping build's target cost still uploads the package, the sites, the source, the app and the
  loader (the step "The package's gate"); any other red step withholds them (round V16).
- **Reproduction.** A run started by hand (`reproduce`, on by default) also builds upstream's tag as
  its release was built (`./build.sh --release 1.0.1`) and compares the package, the app and the
  loader with the published files; it only reports, and uploads only the report.
- **The release site** (`-release-site`, `.github/ci/gn_release_site.py`): the package, its source, `licences/` (with an
  `index.html`, since the manifest's `licence_url` is that folder and a static host lists none) and the app's
  `manifest.json`; signed (`manifest.json.sig`) only by a run started by hand with `release` on, from the repository
  variable `GN_SITE_BASE` and the secret `GN_MANIFEST_KEY`. `.github/ci/releases.txt` lists the builds the app may update
  (2026-10-13: every build installed on a test synth since V1.5, 2 to 7, `FELUCCA GNOMAD V1 1bd443` to `... V1 f3b245`), so
  the next build is release 8 and supersedes all seven; the published build's own line is added when it is published.

## Touch list

Every upstream file this fork edits, and why. Keep it current: a commit that edits an upstream
file adds or updates its line here. Files that are entirely ours (`FORK.md`, `.github/` except
`FUNDING.yml`, `tests/run_fork_tests.sh` and every `gn_*` file) are not listed.

| Upstream file | Why |
| --- | --- |
| `firmware/src/felucca.c` | the build's name: `FELUCCA_VERSION` from `build/gen/gn_name.h` when CI wrote it and nothing else defines it (5 lines, before upstream's default) |
| `firmware/src/felucca.c` | Easy (2 lines): `#include "gn_hooks.h"` just before `seq.c` (the fork's flag `FELUCCA_GN` and the hooks' declarations, before the first hooked file) and `#include "gn_fork.c"` just before `main.c` (the fork's code, after everything it hooks into) |
| `firmware/src/seq.c` | Easy (4 lines), `keyboard_block`: after Felucca's key map, unless the DRUM grid or NAME has the keys, `kb_note[k] = gn_key(...)`: the note Easy's key plays, or `KB_SILENT` for a key that rests. `gn_key` returns the note unchanged unless Easy is on; it is out of line (one call, only when a key goes down) |
| `firmware/src/seq.c` | Easy (3 lines, round 14), `rec_note`: before a recorded note becomes a hold, `if (gn_rec_hit(t)) return;`: while the track holds Easy's kit the note is a hit, never held, so it never ties over the steps after it (the DRUM engine's own recording never holds). `gn_rec_hit` is 0 unless Easy is on |
| `firmware/src/seq.c` | Round 19 (11 added lines, one condition extended): guarded hooks at `seq_stop`, block-start USB configuration observation, and the existing per-track panic. These are audio-boundary events that the UI cannot reliably observe; `gn_state.c` owns the cleanup. No `voice.c` or `usb.c` change. |
| `firmware/src/ui_input.c` | Easy (8 lines): first in `ui_input`, `if (gn_input()) return;` (Easy has the panel, or the hold that enters it was just completed); in `ui_leds`, after `led_pos_init`, `if (gn_leds()) return;` (Easy's LEDs). Both return 0 while Easy is off |
| `firmware/src/audio.c` | Round 26 (3 lines): in `audio_block`, after `uac_tap` (the USB audio input's copy of the master output) and before the 24-bit shift, `gn_click(out, n)`: the recording's metronome added to what the codec plays only, so the USB audio never carries it. One out-of-line call a block; it returns at once unless a click is due or sounding (never while the record has no Gnomad marker: upstream's tests, another firmware's record) |
| `firmware/src/ui_draw.c` | Easy (4 lines): in `ui_draw`, after `ui.frame++`, `if (gn_draw()) return;` (Easy's screens). 0 while Easy is off and during main.c's update countdown |
| `firmware/src/project.c` | Round 27 (F15, 7 lines): in `project_save_as`, after a save on flash, and in `project_rename`, after a rename on flash, `gn_project_saved(slot)`: the slot written is noted as the last save (Easy's record of it is written at the next input call, `gn_easy.c gn_last_keep`), so power-on restores it. It returns at once with `GN_START_EASY` 0 (upstream's tests) and for Easy's own save (which writes its own record) |
| `firmware/src/main.c` | Branding, round G (3 lines, text only): the splash `GNOMAD` / `SYNTH V1`, the crash page's title `GNOMAD CRASH`. A protected file: the gate allows exactly these lines |
| `firmware/src/felucca.c` | Branding, round V1.6 (7 lines around `#include "usb.c"`): `STR1 -> GN_STR1` saved (`#pragma push_macro`), upstream's shared manufacturer array renamed `STR1_UPSTREAM` (unused, dropped by the linker) for the app only, a size check that `get_desc` sends the 14-byte `Gnomad`, `#undef STR1` after. loader.c and upstream's host tests include usb.c without these lines |
| `firmware/src/usb.c` | Branding, rounds G and V1.6 (1 line): the app's USB strings, manufacturer `Gnomad` (`GN_STR1`, after `_Pragma("pop_macro(\"STR1\")")`, which points `get_desc`'s `STR1` at it) and product `Gnomad Synth V1` (STR2), in the `#else` of `#ifdef FELUCCA_LOADER`; the loader's `Hügelton Instruments` / `Felucca Update` and every other byte unchanged. A protected file: the gate allows exactly this line |
| `firmware/src/ui_menu.c` | Branding, rounds G and V1.6: ABOUT's top shows GNOMAD, "Gnomad Synth V1", the version and the QR to https://gnomad.cc, nothing of Felucca; CREDITS opens with `Includes code from Felucca by Leo Kuroshita, GPL-3`, then the third-party notices (Dexed + msfa / FM6 dropped: FM6 is retired); Felucca's own credit block and the Hügelton copyright / URL lines dropped |
| `firmware/src/editor.c` | Easy (6 lines): in `ed_handle`, after `ed_begin(cmd)` and before any command runs, `gn_editor(cmd, a, na)`: what the command may write is no longer Easy's (a write of steps, pattern settings, motion, a song, a project or a backup drops Easy's undo copy; a sound or project load ends Easy's ownership of the sound it reaches). In `ED_PROJECT`, after the save (a failed save returns before it), `gn_project(op, slot)`: the save read back from flash for the "saved, safe to unplug" screen. Both return at once while Easy is off |
| `firmware/src/felucca.c` | Gnomad engines (1 line): `#include "gn_engines.h"` after the build options, before the HAL and `core.h`: the engine swap's options (`GN_ENGINES`, the retired engines), decided before any engine is compiled. Only felucca.c includes it, so upstream's host tests build Felucca's engines as before |
| `firmware/src/core.h` | Gnomad engines: `#include "gn_size_flags.h"` (gn/size-plan, a413386: the `GN_DROP_*` flags, default 0, and `GN_ENGINES` 0 unless felucca.c set it) and `NENG_SHOWN` less `GN_ENGINES` (slot 8 is not offered with the swap) and `GN_GONE_N` (the retired slots, UX-L) |
| `firmware/src/engines.c` | Gnomad engines: `#include "gn_size_stub.h"` (a413386: the retired engines' silent descriptors); with `GN_ENGINES` `#include "gn_core.c"` before the table and the six retired slots through `GN_SLOT(felucca, gnomad)` (7 GNOME, 8 SAMPLER not offered, 9 VA, 10 DRUMS, 12 FM, 13 SAMPLER; the numbers the stores and the protocol hold do not move, `NENGINES` stays 14), `ENGINE_ORDER`'s Gnomad branch (unsized, `_Static_assert` that it lists `NENG_SHOWN` engines), `eng_ok` refuses slot 8, `eng_rank` shows it as 13; `eng_gone` and the retired engines out of `ENGINE_ORDER`'s Gnomad branch and `eng_ok` (UX-L) |
| `firmware/src/upreset.c` | Gnomad engines (2 lines, round UX-L): `up_auto_name` and `up_engine` keep a record stored on a retired slot (`eng_gone`) on that slot, as before UX-L, instead of DIGITAL's way to FM6; round EW-FIXES (one `#if GN_ENGINES` line in `up_load`): a Gnomad user preset binds the record it was saved from (`gna_up_preset`) |
| `firmware/src/fx.c` | Gnomad engines (3 lines), `mix_block`: after Felucca's parts, `gn_core_mix(mix_l, mix_r, n)`: the core's mono block into the dry mix, before the FX layer and the master |
| `firmware/src/audio.c` | Gnomad engines (3 lines), `audio_init`: `gn_core_boot()` before `fm1_audio_init` (the core's arenas set up before the audio interrupt runs) |
| `firmware/src/ui_graph.c` | Gnomad engines: FM6's three index checks (`ENGI_FM6`) also need `!GN_DROP_FM6` (slot 12 is Gnomad's FM with the swap); a413386's guards (retired engines' graphs and the waveform cache) |
| `firmware/src/ui_slice.c` | a413386's guard (`FELUCCA_SLICE && !GN_DROP_SLICE`); with SLICE retired, `slice_count`/`slice_sel` return 0 (the simulator's page map calls them) |
| `firmware/src/editor.c` | Gnomad engines (4 lines, "INFO and DESC"), `ED_INFO`: with `GN_ENGINES` the names are `N_ENGNAME`'s, DESC's (UX-L; `"-"` on 1 and 8, so INFO names no engine twice); upstream's line in the `#else`. `ED_NAMES`: 0 presets on a retired slot with `GN_EDITOR_HIDES_GONE` (UX-L, off) |
| `firmware/src/params.c` | Gnomad engines (6 lines, "INFO and DESC"): with `GN_ENGINES`, `N_ENGNAME` (DESC of global 20, "ENG") is the swap's fourteen slot names, INFO's (a retired engine's `""` with `GN_EDITOR_HIDES_GONE`, UX-L); upstream's list in the `#else` |
| `firmware/src/eng_analog.c`, `eng_phase.c`, `eng_lofi.c`, `eng_formant.c`, `eng_trio.c`, `eng_wheel.c`, `eng_grain.c`, `eng_phys.c`, `eng_noise.c`, `eng_slice.c`, `eng_sample.c`, `eng_drum.c`, `eng_fm6.c`, `fm4_convert.c`, `fm6_bank.c`, `editor.c`, `editor_backup.c`, `project.c`, `ui.c`, `ui_draw.c`, `ui_input.c`, `icons.c` | gn/size-plan (a413386), brought for the engine swap: each engine and the shared code that only it uses behind its `GN_DROP_<ENGINE>` flag (default 0: unchanged); a retired engine leaves a silent descriptor in its slot and the stubs other files call (FM6: constant empty patch replies through the existing result codes); no protocol command, field, ID or record size changes |
| `tests/target_budget.py` | Gnomad engines (about 20 lines): when `tests/gn_target_budget.py` is in the tree, the functions of the engines the build compiled out (by the build's own configuration, `build/gn_engines.txt`, which `build_info.py --engines` writes) are printed "retired" instead of failing, and after upstream's lines the fork's check runs (the core's functions, each slot against the retired engine's lines, the audio interrupt with the core's per-block work). Upstream's lines, rule and tolerance are unchanged |
| `firmware/src/eng_sample.c` | Gnomad engines (6 lines): with `GN_ENGINES`, `pow2_q16` and `sample_next` stay calls (`noinline`): with GRAIN and SLICE retired SAMPLE is their only caller, the compiler inlined them, and `sample_render`'s loop then carried their per-sample work that V1's target budget line (126) never counted (CI run 38079745344: 1,106) |
| `firmware/src/ui_menu.c` | Gnomad engines (10 lines, re-vendor of gnomad-core 0f72e43): two entries in ABOUT's third-party notices (`MENU_CREDITS`, after Rings): the Gnomad core's FM kernel from msfa (Google Inc., Apache-2.0) and its drum voices (Emilie Gillet, MIT), as the core's NOTICE asks. The ABOUT document changes, so the skin's inert test leaves exactly ABOUT's 4 x 8 renders out (`tests/gn_skin_tests.sh`, the same lines as Branding round G on gn/gnomad-1.0) |
| `LICENSING.md` | Gnomad engines (2 rows, same re-vendor): the Gnomad core's adapted msfa (Apache-2.0) and drum code (MIT) in the third-party table |
| `firmware/src/ui.c`, `ui_graph.c`, `ui_draw.c`, `console.c`, `editor.c`, `tests/hostsim.c` | Gnomad engines (round FW-CONTENT-7, one expression a reader): a preset is read through `GN_PRESET` / `GN_PRESET_NAME` / `GN_PRESET_PAT` (`gn_size_flags.h`): without `GN_ENGINES` upstream's `&e->presets[k]`, `.name`, `.pat`; with it the Gnomad engines' records (they keep no `preset_t` table). In ui.c `apply_preset_to` reads the fields through one pointer, `pr` |
| `tools/gen_samples.py` | a413386: `felucca_samples.h` puts the bundled audio, zone extents and BREAK behind `GN_DROP_SAMPLES` (indices and metadata kept); its summary line says the block is generated always and compiled in only with `GN_DROP_SAMPLES` 0 |
| `tools/size_fns.py` | Size diet (4 lines): when `tools/gn_size_fns.py` is in the tree, the functions its `gn_minsize` names get `minsize` too (with the engine swap: every function the two interrupts cannot reach, outside the protected files, the core and gn_core.c; without it none). `SIZE_FILES`, the rule and the output are upstream's |
| `firmware/src/params.c` | Round EW (7 lines, `GN_ENGINES` only), `page_desc`: on a Gnomad sound a page's knob asks `gn_page_slot` (gn_core_pages.c) what it does there: EDIT 1 the sound's four knobs where their amounts are kept, EDIT 2 the engine's four, a knob with nothing to do no descriptor (inert; the skin says why). Upstream's path unchanged otherwise |
| `firmware/src/ui.c` | Round EW (4 lines, `GN_ENGINES` only), `page_visible`: SLICER and MOD are not offered on a Gnomad sound, ENV DEST and LFO DEST only where a knob of theirs plays (`gn_page_shown`) |
| `firmware/src/ui_draw.c`, `ui_graph.c`, `ui_input.c` | Round EW (`GN_ENGINES` only): the MIXER page has no PAN on the mono core (SPEC/01 D03): its card is a knob with nothing to do (the skin says "Mono: no pan"), its graph draws no pan knob, KNOB 2 writes nothing; upstream's lines in the `#else` |
| `firmware/src/editor_fm6.c` | Size diet (3 lines, 2 expressions): factory patch k is read through `FM6_FACTORY_ROW(k)`, upstream's `FM6_FACTORY[k]` unless the retired FM6's stub (`gn_size_fm6.h`) defines it as `FM6_INIT`'s 128 zeros (the 1,024 B all-zero table it replaces held the same bytes) |

**Not a touch, but bytes of upstream's record we use.** Easy keeps the mode the synth was left in (round 7,
pick 1) in favourites' row 15, byte 30 of the settings record (`persist_t`), and (round 8) its song's slot in
byte 29 (`0x80 | slot`) with a fingerprint of what it last wrote there in bytes 25 to 28: no engine is number
15, so 1.0.1 never reads, writes or validates them, and keeps them as saved. Round 9 moved the song's record
to bytes 17 to 29: byte 29 the slot (`0x80 | slot`), 25 to 28 the storage's commit sequence of Easy's write
of it, 17 to 24 a 64-bit fingerprint of that write's payload (its trailing checksum left out). Upstream keeps its own `layer_seen` beside it
(`ui_layer.c`, row 15 byte 31), so a later tag could take ours: `tests/gn_mode_byte_test.py` (in
`tests/run_fork_tests.sh`, so an upstream-sync pull request goes red) fails when upstream's sources index the
factory rows in any way 1.0.1 does not, name row 15 or its bytes 17 to 30, change the rows' layout, or let `NENGINES`
reach 15 (`gn_easy.c` asserts the same at compile time). Every mode switch queues the normal settings save;
the first power-on's byte is written in RAM only (no save at start-up) and goes to flash with the next
settings save, the editor's UI save among them (`ed_ui_save` saves before it compares the record's
favourites, so it never answers "not saved" for the byte: `tests/gn_start_test.c`).

## Easy mode (branches `gn/easy-1.0`, round 7 `gn/easy-1.1`)

What a child sees on the synth itself (the design boards FM1-Panel 3a to FM1-Boot 3i, FM1-Gnome 9b, and
round 7's easy-controls picks of 2026-10-06): nine pictures, each a group of sounds, four coloured knobs
(Bright, Wobble, Echo, Space), a loop on Felucca's own sequencer with three pages of its own. Felucca's own
pages are the other mode, Advanced, untouched. **The first power-on is Easy** (in GNOMAD's colours where the
Advanced skin is built in); after that the synth starts in the mode it was left in (round 7, pick 1: the mode
rides in favourites' row 15, byte 30 of the settings record, beside the touch list above). **HOME + SAVE held
together for 2 s without a break** switch between the two. Our code is `firmware/src/gn_*` (`gn_hooks.h`,
`gn_fork.c`, `gn_state.c`, `gn_easy.c`, `gn_screens.c`, `gn_screens2.c`, `gn_gfx.c`, `gn_type.c`,
`gn_art.c`); upstream's files carry only the hook lines in the touch list. `FELUCCA_GN` (gn_hooks.h, default
1) builds them; 0 compiles every hook out. `GN_START_EASY` (gn_hooks.h, default 1) is the start in Easy;
`tests/gn_upstream.c` builds upstream's own tests with it 0, so the hooks-in proofs (upstream's renders, the
4-track WAVs, the golden regressions) stay byte-identical. Easy never changes how the synth starts or
installs: `main.c`, `ota.c`, `ota_hw.c`, `usb.c`, the loader and the package format are upstream's bytes, and
the base under it changes no file of `firmware/` but `felucca.c`'s name lines (the base's single-byte SysEx
change in `usb.c` was reverted, 434dafc). Easy's state is all zero at power-on (`.bss`); the start in Easy
is decided at the first main-loop frame (`gn_boot`, from `gn_input`), never earlier, and nothing is saved
then; the only code of Easy's that can run before the main loop is the key hook, which the audio ISR may call
as soon as audio runs (`main.c` enables it before the start-up delays), and which returns every note
unchanged while Easy is off.

### RAM

The base leaves 9,420 B of `.data+.bss` (88,884 of 98,304), 5,920 B of usable pool and 1,160 B of
`.noinit`. Easy takes its drawing scratch from the canvas Felucca already has, and only `.bss` for its
state:

- **gfx.c's canvas `cv_px`** (`uint16_t[CV_MAX]`, 240 x 124 pixels, 59,520 B, in `.pool`). Felucca
  keeps nothing in it from one draw to the next: every draw starts with `cv_begin` (which waits for the
  last strip's DMA, `lcd_sync`, and clears the width x height it uses), draws, and blits (`cv_blit_from`,
  the only reader of an older canvas, is never called). While Easy owns the screen (`gn.on`, and
  `gn_draw` has the frame: `ui_draw` returns at its hook) none of Felucca's drawing runs, and Easy uses
  the canvas as:
  - bytes 0 to 30,719: the strip being drawn, at most 240 x 64 pixels (`gx_begin` -> `cv_begin`; the
    strips `gn_draw` asks for are at most 7,680 pixels, so 15,360 B of this is used);
  - 30,720 to 36,863: the edge list of the shape being filled or stroked, 512 segments of 6 `uint16_t`
    (6,144 B), lives for one shape;
  - 36,864 to 37,831: the fill's accumulation row, 242 `int32_t` as `uint16_t` pairs (968 B), lives for
    one row of one fill;
  - 37,832 to 53,191: the stroke coverage mask, a byte per pixel of the strip (15,360 B), lives for one
    stroke, and is cleared at every `gx_begin`;
  - 53,192 to 59,519: 6,328 B never touched (kept for the Advanced skin).

  Nothing of Easy is kept in the canvas between two calls of `gn_draw`. That matters because Felucca
  draws into the canvas while Easy is on: main.c's update countdown (OCT- + OCT+ held: `ui_draw` hands
  the frame to Felucca), the update session's screen (`ota_hw.c ota_show`), a crash screen. The tail is
  accessed only as `uint16_t` (the canvas's own type) and as bytes, so nothing is type-punned. Easy's
  own state (what is on the screen, the next strip) is in `.bss`.
- **`.bss`**: Easy's state (`gn`, 1,024 B, all zero at power-on, with the undo copy of one track's 64
  steps), the screen's snapshot and its three part lists (on the screen, this frame's, the strip
  pass's: 512 B each), the drawing state: 2,768 B in all, as CI's linker reports it (below). `.data`:
  one byte (`gs_shown_view`, which view is on the screen, starts as "none").
- **pool, `.noinit`, flash objects**: nothing. Easy stores nothing of its own anywhere: the sounds it
  puts on tracks are Felucca's own parameters, saved in Felucca's projects as anything else is.

### Budgets (CI's linker, this branch against the base built in the same job)

`.github/ci/gn_budget.py`, the workflow's step "Easy's budgets against the base", builds the base
(`GN_BASE_COMMIT`, 434dafc) with the same toolchain and reads both builds' symbol tables and
disassemblies (uploaded as `<tag>-budget-inputs`, so the figures can be rerun from them). The figures
below are those of run 37526588992, on commit 20d1d0a (round 11's code, gn/easy-1.1); a later commit that
changes nothing but this file gives the same figures in its own run's manifest ("Easy's budgets against the
base"). Round 6's (run 37393816722, fa5b7e7) are in brackets:

- **Flash**: the image is 527,256 B of the 581,564 B app slot (54,308 B free); the base's is 477,008 B, so
  Easy adds 50,248 B (36,452): 50,244 B of code and constants in `.text`, 4 B of `.data`'s image. Of it the
  fork's own symbols are 27,180 B of code (20,324; what the compiler inlined into upstream's functions is
  counted there instead) and 7,005 B of constants (5,435: the pictures, the type, the 49 sounds and their
  maps).
- **RAM**: `.data` 284 B (base 280: +4, one byte and alignment), `.bss` 92,148 B (base 88,596: +3,552; round 6
  +2,768: the recording's state, the undo copy's motion store, the loop pages' snapshot; round 10 +224: the
  64-key onset log, a REC tap's own snapshot; round 11 +32: that snapshot 18 steps, the pad times halved),
  `.data+.bss` 92,436 B of 98,304 (5,868 B free; Easy's allowance +3,584 B, 32 B left).
  The pool (329,952 B, 5,920 B usable), `.noinit` (14,536 of 15,696 B) and `.ram_text` (2,888 B) are the
  base's, byte for byte in size. The canvas's tail is used as before (6,328 B untouched: the draw test).
- **Stack**, a static estimate (each function's frame read off its prologue, the deepest direct call path;
  Felucca's own calls through pointers, the engines' renders among them, are not followed, in either build):
  the main loop (`fm1_cstart`) 3,640 B, the base's 3,596 B (+44; the deepest path is upstream's, an editor
  user-preset store reaching flash); `ui_draw` through Easy's scenes 1,112 B (Felucca's own deepest from
  `ui_draw` 1,616 B, the base's 1,612; 1,096 B through Easy); `ui_input` through Easy 1,308 B, Felucca's own deepest from it 1,944 B (the base's
  2,048) (a mode switch's settings save
  reaching flash; Felucca's own deepest 1,888 B, the base's 2,048); the audio interrupt 896 B in both; the
  key-scan interrupt 112 B (base 108); the key hook `gn_key` 44 B. The user stack is 24,320 B. Neither is the
  synth's own high-water: that needs the synth.
- **The merged firmware (V1)**, estimated from these and V1's run 37395252799 (the skin adds about 21,336 B
  of image and 32 B of `.bss` over Easy alone): image about +70,800 B (allowance +77,824; about 33,500 B of
  the slot free, floor 16,384), `.bss` about +3,330 B (allowance +4,096). The merged firmware's own CI run measures
  it.

**Allowances.** The decision of 2026-10-06 (round 7, the easy-controls picks) raised the growth
allowances so every pick fits, keeping the floors (16 KB of the slot and 4 KB of RAM free): EASY image
+57,344 and .bss / .data+.bss +3,584 (from +40,960 and +3,072), V1 image +77,824 and .bss / .data+.bss
+4,096 (from +61,440 and +3,584); every other figure as before. The budget step fails the run when a figure is over the allowance stated here for the
build's milestone (`GN_MILESTONE`), or when the line for it is missing, doubled, empty or not whole: it
must name each of image, .data, .bss, .data+.bss, pool, .noinit, .ram_text, free:image, free:.data+.bss,
stack:fm1_cstart, stack:fm1_alnk0_irq and stack:fm1_timer5_irq once, may add other stack roots, and
nothing else. `+N` is the most a figure may grow against the base, `free:<figure> N` the least that must
stay free, `stack:<root> +N` the most a root's static depth may grow; a number is plain digits or
thousands grouped by commas (40960 or 40,960; never 4,0960 or 40,96,0, nor a trailing or doubled comma). `tests/gn_budget_test.py`,
run first in that step, checks that each kind of bad line is refused, apart from whole lines whose
figures pass or fail. EASY is this branch's; V1 is the line for the merged firmware (Easy and the
Advanced skin, gn/gnomad-1.0: its run 37384547040 measured image +57,200 B, `.bss` +2,800 B, 47,356 B of
the slot free, `ui_draw` +388 B):

```
gn-budget EASY  image +61440 .data +64 .bss +3584 .data+.bss +3584 pool +0 .noinit +0 .ram_text +0 free:image 32768 free:.data+.bss 4096 stack:fm1_cstart +256 stack:fm1_alnk0_irq +64 stack:fm1_timer5_irq +64 stack:ui_draw +256 stack:ui_input +256
gn-budget V1    image +81920 .data +64 .bss +4096 .data+.bss +4096 pool +0 .noinit +0 .ram_text +0 free:image 16384 free:.data+.bss 4096 stack:fm1_cstart +256 stack:fm1_alnk0_irq +64 stack:fm1_timer5_irq +64 stack:ui_draw +512 stack:ui_input +256
```

### What a child does (round 7)

- **ALGORITHM**: the picture (the group), on the selected track: Gnome, Organ, Bells, Laser, Whale, Choir,
  Cloud, Piano, Drums, round and round (since round CR ten: FUNNY the ninth, before the Drums; 57 sounds). A detent puts the picture's first sound there, which is the
  picture's sound of rounds 1 to 6. The pick screen (3c) shows meanwhile.
- **PRESETS**: the sound inside the picture, round and round; a peek list (the picture small on the left,
  its sounds listed, the current one inverted) shows and gives way to the play screen 2 s after the last
  turn. On a sound that is not Easy's PRESETS rests. On the loop page (the circle until round UX-2, now LOOP: a
  block a bar) PRESETS is the demo grooves instead (below, "Demo grooves").
- **FX SCL ENV LFO**: tracks 1 to 4, the selected one lit (the LEDs are one colour; the screen's badge
  carries the track's colour: 1 blue, 2 green, 3 yellow, 4 vermilion).
- **SELECT**: the tempo itself, 40 to 240 BPM, a beat a detent; every Easy header shows it. With an external
  clock the header shows the clock's tempo ("BPM IN") and SELECT and EDIT set nothing. Since round SEQ-1 SELECT
  turns the page ring and EDIT held + SELECT is the tempo; since round FLOW-1 the ring is six pages, HOME SOUND
  GROOVES LOOP STEPS MIX (SONG hidden; gn_seq.c GN_RING).
- **EDIT**: tap tempo, Felucca's own (`ui_layer.c glo_tap`: from the third tap, the last four), with the
  big number while tapping.
- **GLO**: the four tracks' volumes on K1 to K4 (and back).
- **ARP**: the selected track's arpeggio on and off (Easy's sounds only; the sound's own settings, else up,
  1/16, one octave).
- **SEQ**: the loop's pages, round and round: the circle (3d), the steps (one bar up close: a melody as dots
  by pitch, the drums one row per sound), the record screen (playing: REC held records into the selected
  track and letting go stops; stopped: each key is written at the cursor and the cursor moves on, K1 moves
  it, K2 a note or a rest at the cursor in place: round 24, only K1 moves the cursor). On the loop pages K1 to K4 are BARS (1, 2, 4, 8), SWING (5 % a detent), SNAP
  and VOLUME of the selected track; off them Bright to Space again.
- **REC**: a tap arms (it never leaves the page); a tap again disarms or stops. Armed and stopped, PLAY
  counts in 4, 3, 2, 1 (four beats, big digits in the track's colour) and the loop starts recording; armed
  while playing, the recording starts at the next bar 1. It goes into the selected track; another track
  while armed or recording takes the arming. KEEP (Felucca's overdub) is the default; K4 while armed or
  recording switches KEEP or REPLACE (REPLACE clears each step once, just before the recording can reach
  it). REC held 0.4 s while the loop plays punches in until let go. No other REC hold. The "first key
  starts it" style is a reserved bit beside the mode bits, always 0 (count-in): it needs the phone app,
  and so a protocol command (a Discussion first).
- **SAVE**: a tap saves the song into Easy's own slot, else the first empty one, never over a song of
  someone else's. Round 8: the slot is Easy's while it holds exactly what Easy last wrote there (the slot and
  a fingerprint of the packed project, beside the mode byte); a name proves nothing, and any load, restore or
  save that replaces the slot ends it (the Gnome says SAVED once it is read back from flash,
  FULL when the four slots hold others' songs); while the loop plays it stops first. **SAVE held** 0.7 s:
  undo of Easy's last recording (or step entry). Both act when SAVE is let go, never when HOME was down at
  any moment of the press. Round 24: the loop Easy wrote is also saved by itself, by the same rules,
  once it is stopped and settled (below, round 24); SAVE stays for "save now, and say SAVED".
- **HOME**: back to playing. **PLAY**: start and stop, from any page. **OCT- / OCT+**: two octaves either
  way.
- **KNOB 1 to 4** elsewhere: Bright, Wobble, Echo, Space of the selected track, while it holds the sound
  Easy put there; on any other sound the bubbles rest and the knobs write nothing.
- **keys**: on one of Easy's sounds the sixteen white keys of the lower row play C major and the black keys
  of the upper row rest; on a kit (Easy's kit, the drum machine) the n-th white key is pad n. On any other
  sound every key plays as in Felucca. Since round 13 every key of Easy's melodic sounds plays its own note,
  chromatic, the black keys too; on a kit (KIT, DRUM MACHINE, and since round EW HAND DRUM and TOMS) the
  black keys are the kit's eleven other sounds (on a Gnomad kit, whose sixteen pads are all on the white keys:
  since round EW-FIXES each black key a soft hit, a ghost, of a drum beside it).
- The screens: entering (3i), play (3b, the Gnome 9b), pick (3c), the peek list, the loop's circle (3d),
  steps and record screen, drums (3e), the volumes, tap tempo, the count-in, saved (3g: the app's save; the
  Gnome's SAVED: the panel's), leaving (3h). Every one but 3g, 3h and 3i has the header (the track's badge,
  PICTURE * SOUND, the tempo), the four knob cards in their colours, the field and a band that names the
  buttons in brackets.

### The grid: BARS and SNAP (round 7, picks 7 to 9)

A track's SNAP is its DIV: on records onto 1/16 steps with the track's swing, off onto 1/32 (the finest step
the sequencer has: its steps hold no timing finer than a step, so "not quantised" is a 32nd); 64 steps hold
2 bars at 1/32, 4 at 1/16, 8 only at 1/8. So BARS 8 is 1/8 whatever SNAP says, SNAP off is refused above 2
bars, and swing at 1/32 swings 32nds. BARS and SNAP change the grid only when the child turns them: spreading
onto a finer grid is exact; merging onto a coarser one keeps each pair's step that sounds, the first when
both do (a note on an odd fine step moves to the one before: what SNAP means). The track's motion moves with
its steps. Both rest while recording or counting in, during a song, and on a DIV that is not one of the
three (a grown-up's 1/4 or triplets). A first recording into an empty pattern takes the default grid (pick
9): the drums snapped (1/16, 2 bars), a melody not (1/32, 2 bars); a pattern with notes keeps its grid.

### Round 8 (the review of e8cda2e)

- **Motion through a grid change.** Coarser, an event of a pair's first fine step wins over the same
  setting's on the second (that one goes); finer, an event past the 64 steps goes with its steps; the motion
  store stays valid (`motion.c motion_valid`), so the project saves and backs up. Easy's undo copy holds the
  whole motion store (260 B of `.bss`) and SAVE held puts it back byte for byte.
- **The music replaced.** A project or the runtime backup loaded, the settings restored, the editor's pattern
  writes: armed, a count-in, recording, a punch, a SAVE waiting for the stop all end (`gn_rec_cancel`).
- **REPLACE** takes out of each step the notes it held before the take began, as the playhead enters it while
  recording, once a take; what the take records stays. The old note of a step sounds once as the playhead
  enters it (the clearing is the main loop's, a frame after).
- **The default grid** is only for an untouched empty pattern: BARS or SNAP the child turned on a track stay.
- **Level motion.** Playing a track's own level motion retires Easy's claim to that level and takes none.
- **One knob map** (`gn_knob_role`) for the input and the screen: every overlay shows the knobs that are live;
  K4 is KEEP / REPLACE over GLO while armed or recording.
- **Settings restored** under the running synth are made to agree with it (the mode it runs in, the marker),
  by the normal settings save; nothing at start-up.
- **A punch keeps the keys** played during REC's 0.4 s hold (up to eight, each into the step it was played
  in); a tap under 0.4 s only arms.

### Round 9 (the recheck of 180e412)

- **The song's slot** is Easy's only while the flash's current copy of it is Easy's own last write: the same
  commit sequence (`storage.c st_hdr_t seq`, which every later write of the slot moves, by the editor, a
  backup restore or Advanced's SAVE pages, the same bytes or not) and the same 64-bit payload fingerprint.
  Restoring the old bytes later does not bring it back. SAVE says SAFE TO UNPLUG only when the project and
  Easy's record of it are both written and read back; when the record cannot be written it says SAVED, NOT
  KEPT AS EASY, NEXT SAVE: A NEW SLOT, and claims nothing. Easy never infers ownership: unsure, an empty slot,
  else FULL. A settings-only restore brings back an older record, which owns nothing unless the slot still is
  that very write; a full backup restore writes the slots, which ends every claim.
- **REPLACE** is applied when a take ends (REC, a punch let go, a STOP, a START or CONTINUE, another track):
  each step the playhead crossed while recording (the sequencer's own step, its tick: a START is a new start,
  not a wrap) keeps only what the take recorded there (the panel's keys logged, up to 24 a take, and any note
  the snapshot did not have: MIDI in, the arpeggio), a tie the take held stays; every other step is as the
  take's snapshot (the undo copy) had it, with any note the take recorded ahead into it. Nothing is deleted
  while a take goes on, so a step's old note still sounds once as the playhead crosses it. Past 24 keys a
  take, a key of the same pitch as an old note in a crossed step is taken for the old one. The music replaced
  ends a take with nothing applied.
- **REC held while playing** begins a take at once, through Felucca's own recording (ties, chords,
  retriggers, releases, as any recording); let go before 0.4 s it was a tap: the pattern back as it was and
  REC armed; held, a punch until let go. A STOP during a punch ends it and its arming. (Round 10: a key
  already down when REC goes down is recorded from there.)
- **Motion undo** applies only while the motion's generation is unchanged (`gn.mgen`: every editor command
  that may write or reset motion moves it, and Easy's picks) and the store is as Easy left it, and only to the
  undone track's events.
- **The music replaced mid-gesture**: a REC under 0.4 s, a punch, a SAVE held do nothing more, nor does any
  button until every one is up; a recording's held notes are let go without writing.
- **The grid choice** is cleared only on the tracks an operation changed.

### Round 10 (the recheck of 41f9ddf)

- **Motion undo**: Easy's own sound picks move the generation too; it is 32 bits now.
- **REPLACE**: a DRUM hit the take recorded ahead into a step it never crossed is kept, as a note is. The
  onset log holds 64 keys a take (a normal take never reaches its end); past it, a crossed step keeps both the
  old note and the take's, nothing deleted.
- **STOP**: seen stopped at any frame during a take, the take ends with every step up to the playhead's
  processed, the step it has just entered too (an external STOP as well).
- **HOME + SAVE**: the music replaced during the hold cancels the switch; it counts again only after every
  button is up.
- **A REC tap** under 0.4 s takes its own snapshot of the steps it may record into (18 from the playhead,
  wrapped at the pattern's LEN, and LEN and DIV); the last recording's undo copy is never touched, and the
  rollback puts that window back exactly, with the audio IRQ off. Held on, the snapshot becomes the punch's
  undo copy. A tap is never a punch before 400 ms: the window holds the worst case (240 BPM, 1/32, the most
  swing: 400 ms and a frame enter 15 steps, plus the one recorded ahead); if it would still overflow (a stalled
  panel), the tap is put back and REC armed. A pattern no longer than the window is covered whole. (Round 11.)
  Easy's knobs never record motion (they set the track's settings directly), so a tap has no motion to put
  back; motion an app writes during it is the app's and stays.
- **A key already down when a punch begins** is recorded from the punch's start (its notes, then its ties).

Accepted residuals (not fixed):
- The ownership check can wrap after a full cycle of the slot's commit sequence, or after a factory reset and
  a restore of an identical first save.
- Power lost between the project write and Easy's record leaves an orphaned slot: the next save warns, and
  FULL says what to do.
- During REPLACE, a MIDI-in or arpeggio note identical to an old note is judged against the snapshot only.
- A STOP then CONTINUE within one UI frame is not seen as a stop.

### Round 12 (the count-in's take)

- **A take that begins at the end of a count-in** (the loop stopped, about to start) begins with a fresh tick:
  the transport starts from 0, so its first frame is no START, and the take is not ended and begun again at
  once. Before, it kept the last play's tick, its first frame looked like a restart, and the take began again
  with a new undo copy that already held what it had recorded in the loop's first moment (a key played as the
  loop starts): SAVE held then left that note. Its undo copy is now the pattern as the count-in found it.
  While the loop has not started, the take marks no step crossed (the old playhead is not a step of it).
  Found in the simulator (a key at the count-in's end).

### Round 27: power-on restores the last save, whichever mode made it (report F15, V1.5)

Reported on V1.5 (gn/gnomad-1.0 1bd4432): a beat built in Advanced and saved there; the power off and on; "it loaded the ones
you made from the easy mode, and that seemed to overwrite what I recorded".

- **What the code did (host, a real power cycle as round 25's).** Advanced's SAVE (`ui_name.c name_ok` ->
  `project.c project_save_as`) wrote the beat to its slot and left no record of it; power-on (`gn_easy.c gn_boot`)
  knew only Easy's song: left in Easy it loaded Easy's older song over the tracks (`gn_song_boot`), left in
  Advanced it started Felucca's default tracks (`main.c felucca_init`, `TRK_DEF`). Either way the beat was on flash,
  in its slot, never written over, but not what played. And the autosave did write over something: Easy still owned
  its older slot, so a recording in Easy after an Advanced save went into that slot, over Easy's older song, with
  whatever the tracks held (`gn_song_slot`); Advanced's own slot was safe (the record's commit sequence). Tests
  `t_r27_f15_left_advanced`, `t_r27_f15_left_easy`, `t_r27_autosave_refuses` (2), `t_r27_full` failed before the fix.
- **The rule.** Power-on restores the last thing saved, whichever mode saved it, in the mode
  it was saved from, loaded and stopped (no autoplay): Advanced's SAVE or rename, Easy's SAVE or autosave, the app's
  project save. Easy owns a slot only while its own save is the last save of it: its SAVE and autosave never write a
  save of Advanced's or the app's, nor an older song of its own once another save came after; they take an empty
  slot, else FULL (as before). A slot saved from Advanced stays in Advanced's PROJECT list and loads there. Easy has
  no slot list (loading is Advanced's); none was added here.
- **The record (no new bytes).** Easy's song record (favourites' row 15, bytes 17 to 29: fingerprint, commit sequence,
  slot) becomes the record of the last save. Byte 29: bit 7 valid, bits 0 and 1 the slot, bit 4 round 27 wrote it,
  bit 5 saved in Advanced, bit 6 not Easy's own song. `gn_project_saved` (`gn_state.c`, the hook in `project.c`) notes
  any save on flash that is not Easy's own; `gn_last_keep` writes its record at the next input call of either mode
  (one settings save, as Easy's own save makes); a slot that does not read back clears the record. `gn_boot_mode`:
  the record holds (the slot's flash copy is that write: sequence and payload) -> the mode byte set in RAM to its mode
  (nothing written at start-up), `gn_enter` in Easy, the slot loaded (`project_load`; in Easy `gn_reclaim` and the
  header's mark only for Easy's own song; in Advanced Felucca's own LOADED shows). No record, or one that no longer
  holds: as before. `gn.save_note` sits in `booted`'s padding byte: `sizeof gn` 1,704 before and after.
- **Migration.** A record without Gnomad's marker: as before (no song, nothing loaded). A round 25/26 record (bit 4
  clear, `0x80 | slot`) starts as it did: the mode it was left in, Easy's song loaded only in Easy, still Easy's to
  autosave; the next save writes round 27's record. Back on round 26 firmware a round 27 record reads as no song of
  Easy's (its mask `0xFC`): nothing loaded, nothing written over.
- **Tests.** `tests/gn_easy_test.c` round 27 (10 cases: F15 left in Advanced and back in Easy, Easy's save the last
  left in Advanced, the autosave refusing Advanced's save and Easy's older song, FULL, the app's save, a rename,
  migration both ways); `tests/gn_start_test.c t_r27_last_save` (the mode and project from the last save, nothing
  written at start-up, again on the next cycle). Changed by design: `t_r25_power_cycle`'s last step (Easy's
  autosave the last, left in Advanced: now Easy with the song, was Felucca's power-on), `t_save_song`'s and
  `t_song_ownership`'s record byte (bit 4). No screen changed: 44 goldens byte-identical.
- **Budget (run 38283069584, b8b7718, against 1bd4432's run 38214198644).** Image 557,400 B, +380 B for round 27
  (+80,392 B against the base; V1 allows +81,920: 1,528 B left; 24,164 B of the slot free). `.data` +4 and `.bss`
  +3,648 against the base, both unchanged by round 27 (`gn.save_note` in padding). Stacks: every V1 line as before
  (`ui_input` -108, `ui_draw` +392, audio IRQ +32 against the base).
- **On the gn/ux line (merged `--no-ff` onto 41377ee).** One conflict, `gn_state.c`'s `gn`:
  `booted` has no padding byte there (UX-1's fields follow it), so `save_note` sits in `save_ok`'s padding instead
  (`sizeof gn` 1,800 before and after). `gn_boot` keeps FW-CONTENT-8's power-on sounds first, then the slots from flash,
  then `gn_boot_mode`; `gn_save_now` keeps UX-5's `gn_saved_now`. The `project.c` hook is the same bytes as on
  gn/gnomad-1.0. Every round 27 case and the boot test pass there; the 61 approved screens byte-identical.
- **Testing guide.** A one-card testing guide (which mode, what to test, which buttons, what power-on does):
  `firmware/docs/TEST-GUIDE-V1.md`.

### Round 25: the song after a power cycle (report on V1.1)

Reported on V1.1 (gn/gnomad-1.0 aa6796a): a beat recorded in Easy, SAVE showed SAVED, the power off and on: the loop
was gone, and the Advanced slot list did not show it.

- **What the code did (traced, host-tested with a real power cycle: the flash kept, the RAM as at power-on, the
  slots and the settings record read back as `persist_boot` reads them).** SAVE (and round 24's autosave, the same
  `gn_save_now`) shows SAVED only after `project_save_as` wrote the slot with `st_save` (erase, program, commit
  record last, and `st_save`'s own read-back of header and payload over SPI, `fl_read_ram`), `gn_saved_verified`
  read the slot back from flash equal to RAM, and `gn_song_keep` wrote Easy's record of the slot in the settings
  and read it back (`persist_pending` clear, the bytes on flash). Every write is synchronous in the main loop;
  nothing is queued behind SAVED, and a power cut 3 s after SAVED keeps it: `t_r25_kit`, `t_r25_drum_machine`,
  `t_r25_groove` (the beat on Easy's kit, on the drum machine, over a demo groove; SAVE tapped while the loop
  plays) find the beat after the cycle in a slot of the list, named GNOMAD SONG, and loaded by Advanced. The host's
  flash is NOR-faithful (erase to FF, program ANDs bits) but is not the SPI driver or the chip; what it cannot
  show is below.
- **Finding 1 (hypothesis a): power-on never loaded the song.** `main.c felucca_init` puts Felucca's default tracks
  (`TRK_DEF`) up at every power-on and nothing loads a project: a saved beat was on flash but an Easy player could
  only hear the defaults. `t_r25_power_cycle` (the reported sequence exactly) failed on this before the fix.
  **Fixed:** when the synth starts in Easy, `gn_boot` (the first main-loop frame) loads Easy's slot if Easy's
  record of it still holds (`gn_song_owned`: slot, commit sequence and fingerprint against flash), stopped, no
  autoplay (`project_load`), makes each track's sound Easy's again where it is one of the 49 (`gn_reclaim`: the
  sound of its engine and preset whose own settings the track holds most; the knobs' bubbles where the song left
  them) and drops the entry Gnome. A synth left in Advanced starts as Felucca does; a slot someone else wrote since
  is not loaded. Nothing is written at start-up.
- **Finding 2 (hypothesis b), not reproduced, hardened.** The one host-reproducible way to have SAVED and yet a
  slot list without the beat: `persist_boot` keeps a slot's `.noinit` RAM copy whenever it still reads as a valid
  project and fetches from flash only the ones that do not (its comment says the flash copy wins; the code lets it
  win only over garbage), so a RAM copy that survived the power-off but is older than the flash's hides the newer
  song. Now `gn_slots_from_flash` (first main-loop frame, before anything reads a slot) fetches each slot whose
  flash copy differs from RAM (`t_r25_stale_ram`). Not settled without the synth: whether `persist_boot`'s JEDEC
  check (`flash_ok`) passed on that power-on (if not, the slot list, the settings and Easy's record are all RAM
  garbage for the session: the other settings, the colour or the mode, lost too would show it), or a chip-level
  write the read-back passed; to be checked on the synth.
- **The mark.** `gn.auto_dirty` 2 (`GN_LOOP_SAVED`): the tracks hold Easy's saved song, since a verified save or the
  power-on's load; the header draws a check left of the tempo (the armed disc's place, never while armed or
  recording). Easy's recording, step entry, BARS, SNAP or undo take it off (and the autosave puts it back); a
  pick on the saved song takes it off and the autosave saves the new sound (so the next power-on has what was
  last heard); the app's pattern writes, a project or backup, a groove, leaving Easy drop it. `gs.mark` in `gs.name`'s
  padding: `sizeof gs` 188 and `sizeof gn` 1,704 B before and after, no `.bss`. The approved screen
  `easy-11-saved-gnome.png` changed only by the mark.

### Round 24: a test on the synth

V1 was tested on an FM-1 on 2026-10-11; four changes followed:

- **Step entry in place.** On the record screen while stopped, K2 (NOTE/REST) writes a note or a rest at the
  cursor in place: turned to and fro it sets one step; only K1 (MOVE) moves the cursor. A key still writes its
  note and moves on (one note at a time, as the screen says). `gn_step_write(.., advance)`; `t_loop_pages`.
- **Sounds.** The Cloud's Wind and Rain are String pad and Warm strings; every Whale sound is a bass from its
  lowest key (Sounds, round 24).
- **An app pick is Easy's pick.** The app's PRESET of one of the 49 while Easy runs is adopted (Whose sound,
  round 24).
- **The loop survives a power-off.** Project RAM is `.noinit` and a power cycle loses an unsaved loop, so
  Easy now saves the loop it wrote by itself (`gn_dirty`, `gn_autosave`, `gn_save_now`). Each of Easy's own
  writes of a pattern marks the loop unsaved: a recording that wrote a step (the take's frames, its last frame,
  REPLACE), step entry, BARS, SNAP and an undo of them; picks, knobs and the tempo do not (SAVE saves those). The
  app's writes do not mark it, and a project loaded, a backup put back or an editor save drops the mark (the
  music is not Easy's to save then), as does leaving Easy (Advanced's music is Advanced's). The autosave runs once
  a frame when the loop is marked and settled: the transport stopped (a flash erase silences the audio and stalls
  the sequencer: Felucca saves only while stopped), no song, nothing recording, armed or counting in, no SAVE
  waiting, no HOME + SAVE hold, no key or button down, 4 s since the last mark, key or button, and 10 s since the
  last autosave. It saves exactly as SAVE does (`gn_save_now`): into Easy's own slot, else the first empty one,
  never over a song of someone else's, named GNOMAD SONG, read back, Easy's record of the slot in the settings
  record. A save that worked says nothing (the child plays on); FULL, NOT SAVED, or the song saved but Easy's
  record of it not, show as SAVE's screens do, once, and the mark is dropped until Easy writes the loop again (no
  warning every 10 s). Unlike SAVE, an autosave leaves Easy's claims to the kept settings (`GN_KEPT`) as they
  are, so leaving Easy after an autosave still changes nothing musical; the slot holds the music as Easy plays it.
  Flash: each autosave is one project erase (the slot's A/B pair) and, when the slot's sequence moves, one
  settings erase; with the 4 s settle and the 10 s gap the worst case is 360 an hour of a child entering a note,
  pausing 4 s, and so on, about 550 hours to the 200,000 erases the slot's pair is rated for, far from any real
  session (one autosave per stop after a recording). What remains: a power-off within 4 s of the last write (or
  10 s of the previous autosave), or while the loop still plays, loses that last change; a full synth (four
  others' songs) says FULL and keeps nothing. State: `auto_dirty` and `auto_t` (16 ms ticks, 16 bits) in
  `saved_mine`'s padding, and the adoption's `adopt` in `sw_mark`'s: `sizeof gn` 1,704 B before and after (host),
  so `.bss` / `.data` growth is zero; the code grows the image (CI's budget line below).
  `tests/gn_easy_test.c t_r24_autosave`: nothing saved for play, stop and a pick; a recording saved 4 s after the
  stop and not before, not while playing, silently, into the first empty slot; step entry with a key every 1.6 s
  not saved, nor under a key held 6 s, then saved into the same slot 4 s after; a write right after an autosave
  waits for the 10 s; a power cycle (flash kept, RAM as at power-on) gives the loop back from the slot; the app's
  project load before the autosave saves nothing; four others' songs: FULL once, nothing written, no warning again;
  leaving drops the mark. With `gn_autosave` taken out the test fails in seven places.

Validation (local, as rounds 16 to 23): the generated headers from `tools/build.py`'s generators where they run
locally (`felucca_fm6.h`, `felucca_tables.h`, `felucca_samples.h`, `ui_palettes.h`, byte-identical to the
easy-1.1 branch's), the font, icon and keycap headers copied from that branch (no libraqm / fontTools here); a copy
of `tests/run_fork_tests.sh` without the package, OTA, loader and name checks (no `build/felucca.fwsc` locally):
ALL FORK TESTS PASSED, the Easy tests under ASan / UBSan. Upstream's `tests/run_tests.sh` is unchanged and none of
its sources include `gn_*` (CI runs it). The firmware is built and measured by CI (`firmware.yml`).

### Round 21: a suspension releases only what USB started

A long, legitimate host suspension (the computer or phone asleep while still plugged in) cannot be told from an unplug
by its length (usb.c treats frozen frames as "unplugged / host asleep"), so the 500 ms suspension path of round 20 no
longer runs the all-tracks cleanup. It releases only the notes that USB-MIDI started: each `midi_notes` entry now
carries its origin (USB or TRS) in spare bits, written by `gn_midi_event` at the existing source-aware dispatch
in `seq.c`. `GN_MIDI_TRACK_MASK` is 7 with the fork hooks compiled in, 0x7F with them compiled out; the
`midi_control.c` reads mask off the origin bits. Panel keys, TRS MIDI and
the song keep sounding through a suspension and resume. The full cleanup (every track) stays where it was: STOP, either
mode switch, a sound change, and a verified disconnect (configuration loss or bus reset). Tests: t_r21_suspend_local
(panel + TRS notes survive a long suspension; USB notes released after the timeout; playback continues through resume),
t_r21_suspend_shared.

Round 22 (target static loop budget): origin remains recorded once per dispatched message in the existing
`midi_notes` entry; `gn_usb_watch` still runs at audio-block start, including when the UI stalls. Its cold
USB-only scan (`gn_usb_notes_off`), source-aware dispatcher (`gn_midi_event`), all-track cleanup
(`gn_stop_notes`) and per-track cleanup (`gn_panic_track`) now have explicit `noinline` boundaries, matching
upstream's `midi_event`. No scheduling, ownership, timeout or cleanup behavior changes, new hooks, state or protocol.
The watch's common path is still a small state check; its nested ownership/voice scans cannot be inlined into
`fm1_alnk0_irq`'s block loop. This also keeps rare controller scans out of the IRQ's MIDI-queue loop.

`target_budget.py` counts instructions in backward-branch spans, weighted x4 per nesting level (capped at
four levels), and divides x9; calls are reported, not given a separate surcharge, and callees are not traversed.
The failing cost 45,988 must fall below 41,252.2 (37,502 x 1.10). Source-level count: the USB cleanup's 14
call sites inside its nested loops become one cold call at the watch boundary; the queue's dispatch stays one
call per message (replacing upstream's one `midi_event` call). Zero new call sites were added to existing loop
bodies. The actual target's 69 calls / cost cannot be recounted without CI's pi32v2 disassembly; this is an
outlining change, not a claimed target measurement or a change to the budget checker.

Validation: local copies of the round-20 host/fork runners, same unavailable package/target
checks omitted, existing generated headers unchanged. `host.log`: ALL HOST TESTS PASSED;
`fork-final.log`: ALL FORK TESTS PASSED, including ASan/UBSan and rounds 19–21. Host optimized IR
(`fixed.ll`) confirms all four helpers retain `noinline`; declarations precede the host harness's
attribute-stripping macro. Host `size -m`: data 256 B and BSS 2,305,056 B before and after, unchanged.
`call-audit.txt` records the source call count. Target cost / stack remain for CI. No network, firmware
build or commit.

Round 23 (one shared watch call): round 22's CI still reported 56 loop call sites and cost 42,204,
versus round 20's 55 and 37,497. The watch's configuration-loss and suspension-timeout branches now
only select a scope in an automatic `release` value. After those branches there is one shared call to
`gn_stop_notes(usb_only)`, the block-rate cleanup path already present in round 20. The USB-only scan
call moved into that cleanup function, before its all-track loop; it is no longer a separate call site
in the watch / IRQ block loop. STOP's existing guarded hook explicitly passes 0 (full cleanup).
`gn_midi_event` still handles each queued message in order and records origin in `midi_notes`; deferring
those messages would break mixed USB/TRS on/off ordering. The watch still runs at each audio-block
start. No event or USB-release deferral, not even one extra block; no state growth or new hooks.

Source call-site accounting: watch cleanup 2 -> 1, MIDI-queue dispatch 1 -> 1, all other IRQ hook sites
unchanged: expected target 56 - 1 = **55**. This is a source delta against the supplied CI count, not a
local pi32v2 measurement. The checked-in `target_budget.py` does **not** traverse a call's callee or add
a callee-cost surcharge: its weights depend on backward-branch spans and divides. Thus equal total
instructions / divides with different branch layout can have different cost; CI must verify the final
weighted cost and emitted call count. The budget and analyzer are unchanged.

`t_r23_usb_cleanup_scope` checks suspension followed by verified configuration loss: USB notes alone
end first, then the remaining panel/TRS voices and ownership end. Validation uses local
copies of the same local host/fork runners and existing generated headers (package / target checks
unavailable as before). `host.log`: ALL HOST TESTS PASSED; `fork.log`: ALL FORK TESTS PASSED,
including ASan/UBSan and rounds 19–23. Optimized host IR (`fixed.ll`, `call-audit.txt`) confirms the
USB-only call moved out of `mix_block`, reducing its call sites by one. Host data / BSS remain exactly
256 / 2,305,056 B; no static storage was added. `git diff --check` passes. No network, firmware build,
protocol change or commit.

Accepted residual (round 21, a review finding): with ARP HOLD latched on a panel note, a USB note on
the same pitch, the panel key released, then a long suspension: gn_usb_notes_off releases that pitch and the latched
local arp loses its root. Reaching it needs ARP HOLD + the same pitch from USB + a host suspension; a plain unplug or
any ordinary play is unaffected. Fix later by tagging the latched root's origin when ARP HOLD is reworked.

### Round 20: qualify the driver's suspended USB path

- `usb_poll` leaves `usb.config` unchanged when the bus freezes or the cable is unplugged;
  it sets `usb.suspended` after about 100 ms without frame-number progress (a suspend interrupt
  can set it sooner). `gn_usb_watch` now qualifies **more than 500 ms continuously suspended**
  before the existing cleanup. This tolerates short stalls / suspend-resume cycles while bounding
  frozen-bus cleanup to about 600 ms from lost frames, plus audio-block latency. Resume cancels
  the timer; one panic fires per continuous suspension. Config loss still cleans up immediately.
- No `usb.c` or hook changes. The 16-bit onset timestamp occupies two bytes of existing padding
  between `gn.grid_set` (offset 636) and `gn.mgen` (640); timing / expired flags use spare bits of
  `gn.booted`. `sizeof gn` remains 1704, subsequent offsets unchanged: zero `.data` / `.bss`
  storage growth. The audio-block watcher expires before a timestamp wrap; subtraction handles
  crossing the 16-bit boundary. No protocol or persistence-format changes.
- `tests/gn_easy_test.c`: `t_r20_usb_suspend` reproduces the real driver report (suspended set,
  config still 1) in Advanced after Easy and fails on `96ff7cb`. `t_r20_usb_resume` proves two
  separate short suspensions do not kill notes or accumulate time. `t_r20_usb_wrap` crosses
  the 16-bit timer boundary; both timeout cases check no premature panic, all voices / ownership
  cleared after expiry, and no repeated panic of a subsequent TRS note. Existing `t_r19_usb`
  still covers config loss. `t_r20_bars_erased` explicitly erases a tail note over STEP_SET,
  shortens then lengthens, and verifies it stays erased while the unerased note returns.
- Local validation uses local copies of the round-19 runners, with the same unavailable
  package / target checks omitted and existing generated headers untouched. `host.log`: ALL HOST
  TESTS PASSED; `fork.log`: ALL FORK TESTS PASSED, including ASan / UBSan and the config-loss case.
  `baseline.log` records the suspension regression failing on round 19; `fixed.log` passes all four
  round-20 cases. Host `size -m` (`sections.log`) confirms unchanged data (256 B) and BSS
  (2,305,056 B), and the layout probes confirm unchanged size / offsets. Target validation stays in CI.
  No network, firmware build, protected-driver edit or commit.

### Round 19: lossless BARS shortening and note cleanup

- **BARS tail:** refining the grid copied only steps 0–31 over the entire array, deleting hidden
  steps 32–63. `gn_bars_set` now checks the hidden half's notes, ties, hits and motion before each
  refinement. When they would be lost, it keeps the current division and changes LEN only; later
  lengthening reveals them. Empty-tail refinement and the existing explicit SNAP conversions remain.
- **STOP and either mode switch:** STOP formerly released only sequencer notes; switching modes
  requested no live-note cleanup. STOP now invokes the fork's block-boundary panic; entering and
  leaving request it for all tracks. MIDI ownership/chords, panel-key chords, mono/arp state,
  recording holds, pending engine notes and slicer playback are retired. Incoming and panel notes
  get outgoing MIDI note-offs while USB is configured. Voices use the existing one-block kill fade,
  independent of RELEASE, including the normal sound/engine-change panic. A mode switch deliberately
  cuts held notes; a physical key held across it must be released and pressed again to sound.
- **USB disconnect:** `gn_usb_watch` observes the falling configured state each audio block and
  invokes the same cleanup in either mode. This missed the suspended-only driver path until round 20.
  The disconnected cable cannot carry outgoing packets;
  local voices and ownership are still released. Reconnection does not resurrect a held key.
- Only `seq.c` needs upstream hooks (touch list); all behavior lives in `gn_*`. No protocol,
  persistent layout or static allocation change. The existing `gn.booted` byte's spare bits hold
  product activation and USB state. The Easy-off / golden proofs cover **`GN_START_EASY=0` before
  first entry only**. Cleanup stays active in Advanced after Easy has been used, including in those
  comparison builds. Production enables cleanup in Advanced as well as Easy from the start; these
  proofs do not establish universal “Easy off equals upstream” behavior.
- `tests/gn_easy_test.c`: `t_r19_bars`, `t_r19_stop`, `t_r19_enter`, `t_r19_leave`,
  `t_r19_usb`, plus `t_r19_sound` and `t_r19_engine`. The five tour regressions fail against
  unmodified `c4a84a1`; tests observe the actual mode transition, render 100 ms, count active voices
  including release tails, and check ownership and outgoing note-offs. BARS checks the whole step
  array and hidden motion through 8 -> 1 -> 8. Sound and engine changes also check note-offs.
- Validation: local copies of the round-18 runners use existing `build/gen` headers,
  omit unavailable package/OTA/loader and target-disassembly checks, and use `sips` for ABOUT PNGs.
  `fork-final.log`: ALL FORK TESTS PASSED (including ASan / UBSan and unchanged golden renders).
  `host-final.log`: ALL HOST TESTS PASSED. No firmware build, header regeneration, network or commit.
  No new globals or enlarged structs: zero added `.data` / `.bss` storage. Same-flags host binaries
  have identical data allocations; Mach-O BSS alignment padding differs by 8 bytes (symbol-order
  comparison in `base.nm` / `fixed.nm`), so target section totals remain for CI.

### Round 18: settings migration from Felucca 1.0.1

- **Explicit first-boot migration.** `gn_settings_migrate`, called from `gn_boot` at the first
  main-loop frame with `GN_START_EASY` enabled, tests the existing mode marker:
  `(favorites.factory[15][30] & 0xF0) != 0xA0` means an image not yet marked by Gnomad
  (Felucca 1.0.1's never-set byte is zero). The intended defaults are GNOMAD colour
  and Easy mode. A marked image keeps the user's palette and saved mode on later boots.
- **Exactly two fields:** `persist_t.palette` (offset 4, four bytes) becomes the tagged GNOMAD
  palette, `palette_to_stored(GSK_PAL)`, and `persist_t.favorites.factory[15][30]` (offset 562,
  one byte) becomes `0xA1` (Gnomad marker, Easy, count-in). Every other byte of the 572-byte
  PER4 record stays identical, including calibration, HOLD, LEDS, lowcut, favourites, the song
  bytes and upstream's adjacent `layer_seen`. The Easy-only build has no `GSK_PAL` and changes
  only the mode byte; the combined product's Advanced skin supplies GNOMAD.
- The migration changes imported RAM state only; the next normal settings save writes both
  fields together. Until that save, another power cycle repeats the migration from the unchanged
  upstream image. No boot flash write, new marker, `persist_t` growth or `PERSIST_MAGIC` change;
  no new static storage, so `.data` / `.bss` growth is zero. A live settings restore still uses
  `gn_mode_reconcile` and keeps the restored colour. Because upstream preserves the marker, a
  Gnomad record subsequently saved by Felucca remains marked; this detects unmarked images,
  not the identity of every intervening writer.
- `tests/gn_start_test.c`: `t_r18_felucca101_migration` constructs an independent copy of the
  upstream PER4/PAN5 layouts with recognisable valid values, writes it to simulated NOR, boots,
  checks the complete exported and normally saved image, then boots again. The retired DIGITAL
  row is zero as 1.0.1 writes it; the unused mode byte starts at zero. `t_r18_gnomad_palette_kept`
  saves a user's different colour through Gnomad and checks all bytes after later Easy and Advanced
  boots. Both run under the fork runner's ASan / UBSan. With no skin on this branch, this test alone
  supplies palette index 7 as a valid stand-in for `GSK_PAL`, so the colour migration branch is
  exercised; a merged tree automatically uses the actual skin's index. This does not test skin artwork.
- **Local validation:** the exact root commands were run: `bash tests/run_tests.sh` passed its
  initial checks then stopped at missing `build/felucca.fwsc`; `bash tests/run_fork_tests.sh`
  stopped at its three-argument usage check. As in rounds 16–17, local copies omit
  package/OTA/loader and missing target-disassembly checks, use the existing generated headers,
  and run from this workspace root. `host.log`: ALL HOST TESTS PASSED; `fork-final.log`:
  ALL FORK TESTS PASSED, including the new cases under ASan / UBSan. The first fork copy's only
  failure was absent Pillow; the final copy uses macOS `sips` for ABOUT's 12 PNG conversions,
  retaining the name assertion and layout lint. Optional DaisySP, font-spacing/host PNG conversion
  and package-dependent checks remain unavailable. No headers regenerated or firmware built locally.

### Round 17 (the recheck of round 16)

- **A rejected track cannot end a kept-setting claim.** `gn_editor` checks `kt < NTRK` before
  `gn_kept_retire`; retirement itself ignores an out-of-range track instead of wrapping it. Track 0's
  TRANS baseline 12 survives a rejected TRACK_PARAM to track 4 after the first key loads the Gnome (TRANS 0),
  and leaving restores 12. The other retirement callers, `gn_kept_retire_motion` and `gn_input`, loop only
  over tracks 0 to `NTRK - 1`.
- **The ownership index audit (`gn_state.c`, `gn_easy.c`).** Before any ownership or motion-generation
  change, SET now checks scope and setting, TRACK_PARAM checks track and setting, TRACK_MIX checks track,
  TRACK_STEP checks track and step, STEP_SET checks step, MOTION checks track and event step/setting (using
  `motion_param`, as the editor does), and FM6_PUT rejects an invalid track target. Pattern track masks no
  longer wrap the editor's track. SET and TRACK_PARAM's invalid `P_COUNT` formerly matched the no-selector
  sentinel and disowned the sound; rejected pattern/event indices dropped recording undo, grid choices and
  the kept level; rejected MIX and FM6 track writes also advanced undo's motion generation. MOTION's track
  was already checked before the hook by `ed_args_ok`; its local check is defensive. The sound-disown loop,
  FM6 track mask and round-16 Wobble retirement already bound their tracks; `gn_easy.c` has no other raw
  editor index entering ownership (its remaining modulo uses are internal panel/track paths).
- `tests/gn_easy_test.c`: `t_r17_kept_invalid_track`, `t_r17_sound_invalid_setting`, and
  `t_r17_ownership_invalid_indices`. All three fail when built against round 16 (`c5a86e7`) and pass with
  the fixes under ASan / UBSan; every rejected-index case independently checks a real grid/motion undo.
  No static storage or protocol change; `.data` / `.bss` growth zero.
- Local validation uses local copies of the round-16 runners, with package/OTA/loader and
  target-disassembly checks omitted; existing `build/gen` headers are unchanged. Both host and fork runners
  pass (`host.log`, `fork-final.log`), including the full Easy ASan / UBSan suite. The fork rerun also passes
  ABOUT's layout and PNG checks using the installed runtime Python (the default Python lacks Pillow).
  An extra font-spacing probe fails with that Pillow falling back from unavailable Raqm; the normal host
  runner skips it. The optional DaisySP reference and package-dependent web checks are unavailable here.

### Round 16 (the recheck of round 15)

- **A rejected track cannot end Wobble's claim.** The editor rejects TRACK_PARAM with a track outside
  0 to 3; Easy now checks that range before retiring the rate or depth, and retirement no longer wraps the
  track number. A write of LRATE to track 4 leaves track 0's claim intact, so leaving restores its baseline.
- **A fresh press replaces its queued entry press.** When a key is released and pressed again after the
  Gnome's load but before the UI replays it, the key hook clears its old `entry_keys` bit. The fresh press
  sounds once and keeps sounding; the UI cannot send its note-on again. VOICE's mono stack can hide the
  duplicate from the voice age, so the regression counts the outgoing MIDI note-ons and checks the held voice.
- `tests/gn_easy_test.c`: `t_r16_wobble_invalid_track`, `t_r16_entry_repress`; both fail on round 15
  (14ef5c7), both pass with the fixes under ASan / UBSan. RAM: nothing added; no protocol change.

### Round 15 (the recheck of round 14)

- **The fallback Wobble's claim is to what Easy wrote.** The knob's bubble follows anyone else's write of the
  LFO's depth (gn_knob_follow) and took it as the knob's own, so leaving put Easy's baseline back over the app's
  depth. Now anyone else's write of the rate or the depth ends that half of the claim, the same value or not: the
  editor's (gn_editor, before the command runs) and any other the bubble follows. Easy's next move of Wobble
  starts that half again from the track's value then (its place, where the knob is then). No new state: an ended
  half is a marker in the baseline it no longer needs (`wob_rate`, `wob_depth`).
- **The first key always sounds.** The key that asks for the entry's Gnome is played once the sound load's
  release of track 1's notes (seq.c `panic_req`) is done, never before it (track 1 already on VOICE: no engine
  switch to wait for, so the Gnome's own load ended the key it had just started). And when the app changes track 1
  before the UI's frame sees it, the key is no longer dropped with the Gnome: it plays the app's sound.
- Accepted: a key let go before its replay (one audio block, a fade's 3 ms more on an engine switch) is not
  played; a writer no hook sees (a song's or the track's own motion of the depth) on a track not selected that
  writes exactly Easy's value is taken as Easy's (the kept settings' backstop, as before). RAM: nothing added.

### Round 14 (the recheck of round 13)

- **The entry's Gnome costs nothing.** Of the three ways asked for, the cheapest: the Gnome is shown on
  track 1 at once (the play screen, the GNOME picture, his bubbles at rest: `gn_shown`) and put there only
  when he is first used: a key on track 1 (it sounds once his engine is in, a few ms later), a knob, PRESETS,
  ARP or REC. No record is kept, so none can be lost: never used, track 1 (sound, motion) and Felucca's undo
  copy are byte for byte as before (a pick on another track meanwhile is that pick's own undo); used, he is a
  pick like any (Felucca's undo copy, taken fresh, is track 1 as it was before Easy: SAVE held in Advanced
  gives it back exactly) and stays when Easy is left. Track 1 changed by anyone else meanwhile: no Gnome.
  This replaces round 13's put-at-entry and put-back-on-leaving.
- **The fallback Wobble's LFO is a claim.** Moved off where Easy found it, another sound's Wobble brings the
  LFO's rate to Easy's 5.4 Hz; the rate, the depth and the knob's place then are kept, and put back when the
  knob comes back to that place and when Easy is left, each only while it still holds what Easy wrote; a sound
  load ends the claim.
- **The kit's hold is stopped in the recording path** (`seq.c rec_note`, one guarded line: the touch list):
  a stalled panel can no longer let a held pad tie over the steps after it. Live, a held cymbal still rings
  out and repeated hits still sound (only the recording's hold is gone).

### Round 13 (a test in the simulator)

- **Easy begins on the Gnome.** The first power-on and every HOME + SAVE entry land on the play screen with
  the Gnome (the GNOME picture's first sound) on track 1, selected. Before, entering disowned every track,
  so track 1 showed Felucca's own sound as "another sound". The Gnome is put there as a pick puts it
  (Felucca's own sound load, a fresh undo copy); leaving with track 1 just as the entry left it (only played)
  puts its own sound and the selection back (Felucca's undo swap); changed, the Gnome stays, and SAVE held in
  Advanced gives the old sound back. This replaces "entering changes nothing musical" for track 1's sound.
- **Every key plays, chromatic.** Easy's sounds already had no scale snap (GN_COMMON: QUANT off, TRANS 0); the
  black keys rested by Easy's own key map. They now play their own notes on every melodic track; another
  sound's keys are Felucca's with its scale snap off (its setting is not touched; an engine's own key map, a
  GM kit or slices, stays). Easy has no scale setting. The kit keeps its white-key pads; its black keys play
  the PERC set's sounds the pads leave out, low to high: the toms (41 43 45 47 48 50), the ride bell (53),
  the congas (62 63 64) and the shaker (69).
- **The knobs move any track's sound.** Another sound's four knobs are its engine's fallback map (one per
  engine: the engine's Bright as Easy's own sounds of it, Wobble the LFO's pitch, or its level where the pitch
  does not move it, with the LFO's rate brought to Easy's 5.4 Hz, Echo and Space the sends), each knob's place
  read from the sound as for Easy's own (gn_knob_follow); the bubbles live.
- **A kit's held pads never tie.** Two bars of drums, then a take of hats: each hat held past its step was
  recorded by Felucca as a held note (seq.c rec_hold), tying it over the following steps and wiping what they
  held. KEEP / REPLACE was not the cause (KEEP in the reproduction; it now starts KEEP at every entry, before
  it carried over between sessions), nor REC's second press (it ends the take as any). Easy's kit is a SAMPLE
  track (not the DRUM engine, whose recording has no holds): its holds are now dropped as they start.
  Reproduced headless in the simulator.

### The hold, and Felucca's own held buttons

HOME + SAVE held 2 s without a break switch; either button up, or another button down, ends the hold,
and the count starts again with the next chord of the two alone (finding 7). It counts only on presses
Felucca has not acted on. Felucca's own uses of these buttons all go through `btn_hold` (ui_input.c): HOME tap
(home), HOME held 0.7 s (the menu), SAVE tap (the SAVE pages), SAVE held 0.7 s (undo of the last sound
load); `btn_hold` marks a press it acted on with bit 1 of its `t0` and never acts on a press so marked.
When the second of the two goes down before either has fired, Easy marks both presses at once, before
Felucca's `ui_input` sees that frame, so neither becomes a tap or a hold for Felucca. A press Felucca
already acted on (the menu opened, the undo happened) never starts the hold: let go and press again.
Felucca's other held buttons (SEQ: SONG; FX, GLO, SCL, EDIT: the layers; OCT- + OCT+: the update mode)
are other buttons, and with any of them down the hold does not count. While Easy is off and the hold
counts, Felucca keeps its screen and the rest of its panel. A REC, SEQ or page-button press under way
when the chord takes the panel is no tap afterwards, on either side (Easy's REC and Felucca's REC, SEQ
and page buttons act when let go: the chord marks them acted on, and so does each crossing of the mode
boundary); otherwise REC pressed, HOME and SAVE added, REC let go and the hold completed would leave a
REC tap to fire at the next entry. `tests/gn_easy_test.c` presses every one of these combinations.

### Entering and leaving change nothing musical

Entering keeps every track's sound, settings and pattern, the tempo and the selected track; it closes
Felucca's menu, a dialog, NAME and a layer, and stops live recording. Easy's sounds are applied only
when a picture is picked. Leaving goes to Felucca's HOME page; what Easy put on the tracks stays, but is
no longer Easy's (the next entry starts with every track "another sound").

**What a pick changes, and what Felucca's undo gives back.** A pick is one of Felucca's own sound loads
(`load_begin` .. `load_end`, ui.c): the engine, the preset, the sound's own settings, the knobs at rest,
all inside the one load, so Felucca's SAVE held (0.7 s, in Advanced) swaps back the sound from before
it, and picks in a row with nothing else changed between them are one load (Felucca's own "browsing"
rule: the copy from before the first stays). A sound load never touches the track's own settings
(`ui.c param_kept`: level, pan, mute, the arpeggiator, scale and key, transpose, the pattern settings,
SLICER, chord keys and voicing), so Felucca's undo can never give those back. Easy's sounds need some of
them (arpeggio, chord keys, scale snap and transpose off so a key plays its own note; the Laser's own
arpeggio; each sound's level): these (`GN_KEPT`: level, arpeggio mode, rate, octaves and gate, chord
keys, scale snap, transpose) Easy puts back on leaving, by these rules (`gn_set_kept`, `gn_editor`,
`gn_leave`):
- Easy's claim to put a setting back starts at Easy's own first write of it on that track, and its
  baseline is what the track has at that moment: never what it had when Easy was entered;
- the claim ends at any outside write of that setting, the same value or not: the app's SET (the
  selected track), TRACK_PARAM, TRACK_MIX (the level), a track's motion (it can move the level), Play's
  knobs; and every claim ends when the tracks are replaced: a project loaded, the runtime backup put
  back, a command Easy does not know; and when the project is saved (the editor's PROJECT save, once it
  has succeeded: what the person saved is what they meant to keep, so the live tracks stay as saved).
  The panel cannot save while Easy has it (SAVE alone does nothing in Easy, and Felucca's SAVE pages are
  Advanced's); the editor's save is the only one. A sound load (PRESET, UP_LOAD, an engine) keeps these
  settings (`param_kept`), so it ends no claim;
- a song (a chain of saved projects) plays its projects' motion, and motion can move the level (of
  these settings the only one: `motion.c motion_param`), written as it plays (`motion_step`) without
  any hook: once a song is prepared or plays (`chain_busy`: the editor's SONG play accepted; the panel
  cannot start one in Easy, PLAY there is the plain loop) every claim to the level ends at Easy's next
  input call, before anything else of Easy's runs, and none is taken while it lasts, however the song
  started (one started in Advanced before Easy was entered too); a SONG play refused (no song to play)
  ends no claim. Stopping a song (SONG stop, PLAY) puts back what its
  motion moved, each to what it was when the song started or as the last sound load left it
  (`motion_end`: Felucca's own); the live tracks after a stop are those, and leaving Easy then changes
  nothing of the level;
- after a claim ended, Easy's next write of that setting starts a new one, with the track's value then
  as its baseline;
- leaving puts back each setting Easy still has a claim to and that still holds what Easy wrote (a
  backstop for a writer no hook sees: a different value stays theirs).
So project A (transpose +5), Easy, a pick (transpose 0), project B loaded (transpose 0), leaving: B as
loaded; "transpose 0" from the app stays 0; the app's +3 and then a pick: +3 on leaving; a save in Easy,
then leaving: the live tracks are the saved project (the decision of round 4). After
leaving, SAVE held gives back the whole track as it was before the first of the last picks in a row.
What it cannot give back: an earlier pick when something else changed the track between two picks (a
knob, a key of the Gnome's, the app), as with any two of Felucca's loads; a pick on another track than
the last load's (Felucca keeps one copy, for the track of its last load: picks of a sound and then the
drums leave only the drums' undoable); Easy's recordings (REC held in Easy undoes those); the pan, mute,
pattern settings and SLICER, which Easy never writes. `tests/gn_easy_test.c`: retained settings (all
of them set in Advanced, three picks, leave, SAVE held, every track, every setting, the FM6 patch and the
globals compared with before) and kept settings (A, a pick, B loaded or the runtime backup put back,
leave; outside writes equal to Easy's; new baselines after them; a save in Easy, then leave, the live
state compared with the slot loaded again, and a pick after the save; the whole state, and that state
saved and loaded again).

### Whose sound, and whose undo copy

Easy's knobs and keys act on a track only while it holds the sound Easy itself put there. Easy owns a
sound from the moment its pick's load is complete (`gn_sound_put`: the track is no one's while the sound
is made; the engine, preset and user slot it ended with are kept, then the owner is published, last);
it stops owning it at the first sign of anyone else's load, for good:
- before any editor command that loads a sound or a project runs (`gn_editor`, the editor.c hook):
  PRESET, UP_LOAD, a SET of the engine, a track's FM6 patch, a project load, a backup put back
  (BACKUP_PUT: `project_restore_runtime`), a song or any command Easy does not know (a PRESET of one of
  Easy's own sounds is then adopted as Easy's pick: round 24, below);
- before Play's PRESETS (Felucca's presets of the track) loads;
- before an editor command changes the setting that chooses the sound within its engine (the kit's
  SET, an FM6 track's PTCH: a SET of the selected track, TRACK_PARAM), and before an FM6 bank slot is
  put or erased (FM6_PUT and FM6_ERASE of a bank slot: every FM6 track whose PTCH, or loaded patch, is
  that slot reloads it: `fm6_bank_put`, `fm6_poll`); a bank slot no track plays leaves Easy's sounds
  Easy's;
- and, as a backstop for any writer no hook sees (a song's motion of PTCH or SET, say), at Easy's first
  input call at which the track's engine, preset, user slot, that SET or PTCH, or (FM6) the patch
  generation (`fm6_pgen`: any patch loaded since, even under the same PTCH) is not what Easy's load
  left: even if they come back, the sound is no longer Easy's. That check runs at every input call,
  HOME + SAVE held or not (a song moving PTCH away and back while the hold counts and is let go before
  2 s is seen). The audio interrupt's key hook asks the same of the track at every key.

What the editor's commands leave alone, Easy on (`gn_editor`, by what each command does): the reads,
INFO GET DUMP DESC STEP_GET NAMES SMP_INFO UP_LIST UP_GET WATCH PING TRACK (a selection moves the keys
to that track: gn_input) TRACK_DUMP UI_STATE UI_PALETTES FAV_GET BACKUP_LIST BACKUP_GET FM6_GET FM6_LIST,
TRACK_STEP TRACK_PARAM TRACK_MIX and MOTION without a value, PROJECT's query, SONG's read, the synth's own
pushes (CHANGED RELOAD STEP_CHANGED TRACK_CHANGED, which ed_handle ignores); and the writes that touch no
track: UI_SET FAV_SET (preferences), UP_PUT UP_STORE UP_ERASE (the user bank), SMP_* (user samples),
FM6_PUT and FM6_ERASE of a bank slot no track plays, PROJECT's save, SONG's chain and its stop, BACKUP_PUT's begin, chunks, abort
and the commit of the settings, the project slots and the user banks. SONG's play (op 2) drops the undo
copy (a song plays its projects' patterns) and, once the song is prepared, the claims to the level end
(their motion; a play refused ends none); SONG's stop
(op 3) changes no ownership, but is a stop: what motion moved goes back (`motion_end`); BACKUP_PUT's
commit of the FM6 bank ends sound ownership only. The web editor reads SONG when it connects and after every project save or load
(web/editor.html); `tests/gn_easy_test.c` (the editor's reads) sends every read the protocol has as the
editor does, a save then SONG, the backup page's reads, preferences and the user bank, Easy on (the
Gnome, the drums and a recording still Easy's: the pads, the knobs, REC held) and off (Easy's state and
the music unchanged).

**Round 24 (finding F6): the app's PRESET of one of Easy's sounds is Easy's pick.** When the phone app loads,
by the editor's PRESET (engine, preset), one of the 49 sounds while Easy runs, Easy adopts the load as its own pick
of that sound: right after the command, before the app's next command and before Easy's next input call
(`gn_easy.c gn_adopt_of`, read by `gn_editor` before the load runs; `gn_adopt`), the track gets Easy's whole sound
(`gn_sound_put`: the settings, the GN_KEPT claims against what the app's load left, the knobs at rest), Easy owns
it and Easy's screen shows the pick (a picture's first sound) or the peek list, not Felucca's screen for another
sound. No new command: the app sends the PRESET it always sent. Which sound: the one the track shows when it is
that preset (the Gnome, shown or put, stays the Gnome); else the sound named for the preset (a picture's first
sound is named for the picture: VOICE VOX LEAD is Vox lead, FM6 TINE EP Tine piano, VOICE WOW BASS Wow bass);
else the picture's own. Never the kit: SAMPLE preset 0 is PIANO (the kit is its SET, PERC). The adoption takes
place only if the load landed (the track's engine and preset are the sound's, no user slot), and it is one of
Felucca's loads in a row with the app's, so SAVE held in Advanced gives back the sound from before the app's load.
Every other load stays the app's, as below (UP_LOAD, an engine by SET, a preset none of Easy's, a project, a
backup). One byte of state (`gn.adopt`, in `sw_mark`'s padding): no RAM. `tests/gn_easy_test.c` `t_r24_adopt`;
`t_ownership` now checks the same factory sound from the app as adopted (all but the kit); the matrix of
`t_knobs` loads the presets none of Easy's (225 loads).

Reloading the drums' own preset (SAMPLE preset 0) is the app's sound: the keys play it as Felucca does, the
knobs rest, and picking the picture again puts all of Easy's sound back (the drums set up again). Easy's undo copy (REC held) is of Easy's own last
recording only, and goes before any editor command that writes steps, pattern settings, motion, a
song, a project or a backup runs, whether Easy is recording or not and whether a recorded key is still
held: "recording" is no proof of who wrote. `tests/gn_easy_test.c` (ownership; undo: the app's writes)
checks each of these from each of the nine pictures against the state the load left, read off the
tracks and the voices, never off Easy's own idea of what it owns.

### Sounds

49 sounds in nine pictures (`gn_state.c GN_SOUNDS`, `GN_GROUP`), each a factory preset of 1.0.1 plus a few
settings, and each first gets the LFO Wobble uses (5.4 Hz) with the track's arpeggio, chord keys, scale snap
and transpose off (a sound load keeps those; they would make a key play other than its own note). Each
picture's first sound is its sound of rounds 1 to 6: the Gnome VOICE VOX LEAD, one voice, a syllable per key;
the Organ WHEEL FULL ORGAN; the Bells (GLASS BELL) FM6 BELL; the Laser LOFI ARP 8BIT with the arpeggio (up,
1/16, two octaves); the Whale VOICE WOW BASS an octave down with glide; the Choir VOICE CHOIR AAH and the Cloud
GRAIN CLOUD PAD made to speak at once; the Piano FM6 TINE EP; the Kit SAMPLE's PERC set (upstream's own kit
for a 0.9 project's drum part, `project.c proj_legacy_perc`, with attack 0, sustain full, a release of about
6 s, POLY, no chorus). The rest map to 1.0.1's presets by name (FACTS-SYNTH-1.0.md); where a name had no
match, or its match is already the picture's first sound: Full organ is FM6 ORGAN (its patch DRAWBARS),
8-bit arp TRIO ARP LEAD with the Laser's arpeggio, Nylon guitar FM6 PLUCK (its patch NYLON PICK), Chopped
break SLICE CHOP, Marimba FM6 MARIMBA (PHYS has one too), Pluck PHYS PLUCK, Brass FM6 BRASS, Soft pad ANALOG
SOFT PAD, Drum machine DRUM KIT, Tine piano FM6 TINE EP (the Piano's too, with the Organ's own maps).

**Round 24 (a test on the synth).** The Cloud's Wind and Rain (NOISE WIND, NOISE RAIN) were "just
noise" in that test: they are now String pad, PHASE STRING (attack made to speak at once, DCW's Bright 50 at 0,
70 at rest, 127 at 16: DCW below 50 is no darker on this preset), and Warm strings, ANALOG PWM STR (attack 20,
CUT's Bright). Both chosen by preset name from FACTS-SYNTH-1.0.md, never from an engine's source; two others were
tried and refused by the sound test: FM6 PAD (its patch's own slow attack: a 40 ms tap is silent) and PHYS BOWED
METAL (too quiet even at the top level, and its BRIT no Bright). The Whale is basses: each of its sounds plays
its lowest white key (key 0, F3 at the middle octave) as a bass note. SUB BASS, SQUARE BASS and FM BASS were at
the panel's own octave (key 0 at 174.6 Hz); they are now an octave down like the Whale itself (87.3 Hz), FAT
BASS's preset has its third oscillator an octave down already (87.3 Hz at octave 0). FM BASS's Bright rests a
quarter of the way (MLVL 0 at 4) since an octave down MLVL below 0 changes it too little. `gn_sound_test`
measures each Whale sound's lowest key (the fundamental by autocorrelation, 35 to 110 Hz) besides every check
below.

Every sound has its own Bright, Wobble, Echo and Space (const flash, keyed by the sound Easy remembers on the
track). A knob is two straight pieces, the setting's lo at 0, the sound's own value at rest, hi at 16, and
may move a second setting with it, so a setting that runs out in one half hands over: Bright is the
engine's brightness (VOICE the formants' SHIFT with BUZZ, WHEEL TOP then the DRIVE (the Organ, Rock drive)
or TOP up from the bars off (Gospel, Jazz, Soft flute), FM6 MLVL (FULL ORGAN, whose patch ignores it: the
track's DIST), LOFI and SLICE TONE, GRAIN TONE then a brighter source, PHASE DCW, TRIO CUT, PHYS BRIT, NOISE
FREQ, ANALOG CUT (SUB BASS, a sine: DIST), SAMPLE CUT, DRUM TONE); Wobble the LFO to pitch (0 to +-1.9
semitones; DRUM and SLICE, which the LFO's pitch does not move: a tremolo, LFO to AMP); Echo and
Space the delay and reverb sends (Space also lengthens the release of the short ones: Arcade, Pluck,
the drum machine's DECY). `tests/gn_sound_test.c` plays every sound through the firmware's own audio and
checks: a 40 ms tap heard within 25 ms; the pictures' first sounds within 6 dB of each other, every melodic
sound within 10 dB (8.2; 7.4 before round 24); each knob at 16 against 0: Bright >= 6 dB more above 2 kHz or the spectral centroid
1.5 x as high; Wobble a swing of the pitch (the strongest spectral peak, against the same note at 0) of >= 25
cents, or of loudness of >= 3 dB; Echo: the first echo (the two renders' difference, the echo alone) no more
than 10 dB under the note's first 60 ms; Space: the reverb 0.1 to 0.5 s after the tap no more than 12 dB
under it; and across the whole travel: 0 against 8 and 8 against 16 each at least half of that (Bright 3 dB
or 1.22 x, Wobble 12.5 cents or 1.5 dB, Echo within 16 dB, Space within 18 dB of the note). The pads: each
above -32 dBFS over 500 Hz but the kick, toms and congas (no pad) quieter there.

The kit reaches further (round 7, pick 10): its Bright, which rested at its top (CUT
open) and so reached nothing past it, rests at 10 with the CUT open and climbs into the sample's BITS crunch
to 16 (+1.1 dB above 2 kHz, the centroid 1.42 x past rest); its Wobble's top is +-5.6 semitones (+-1.9 before);
its Space adds the chorus send above rest (at 16 -22.6 dB against the note's -15.4, -36.5 at rest). Its
Echo's send was at its top already, and a track's send cannot go past it. The sound at rest is as before.

### Demo grooves (2026-10-11)

Five grooves in flash, read only (`gn_grooves.c`, `GN_DEMO_GROOVES` in `gn_hooks.h`, default 1), so a new player
hears what the synth can do. Each is all four tracks (track 1 a lead, 2 chords, 3 a bass, 4 Easy's kit), each track
one of Easy's 49 sounds with its Echo and Space, its level, its steps (velocity in four classes, ties, chords of up to
four notes), LEN DIV SWING GATE, and the groove's tempo. The music is original, written for these sounds:

| Groove | Style | BPM | Bars | Lead | Chords | Bass | Kit |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Groove | Style | BPM | Bars | Lead | Chords | Bass | Drums (DRUM MACHINE: TONE DECY SNAP KICK) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| HEAD NOD | hip-hop, swung 30 % | 90 | 2 (1/16) | GLASS BELL (an octave down, Bright 3) | PIANO (Dm9, Bbmaj7) | FM BASS | 50 64 48 ROUND |
| FLOOR FILLER | house, swung 10 % | 124 | 1 (1/16) | PLUCK | ORGAN stabs (Am7, voiced high) | SQUARE BASS (Bright 3) | 64 52 52 PUNCH |
| RAINY DESK | lo-fi study, swung 40 % | 78 | 4 (1/16) | SOFT FLUTE | NYLON GUITAR (Fmaj7 Em7 Dm7 Cmaj7) | SUB BASS | 40 56 40 ROUND (2 bars again) |
| NIGHT BUS | drum and bass, half-time | 170 | 2 (1/16) | VOX LEAD | SHIMMER (Fm9, Dbmaj7) | FAT BASS | 58 50 48 PUNCH |
| SKY GARDEN | dreamy ambient | 100 | 8 (1/8) | KALIMBA | CHOIR | WHALE | 45 72 40 ROUND (2 bars again) |

The bass stays in E1..B2 (at or below the octave round 24 moved the Whale sounds' keys to), the drums only on
Easy's pads (`GN_PADS`, `GN_BLACK_PADS`), one hat a step (the DRUM engine's hat lane is mono). v2 (after a listening test of v1):
the drums moved from KIT (SAMPLE PERC: 22 kHz ADPCM, closed hat 155 ms with its centroid at 2.6 kHz, cymbals cut to
0.5 s) to the DRUM engine (closed hat 50-60 ms at about 9 kHz with SNAP 40-52, open hat 255-335 ms); each groove's
`kit[]` writes TONE, DECY, SNAP and KICK after the pick (no new Easy sound). Each track's level was set from the
firmware's own audio with the other three muted (10 s each), the kits 3 dB down so the master limiter (`fx.c LIM_T`)
acts at most 1.2-2.4 dB on 1-3 % of the time (v1: up to 3.7 dB, up to 11 %).

**Where.** The loop page (SEQ, the circle): PRESETS turns through the grooves; a detent loads the next one on all four
tracks and plays it from the top (an external clock: the tempo and the transport stay the clock's). The list (the
drums' picture, the five names, the one loaded inverted, the band GROOVES (PRESETS)) shows 2 s in the peek list's
place. Off the loop page PRESETS is the sound inside the picture as before. *Alternative considered:* a tenth picture
GROOVES on ALGORITHM. Not taken: a picture is one track's sound and a groove is four tracks' music, so it would need
its own art, and the nine pictures are what every screen, LED and test counts.

**Whose music.** A groove loads only when nothing of the player's is lost: every track empty or still the steps a
pattern load put there (upstream's own `ui.c pat_sig`: a groove sets it as SEQ > PATTERNS does), or the four tracks'
steps exactly as one of the four project slots holds them (saved). Otherwise nothing changes and the band says SAVE
FIRST (SAVE). A load writes no project slot: the four slots stay the player's. Each track's sound is an Easy pick
(`gn_sound_put`, Easy owns it), the knobs Easy's, the level a kept write (put back on leaving); arming, a take, a SAVE
waiting and Easy's undo copy end (`gn_rec_cancel`, `gn_undo_drop`). A groove is a new song: Easy's claim on its song
slot ends (the earlier song stays in its slot), so the next SAVE takes the first empty slot, or says FULL. State: one
byte in `gn` and one in `gs`, both in alignment padding (no `.bss` growth); data 991 B of tables and events.

**Tests.** `tests/gn_easy_test.c`: `t_grooves_defined`, `t_grooves_load`, `t_grooves_owner`, `t_grooves_rules`;
`gn_draw_test` every list state; `gn_screen` the approved `easy-12-grooves` and `easy-12-grooves-save-first`. Heard in
the simulator (gn/sim, headless: SEQ, then PRESETS +n), 8 bars each.

**Budget (v2: run 38194209281, 7aa23ac).** Image 533,740 B, +56,732 B against the base (allowance +57,344: 612 B
left; before the grooves +54,372, so they cost 2,360 B); `.data` +4, `.bss` +3,584, `.data+.bss` +3,584 (unchanged:
0 B left), pool, `.noinit`, `.ram_text` +0; stacks: `fm1_cstart` 3,632 B, the audio IRQ 928 B, `ui_draw` via Easy
1,100 B, `ui_input` via Easy 1,324 B. All inside EASY's line. v2 was also rendered with
FLOOR FILLER's drums alone, 16 bars, old kit against new (the same gain).

### Round 26: the recording's metronome (2026-10-11)

**What.** A click on each beat while recording, the bar's 1 accented (G6, louder) and the other beats plain (C6),
and on each of the count-in's four beats (the first accented: "4" is the bar's 1). It clicks while a take records
(REC armed then PLAY's count-in, a punch, REC tapped while playing once bar 1 comes) and while Easy is armed and the
loop plays (the beats up to bar 1: a count-in of its own); it is silent while the loop only plays and while stopped.
In Advanced it clicks while Felucca's own recording runs (`song.rec`, the loop playing): the same setting, so the
choice made in Easy holds there too (Advanced has no switch of its own: that needs a menu or a protocol command).

**The switch: EDIT held + K3**, on any Easy page (pick 7's record screen has no free knob: K1 MOVE / BARS, K2 NOTE
/ REST / SWING, K3 SNAP, K4 KEEP / REPLACE). While EDIT is down, K3's card reads CLICK ON / CLICK OFF in K3's place
(the card lights in line with the knob); right is ON, left OFF; let go, K3 is what it was (the record screen's
SNAP untouched). EDIT's press is still tap tempo's (one tap changes no tempo: Felucca's needs three); turning K3 ends
the tap screen so the page and its card show. Screen `easy-26-record-click` (new).

**Default ON (recommended), kept in the settings record.** Bit 2 of the mode byte (favourites' row 15, byte 30, beside
the mode bit; no record growth) is the metronome **OFF**, so every record written before round 26 (bits 3 and 2 zero)
and every first power-on reads ON with no migration. `gn_mode_keep` carries the bit across mode switches; the switch
writes it and queues the normal settings save (written while stopped, only when the record changed), so it survives a
power cycle. A record without Gnomad's marker never clicks (upstream's tests, the hooks-inert runs).
*Alternative:* default OFF (click only for those who ask): bit 2 would mean ON and nothing else changes; not taken
because a count-in that only shows digits is easy to miss, and a click while recording was asked for.

**How it sounds and where it goes.** `gn_click` (`gn_state.c`, out of line, called from `audio.c audio_block`
after `uac_tap`): a 1,024-sample (23 ms) sine blip, the sine a parabola (no table, no sample), a falling square
envelope, 32-bit integer only; added to both channels of the master output and saturated (the limiter keeps the
music under 18,000 of 32,767; the accent peaks at 7,000). It takes no voice of the four tracks, is never written into
a pattern, never reaches MIDI (USB or TRS) and never the USB audio input (it is added after the USB tap). The beat is
the sequencer's own (`seq.c beat_pos`, `beat_n`, reset by `seq_start`; the external clock's when it follows one),
read once a block in the ISR: the click starts on the block the beat begins in (0.73 ms), not on a UI frame. The
count-in sets `beat_pos` / `beat_n` from its start (`gn_play_btn`, IRQ off) so its four clicks are its four digits;
a fifth is never heard (the loop's start, a frame later, is the next 1). One byte of state (`gn.click`: blocks since
the click began, bit 7 the accent; `undo_of`'s padding byte); a new click waits 120 blocks (87 ms), so a beat an
external clock's uneven advance shows twice clicks once.

**Tests** (`tests/gn_easy_test.c`, round 26): the scenario run twice in processes of their own, click ON and OFF:
the count-in's four clicks a beat apart (120 BPM: 22,050 samples, to a block), its first accented, no fifth; two
bars recorded, eight clicks a beat apart, each bar's 1 accented, the first on the loop's start; nothing after REC is
let go. OFF: the codec's output equals the USB audio's; ON against OFF: the USB audio the same samples, the recorded
steps the same, the voices sounding every frame the same, the MIDI/SysEx out the same. Armed while playing: clicks
from the arming only, an accent every fourth, the take beginning at an accented 1. The switch: the card, SNAP
untouched, OFF written to flash, kept into Advanced and across a power cycle, back ON across another; no marker no
click. Advanced: Felucca's recording clicks (an accent each bar), none when OFF. `tests/gn_host.h` runs `gn_click`
after `mix_block` as `audio_block` does, and captures the USB audio's stream before it.

**Budget (run 38201479533, 2da44ee, against a546a8a's run 38197004519).** Image 556,592 B, +79,584 B against the base
(+556 B for round 26; V1 allows +81,920: 2,336 B left; 24,972 B of the slot free). `.data` +4 (unchanged). `.bss`
+3,648 (+16 B: no symbol grew, `gn` is 0x6a8 B in both, every `.bss` symbol the same size; the linker's alignment of
the objects after `gn` moved by 8 and 16 B), V1 allows +4,096. Stacks: the audio IRQ +32 B against the base (as
before: `gn_click` is shallower than the deepest path), `fm1_cstart` +56. Target cost: `fm1_alnk0_irq` 37,498 of its
37,502 line (+3: the call), every engine line unchanged.

### Drawing

Round 7 gives every Easy screen but 3g, 3h and 3i one frame (`gn_screens.c`, shared elements): the header
(0 to 30: the selected track's badge in its colour, inverted while armed, the whole header in the track's
colour while recording; PICTURE * SOUND, or another sound's own name; the tempo, "BPM IN" on an external
clock), the knob row (31 to 69: the four bubbles, or labelled cards on the loop pages and the volumes, K4's
card KEEP or REPLACE while armed or recording), the field (70 to 210) and the band (210 to 240: the loop's
state and what the buttons do here, in brackets). The parts and the strip budget are unchanged.

Each frame `gn_draw` splits the screen into parts, each a box with a signature of what is drawn in
it, and redraws the parts not on the screen yet, a strip at a time, at most 240 x 32 pixels a call
(the first strip of a call always): then back to the main loop, so the editor, the update entry and
USB are served between strips (finding 6). A whole screen takes 8 calls. A part counts as drawn only
when every strip that drew it drew the same signature. Felucca's `ui.force` (every editor SET sets
it) is no reason for a whole screen; a new view is, a frame Felucca drew (the update countdown) is,
and `ui.force` after the main loop stood still for a second is (an update session that returned).

Leaving goes through the same budget: `gn_draw` clears the screen to Felucca's ground a strip of 240 x
32 a frame (8 frames, about 0.12 s; Felucca's input runs as soon as the chord is let go, its drawing
waits), then hands the
screen back with `ui.force`, and Felucca draws its page whole in its own next frame. That frame, and the
update countdown's (main.c: OCT- + OCT+ held past 2 s, in Easy too; `draw_uboot` fills all below the
header), are Felucca's own drawing, upstream's, outside Easy's budget: a page change of Felucca's sends
the whole screen, 57,600 pixels (its HOME page, upstream's palettes), in one frame, in Advanced as after
Easy; the countdown sends 74,160 pixels in the frame of each new second (3, 2, 1: the screen below the
header, its text, the keycaps, the header) and none while a second stands, in Advanced as in Easy: at
the LCD's 12 MHz, 77 ms and 99 ms of wire time, well over the 15 ms frame (Felucca's own, not bounded
here). The countdown during the leaving's clearing ends the clearing (the countdown covers the screen);
after the countdown Easy draws its whole screen again strip by strip. `tests/gn_easy_test.c` counts
every pixel sent, fills and blits (the host's own LCD counter), in every frame: Easy's at most 7,680
each, and every frame in Easy is Easy's own but the countdown's; the leaving is 8 frames, 57,600 pixels
in all, then Felucca's first frame, exactly its own page change, 57,600 (with upstream's palettes and,
in the merged firmware, with GNOMAD: the palette session asserts the same figures), after which the
screen is exactly Felucca's page drawn on a cleared screen; the countdown's frames in Easy are exactly
Felucca's own, 74,160 at each of 3, 2 and 1, and 0 while a second stands; the largest frame of the whole
sequence is 74,160, the countdown's.

### Found in upstream 1.0.1 while testing Easy (not ours to change; to offer upstream, rule 3)

- **SAVE held (undo) of a track with knob motion, while the loop plays, leaves a setting out of its
  engine's range.** `ui.c undo_swap` swaps the track's motion events back (`motion_replace_track`, which
  rebases `motion_base` on the sound being swapped out), then swaps the sound; at the loop's next turn
  `motion.c motion_restore` writes those base values back unclamped. Seen in Easy's random walk with Easy
  off: GRAIN's PTCH (-24..24) given VOICE's BUZZ 90. Most engines mask what they index tables with
  (`& 15`, `% N`), so this was a wrong value rather than a read out of a table in what was checked; not
  every use was checked. The walk in `tests/gn_easy_test.c` never holds SAVE that long, so it tests Easy's
  writes only.
- Upstream's DSP shifts negative values left (`fm6_core.c`, `fx.c`, `eng_analog.c`, `eng_wheel.c`, ...)
  and `fm6_core.c:341` overflows a signed add, as UBSan reports: defined by every compiler upstream uses,
  undefined in C. Our tests exempt upstream's sources from UBSan (`tests/gn_ubsan_upstream.txt`).
| `firmware/src/felucca.c` | the build's name: `FELUCCA_VERSION` from `build/gen/gn_name.h` when CI wrote it and nothing else defines it (5 lines, before upstream's default). The skin: `GSK_BUILD` (default 1) after the build options, `gn_skin.h` before `gfx.c`, `gn_skin.c` after `ui_layer.c` (9 lines) |
| `firmware/src/gfx.c` | the skin (each under `#if GSK_BUILD`, 17 lines): `palette_set` calls `gsk_palette(i)` after the derivation; `palette_from_stored` reads 13 as GNOMAD; `palette_to_stored` is `gsk_palette_to_stored` and `palette_stored_ok` is `gsk_palette_stored_ok` (a tagged GNOMAD, 72, refused; upstream's two lines kept under `#else`); `cv_keycap` calls `gsk_keycap` (a knob's badge in its colour) |
| `firmware/src/ui_draw.c` | the skin (12 lines): `draw_column` entry (the card), `draw_head` (the message in words, before upstream's `cv_free_hint`), `draw_foot` entry, `draw_confirm` entry (its No / Yes are literals inside `draw_confirm`, which `confirm_text` alone cannot reach; the target's name is drawn as stored) |
| `firmware/src/ui_graph.c` | the skin (18 lines): `graph_grid` entry, `graph_chord` (its caption), `graph_mod` entry (the slots' sources and destinations in words, measured), `graph_slices` (the length), `panel_note` (its words), `graph_song` entry |
| `firmware/src/ui_layer.c` | the skin (9 lines): `lcell` (its name; a waiting repeat in second ink), `layer_scl` entry (the scale cells in words, measured), `draw_layer` (the footer's key words, before upstream's `cv_key_row`) |
| `firmware/src/ui_menu.c` | the skin (3 lines): `draw_menu` entry |
| `firmware/src/ui_name.c` | the skin (9 lines): `nm_title` (in place, at its end: upstream's own title by NAME's kind, never the name), `nm_draw_panel` entry, `nm_draw_foot` entry |
| `firmware/src/ui_input.c` | the skin (6 lines): `setup_title` entry, `setup_show` (its prompt); the input logic untouched |
| `tools/gen_ui_palettes.py` | the skin: two lines before the header is written call `tools/gn_ui_palette.py`, which adds the GNOMAD record (named `GN_NAME`) and the count 9, both under `#if GSK_BUILD`; upstream's eight records and checks unchanged |

## The Advanced skin, phase 1 (`gn/skin-1.0`)

Option C (chosen 2026-10-05): Advanced is Leo's 1.0.1 page system re-skinned, in our
paper and ink, one colour per knob (1 blue `#2352E6`, 2 green `#0B7D4F`, 3 yellow `#F4B400`, 4 vermilion
`#F2622A`) and plain words instead of codes. Phase 1 (option C, sections 1, 2, 3, 7, 8) is the skin
everywhere; our rows, row frame and pictures (sections 4 to 6) are phases 2 to 4.

- **The switch.** MENU > COLOR > GNOMAD, the ninth palette (runtime index `GSK_PAL` 8; the editor lists it
  too). Settings store it as **13**, the legacy PAPER id (`GSK_STORED`): unmodified 1.0.1 reads 13 as PAPER
  and accepts it in its import and backup validation, where `64 + 8` would be refused; a tagged PAPER (70)
  stays PAPER. Saving again in 1.0.1 loses GNOMAD, never the settings. No structure, format or flash object
  changes. Any of the eight palettes restores Leo's rendering completely: every hook returns at once. Only
  `palette_set` sets the skin's state (`gsk.on`); Easy's entry and exit never touch it.
- **72**, a tagged GNOMAD, is written by nothing. Gnomad refuses it where 1.0.1 does, at the one guarded spot
  `palette_stored_ok` (`gsk_palette_stored_ok`): a backup PUT carrying 72 is refused (rc 2, nothing
  written) in both builds. A flash record holding 72 (nothing in either build writes one)
  imports in Gnomad as GNOMAD and is saved again as 13; 1.0.1 reads it as MONO.
- **The build flag.** `GSK_BUILD`, 1 in `felucca.c`. Upstream's host tests include the sources one by one,
  without it: they compile them as upstream does (eight palettes, 13 is PAPER) and stay green unchanged.
  Ours build with `-DGSK_BUILD=1`.
- **Its names.** Everything of the skin's is `gsk`, `gsk_*` or `GSK_*` (state, functions, tables, enums,
  macros), so nothing can collide with Easy's `gn`. The only shared name is `GN_NAME`, defined in `gn_skin.h`
  under `#ifndef`: if Easy defines it first, the skin uses Easy's.
- **The code.** `firmware/src/gn_skin.h` (declarations, `GN_NAME`, the state), `gn_skin.c` (the palette's
  exact roles, `gsk_paint`, the card, and the functions drawn in its words: footer, menu, dialog, NAME's
  panel and footer, calibration's title, the song list, the drum grid, the MOD slots, the scale layer),
  `gn_skin_words.c` (the word tables and the one lookup, `gsk_word(context, id, form, fallback)`, keyed by
  family, scope, graph or parameter and checked against upstream's code; an unknown page or word shows
  upstream's text). `gsk_paint` and `gsk_word` are option C's `gn_skin_paint` and `gn_word`, renamed. Every
  skin function is built for size (`GSK_MIN`, `minsize`), as upstream's UI files are. RAM: `gsk.on` and a 40 B
  phrase buffer (`gsk.made`); no canvas, no scratch.
- **Whose words.** The skin translates upstream's own words by where they come from (the card's descriptor
  and value, the site that draws a phrase, the dialog's or NAME's kind), never by matching what a string says.
  User content is drawn verbatim: a sound, project or pattern name never passes through `gsk_text` or
  `gsk_word`, so a sound called `INIT` or `SAVE ERROR` shows as `INIT` or `SAVE ERROR`, in the dialogs, the
  load message and the footer, and its stored bytes are unchanged (tested with names matching every phrase
  and wildcard of the tables). Destructive confirmations name their target as stored.
- **Choices and status in words**, measured before drawing: card values (the choice lists, packed per
  descriptor, and the units), the scale layer's cells (`layer_scl`), the MOD rows' sources and destinations
  (`graph_mod`), the layer and NAME footers. A choice word too long for Leo's cell has a shorter cell form
  (`Chrom.`, `Mixolyd.`, `Maj pent`, `Harm min`, `Whole tn`, `Dim h/w`, `Scale3`, `Dom7`, `Bass oct`,
  `Res tri`, `3 saws`, `Sympath.`, ...); phase 2's rows say them in full.
- **What stays upstream's, deliberately** (tests/gn_skin_test.c `GSKT_KEEP`; a token outside this list that
  the default build draws untranslated fails the test): note names `C` to `B`; engine names (`ANALOG`, `PHASE`,
  `LOFI`, `SAMPLE`, `VOICE`, `TRIO`, `WHEEL`, `GRAIN`, `PHYS`, `DRUM`, `NOISE`, `FM6`, `SLICE`); the user slot
  `U01`; the sample sets `PIANO`, `FLUTE`, `SAX`, `PERC`, `USR1` to `USR3`; `[OCT+]`; the engine slots `E6`,
  `E8` where no engine word applies; FM6's algorithm numbers; the slicer's divisions `4` to `32`; the noise
  registers (`L23` ... `L7`, `127B`, `63B`); and the tokens no plain word fits in Leo's cell: `SPLS`, `SQ50`,
  `SQ25`, `SQ12`, `P&S`, `P+N`, `SYNCP`, `R+S`, `R+SP`, `MEMB`, `H+CYM`. Also upstream's: the cents unit `ct`
  on the card (`127 cents` does not fit), the mixer's `PAN` / `REV` captions, `graph_slots`' `USED`, the update
  countdown and every user name.
- **Where it differs from option C**: the shorter cell forms above; the code beside a word is
  drawn in the card's own S face and fits only on short words; GNOMAD's accent is ink, so a graph not reworked
  here loses Leo's accent-versus-text distinction; the divider (option C's exact wells on paper) has a contrast
  of 1.24, where upstream's generator asks 1.25 of a derived palette's 1 px dividers, and the yellow spine 1.56
  against ground (inside its ink keyline); the GLO layer's cards carry the knob spines while labelled Part 1 to
  4; the editor lists nine palettes.
- **Tests** (`tests/gn_skin_tests.sh`): `gn_skin_test.c` (the roles, the menu, every word entry matching
  upstream, complete coverage, the fits, the keep-list, names verbatim); `gn_skin_persist_test.c` against this
  tree and the base (13, 70 and 72 through import, export, the editor's real preferences handler and a full
  BACKUP_LIST / GET / PUT round trip, in both builds, and the record each build wrote read back by the other);
  `gn_skin_render.c inert` against both (every scene of upstream's host renderer, every page of every engine
  and descriptor variant, HOME, the hold layers and every value of every card, in the eight palettes, and
  successive unforced frames of a scripted session: byte-identical; plus no trace left after GNOMAD, the
  framebuffer kept across a real palette change); `gn_skin_render.c sheet` (GNOMAD's screens, the variant
  cross-products, all ten confirmation kinds and every card value through upstream's layout lint; `ops` the
  five FM4 pages) and `gn_skin_sheet.py` (the PNGs, 2x, the labelled sheet).
- **Numbers** (CI run 37380314852, commit 7c02ff9): the image is 498,836 B of 581,564 (the base: 477,040),
  **+21,796 B** against option C's 12,288 B allowance: over by 9,508 B (the app slot keeps 82,728 B free).
  `.data+.bss` 88,916 B of 98,304 (the base: 88,884), +32 B against the 64 B allowance. Stack (a host proxy:
  `-fstack-usage` for thumbv7m, nothing inlined): the deepest skin path is `draw_foot` 200 B, `gsk_foot` 232,
  `gsk_keyrow_ctx` 144, `cv_key_row` 56 and upstream's text below, about 376 B more than upstream's own footer
  path; the target's real figure needs the JieLi linker's. To trim, in order of yield: pack the word tables
  (16-bit keys and offsets into one string blob, the upstream-code check moved to the host test, which already
  proves every entry): about 3 KB, nothing visible; move the song list, drum grid and MOD slot redraws to phase
  2, whose rows replace those pages: about 2 KB; translate the footer, menu and NAME panel in place instead of
  redrawing them: about 1.5 KB, at the cost of more guarded upstream lines; keep only the frequent messages:
  about 1 KB. That is about 7.5 KB, still about 2 KB over; the rest would have to come from the choice words or
  the dialog's redraw (a host proxy, clang -Oz for thumbv7m: the skin's code 9.8 KB, tables 5.7 KB, strings
  3.9 KB).

### Merging with Easy

The skin draws only from guarded lines inside upstream's functions and never consumes input; it adds no
state to `ui`, no flag to `felucca_dbg` and nothing to the main loop or the input path. With Easy merged,
`ui_draw` keeps this order: `ui.frame++`; Easy's `gn_draw()` (returns if it drew); upstream's modal and layer
handling (update, menu, confirmation, NAME, layer); `cursor_fix()`; the skin's page hook (phase 2:
`if (!gsk_page()) { upstream's draw_frame, draw_head, draw_columns, draw_graph }`) or upstream's page; then
the one timer tail (the second message, `bpm_t`, `hot_t`) and `draw_foot`. Phase 1 adds nothing to `ui_draw`
itself, so it cannot conflict there. In `felucca.c` the skin needs only: `GSK_BUILD` after the build options,
`gn_skin.h` before `gfx.c` (gfx.c's guarded lines call it), and `gn_skin.c` after `ui_layer.c` (it uses the UI
files' statics) and before storage, the editor and `main.c` (which call nothing of the skin's). Easy's includes
can sit anywhere else; the two branches' lines do not overlap. If both define `GN_NAME`, keep Easy's: the skin's
is under `#ifndef`.

## Gnomad engines (branch `gn/engines`)

Gnomad's own sound core (gnomad-core: five engines, VA, FM, DRUMS, GNOME VOICE, SAMPLE, with its own echo, chorus and
reverb) plays in the firmware in place of six of Felucca's engines. `GN_ENGINES` (`firmware/src/gn_engines.h`, which only
felucca.c includes, first) switches it on: 1 by default, 0 with `FELUCCA_GN` 0 or with `#define GN_ENGINES 0` in
`build/gen/gn_engines_config.h` (the build interface, `tools/build.py`, is upstream's and takes no new flag). Upstream's
host tests and the fork's earlier tests never include felucca.c, so they build Felucca's engines as before: the golden
regression (87 renders), the hooks-in proofs and Easy's tests are unchanged; `tests/gn_core_test.c` builds the swap.

### Retired, kept

- **Retired with the swap** (the replacement list; gn/size-plan's flags, a413386): PHYS, GRAIN, WHEEL, FM6, SLICE,
  DRUM (`GN_DROP_<ENGINE>` 1, forced: their slots are the Gnomad engines') and the bundled factory sample block
  (`GN_DROP_SAMPLES` 1 by default; `#define GN_DROP_SAMPLES 0` in `build/gen/gn_engines_config.h` keeps it, 196,816 B).
  Measured on 088befd: the six engines and the samples together free 244,392 B of image, 21,328 B of RAM and 90,400 B
  of pool (size-measurements.md, "replacement").
- **Kept for now**, each droppable by its own flag for a later decision (`#define GN_DROP_NOISE 1` and so on: a
  silent descriptor in its slot): ANALOG, PHASE, LOFI, VOICE, TRIO, NOISE, SAMPLE. With the sample block out SAMPLE
  keeps its user slots (USR1..3) and its built-in sets play nothing.

### Slots

The stores (projects, user presets, favourites) and the editor protocol hold engine numbers, and `NENGINES` stays 14
(favourites' row 15 is Easy's), so the Gnomad engines take retired engines' numbers: 7 GNOME (was WHEEL), 9 VA (PHYS),
10 DRUMS (DRUM: `ENGI_DRUM`, where PHYS-DRUM migrations land), 12 FM (FM6: where DIGITAL's sounds land), 13 SAMPLER
(SLICE); 8 (GRAIN) holds SAMPLER's descriptor but is not offered (`eng_ok`), so a project or Easy sound on GRAIN still
plays. PRESETS and the editor offer VA FM DRUMS GNOME SAMPLER first, then Felucca's seven. Names reach NAMES and the UI from
the table; INFO names a slot not offered "-" (1 and 8) and DESC's ENG enum (global 20) is the same fourteen names ("INFO and
DESC"); no command, field or number of the editor protocol changes. A 1.0.1 project with a track on
a retired engine loads with the Gnomad engine of that slot, its old E values ignored. FM6 patches are kept opaquely per
track (gn_size_fm6.h: a project's packed patch is saved back byte for byte; `gn_core_test` checks the round trip); the
FM6 bank in flash is left untouched and answers with the existing error codes.

### How it plays (`firmware/src/gn_core.c`)

One core instance, its arenas static (state 7,168 B in `.bss`, effects 65,536 B in `.pool`), started in `audio_init`
before the audio interrupt. Every note source (keys, sequencer, ARP, chords, USB and TRS MIDI) reaches voice.c as before;
a Gnomad engine's voice is a token in Felucca's budget of 8: its `note_on` queues the core's NOTE_ON (IDs per track,
never 0; DRUMS: pad = (note − 36) mod 16, keys 36..51), its `done()` queues NOTE_OFF when the gate falls and keeps the
token (stage 2, so the ADSR never ends it) for the bound patch's release (P4; FM's longest operator release; a kit's longest pad decay; at most 4 s), so
Felucca's budget counts the core's releasing voice. The queue is a single-producer ring (slot, `RING_PUBLISH`, index:
the firmware's own pattern; every note path runs in the audio interrupt today). Each block a sweep finds tokens voice.c
ended without `done()` (an engine switch's clear, a kill, the ADSR): a held note gets its NOTE_OFF; a releasing token
given to another part, on a track with no held note, cuts that track's tails (PANIC), so the core never sounds past the
budget. Once a block (CTL, 32 frames; the core costs the
same per frame at 32 as at 128) a track whose engine or preset changed gets ALL_OFF and its preset's record from the banks (round FW-CONTENT-2), changed
controls are sent, and `gc_render_mono` adds the core's mono block to `mix_block`'s dry mix after Felucca's parts. The
four HOME knobs: BRIT (Bright), WOBL (Wobble), the track's DLY and REV sends (the core's Echo and Space); CHOR is the
core's chorus send, LEVEL and MUTE its track gain. After a silent second with no voice the core is not rendered. Easy:
on a Gnomad slot its four knobs are those four (`gn_map`), and its LFO claim does not apply.

### Easy on the swap

Since round FW-CONTENT-3 these sounds play bank sounds chosen by bank ID (the list there); what follows is the state
before it. 26 of Easy's 49 sounds were built on retired engines (8 FM6, 7 PHYS, 5 WHEEL, 4 GRAIN, 1 DRUM, 1 SLICE): they now play
the default patch of the Gnomad engine in their slot (FM6's on FM, PHYS's on VA, WHEEL's on GNOME, GRAIN's and SLICE's
on SAMPLER, DRUM's on DRUMS), not the sounds picked for them. With the sample block out, SAX and KIT (the Drums
picture's first sound, SAMPLE's PERC set) would be silent: interim (a review finding), with the swap and the
sample block out KIT plays on the Gnomad DRUMS and SAX on the Gnomad VA (gn_state.c). Re-voicing Easy's sounds on the
Gnomad engines is content work for Easy. The simulator's tour (sample block out) fails 7 checks: Space barely heard on four
sounds (the core's reverb, after the interim below, about 21 to 24 dB under the note where Easy's gate allows 12),
Wobble barely heard on Easy's DRUMS (the core's drums), and track 2's Wobble no longer claiming the LFO, which the
mode-switch check expects of FM6 (the simulator's check to update).

### Checked on the host and in the simulator

`tests/gn_core_test.c` (run_fork_tests.sh) drives the swap through the firmware's audio path. The simulator builds this
tree unchanged (felucca.c pulls the core in through gn_core.c; its Makefile's dependency list does not name
`firmware/gnomad-core/`, so after a re-vendor rebuild it clean) and its tour visits every engine's HOME, EDIT 1 and EDIT 2.
`tests/gn_core_keys.sim` is the playable check through the synth's own controls: leave Easy, SAVE to the Sounds page,
KNOB 2 to VA, HOME, C4 on the keys (`gnomad-sim -o OUT --flash FILE tests/gn_core_keys.sim`; in the 088befd simulator
build: -32.5 dBFS held, sounding 4.9 ms after the key, silent a second after the release).

### The core: version, re-vendoring, licence

`firmware/gnomad-core/` is the core as committed in its own repository: `CORE-VERSION.txt` names the commit (f16ae35 at
this writing, round CR; every field from the commit, so a re-run at the same commit leaves the tree clean), its test result there and every file's SHA-256 (`tests/gn_core_vendor_test.py` fails on any other byte).
`firmware/gnomad-core/sync-core.sh [REF]` re-vendors in one command (default: the core's main), runs the core's own
tests and checks the records against it; commit the folder as "engines: re-vendor gnomad-core <sha>". The firmware
compiles the core as part of felucca.c's one unit (`gn_core_unity.h`: its tables guarded per table, its macros
#undef'd after it), with the firmware's flags; the core's own `GC_HOST_TEST` is off. **Licence boundary**: the core is
the copyright holder's code, written clean-room (its PROVENANCE.md); inside this firmware it is granted under GPL-3.0-only as part
of the corresponding source (`firmware/gnomad-core/NOTICE.txt`); the copyright holder keeps every other right. Since 0f72e43 the core
also adapts open-source code, vendored with its own licence and notices: the FM operator kernel
from msfa (`third_party/msfa/`: Apache-2.0, LICENSE, NOTICE, UPSTREAM.md, CHANGES.md; `upstream/`, the core's host-test
reference, is not vendored) and the drum voices from Emilie Gillet's eurorack code (MIT; `CORE-NOTICE`, the notice in
`src/drums.c` and `src/drum_blocks.h`); ABOUT and `LICENSING.md` credit both. Profile 0x00010200 (the core's
SPEC/CHANGES-1.2.0.md): the adapter passes `GC_PROFILE_FX1` from the header, so it follows on its own. The core's
lab-only drum build (`GC_DRUM_LAB`: division in the render path) is never the firmware's: `gn_core_unity.h` stops the
compile with `#error` if it is defined, and `tests/gn_core_vendor_test.py` fails if anything in the build defines it. The engine
clean-room gate (a separate tool, run with `--fw --paths firmware/gnomad-core/`) needs local reference trees,
so it runs outside CI; it passes on this branch (review items only).

### Open questions

- Timing on the pi32v2 is unmeasured. On the host (Apple M4, -O2) the worst case, 8 FM voices with echo and reverb,
  takes 85 µs per 128 frames, 21 times 8 ANALOG voices; SPEC/40 allows the core 2,177 µs on the synth, so the target
  may be at most about 25 times slower than the M4. The core runs from XIP flash (its code is far larger than
  `.ram_text`'s 24 KiB). Measure on hardware before shipping (the audio interrupt's load is in `felucca_dbg`).
- The JieLi clang is 4.0.1 and the firmware links no runtime library: CI run 38078834949 failed on `__udivdi3`, so the
  core (a624452) divides 64-bit values itself; `GN_CORE_LIBCALLS` (gn_core.c, default 0) still holds a C fallback.
- Interim values in gn_core.c: `GN_CORE_GAIN_Q8` 1280 (the core's output x5, +14 dB: a VA note now
  within 1 dB of an ANALOG note at the same LEVEL; it was 14 dB under), `GN_CORE_SPACE_X` 2 (the Space send doubled,
  saturating at half the knob) and `GN_CORE_WET` 65535 (the core's master wet level at its top, +6 dB; set once by
  `gc_set_initial`). The reverb is still 21 to 24 dB under the note at full Space: more needs the core (SPEC/20's
  reverb gain), a later decision for the core. Easy's 26 sounds on retired engines, KIT and SAX: now bank sounds (round FW-CONTENT-3).
- The core's other master controls (echo time and feedback, reverb feedback, chorus, drive, filter) stay at their
  defaults; Felucca's global FX page does not reach them, and Felucca's DIST, pan and the SLICER do not touch a Gnomad
  track.
- CPU against the slots (CI run 38079745344, d8c4090; tests/gn_target_budget.py; no
  line is widened): the core's functions per sample (whole bodies, target_budget's weights) against the retired engine's
  lines: GNOME 2,497 / WHEEL 369, VA 1,111 / PHYS 1,157 (fits), DRUMS 2,560 / DRUM 848, FM 3,645 / FM6 470, SAMPLER
  1,082 / SLICE 573; the audio interrupt with the core's per-block work 61,642 against 37,502 (x1.64). Host
  instructions per sample, 8 notes held (cpu_baseline.txt's method): GNOME 6,668 (WHEEL 1,418), VA 8,396 (PHYS 1,451),
  DRUMS 7,825 (DRUM 1,099), FM 12,249 (FM6 1,475), SAMPLER 4,900 (SLICE 1,008): the core costs 4.7 to 8.3 times the
  engines it replaces. Those checks fail until the lines are set or the core gets cheaper.
- On the chip (review section 2): the core's 64-bit division is gone (core a624452; `GN_CORE_LIBCALLS` 0); 32-bit
  divisions remain at event rate. XIP flash timing, the audio interrupt's stack high-water (the static estimate allows
  +1,536 B) and the target's `int64_t` alignment (the core's struct-size asserts) are measured on the synth or by CI's
  compile. `FELUCCA_FM4` with the swap is refused at compile time (DIGITAL converts to the retired FM6).
- Pitch: a Gnomad note plays its key; Felucca's global TUNE, pitch bend, glide and the LFO's pitch do not reach it (the
  core has Tune and Wobble). A legato change is an OFF and an ON.

Measured in CI (run 38081203494, 83a29e2): the shipping image 355,508 B of the 581,564 B slot,
`.data+.bss` 81,844 B of 98,304, the pool 305,088 B of 344,064; the core 37,529 B of image, 7,168 B of RAM and the
65,536 B pool; the files of how the synth starts and installs upstream's bytes.

### Budgets: the ENGINES line

The engine swap's hard lines are the engine-swap budget's (from the size measurements of
CI run 37685583682). CI checks this branch's build against the line `GN_BUDGET_LINE` names (`ENGINES`; the build's name
had the short milestone `ENG`, "FELUCCA GNOMAD ENG" leaving five of INFO's 24 characters for the commit; since Branding
round V1.6 it is `V1`, six). The deltas are
against the base (434dafc: image 477,008 B, `.data+.bss` 88,884 B) and only keep the floors' arithmetic; the binding
figures are the absolute `max:` keys (`.github/ci/gn_budget.py`: `max:<figure> N`, the most the figure may be):

- `max:image 565180` (= 581,564 − 16,384 floor) and `max:.data+.bss 94208` (= 98,304 − 4,096 floor), with the floors as
  `free:` figures too (the `image` delta, +88,172 = 565,180 − 477,008, follows the floor). The floor was lowered to
  12,288 B on 2026-10-12 (round MF: macro.c + plate + Gnome voice 2, +7,972 B core) and is back at 16,384 B
  since the size diet ("Size diet" below: the image 569,060 -> 560,148 B);
- `max:core:image 81920` (D20, 2026-10-10: the shared allowance 16 → 24 KiB; was 73,728): the core's code and constants
  in the image, by the names its vendored sources define (`firmware/gnomad-core/src`; a table by the
  `GC_TABLES_DEFINE_*` block it is generated in);
- `max:core:ram 7168`: the core's `.data`/`.bss` (none: the core has no globals) and its state arena `gnc_state`
  (7,168 B, gn_core.c); its effect pool `gnc_fx` (65,536 B) is in `.pool`, under `pool +0` against the base;
- `max:content 196152` (D20: 565,180 − 287,108 − 81,920; was 204,344): the factory content in the image, the arrays
  `firmware/gnomad-content/gn_content.h` defines (round FW-CONTENT-1), by name. Until FW-CONTENT this figure was the
  image less the core and less the measured floor (287,108 B: every Felucca engine and the samples out); with the
  banks in, that figure is about 224 KB (the banks' 193,536 B plus the ~31 KB Felucca's kept engines, the UI, Easy and
  the skin already took), so it is printed as `rest` (held by `max:image`) and the line binds the content itself, as
  D20 words it ("content ≤ 196,152 B inside the image");
- `max:ram:rest 17644`: `.data+.bss` less the core's RAM and less the floor's (69,396 B): the RAM left for
  sequencer/UI growth.

Every run prints each of these with its headroom, and each engine's code and constants against its estimate (VA 8 KiB,
FM 20 KiB, Drums 12 KiB, Gnome Voice 6 KiB, Sample 10 KiB, shared DSP/FX 24 KiB since D20): REVIEW over 125 %, never a failure.
The audio interrupt's static depth may grow by 1,536 B (the core's render is at most 1,024 B, SPEC/40; the static
estimate follows the direct call from fx.c into the core).

```
gn-budget ENGINES image +88172 .data +64 .bss +5324 .data+.bss +5324 pool +0 .noinit +0 .ram_text +0 free:image 16384 free:.data+.bss 4096 stack:fm1_cstart +256 stack:fm1_alnk0_irq +1536 stack:fm1_timer5_irq +64 stack:ui_draw +512 stack:ui_input +256 max:image 565180 max:.data+.bss 94208 max:core:image 81920 max:core:ram 7168 max:content 196152 max:ram:rest 17644
```

The content's own line (round FW-CONTENT-1): the vendored files' sizes (`firmware/gnomad-content`: the pitched bank,
the kits, the factory PCM's three assets) and D20's cap; `tests/gn_budget_test.py` fails when a vendored size is not
the line's or their sum is over the cap, and `.github/ci/gn_budget.py` when the linked arrays are not those sizes.

```
gn-budget CONTENT pitched 49152 kits 13312 pcm 131072 max:content 196152
```

## The UX rounds (branch `gn/ux`)

Gnomad's own screens on the synth, one coherent Easy and Advanced, built on `gn/engines` from the design
boards of 2026-10 (option A of GN-02 Sequencer views, GN-03 Four-track home, GN-05 Sound browser) and a UX plan
(Home / Notes / Sound / Mix / Library; page identity, selected object and unsaved music shared by
the two depths; SHIFT reveals; a persistent status strip; save words that are true). The design rules: plain words
a nine-year-old reads; the Gnome in help and recoveries, never over notes; one small dry Gnome touch a screen at most;
nothing recognisable as Felucca. Rounds 7 to 23 keep every behaviour; their tests stay as they were but for the checks
that name the screen a press lands on (each listed in its round below). Everything is `gn_*` and behind `FELUCCA_GN`,
like Easy: no upstream file, protocol command, store or record changes. `tests/gn_ux_test.c` (in
`tests/run_fork_tests.sh`, twice: as the fork's other tests build, and with `GN_ENGINES 1` for the pages that read
the Gnomad engines) holds the rounds' cases; `tests/gn_draw_test.c`, `gn_stack_test.c` and `gn_screen.c` draw every
view through `gn_screens.c`'s `GS_SCENE` / `GS_PARTS` tables. Easy's state grows only at the end of `gn` (the layout
of rounds 7 to 23 byte for byte: `t_r20_*` checks the old offsets) and in `gs`.

### Round UX-1: Home

- **HOME is Home** (GN-03 A, `GP_HOME`, `GV_HOME`): the four tracks at a glance, a row each: the track's colour, its
  number, its sound (Easy's name, or the sound's own, cut to fit), its level as a bar in its colour, MUTE (`M`, filled
  when muted) and whether it sounds now (a filled triangle; while the selected track records, the red disc; armed, a
  red ring). The selected track's row is outlined; muted, sounding, selected and recording differ by shape and word,
  not only by colour. The header keeps the selected track's badge and the tempo, and says that track's BARS and SWING.
  An empty kit wears the Gnome's hat on its row until a hit is dropped in (the board's one dry touch, one row at most).
- **Its knobs** stay the selected sound's four (Bright, Wobble, Echo, Space; round 7: "elsewhere"). The board's help
  line puts BPM, BARS and SWING on K1 to K3; SELECT is already the tempo everywhere (pick) and BARS and SWING are the
  loop page's knobs (pick 7), so Home shows them in the header and leaves the knobs to the sound.
- **Where it sits.** Easy opens on Home (entering, and the first power-on after its start-up picture); HOME goes Home
  from every page, and HOME on Home opens the selected track's sound (`GP_PLAY`, the round 7 play screen, redrawn in
  UX-3); GLO's "and back" comes back to Home. ALGORITHM and PRESETS show the pick screen and the peek list over the
  page and give it back.
- **Its parts**: a row (its name, level, MUTE, selection, hat; the whole row's width since UX-6) and its last box
  (sounding, armed, recording) are parts of their own, so a level or a note redraws one row's worth (`t_ux1_rows`: at
  most 240 x 36 pixels).
- **State**: `gn.live_ms[4]`, `gn.live_age[4]` (a note begun in the last 150 ms counts as sounding: a hit's gate is
  short), appended to `gn`; `gs` gains the rows (names, signatures, mute, sounding, armed, the hat). Host: `gn` 1,704 ->
  1,728 B, `gs` 188 -> 260 B: +96 B of `.bss`.
- **Tests**: `gn_ux_test.c` `t_ux1_home` (entered on Home; HOME toggles Home and the sound; GLO and HOME come back),
  `t_ux1_rows` (the app's level and MUTE on a row, the selection, the sound's knobs on Home, a key's triangle, one row
  redrawn), `t_ux1_hat`. Changed in `gn_easy_test.c` (the screen a press lands on, now Home): lines "..then Easy's
  Home", "into Easy: the Gnome shown on track 1", "2 s after the last tap", "2 s after the last turn", "the page as it
  was (REC never leaves it)", "the first power-on", "into Easy: the Gnome shown, track 1 selected", "HOME: Home" (2),
  "HOME, SAVE, let go before 2 s" (the page as it was: Home); `t_r20_*`'s layout check names the old size as the
  offset where the UX fields begin. `gn_start_test.c`: the first frame and after the start-up picture, Home.
  `gn_screen.c`: `easy-ux1-home`, `easy-ux1-home-melody`; the sound shots go one HOME further.

### Round UX-2: the loop's pages, straight

Board GN-02 option A: one row language on every page, left to right; no circle, no 4 x 4 grid. SEQ still turns the
loop, the steps and the record screen round (pick); REC still only arms and never leaves the page (pick).
- **The loop** (`GV_LOOP`): a block a bar (a row of up to four, two rows for eight), each marking the bar's sixteen
  places where something sounds, the bar playing in the track's colour with the playhead in it (red while
  recording); under them "PLAYING: BAR 1 OF 2" or "STOPPED: 2 BARS" and "SNAP ON * KEEP". Its knobs stay BARS SWING
  SNAP VOLUME (pick 7). A block, and the two lines, are parts of their own.
- **The steps** (`GV_STEPS`): one bar up close, straight, with "BAR 1 OF 2" and the bar switch over it. **K1 is the
  bar switch** (`KR_BAR`): the bar shown; while the loop plays the page follows the playhead again 3 s after K1's
  last turn (`gn_view_bar`, `GN_VBAR_MS`). A melody: a row of sixteen places (eight on the 1/8 grid; with SNAP off,
  two 32nds to a place), each with the name of the note it begins over it (staggered, two places' room a name), a
  held note drawn held. A kit: **the list of its drums**, eight rows at a time (the half that holds K2's drum), each
  with its name, its note's name and its row of places; **K2 picks the drum** (`KR_DRUM`, marked in the list). The
  playhead is a mark under the bar switch, so a step redraws one strip, not the rows. K3 SNAP and K4 VOLUME as before.
- **Step entry** (the record screen stopped, `GV_RECORD`): STEP n and NOTE / REST (what the cursor's step holds),
  the note NOTE writes (a melody: the last a key wrote) or the drum K2 picked (a kit), the bar's steps and the
  cursor. A melody's knobs as before (K1 MOVE, K2 NOTE / REST, K3 SNAP); a kit's: K1 MOVE, **K2 the drum**, K3 NOTE /
  REST (NOTE writes K2's drum at the cursor). A key writes its note (a kit: its drum, which becomes K2's) and the
  cursor moves on, as before. Armed: "ARMED: PLAY OPENS THE LOOP". **PLAY from step entry opens the loop** (the
  page becomes the loop; armed, after the count-in it records there); from any other page PLAY leaves the page as it
  is. The record screen while playing (REC held records) is unchanged.
- **A kit's drums** (`gn_kit_n`, `gn_kit_note`, `gn_kit_name`, `gn_kit_of`, `gn_kit_has`): the Gnomad DRUMS' sixteen
  stable identities (pad i on note 36 + i: gn_core.c; their roles from the core's SPEC/12 table: KICK SNARE CLAP
  CLOSED HAT OPEN HAT LOW TOM MID TOM HIGH TOM RIM COWBELL CLAVE SHAKER CONGA BONGO RIDE CRASH), Felucca's DRUM
  engine's eight lanes, else Easy's sixteen pads (GN_PADS, plain names). A step plays a drum by its notes and the
  DRUM engine's hits.
- **The type** gains `#` (our own glyph, the same strokes), so note names read C#4.
- **State**: `gn.drum`, `gn.vbar`, `gn.vbar_t` (appended); `gs` gains the row's places and notes, the cursor's step,
  the drum list (eight rows: notes, places, names); the circle's `mel` / `hit` are gone. Host: `gn` +8 B, `gs` +116 B
  (72 of them nine pointers: 36 on the target).
- **Tests**: `gn_ux_test.c` `t_ux2_loop`, `t_ux2_steps` (the bar switch, its card, following the playhead, C#4 named),
  `t_ux2_drums` (K2 the drum, NOTE / REST, a key's drum, the list's rows and halves; the Gnomad kit's sixteen in the
  engines build), `t_ux2_play_opens`; every gn_easy_test case unchanged. `gn_draw_test.c` the loop in 1 to 8 bars,
  the steps and step entry with the list, long names and both knob maps. Goldens: the loop, steps (melody, drums) and
  step entry redrawn.

### Round UX-3: the sound browser

Board GN-05 option A. The library browsed is Easy's nine pictures and their sounds (the Gnomad engines carry one
patch each today, `gn_core.c`; 26 of Easy's sounds play them, "Gnomad engines" above); the pages read the Gnomad
engines where a track plays one.
- **ALGORITHM: the picture wheel** (`GV_PICK`): the picture picked in the middle, big, in its colour; its neighbours
  smaller either side (the outer ones half off the screen); under it the picture's name and how many sounds it holds
  ("BELLS * 6 SOUNDS"), then the other pictures in the wheel's order. A detent still puts the picture's first sound on
  the selected track (round 7); the wheel gives the page back 1.6 s after the last turn.
- **PRESETS: the peek list** (`GV_PEEK`): the picture's sounds a row each across the page, the one playing inverted;
  the page is back 2 s after the last turn, the sound kept (round 7).
- **The sound page** (`GV_PLAY`, HOME on Home): the picture in its tile (the Gnome small in his, singing what he sings),
  its name and its place among the nine under it, and **the four knobs named**, each a bar in its knob's colour as far
  as it is turned: **TONE, MOTION, ECHO, SPACE**. The board names them Tone, Shape, Motion and Space; the knobs are
  the sound's Bright, Wobble, Echo and Space (the Gnomad engines' four macros: P5 to P8, `gn_map`), so the screen names
  what each does: Bright is the tone, Wobble the motion; there is no "shape" macro to name (the core's patches would
  need one). Another sound the knobs do not move: the bars rest.
- **The kit** (`GV_DRUMS`, any track that holds a kit): its pads eight at a time, named, in two rows of four in the
  knobs' colours, dark while they sound; the banks A (pads 1 to 8) and B (9 to 16) under them, the one shown dark, a
  dot on the other while one of its pads sounds. **OCT- shows A, OCT+ shows B** on a kit (their LEDs say which; the
  octave is not touched: a kit's pads ignore it); a kit plays its sixteen pads on the sixteen white keys in either
  bank, so the bank never moves a pad off its key. A kit of eight (Felucca's DRUM engine) has A only.
- **The Gnomad kit's keys**: on the Gnomad DRUMS, Easy's kit and drum machine play white key n on the kit's pad n
  (MIDI 36 + n: `gn_state.c gn_pad_note`), so the sixteen white keys are its sixteen identities in order (KICK SNARE
  CLAP CLOSED HAT OPEN HAT LOW TOM MID TOM HIGH TOM RIM COWBELL CLAVE SHAKER CONGA BONGO RIDE CRASH); before, the
  General MIDI notes of Felucca's PERC set reached only eleven of them, twice over. The black keys are as before. On
  Felucca's engines nothing changes (GN_PADS).
- **SHIFT is EDIT held 0.5 s** (`gn_shift_watch`, `GN_SHIFT_MS`): the panel has no SHIFT and no knob that presses (the
  boards' "hold SELECT" cannot exist on it). EDIT's press is still a tap of the tap tempo (pick 4, on the press);
  held on, it is SHIFT and that tap is taken back (the taps and the tempo as they were before it). EDIT is lit while
  SHIFT is held. Let go: SHIFT ends.
- **The deep page** (`GV_DEEP`, SHIFT on the sound page): its Advanced depth at the same place, for the same track,
  **read-only**. On the Gnomad FM (`gn_is_fm`): its six operators in two rows, the algorithm's links between them
  (the core's own `fm_in`), the carriers' outputs, the feedback operator's loop (`fm_carr`, `fm_fb`); K1 picks an
  operator (`KR_OP`), whose RATIO (or FIXED Hz), LEVEL, ATTACK, DECAY, SUSTAIN and RELEASE show beside, read from
  the patch the core plays (`GN_CORE_PATCH[GC_FM]`, the record's words). Another sound: its four knobs' settings as
  numbers with their ranges. The other knobs rest (`KR_REST`) and the cards say so; editing is a later round.
- **State**: `gn.bank`, `gn.shift`, `gn.op`, `gn.edit_t0` and the tap tempo before EDIT's press (`tap_n`, `tap_bpm`,
  `tap_was[4]`), appended; `gs` gains the knobs' amounts, the kit's bank and names, the deep page's operators and
  settings; the play screen's floating note is gone. Host: `gn` +28 B, `gs` +128 B (64 of them eight pointers: 32
  on the target).
- **Tests**: `gn_ux_test.c` `t_ux3_browse` (the wheel and the peek list over the page, the named bars following the
  knobs), `t_ux3_kit` (A and B on OCT, the octave untouched, a key's pad lit in bank B; the Gnomad kit's sixteen white
  keys), `t_ux3_shift` (taps, a hold: SHIFT with the tempo kept, the deep page, the knobs write nothing, let go), and in
  the engines build `t_ux3_fm` (the FM page's algorithm and every operator's pitch against the patch, operator 2's
  envelope, every algorithm drawn whole). `gn_draw_test.c`: the kit (A, B, eight), the deep page (FM, every
  algorithm and operator, and not), the sound page at every amount. `gn_screen.c`: `easy-ux3-kit-bank-b`,
  `easy-ux3-deep`; built again with the engines into `build/gn_screens_eng` (no approved copy: the FM page,
  `eng-ux3-fm-op1` to `op6`).

### Round UX-4: one Easy and Advanced

The plan: the two depths share page identity, the selected object and the unsaved music; Advanced reveals more at
the same place; a switch never converts a project. On this synth Advanced is Felucca's pages (the skin's), entered by
HOME + SAVE held 2 s (round 7), and the fork's hooks stay inert while Easy is off: so the Gnomad pages carry their own
Advanced depth under SHIFT, and the switch carries the place and the object across to Felucca's pages and back.
- **Page identity across the switch** (`gn_page_from_felucca`, `gn_page_to_felucca`). Entering reads where Felucca
  was: its STEP page opens Easy's steps at STEP's cursor (`ui.cursor`, that bar shown); PATTERN or PHRASES the loop;
  the mixer or GLO the volumes; a sound's pages (ENV LFO FX EDIT ARP SCL) the sound page; HOME, the menu, a dialog,
  anything else Home. Leaving opens the same place in Felucca: the steps and step entry STEP at Easy's cursor
  (`cursor_set`); the loop PATTERN; the volumes the mixer; Home and the sound HOME (as before).
- **The selected object.** The selected track is one variable (`song.sel`) in both, and only an entry that lands on
  Home shows the Gnome on track 1 and selects it (round 13, 14: unchanged for every entry from Felucca's HOME and for
  the first power-on); an entry onto the steps, the loop, the volumes or the sound keeps the track that was selected.
  The step is carried both ways as above.
- **Never converts a project.** The switch, and looking at every page, writes nothing musical: a grown-up's triplet
  grid, an odd LEN, swing, ties and velocities go through Easy and back byte for byte (`t_ux4_never_converts`, from
  HOME, STEP and the mixer); BARS and SNAP change a grid only when the child turns them (round 7).
- **SHIFT's depth on every page** (EDIT held, UX-3), said in the band before anything acts (`gs_shift_hint`): Home
  "MUTE: FX SCL ENV LFO": SHIFT + a track button mutes or unmutes that track and leaves the selection; the steps and
  step entry: K1 the step (on the steps page with SNAP off a place, two 32nds), K2 its note a semitone a detent (a
  kit: the drum), K3 its strength (velocity, 4 a detent), with step entry's undo copy (`gn_step_edit`,
  `gn_cursor_step`; SAVE held undoes the session), the cursor's place outlined and the cards saying the note and the
  strength; the volumes: K1 to K4 the four tracks' pan (a dot on a line under each, the cards L / CENTRE / R), MUTE
  said in words on a muted track's bar; the sound: the deep page (UX-3); the loop: its knobs as they are. MUTE and pan
  are the person's mix, so leaving keeps them (they are not among the settings Easy puts back, GN_KEPT).
- **State**: none new in `gn`; `gs` +16 B (SHIFT, the cursor's place and step, the pans).
- **Tests**: `gn_ux_test.c` `t_ux4_identity` (STEP with its cursor, PATTERN, the mixer, a sound's page and HOME, both
  ways, the same track; the Gnome only from HOME), `t_ux4_never_converts`, `t_ux4_shift` (MUTE on Home and the
  selection kept, a step's note and strength and their cards, SAVE held, pan, the loop's knobs). `gn_draw_test.c`:
  SHIFT on the steps, step entry, the volumes and Home. `gn_screen.c`: `easy-ux4-steps-shift`, `easy-ux4-volumes-pan`.

### Round UX-5: the status strip and the save words

- **The status strip** (`gs_band`, every Easy page with a band: Home, the sound, the wheel, the peek list, the loop
  pages, the kit, the volumes, tap tempo, the count-in, the panel's saved screen, the deep page): the first line says
  what the buttons do here, in brackets, as before (round 7; under SHIFT what SHIFT does: UX-4); the second where you
  are and what the loop does, in words beside the band's triangle or record disc: PLAYING, STOPPED, ARMED, COUNT-IN or
  RECORDING, then HOME, NOTES, SOUND, MIX or TEMPO (`gs_state_words`); and at its right **the save word**.
- **The save words** are true or not said: **SAVED** (with a check) only while the music is what the last save read
  back from flash held; otherwise **ON SYNTH** (the music is on the synth and not saved). `gn_music_sig` fingerprints
  what a saved project holds (project.c `project_capture`): the globals, each track's settings as motion's base keeps
  them, its engine, preset, steps and FM6 patch, the motion store and the song's chain; not the selected track nor the
  project's name (choosing a track is not music). `gn_saved_now` takes it when a save is read back from flash the same:
  the panel's SAVE (saved_mine 1, or 4: the song saved, Easy's record of the slot not) and the app's PROJECT save
  (`gn_project`, as the saved screen 3g). A save the flash does not hold (NOT SAVED), a save of Felucca's own pages
  that Easy did not see, a power-on, a project loaded: no claim, ON SYNTH, until the music is again what a verified
  save held (turning a knob away and back says SAVED again: it is). Refreshed every fourth frame in Easy (about 4.5 KB
  hashed). The app's own words (On app) are the app's.
- **State**: `gn.music_sig`, `gn.saved_sig`, `gn.save_ok` (appended); `gs.stored`. Host: `gn` +12 B.
- **Tests**: `gn_ux_test.c` `t_ux5_words` (ON SYNTH at power-on; SAVE read back: SAVED; a knob: ON SYNTH, back: SAVED;
  a track selected: SAVED; the tempo: ON SYNTH; the app's save: SAVED; a save the flash does not take: NOT SAVED and ON
  SYNTH), `t_ux5_strip` (STOPPED * HOME, NOTES, ARMED, RECORDING, PLAYING, MIX, SOUND). `gn_draw_test.c`: the strip's
  words in Home's states. Every golden screen with a band redrawn.

### Round UX-6: Home redraws the whole row on a selection move

- **The bug** (the simulator's tour: `coverage_test` 1 failed, Home "selection redrawn whole"): moving the
  selection on Home left the old outline's right end behind. `scene_home` draws the selected row's outline x 10..236,
  but `parts_home`'s row part, the only part whose signature holds the selection, stopped at x 192; the row's last
  box (x 192..230) and the ground do not change with the selection, so x 192..236 of both rows was never redrawn:
  on the row it left the outline's end stayed, on the row it took the outline stood open. 496 px on each of the four
  moves.
- **The fix**: the row part is the whole row, x 0..240 (y as before, so a row's level, MUTE or selection still redraws
  only its own rows; the last box stays a part of its own inside it, so a note still redraws only the box). A level
  change now redraws 240 x 35 px instead of 192 x 35 (`t_ux1_rows`' bound, 240 x 36, holds); a selection move two
  such rows. No state, no `.data` or `.bss` (host `size -m`: 256 B and 2,305,568 B, unchanged); host `__text` +4 B.
- **Tests**: `gn_ux_test.c` `t_ux6_select`: five track moves on Home (2, 3, 4, 1, 3), each compared pixel for pixel
  with the same screen drawn whole (fails before the fix with the tour's 496 px a move, in both builds), and nothing
  in the field outside the row left and the row taken changes.
- **The tour** (gn/sim's recipe, headers of 088befd): 260 screens, 2,131 checks, 0 failed (`coverage_test` passes;
  Home 14 passing); the session test passes; boot, editor, note and record fail as on d9acb65 without the fix (the
  same lines, measured on both), predating the UX rounds.

### The UX rounds: budgets and what the simulator's tour must check

- **RAM** (host sizes; the target's from CI's linker): `gn` 1,704 -> 1,776 B (+72), `gs` 188 -> 520 B (+332, of which
  136 are seventeen pointers: about 264 B on the target), no `.data`. The ENGINES line's "RAM for sequencer/UI
  growth" had 12,364 B of headroom at d8c4090 (run 38079745344); the rounds take about 340 B of it. No pool, no
  `.noinit`, no flash object; the canvas's tail as before (`gn_draw_test`: its last 6,328 B untouched).
- **Image**: new code and constants only (no new assets: the pictures are `gn_art.c`'s path data and the type's `#`);
  the image had 209,560 B of headroom under `max:image`; CI measures the rounds' figure.
- **Stack** (host walk, `gn_stack_test`): Easy's walk 3,416 B against Felucca's 3,096 (it was 3,032 before the
  rounds); the target's static figures (`stack:ui_draw`, `stack:ui_input`) are CI's.
- **The tour** (the simulator: `sim/sim/sim_pages10.c`'s Easy view list must name the two new views, HOME and
  DEEP, in `GV_*` order; `sim_coverage10.c`'s Easy walk expects "EASY PLAY GNOME" after entering, now Home): every
  page and its way out (Home <-> sound by HOME, GLO back to Home, SEQ's three, the wheel and the peek list giving the
  page back, SHIFT held and let go on each page); MUST-CHECK 2 on every new card (BAR, DRUM, NOTE, STRENGTH, PAN,
  OPERATOR) and on the sound page's bars; MUST-CHECK 4 (both drum banks: OCT- / OCT+ on a kit, sixteen pads) and 10
  (the FM page, read-only: the patch's values) are no longer n/a for the Gnomad build; long names on Home's rows and
  the drum list; the mode switch from STEP, PATTERN, the mixer and a sound's page, both ways, the same track; the save
  words after SAVE, after a change, after a failed save.
- **The tour today** (gnomad-sim built from this branch at 3e9a4b7 with the sim's page model extended, gn/sim b10ba0e,
  headers of 088befd): 238 screens, 1,983 checks, 34 failed, MUST-CHECK 14/14 covered; the base
  (fffbca8, the same headers) 238 screens, 1,982 checks, 7 failed. Six of the base's seven remain (Space barely heard on
  four sounds, the FM6 Wobble claim twice: "Gnomad engines" above); the 28 new ones are the walk expecting the old
  screens, no behaviour lost: the play screen after entering, after HOME and after GLO (Home now: 9), Easy's General
  MIDI pads on the Gnomad kit (white key n plays pad n now: 13 keys and their summary), and three views it never
  presses into (the sound page: HOME on Home; the kit view; the deep page: EDIT held). The walk is the simulator's to
  update (sim_coverage10.c).

### INFO and DESC name the slots honestly

- **The bug** (the simulator's editor test): INFO sent `ENGINES[i]->name` for all fourteen slots, so slot 8
  (was GRAIN; SAMPLER's descriptor, hidden only by `eng_ok`) read SAMPLER as slot 13 does; and DESC of global 20 (ENG,
  `N_ENGNAME`) still listed Felucca's WHEEL, GRAIN, PHYS, DRUM, FM6 and SLICE.
- **The fix** (string literals only): with `GN_ENGINES`, editor.c's INFO loop sends `eng_ok(i) ? name : "-"`, and
  params.c's `N_ENGNAME` is the swap's list (a `_Static_assert`: one name a slot). Both now read
  `ANALOG - PHASE LOFI SAMPLE VOICE TRIO GNOME - VA DRUMS NOISE FM SAMPLER`. Upstream's lines are kept in the `#else`.
  NAMES (slot 8 still answers SAMPLER's presets: a project on GRAIN plays them) and the device UI (G_ENGSEL has no
  page; the screens read the table) are unchanged. No command, field, length or number of the protocol changes.
- **Size**: the simulator's firmware unit (`sim_fw.o`, host `size -m`) is the same in every section before and after
  (`__text` 540,284, `__bss` 9,316, `__DATA,__const` 35,744); no `.data` or `.bss`.
- **Tests**: upstream's and the fork's host suites pass (as rounds UX-5/UX-6 ran them: without the package, OTA and
  loader steps). gn/sim's `gn_sim_editor_test` asserts, on a swap build, INFO's fourteen names, no name but "-"
  twice, no retired name, and DESC global 20's names equal to INFO's; the swap-off build keeps upstream's check.

### Round FW-CONTENT-1: the banks vendored, the CONTENT line

- **What**: `firmware/gnomad-content/` holds the factory content as gnomad-core builds it: `pitched.gcpbank`
  (384 records of 128 B, headerless, offset id × 128), `kits.gckbank` (32 × 416 B) and the factory PCM, its three
  assets (`tine.pcm`, `string.pcm`, `knocks.pcm`: 48,000 + 48,000 + 35,072 = 131,072 B) with their
  `factory.manifest` (ID, frames, root, CRC32C, SHA-256). Source (`CONTENT-VERSION.txt`): gnomad-core 86e20aa; the banks are
  a snapshot whose SHA-256 are that commit's
  `content/BANKS.sha256`; the PCM is that commit's `content/pcm` (committed files, not regenerated). 60 of the 384
  pitched slots and 8 of the 32 kits are authored; the rest are zero records and are skipped. The catalogue fills later
  and the bytes are simply replaced.
- **Re-vendoring**: `firmware/gnomad-content/sync-content.sh SRC [PCM]` (SRC: a folder with the two banks, e.g.
  gnomad-core's `content/build`; PCM: the manifest's folder, default `SRC/../pcm`; `SOURCE="..."` names them) copies
  the files, regenerates `gn_content.h` with `tools/gn_gen_content.py` and writes `CONTENT-VERSION.txt` (the source,
  the authored slots per engine, every file's SHA-256). Commit the folder.
- **Into flash**: `gn_content.h` (generated, committed: `tools/build.py` is upstream's and the simulator takes CI's
  `build/gen`, so the arrays are not a `build/gen` header) defines `GN_CONTENT_PITCHED`, `GN_CONTENT_KITS` and
  `GN_CONTENT_PCM` as `static const` arrays (flash: `.text`), the asset list, and each engine's authored slots in bank
  order. The generator accepts a record only when its magic, version, length, flags, CRC32C, name and engine (a kit:
  pad count 16 and kit ID = slot) are well formed, checks every asset's size, CRC32C and SHA-256 against the manifest
  and the PCM's total against SPEC/30's 131,072 B, and refuses an engine with no authored sound. `tests/gn_content_test.py`
  (run_fork_tests.sh, CI) fails on any file not as `CONTENT-VERSION.txt` lists it and regenerates `gn_content.h` to
  check it byte for byte, the way CI already checks the vendored core.
- **Budget** (D20): the CONTENT line above (`gn-budget CONTENT pitched 49152 kits 13312 pcm 131072 max:content
  196152`): 193,536 B of 196,152, 2,616 B headroom. `tests/gn_budget_test.py` (in the budget step and now in
  run_fork_tests.sh) checks FORK.md's line against the vendored files and its refusals; `.github/ci/gn_budget.py`
  checks the linked arrays against it. The ENGINES line takes D20's figures (`max:core:image 81920`,
  `max:content 196152`, the shared estimate 24 KiB), and its `content` figure is now the linked content arrays (see
  "Budgets: the ENGINES line": the old figure, image − core − floor, is printed as `rest`).

### Round FW-CONTENT-2: the presets are the banks'

- **NAMES and PRESET**: each Gnomad engine's presets are the authored slots of that engine in the banks, in bank order
  (zero and malformed records skipped; `gn_content.h`'s lists). With the snapshot: **VA 16** (bank 0..4, 64, 65,
  136..139, 184..186, 224, 225), **FM 32** (32..36, 72..79, 168..170, 200..203, 232..235, 336..341, 368, 369),
  **DRUMS 8** kits (0, 2, 5, 10, 11, 15, 17, 27), **GNOME 8** (272..279), **SAMPLER 4** (128, 256, 257, 376); slot 8
  (not offered) answers SAMPLER's. NAMES answers the record's own name (bytes 16..31 of the record in flash: mixed case,
  up to 15 characters, sent as NAMES always sends a name, in 12); the device's fonts draw it in capitals. PRESET k
  binds the k-th listed record through the existing path (`gnc_sync_engines` → `gc_bind_track`): a pitched record as
  the track's patch; a kit with the core's drum patch (`gn_core_records.h` "Init Drums", the only record of that
  file still bound); SAMPLE's assets are the factory PCM's three (IDs 1..3 from flash) in place of the triangle.
  No command, field, length or number of the protocol changes; NENGINES, the slots and the stores are as before.
- **A preset's knobs**: loading a preset sets BRIT, WOBL, CHOR, DLY and REV to the record's stored Bright, Wobble,
  Chorus, Echo and Space (the knob values whose sent values come nearest: the generator mirrors `gnc_macro` and the
  Space send's doubling), so the controls the shim sends at bind are the record's own and the sound is the authored
  one; a kit's knobs are the drum patch's (Bright centre, the sends at 16,384: unity on the kit's pad sends).
- **Release tokens**: FM's token is now kept for the patch's longest operator release (SPEC/11: release code × 40 ms,
  0 = P4), not a fixed second; the other engines as before (P4; a kit's longest pad decay); 4 s at most, as before.
- **Gone**: the hand tables `GNC_*_PRESETS` ("INIT VA", "INIT FM", "INIT KIT", "INIT GNOME", "TRIANGLE"; nothing else
  named them), and with them every use of `gn_core_records.h` but the drum patch: its triangle asset (4,000 B), its
  default kit (416 B) and the four other patches stay in the vendored file (sync-core.sh's output, checked by SHA-256)
  but are no longer referenced, so the compiler leaves them out. The preset tables now cost a `preset_t` and a
  `uint16_t` a sound (no name copies).
- **Tests**: `gn_core_test`'s engine loop dries the track after the preset (a kit's preset now sets the sends at unity,
  whose reverb tail outlasted the check's silence); see round FW-CONTENT-4 for the new checks.

### Round FW-CONTENT-3: Easy's sounds on the Gnomad engines are bank sounds

- **How**: in `gn_state.c`, a sound on a Gnomad slot names its bank sound by ID: `GN_BANK(<Felucca engine>, <preset>,
  <bank id>)` or `GN_KIT(..., <kit id>)`. With the swap these expand to `gn_content.h`'s `GN_CONTENT_P<id>` /
  `GN_CONTENT_K<id>` (the slot and the preset index of that ID: a sound may move to the Gnomad engine its bank sound
  is on, e.g. ORGAN from GNOME's slot to FM's); an ID the banks do not have authored does not compile. Without the swap
  they expand to the Felucca engine and preset as before, byte for byte. The mapping is by ID, never by name, so a
  rename in the banks changes nothing here. Easy's own names (its screens) stay.
- **Bright**: four sounds' Bright rests wrote the core's own BRIT or WOBL on a Gnomad slot (SHIMMER, CLOUD and FROZEN:
  GRAIN's SRC into P_E0, Bright 0 or 2, almost dark; ROCK DRIVE: WHEEL's DRIVE map into P_E1); with the swap their
  Bright is the core's Bright at its centre (`GN_GB`: `gn_easy.c`'s `GN_CORE_KNOBS[0]`, knob 64 = 32,768, the stored
  Bright of every authored sound today), so they play as authored. Easy's Echo and Space rests (3 and 4) still set the
  sends, as for every Easy sound; Wobble's rest moves Felucca's LFO, which a Gnomad track does not hear, so the record's
  stored Wobble stays.
- **The 28** (Easy's name → bank ID "bank name today", fit): ORGAN → 73 Parlor Organ (FM, organ); FULL ORGAN → 64 Tin
  Harmonium (VA, pump organ); GOSPEL → 128 Steady Tine (SAMPLE, sustained organ-like; loose); JAZZ → 79 Reed Keys (FM;
  loose); ROCK DRIVE → 75 Copper Clav (FM, biting keys; loose); TINE PIANO → 72 Lamplight (FM, tine EP); GLASS BELL →
  339 Ice Chimes (FM); BELL TREE → 337 Starlight Glock (FM; loose); MARIMBA → 225 Tick Tock (VA, marimba-like);
  KALIMBA → 234 Kalimba Seed (FM); GLITCH → 369 Robot Hiccup (FM; loose); FM BASS → 33 Deep Current (FM); SHIMMER → 200
  Glass Harmonics (FM pad); DRONE STRING → 184 Warm Blanket (VA pad; loose); CLOUD → 185 Morning Fog (VA pad); FROZEN →
  202 Northern Air (FM pad; loose); SOFT FLUTE → 137 Glass Whistle (VA, flute edge); PIANO → 74 Felt Hammer (FM, muted
  piano); PLUCK → 224 Snap Pea (VA); HARP → 233 Harp Drops (FM); NYLON GUITAR → 232 Silk Pluck (FM, nylon-like); SAX
  (swap, sample block out; was VA's default) → 169 Reed Whistle (FM woodwind; loose: no sax authored); BRASS → 168 Brass
  Kite (FM); KIT (swap, sample block out) → kit 0; DRUM MACHINE → kit 2; HAND DRUM → kit 10 (hand drums); TOMS → kit 17
  (its toms); CHOPPED BREAK → 257 Woodpile (SAMPLE, percussive slices). HAND DRUM and TOMS were pitched PHYS sounds and
  are now kits on DRUMS without Easy's pad map (`GNF_PADS`): each key a pad, (note − 36) mod 16. "Loose" marks a
  closest-authored pick that may change when the catalogue fills (one ID in `gn_state.c` each).
- **Unchanged**: the 21 sounds on Felucca's seven kept engines (VOICE, ANALOG, PHASE, LOFI, TRIO, NOISE, and SAMPLE's
  SAX and KIT with the sample block in): engine and preset as the engines-off build has them (`gn_bank_test` compares
  the two builds' tables).

### Round FW-CONTENT-4: tests, sizes, and notes for the app

- **Tests**: `tests/gn_bank_test.c` (run_fork_tests.sh, ASan and UBSan; built twice, the engines-off build dumping
  Easy's table for the swap build to compare): the flash arrays are the vendored files and the core boots with the
  three assets; each engine's list is exactly what the core's own `gc_validate_patch` / `gc_validate_kit` accept, in
  bank order; NAMES for slots 9, 12, 10, 7, 13 and 8 (count = authored, each name the record's, first 12 characters);
  PRESET of every preset of every Gnomad engine through the editor: echoed, the bytes the core was given
  (`GN_CORE_BOUND`, a test hook after the bind) equal the bank file's at that ID (a kit's 416 with the drum patch), the
  macros sent within half a knob step of the record's, and a note sounds; every kit's 16 pads sound on keys 36..51;
  Easy's 28 sounds bind and sound their listed IDs with the record's Bright, and its 21 others are the engines-off
  build's. Every expectation is by ID or slot; names are read from the bank files at run time (renames in the banks
  are a byte swap). `tests/gn_content_test.py` and `tests/gn_budget_test.py` as in FW-CONTENT-1.
- **Results** (local, as "INFO and DESC" ran them: a local copy of `tests/run_tests.sh` and the fork runner without the UX-only lines
  and without the package, OTA and loader steps): host ALL PASSED, fork ALL PASSED (gn_core_test, gn_bank_test,
  gn_content_test, gn_budget_test among them). Simulator (sim 68dce0a, `GEN` gen-gnomad10-088befd): the editor test
  passes; coverage 238 screens, 1,974 checks, 0 failed (b3566f1: 1,977, the knob-label sweep 449 instances where it had
  452); the tour's five known "barely heard" notes are the same five sounds, now on their bank sounds' engines; no
  preset-name check broke. The session test fails as on b3566f1 (the same seven pictures) and now also on its sound
  (`audio.txt`: another hash, peak 1,697,280 where it was 1,587,072): the sounds it plays on Gnomad slots are bank
  sounds now; its reference is the simulator's to re-record.
- **Sizes** (the simulator's firmware unit, host `size -m sim_fw.o`, b3566f1 → this round): `__TEXT,__const`
  291,700 → 480,956 (+189,256: the content's 193,536 less the triangle and default kit no longer referenced, plus the ID
  lists), `__DATA,__const` 35,744 → 37,888 (+2,144: 72 preset entries of 32 B on the 64-bit host, 24 B on the target),
  `__text` 540,284 → 540,656, `__cstring` 18,030 → 17,985; `__bss` 9,316, `__gnfw_bss` 81,580 and `__gnfw_data` 276
  unchanged: no RAM. On the target, expected about +191 KB of image (CI run 38081203494 measured 355,508 B at 83a29e2:
  about 547 KB of 565,180), the content 193,536 of 196,152, `.data+.bss` unchanged. The target's own figures are CI's
  (the toolchain builds only there, rule 6).
- **For the app's facts**: NAMES per Gnomad engine = the authored slots of that engine, today VA 16, FM 32, DRUMS 8,
  GNOME 8, SAMPLER 4 (slot 8 answers SAMPLER's); names are the bank's (mixed case, 12 characters on the wire). A preset
  number is the position in that list, not a bank ID: the bank ID of VA preset k is the k-th authored VA slot in bank
  order (CONTENT-VERSION.txt lists them). When the catalogue fills, slots are authored between today's, so preset
  numbers move: a project, favourite or user reference saved by preset number then names another sound (Easy is
  compiled from IDs and follows). The app should key Gnomad sounds by bank ID and read NAMES fresh per firmware.
- **Open items**: (1) NAMES and PRESET carry 7-bit numbers and the editor's reply buffer is 600 B: at D14's
  catalogue FM's 192 presets exceed both the count byte (127) and the buffer (FM's 32 names take 382 B today; about 45
  fit), and PRESET cannot address FM presets 128..191. Today's banks fit (the test asserts it); the full catalogue needs
  a protocol decision (rule 4: our own commands, not upstream's). (2) The ENGINES line's content figure is now the
  linked content (D20's wording); the old figure, image − core − floor, would read about 224 KB and is printed as
  `rest`. (3) The PCM is read from XIP flash by the core in the audio interrupt (SPEC/30 allows flash-resident audio
  "if host reads are safe within timing gates"): to measure on the synth with the rest of the core's timing.

### Round FW-CONTENT-5: at most 127 presets an engine (interim)

- **Interim**: NAMES and PRESET carry 7-bit preset numbers and the editor's reply is 600 B (round FW-CONTENT-4, "open
  items" (1)), and the catalogue will hold 192 FM sounds (D14). How the rest are reached is a later
  decision. Until then an engine offers at most 127 presets, the first 127 authored slots in bank
  order; this round is undone or replaced by that decision.
- **The cap** (`gn_core.c`, `GN_PRESETS_MAX` 127): each Gnomad engine's `npresets` and `GNC_LIST` count are the
  authored count capped, so every reader of a preset number (the screens' lists and KNOB 2, NAMES, PRESET, projects,
  favourites, the user-preset limit, the bind) sees the same 127. Today's banks (VA 16, FM 32, DRUMS 8, GNOME 8,
  SAMPLER 4) are under it: nothing changes for them. The tables still hold every authored entry (the ones past 127
  are not offered: on the target 26 B each, about 1.7 KB at 192 FM). Easy: a `GN_BANK` / `GN_KIT` bank sound past an
  engine's first 127 does not compile (`gn_state.c` `GN_OFFERED`) rather than play another sound (superseded by round
  FW-CONTENT-6: Easy binds by bank ID past the offered list).
- **PRESET past the list**: PRESET has never refused a number: `apply_preset_to` (ui.c) loads it modulo the count and
  the reply echoes the preset loaded. So PRESET 127 on a list of 127 loads and echoes 0, as PRESET 16 on VA (16)
  does today; a client tells by the echo. 128 and over cannot be sent (7-bit).
- **NAMES was not bounded cleanly**: `ed_b` never overran (it stops at 599 B, F7 the 600th), but NAMES sent the
  count as `npresets` and then names until the buffer filled: past about 44 names the reply ended inside a name, with
  no terminator and no page titles, and upstream's editor (web/editor.html reads count strings, then the titles; its
  `Reader` throws "short reply" and the connect loop has no catch) would fail to connect. **Fixed** (editor.c, under
  `GN_ENGINES` as "INFO and DESC"; upstream's lines in the `#else`): the count is the number of whole names the reply
  holds with the two titles after them, and names 0..n-1 are the device's presets 0..n-1. The reply has 584 B for
  names: **44 names** when every name is 12 characters or more on the wire (13 B each), more with shorter names (all
  127 at 3 characters). Every list today fits, so every reply today is the same bytes as before.
- **So, past 44 names**: NAMES' count is less than the device's list. Its names are the start of the same list (the
  same order; PRESET k binds the device's preset k for every k below 127), but names further down are not on the
  wire. A count of 127 with fewer strings would break the protocol's own shape ("count, count preset-name strings")
  and upstream's editor (rule 1). How the app reads the rest is part of that decision.
- **Tests**: `gn_bank_test` is built a third time (`-DGN_BANK_SYNTH`, run_fork_tests.sh, ASan and UBSan): 200 FM
  records sealed in the core's record format (an authored FM record as the template, a name of its own, CRC32C),
  each accepted by `gc_validate_patch`, appended after the snapshot's 384 slots; FM offers 127 (bank order), the
  other engines as before; PRESET 126 binds the 127th record and sounds; PRESET 127 loads and echoes 0 as VA 16 does,
  never the 128th; the device's list (`eng_list_pos`, `preset_shown`) is 127 and wraps 126 <-> 0; NAMES stays within
  600 B, ends on a whole name and the titles, its count is what it holds (44 of 127 at 12-character names, each the
  device's name of that number; all 127 at 3-character names). Each check fails without its change (the old NAMES
  loop: a 600-byte reply ending mid-name; no cap: 200 offered; a cap below an Easy sound: gn_state.c does not compile).
- **Sizes** (the simulator's firmware unit, host `size -m sim_fw.o`, ac89e5b -> this round, GEN
  gen-gnomad10-088befd): `__text` 540,656 -> 540,856 (+200: the NAMES count); `__TEXT,__const` 480,956,
  `__DATA,__const` 37,888, `__cstring` 17,985, `__bss` 9,316, `__gnfw_bss` 81,580, `__gnfw_data` 276 unchanged: no
  RAM, no table growth. The target's figures are CI's (rule 6).
- **Results** (local, as round FW-CONTENT-4 ran them: a local copy of `tests/run_tests.sh` and the fork runner without the
  package, OTA and loader steps): host ALL HOST TESTS PASSED (the golden regression's 87 renders unchanged), fork ALL
  FORK TESTS PASSED (gn_bank_test's three builds among them).

### Round FW-CONTENT-6: the complete sound catalogue; Easy binds by bank ID beyond the offered list

- **The catalogue**: `firmware/gnomad-content` re-vendored with `sync-content.sh` from a snapshot of the banks
  (gnomad-core ff8463d; every file's SHA-256 as its `SHA256SUMS`): all
  384 pitched slots are authored (**VA 96, FM 192, GNOME 64, SAMPLER 32**) and all **32 kits**; the PCM and its
  manifest are byte-identical to 86e20aa's. `gn_content.h` regenerated, `CONTENT-VERSION.txt` rewritten (its census
  header now says the list is every authored slot and NAMES and PRESET offer an engine's first 127);
  `tests/gn_content_test.py` passes. The CONTENT line is unchanged (193,536 B of 196,152: the banks were full-size).
- **Offered** (round FW-CONTENT-5's cap, unchanged): VA 96, **FM 127 of 192** (bank order: places 0..126, bank IDs
  32..222; places 127..191, bank IDs 223..375, are not offered over NAMES, PRESET or the screens), DRUMS 32, GNOME 64,
  SAMPLER 32 (slot 8 answers SAMPLER's). NAMES on the wire (whole names in the 600-B reply, round FW-CONTENT-5): VA
  **52** of 96, FM **54** of 127, DRUMS 32, GNOME 64, SAMPLER 32.
- **Easy binds by bank ID** (was: through the preset position). `gn_sound_put` loaded a sound with ui.c's
  `apply_preset_to`, which takes the preset modulo the offered count, and gn_core.c's bind read the offered list
  modulo the same, so six Easy sounds whose bank IDs now sit past FM's first 127 would have wrapped to another FM
  sound, and `gn_state.c`'s round FW-CONTENT-5 check (`GN_OFFERED`, place < 127) did not compile: NYLON GUITAR 232
  (place 128), HARP 233 (129), KALIMBA 234 (130), BELL TREE 337 (153), GLASS BELL 339 (155), GLITCH 369 (185). Now:
  `GN_BANK` / `GN_KIT` give the bank sound's place in its engine's **whole** authored list (`GN_CONTENT_P<id>`, as
  before), checked at compile time to be an authored record of that engine (else the macro does not exist) inside
  that engine's list and a byte (`GN_AUTHORED`); `gn_easy.c` `gn_preset_put` loads it: an offered place exactly as
  before (`apply_preset_to`), a place past the offered list by `apply_preset_to` (the rest of the sound, as any
  preset) and then the track's preset set to the place and the record's own preset entry (knobs, sends) written over
  it, the audio interrupt off between the two; gn_core.c's `GNC_LIST` holds the offered count (`n`) and the whole
  count (`all`), and the bind reads the whole list (`id[pi < all ? pi : pi % n]`: every offered preset binds as
  before). So **Easy sounds may live beyond the offered list**; the app maps Easy sounds by bank ID (its facts
  already say so). Nothing else about the cap changes: NAMES, PRESET, the screens' lists, projects, favourites and
  user presets still see the first 127.
- **Where a past-list Easy sound shows its number** (until that decision): the Advanced
  screens name the track's preset modulo 127 (another FM sound's name) and KNOB 2 steps on from that position; the
  editor's preset bytes (RELOAD, the track state) carry the place's low 7 bits; a project or an Easy song saved
  with one reloads that track at its place modulo 127 (`project.c`'s restore, upstream's lines, takes the preset
  modulo the offered count): **another FM sound**. Keeping a Gnomad place below the whole count on restore is a
  one-line change in `project.c`, but it lets projects address presets past the offered list: part of that decision.
- **Tests** (`gn_bank_test`): census against the whole list, and the offered count = min(authored, 127); NAMES of
  each slot: the reply holds the first offered presets as whole names, each the record's, then the titles, within
  600 B, as many as fit; PRESET of every offered preset as before (VA 96, FM 127, DRUMS 32, GNOME 64, SAMPLER 32 twice);
  **the whole bank**: every one of the 384 records and 32 kits, offered or not (65 FM past the list), validated by the
  core, bound through `gn_preset_put` (the bytes the core was given are the bank's at that ID) and sounding; Easy: all
  28 bind and sound their IDs with the track's preset at the place, 6 past the offered list; the 21 on Felucca's
  engines are the engines-off build's. Without the change: Easy through `apply_preset_to` fails the six (and the
  count); the old check does not compile. The `-DGN_BANK_SYNTH` build is round FW-CONTENT-5's (200 synthetic FM, 127
  offered, PRESET 126/127, the screens' list, NAMES 44 and 127), its expectations for the other engines now read
  from `gn_content.h` and its out-of-range example PRESET VA 96 (VA has 96 now, so 16 is in range).
- **Results** (local, as rounds FW-CONTENT-4/5 ran them: a local copy of `tests/run_tests.sh` and the fork runner without the
  package, OTA and loader steps): host ALL HOST TESTS PASSED (the golden regression's 87 renders unchanged), fork ALL
  FORK TESTS PASSED (gn_bank_test's three builds, gn_content_test, gn_budget_test among them). Simulator (`GEN`
  gen-gnomad10-088befd): the editor test passes; coverage 238 screens, 1,974 checks, 0 failed (as rounds
  FW-CONTENT-4/5); the tour's five known "barely heard" notes are the same five (EASY PLAY ORGAN, GLASS BELL, CLOUD,
  PIANO: SPACE; EASY DRUMS: WOBBLE), GLASS BELL now heard on bank 339 at place 155; the session test fails as before
  (the same seven pictures and its sound, peak 1,697,280: the simulator's to re-record).
- **Sizes** (the simulator's firmware unit, host `size -m sim_fw.o`, f9fe98c -> this round): `__TEXT,__const`
  480,956 -> 481,708 (+752: the ID lists, 344 more `uint16_t`; the banks were full-size already), `__DATA,__const`
  37,888 -> 49,024 (+11,136: 344 more `preset_t`, 32 B on the 64-bit host), `__text` 540,856 -> 541,216 (+360:
  `gn_preset_put`); `__cstring` 17,985, `__bss` 9,316, `__gnfw_bss` 81,580 and `__gnfw_data` 276 unchanged: **no
  RAM**. On the target, expected about +9 KB of image (26 B a sound: a 24-B `preset_t` and its 2-B ID), outside the
  CONTENT line (it counts the banks and the PCM only) but inside `max:image`: to watch against CI's
  figures (rule 6).

### Round FW-CONTENT-7: the preset lists are bank ids; names and knobs read from the records

- **What**: a Gnomad engine's preset is its bank ID and nothing else: `gn_core.c`'s lists are `uint16_t` IDs in bank
  order (`GNC_*_IDS`, expanded from `gn_content.h`'s generated `GN_CONTENT_*` lists, so they follow the banks; a
  `_Static_assert` ties each to the generated count), and the five `preset_t` tables of rounds FW-CONTENT-2/6 (416
  entries: 32 B each on the host, 24 B on the target) are gone. The descriptors keep `.npresets` (the offered count,
  capped at 127 as in round FW-CONTENT-5) and set `.presets` to 0. Offered, whole lists, the cap, Easy's place past
  the offered list (round FW-CONTENT-6), NAMES, PRESET, the screens: unchanged.
- **Reading a preset**: `GN_PRESET(e, k)`, `GN_PRESET_NAME(e, k)` and `GN_PRESET_PAT(e, k)` (`gn_size_flags.h`, which
  core.h includes). Without `GN_ENGINES` they are upstream's own expressions (`&e->presets[k]`, `.name`, `.pat`), so
  upstream's builds and host tests compile what they compiled before. With the swap they call `gn_core.c`'s
  `gn_preset_get` / `gn_preset_name` / `gn_preset_pat`: a Felucca engine's preset from its table; a Gnomad engine's
  from its record, as its `preset_t` had it: the name at the record's bytes 16..31 (the same flash address as before),
  BRIT, WOBL, CHOR, DLY and REV at the knobs nearest the record's stored Bright, Wobble, Chorus, Echo and Space
  (`gnc_knob`: `gnc_ctl`'s laws, the same nearest-knob search `tools/gn_gen_content.py` runs; a kit's from the drum
  patch it binds with, Bright 32768, Wobble 0, sends 16384, as `KIT_MACROS`), the envelope (0 64 127 127), DIST 0 and
  no pattern. `GN_PRESET`'s `preset_t` is a compound literal in the caller's block (the stack, 24 B on the target
  during a preset load; no `.bss`); the readers are out of line (a call, not a copy at every site).
- **The readers** (one expression each; touch list): ui.c `apply_preset_to` (the fields through one pointer) and
  `preset_pat_hint`, ui_graph.c (the browser's rows, the EDIT layer's row, a track's short name), ui_draw.c
  `sound_name`, console.c, editor.c NAMES (the `GN_ENGINES` branch of round FW-CONTENT-5), gn_easy.c `gn_preset_put`
  (the record read before the audio interrupt goes off), tests/hostsim.c `host_preset_req`, and gn_bank_test's three
  name reads (its assertions unchanged). The bind (`gnc_record`, the audio interrupt) still reads only the ID list.
  A Gnomad preset load runs five 128-step knob searches at UI time; the screens' names are an ID lookup; the pattern
  hint reads no record.
- **Generated**: the lists, `GN_CONTENT_P<id>` / `K<id>` and the Easy checks (`gn_state.c` `GN_AUTHORED`, `GN_NAUTH`)
  are as before. The generator's docstring and the header's comment now say the X lists' knob values are the check's
  (`gn_content.h` regenerated: three comment lines; `CONTENT-VERSION.txt` carries its new SHA-256);
  `tests/gn_content_test.py` passes.
- **For the simulator**: `sim/sim_pages.c` and `sim/sim_pages10.c` (`sp_preset_name`) read `e->presets[...]` directly;
  a Gnomad engine has no table now, so the sim needs those two lines through `GN_PRESET_NAME` under
  `#ifdef GN_PRESET_NAME` (older firmware does not define it: with the patch, 4d8a1b9's tour report and all 476 page
  files are identical to the unpatched sim's). Without it the sim reads a null table when a Gnomad track is selected.
- **Tests**: `gn_core_test` checks every one of the 416 presets field for field against what the tables held (the
  record's name pointer, the generator's knob values from the X lists, the constants) and that a Felucca engine's
  come from its table: 0 differ. `gn_bank_test`'s output (all three builds) is line for line 4d8a1b9's. A host harness
  (gn_host.h) dumping NAMES of all 14 slots, PRESET of every offered preset and one past (the echo, the track's every
  parameter, the screens' two names, the pattern hint) and Easy's every place of every whole list: the same 1,240
  lines, byte for byte, on 4d8a1b9 and on this round.
- **Results** (local, as rounds FW-CONTENT-4/5/6 ran them: a local copy of `tests/run_tests.sh` and the fork runner without the
  package, OTA and loader steps): host ALL HOST TESTS PASSED (the golden regression's 87 renders unchanged), fork ALL
  FORK TESTS PASSED (gn_core_test, gn_bank_test's three builds, gn_content_test, gn_budget_test among them). Simulator
  (`GEN` gen-gnomad10-088befd, with the simulator's two lines above): the editor test passes; coverage 238 screens,
  1,974 checks, 0 failed (as rounds FW-CONTENT-4/5/6); the tour's report and all 476 page files are byte for byte
  4d8a1b9's.
- **Sizes** (the simulator's firmware unit, host `size -m sim_fw.o`, 4d8a1b9 -> this round): `__DATA,__const` 49,024 ->
  35,712 (**-13,312**: the 416 `preset_t` of 32 B), `__TEXT,__const` 481,708 unchanged (the ID lists were there
  already), `__text` 541,216 -> 542,464 (+1,248: the readers, `gnc_knob`, the calls), `__literal8` 1,568,
  `__cstring` 17,985, `__bss` 9,316, `__gnfw_bss` 81,580, `__gnfw_data` 276, the pool and `.noinit` unchanged: **no
  RAM**. On the target, expected about -10 KB of constants (416 x 24 B = 9,984 B), less the readers' code at `-Os`;
  outside the CONTENT line, inside `max:image`; the target's own figures are CI's (rule 6).

### Round FW-CONTENT-8: a fresh Gnomad starts with chosen sounds, by bank ID

- **Before**: main.c's `felucca_init` puts `TRK_DEF`'s engine and preset *number* on each track (engines.c, upstream's:
  ANALOG 4, FM6 4, LOFI 0, DRUM 0). On the Gnomad slots a preset number is a place in bank order, so track 2 started
  on whatever FM sound sat fifth (bank 36 "Pylon", where Felucca had its pad) and track 4 on the first kit: inherited
  positions, not choices (finding SIM-R20).
- **The table** (`gn_state.c` `GN_POWER_ON`, each by bank ID through `gn_content.h`'s `GN_CONTENT_P<id>` / `K<id>`
  and `GN_AUTHORED`: an ID the banks do not have authored, or a place past the engine's list, does not compile):

  | Track | Engine | Bank ID | Name today | Why |
  | --- | --- | --- | --- | --- |
  | 1 | VA (9) | 145 | Acid Lead | a lead from VA's Lead group (136..183) for the melody track; Felucca's own track 1 was ANALOG ACID, so the same character on Gnomad's VA |
  | 2 | FM (12) | 72 | Lamplight | the first sound of FM's Keys group (72..127): a tine electric piano, FM's signature keys (Easy's TINE PIANO plays it too) |
  | 3 | GNOME (7) | 282 | Sing Ah | a lesson vowel (280..295): the "Sing" row's open, clear "ah", the plainest of the five vowels |
  | 4 | DRUMS (10) | kit 0 | Big Step | kept: the first kit, and Easy's KIT (the Drums picture's first sound, kit 0 with the sample block out), so Advanced's track 4 and Easy's drums start on the same kit |

  Names are the banks' today and only shown here; the firmware and the test name the IDs (a rename changes nothing).
  Each place is inside its engine's offered list (VA 49 of 96, FM 32 of 127, GNOME 10 of 64, kit 0 of 32), so
  NAMES, PRESET, the screens and a saved project see them as any preset.
- **The hook** (`gn_easy.c` `gn_power_on_sounds`, under `GN_ENGINES`): once a power-on, at the first main-loop frame
  (`gn_boot`, before Easy's entry decides the Gnome and before the start-in-Easy check, so `GN_START_EASY` 0 builds get
  it too), or, if the app's first command comes earlier (main.c runs `ed_service` before the first `ui_input`), from
  `gn_editor` before that command runs, so the command's own load lands on top. Loaded as `felucca_init` loads
  `TRK_DEF`'s: `set_engine_of` where the engine differs, then the preset with its sends (`gn_preset_put`), no undo copy
  (`undo_depth`); the steps, the patterns and every kept setting are `felucca_init`'s. The flag is a spare bit of
  `gn.booted` (32, `GN_BOOT_SOUNDS`): no `.bss` growth. main.c, engines.c (`TRK_DEF`) and project.c are untouched; the
  engines-off build keeps Felucca's `TRK_DEF` sounds.
- **Every power-on, not only the first**: the first plan said "the first power-on path only". Unlike the palette and the
  mode (round 18, kept in the settings record, so only an unmarked record is changed), the tracks' sounds are not
  saved: every power-on starts from `felucca_init`'s defaults and nothing loads a project at power-on, so applying the
  table only to an unmarked record would start the second power-on on bank 36 again. It runs on every power-on and
  never after anything has been loaded; restricting it to the unmarked record is one condition (`gn_mode_marked`).
- **Not changed**: a project track stored as "the power-on sound" (project.c `PROJ_DEF_SOUND`: a
  format-1 project's tracks 2..4) still loads `TRK_DEF`'s preset number on its stored engine (FM 4: bank 36). That is
  a project's own content path in upstream's file; following this table there is a decision about old projects.
- **Tests** (`gn_bank_test`, the swap build, ASan and UBSan): `felucca_init` alone leaves track 2 on FM place 4 (bank
  36) and track 4 on kit 0; after the first frame, fresh (an unmarked record: a new synth, or Felucca 1.0.1's), a
  Gnomad record left in Advanced, and `GN_START_EASY` 0: each track's engine (asked and playing), the bank ID at its
  preset, the record's bytes the core bound, empty steps, a note sounding, no undo copy, Easy's entry Gnome still
  only pending; the app's PRESET before the first frame wins on its track; a project saved with other sounds (bank 36
  on FM among them), the synth switched off and on with it in flash, loaded by the app before the first frame and
  again after: every track's engine, preset, every parameter and the steps as saved. Without the hook six checks
  fail; with it only at the first frame (not in `gn_editor`), the two "before the first frame" checks fail.
- **Results** (local, as rounds FW-CONTENT-4..7 ran them: a local copy of `tests/run_tests.sh` and the fork runner without the
  package, OTA and loader steps): host ALL HOST TESTS PASSED (the golden regression's 87 renders unchanged: the
  engines-off build), fork ALL FORK TESTS PASSED (gn_bank_test's three builds with the seven power-on checks,
  gn_core_test, gn_content_test, gn_budget_test among them). Simulator (`GEN` gen-gnomad10-088befd, sim 0d2e1df):
  the editor test passes; coverage 238 screens, **1,973** checks, 0 failed (f920b0a: 1,974): the same screens, but
  the tour now meets track 1 on VA at power-on and ANALOG later in its engine sweep (VA's EDIT pages have fixed
  knobs: exits 80 -> 79, knob labels 449 -> 448), and EASY DRUMS' Wobble is now heard (sound 101 -> 102; the known
  "barely heard" notes are 4: ORGAN, GLASS BELL, CLOUD, PIANO: SPACE). The tour's pages, each run into an empty
  directory: of 238, the HOME / EDIT 1 / EDIT 2 pages named after track 1's engine swap places (003-home-va, ...;
  the ANALOG ones move to 080..082), 158 others differ (the Advanced pages name track 1 "VA Acid Lead", was "ANALOG
  ACID", with its values; the Easy pages after the tap test show 119 BPM, the tap check "about 120" passing), 62 are
  the same bytes. Session test: the same seven pictures fail as on f920b0a and its pictures are byte for byte
  f920b0a's run; its sound now differs as well (`audio.txt`: hash ff01b6765028280f, peak 1,697,152; was
  efef80e194d6218d, 1,697,280): the session plays tracks 2..4's power-on sounds. Its references and the tour's are
  the simulator's to re-record.
- **Sizes** (the simulator's firmware unit, host `size -m sim_fw.o`, f920b0a -> this round): `__text` 542,464 ->
  542,732 (+268: the hook), `__TEXT,__const` 481,708 -> 481,712 (+4: the table, 8 B), `__DATA,__const` 35,712,
  `__literal8` 1,568, `__cstring` 17,985, `__bss` 9,316, `__gnfw_bss` 81,580, `__gnfw_data` 276, the pool and
  `.noinit` unchanged: **no RAM** (the flag is a spare bit of `gn.booted`). The target's figures are CI's (rule 6).

### Round FW-CONTENT-9: a remembered Easy song survives power-on (test only)

- **Check** (`gn_bank_test`, power-on block): Easy SAVEs a song of the user's own sounds (banks 137, 73, 200, kit 2),
  notes on three tracks, 97 BPM; off and on, flash kept, `GN_BOOT_SOUNDS` set: slot and record bytes 17..30 unchanged
  (RAM and flash), Easy on, the slot Easy's; loaded (by the app), tracks, bound records, notes and tempo as saved.
- **Mirror**: no record gives exactly the power-on sounds (FW-CONTENT-8's "power-on, fresh", not repeated).
- **Proof**: the hook ignoring `GN_BOOT_SOUNDS` fails it (0 of 4 as saved). No firmware change (byte-identical).

### Round UX-R: Felucca's seven remaining engines retired (tests, Easy, the target cost lines)

The decision ("Felucca's engines retire in the shipping build"; `gn_engines.h`, b5ef6a6) compiles out
ANALOG, PHASE, LOFI, SAMPLE, VOICE, TRIO and NOISE too: thirteen retired, each slot a silent descriptor
(`gn_size_stub.h`). The fork's host tests were written for the seven kept; CI run 38190621183 failed three of them.

- **Easy had 21 silent sounds** (the cause of `gn_ux_eng_test` UX-1 "a key held: the row sounds"): the sounds on those
  seven engines, among them Easy's own Gnome (VOICE), the one EN_ALGO picks on track 3 there. Changing the test's
  sound would have hidden it. `gn_state.c` `GN_FEL(<engine>, <preset>, <bank id>)`: with the swap and that engine
  dropped, the bank sound; otherwise (the engines-off build, or `GN_DROP_<ENGINE> 0`) Felucca's engine and preset as
  before, byte for byte. Easy's knobs on a Gnomad slot are the core's four already (`gn_easy.c` `GN_CORE_KNOBS`), and
  the Gnome's syllables only act on VOICE, so nothing else changes. The 21 (Easy's name -> bank ID "name today"; all
  are picks, one ID each in `gn_state.c`, "loose" where nothing close is authored): GNOME -> 282 Sing Ah
  (the power-on track 3 sound); VOX LEAD -> 280 Sing Oo; WHISPER -> 302 Whisper Oo; WOW BASS -> 309 Rumble Ah; WHALE
  -> 299 Giant Ah; CHOIR -> 279 Mushroom Choir; PHASE BELL -> 338 Tower Bell; RING BELL -> 364 Silver Bell (loose);
  LASER -> 158 Laser Line (its arpeggio kept); PULSE -> 141 Pure Square; 8-BIT ARP -> 166 Bright Square; STEP LEAD ->
  153 Twin Pulses; CHIP CHOIR -> 160 Glass Square (loose); ARCADE -> 371 Space Zap; SUB BASS -> 5 Moss Sub; SQUARE BASS
  -> 38 Square Root; FAT BASS -> 24 Night Train (loose); SOFT PAD -> 190 Wool Pad; STRINGS -> 188 Soft Strings; WIND
  -> 217 Hollow Wind; RAIN -> 212 Rain Glass (loose). (Round FW-P0: round 24's STRING PAD and WARM STRINGS replace WIND
  and RAIN: 214 Tape Strings, 194 Velvet Pulse.)
- **A core voice for a new note on a full core** (`gn_core.c`): with every sounding part the core's, 8 releasing VA
  tails held all 8 core voices and 8 FM notes on another track, which took their tokens, got 1 voice: the sweep's
  PANIC for the VA track was appended after the FM NOTE_ONs. `gnc_add_first` puts that PANIC at the head of the
  block's events (offsets recomputed), so the tails are cut before the new notes ask. Without it the budget case below
  fails (1 core voice for 8 tokens).
- **Tests**: `gn_core_test`: "build: the thirteen retired"; a key on each of the seven slots is silent and holds no
  token (new); switch: a held VA note released when the track switches to FM, and to a retired slot (ANALOG's); mix:
  VA against FM, GNOME and SAMPLER on another track, each pair within 6 dB and louder together (the ANALOG-against-VA
  case kept under `#if !GN_DROP_ANALOG` for a comparison build); budget: 8 releasing VA voices, then 8 FM notes (72
  Lamplight) on another track: VA's tails cut, 8 core voices for 8 tokens, none left on VA; cost: FM against VA.
  `gn_bank_test`: the 21 in its Easy list (each bound, its record's bytes, heard, Bright as stored: 49 of 49) and a
  check that fails on any Easy sound left on a compiled-out engine. No screen golden changed (`tests/gn_screens` is
  the engines-off build; cb7fee9 kept its bytes).
- **Results** (local, the fork runner without the package, OTA, loader and name steps; `build/gen` from the
  generators, the font and icon headers from an earlier CI copy, their tools unchanged): ALL FORK TESTS PASSED.
  Simulator (gn/sim cea9640 built on this tree): 242 screens, 2,133 checks, 0 failed; "every Easy sound heard on a
  key" 50 passing.

### The target cost lines on the shipping build

`tests/target_budget.py` (upstream's) counts, in each listed function of the chip's disassembly, the instructions
inside loops, x4 per loop level, a divide x9; `tests/target_budget.txt` holds 1.0.1's figures, +10 % allowed. A
static count, not cycles. `tests/gn_target_budget.py` adds the swap's lines: each Gnomad engine's functions (every
function its source file defines, the whole body weighted the same way, because the core works per sample through
calls) against the sum of the retired engine's lines in its slot, and the interrupt's own count plus the loop count
of `render_common` (the core's block loop) against `fm1_alnk0_irq`'s line. Run 38189514492 (gn/engines):

| Line | Got | Line | x | What it is |
| --- | --- | --- | --- | --- |
| FM (FM6's slot) | 2,265 | 470 | 4.8 | 1,229 of it is note-on, patch checks and defaults, not per sample; `gc_fm_render` 831 and its helpers about 1,036: still about x2 |
| GNOME (WHEEL's) | 1,385 | 369 | 3.8 | `gc_gnome_render` 566 and `gc_gnome_coeffs` 564 (per sample or per control tick: the count cannot tell) |
| SAMPLER (SLICE's) | 994 | 573 | 1.7 | 573 is note-on, asset and group checks; the render path, about 421, is under the line |
| DRUMS (DRUM's) | 35,730 | 848 | 42 | `gc_drums_render` alone is 35,028 (1,836 instructions, 8,735 "in loops") |
| interrupt + `render_common` | 61,881 | 37,502 | 1.65 | 36,309 the interrupt's own, 25,572 the core's frame loop (its callees not counted) |

- **DRUMS x42 is mostly the method.** `gc_drums_render` is one hit voice for one sample, not 16 pads (each pad hit is
  its own voice; no pad loop). It is a switch over the drum recipes and only one runs a sample, but the whole-body
  count adds every case; its real loops are short (recipe H's 6 sines, `dr_T3`'s at most 3 subtractions), yet the
  x4-a-level weighting (an assumption about iteration counts) and backward branches into shared code read as nested
  loops multiply the 64-bit arithmetic's long instruction sequences. Some of it is real: per-sample 64-bit maths on a
  32-bit core. Only a cycle measurement on the synth settles it.
- **The same holds less strongly for FM, GNOME and SAMPLER**: the slot sum counts note and patch-setup code with the
  render path. FM's render path is still about twice FM6's line.
- **The interrupt line** measured Felucca's interrupt with its engines as calls (not counted); the core's frame loop
  is added on top, its callees still not counted, so 61,881 is neither a bound nor comparable with 37,502.
- **What the seven retired engines free**: not interrupt time by themselves (a track plays one engine; Felucca's
  per-block worst case was one engine a track, and their lines, 2,002 together: ANALOG 585, TRIO 516, VOICE 314,
  NOISE 259, SAMPLE 126, LOFI 123, PHASE 79, have no Gnomad engine in their slots). They free flash and RAM: the
  shipping image links at 535,060 of 581,564 B, `.data` + `.bss` 80,400 of 98,304 B (b5ef6a6); with them in it did
  not link. Every sounding voice is now the core's 8, so the block's cost is the core's 8 voices plus Felucca's
  sequencer, FX layer and master.
- **Not changed**: the lines, the tolerance and the slot mapping. A suggestion only: count the slot from
  the per-sample path (`gc_mix_frame` and what it calls), not every function in the file, and measure the interrupt
  with the synth's cycle counter under 8 core voices.

### Round UX-L: Advanced lists only the engines that play

After UX-R the seven retired slots were silent but still offered: PRESETS, the ENG knob, the EDIT layer's keys and the
editor listed ANALOG PHASE LOFI SAMPLE VOICE TRIO NOISE.

- **The synth** (`engines.c`): `eng_gone(e)` is a slot of a Felucca engine the swap compiled out (`GN_DROP_<ENGINE>`);
  `eng_ok` no longer offers it, `ENGINE_ORDER` leaves it out and `NENG_SHOWN` counts it out (`GN_GONE_N`,
  `gn_size_flags.h`): VA FM DRUMS GNOME SAMPLER, 5. A track already on such a slot shows where FM is (`eng_rank`, as
  DIGITAL's sounds do) and steps from there. The numbers do not move (stores, projects, favourites, the protocol). A
  user preset stored on a retired slot stays on it (`upreset.c`), rather than playing its values on FM.
- **The editor**: INFO sends `N_ENGNAME`, DESC's ENG list, so the two cannot differ. `GN_EDITOR_HIDES_GONE` (default 0)
  makes a retired slot an empty descriptor: INFO and DESC name `""`, NAMES 0 presets. **Off** because the app on main
  (the companion app's `FirmwareShape.isGnomad10`) recognises the Gnomad engines only when INFO's names are exactly
  `EngineTable.gnomad10.infoNames`; with `""` it reads an unknown shape, loses the 1.0 layout (`hasFelucca10Layout`:
  Easy's engine settings would land on the matrix ids) and lists every slot by number. Turn it on with an app change:
  the seven slots `.hidden` (name `""`) in a table the shape accepts, and `order` the five.
- **Tests**: `gn_ux_test` UX-7 (GN_ENGINES): the order; every PRESETS entry offered; the EDIT layer's INIT on the
  sixth key; ENG turned five detents round both ways on the PRESETS page; INFO equal to the app's table (switch off)
  or `""` on the seven (switch on: the runner builds it again with `GN_EDITOR_HIDES_GONE 1`); DESC's ENG names equal
  to INFO's; NAMES of each retired slot. `gn_core_test`: five offered, none retired. Simulator tour on this tree: 239
  screens, 1,862 checks, 2 failed, the same 2 as on 1b4f02a (EASY PLAY CHOIR's WOBBLE barely heard); the 21 fewer
  screens are the seven engines' EDIT pages no longer walked.

### Round SEQ-1: the sequencer, phase 1

The sequencer's first phase on the cutover base (gn/ux), from the sequencer spec and the design choices of
2026-10-11: GN-09 **layout A** (four lanes, one playhead; the ring is out); knobs do knob things, buttons act, what
turns lights up in line, SELECT changes the page; SELECT is more than the tempo. Everything is `gn_*` and behind `FELUCCA_GN`; no upstream file, protocol command, store or record changes.
- **`gn_seq.c` (new): the sequencer's API.** `gn_seq_*` reads and writes the steps for the pages (length, grid, bars,
  places, the playhead, what a step begins, holds, which drums it plays, its strength, chance and length; a drum on or
  off, a note in or out, a held step's four edits). Phase 1 keeps Felucca's model (four tracks of 64 `step_t`, LEN,
  DIV, ties, velocity, chance) behind it, so phase 2 swaps the storage (the spec's 12-byte step, shared 256-step pool)
  without the pages changing. Every write takes Easy's undo copy first (SAVE held undoes) and is refused while a take
  records or a song plays; the bytes are written with the audio interrupt off. Nothing writes flash.
- **The page ring (spec S-1, S-3).** SELECT turns HOME . SOUND . GROOVES . LOOP . STEPS . SONG . MIX and round, a
  detent a page (up to three a frame on a fast turn); seven dots under the tempo show where you are. Step entry (SEQ's
  third page) sits where STEPS does. HOME, SEQ and GLO still go where they went. GROOVES and SONG are in the ring and
  say SOON (the shelf and the song are phase 2).
- **The tempo (spec S-2).** EDIT held + SELECT moves it a beat a detent, its big number on screen while EDIT is held
  ("TURN SELECT"); EDIT's press then counts no tap (UX-3's take-back). EDIT tapped is tap tempo, as before. SELECT
  alone never moves the tempo now (`t_tempo` and every test that set the tempo use `host_tempo`, EDIT held + SELECT).
- **LOOP, layout A** (`scene_loop`): the four tracks as lanes across one bar (16 places, x 26..234), the selected one
  tall (44 px: a melody's notes by pitch, a kit's three most used drums as rows, its level in the badge), the others
  slim (18 px) with their hits; the strip above says BAR n OF m with a block a bar; ONE playhead line through every
  lane, each lane's place under it drawn in ink. Muted lanes are drawn hollow. Each lane is a badge and four parts (a
  beat each), so a playhead step redraws two beats of four lanes, not the field.
- **STEPS, layout A** (`scene_steps`): the other three tracks as slim strips (the playhead through them), then the
  selected track's bar: sixteen places in line with the sixteen white keys (eight on the 1/8 grid), the step numbers
  1 5 9 13 under them, the playhead's place marked, the cursor (the last step tapped) outlined. A melody: each note's
  name over it, its strength as the block's height; a kit: K2's drum as blocks and a dot for each other drum on the
  step; a step that plays only sometimes wears a ring. **The tap grid**: while STEPS shows an Easy grid and nothing
  records (not armed, no song, SHIFT up) a white key is the place of the bar shown: tap = on (the sound plays: K2's
  drum or the pen note, `gn_key`), tap a lit place = off (on letting go, unless a knob turned meanwhile), and the page
  stays on that bar 3 s. **Hold a key + K1..K4** = that step's NOTE (a semitone a detent; a kit has none), STRENGTH (4 a
  detent), LENGTH (1 to 16 steps as ties over rests) and WHEN (ALWAYS OFTEN SOMETIMES RARELY HARDLY: step chance 100
  75 50 25 10), the place outlined in the colour of the knob turning. K2 on a melody is the **PEN** note (what a tap
  writes); K1 the bar, K3 SNAP, K4 VOLUME as before; black keys rest on STEPS. REC armed or recording gives the keys
  back to playing.
- **Lit in line** (every page with cards): the knob turning (and 1.2 s after, `GN_LIT_MS`) has its card drawn with a
  3 px ink border (a sound's bubble a 3 px ring), and on LOOP and STEPS what it changes is drawn in its colour: BARS /
  the bar outlines the bar blocks blue, SWING tints the off-beat places green, SNAP outlines the places yellow, VOLUME
  fills the tall lane's level vermilion, the PEN / DRUM label turns green; a held step's knobs light the held place.
  `t_seq1_lit` checks it by pixels: only that card changes among the cards, the changed field is >= 90 % green, and
  1.2 s later the field is exactly as before.
- **State**: appended to `gn` (`klit_t[4]`, the tap grid's `steps_keys / steps_note / steps_cells / hold_key /
  hold_used / hold_off`, `tempo_hold`: 25 B host) and to `gs` (lit, ring, tempo, the lanes, the tap grid's places:
  about 90 B); `GP_GROOVES`, `GP_SONG`, `GV_GROOVES`, `GV_SONG` appended to their enums. `gn_key` (the audio interrupt)
  reads only `steps_keys`, `steps_cells` and `steps_note`, which the panel's frame sets.
- **Tests**: `gn_ux_test.c` `t_seq1_ring`, `t_seq1_tempo`, `t_seq1_loop`, `t_seq1_steps`, `t_seq1_steps_kit`,
  `t_seq1_lit` (both builds); every earlier case unchanged but the tempo's gesture. `gn_screen.c` builds a beat on four
  tracks with the tap grid and shoots `easy-seq1-*` (LOOP stopped and at three playhead places, STEPS on a kit, a
  melody playing and a held step, the lit SWING, the tempo, GROOVES and SONG); every approved screen with a header
  gains the ring's dots, the loop and steps screens are layout A (all re-approved in `tests/gn_screens`).
- **The simulator** (gn/sim, `easy_seq10`): the ring both ways, the tempo on EDIT + SELECT, the tap grid, a held step,
  the lit card, and the playhead on LOOP at three places and on STEPS at two (drawn where the sequencer plays, the
  line on the screen): 28 checks, 0 failed; the tour's tempo turns use EDIT + SELECT where the ring exists.

### Round FW-P0: gn/ux catches up with the shipping line (the staged cutover's V1.6 step)

gn/gnomad-1.0 1bd4432 (V1.5, as tested on a synth) merged into gn/ux 4351eb0 with `--no-ff`: Easy rounds 24 (step
entry in place, the Whale's octaves and the Cloud's strings, autosave, adoption `gn.adopt`), 25 (the last
saved song loaded at a power-on in Easy, the slots from flash, the header's check mark), 26 (the recording's
metronome `gn.click`, EDIT held + K3), the five demo grooves (`gn_grooves.c`), branding (round G: the 4
approved lines in main.c and usb.c, the `gn_protected.py` gate) and the raised EASY / V1 image lines. Both sides'
behaviour is kept everywhere; where the shipping line names a Felucca engine gn/ux retired, it is mapped:
- **the two new Cloud sounds on the Gnomad engines** (`GN_FEL`, by intent): STRING PAD (PHASE STRING) -> **214 Tape
  Strings** ("slightly wobbly string pad"), WARM STRINGS (ANALOG PWM STR) -> **194 Velvet Pulse** (narrow pulses nine
  cents apart, chorused: PWM strings; it was the CHIP CHOIR alternative, still unpicked). The Whale's octave -1 on SUB,
  SQUARE and FM BASS keeps gn/ux's bank ids (5, 38, 33); FM BASS's round 24 Bright map is kept (on a Gnomad slot the
  knobs are the core's four, `GN_CORE_KNOBS`, so it acts only in the engines-off build). `gn_bank_test`'s list follows.
- **The grooves' drums** are written in General MIDI notes for Felucca's DRUM engine. On gn/ux the DRUM MACHINE is the
  Gnomad kit 2 (House), whose pad i is MIDI 36 + i: `gn_gv_pad` maps each note to the identity with its role (snare and
  snare 2 -> SNARE 37, clap -> 38, closed and pedal hat -> 39, open hat -> 40, rim -> 44, cowbell -> 45, claves and
  wood blocks -> CLAVE 46, tambourine, shaker and maracas -> SHAKER 47) when the track holds a Gnomad kit. The kit
  shape (DRUM's TONE DECY SNAP KICK, `G->kit`) is not written on a Gnomad kit: the core reads only BRIT and WOBL there,
  so the kit plays as authored. Every groove uses kit 2 (one Easy sound); a per-groove kit (Boom Bap, House, Hush,
  Night Bus, Sprout) is left to a listening test.
- **EDIT held is two things now.** Round 26's metronome switch (K3, from EDIT's press) and UX-3's SHIFT (after
  `GN_SHIFT_MS`). K3 is the metronome on every page except where SHIFT, once on, gives K3 a write job of its own: a
  step's strength (the steps page, the record screen stopped) and track 3's pan (the volumes) (`gn_k3_shift_job`).
  The deep page (SHIFT on a sound) is read-only, so its K3 card says CLICK there. `KR_CLICK_ON/OFF` join gn/ux's
  `KR_*` list at its end.
- **Step entry** (round 24 on UX-2): NOTE / REST writes in place on a kit too (its K3); a key still writes its drum and
  moves on. `gn_ux_test` UX-2 follows (K1 moves the cursor before the rest is written).
- **Saves:** `gn_save_now` (round 24/25, SAVE and the autosave) marks UX-5's strip SAVED (`gn_saved_now`) once the
  slot reads back the same; the header shows both the ring's dots and round 25's check.
- **Power-on:** round 25 loads Easy's saved song after FW-CONTENT-8's power-on sounds were put; `gn_bank_test`'s
  FW-CONTENT-9 case now expects the song on the tracks at the first frame (it said "none loaded at power-on").
- **The grooves' list** draws in UX-3's peek rows (board GN-05 A: across the page, no picture). It stays where it was
  (PRESETS on the loop page); the ring's GROOVES page still says SOON (the shelf is phase 2).
- **Screens re-approved** (`tests/gn_screens`, by design): `easy-11-saved-gnome` (round 25's check beside UX-5's strip),
  `easy-26-record-click` (gn/ux's record screen: kit DRUM card, step-entry layout, the lit CLICK card),
  `easy-12-grooves` and `easy-12-grooves-save-first` (the GN-05 A rows and gn/ux's strip), `easy-ux3-deep` (K3's CLICK
  card while EDIT is held). The other 56 are byte-identical.
- **Tooling**: `sim/gen.sh` (gn/sim) and the install rehearsal accept a CI run whose only
  failed step is the shipping build's target cost, and say "target cost red (known over-count)"; any other red step
  is refused as before.
- **Figures** (CI run 38219995846 on 81b0636): image 546,676 B (+5,008 over gn/ux's 541,668; 34,888 B of the 581,564 B
  slot free; ENGINES `max:image` 565,180 holds), `.data+.bss` 80,484 B (unchanged; 17,820 free), core RAM 7,168 /
  7,168. Every step green but the shipping build's target cost, the same 5 lines (GNOME x3.8, DRUMS x42.1, FM x4.8,
  SAMPLER x1.7, the audio interrupt 62,157 vs 37,502). Simulator (gn/sim e303019 + the two FW-P0 sim
  commits): 251 screens, 1,917 checks, 2 failed (the known two: CHOIR's SPACE / WOBBLE, the Gnome's BRIGHT on Home);
  the boot test's crash and splash pictures differ from its 1.0 references by the branding (GNOMAD), by design.

### Round V16: the new core under gn/ux's screens (the staged cutover's V1.6 candidate)

gn/engines 4069648 (gnomad-core 6b11ee7: the msfa FM kernel, Apache-2.0; the drum voices adapted from Emilie Gillet's
code, MIT; profile 0x00010200) merged into gn/ux 8515e88 with `--no-ff`. The merge base is 81350ed, so git joined it on
its own: the vendored core is gn/engines' tree wholesale (`sync-core.sh 6b11ee7` re-run afterwards leaves git clean);
`gn_core.c` keeps gn/ux's PANIC-first sweep (the core's API is unchanged but for `GC_PROFILE_FX1`, which the adapter reads
from the header); ABOUT keeps branding's lines at the top and the re-vendor's two credit entries after Rings; LICENSING
and this file are the union; no budget line changed (gn/engines changed none). Bank tables and `GN_SOUNDS` ids are
gn/ux's (the content rebalance comes later).
- **CI** (the package's gate): a run red only on the target cost still uploads the package, the sites, the source, the
  app and the loader; any other red step withholds them (`pkg_gate`, see CI above).
- **Figures** (CI run 38223662324 on befee05): image 556,852 B (+10,176 over gn/ux's 546,676; 24,712 B of the 581,564 B
  slot free; ENGINES `max:image` 565,180 holds, +8,328), `.data+.bss` 80,484 B (unchanged; 17,820 free), core image
  42,697 / 81,920, core RAM 7,168 / 7,168, content 193,536 / 196,152. Every step green but the target cost, 5 lines
  (unchanged): FM 12,693 vs 470 (x27.0), DRUMS 1,469 vs 848 (x1.7), GNOME 1,381 vs 369 (x3.7), SAMPLER 994
  vs 573 (x1.7), the audio interrupt 62,157 vs 37,502 (x1.66). Goldens: 61 of 61 byte-identical, none re-approved.
- **Simulator** (gn/sim 49e5e05): 251 screens, 1,917 checks, 2 failed (the known two). The session test's sound hash
  differs (same length, peak -0.4 %; its screens and MIDI are the same): the new FM and drum voices, by design, like the
  boot test's branded splash; neither reference re-approved (older trees still use them). Every Easy sound sounds; FM
  levels within 0.5 dB of the old core but for SHIMMER (-4.4 dB RMS, darker), FROZEN (-3.6), STRING PAD (-3.4),
  RING BELL (-3.4, darker), PHASE BELL (-2.6), GLASS BELL (-2.0); the drum kits' hits are shorter (to be judged by ear).

### A second scorer: the interrupt's reachable code on its worst path 

`tools/slot_cost_reachable.py` reads the same listing and changes no line and no check. It follows the calls from
`fm1_alnk0_irq` (as gnomad-core's `tools/divcheck.py` walks the render path), builds each function's control-flow
graph (jump tables resolved, natural loops checked by dominators: a backward jump into shared code is not a loop)
and costs the worst path: a switch's dearest arm, not every arm added up; a loop's trips from the code (counter,
step, bound), else from the source line named in its `TRIPS` table, else 4, each such loop listed; a call its
callee's worst path; the core's control tick (`gc_ktick`, `GC_KRATE` 16: LFO, pitch and coefficient caches) at 1/16
("amortised", exact over a 128-frame half) or whole ("tick sample"); a dual-issue pair 1, a divide 9. Note-on,
MIDI, sequencer and envelope-stage-entry work (divcheck's `STAGE_ENTRY`) are listed apart; buffer clears once.
1.0.1's engines (the `--base` listing) by the same rules, from the listing only (clean room): one voice's render
for one CTL block / 32. Unit: static instruction cycles, an estimate; the clock, multiply latency and flash wait
states are not in it. Runs: gn/ux 1b4f02a (CI 38194365411) and gn/engines 81350ed (CI 38189514492), each with the
run's own 1.0.1 base listing; both give the same core figures (same gnomad-core).

| Slot | old scorer | old line | old x | reachable, amortised | tick sample | 1.0.1 engine, same rules | x 1.0.1 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| FM (FM6's) | 2,265 | 470 | 4.8 | 901 | 1,299 | 513 | 1.8 |
| GNOME (WHEEL's) | 1,385 | 369 | 3.8 | 469 | 1,089 | 107 | 4.4 |
| DRUMS (DRUM's) | 35,730 | 848 | 42 | 607 | 3,433 | 237 | 2.6 |
| SAMPLER (SLICE's) | 994 | 573 | 1.7 | 474 | 617 | 3,426 | 0.14 |
| VA (PHYS's) | 865 | 1,157 | 0.7 | 1,050 | 1,050 | 182 | 5.8 |

| One audio interrupt, one 128-frame half (2.901 ms), steady | gn/ux | 1.0.1 base |
| --- | ---: | ---: |
| Felucca's frame: sequencer, FX layer, reverb, master (every engine and the core at 0) | 274,427 | 271,445 |
| 8 voices of the heaviest engine (the core: its four `render_common` calls) | 1,676,368 | 3,507,872 |
| total | 1,950,795 | 3,779,317 |
| old scorer, the interrupt + `render_common` loops, against 37,502 | 62,156 | 37,857 |

- **DRUMS x42 was the method.** On its worst path one drum voice costs 607 a sample amortised: about 420 every
  sample and about 3,000 more on the one sample in 16 that runs the control tick (the LFO and, when the pitch moved, the
  per-recipe pitch cache, inlined: 1,610 instructions). The old count added every switch arm, read the switch's
  shared tail as nested loops and counted the tick on every sample.
- **FM**: 6 operators x an envelope step; the stage change (`fm_stage_n`, `gc_env_segment`) runs once a stage, so it is
  out of the per-sample figure. GNOME's formant coefficients are inside the tick (`gc_gnome_coeffs` 580, once in 16).
- **But the core is dearer per voice than the engines it replaced** in four slots (x1.8 to x5.8 by the same rules),
  and its own frame (FX 1,274, track gains 364, sends, master) is about 1,850 a frame always, up to 3,600 while a
  control ramps, drive is up, the master filter is under 8 kHz or the output clips. As instruction cycles at an
  assumed 320 MHz (the AC79's top clock; ours is not measured), 8 VA voices plus Felucca's frame are 210 % of a
  half. 1.0.1's own worst case by the same rules is 407 % (8 SLICE voices; 8 ANALOG voices, its dearest synth engine
  at 560 a voice-sample, 91 %): a static estimate, not a prediction. No build with the core has run on a synth yet:
  only a measurement settles it (below).
- **A one-off spike**: when an effect's input has been silent for `CLEAR_FRAMES`, `gc_fx_process` clears its delay
  memory in that one frame, byte by byte: echo 44,100 B (132,300 cycles), FDN 17,960 (53,880), chorus 2,048 (6,144),
  about 192,000 together, a fifth of a half at 320 MHz, inside one interrupt. For the core: clear in slices or
  by words.
- **Shedding does not relieve the core at once**: Felucca's overload shed (two halves over 85 %) kills a voice token;
  for a Gnomad engine that sends the core a NOTE_OFF (`gnc_done`) and the core voice renders its release tail.
- The core's event work, one call each (worst path): `gc_ev_note_on` 39,841 (FM note-on 23,211 of it),
  `gc_ev_parameter` 12,695, `gc_bind_track` (a sound change) 9,907, `gc_ev_all_off` 7,456.

#### Measuring it on the spare synth (no new protocol command)

The firmware already times its interrupt (`audio.c`): every half's render time in TIMER4 ticks (24 MHz), the
SYSTEM page's CPU column (`song.cpu_q8`: render time over the half's 2.901 ms, averaged over ~93 ms), the peak
(`felucca_dbg.max_us`), the halves the DMA overtook (`late`) and the voices shed. Only on the spare FM-1, after the
recovery kit has been rehearsed on it; never on a unit in daily use until the numbers are in.

1. **Stage A, no change**: the gn/ux CI package as it is. Read SYSTEM > CPU (Advanced, the global SYSTEM page, column
   4) after each step has run for 3 s: (0) stopped, silent; (1) one held note, one track, for each engine: VA, FM,
   GNOME, SAMPLER, a DRUMS pad roll; (2) 2, 4, 6 and 8 held notes of the dearest (VA, a pad with a long release),
   chords from the keys or 8 notes on channel 1 from a computer
   (`python3 -c "import mido,re,time; o=mido.open_output([n for n in mido.get_output_names() if re.search('fm-1|felucca',n,re.I)][0]);
   [o.send(mido.Message('note_on',note=48+k,velocity=100)) for k in range(8)]; time.sleep(8);
   [o.send(mido.Message('note_off',note=48+k)) for k in range(8)]"`); (3) the same with ECHO, SPACE and CHORUS sends
   up and the master drive on; (4) a demo groove on four tracks. Write down CPU % and whether it crackles.
2. **Stage B, upstream's own debug flag**: the same tree built with `FELUCCA_CDC=1 ./build.sh` (`tools/build.py`
   passes it; no source change, `usb.c` byte-identical; the USB descriptor gains a serial function, so the spare
   only). On the computer: `screen /dev/cu.usbmodem* 115200`, then `status` after each step of stage A:
   `cpu_pct`, `audio_max_us` (the worst half since power-on; `dbg` shows `last_us` and `max_us` in hex), `audio_late`,
   `voices_shed`. Step (5): play, stop all sound and wait 2 s with the console open: the effects' silence clear shows
   as a jump in `audio_max_us`.
3. **Reading it**: a half has 2,901 us; the shed threshold is 85 % (2,466 us). The clock and IPC fall out of step (2):
   one VA voice is 151,000 static cycles a half (1,182 x 128), so cycles per us = 151,000 / (us(n+1) - us(n)); the
   figure calibrates every number in the tables above. Pass: `audio_late` 0 and `voices_shed` 0 at 8 voices of the
   dearest engine with a groove playing; CPU % below a line still to be chosen (a suggestion: 70 %).
4. **Stage C, only if A and B leave it open** (a proposal, not built): `GN_CPU_PROBE=1`, our flag, in `gn_core.c`
   alone: `fm1_ticks()` around `gc_render_mono` to split the core's share from Felucca's per half, shown on Easy's
   screen (gn_screens.c `gn_draw`, top line: `CPU 43/61 CORE 38`, average / peak / core %). Not in `INFO`: that text
   is compiled in `editor.c` (upstream's), and the loader identity `fm1_install.py --info` reads must stay `FM-1_<n>`.

#### Line proposals (nothing here is changed)

1. **Slots: score them with the reachable scorer and set the lines at today's figures** (+10 % as now), as upstream set
   its own lines at 1.0.1's figures: FM 901, GNOME 469, DRUMS 607, SAMPLER 474, VA 1,050 (one voice, one sample,
   amortised). A static line catches a render path that grew; it cannot say the synth keeps up. The old lines are
   Felucca's engines counted by another rule (loops only, no callees) and compared with whole-body sums: no headroom
   follows from them either way.
2. **Interrupt: two lines instead of 37,502.** A static regression line, the steady half by the reachable scorer:
   1,950,795 (+10 %). And the bound that matters, from stage A/B: CPU % with 8 voices of the dearest engine and a
   groove, `audio_late` 0, set once measured (a suggestion: 70 %, 15 points under the shed threshold).
3. **If the measurement says no** (CPU % over the threshold at 8 voices), the choices are: fewer core
   voices than 8, cheaper master/FX paths (the per-frame 1,850 to 3,600 is paid whatever plays), per-engine voice
   caps (VA and FM dearest), or the clear spike first. The table above is where each would show.

### The target cost lines, adopted (2026-10-12)

The lines above were adopted. The CI step "The shipping build's target cost" now runs the reachable scorer on
the run's own listing (`build/felucca.dis`; the base's, built for Easy's budgets, beside it) with
`--lines tests/gn_target_lines.txt`, and fails only when a figure is over its line. The lines live in that one file,
each with its source. The old scorer (`tests/target_budget.py` + `tests/gn_target_budget.py`) still runs in the step,
after the new one, printed for the history and never a gate; it stays in the tree because upstream's host tests run
`target_budget.py` on the engines-off build and its retired-engine rule has its own test (`gn_target_budget_test.py`).
So Felucca's own lines (`fm1_alnk0_irq` 37,502, `perf_block`, `slicer_track` ...) no longer gate the shipping build
here: upstream's run of them on the engines-off build does, and the IRQ line below covers Felucca's frame in the
shipping build.

| Line | value | the new core's figure (gn/ux 41377ee, core 6b11ee7, CI 38223662324 listing) x 1.10, half up |
| --- | ---: | --- |
| FM | 988 | 898 x 1.10 = 987.8 |
| GNOME | 512 | 465 x 1.10 = 511.5 |
| DRUMS | 1,025 | 932 x 1.10 = 1,025.2 |
| SAMPLER | 521 | 474 (exactly 474.5, printed 474) x 1.10 = 521.4 |
| VA | 780 | 709 x 1.10 = 779.9 |
| IRQ, the steady half | 2,019,018 | 1,835,471 x 1.10 = 2,019,018.1 (198 % of a half at an assumed 320 MHz) |

- **Whose figures.** The proposal adopted was "today's figures +10 %"; today's core is 6b11ee7, so every line was
  set from the new core's figures (2026-10-12), not the proposal's old-core ones (070cf68: FM 901, GNOME 469, DRUMS 607,
  SAMPLER 474, VA 1,050 with its tick then undetected, the half 1,950,795; a first cut of this round gated on those as
  written and was red on DRUMS alone, CI 38282935342). Each line is a +10 % regression guard; the device bound (stage
  A/B on the spare FM-1) replaces it. The +10 % is inside the values: nothing is added at check time. A figure is
  compared as the scorer prints it, to a whole cycle.
- **DRUMS, the open cost item.** The new core's drums (6b11ee7, Peaks-derived recipes) cost 932 a voice-sample on the
  dearest recipe arm, every sample (the tick is 15 instructions: the wobble LFO only); the old core's drums were ~420
  every sample + the per-recipe pitch cache on the tick sample (607 amortised, 3,433 on the tick sample). 8 drum voices
  are 103 % of a half at 320 MHz by these rules. The line now guards 932 against regression; a cheaper per-sample drum
  path is gnomad-core's to find.
- **The scorer's control tick, fixed before measuring** (fc820ea). Its detector knew only `if (k != 0) goto JOIN`;
  the new core compiles `gc_fm_render`'s block (GC_FM_BLOCK 16: envelopes, gains and 16 samples of six operators) and
  `gc_drums_render`'s `gc_ktick` as `if (k == 0) goto TICK`, so FM's whole block was counted on every sample: FM
  13,926 a sample and the half 15,140,943 (1,631 %), as merged. Now both forms are found (the tick arm runs from the
  target to where the other arm first meets it; the factor is 1/(MASK+1) from the code's mask). On the old core's
  listing every figure is unchanged but VA's, whose tick was the inverted form all along: 1,050 -> 709
  amortised (the tick sample 1,050 unchanged); The proposal's VA line 1,050 is that undetected figure. On the new core: FM
  13,926 -> 898, DRUMS 968 -> 932, VA 1,050 -> 709, the half 15,140,943 -> 1,835,471. Test:
  `tests/gn_slot_cost_test.py` (both branch forms on a made-up listing, the gate, the lines file), in the fork tests.
- **Still an estimate.** Static instruction cycles at IPC 1, clock assumed; the bound that matters is the synth's
  (stage A/B above: CPU % at 8 voices of the dearest engine + a groove, `audio_late` 0).
- **CI run 38282935342** (231cb3e, the first cut on the old-core lines): every step green but the target cost, red on
  DRUMS alone (932 vs 607). The lines above are re-set from the new core's figures.
### Round CR: gnomad-core f16ae35 and its banks; Easy by bank name, the rebinds, FUNNY

The core and its banks re-vendored together from gnomad-core main **f16ae35** (the content rebalance, FUNNY v2 and the
VA's P22 Pitch envelope / P23 Motion depth; profile still 0x00010200, so an older core would reject the FUNNY records
with GC_E_FORMAT: the two land in one commit). Easy's table names every bank sound by its NAME; the rebind
picks and FUNNY are in.
- **Re-vendor** (one command each, both reproducible: a re-run at f16ae35 leaves git clean): `sync-core.sh f16ae35`
  (va.c, gc_internal.h, the tables; the records unchanged); `sync-content.sh --core f16ae35` (new mode: `git archive` of
  the commit, the core's own `build_banks.py --check` against its `content/BANKS.sha256`, `build_easy.py --check`, then
  `content/build`'s banks and `easy.tsv` with `content/pcm`; CONTENT-VERSION.txt has no time and no local path any
  more). The banks: VA 127, FM 127, GNOME 101, SAMPLE 29 (all 384), 32 kits; the PCM byte-identical; the CONTENT line
  unchanged (193,536 B). `tools/gn_gen_content.py` takes SPEC/30 1.1.6's group ranges (Bass 0..59, Keys 60..131, Leads
  132..173, Pads 174..213, Plucks 214..261, Gnome 262..325, Bells 326..357, Play 358..383) and now also writes
  `GN_CONTENT_PN_<NAME>` / `KN_<NAME>` ("slot, place") and `GN_CONTENT_ID_<NAME>` / `KID_<NAME>` (the id) for every
  authored sound whose name is unique. With FM at 127, every authored sound is offered (round FW-CONTENT-5's cap no
  longer hides any: PHASE BELL, RING BELL and ARCADE have console names again).
- **By name, never by id**: `GN_SOUNDS` and `GN_POWER_ON` name bank sounds (`GN_BANK(e, p, ICE_CHIMES)`); a name the
  bank lacks or has twice does not compile, a renumbered bank keeps every sound. `tests/gn_bank_test.c` checks Easy by
  the bank NAME it expects (it finds the one record of that name and checks it is the one bound) and the power-on
  sounds the same way; it writes **`build/gn_easy_table.tsv`** (index, picture, picture index, slot, Easy name, kind,
  bank id, bank name, octave, engine: the 57 rows as the firmware binds them, for the app's `EasyPictureSounds`).
- **FUNNY**: gnomad-core `content/easy.toml`'s FUNNY (FUNNY v2), generated: `tools/gn_gen_easy.py`
  turns the vendored `easy.tsv` into `firmware/gnomad-content/gn_easy_set.h` (`GN_EASY_PIC_<PICTURE>(X)`, each row
  checked against the bank by name; `tests/gn_content_test.py` regenerates it byte for byte). The ninth picture of
  **ten**, before the drums (the drums stay one turn back from the Gnome): 57 sounds, `GN_KIT` 9, `GN_SND_KIT` 52,
  the grooves' drums `GN_SND_KIT + 1`. Each plays its record as authored at Easy level 127, the top (+4 dB over 101 on the core's square law: at 101 the simulator measured their peaks ~6 dB under the other pictures'); the engines-off build's
  stand-in is ARCADE's Felucca recipe (NOISE ZAP, 106) so the host tests' knob checks hold. The picture: a face
  laughing with its eyes squeezed shut, the mouth wide open, shaking (gn_art.c `GA_FUNNY`, the pictures' line).
- **Screens**: ten dots, centred (play page, field name); the wheel for any count of pictures (`2 * GN_NPICS + d`; the
  old `+ 9` was nine's), the others' names on two lines (the second takes the rest); the peek list fits FUNNY's eight
  rows (17 px from y 72, the band untouched; seven or fewer as before); a header too long for "PICTURE * SOUND" shows
  the sound's whole name alone ("WHOOPEE CUSHION"). Goldens: 15 re-approved by design (the tenth dot on every play
  page, the wheel's FUNNY tile and name), 2 new (`easy-cr-funny`, `easy-cr-peek-funny`); the other 46 byte-identical
  (the BELLS and LASER peek lists too: Easy's names are unchanged there).
- **Levels** (engines-on only, `GN_EO`; on the core P_LEVEL is a square law, 40 log10(L / 127) dB, `gn_core.c` `gnc_ctl`,
  not Felucca's 0.5 dB a step): PHASE BELL octave +1 and P_LEVEL 104 -> 83 (-3.7 dB; "really low", meaning
  pitch; FIT: the loudest of the 21); WOW BASS 101 -> 127 (+4.0 dB, FIT); RING BELL 114 -> 127, the top (+1.9 dB;
  Handbell is 5.5 dB quieter than Silver Bell was: it still sits ~3 dB under GLASS BELL, the rest is the content's).
- **Figures** (CI run 38284985670 on ec23979; 38285513817 on 0ce94f1 changes only level constants): image 557,924 B
  (+1,072 over V16's 556,852; 23,640 B of the 581,564 B slot free; ENGINES `max:image` 565,180 holds), `.data+.bss`
  80,484 B (unchanged), core image 42,845 / 81,920 (+148: P22/P23), core RAM 7,168, pool 305,088, content 193,536 /
  196,152 (unchanged). Every step green (fork tests, skin, screens, protected gate, package uploaded) but the target
  cost's 5 lines (unchanged: FM x27.0, DRUMS x1.7, GNOME x3.7, SAMPLER x1.7, the interrupt x1.66).
- **Simulator** (gn/sim 49e5e05 with a local four-function patch so its tour walks any count of pictures: `GN_NPICS`,
  `GN_KIT`, `GN_NSOUNDS` read from the firmware instead of 9 / 8 / 49): 260 screens, 2,035 checks, 3 failed: the known
  two (CHOIR's Wobble barely; the Gnome's Bright dead on Home) and WHOOPEE CUSHION's Wobble barely heard (its own pitch
  envelope moves more than the core's Wobble adds: the content's); every Easy sound visited and heard, 57 of 57;
  the sequencer 28 / 0. gn/sim needs the patch before it tours this tree.

| picture | Easy sound | bank before (41377ee, gnomad-core ff8463d ids) | bank now (f16ae35) | why |
|---|---|---|---|---|
| GNOME | GNOME | 282 Sing Ah | 272 Sing Ah |  |
| GNOME | VOX LEAD | 280 Sing Oo | 270 Sing Oo |  |
| GNOME | WHISPER | 302 Whisper Oo | 292 Whisper Oo |  |
| GNOME | WOW BASS | 309 Rumble Ah | 299 Rumble Ah | level 101 -> 127 |
| ORGAN | ORGAN | 73 Parlor Organ | 68 Parlor Organ |  |
| ORGAN | FULL ORGAN | 64 Tin Harmonium | 60 Tin Harmonium |  |
| ORGAN | GOSPEL | 128 Steady Tine | 124 Steady Tine |  |
| ORGAN | JAZZ | 79 Reed Keys | 86 Reed Keys |  |
| ORGAN | ROCK DRIVE | 75 Copper Clav | 82 Copper Clav |  |
| ORGAN | TINE PIANO | 72 Lamplight | 80 Lamplight |  |
| BELLS | GLASS BELL | 339 Ice Chimes | 329 Ice Chimes |  |
| BELLS | PHASE BELL | 338 Tower Bell | 328 Tower Bell | octave +1, level 104 -> 83 |
| BELLS | RING BELL | 364 Silver Bell | 335 Handbell | rebind |
| BELLS | BELL TREE | 337 Starlight Glock | 327 Starlight Glock |  |
| BELLS | MARIMBA | 225 Tick Tock | 215 Tick Tock |  |
| BELLS | KALIMBA | 234 Kalimba Seed | 225 Kalimba Seed |  |
| LASER | LASER | 158 Laser Line | 150 Laser Line |  |
| LASER | PULSE | 141 Pure Square | 137 Pure Square |  |
| LASER | 8-BIT ARP | 166 Bright Square | 152 Glass Square | 1.1.6 removed Bright Square |
| LASER | STEP LEAD | 153 Twin Pulses | 143 Hollow Square | 1.1.6 removed Twin Pulses |
| LASER | CHIP CHOIR | 160 Glass Square | 184 Velvet Pulse | rebind |
| LASER | ARCADE | 371 Space Zap | 372 Space Zap |  |
| LASER | GLITCH | 369 Robot Hiccup | 371 Robot Hiccup |  |
| WHALE | WHALE | 299 Giant Ah | 289 Giant Ah |  |
| WHALE | SUB BASS | 5 Moss Sub | 5 Moss Sub |  |
| WHALE | SQUARE BASS | 38 Square Root | 29 Square Root |  |
| WHALE | FM BASS | 33 Deep Current | 37 Deep Current |  |
| WHALE | FAT BASS | 24 Night Train | 8 Thick Felt | rebind |
| CHOIR | CHOIR | 279 Mushroom Choir | 269 Mushroom Choir |  |
| CHOIR | SOFT PAD | 190 Wool Pad | 180 Wool Pad |  |
| CHOIR | STRINGS | 188 Soft Strings | 178 Soft Strings |  |
| CHOIR | SHIMMER | 200 Glass Harmonics | 196 Glass Harmonics |  |
| CHOIR | DRONE STRING | 184 Warm Blanket | 174 Warm Blanket |  |
| CLOUD | CLOUD | 185 Morning Fog | 175 Morning Fog |  |
| CLOUD | FROZEN | 202 Northern Air | 197 Northern Air |  |
| CLOUD | WIND | 214 Tape Strings | 176 Paper Lantern | rebind, was STRING PAD |
| CLOUD | RAIN | 194 Velvet Pulse | 353 Rain Chime | rebind, was WARM STRINGS |
| CLOUD | SOFT FLUTE | 137 Glass Whistle | 133 Glass Whistle |  |
| PIANO | PIANO | 74 Felt Hammer | 81 Felt Hammer |  |
| PIANO | PLUCK | 224 Snap Pea | 214 Snap Pea |  |
| PIANO | HARP | 233 Harp Drops | 224 Harp Drops |  |
| PIANO | NYLON GUITAR | 232 Silk Pluck | 223 Silk Pluck |  |
| PIANO | SAX | 169 Reed Whistle | 161 Reed Whistle |  |
| PIANO | BRASS | 168 Brass Kite | 160 Brass Kite |  |
| FUNNY | WHOOPEE CUSHION | - | 361 Whoopee Cushion | new (FUNNY, easy.tsv) |
| FUNNY | BIG BURP | - | 362 Big Burp | new (FUNNY, easy.tsv) |
| FUNNY | BIG BOING | - | 360 Big Boing | new (FUNNY, easy.tsv) |
| FUNNY | SLIDE WHISTLE | - | 358 Slide Whistle | new (FUNNY, easy.tsv) |
| FUNNY | SAD TROMBONE | - | 369 Sad Trombone | new (FUNNY, easy.tsv) |
| FUNNY | BOMB DROP | - | 366 Bomb Drop | new (FUNNY, easy.tsv) |
| FUNNY | DUCK QUACK | - | 363 Duck Quack | new (FUNNY, easy.tsv) |
| FUNNY | PEW PEW | - | 359 Pew Pew | new (FUNNY, easy.tsv) |
| DRUMS | KIT | kit 0 Big Step | kit 0 Big Step |  |
| DRUMS | DRUM MACHINE | kit 2 House | kit 2 House |  |
| DRUMS | HAND DRUM | kit 10 Latin | kit 10 Latin |  |
| DRUMS | TOMS | kit 17 Woodshed | kit 17 Woodshed |  |
| DRUMS | CHOPPED BREAK | 257 Woodpile | 247 Woodpile |  |
| power-on | track 1 | 145 Acid Lead | 141 Acid Lead | |
| power-on | track 2 | 72 Lamplight | 80 Lamplight | |
| power-on | track 3 | 282 Sing Ah | 272 Sing Ah | |
| power-on | track 4 | kit 0 Big Step | kit 0 Big Step | |

### Round MF: each sound's own four knobs

Asked for on 2026-10-12: "each sound preset needs to have different things assigned to each knob". The core side is
gnomad-core's macro maps (SPEC/CHANGES-1.3.0-macros: four recipe ids in record bytes 10, 11, 54, 55, `gc_macro_apply` /
`gc_macro_rest` / `gc_macro_label`, profile and API 0x00010300); this round is the firmware's.
- **Re-vendor** at gnomad-core **73aac7e** (5f71d82's macros, SPACE plate and re-fit, plus Gnome voice 2): `sync-core.sh
  73aac7e` (new `src/macro.c` and `src/fx_plate.h` in the unity build; the core's own make test 118/118) and
  `sync-content.sh --core 73aac7e`; both re-runs leave git clean. `tools/gn_gen_content.py` accepts a patch's bytes
  10, 11 (its map now; a kit's stay the zero flags). The glue's profile is the header's (`GC_PROFILE_FX1`, 0x00010300):
  a 1.2 core would reject the mapped records, so core, banks and glue land in one commit.
- **The shim** (gn_core.c `gnc_slot`): a knob whose recipe is one of the controls stays that control as before (the
  default map: Bright P5 from BRIT, Wobble P6 from WOBL, Echo P7 from DLY, Space P8 from REV; an ECHO or SPACE recipe in
  any place is that send, so the FX page's DLY and REV stay the sends). Any other knob is the sound's own: its amount is
  kept in EDIT 1's K1..K4 (P_E0 + its place: BRIT and WOBL are K1 and K2; K3, K4 are new descriptor entries, 0..127, so
  Advanced, the app and projects reach them), and a change sends `gc_macro_apply`'s writes for the **flash record**
  (the record as bound, never one the macro moved). The planning and `gc_macro_apply` run on the **main loop**
  (`gn_core_host`, from `gn_input` every pass, Easy on or off: host rate, as the core says); the interrupt only copies
  the one posted move of a track (dropped if the bind changed meanwhile) and holds P5..P9 of a mapped sound until its
  bind is planned (the core plays the record as stored meanwhile). With `gc_macro_apply` in the interrupt the worst-path
  scorer counted 18,016,217 (CI 38293369931); planned on the main loop the IRQ is 1,867,343 (line 2,019,018). A control
  a knob of the sound's own owns (K1/K2's setting, or a parameter its writes reach, e.g. SWIRL's chorus send) is not
  sent from its setting. The record-group writes of a move are the track's one group of the block, at one offset (the
  core rejects a block with two): one move a track at a time. Amount law: the knob's rest (`gc_macro_rest`) sits exactly on a knob place
  (`gnc_kr`), two straight pieces 0 .. rest .. 65535 (`gnc_amount`), so GN_PRESET (`gn_preset_get`) and Easy's load start
  where the sound was authored. DRUMS keep the kit four; a record without a map is exactly as before.
- **Easy** (gn_easy.c): `gn_map` reads the sound's own knobs (`gn_cmap`, per track, keyed by engine and preset);
  `gn_sound_put` leaves a Gnomad engine's E2..E7 alone (an Easy sound's Felucca settings, which those engines never had,
  were no-ops before and must not land in K3/K4 now) and puts the own knobs at their rest; the amounts persist, undo and
  autosave as every track setting does (same project bytes, same records). `gn_knob_label`: the record's names.
- **The SOUND page** (gn_screens.c): each knob row is named as the record names it (`gc_macro_label`: TONE MOTION ECHO
  SPACE for a sound without a map and for the kits); a turning knob's name is lit in its colour, every sound; a
  bubble's sign follows its name (sun, wave, echoes, arch; none for PUNCH, DECAY, SHIMMER, ...). Screens: 62 approved
  byte-identical; `easy-3b-knobs` re-approved by design (its four knobs were just turned: their names lit).
- **Which Easy sounds have their own knobs**: at 73aac7e 19 of the 53 pitched Easy sounds (48 own knobs); gnomad-core
  **706b31a** (macros-content: maps for the synth's Easy set, from our `build/gn_easy_table.tsv`; new recipes VA 19-20,
  FM 14-17, a Sample table 1..9; profile still 0x00010300) maps all 53: **114 own knobs** (the other knobs are the
  controls: Bright, Wobble, ECHO, SPACE). Re-vendored with both syncs at 706b31a (git clean on re-run).
- **Tests**: gn_bank_test round MF (the SOUND page names all 57 sounds' knobs as their records do; each own knob loads
  at its rest place and sends exactly its rest amount, its ends send 0 and 65535; every own knob moves the sound it
  plays, a fresh note at each end measured: level, brightness, length, a spectrum band (G14's 3 dB, an FFT of 24 bands)
  or the difference itself, each beyond two takes at 0, figures in
  build/gn_macros.tsv; the kits keep their four; `MF_WAVS=dir` writes each own knob swept); PRESET's check: a knob of a
  sound's own is exactly at rest and its writes there are the record's stored words; gn_core_test: every preset's own
  knobs at their rest places (102 across the 416 presets).
- **Figures** (CI 38294837725): image **568,564 B**, 13,000 B of the slot free: over the then `max:image` 565,180 and the
  16,384 B floor by 3,384 B. Decided 2026-10-12: the ENGINES floor lowered to 12,288 B (`max:image`
  569,276, "Budgets: the ENGINES line") until the size diet lands; the GNOME cost line 959 (872 x 1.10). The core grew 42,845 -> 50,817 B
  (+7,972: macro.c, the plate, Gnome voice 2); the fork's own code +1,194 B (gn_*.c 56,334 -> 57,528), `.bss` +680 B
  (the main loop's plans); `.data+.bss` 80,820 B (17,484 free). Target cost: FM, DRUMS, SAMPLER, VA and the IRQ under
  their lines; **GNOME 872 against 512** (Gnome voice 2's static cost). With the budget red
  the package is not made, so CI's fork tests stop at "no package"; they were run on the host instead (every one but
  the package, update and name steps, which need the package).
- **Simulator** (gn/sim 16ce99e): the session references re-approved (the same length and MIDI;
  only Home's K2 bubble, the power-on Acid Lead's own GROWL at its rest, and the new core's sound differ). The tour:
  260 screens, 2,071 checks, 3 failed: the two known, and WHOOPEE CUSHION's K1 (PITCH, the pitch envelope's depth: a
  next-note target the tour's held-note measure barely hears; the host's fresh-note measure hears it, x1.11 brighter).

### Size diet

The all-ours image reached 569,060 B (61a326f, CI 38299831430), 216 B under the ENGINES line with its floor lowered to
12,288 B. Measured from that run's map and listing (budget-inputs): the content 193,536 B and the core 51,295 B are
their own lines; what is left is the UI (upstream's and ours), the fonts and icons (70,895 B, every glyph drawn), the
editor protocol, the stores and the audio path. Three changes, none of behaviour, take 8,912 B; the floor is back at
16,384 B (`max:image` 565,180) with 5,032 B of headroom:
- **Main-loop code built for size** (5,504 B): upstream builds its UI, stores, editor and console with `minsize`
  (`tools/size_fns.py` SIZE_FILES) and the rest at -Os; the fork's screens and Easy (gn_*.c) and upstream's later UI
  files were never in that list. `tools/gn_size_fns.py` adds, with the engine swap, every function neither interrupt
  can reach (IR call graph from `fm1_alnk0_irq` and `fm1_timer5_irq`, an indirect call reaching every address-taken
  function of its parameter types), leaving out the protected files (main.c ota.c ota_hw.c usb.c libc.c hal), any
  function in its own section (.ram_text) and the core with gn_core.c. The same source, the same calls: only the
  code generation of main-loop functions changes. The interrupts' code is compiled as before: the reachable scorer's
  report (`tools/slot_cost_reachable.py --lines`) is the same figures, every line; the listing's interrupt-reachable
  functions are the same instructions but for global addresses and offsets (the merged globals moved 4 B), so the
  old scorer reads `fm1_timer5_irq` 33,019 -> 32,955 (one load encoded shorter). Stacks: `fm1_cstart` and `ui_input`
  8 B less, the rest the same. `.ram_text`, `.data`, `.bss` unchanged.
- **The retired engines' words in the skin** (2,368 B, gn_skin_words.c): each engine's knob labels and EDIT titles are
  looked up by the slot's descriptor label and page title, which a retired engine's slot never has (its silent
  descriptor says "-", a Gnomad engine BRIT WOBL K3 K4 and "MACRO"), so they are compiled only while that engine is
  (`GN_DROP_<ENGINE>` 0). A retired engine's HOME title stays (its descriptor keeps the name: a 1.0.1 project's track
  on ANALOG still shows "Analog"), except where a Gnomad engine took the slot (`GN_ENGINES`). The skin's host builds
  keep every entry (no swap), so its tests and renders are unchanged by construction.
- **FM6's factory table** (1,024 B): with FM6 retired its stub held 8 x 128 zero bytes for the editor's factory GET and
  LIST; `FM6_FACTORY_ROW(k)` reads `FM6_INIT`'s zeros instead: the same answers, byte for byte.
- **Not done** (each a choice, not dead weight): the S font and the icons Huffman-coded as M and L are (about 1.5 KB and
  5 KB, the same pixels, slower label and icon drawing: upstream kept S uncoded for speed); 13 icons no code names
  (about 1.7 KB, but upstream's tests and the skin's base renders build from the same generated header); FELUCCA_CDC or
  UAC off (behaviour); the core's tables (msfa_sin 8 KB and msfa_exp2 4 KB, vendored, its own line).

### Round FLOW-1: the performance flow (decided 2026-10-13 after a flow review)

The bar, from a player's feedback: "too much menu diving ... flow state is all I want ... auditioning things",
and "the ability to start and stop looping from the head of a section from a button". Built in three milestones on
gn/ux f3b2454; each pushed with CI green.

**M1 (P0).**
- **Performance transport.** Stopped, PLAY starts on the press, as before. While the loop plays (no song, no external
  clock: the clock owns the transport; not during a count-in), PLAY acts when let go: a tap stops at once, as before;
  held 400 ms (`GN_RESTART_MS`) it queues a restart from the top for the next bar line, and the band's transport word
  says RESTART NEXT BAR; held again before the bar, the restart is now (the NOW escape). GLO held + PLAY queues a stop
  at the bar's end (STOP AT BAR END); again before the bar, now. A queued state replaces only the transport word; it
  works from every page and changes no step, LEN or DIV. The audio interrupt does it on the bar line itself:
  `gn_click` (once a block, after the mix) sees the selected track about to enter a bar's first step in the next block
  (its own `seq_pos`, step length with swing, `seq_idx`; a bar = four beats of its DIV) and sets seq.c's
  `transport_req` (3: Felucca's own restart, GLO + PLAY in Advanced; 2: stop), which the next block's `events_block`
  runs before the sequencer's tick: the bar's first step from the top, or none. (The beat counter, `beat_pos`, was
  tried first: it runs a few samples apart from the steps' rounded lengths and let the next bar's first step through
  one block in four at 120 BPM.)
- **Fill.** GLO held + any key: the key's note (the sound's, a kit's pad; on STEPS too, never a step) rolls on the
  selected track, a sixteenth of the beat at a time on the beat's grid (seq.c `beat_pos`), half a sixteenth long; the
  first hit at the press; a key let go, or GLO, ends it. The key itself rests (`gn_key` returns KB_SILENT: seq.c records
  nothing, sends no MIDI); the notes go to the track's voices only (`trk_note_on`, in `gn_perf`, the audio interrupt):
  never written, never recorded, never a step, no MIDI out. GLO's tap (let go, unused) still opens the volumes; GLO
  held names its gestures in the band: FILL (KEYS)  STOP AT BAR (PLAY).
- **Record words.** The header and the band both name the state with the track: ARMED T1, COUNT-IN 3, RECORDING T1
  (the header); the band's second line ARMED T1 * KEEP, COUNT-IN 3 * NOTES, RECORDING T1 * REPLACE. The record panel
  says RECORD INTO T1 (playing) and T1 * STEP 5 (stopped).
- **The click's mark.** While the click sounds (count-in, recording, armed while playing) a small metronome sits beside
  the tempo; EDIT held says CLICK ON (EDIT + K3) / CLICK OFF in the band (with SHIFT's MUTE where SHIFT has it).
- **A blank loop is one bar** (spec S-5, 430-433): REC into an empty pattern makes one bar of its grid (the drums 1/16,
  a melody 1/32 as pick 9 has it), not two.

RAM: 24 B appended to Easy's state (`gn.tq`, the fill's keys and notes, PLAY's and GLO's presses). Tests: gn_ux_test
FLOW-1 (the restart on the bar line within a block, the NOW escape, the stop with the bar's last step played and the
next bar's first never, a tap, GLO's tap, the fill's grid and its never-written, never-recorded, never-a-step, the
words, the mark); gn_easy_test's three blank-loop checks at one bar; screens: 6 new (`easy-flow1-*`), 13 re-approved
(the record screens: the words; the loop pages recorded from blank: one bar).

**M2 (P1).**
- **Stable pickers.** The picture wheel (ALGORITHM) and the sound list (PRESETS) no longer time out: they stay until
  HOME (which closes them, the page under them staying), PLAY, a key, or the other encoder; the band says TURN
  ALGORITHM: PICTURE / TURN PRESETS: SOUND. (The loop page's grooves list keeps its 2 s.)
- **Audition.** FX held + keys: the selected track's sound, as long as both are held, with its ECHO and SPACE as their
  knobs have them; K1..K4 are the sound's own four on every page while FX is held (an echo or a space tried from the
  loop page); the band says AUDITION (KEYS)  SOUND (K1-K4). The same path as a fill (`gn_perf`, no roll): the keys rest
  for seq.c, nothing recorded, no step, no track selected. FX's tap (let go, unused) still selects track 1.
- **GROOVES, real.** SELECT's GROOVES page is the shelf: nine tiles, the five factory grooves (keys 1 to 5: the first
  white keys) and the four project slots, MINE 1 to 4 (keys 6 to 9), each with its key's number, FACTORY / MINE rows,
  an empty MINE in the second ink, the tile loaded filled in the track's colour with the loop's place under its name,
  a queued one blinking NEXT BAR. Playing, a key queues its tile: the loop stops on the bar line (the M1 queue) and the
  tile loads and plays from the top at the next input pass; the same key again: now. Stopped: at once. Never over music
  not kept (spec 180-182): stopped, Easy keeps it first (its SAVE, silently; FULL or NOT SAVED: it asks); playing, the
  tile asks NOT KEPT YET: STOP IT OR PRESS AGAIN, and a second press loads anyway. A MINE tile loads its slot as the
  power-on does (round 25: project_load, each Easy sound Easy's again). The tile keys rest (no sound) on the page.
- **SONG hidden** from the ring until it is real (six pages, six dots); the SOON stop is gone.
- **No DEEP page in Easy.** EDIT held (SHIFT) on the sound page keeps the sound's own knobs; SHIFT keeps its other jobs
  (MUTE, a step's note and strength, pan, the tempo). The deep page's code is gone from the build.

Known limit: a tile queued while playing loads in the main loop just after the loop stops on the bar line: the new
groove's first step comes one input pass late (a millisecond, or a frame's drawing: up to ~15 ms), where a restart or a
stop is on the line exactly. Screens: 6 new (`easy-flow1-*`: audition, the shelf, a tile queued, a tile asking, SHIFT
on a kit), 3 gone (the deep page, the SOON pages), every page with SELECT's ring re-approved for its six dots, the
sound page and the pickers for their band words (nothing else changed: checked by pixel box).

**M3 (P2).**
- **Named undo.** SAVE held, let go: the band says what it undid for 2 s: UNDID: LAST TAKE, UNDID: STEP EDIT (step entry,
  SHIFT's note or strength), UNDID: BARS, UNDID: SNAP, UNDID: STEPS (taps or a held step's knobs on STEPS), or NOTHING TO
  UNDO. The undo copy is named by its latest edit (`gn_undo_take`'s force argument carries the kind, `GN_UK`); the undo
  itself is unchanged (one level, the session's copy).
- **A sound changed while playing** (ALGORITHM or PRESETS): the band says TRACK 2 * SOUND CHANGED for 1.5 s; the
  transport is not touched (nothing was ever stopped by a pick: this only says so).
- **Another sound on the sound page** says so: the band NOT EASY: PICK ONE (ALGORITHM) (PRESETS rests on another sound);
  where the knobs do not move it either (no map), the four bars give way to NOT AN EASY SOUND, THE KNOBS REST HERE, TURN
  ALGORITHM FOR AN EASY SOUND.

Screens: 2 new (`easy-flow1-sound-changed`, `easy-flow1-undid-take`), `easy-3b-other` re-approved for its band.
A queued restart, stop or tile ends when Easy is left (Advanced's transport is Felucca's own) and when the music is
replaced by the app (`gn_rec_cancel`).

### Round EW: the Advanced pages on the Gnomad engines

A reading of gn_core.c (2026-10-13): the core heard only Bright, Wobble, Echo, Space, Chorus, the level and MUTE
(and a sound's own knobs, round MF); ENV, ENV DEST, LFO, LFO DEST, FX's Drive, SLICER, DLY, REVERB, CHORUS and every
PAN changed nothing; EDIT 2 was empty and EDIT 1 showed codes. Now (gn_core_adv.c, the main loop's side; gn_core_pages.c,
the pages; their head comments are the law of each knob):
- **What each page does on the core.** ENV: P1..P4 (VA, GNOME, SAMPLER); FM its six operators' envelopes (attacks moved
  by the knob's travel, decays, sustains, releases scaled). ENV DEST: VA's filter and pitch envelopes (P20, P22), the
  Gnome's scoop (P18). LFO: the engine's motion rate (VA P21, FM P15, GNOME P16). LFO DEST: Wobble's pitch depth (VA,
  GNOME P23), the Gnome's growl (P15). EDIT 1: the sound's four knobs where Home and Easy keep them, named as the record
  names them. EDIT 2 (new descriptors, P_E4..P_E7, each a move from the sound as authored, 0 = the sound): VA cutoff,
  resonance, detune, sub; FM depth (index), grit (feedback), the modulators' ratio, touch; GNOME vowel, size, breath,
  syllables (TALK, voice 2's P20); DRUMS tone, decay, snap (snare, clap, hats, rim), the kick's pitch; SAMPLER start,
  end, loop (OWN ONCE LOOP), pitch. DLY: the echo's time on the song's tempo (Felucca's divisions, halved until they fit
  the core's second), its repeat (feedback), MIX (every part's Delay send scaled, 90 = as sent). REVERB: SIZE (the
  plate's size and damping, 0x307; 90 = the core's 2.17 s). CHORUS: speed and depth. The master controls go to the core
  as track-255 PARAMETER events (SPEC/20).
- **What has none, said.** The core's motion is a sine from each note's start, never faded (WAVE, START, FADE); drums
  fade by themselves (ENV on DRUMS: EDIT 2's DECAY); no drive per part (Drive is master-only in the core); the echo has
  no tone; one plate, its damping follows its size (TYPE, DAMP); no pan (SPEC/01 D03: dual mono): VOICE 2's and the
  MIXER's PAN and Easy's SHIFT pan on the volumes (SHIFT there leaves the volumes as they are). Such a knob has no
  descriptor (inert) and the skin draws why in two short lines; a knob a sound's own knob already moves (its writes reach
  the same parameter) says "Knob n has it". SLICER and MOD are not offered on a Gnomad sound; ENV DEST and LFO DEST only
  where one of their knobs plays.
- **Rest and old songs.** An absolute page (ENV, ENV DEST, LFO, LFO DEST) sends nothing at its rest place (the place
  whose law comes nearest the record's own value), so the core plays the record exactly; elsewhere the page's own law
  (what its card shows). A track's settings get their rest places once, while P_ED_FX (a stored setting nothing ever
  read: "unused, kept for the formats") is not GNA_MARK (1): after every sound load (apply_preset_to puts it at 0;
  Easy's loads settle at once, gn_easy.c, so Easy's sounds play as authored and the entry's Gnome is not "changed"), and
  for a project saved before this round, which therefore keeps its sound (GN_PRESET's ENV stays 0 64 127 127: the
  settle replaces it).
- **Where it runs.** The main loop (gn_core_host, every pass) works out each bound track's wanted record words and kit
  fields and posts what differs from what the core has, one batch a track at a time (record group <= 8 words then
  P1..P4 streaming; a kit: one EDIT 2 knob's field on its pads, <= 16); the interrupt copies a batch into the block after the track's macro move, each group
  whole at one offset (an offset allocator in gnc_sync_controls: a group that would straddle an offset starts the next;
  SPEC/01's one patch and one kit group a track a block), never a move's group and the pages' in one block. The
  interrupt's only new arithmetic is DLY MIX's scale of the Echo send.
- **Kits and the Gnome (M2).** HAND DRUM and TOMS are kits on the Gnomad DRUMS (GNF_PADS): white keys pads 1..16; on a
  Gnomad kit the eleven black keys are LOW TOM .. CRASH, each once (they were Felucca's GM notes, three duplicates).
  Easy's GNOME loads with TALK 12: the core sings each key's syllable (P20, key mod 12); gn_key needs no part in it.
  The grooves play the kits they were written for (HEAD NOD Boom Bap, FLOOR FILLER House, RAINY DESK Hush, NIGHT BUS
  Night Bus, SKY GARDEN Pebble; all played House) with their TONE, DECY and SNAP on EDIT 2 (64-centred).
- **Tests**: tests/gn_wire_test.c (run_fork_tests.sh; ASan, UBSan): every page knob that reaches the core moves a fresh
  note at its two ends on each engine (45 of 45, build/gn_wire.tsv), every inert one says why (60), DLY / REVERB /
  CHORUS each move the sound, rest sends nothing, EDIT 1 is the sound's four, kits 16/16 white and 11/11 black distinct,
  the Gnome's syllables, no pan; `GW_WAVS=dir` the sweeps, `GW_PNGS=dir` the pages. gn_ux_test (engines build): SHIFT on the volumes has no pan.
- **With flow-1 (the merge, b2d13b4).** Both kept whole: gn_key is flow-1's (a fill's or an
  audition's key plays its sound through gn_perf_add, the GROOVES tiles rest) with EW's pads (a Gnomad kit's black keys
  LOW TOM .. CRASH, so a fill on a black key rolls that drum); the GROOVES tiles load through gn_gv_load, so each
  groove comes on its own kit with its EDIT 2 shape; the MIX page's SHIFT hint on the mono core is GS_HINT_MUTE. Test:
  gn_ux_test (engines) FLOW-1 x EW.
- **Size (the two together were 1,832 B over max:image).** No floor, cap or feature changed; image 567,012 ->
  565,012 B (+168 B under max:image 565,180; free 16,552): the Felucca VOICE paths the engines build can never take
  (gn_key's VOWL writes, gn_screens.c gs_vowel: VOICE is dropped, its slot never sounds) and Felucca DRUM's lane
  branches in the kit helpers (its slot is the Gnomad DRUMS: gn_gnomad_kit first) compiled out (416 B); the fallback
  knob maps keep each engine's Bright and the Wobble, Echo, Space maps once (GN_FB_REST, 272 B); Easy's sounds keep
  their Bright and an index to one of six Wobble/Echo/Space kinds (GN_TAILS, gn_snd_knob: 1,168 B); the why-cards'
  lines one string (48 B); the LOFI and FM6 phrase tables out with their engines (96 B). The same data, the same
  behaviour.

### Round EW-FIXES: the review of round EW (on gn/ux 81727bd)

A code review of 92ae6ee..81727bd and the gaps left by round EW:
- **A user preset reloads as saved.** up_store keeps every setting, P_ED_FX (round EW's mark) among them, and up_load
  set the track's preset to 0: a Gnomad sound saved from preset k came back on preset 0's record (another sound) under
  the Advanced settings settled for k, and the mark kept them from settling again. The mark now names the record its
  rest places are for: preset p is GNA_MARK_OF(p) = p + 1 wrapped into P_ED_FX's own -64..63 (preset 0 is GNA_MARK, 1,
  itself; 127 places, the most presets an engine offers), and up_load binds that record again (gn_core_adv.c
  gna_up_preset, one guarded line in upreset.c). No byte of a record or project changes shape and nothing is added to
  the editor protocol (UP_GET / UP_PUT carry P_ED_FX as before, inside its descriptor's range). Before: a record of mark
  0 (saved before round EW) loads on preset 0 and settles there, a record of mark 1 (round EW's) loads on preset 0 with
  its settings, both as they did (which record it came from was never stored); a song's track of mark 1 keeps its
  pages on whatever preset it has (round EW's settle accepted it, and still does).
- **The black keys on a Gnomad kit.** A Gnomad kit has sixteen pads (gnomad-core's sixteen fixed identities, KICK ..
  CRASH) and all sixteen are the white keys; the core plays a pad by its number and the velocity, nothing else, so a
  27-key kit cannot have 27 different drums. Round EW gave the black keys LOW TOM .. CRASH (MIDI 41..51), which are white
  keys 6 to 16 again (the review: the first black key and pad 6 both 41). Now each black key is a ghost: a soft hit of a
  drum beside it (F#3 KICK, G#3 SNARE, A#3 CLOSED HAT, C#4 OPEN HAT, D#4 LOW TOM, F#4 RIM, G#4 COWBELL, A#4 CLAVE, C#5
  SHAKER, D#5 CONGA, F#5 RIDE: the ghost notes grooves use; CLAP, the other toms, BONGO and CRASH have none), at half the
  key's velocity: the core's law makes it quieter, darker and shorter (5.5 to 8.4 dB under the white key's hit on KIT).
  Its note is the pad's in the ghost block, MIDI 4..19 (gn_core.c GNC_GHOST: below every General MIDI drum; pads by the
  same (note - 36) mod 16), so no black key shares a note with a white key: it records, plays back, fills, auditions and
  leaves MIDI OUT as itself, and MIDI IN reaches the ghosts the same way. Advanced's kit keys likewise: keys 1..16 the
  pads, keys 17..27 (A4 .. G5, which wrapped onto pads 1..11) the first eleven pads soft (gnc_drum_keys). Other MIDI
  notes still wrap onto the sixteen pads as before.
- **Glide, its Mode, Detune.** VOICE's GLD and GLMOD and VOICE 2's DTUNE move Felucca's voice pitch, which the core
  never reads (gnc_note_on sends the key alone): on a Gnomad sound they have no descriptor now and say "Not on this
  sound", as the other inert knobs do (three GNA_DEAD entries; no new words).
- **Step writing on a Gnomad kit (checked, unchanged).** gn_seq_drum_set and the record screen test `eng_req ==
  GE_DRUM`, which on the engines build is the Gnomad DRUMS' slot: a drum whose note is one of Felucca's lane notes (36
  37 38 39 42 45 46: KICK SNARE CLAP CLOSED HAT MID TOM COWBELL CLAVE) is written as that lane's hit bit, the others as
  notes. seq.c seq_step plays a hit bit on any engine as its lane note, the same note, at the same velocity (or 127
  accented), and gn_kit_has / gn_kit_of read both forms as the same identity, so a written step sounds and shows
  exactly as a note would. Only upreset.c's pattern (16 steps, one note each, read by SEQ > PATTERNS) skips hit bits on
  a track that is not Felucca's DRUM; that is upstream's store and touches no sound load.
- **Tests**: gn_wire_test (ASan, UBSan): a user preset of each engine saved from preset 5 with ENV and EDIT 2 moved
  reloads on preset 5 with the same words to the core and the same take; marks 0 and 1 load as they did; a song's
  settled track keeps its pages; Glide, Mode and Detune say why on every engine; on the four kits every black key is a
  drum beside it on a note no white key plays (44 of 44), KIT's eleven ghosts 3 dB or more under the hit and heard;
  Advanced's 27 keys 27 notes. Each of these fails on 81727bd. gn_ux_test FLOW-1 x EW: the first black key's fill is a
  soft KICK (note 4).
- **Size.** Image 565,028 -> 565,172 B (+144 B; max:image 565,180, +8 B headroom; free 16,392 >= the 16,384 floor);
  .data+.bss 82,580 B unchanged; the audio interrupt's target cost unchanged (1,926,653 of 2,019,018). No floor, cap,
  budget line or feature changed. The next change on this line needs its own size work.

## M1, and what upstream now does itself

M1 (branch `gn/m1-trs`, pull request #4, on 0.9-beta) is closed without merging. Upstream 1.0.1 ships
most of it: TRS MIDI in, on by default, with its own overrun handling and counters on the console;
ChanceTheMaker's pull request 8, adapted (pitch bend with the RPN range, sustain, CC 120/121/123, clock
in from USB or TRS); USB MIDI back-pressure (the host is NAKed while the ring is full) and the release
of every MIDI-held note after an overflow. Dropped with M1: all of that code of ours, the 128-packet
USB queue, the boot-guard recovery mode with its boot record, its boot hooks and the safe start, and
the console's `recovery` command. Accepted differences from M1: the mod wheel (CC1) is only a source
of the modulation matrix, not a fixed vibrato; MIDI channels 1-4 play tracks 1-4 and every other
channel the selected track (M1: 1-3 and the drums on 10). Kept from M1: CI, the package with the
released loader, and the name.

## Recovery

There is no recovery code of ours. If a build cannot start, upstream's boot guard sends the FM-1 to the
mask ROM's update mode after two failed starts within 30 s, and
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter) recovers it (upstream's
`BUILDING.md`). A build that starts can always be replaced from the installer page, and 1.0.1's page
can also return the FM-1 to the official V15 firmware after a full backup; it accepts our builds
because their `INFO` starts with `FELUCCA `. Firmware-side recovery waits until it can be developed
on a spare synth.

## Licensing

The code is GPL-3.0-only. Whoever gets our firmware, even one friend, must also get its complete source
under the GPL. Since 1.0.1 (`LICENSING.md`) nothing in the tree is all rights reserved: the icons are the
Fukiai font (MIT), the panel picture (`docs/panel.jpg`) and the Hügelton Sample Pack (the drum sounds
`tools/gen_waves.py` makes) are GPL-3.0-only, the UI font is Inter Tight (OFL), the instrument samples CC0,
the ported DSP MIT and Apache-2.0, the three JieLi SDK files in a package Apache-2.0; keep every licence
notice and `LICENSES/`. What still matters before any public release are the names: "Felucca" and
"Hügelton Instruments" are Hügelton's, so a public release goes by its own name (`GN_NAME`), and the
installer's `FELUCCA ` prefix in `INFO` is only there because 1.0.1's installer page checks it.

## Upstream sync: tags only

`.github/workflows/upstream-sync.yml` runs every day at 13:17 UTC (or by hand from the Actions
tab, with an optional tag). It fetches upstream's tags and takes the newest by version (a suffix
such as `-rc1` sorts before its release). If our branch does not contain it, it merges that tag on
a new branch `sync/upstream-<tag>` and opens a pull request "Merge upstream Felucca <tag>" that
lists the commits, links the release, and asks for the base pins in `firmware.yml` to be moved to
the new tag on that branch before merging. Commits on upstream's `main` between tags are never
proposed. GitHub holds the build of a pull request that GitHub Actions opened until someone approves
it; the job approves it, so the firmware workflow runs on the pull request. If the merge conflicts,
the branch holds upstream's tag as it is and the pull request names the conflicting files. It never
pushes to `main`; a person reviews and merges. Merge sync pull requests with **Create a merge
commit**, never squash or rebase. If upstream's tag changes `.github/workflows`, the job stops and
asks for a merge by hand. The job needs the repository setting Settings > Actions > General >
"Allow GitHub Actions to create and approve pull requests" (on). Scheduled runs use the copy of the
workflow on the default branch (`main`).

## gn/gnomad-1.0: Easy and the Advanced skin together (merged 2026-10-05)

The branch that is installed and tried on the synth. It is gn/easy-1.0 (ea51e02) merged with gn/skin-1.0 (346265b),
both on gn/base-1.0.1 (434dafc). The two branches' hooks sit at different spots, so the firmware sources merged without
conflict; FORK.md and the workflow were joined by hand (every upload step of both kept). Build name: GN_NAME V1 and
six hex digits of the commit. Order in felucca.c: gn_skin.h before gfx.c, gn_hooks.h before seq.c, gn_skin.c after
ui_layer.c, gn_fork.c before main.c. Draw order in ui_draw: frame++, Easy's gn_draw (returns when it drew), upstream's
modal and layer handling, cursor fix, the skin's guarded draw hooks inside upstream's page draw, the one timer tail.
The skin never consumes input; Easy may. Both GN_NAME definitions are the same token. State symbols: Easy's `gn`,
the skin's `gsk`. The canvas tail: Easy's buffers while Easy owns the screen; the skin borrows nothing in phase 1.

## Branding, round G (2026-10-11)

The synth presents as **Gnomad Synth V1**. One reviewed change, text only: no logic, timing, descriptor structure or
loader byte changes.

- **main.c** (3 lines): the start-up splash `GNOMAD` (L face; every glyph is in `L_CHARS`) over `SYNTH V1` (was
  `FELUCCA` / `MULTI-ENGINE SYNTH`); the crash page's title `GNOMAD CRASH` (was `FELUCCA CRASH`).
- **usb.c** (1 line): the app's USB product string (STR2) `Gnomad Synth V1`, a 32-byte descriptor (was `Felucca`,
  16 bytes; the 42-byte manufacturer string already goes out through the same control path, EP0 is 64 bytes). Only the
  app's branch of `#ifdef FELUCCA_LOADER`: the update loader (`loader.c` includes `usb.c`) keeps `Felucca Update` and
  every byte. The manufacturer string (`Hügelton Instruments`, shared with the loader) is unchanged: round G names the
  product only.
- **ui_menu.c** ABOUT: `GNOMAD`, `Gnomad Synth V1`, the version (`FELUCCA_VERSION`, the build's name) and the date,
  and one credit line, `built on Felucca by Leo Kuroshita, GPL-3`. Dropped: Felucca's own credit block (Hügelton
  Instruments / OWN WORK, Leo Kuroshita, CrispyZebra/DRUM/Sample Pack, Fukiai, github.com/hugelton), the
  (C)/Hügelton/GPL/hugelton URL lines, `DRUM + own samples` and the hugelton source line. Kept: the third-party
  notices (Klattsch, DaisySP, Dexed + msfa, Rings, Inter Tight, VSCO-2/VCSL, the voice research), NO WARRANTY, and the
  QR (Felucca's source, the credit line's link; upstream's ABOUT tests require its quiet zone).
- **The gate.** "How the synth starts and installs is upstream's bytes" now runs `.github/ci/gn_protected.py`: every
  protected path keeps the tag's bytes, no file added or removed, except the four lines above, each compared as one
  whole line (newline included) against the exact approved line, same line count, and the USB line only in the app's
  branch of `#ifdef FELUCCA_LOADER`. `tests/gn_protected_test.py` runs first in the step and must fail every altered
  copy: about 6,800 single-byte flips across main.c and usb.c, a byte inside an approved line, a line added, removed or
  swapped, a trailing newline, the new name in the loader's branch, the manufacturer string, one byte in ota.c, and the
  gate's own `main()` on a tree with one stray byte (exit 1); HEAD and upstream pass.
- **The skin's inert test** (`tests/gn_skin_tests.sh`) leaves ABOUT's four scenes (`about`, `about_rec`,
  `about_credits`, `about_end`) out of the byte-identical comparison with the base, and only them: the scene lists
  must be equal and exactly 32 (4 x 8 palettes) are left out; the other 35,712 renders stay byte-identical.
- **Local validation** (generated headers from the a546a8a run's artifact, no firmware built locally):
  upstream's `ui_test` all passed (ABOUT scrolls, QR quiet zone, CREDITS continuation, L-face glyphs);
  `ui_render` 112 screens x 8 palettes, 0 lint findings; `tests/gn_skin_tests.sh` ALL SKIN TESTS PASSED;
  `tests/gn_protected_test.py` ALL PROTECTED-GATE TESTS PASSED; the gate on this tree exit 0.
- **Before installing on a synth** (the port name changes): the companion app finds the synth by a port name containing
  `Felucca`; `tools/fm1_install.py` (protected,
  upstream's) finds running synths by `fm-1|felucca|ota|...` and so needs `--port Gnomad` (or the synth put in update
  mode by hand: the loader is still `Felucca Update`); `tools/fm1_sample_upload.py` looks for `Felucca`. The app must
  accept `Gnomad` first. macOS may keep showing a cached name until the device is removed in Audio MIDI Setup.

## Branding, round V1.6 (2026-10-12)

The app's USB manufacturer string "Hügelton Instruments" is replaced, the ABOUT page's QR code goes to gnomad.cc, and
the synth's face shows GNOMAD, not Felucca. The product is **Gnomad Synth V1**.

- **The app's USB manufacturer is `Gnomad`** (a 14-byte descriptor, ASCII, its length byte counted as the product
  line's is). Upstream writes the manufacturer (`STR1`) once, above `#ifdef FELUCCA_LOADER`, for the loader and the
  app alike, and the app's branch has exactly one line, so there is no fifth line to approve: the one approved app
  line (round G's product line) now carries both strings. It starts with `_Pragma("pop_macro(\"STR1\")")` and defines
  `GN_STR1` "Gnomad" beside `STR2`; felucca.c, before it includes usb.c, saves `STR1 -> GN_STR1` and renames upstream's
  array (`STR1_UPSTREAM`, unused, dropped by the linker), so `get_desc` sends `GN_STR1`; a size check after the include
  fails the build unless it is the 14-byte one. Anything that includes usb.c without felucca.c's lines (the update
  loader, upstream's host tests `hostsim.c`, `uac_test.c`, `midi_uart_test.c`) has nothing to pop: `STR1` stays
  upstream's, every byte. The approved lines stay four (main.c 3, usb.c 1); round G's usb.c line no longer passes.
- **The gate's test** adds: round G's line, a byte of the manufacturer, its length byte, the pop left out or popping
  another name, all fail; every usb.c line but the approved one is upstream's, in place; and the strings as compiled
  (cc on usb.c's descriptor lines): the app `Gnomad` / `Gnomad Synth V1`, the loader the same bytes as upstream's usb.c
  gives it (`Hügelton Instruments` / `Felucca Update`), usb.c without felucca.c's lines builds with upstream's
  manufacturer. The companion app matches ports by product name only: no app change.
- **ABOUT**: the top shows `GNOMAD`, `Gnomad Synth V1`, the version (the build's name) and the date, and the QR, now
  https://gnomad.cc (version 3, correction H, CoreImage's `CIQRCodeGenerator`, which reproduces upstream's matrix for
  its URL bit for bit; decoded back from the renders). Nothing of Felucca there. CREDITS opens with `Includes code from
  Felucca by Leo Kuroshita, GPL-3`, then the third-party notices; `DEXED + MSFA / FM6 PORT` is gone (FM6 is retired;
  the FM credit is the core's `MSFA / GNOMAD FM KERNEL`). Upstream's ABOUT tests pass unchanged (the QR keeps its
  size and quiet zone). The skin's inert test leaves ABOUT's 4 x 8 renders out, as before.
- **Why one Felucca line stays (licence).** GPL-3.0 section 5(d): a covered work with an interactive interface must
  display Appropriate Legal Notices (section 0: a copyright notice, no warranty, that it is GPL-licensed, and how to
  view the licence). The credits page is that notice: the Felucca line names the GPL-3.0 code and its author,
  `LICENSES + SOURCE` points at the GPL-3.0 text and the source (`gnomad.cc`, the ABOUT QR; that page must carry both
  before a release), and `NO WARRANTY` stays. Section 7(b) is not the reason: it covers only additional preservation
  terms upstream attaches, and Felucca attaches none (`LICENSE` is the verbatim GPL-3.0 text; `README.md` and
  `LICENSING.md` add credits and a trademark note, no section 7 terms). The face of the synth is Gnomad's alone.
- **The update loader stays upstream's, blocking.** The workflow's step "The update loader built here is upstream's
  released one" fails the job unless `build/loader/ota.bin` has `RELEASED_LOADER_SHA256`, and the package step
  byte-compares it with the loader in upstream's released package (before, only the report-only reproduce job did).
- **The INFO name stays `FELUCCA GNOMAD V1 <hex>`.** Without the `FELUCCA ` prefix: upstream's installer page
  (`web/index_pkg.html`, also on hugelton.github.io) refuses its complete backup before a return to stock
  (`/^FELUCCA /`); the phone app's `FirmwareName` would read `GNOMAD V1 ...` as the Gnomad family and switch on
  transport, saves-by-itself and Easy-on-device (`FirmwareFeatures`), off today; `.github/ci/gn_release_site.py` and `releases.txt`
  (`<n> FELUCCA ...`), `tests/run_fork_tests.sh` and the workflow's name checks all build or match the prefix.
  The ABOUT page and the console already show the part after it.
- **The milestone is `V1`** (`GN_MILESTONE`, `.github/workflows/firmware.yml`'s env; budgets read `GN_BUDGET_LINE`):
  the build is named `GNOMAD V1 <6 hex>` (ABOUT), `FELUCCA GNOMAD V1 <6 hex>` (INFO), tag `gnomad-v1-<7 hex>`.
