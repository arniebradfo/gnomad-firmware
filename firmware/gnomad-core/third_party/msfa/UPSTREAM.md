# msfa upstream snapshot

- Project: music-synthesizer-for-android ("msfa"), Google.
- Repository: https://github.com/google/music-synthesizer-for-android
- Pinned commit: `f67d41d313b7dc85f6fb99e79e515cc9d208cfff` (2017-09-12, "Make Mac build generate audio"), the default branch head when cloned on 2026-10-11.
- Licence: Apache License 2.0 (upstream `COPYING`, copied unmodified to `LICENSE` here). Upstream has no NOTICE file; the per-file headers read "Copyright 2012 Google Inc." and "Copyright 2013 Google Inc." (exp2.cc).
- Reference snapshot: `upstream/` holds the files below, byte-identical to `app/src/main/jni/<file>` at the pinned commit, read-only. They are NOT compiled into the Gnomad core; only the host test `make msfa-ref` builds them (scalar path, NEON disabled) as the bit-exactness reference. `upstream/` exists only in the gnomad-core repository as the host-test reference and is intentionally not vendored into the firmware (its `firmware/gnomad-core/sync-core.sh` copies this directory's top-level files and `adapted/` only).

| File | SHA-256 |
|---|---|
| exp2.cc | fe7dc02d600fcaf198ca32b10edfb28ff695d107b401c3fa92a7ac73d2e9096f |
| exp2.h | 23aa480e221e5d1f928a5ccab654a9c01a80eb0b610fd66d9364af09d18cdda8 |
| fm_op_kernel.cc | e91aa1e067bf67c799871344eb1f3cdd92c2c0e4797b2efc2403ac7bb7113af5 |
| fm_op_kernel.h | 62bfcbbccf445504dd195a6beb05179b9e03235e8e4b19e661b761ddc0e80603 |
| sin.cc | 06ac6f168f6e63f73e6195bbd4a1a65713f01594aeccef469f9c36e9e22e1d11 |
| sin.h | 77486d3ac19d53514a8d26cd2164374ceceab9b15f8d98f876d4db7647321cc2 |
| synth.h | 96f1881020f67e12e3d8ac5d79fc546cab9407605d5274d58ba3b4576b8aaf73 |

Removed from the vendored set on 2026-10-11 (a review): `fm_core.h/.cc` (it carries upstream's 32-entry DX algorithm table), `env.h/.cc` (it carries upstream's output-level table) and `aligned_buf.h` (only fm_core.h used it). PROVENANCE rule 1 admits no upstream tables, not even as reference copies. They were read for the port and compared against in commit e6a93b9 (0 mismatches); the parity test now drives upstream FmOpKernel op-major itself and checks the Env stepping law against a transcription. Not vendored and not read for the port: `dx7note.*`, `patch.*`, `freqlut.*`, `lfo.*`, `pitchenv.*`, `log2.*`, `synth_unit.*`, the NEON assembly, the Android/Java glue, and every `.syx` cartridge or other patch data (`res/raw/*`). `dx7note.cc` was opened once, read-only, to confirm how upstream seeds the per-block gains (`gain[0]=gain[1]`, `gain[1]=0` at note-on) and its feedback shift `8 - feedback`; nothing from it is in the build.

The Gnomad engine routes with its own eight G6 graphs (SPEC/11), never with upstream's DX7 algorithm table, which is not in this repository.
