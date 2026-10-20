/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): drawing for the fork's screens, in the design boards' own terms. The boards
 * draw the synth's 240 x 240 screen as SVG: filled shapes with an ink keyline, round-capped
 * monoline strokes, rounded type. This draws the same things, anti-aliased, into gfx.c's canvas
 * (cv_px), a rectangle of the screen at a time (gx_begin .. gx_end), so no screen-sized buffer
 * is needed:
 *   gx_rect, gx_rrect, gx_circle   exact coverage (area, or distance to the edge)
 *   gx_path                        SVG path data as the boards write it (M L H V Q T C S A Z,
 *                                  absolute and relative; elliptical arcs without rotation),
 *                                  placed by a translate + scale (gx_xf_t), filled (non-zero,
 *                                  exact area per pixel, a row at a time) and / or stroked
 *                                  (round caps and joins: the union of the segments' capsules)
 *   gx_text                        the fork's stroke type (gn_type.c) drawn with gx_path
 * Integers only, as everywhere in the firmware. Coordinates are in 1/64 px (Q6); Q(v) turns a
 * board number into that at compile time. Paint order is the SVG one: fill, then stroke, each
 * shape over the ones before. A shape is clipped to the canvas, so a screen is drawn by calling
 * the same scene for each strip of it. */
#define GX_Q 6
#define Q(v) ((int32_t)((v) * 64.0 + ((v) < 0 ? -0.5 : 0.5)))   /* a constant: folded at compile time */
#define QI(v) ((int32_t)(v) << GX_Q)                                /* a run-time integer: no floating point (the chip links no soft-float) */
#define GX_NONE 65536u                                          /* no fill, no stroke: no 16-bit colour */

#define GX_PAPER RGB(0xF9, 0xF8, 0xF4)           /* shiro: paper, raised */
#define GX_GROUND RGB(0xEE, 0xED, 0xE8)          /* kami: the ground */
#define GX_WELL RGB(0xE3, 0xE2, 0xDC)            /* kumo: wells */
#define GX_INK RGB(0x17, 0x18, 0x1B)             /* sumi */
#define GX_INK2 RGB(0x5B, 0x5D, 0x62)            /* hai: second ink */
#define GX_BLUE RGB(0x23, 0x52, 0xE6)            /* knob 1 */
#define GX_GREEN RGB(0x0B, 0x7D, 0x4F)           /* knob 2 */
#define GX_YELLOW RGB(0xF4, 0xB4, 0x00)          /* knob 3 */
#define GX_RED RGB(0xF2, 0x62, 0x2A)             /* knob 4, vermilion */

typedef struct {
    int32_t tx, ty;              /* 1/64 px */
    int32_t s;                   /* scale, 4096 = 1 */
} gx_xf_t;
#define GX_XF(x, y, sc) {Q(x), Q(y), (int32_t)((sc) * 4096.0 + 0.5)}   /* constants only */
#define GX_S14 2389                               /* a 24-unit icon drawn 14 px wide: scale 14 / 24 */
static const gx_xf_t GX_ID = {0, 0, 4096};

#define GX_MW 240u                                /* the largest canvas: 240 x 64 */
#define GX_MH 64u
#define GX_SEGS 512u
/* Easy's scratch lives in the tail of gfx.c's canvas, cv_px (59,520 B, uint16_t[CV_MAX]): its first
 * GX_MW x GX_MH pixels (30,720 B) are the strip being drawn (gx_begin: cv_begin), then the edge list
 * (6,144 B), the fill's accumulation row (968 B) and the stroke mask (15,360 B); the last 6,328 B are
 * not touched. Felucca keeps nothing in cv_px from one draw to the next (each one starts with
 * cv_begin), and Easy keeps nothing of its own there between two calls of gn_draw: the edge list
 * lives for one shape, the row for one row of a fill, the mask for one stroke and is cleared at
 * every gx_begin, so whatever another draw left there (the update countdown or screen, drawn while
 * Easy is on) is never read. Accessed only as uint16_t, the canvas's own type, and as bytes
 * (FORK.md, Easy: RAM). */
#define GX_TAIL_SEG (GX_MW * GX_MH)                     /* cv_px index of the edge list: 6 uint16_t each */
#define GX_TAIL_ACC (GX_TAIL_SEG + GX_SEGS * 6u)        /* .. of the row: an int32_t in 2 uint16_t each */
#define GX_TAIL_MASK (GX_TAIL_ACC + (GX_MW + 2u) * 2u)  /* .. of the mask: a byte a pixel */
#define GX_TAIL_END (GX_TAIL_MASK + GX_MW * GX_MH / 2u)
_Static_assert(GX_TAIL_END <= CV_MAX, "Easy's scratch fits in gfx.c's canvas");
_Static_assert(GX_TAIL_END * 2u == 53192u && CV_MAX * 2u - GX_TAIL_END * 2u == 6328u, "FORK.md's numbers");
#define gx_mask ((uint8_t *)&cv_px[GX_TAIL_MASK])       /* stroke coverage, 0..255 */
/* segment k of the shape being built: x0 y0 x1 y1 (canvas, 1/64 px), dx / dy (fill, Q12) */
static int32_t gx_sg(uint32_t k, uint32_t f) { return (int16_t)cv_px[GX_TAIL_SEG + k * 6u + f]; }
static void gx_sg_put(uint32_t k, uint32_t f, int32_t v) { cv_px[GX_TAIL_SEG + k * 6u + f] = (uint16_t)v; }
static int32_t gx_sg_dxdy(uint32_t k)
{
    const uint16_t *q = &cv_px[GX_TAIL_SEG + k * 6u + 4u];
    return (int32_t)((uint32_t)q[0] | (uint32_t)q[1] << 16);
}
static void gx_sg_dxdy_put(uint32_t k, int32_t v)
{
    cv_px[GX_TAIL_SEG + k * 6u + 4u] = (uint16_t)(uint32_t)v;
    cv_px[GX_TAIL_SEG + k * 6u + 5u] = (uint16_t)((uint32_t)v >> 16);
}
enum { GXS_X0, GXS_Y0, GXS_X1, GXS_Y1 };
static int32_t gx_acc_get(int32_t i)
{
    const uint16_t *q = &cv_px[GX_TAIL_ACC + (uint32_t)i * 2u];
    return (int32_t)((uint32_t)q[0] | (uint32_t)q[1] << 16);
}
static void gx_acc_put(int32_t i, int32_t v)
{
    cv_px[GX_TAIL_ACC + (uint32_t)i * 2u] = (uint16_t)(uint32_t)v;
    cv_px[GX_TAIL_ACC + (uint32_t)i * 2u + 1u] = (uint16_t)((uint32_t)v >> 16);
}
static struct {
    int32_t x0, y0, w, h;        /* the canvas: a screen rectangle, px */
    int32_t mx0, my0, mx1, my1;  /* where the mask holds something */
    uint32_t nseg, lost;         /* segments of the shape being built; dropped (too many) */
    int32_t sx, sy;              /* the shape: start of the subpath (canvas, 1/64 px) */
    int32_t px, py;              /* the shape: last point */
    uint8_t open;                /* a subpath has a point */
    uint32_t trim;               /* 1..999: a stroke draws only that much (1/1000) of its length; 0 all */
} gx;

static void gx_begin(int32_t x0, int32_t y0, int32_t w, int32_t h, uint16_t bg)
{
    w = w < 1 ? 1 : w > (int32_t)GX_MW ? (int32_t)GX_MW : w;   /* the strip: 1 x 1 .. GX_MW x GX_MH */
    h = h < 1 ? 1 : h > (int32_t)GX_MH ? (int32_t)GX_MH : h;
    cv_oy = 0;
    cv_begin((uint32_t)w, (uint32_t)h, bg);       /* gfx.c: waits for the last blit to finish */
    gx.x0 = x0;
    gx.y0 = y0;
    gx.w = w;
    gx.h = h;
    gx.mx0 = w;
    gx.my0 = h;
    gx.mx1 = gx.my1 = 0;
    gx.nseg = 0;
    gx.open = 0;
    memset(gx_mask, 0, (uint32_t)w * (uint32_t)h);   /* (another draw may have used the canvas's tail) */
}

static void gx_end(void) { cv_blit((uint32_t)gx.x0, (uint32_t)gx.y0); }

/* a screen box (px) touches the canvas: a scene skips the shapes that cannot (their path data is
 * not even read) */
static int gx_visible(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    return x1 > gx.x0 && x0 < gx.x0 + gx.w && y1 > gx.y0 && y0 < gx.y0 + gx.h;
}

/* ------------------------------------------------------------ pixels --- */
static uint32_t gx_div255(uint32_t v) { return (v + 128u + ((v + 128u) >> 8)) >> 8; }

static void gx_blend(int32_t x, int32_t y, uint32_t c, uint32_t a)   /* canvas pixel, coverage 0..255 */
{
    uint16_t *p = &cv_px[(uint32_t)y * (uint32_t)gx.w + (uint32_t)x];
    uint32_t d, r, g, b;
    if (a >= 255u) {
        *p = swap16(c);
        return;
    }
    d = swap16(*p);
    r = gx_div255(((c >> 11) & 31u) * a + ((d >> 11) & 31u) * (255u - a));
    g = gx_div255(((c >> 5) & 63u) * a + ((d >> 5) & 63u) * (255u - a));
    b = gx_div255((c & 31u) * a + (d & 31u) * (255u - a));
    *p = swap16(r << 11 | g << 5 | b);
}

static uint32_t gx_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}

