/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): the fork's type for the synth's screen. The boards set the screen's words in
 * a rounded gothic (capitals, 10 px and up); the synth has no such font, so this is one of our own,
 * drawn from scratch as strokes: each glyph is SVG path data along the middle of its strokes, on
 * a cap height of 100 (y 0 the top, 100 the baseline), drawn by gx_path with round caps and joins,
 * so one drawing serves every size and weight. Digits all have the same advance (the boards'
 * figures are tabular). Characters without a glyph draw nothing and advance like a space. */
typedef struct {
    char c;
    uint8_t w;                   /* width of the stroke middles, in cap-height hundredths */
    const char *d;
} gx_glyph_t;

static const gx_glyph_t GX_GLYPHS[] = {
    {' ', 34, ""},
    {'-', 40, "M0 56H40"},
    {'.', 0, "M0 100V100"},
    {'/', 44, "M44 0L0 100"},
    {':', 0, "M0 34V34M0 100V100"},
    {'=', 52, "M0 38H52M0 68H52"},
    {'+', 58, "M29 26V84M0 55H58"},
    {'(', 24, "M24 -8Q-8 50 24 108"},
    {')', 24, "M0 -8Q32 50 0 108"},
    {'%', 70, "M70 0L0 100M14 0A12 14 0 1 1 14 28A12 14 0 1 1 14 0ZM56 72A12 14 0 1 1 56 100A12 14 0 1 1 56 72Z"},
    {'*', 0, "M0 55V55"},                         /* the boards' middle dot (BELLS * GLASS BELL) */
    {'#', 60, "M22 4L14 96M48 4L40 96M4 34H60M0 66H56"},   /* round UX-2: sharps in note names (F#2) */
    {'0', 64, "M32 0A32 50 0 1 1 32 100A32 50 0 1 1 32 0Z"},
    {'1', 64, "M10 20L36 0V100"},
    {'2', 64, "M4 24C8 -6 60 -8 60 26C60 52 6 66 2 100H62"},
    {'3', 64, "M4 14C16 -4 58 -2 58 24C58 42 42 47 24 47C44 47 62 52 62 74C62 104 12 106 2 86"},
    {'4', 64, "M46 100V0L0 70H64"},
    {'5', 64, "M58 0H10L6 44C22 32 62 34 62 68C62 104 12 104 2 86"},
    {'6', 64, "M54 4C26 -6 2 18 2 62C2 88 16 100 33 100C51 100 62 86 62 69C62 50 50 38 33 38C18 38 6 46 2 58"},
    {'7', 64, "M0 0H62L20 100"},
    {'8', 64, "M32 46C12 46 5 36 5 23C5 9 17 0 32 0C47 0 59 9 59 23C59 36 52 46 32 46C11 46 1 58 1 73C1 90 15 100 32 100C49 100 63 90 63 73C63 58 53 46 32 46Z"},
    {'9', 64, "M10 96C38 106 62 82 62 38C62 12 48 0 31 0C13 0 2 14 2 31C2 50 14 62 31 62C46 62 58 54 62 42"},
    {'A', 80, "M0 100L40 0L80 100M15 66H65"},
    {'B', 60, "M0 100V0H31A24 24 0 0 1 31 48H0M31 48A26 26 0 0 1 31 100H0"},
    {'C', 84, "M84 22A46 50 0 1 0 84 78"},
    {'D', 74, "M0 0V100H26A48 50 0 0 0 26 0Z"},
    {'E', 56, "M56 0H0V100H56M0 50H50"},
    {'F', 54, "M54 0H0V100M0 50H48"},
    {'G', 90, "M84 22A46 50 0 1 0 90 54H52"},
    {'H', 72, "M0 0V100M72 0V100M0 50H72"},
    {'I', 0, "M0 0V100"},
    {'J', 48, "M48 0V72A24 28 0 0 1 0 72"},
    {'K', 66, "M0 0V100M64 0L4 60M24 42L66 100"},
    {'L', 54, "M0 0V100H54"},
    {'M', 88, "M0 100V0L44 64L88 0V100"},
    {'N', 72, "M0 100V0L72 100V0"},
    {'O', 94, "M47 0A47 50 0 1 1 47 100A47 50 0 1 1 47 0Z"},
    {'P', 60, "M0 100V0H32A28 27 0 0 1 32 54H0"},
    {'Q', 94, "M47 0A47 50 0 1 1 47 100A47 50 0 1 1 47 0ZM62 74L94 104"},
    {'R', 64, "M0 100V0H32A27 26 0 0 1 32 52H0M30 52L64 100"},
    {'S', 64, "M60 16C52 0 6 -4 6 24C6 48 62 46 62 76C62 104 10 104 2 84"},
    {'T', 64, "M0 0H64M32 0V100"},
    {'U', 72, "M0 0V62A36 38 0 0 0 72 62V0"},
    {'V', 76, "M0 0L38 100L76 0"},
    {'W', 106, "M0 0L24 100L53 14L82 100L106 0"},
    {'X', 70, "M0 0L70 100M70 0L0 100"},
    {'Y', 72, "M0 0L36 52L72 0M36 52V100"},
    {'Z', 64, "M2 0H64L0 100H64"},
};
#define GX_NGLYPHS (sizeof GX_GLYPHS / sizeof GX_GLYPHS[0])

