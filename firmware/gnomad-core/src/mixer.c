/* Mixer and master output chain (SPEC/20-effects.md "Mix, controls and
 * safety"): voices in slot order -> per-track sum -> track gain -> R(total,4);
 * sends likewise; wet return from the effects lane; then master filter ->
 * drive -> DC -> master level -> soft ceiling. */
#include "gc_internal.h"
#include "tables.h"

void gc_master_clear(gc_core *c) {
  c->msvf[0] = 0;
  c->msvf[1] = 0;
  c->dc_x = 0;
  c->dc_y = 0;
}

static void step_ramp(gc_ramp *r) {
  if (r->k < 64u) {
    r->k++;
    r->cur = (uint16_t)(r->start +
                        gc_Rp2((int64_t)(r->target - r->start) * r->k, 6u));
  }
}

/* Soft ceiling C(x) of SPEC/20, K=16384, H=29204, memoryless. */
int32_t gc_ceiling(int32_t x, uint32_t *clips) {
  const int32_t K = 16384, H = 29204;
  int32_t a = x < 0 ? -x : x, y;
  if (a <= K) {
    return x;
  }
  (*clips)++;
  {
    /* R(n,d)=floor((n+floor(d/2))/d), n=(H-K)(a-K) < 2^32, d < 2^18, q <
     * 2^14; scaling numerator-plus-half and d by 2^17 keeps the quotient and
     * meets gc_udiv_nr's domain (d*2^17 >= 12821*2^17 > 2^30). */
    uint64_t d = (uint64_t)(uint32_t)((a - K) + (H - K));
    uint64_t n = (uint64_t)(uint32_t)(H - K) * (uint32_t)(a - K) + (d >> 1);
    y = K + (int32_t)gc_udiv_nr(n << 17, d << 17);
  }
  if (y > H) {
    y = H;
  }
  return x < 0 ? -y : y;
}