static uint32_t gx_isqrt64(uint64_t v)
{
    uint64_t r = 0, b = (uint64_t)1 << 62;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return (uint32_t)r;
}

/* canvas pixel box of a screen box in 1/64 px, clipped; 0 = nothing on the canvas */
static int gx_box(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t *b)
{
    b[0] = (x0 >> GX_Q) - gx.x0;
    b[1] = (y0 >> GX_Q) - gx.y0;
    b[2] = ((x1 + 63) >> GX_Q) - gx.x0;
    b[3] = ((y1 + 63) >> GX_Q) - gx.y0;
    if (b[0] < 0)
        b[0] = 0;
    if (b[1] < 0)
        b[1] = 0;
    if (b[2] > gx.w)
        b[2] = gx.w;
    if (b[3] > gx.h)
        b[3] = gx.h;
    return b[0] < b[2] && b[1] < b[3];
}

static uint32_t gx_cov(int32_t d)                 /* distance inside the edge (1/64 px) -> coverage */
{
    d = (d + 32) * 4;
    return d <= 0 ? 0u : d >= 255 ? 255u : (uint32_t)d;
}

/* --------------------------------------------- rectangles and circles --- */
/* x, y, w, h, r: screen, 1/64 px. A plain rectangle is its exact area per pixel */
static void gx_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c)
{
    int32_t b[4], px, py;
    if (c == GX_NONE || !gx_box(x, y, x + w, y + h, b))
        return;
    for (py = b[1]; py < b[3]; py++) {
        int32_t sy = (py + gx.y0) << GX_Q, oy = (y + h < sy + 64 ? y + h : sy + 64) - (y > sy ? y : sy);
        for (px = b[0]; px < b[2]; px++) {
            int32_t sx = (px + gx.x0) << GX_Q, ox = (x + w < sx + 64 ? x + w : sx + 64) - (x > sx ? x : sx);
            if (ox > 0 && oy > 0)
                gx_blend(px, py, c, (uint32_t)(ox * oy) >> 4 > 255u ? 255u : (uint32_t)(ox * oy) >> 4);
        }
    }
}