static const gx_glyph_t *gx_glyph(char c)
{
    uint32_t i;
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 32);
    for (i = 0; i < GX_NGLYPHS; i++)
        if (GX_GLYPHS[i].c == c)
            return &GX_GLYPHS[i];
    return &GX_GLYPHS[0];
}

/* type metrics at size (the boards' font-size) and weight (stroke width), both 1/64 px:
 * capitals 0.70 of the size tall, outside to outside; side bearings 0.10 of the size */
typedef struct {
    int32_t size, weight, track;    /* track: letter spacing, 1/1000 of the size */
} gx_type_t;

static int32_t gx_glyph_scale(const gx_type_t *ty)     /* glyph units (1/64 of a hundredth) to 1/64 px, Q12 */
{
    int32_t cap = ty->size * 70 / 100 - ty->weight;
    return cap > 2 ? cap * 4096 / 6400 : 1;           /* (never 0: gx_text divides by it) */
}

static int32_t gx_advance(const gx_type_t *ty, const gx_glyph_t *g, int32_t s)
{
    int32_t w = g->c >= '0' && g->c <= '9' ? 64 : g->w;
    return ((w * 64 * s) >> 12) + ty->weight + ty->size / 10 + ty->size * ty->track / 1000;
}

static int32_t gx_text_w(const gx_type_t *ty, const char *str)   /* ink width, 1/64 px */
{
    int32_t s = gx_glyph_scale(ty), w = 0;
    if (!*str)
        return 0;
    for (; *str; str++)
        w += gx_advance(ty, gx_glyph(*str), s);
    return w - ty->size / 10 - ty->size * ty->track / 1000;   /* no bearing after the last */
}

/* str on the baseline y; x the left (align 0), the middle (1) or the right (2) of its ink */
static void gx_text(int32_t x, int32_t y, const gx_type_t *ty, int align, const char *str, uint32_t c)
{
    int32_t s = gx_glyph_scale(ty), pen;
    gx_xf_t t;
    if (align)
        x -= gx_text_w(ty, str) / (align == 1 ? 2 : 1);
    pen = x + ty->weight / 2;
    t.s = s;
    t.ty = y - ty->weight / 2 - (int32_t)(((int64_t)6400 * s) >> 12);
    for (; *str; str++) {
        const gx_glyph_t *g = gx_glyph(*str);
        int32_t dx = g->c >= '0' && g->c <= '9' ? ((64 - g->w) * 64 * s) >> 13 : 0;   /* figures centred */
        t.tx = pen + dx;
        if (g->d[0])
            gx_path(&t, g->d, GX_NONE, c, (ty->weight << 12) / s);
        pen += gx_advance(ty, g, s);
    }
}