int16_t gc_mix_frame(gc_core *c, uint32_t *sat, uint32_t *clips) {
  /* Track sums: at most eight S15 voices, so every sum is below 2^19 and
   * every R(.,4) below is a 32-bit operation. */
  int32_t dry[GC_TRACKS], s0[GC_TRACKS], s1[GC_TRACKS], s2[GC_TRACKS];
  int32_t total = 0, b0 = 0, b1 = 0, b2 = 0;
  int32_t x, wet, b;
  unsigned i, t;
  uint16_t dctl[6];

  for (t = 0; t < GC_TRACKS; t++) {
    dry[t] = s0[t] = s1[t] = s2[t] = 0;
  }
  if (c->ramp_busy) { /* exact: idle ramps (k == 64) never change, so only
                       * the tracks (and master) flagged as moving step */
    uint8_t busy = c->ramp_busy, still = 0;
    for (t = 0; t < GC_TRACKS; t++) {
      uint8_t mv = 0;
      if (!(busy & (1u << t))) {
        continue;
      }
      for (i = 0; i < GC_TR_RAMPS; i++) {
        step_ramp(&c->tramp[t][i]);
        mv |= (uint8_t)(c->tramp[t][i].k < 64u);
      }
      for (i = 0; i < 6u; i++) {
        c->tctl[t][i] = c->tramp[t][GC_TR_LEVEL + i].cur;
      }
      still |= (uint8_t)(mv << t);
    }
    if (busy & GC_RAMPS_MASTER) {
      uint8_t mv = 0;
      for (i = 0; i < GC_MR_RAMPS; i++) {
        step_ramp(&c->mramp[i]);
        mv |= (uint8_t)(c->mramp[i].k < 64u);
      }
      still |= (uint8_t)(mv << 4);
    }
    c->ramp_busy = still;
  }

  for (i = 0; i < GC_VOICES; i++) {
    gc_voice *v = &c->voice[i];
    const uint16_t *ctl;
    int32_t y;
    uint32_t ay, e0, e1, e2;
    unsigned s;
    if (!(v->h.flags & GC_VF_ACTIVE)) {
      continue;
    }
    t = v->h.track;
    if (v->h.flags & GC_VF_ATTACHED) {
      ctl = c->tctl[t];
    } else {
      dctl[0] = v->p[0];
      for (s = 1; s < 6u; s++) {
        dctl[s] = v->p[4u + s];
      }
      ctl = dctl;
    }
    switch (v->h.engine) {
    case GC_DRUMS:
      y = gc_drums_render(v, ctl, sat);
      break;
    case GC_FM:
      y = gc_fm_render(v, ctl, sat);
      break;
    case GC_GNOME:
      y = gc_gnome_render(v, ctl, sat);
      break;
    case GC_SAMPLE:
      y = gc_sample_render(v, ctl, sat);
      break;
    default:
      y = gc_va_render(v, ctl, sat);
      break;
    }
    if (v->h.flags & GC_VF_XFADE) {
      /* Steal transition: R(old*(31-k)+new*(k+1),32), k=0..31 (S15 terms). */
      y = gc_Rp2s(v->h.xf_old * (31 - (int32_t)v->h.xf_k) + y * ((int32_t)v->h.xf_k + 1),
                  5u);
      if (++v->h.xf_k >= 32u) {
        v->h.flags &= (uint8_t)~GC_VF_XFADE;
      }
    }
    v->h.prev_sample = (int16_t)y;
    dry[t] += y;
    if (v->h.engine == GC_DRUMS) {
      e0 = gc_drums_send(v, 0, ctl[3]);
      e1 = gc_drums_send(v, 1, ctl[4]);
      e2 = gc_drums_send(v, 2, ctl[5]);
    } else {
      e0 = ctl[3];
      e1 = ctl[4];
      e2 = ctl[5];
    }
    /* Sends G(y,send) on the S15 magnitude, sign once (exact). */
    ay = y < 0 ? (uint32_t)0 - (uint32_t)y : (uint32_t)y;
    e0 = gc_Gu(ay, e0);
    e1 = gc_Gu(ay, e1);
    e2 = gc_Gu(ay, e2);
    if (y < 0) {
      s0[t] -= (int32_t)e0;
      s1[t] -= (int32_t)e1;
      s2[t] -= (int32_t)e2;
    } else {
      s0[t] += (int32_t)e0;
      s1[t] += (int32_t)e1;
      s2[t] += (int32_t)e2;
    }
  }

  for (t = 0; t < GC_TRACKS; t++) { /* G(0,g)=0: empty sums skip the gain */
    uint16_t g = c->tramp[t][GC_TR_GAIN].cur;
    if (dry[t]) total += gc_G(dry[t], g);
    if (s0[t]) b0 += gc_G(s0[t], g);
    if (s1[t]) b1 += gc_G(s1[t], g);
    if (s2[t]) b2 += gc_G(s2[t], g);
  }
  wet = gc_fx_process(c, gc_S15(gc_Rp2s(b0, 2u)), gc_S15(gc_Rp2s(b1, 2u)),
                      gc_S15(gc_Rp2s(b2, 2u)), sat);
  if (wet != 0) {
    wet = gc_G(wet, c->mramp[GC_MR_WET].cur);
  }
  x = gc_S18((int64_t)gc_Rp2s(total, 2u) + wet, sat);

  /* Master low-pass: 8000 Hz is an exact bypass that zeros histories.
   * Coefficients are cached on the integer cutoff/resonance steps. */
  if (c->mstep[GC_MS_CUTOFF] >= 8000u) {
    c->msvf[0] = 0;
    c->msvf[1] = 0;
  } else {
    if (c->mstep[GC_MS_CUTOFF] != c->mkc || c->mstep[GC_MS_RES] != c->mkr) {
      c->mkc = c->mstep[GC_MS_CUTOFF];
      c->mkr = c->mstep[GC_MS_RES];
      c->mF = gc_svf_f(c->mkc);
      c->mQ = gc_svf_q(c->mkr);
    }
    x = gc_svf_run(c->msvf, x, c->mF, c->mQ, sat);
  }

  /* Drive: y=R(x*(65535+4d)*32768, 65535*32768+4d|x|); d=0 returns x. */
  b = c->mramp[GC_MR_DRIVE].cur;
  if (b != 0 && x != 0) { /* drive maps 0 to 0 */
    /* R(n,D) = sign(x)*floor((|n|+floor(D/2))/D) with |n| < 2^52, D in
     * [2^31-2^15, 2^37) and |y| <= 5|x| < 2^21: the exact Newton-estimate
     * divide (gc_udiv_nr), no divide instruction. */
    uint32_t ax = x < 0 ? (uint32_t)0 - (uint32_t)x : (uint32_t)x;
    uint64_t dd = 65535ULL * 32768u + 4ULL * (uint32_t)b * ax;
    uint32_t q = gc_udiv_nr((uint64_t)ax * ((65535u + 4u * (uint32_t)b) * 32768ULL) + (dd >> 1),
                            dd);
    x = x < 0 ? -(int32_t)q : (int32_t)q;
  }

  /* DC blocker (SPEC/20, clarified by CHANGES-1.1.2 C1). The output history
   * keeps 16 extra fractional bits: Y=(x-prev_x)*2^16+T(prev_Y*c,2^30) with
   * c=GC_DC_LEAK=R(995*2^30,1000) and T truncating toward zero, then
   * y=S18(R(Y,2^16)). T(|Y|*c,2^30)=|Y|-ceil(|Y|*(2^30-c)/2^30) keeps the
   * product inside int64. Under R on the 15-bit state every |y|<=100 was a
   * fixed point, so a residual offset never decayed. */
  if (x != 0 || c->dc_x != 0 || c->dc_y != 0) { /* else Y = 0: y = 0, state stays 0 */
    const uint64_t dl = (uint64_t)1073741824u - GC_DC_LEAK;
    uint64_t ym = gc_mag64(c->dc_y);
    uint64_t lm = ym - ((ym * dl + 1073741823u) >> 30);
    int64_t Y = ((int64_t)x - c->dc_x) * 65536 +
                (c->dc_y < 0 ? -(int64_t)lm : (int64_t)lm);
    int32_t y;
    if (Y > (int64_t)262143 * 65536) {
      Y = (int64_t)262143 * 65536;
      (*sat)++;
    } else if (Y < (int64_t)-262144 * 65536) {
      Y = (int64_t)-262144 * 65536;
      (*sat)++;
    }
    y = gc_S18(gc_Rp2(Y, 16u), sat);
    c->dc_x = x;
    c->dc_y = Y;
    x = y;
  }

  x = gc_G(x, c->mramp[GC_MR_LEVEL].cur);
  return (int16_t)gc_ceiling(x, clips);
}