/* rounded rectangle by its distance field: fill, then a stroke of width sw centred on the edge */
static void gx_rrect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t fill, uint32_t stroke, int32_t sw)
{
    int32_t b[4], px, py, hx = w / 2, hy = h / 2, cx = x + hx, cy = y + hy, m = stroke != GX_NONE ? sw / 2 + 64 : 64;
    if (r > hx)
        r = hx;
    if (r > hy)
        r = hy;
    if (!gx_box(x - m, y - m, x + w + m, y + h + m, b))
        return;
    for (py = b[1]; py < b[3]; py++)
        for (px = b[0]; px < b[2]; px++) {
            int32_t qx = (((px + gx.x0) << GX_Q) + 32) - cx, qy = (((py + gx.y0) << GX_Q) + 32) - cy, d;
            qx = (qx < 0 ? -qx : qx) - (hx - r);
            qy = (qy < 0 ? -qy : qy) - (hy - r);
            if (qx > 0 && qy > 0)
                d = (int32_t)gx_isqrt((uint32_t)(qx * qx + qy * qy)) - r;
            else
                d = (qx > qy ? qx : qy) - r;
            if (fill != GX_NONE && d < 32)
                gx_blend(px, py, fill, gx_cov(-d));
            if (stroke != GX_NONE)
                gx_blend(px, py, stroke, gx_cov(sw / 2 - (d < 0 ? -d : d)));
        }
}

static void gx_circle_s(int32_t cx, int32_t cy, int32_t r, uint32_t fill, uint32_t stroke, int32_t sw)
{
    int32_t b[4], px, py, m = r + (stroke != GX_NONE ? sw / 2 : 0) + 64;
    if (!gx_box(cx - m, cy - m, cx + m, cy + m, b))
        return;
    for (py = b[1]; py < b[3]; py++)
        for (px = b[0]; px < b[2]; px++) {
            int32_t dx = (((px + gx.x0) << GX_Q) + 32) - cx, dy = (((py + gx.y0) << GX_Q) + 32) - cy;
            int32_t d = (int32_t)gx_isqrt((uint32_t)(dx * dx + dy * dy)) - r;
            if (fill != GX_NONE && d < 32)
                gx_blend(px, py, fill, gx_cov(-d));
            if (stroke != GX_NONE)
                gx_blend(px, py, stroke, gx_cov(sw / 2 - (d < 0 ? -d : d)));
        }
}

/* a circle in a transform's local units (the boards' <circle> inside a <g transform>) */
static void gx_circle(const gx_xf_t *t, int32_t cx, int32_t cy, int32_t r, uint32_t fill, uint32_t stroke, int32_t sw)
{
    gx_circle_s(t->tx + (int32_t)(((int64_t)cx * t->s) >> 12), t->ty + (int32_t)(((int64_t)cy * t->s) >> 12),
                (int32_t)(((int64_t)r * t->s) >> 12), fill, stroke, (int32_t)(((int64_t)sw * t->s) >> 12));
}

/* ----------------------------------------------------- paths: building --- */
static void gx_seg_add(int32_t x0, int32_t y0, int32_t x1, int32_t y1)   /* canvas, 1/64 px */
{
    if (gx.nseg >= GX_SEGS) {
        gx.lost++;
        return;
    }
    gx_sg_put(gx.nseg, GXS_X0, clamp(x0, -32000, 32000));
    gx_sg_put(gx.nseg, GXS_Y0, clamp(y0, -32000, 32000));
    gx_sg_put(gx.nseg, GXS_X1, clamp(x1, -32000, 32000));
    gx_sg_put(gx.nseg, GXS_Y1, clamp(y1, -32000, 32000));
    gx.nseg++;
}

