# Gnomad changes to msfa (Apache-2.0 section 4(b) change record)

Upstream: commit `f67d41d313b7dc85f6fb99e79e515cc9d208cfff`, files in `upstream/` (unmodified, read-only; fm_core and env are not vendored, see UPSTREAM.md).

| Gnomad file | Derived from | Change |
|---|---|---|
| `adapted/msfa_kernel.h` `msfa_sin_lookup` | `sin.h` `Sin::lookup` (SIN_DELTA, 10-bit) | C99; uint32 phase; floor(dy*lowbits/2^14) by a biased unsigned shift (dy*lowbits < 2^31) instead of an int64 arithmetic shift. Identical output for every phase (tested over all 2^24). |
| `adapted/msfa_kernel.h` `msfa_exp2_lookup` | `exp2.h` `Exp2::lookup` | C99; y-only table, dy formed at lookup (last dy = 2^31 - y); floor(x/2^24) by a bias; unsigned final shift (y is nonnegative); a final shift of 32 or more (x below -25 doublings, undefined in upstream's int shift) returns 0, the exact floor. Identical over [-16, 6) doublings. |
| `adapted/msfa_kernel.h` `msfa_dgain`, `msfa_mulq24` | `fm_op_kernel.cc` `FmOpKernel::compute*` | The gain step `(gain2-gain1+N/2)>>LG_N` and the Q24 product `(y*gain)>>24` as exact floors without signed shifts; LG_N is a parameter. |
| `adapted/msfa_kernel.h` `msfa_fb_in` | `fm_op_kernel.cc` `compute_fb` | `(y0+y)>>(shift+1)` generalised to `floor((y0+y)*fbm/2^33)`; fbm = 2^(32-shift) is upstream. |
| `adapted/msfa_kernel.h` `msfa_env_step` | `env.cc` `Env::getsample` | The stepping law only (jump to 1716<<16, `+= (17-floor(level))*inc` rising, `-= inc` falling, clamp to target and report arrival). Targets and rates come from the caller. |
| `src/fm_ops.h` (Gnomad file) | `fm_core.cc` `FmCore::compute` (structure) | Sample-major operator loop on Gnomad's G6 routes with upstream's kLevelThresh skip rule (1120) and fb_buf update rule; no upstream algorithm table. |
| `tools/gen_tables.py` msfa_sin / msfa_exp2 (Gnomad file) | `sin.cc` `Sin::init`, `exp2.cc` `Exp2::init` | Recipes re-implemented offline (certified cos/sin seeds for the integer rotation; the double nearest 2^(1/1024) for the exp2 recurrence, then IEEE double steps as upstream). |
| `tests/msfa_ref/*` (Gnomad files) | — | Host test linking unmodified upstream sources against the adaptation. |
