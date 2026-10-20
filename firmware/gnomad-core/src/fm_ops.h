/* Gnomad FM: one sample of the six operators on our G6 routes, driven by
 * the msfa operator kernel (third_party/msfa/adapted/msfa_kernel.h).
 * Shared by src/fm.c (the engine's block loop) and tests/msfa_ref (the
 * bit-exact comparison against upstream msfa's op-major FmCore), so the
 * comparison exercises the engine's own routing code.
 *
 * Requires gc_internal.h, the FM tables and msfa_kernel.h. */
#ifndef GC_FM_OPS_H
#define GC_FM_OPS_H

/* g[j]: the operator's running gain (msfa FmOpKernel `gain`), stepped by
 * dg[j] before use; on: bit j set = operator computed this block (msfa
 * FmCore kLevelThresh); fbm: Q32 feedback multiplier for s->fb_op. A source
 * pushes its output into its <=2 later targets (sinks 6 and 7 absorb absent
 * targets), a carrier into the returned Q24 sum. A skipped operator still
 * advances its phase; the feedback history moves only when the feedback
 * operator was computed (msfa fb_buf). */
static inline int32_t fm_ops_sample(gc_fm_state *s, int32_t *g, const int32_t *dg,
                                    unsigned on, uint64_t fbm) {
  int32_t acc[8];
  int32_t csum = 0, yfb = 0;
  acc[0] = acc[1] = acc[2] = acc[3] = acc[4] = acc[5] = acc[6] = acc[7] = 0;
  acc[s->fb_op] = msfa_fb_in(s->fb0, s->fb1, fbm);
#define FM_OP(J)                                                              \
  do {                                                                        \
    gc_fm_op *o = &s->op[J];                                                  \
    if (on & (1u << (J))) {                                                   \
      int32_t y;                                                              \
      g[J] += dg[J];                                                          \
      y = msfa_mulq24(msfa_sin_lookup(o->phase + (uint32_t)acc[J]), g[J]);    \
      acc[o->t0] += y;                                                        \
      acc[o->t1] += y;                                                        \
      if (o->car) {                                                           \
        csum += y;                                                            \
      }                                                                       \
      if ((J) == s->fb_op) {                                                  \
        yfb = y;                                                              \
      }                                                                       \
    }                                                                         \
    o->phase += o->inc;                                                       \
  } while (0)
  FM_OP(0);
  FM_OP(1);
  FM_OP(2);
  FM_OP(3);
  FM_OP(4);
  FM_OP(5);
#undef FM_OP
  if (on & (1u << s->fb_op)) {
    s->fb0 = s->fb1;
    s->fb1 = yfb;
  }
  return csum;
}

#endif /* GC_FM_OPS_H */