static void gx_to(int32_t x, int32_t y)          /* line to a canvas point */
{
    if (gx.open)
        gx_seg_add(gx.px, gx.py, x, y);
    else {
        gx.sx = x;
        gx.sy = y;
        gx.open = 1;
    }
    gx.px = x;
    gx.py = y;
}

static void gx_sub_end(int close)                /* the subpath ends; close: back to its start */
{
    if (gx.open && close && (gx.px != gx.sx || gx.py != gx.sy))
        gx_seg_add(gx.px, gx.py, gx.sx, gx.sy);
    gx.open = 0;
}

/* a local point, through the transform, to canvas 1/64 px */
static int32_t gx_X(const gx_xf_t *t, int32_t x) { return t->tx + (int32_t)(((int64_t)x * t->s) >> 12) - (gx.x0 << GX_Q); }
static int32_t gx_Y(const gx_xf_t *t, int32_t y) { return t->ty + (int32_t)(((int64_t)y * t->s) >> 12) - (gx.y0 << GX_Q); }

static int32_t gx_span(int32_t a, int32_t b, int32_t c, int32_t d)   /* rough length of a control polygon (local) */
{
    int32_t l = (b > a ? b - a : a - b) + (d > c ? d - c : c - d);
    return l;
}

/* curves: n straight pieces, more for longer ones (on the screen, after the scale) */
static uint32_t gx_pieces(const gx_xf_t *t, int32_t len, uint32_t lo, uint32_t hi)
{
    uint32_t n = 1u + (uint32_t)(((int64_t)len * t->s) >> 12) / 96u;   /* one per 1.5 px */
    return n < lo ? lo : n > hi ? hi : n;
}

static uint32_t gx_atan2(int32_t y, int32_t x)   /* the angle of (x, y), a full turn = 2^32 (CORDIC) */
{
    static const uint32_t AT[16] = {536870912u, 316933406u, 167458907u, 85004756u, 42667331u, 21354465u,
                                    10679838u, 5340245u, 2670163u, 1335087u, 667544u, 333772u, 166886u,
                                    83443u, 41722u, 20861u};
    uint32_t a = 0, i;
    int32_t t;
    if (!x && !y)
        return 0;
    if (x < 0) {
        x = -x;
        y = -y;
        a = 0x80000000u;
    }
    while ((x < 0x4000000 && x > -0x4000000) && (y < 0x4000000 && y > -0x4000000)) {
        x *= 2;                                       /* (a left shift of a negative y is undefined C) */
        y *= 2;
    }
    for (i = 0; i < 16u; i++) {
        if (y > 0) {
            t = x + (y >> i);
            y -= x >> i;
            x = t;
            a += AT[i];
        } else {
            t = x - (y >> i);
            y += x >> i;
            x = t;
            a -= AT[i];
        }
    }
    return a;
}

/* SVG elliptical arc (no x-axis rotation) from the current local point (x1, y1) to (x2, y2):
 * centre from the endpoints (radii grown if too small), then points along it. Divisions are
 * 32-bit (the synth's link has no 64-bit division); the boards' numbers are far inside the
 * ranges that allows, and anything outside them is drawn as a line */
static void gx_arc(const gx_xf_t *t, int32_t x1, int32_t y1, int32_t rx, int32_t ry, int large, int sweep,
                   int32_t x2, int32_t y2)
{
    int32_t X1, X2, dx, dy, r, d, h, cx, cy, n, i, ux, uy, span24;
    uint32_t a1, a2, raw, len;
    int64_t d2, hh;
    rx = rx < 0 ? -rx : rx;
    ry = ry < 0 ? -ry : ry;
    if (!rx || !ry || rx > 16000 || ry > 16000 || x1 > 32000 || x1 < -32000 || x2 > 32000 || x2 < -32000) {
        gx_to(gx_X(t, x2), gx_Y(t, y2));
        return;
    }
    X1 = rx == ry ? x1 : x1 * ry / rx;             /* x scaled: the ellipse becomes a circle of radius ry */
    X2 = rx == ry ? x2 : x2 * ry / rx;
    dx = X2 - X1;
    dy = y2 - y1;
    d2 = (int64_t)dx * dx + (int64_t)dy * dy;
    if (!d2)
        return;
    d = (int32_t)gx_isqrt64((uint64_t)d2);
    if (!d)
        d = 1;
    r = ry;
    if (2 * r < d)
        r = (d + 1) / 2;
    hh = (int64_t)r * r - (((int64_t)d * d) >> 2);
    h = (int32_t)gx_isqrt64((uint64_t)(hh > 0 ? hh : 0));
    if (large == sweep)
        h = -h;
    ux = dx * 16384 / d;                           /* the chord's direction, Q14 */
    uy = dy * 16384 / d;
    cx = (X1 + X2) / 2 - (int32_t)(((int64_t)h * uy) >> 14);
    cy = (y1 + y2) / 2 + (int32_t)(((int64_t)h * ux) >> 14);
    a1 = gx_atan2(y1 - cy, X1 - cx);
    a2 = gx_atan2(y2 - cy, X2 - cx);
    raw = a2 - a1;                                 /* the sweep: + with the sweep flag, - without; 2^24 a turn */
    span24 = (int32_t)(raw >> 8);
    if (!sweep && raw)
        span24 -= 0x1000000;
    len = ((uint32_t)(span24 < 0 ? -span24 : span24) >> 8) * (uint32_t)r >> 14;   /* ~ 0.64 x the arc's length */
    n = (int32_t)gx_pieces(t, (int32_t)(len * 3u / 2u), 2, 48);
    for (i = 1; i < n; i++) {
        uint32_t a = a1 + ((uint32_t)(span24 * i / n) << 8);
        int32_t px = cx + (int32_t)(((int64_t)r * sine_i(a + 0x40000000u)) >> 15);
        int32_t py = cy + (int32_t)(((int64_t)r * sine_i(a)) >> 15);
        if (rx != ry)
            px = px * rx / ry;
        gx_to(gx_X(t, px), gx_Y(t, py));
    }
    gx_to(gx_X(t, x2), gx_Y(t, y2));
}

static const char *gx_num(const char *s, int32_t *v)   /* a number of the path data, in 1/64 */
{
    int32_t ip = 0, fp = 0, fd = 1, neg = 0, any = 0;
    while (*s == ' ' || *s == ',' || *s == '\n' || *s == '\t')
        s++;
    if (*s == '-' || *s == '+')
        neg = *s++ == '-';
    while (*s >= '0' && *s <= '9') {
        ip = ip * 10 + (*s++ - '0');
        any = 1;
    }
    if (*s == '.') {
        s++;
        while (*s >= '0' && *s <= '9') {
            if (fd < 100000) {
                fp = fp * 10 + (*s - '0');
                fd *= 10;
            }
            s++;
            any = 1;
        }
    }
    if (!any)
        return 0;
    *v = ip * 64 + (fp * 64 + fd / 2) / fd;
    if (neg)
        *v = -*v;
    return s;
}

static const char *gx_flag(const char *s, int *f)    /* an arc flag: 0 or 1, separators optional */
{
    while (*s == ' ' || *s == ',' || *s == '\n' || *s == '\t')
        s++;
    if (*s != '0' && *s != '1')
        return 0;
    *f = *s == '1';
    return s + 1;
}

/* the path data d, through t, into segments; close_all: every subpath is closed (for a fill) */
static void gx_build(const gx_xf_t *t, const char *d, int close_all)
{
    int32_t x = 0, y = 0, sx = 0, sy = 0, cx = 0, cy = 0, v[7];
    char cmd = 0, last = 0;
    gx.nseg = 0;
    gx.open = 0;
    for (;;) {
        const char *s = d;
        int rel, k, nargs, f1 = 0, f2 = 0;
        while (*s == ' ' || *s == ',' || *s == '\n' || *s == '\t')
            s++;
        if (!*s)
            break;
        if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z'))
            cmd = *s++;
        else if (!cmd)
            break;
        rel = cmd >= 'a';
        switch (cmd | 32) {
        case 'z':
            gx_sub_end(1);
            x = sx;
            y = sy;
            d = s;
            last = 'z';
            cmd = 0;
            continue;
        case 'h':
        case 'v':
            nargs = 1;
            break;
        case 'm':
        case 'l':
        case 't':
            nargs = 2;
            break;
        case 'q':
        case 's':
            nargs = 4;
            break;
        case 'c':
            nargs = 6;
            break;
        case 'a':
            nargs = 7;
            break;
        default:
            return;                                   /* not path data */
        }
        for (k = 0; k < nargs; k++) {
            if ((cmd | 32) == 'a' && (k == 3 || k == 4)) {
                s = gx_flag(s, k == 3 ? &f1 : &f2);
                v[k] = 0;
            } else {
                s = gx_num(s, &v[k]);
            }
            if (!s)
                return;
        }
        if (rel && (cmd | 32) == 'a') {
            v[5] += x;
            v[6] += y;
        } else if (rel) {
            for (k = 0; k < nargs; k++)
                if ((cmd | 32) == 'h')
                    v[k] += x;
                else if ((cmd | 32) == 'v')
                    v[k] += y;
                else
                    v[k] += (k & 1) ? y : x;
        }
        switch (cmd | 32) {
        case 'm':
            gx_sub_end(close_all);
            x = sx = v[0];
            y = sy = v[1];
            gx_to(gx_X(t, x), gx_Y(t, y));
            cmd = rel ? 'l' : 'L';                    /* further pairs are lines */
            break;
        case 'l':
            x = v[0];
            y = v[1];
            gx_to(gx_X(t, x), gx_Y(t, y));
            break;
        case 'h':
            x = v[0];
            gx_to(gx_X(t, x), gx_Y(t, y));
            break;
        case 'v':
            y = v[0];
            gx_to(gx_X(t, x), gx_Y(t, y));
            break;
        case 'q':
        case 't': {
            int32_t qx, qy, n, i;
            if ((cmd | 32) == 'q') {
                qx = v[0];
                qy = v[1];
                v[0] = v[2];
                v[1] = v[3];
            } else {                                  /* T: the last control point mirrored */
                qx = (last == 'q' || last == 't') ? 2 * x - cx : x;
                qy = (last == 'q' || last == 't') ? 2 * y - cy : y;
            }
            n = (int32_t)gx_pieces(t, gx_span(x, qx, y, qy) + gx_span(qx, v[0], qy, v[1]), 2, 16);
            for (i = 1; i <= n; i++) {
                int64_t u = i * 4096 / n, w = 4096 - u;
                int32_t px = (int32_t)((w * w * x + 2 * u * w * qx + u * u * v[0]) >> 24);
                int32_t py = (int32_t)((w * w * y + 2 * u * w * qy + u * u * v[1]) >> 24);
                gx_to(gx_X(t, px), gx_Y(t, py));
            }
            cx = qx;
            cy = qy;
            x = v[0];
            y = v[1];
            break;
        }
        case 'c':
        case 's': {
            int32_t c1x, c1y, c2x, c2y, ex, ey, n, i;
            if ((cmd | 32) == 'c') {
                c1x = v[0];
                c1y = v[1];
                c2x = v[2];
                c2y = v[3];
                ex = v[4];
                ey = v[5];
            } else {                                  /* S: the first control point mirrored */
                c1x = (last == 'c' || last == 's') ? 2 * x - cx : x;
                c1y = (last == 'c' || last == 's') ? 2 * y - cy : y;
                c2x = v[0];
                c2y = v[1];
                ex = v[2];
                ey = v[3];
            }
            n = (int32_t)gx_pieces(t, gx_span(x, c1x, y, c1y) + gx_span(c1x, c2x, c1y, c2y) + gx_span(c2x, ex, c2y, ey),
                                   3, 24);
            for (i = 1; i <= n; i++) {
                int64_t u = i * 1024 / n, w = 1024 - u;
                int64_t b0 = w * w * w, b1 = 3 * u * w * w, b2 = 3 * u * u * w, b3 = u * u * u;
                int32_t px = (int32_t)((b0 * x + b1 * c1x + b2 * c2x + b3 * ex) >> 30);
                int32_t py = (int32_t)((b0 * y + b1 * c1y + b2 * c2y + b3 * ey) >> 30);
                gx_to(gx_X(t, px), gx_Y(t, py));
            }
            cx = c2x;
            cy = c2y;
            x = ex;
            y = ey;
            break;
        }
        case 'a':
            gx_arc(t, x, y, v[0], v[1], f1, f2, v[5], v[6]);
            x = v[5];
            y = v[6];
            break;
        }
        last = (char)(cmd | 32);
        d = s;
    }
    gx_sub_end(close_all);
}

/* ------------------------------------------------- paths: rasterising --- */
/* fill: per canvas row, each segment's piece of the row adds its exact area to an accumulation
 * row (the second difference of the area left of x), whose running sum is the coverage; the
 * winding is non-zero, |sum| clamped. A full pixel is 8192 */
static void gx_acc_add(int32_t i, int32_t bx0, int32_t n, int32_t v)
{
    i -= bx0;
    if (i < 0)
        i = 0;                                    /* left of the box: counts for all of it */
    if (i < n)
        gx_acc_put(i, gx_acc_get(i) + v);
}

static int32_t gx_area2(int32_t X, int32_t x0, int32_t x1, int32_t h)   /* twice the area left of X */
{
    int32_t u;
    uint32_t f;
    if (X <= x0)
        return 0;
    if (X >= x1)
        return h * (2 * X - x0 - x1);
    u = X - x0;                                       /* 0 < u < x1 - x0 <= 64000: h u^2 / W as h u (u / W) */
    f = ((uint32_t)u << 15) / (uint32_t)(x1 - x0);
    return (int32_t)(((int64_t)(h * u) * (int32_t)f) >> 15);
}

static void gx_fill_paint(uint32_t c)
{
    int32_t xmin = 0x7FFFFFFF, xmax = -0x7FFFFFFF, ymin = 0x7FFFFFFF, ymax = -0x7FFFFFFF, bx0, bx1, by0, by1, py;
    uint32_t k;
    if (c == GX_NONE || !gx.nseg)
        return;
    for (k = 0; k < gx.nseg; k++) {
        int32_t x0 = gx_sg(k, GXS_X0), y0 = gx_sg(k, GXS_Y0), x1 = gx_sg(k, GXS_X1), y1 = gx_sg(k, GXS_Y1);
        xmin = x0 < xmin ? x0 : xmin;
        xmin = x1 < xmin ? x1 : xmin;
        xmax = x0 > xmax ? x0 : xmax;
        xmax = x1 > xmax ? x1 : xmax;
        ymin = y0 < ymin ? y0 : ymin;
        ymin = y1 < ymin ? y1 : ymin;
        ymax = y0 > ymax ? y0 : ymax;
        ymax = y1 > ymax ? y1 : ymax;
        gx_sg_dxdy_put(k, y1 != y0 ? (x1 - x0) * 4096 / (y1 - y0) : 0);
    }
    bx0 = xmin >> GX_Q;
    bx1 = (xmax + 63) >> GX_Q;
    by0 = ymin >> GX_Q;
    by1 = (ymax + 63) >> GX_Q;
    bx0 = bx0 < 0 ? 0 : bx0;
    by0 = by0 < 0 ? 0 : by0;
    bx1 = bx1 > gx.w ? gx.w : bx1;
    by1 = by1 > gx.h ? gx.h : by1;
    if (bx0 >= bx1 || by0 >= by1)
        return;
    for (py = by0; py < by1; py++) {
        int32_t top = py << GX_Q, bot = top + 64, n = bx1 - bx0, s = 0, px;
        for (px = 0; px < n; px++)
            gx_acc_put(px, 0);
        for (k = 0; k < gx.nseg; k++) {
            int32_t x0 = gx_sg(k, GXS_X0), y0 = gx_sg(k, GXS_Y0), x1 = gx_sg(k, GXS_X1), y1 = gx_sg(k, GXS_Y1), dir = 1;
            int32_t dxdy = gx_sg_dxdy(k);
            int32_t ya, yb, xa, xb, h, lo, hi, c0, cl, i, p1, p2;
            if (y0 == y1)
                continue;
            if (y0 > y1) {
                int32_t tx = x0, ty = y0;
                x0 = x1;
                y0 = y1;
                x1 = tx;
                y1 = ty;
                dir = -1;
            }
            if (y1 <= top || y0 >= bot)
                continue;
            ya = y0 > top ? y0 : top;
            yb = y1 < bot ? y1 : bot;
            xa = ya == y0 ? x0 : x0 + (int32_t)(((int64_t)(ya - y0) * dxdy) >> 12);
            xb = yb == y1 ? x1 : x0 + (int32_t)(((int64_t)(yb - y0) * dxdy) >> 12);
            h = (yb - ya) * dir;
            lo = xa < xb ? xa : xb;
            hi = xa < xb ? xb : xa;
            c0 = lo >> GX_Q;
            cl = (hi + 63) >> GX_Q;
            if (c0 >= bx0 + n)
                continue;                             /* right of the box: no pixel of it changes */
            if (cl < bx0) {
                gx_acc_put(0, gx_acc_get(0) + 128 * h);   /* left of the box: the whole row piece counts */
                continue;
            }
            p2 = 0;                                   /* area at c0 - 1 and c0: nothing left of lo */
            p1 = 0;
            for (i = c0; i <= cl && i - bx0 < n; i++) {
                int32_t cur = gx_area2((i + 1) * (1 << GX_Q), lo, hi, h);   /* i < 0 off the left edge */
                gx_acc_add(i, bx0, n, cur - 2 * p1 + p2);
                p2 = p1;
                p1 = cur;
            }
        }
        for (px = 0; px < n; px++) {
            uint32_t a;
            s += gx_acc_get(px);
            a = (uint32_t)(s < 0 ? -s : s);
            if (a > 8192u)
                a = 8192u;
            a = (a * 255u + 4096u) >> 13;
            if (a)
                gx_blend(bx0 + px, py, c, a);
        }
    }
}

/* stroke: every segment a capsule of half width r (1/64 px) into the mask, keeping the larger
 * coverage where they meet (round joins), then the mask in colour c */
static void gx_capsule(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t r)
{
    int32_t dx = bx - ax, dy = by - ay, len, ux, uy, px, py, m = r + 64, b[4];
    b[0] = ((ax < bx ? ax : bx) - m) >> GX_Q;
    b[1] = ((ay < by ? ay : by) - m) >> GX_Q;
    b[2] = ((ax > bx ? ax : bx) + m + 63) >> GX_Q;
    b[3] = ((ay > by ? ay : by) + m + 63) >> GX_Q;
    b[0] = b[0] < 0 ? 0 : b[0];
    b[1] = b[1] < 0 ? 0 : b[1];
    b[2] = b[2] > gx.w ? gx.w : b[2];
    b[3] = b[3] > gx.h ? gx.h : b[3];
    if (b[0] >= b[2] || b[1] >= b[3])
        return;
    len = (int32_t)gx_isqrt64((uint64_t)((int64_t)dx * dx + (int64_t)dy * dy));
    if (len) {
        ux = dx * 16384 / len;
        uy = dy * 16384 / len;
    } else {
        ux = 1 << 14;
        uy = 0;
    }
    for (py = b[1]; py < b[3]; py++)
        for (px = b[0]; px < b[2]; px++) {
            int32_t cx = (px << GX_Q) + 32 - ax, cy = (py << GX_Q) + 32 - ay, across, along, d;
            uint32_t cov;
            uint8_t *mp;
            across = (int32_t)(((int64_t)cx * uy - (int64_t)cy * ux) >> 14);
            if (across >= r + 32 || across <= -(r + 32))
                continue;
            along = (int32_t)(((int64_t)cx * ux + (int64_t)cy * uy) >> 14);
            if (along < 0)
                d = (int32_t)gx_isqrt((uint32_t)(cx * cx + cy * cy));
            else if (along > len)
                d = (int32_t)gx_isqrt((uint32_t)((cx - dx) * (cx - dx) + (cy - dy) * (cy - dy)));
            else
                d = across < 0 ? -across : across;
            cov = gx_cov(r - d);
            if (!cov)
                continue;
            mp = &gx_mask[(uint32_t)py * (uint32_t)gx.w + (uint32_t)px];
            if (cov > *mp)
                *mp = (uint8_t)cov;
            gx.mx0 = px < gx.mx0 ? px : gx.mx0;
            gx.my0 = py < gx.my0 ? py : gx.my0;
            gx.mx1 = px + 1 > gx.mx1 ? px + 1 : gx.mx1;
            gx.my1 = py + 1 > gx.my1 ? py + 1 : gx.my1;
        }
}

static void gx_mask_paint(uint32_t c)            /* the mask in colour c, then cleared */
{
    int32_t px, py;
    for (py = gx.my0; py < gx.my1; py++)
        for (px = gx.mx0; px < gx.mx1; px++) {
            uint8_t *mp = &gx_mask[(uint32_t)py * (uint32_t)gx.w + (uint32_t)px];
            if (*mp) {
                gx_blend(px, py, c, *mp);
                *mp = 0;
            }
        }
    gx.mx0 = gx.w;
    gx.my0 = gx.h;
    gx.mx1 = gx.my1 = 0;
}

static uint32_t gx_seg_len(uint32_t k)
{
    int32_t dx = gx_sg(k, GXS_X1) - gx_sg(k, GXS_X0), dy = gx_sg(k, GXS_Y1) - gx_sg(k, GXS_Y0);
    return gx_isqrt64((uint64_t)((int64_t)dx * dx + (int64_t)dy * dy));
}

static void gx_stroke_paint(uint32_t c, int32_t r)
{
    uint32_t k, total = 0, left = 0xFFFFFFFFu;
    if (c == GX_NONE)
        return;
    if (gx.trim) {                                    /* only the first gx.trim / 1000 of the length */
        for (k = 0; k < gx.nseg; k++)
            total += gx_seg_len(k);
        left = total > 4000000u ? total / 1000u * gx.trim : total * gx.trim / 1000u;
    }
    for (k = 0; k < gx.nseg && left; k++) {
        int32_t x0 = gx_sg(k, GXS_X0), y0 = gx_sg(k, GXS_Y0), x1 = gx_sg(k, GXS_X1), y1 = gx_sg(k, GXS_Y1);
        if (gx.trim) {
            uint32_t len = gx_seg_len(k);
            if (len > left) {                         /* the last piece: as far as the length goes */
                int32_t f = (int32_t)((left << 12) / len);
                x1 = x0 + (int32_t)(((int64_t)(x1 - x0) * f) >> 12);
                y1 = y0 + (int32_t)(((int64_t)(y1 - y0) * f) >> 12);
                len = left;
            }
            left -= len;
        }
        gx_capsule(x0, y0, x1, y1, r);
    }
    gx_mask_paint(c);
}

/* the same, in screen coordinates (1/64 px) */
static void gx_to_s(int32_t x, int32_t y) { gx_to(x - (gx.x0 << GX_Q), y - (gx.y0 << GX_Q)); }
static void gx_seg_s(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    gx_seg_add(x0 - (gx.x0 << GX_Q), y0 - (gx.y0 << GX_Q), x1 - (gx.x0 << GX_Q), y1 - (gx.y0 << GX_Q));
}

/* SVG path data d through t: filled with fill, stroked with stroke at width sw (local units) */
static void gx_path(const gx_xf_t *t, const char *d, uint32_t fill, uint32_t stroke, int32_t sw)
{
    if (fill != GX_NONE) {
        gx_build(t, d, 1);
        gx_fill_paint(fill);
    }
    if (stroke != GX_NONE) {
        gx_build(t, d, 0);
        gx_stroke_paint(stroke, (int32_t)(((int64_t)sw * t->s) >> 13));
    }
}

/* an ellipse (the boards' <ellipse>), filled, as a 32-sided polygon */
static void gx_ellipse(const gx_xf_t *t, int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t fill)
{
    uint32_t i;
    if (fill == GX_NONE)
        return;
    gx.nseg = 0;
    gx.open = 0;
    for (i = 0; i <= 32u; i++) {
        uint32_t a = i * (0x100000000ull / 32u) & 0xFFFFFFFFu;
        int32_t x = cx + (int32_t)(((int64_t)rx * sine_i(a + 0x40000000u)) >> 15);
        int32_t y = cy + (int32_t)(((int64_t)ry * sine_i(a)) >> 15);
        gx_to(gx_X(t, x), gx_Y(t, y));
    }
    gx_sub_end(1);
    gx_fill_paint(fill);
}
