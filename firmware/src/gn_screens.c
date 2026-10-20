/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): the Easy screens (boards FM1-Easy-Play 3b to FM1-Boot 3i, FM1-Gnome 9b),
 * drawn with gn_gfx.c in the boards' own coordinates. Included by gn_easy.c.
 *
 * Each frame gn_draw takes a snapshot of what the screen shows (gs) and splits the screen into
 * parts, each a box with a signature of everything drawn in it. A part whose box and signature are
 * not on the screen yet is redrawn: the scene is drawn again with the canvas clipped to that box, a
 * strip at a time; a new view (or a screen someone else drew on) is redrawn whole. Each call draws
 * at most GS_BUDGET_PX pixels (240 x 32: about 3.5 million host instructions at the dearest, and
 * 10 ms of the LCD's SPI) and returns to the main loop; the rest goes on in the next frame
 * (finding 6): a whole screen takes 8 frames. A part counts as on
 * the screen only when every strip that drew it drew the same signature, so state that moves while a
 * box is drawn over two calls is drawn again. Animations run on fm1_ms, so they keep their speed
 * however long a frame takes. */
enum { GV_BOOT, GV_PLAY, GV_PICK, GV_PEEK, GV_LOOP, GV_STEPS, GV_RECORD, GV_DRUMS, GV_MIX, GV_TAP, GV_COUNT, GV_SAVED,
       GV_SWITCH, GV_HOME, GV_GROOVES, GV_NONE };   /* (round FLOW-1, M2: no deep page, no SONG until real) */
static const uint16_t GS_FIELD[5] = {GX_BLUE, GX_GREEN, GX_YELLOW, GX_RED, GX_PAPER};   /* 4: another sound */
#define GS_BUDGET_PX (240u * 32u)                 /* pixels a call draws at most: a strip is as many rows of
                                                   * its box as fit (64 at most), so no call draws more */

static struct {
    uint8_t view;
    uint8_t pic, field, ink;              /* the picture (0..7, GN_KIT, GN_OTHER), its colour, its lines */
    uint8_t idle;                         /* the bubbles rest (another sound) */
    uint8_t r[4];                         /* the bubbles: disc radius, 1/16 px */
    uint8_t play, linked;                 /* 0 stopped, 1 playing, 2 recording; a USB host */
    uint8_t blink;                        /* the record disc's blink, 0 off .. 1 on */
    uint8_t vowel;                        /* the Gnome: 0 mouth shut, 1..5 A E I O U */
    uint8_t note;                         /* the Gnome: the floating note's frame, 0 = none */
    uint8_t step, bars;                   /* loop: the playhead in its bar, of 16 (0xFF none), the loop's bars */
    uint16_t pads, keys;                  /* drums: pads lit, pad keys down */
    uint16_t sweep;                       /* switch: the ring, 0 .. 1000 */
    uint8_t to_easy;                      /* switch: the way it goes */
    uint8_t grv;                          /* the grooves' list (gn.grv; 0 the peek list): a byte alignment left free */
    uint16_t boot;                        /* start-up: ms since it began (in the part's frames) */
    uint8_t trk;                          /* the selected track, 0..3 (its colour: GS_FIELD) */
    uint8_t clock;                        /* the tempo follows an external clock */
    int16_t bpm;                          /* the tempo shown (the clock's while it follows one) */
    uint8_t lvl[NTRK];                    /* volumes: each track's level, 0..127 */
    uint8_t taps;                         /* tap tempo: taps counted, 0..4 */
    uint8_t saved;                        /* saved: 0 the app's (3g), 1 SAVED, 2 FULL, 3 NOT SAVED (the panel's) */
    uint8_t snd;                          /* the sound (GN_SOUNDS index, GN_NOSND): the peek list's current one */
    uint8_t rec;                          /* recording: 0 none, 1 armed, 2 counting in, 3 recording */
    uint8_t replace;                      /* K4 while armed or recording: 0 KEEP, 1 REPLACE */
    uint8_t count;                        /* the count-in's beat: 4, 3, 2, 1 */
    uint8_t role[4];                      /* what each knob does now (gn_easy.c gn_knob_role: KR_*): its card */
    /* the loop pages: the selected track's grid and loop, and one bar of it up close */
    uint8_t grid, spb, swing, vol, snap;  /* Easy's grid (0: not Easy's); a bar's steps; SWING %; VOLUME %; SNAP */
    uint8_t drums, punch, playing;        /* the drums (one row per sound); punching in; the loop plays */
    uint8_t bar, col, cur;                /* the bar shown; the playhead's column in it (0xFF none); the cursor's */
    uint8_t lanes;                        /* drums: the rows shown (lane bits) */
    uint8_t pitch[32], lane[32];          /* each column of the bar: its first note (0 none, 1 a tie), its lanes */
    uint16_t occ[8];                      /* the record screen: each bar's used steps, folded onto 16 */
    char name[13];                        /* another sound: its own name */
    uint8_t mark;                         /* round 25: the loop is Easy's saved song (the header's check; name's padding) */
    uint32_t namesig;
    char head[24];                        /* the header's words: the picture, or another sound's name */
    uint32_t headsig;
    /* Home (round UX-1): a row a track */
    char row[NTRK][13];                   /* its sound, as much as fits */
    uint32_t rowsig[NTRK];
    uint8_t mute, live, armed;            /* bits: muted, sounding now, armed (the selected track's) */
    uint8_t hat;                          /* the empty kit row that wears the Gnome's hat; 0xFF none */
    /* the steps and the record screen (round UX-2) */
    uint8_t cells, ccol;                  /* places in the bar's row (8 or 16), the playhead's (0xFF none) */
    uint8_t cnote[16];                    /* each place: the note it begins (0 none, 1 held on) */
    uint8_t curnote, lnote;               /* the cursor's step holds a note; the note NOTE writes */
    uint8_t ndrum, drum, dbank;           /* a kit: its drums, K2's, the half of the list shown (0: 1-8, 1: 9-16) */
    uint8_t dnote[8];                     /* .. the eight rows shown: each drum's note, */
    uint16_t drow[8];                     /* .. its places in the bar, */
    const char *drn[8];                   /* .. its name */
    const char *dname;                    /* K2's drum's name */
    /* the sound, the kit, the deep page (round UX-3) */
    uint8_t amt[4];                       /* the four knobs, 0 .. GN_STEPS */
    char lbl[4][8];                       /* round MF: their names (a Gnomad sound's own, else TONE MOTION ECHO SPACE) */
    uint8_t icon[4];                      /* .. each bubble's sign: 0 sun, 1 wave, 2 echoes, 3 arch, 4 none */
    uint32_t lblsig;
    uint8_t kn, bank;                     /* a kit: its pads (16, or 8), the bank shown (0 A, 1 B) */
    const char *kname[8];                 /* .. the bank's pads' names */
    /* SHIFT's depth (round UX-4) */
    uint8_t shift;                        /* SHIFT held (EDIT): the page's Advanced depth */
    uint8_t ccur;                         /* the steps: the cursor's place in the bar shown (0xFF: another bar) */
    uint8_t snote, svel, slen, son;       /* .. the cursor's step: its note, strength, length (steps), plays */
    int8_t pan[NTRK];                     /* the volumes: each track's pan (-64 .. 63) */
    uint8_t stored;                       /* round UX-5: the music is as the last save read back from flash */
    /* round SEQ-1 (phase 1): the lit knobs, the page ring, layout A's lanes, the STEPS tap grid */
    uint8_t lit, hlit;                    /* bit k: knob k turned in the last 1.2 s; the newest of them (0xFF none) */
    uint8_t ring;                         /* the page's place in SELECT's ring + 1 (0: an overlay, no dots) */
    uint8_t tempo;                        /* the big number is EDIT held + SELECT's (not the taps') */
    uint16_t ln_occ[NTRK], ln_tie[NTRK];  /* each track's lane: the places of its bar shown that begin, hold on */
    uint8_t ln_cells[NTRK], ln_col[NTRK]; /* .. its places (16 or 8), its playhead's place (0xFF none) */
    uint8_t trows;                        /* the selected kit's lane: its most used drums (up to three), */
    uint16_t trow[3];                     /* .. each one's places */
    uint8_t hold;                         /* STEPS: the place of the key held + 1 (0 none) */
    uint8_t swhen;                        /* .. the cursor's step: its chance (%) */
    uint16_t pon, pmaybe;                 /* .. the places lit (K2's drum, or a note), those with a chance below 100 */
    uint8_t pvel[16], podr[16];           /* .. each place's strength, the other drums it plays */
    /* round FLOW-1: the transport queued for the bar line (GN_TQ_*), the click (bit 0 on, bit 1 EDIT held with K3 the
     * click's: the band says which), bit 2 GLO held (the band names a fill and the stop at the bar) */
    uint8_t tq, clk;
    uint8_t tile, tile_q, tile_ask, mine, prog;   /* M2, GROOVES: gn's tiles; the loop's place (0..255) */
    uint8_t undid, chg;                   /* M3: what SAVE held undid + 1 (2 s); the sound changed while playing (1.5 s) */
} gs;

/* --------------------------------------------------------- the parts --- */
typedef struct {
    uint8_t x0, y0, x1, y1;               /* screen px; x1, y1 exclusive */
    uint32_t sig;
} gs_part_t;
#define GS_NPARTS 64
static gs_part_t gs_now[GS_NPARTS];               /* this frame's parts (PARTS[view]) */
static uint32_t gs_n;
static gs_part_t gs_shown[GS_NPARTS];             /* the parts on the screen, of view gs_shown_view */
static uint32_t gs_shown_n;
static uint8_t gs_shown_view = GV_NONE;
static struct {
    uint8_t on, x0, y0, x1, y1, y;        /* the box being drawn, the next strip's first row */
    uint8_t lost;                         /* gs_pass overflowed: nothing of this box counts as drawn */
} gs_job;
static gs_part_t gs_pass[GS_NPARTS];              /* the parts the job's strips drew, with the signature */
static uint8_t gs_pass_y[GS_NPARTS];              /* .. the row after the last strip that drew it with that
                                                   * signature; GS_BAD: a strip drew it with another, or
                                                   * one drew a row of it while it was no part */
#define GS_BAD 0xFFu
static uint32_t gs_pass_n;
static uint32_t gs_px_call, gs_px_total;          /* pixels drawn: this call, since power-on (the tests) */
static uint8_t gs_owned;                          /* the last frame was Easy's (gn_draw returned 1: the tests) */

static void gs_part(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t sig)
{
    if (gs_n < GS_NPARTS) {
        gs_now[gs_n].x0 = (uint8_t)clamp(x0, 0, 240);
        gs_now[gs_n].y0 = (uint8_t)clamp(y0, 0, 240);
        gs_now[gs_n].x1 = (uint8_t)clamp(x1, 0, 240);
        gs_now[gs_n].y1 = (uint8_t)clamp(y1, 0, 240);
        gs_now[gs_n].sig = sig;
        if (gs_now[gs_n].x0 < gs_now[gs_n].x1 && gs_now[gs_n].y0 < gs_now[gs_n].y1)
            gs_n++;
    }
}

static int gs_same_box(const gs_part_t *a, const gs_part_t *b)
{
    return a->x0 == b->x0 && a->y0 == b->y0 && a->x1 == b->x1 && a->y1 == b->y1;
}

static int gs_inside(const gs_part_t *a, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    return a->x0 >= x0 && a->y0 >= y0 && a->x1 <= x1 && a->y1 <= y1;
}

static int gs_is_shown(const gs_part_t *a)
{
    uint32_t i;
    for (i = 0; i < gs_shown_n; i++)
        if (gs_same_box(&gs_shown[i], a) && gs_shown[i].sig == a->sig)
            return 1;
    return 0;
}

static void gs_job_start(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    gs_job.on = 1;
    gs_job.x0 = (uint8_t)x0;
    gs_job.y0 = (uint8_t)y0;
    gs_job.x1 = (uint8_t)x1;
    gs_job.y1 = (uint8_t)y1;
    gs_job.y = (uint8_t)y0;
    gs_job.lost = 0;
    gs_pass_n = 0;
}

/* a strip of the job drawn (rows y0 .. y1 - 1 of the job's columns): each part of this frame that
 * touches it, with the signature drawn. Strips go down the box one after the other, so a part is
 * drawn whole with one signature when the first strip that noted it held its first row, each later
 * one began where the last ended, and the last reached its bottom (gs_job_end) */
static void gs_pass_note(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    uint32_t i, j;
    for (i = 0; i < gs_n; i++) {
        const gs_part_t *a = &gs_now[i];
        if (a->x1 <= x0 || a->x0 >= x1 || a->y1 <= y0 || a->y0 >= y1)
            continue;
        for (j = 0; j < gs_pass_n && !gs_same_box(&gs_pass[j], a); j++)
            ;
        if (j == gs_pass_n) {
            if (gs_pass_n >= GS_NPARTS) {
                gs_job.lost = 1;
                continue;
            }
            gs_pass[j] = *a;
            gs_pass_y[j] = (uint8_t)(y0 <= a->y0 ? y1 : GS_BAD);
            gs_pass_n++;
        } else if (gs_pass_y[j] != GS_BAD) {
            gs_pass_y[j] = (uint8_t)(gs_pass[j].sig == a->sig && gs_pass_y[j] == y0 ? y1 : GS_BAD);
        }
    }
}

/* the job's box is drawn: the parts inside it are on the screen as the job drew them (those drawn
 * with one signature), and nothing else inside it is */
static void gs_job_end(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < gs_shown_n; i++)
        if (!gs_inside(&gs_shown[i], gs_job.x0, gs_job.y0, gs_job.x1, gs_job.y1))
            gs_shown[n++] = gs_shown[i];
    gs_shown_n = n;
    for (i = 0; !gs_job.lost && i < gs_pass_n; i++)
        if (gs_pass_y[i] != GS_BAD && gs_pass_y[i] >= gs_pass[i].y1 &&
            gs_inside(&gs_pass[i], gs_job.x0, gs_job.y0, gs_job.x1, gs_job.y1) && gs_shown_n < GS_NPARTS)
            gs_shown[gs_shown_n++] = gs_pass[i];
    gs_job.on = 0;
}

/* a picture's colour: the sound's field, the drums' */
static uint32_t gs_pic_field(uint32_t pic) { return pic < GN_NPICS ? GN_PIC_FIELD[pic] : 4u; }

/* --------------------------------------------------- shared elements --- */
/* Every Easy screen but the start-up, the hold's and the app's saved screen has one frame (round 7, the
 * picks 2 and 7; the boards of easy-controls): the header (the selected track's badge in its
 * colour, what it plays, the tempo), the four knobs' cards in their colours (y 31 .. 69), the field
 * (70 .. 210), and the band (210 .. 240): what the panel's buttons do here, each named in brackets */
#define GS_ROW_CY Q(50)                           /* the knob cards' middle */
static uint32_t gs_mix(uint32_t a, uint32_t b, uint32_t k)   /* a over b at k / 255 */
{
    uint32_t r = (((a >> 11) & 31u) * k + ((b >> 11) & 31u) * (255u - k) + 127u) / 255u;
    uint32_t g = (((a >> 5) & 63u) * k + ((b >> 5) & 63u) * (255u - k) + 127u) / 255u;
    uint32_t bl = ((a & 31u) * k + (b & 31u) * (255u - k) + 127u) / 255u;
    return r << 11 | g << 5 | bl;
}

static const gx_type_t GS_NAME = {Q(17), Q(2.2), 20};            /* the boards' lcd-name: 900, 17 px */
static const gx_type_t GS_LABEL = {Q(10), Q(1.5), 90};           /* .lb: 700, 10 px */
static const gx_type_t GS_LABEL8 = {Q(10), Q(1.5), 80};
static const gx_type_t GS_FIG = {Q(10), Q(1.4), 0};              /* figures (the boards' DM Mono) */
static const gx_type_t GS_HEAD = {Q(12), Q(1.7), 50};            /* the header's words (.sx) */
static const gx_type_t GS_SMALL = {Q(8), Q(1.25), 60};           /* a card's label, BPM (.su) */
static const gx_type_t GS_HUGE = {Q(60), Q(6), 0};               /* the big number (.hg) */

/* ink or paper on colour c of GS_FIELD: ink on yellow and vermilion, paper on blue and green */
static uint32_t gs_on(uint32_t field) { return field >= 2u ? GX_INK : GX_PAPER; }

/* the header: the track's badge (its number on its colour), what it plays, the tempo */
static void gs_head(void)
{
    char b[8];
    uint32_t f = gs.trk % NTRK;
    if (!gx_visible(0, 0, 240, 31))
        return;
    uint32_t ink = gs.rec == 3u ? gs_on(f) : GX_INK;
    /* armed: the badge inverted (ink, its number in the track's colour) and a red disc; recording: the whole
     * header in the track's colour (pick 5: the armed and recording track unmistakable) */
    gx_rect(0, 0, Q(240), Q(29.25), gs.rec == 3u ? GS_FIELD[f] : GX_PAPER);
    gx_rect(0, Q(29.25), Q(240), Q(1.5), GX_INK);
    gx_rrect(Q(6), Q(5), Q(22), Q(20), Q(5), gs.rec ? GX_INK : GS_FIELD[f], gs.rec ? GS_FIELD[f] : GX_INK, Q(1.5));
    b[0] = (char)('1' + f);
    b[1] = 0;
    gx_text(Q(17), Q(19.5), &GS_HEAD, 1, b, gs.rec ? GS_FIELD[f] : gs_on(f));
    gx_text(Q(34), Q(20), &GS_HEAD, 0, gs.head, ink);
    gx_text(Q(234), Q(20), &GS_SMALL, 2, gs.clock ? "BPM IN" : "BPM", ink);
    fmt_int(b, gs.bpm);
    gx_text(Q(234) - gx_text_w(&GS_SMALL, gs.clock ? "BPM IN" : "BPM") - Q(4), Q(20), &GS_HEAD, 2, b, ink);
    if (gs.rec && gs.rec != 3u && gs.blink)
        gx_circle_s(Q(181), Q(15), Q(4.5), f == 3u ? GX_INK : GX_RED, GX_INK, Q(1.25));
    if (gs.ring) {                                   /* round SEQ-1: SELECT's ring, seven dots, this page filled */
        uint32_t i;
        for (i = 0; i < GN_NRING; i++)              /* (round FLOW-1: six, SONG hidden) */
            gx_circle_s(Q(198) + (int32_t)i * Q(6), Q(25.5), Q(1.6), i + 1u == gs.ring ? ink : GX_NONE, ink,
                        i + 1u == gs.ring ? 0 : Q(1));
    }
    if ((gs.clk & 1u) && (gs.rec >= 2u || (gs.rec && gs.play)))   /* round FLOW-1: the click sounds: a metronome */
        gx_path(&GX_ID, "M159.5 23 L163 7.5 L168 7.5 L171.5 23 Z M165.5 20 L170 11", GX_NONE, ink, Q(1.4));
    if (gs.mark && !gs.rec) {                         /* round 25: saved, safe to switch off (the armed disc's place) */
        gx_xf_t ck = GX_XF(175.5, 8.5, 0.5);
        gx_path(&ck, GA_CHECK, GX_NONE, ink, Q(2.4));
    }
}

static uint32_t gs_head_sig(void)
{
    return gs.headsig * 7u + gs.trk * 131u + (uint32_t)gs.bpm * 1031u + gs.clock + gs.rec * 17u +
           (gs.rec && gs.rec != 3u ? gs.blink * 5u : 0u) + gs.ring * 400009u + gs.mark * 524287u + (gs.clk & 1u) * 8191u + 1u;
}
static void parts_head(void) { gs_part(0, 0, 240, 31, gs_head_sig()); }

/* the knob row's ground and its keyline (drawn with the whole screen) */
static void gs_row(void)
{
    if (!gx_visible(0, 29, 240, 71))
        return;
    gx_rect(0, Q(30.75), Q(240), Q(38.5), GX_PAPER);
    gx_rect(0, Q(69.25), Q(240), Q(1.5), GX_INK);
}

/* the four bubbles (FM1-Easy-Play): Bright, Wobble, Echo, Space, each in its knob's colour, bigger is
 * more. Resting (another sound, which the knobs do not move): empty wells in the second ink */
static void gs_card(uint32_t k, const char *label, const char *value);
static void gs_knob_text(uint32_t k, char *label, char *value);
static void gs_knobs(void)
{
    static const int32_t CX[4] = {Q(34.5), Q(91.5), Q(148.5), Q(205.5)};
    uint32_t k, ink = gs.idle ? GX_INK2 : GX_INK;
    gs_row();
    if (!gx_visible(0, 31, 240, 69))
        return;
    for (k = 0; k < 4u; k++) {
        gx_xf_t t = {CX[k] - Q(7), GS_ROW_CY + Q(20), GX_S14};
        if (gs.role[k] != KR_SOUND) {                /* round 8: the card of what the knob does now */
            char l[12], v[12];
            gs_knob_text(k, l, v);
            gs_card(k, l, v);
            continue;
        }
        if (!gx_visible((CX[k] >> GX_Q) - 19, 31, (CX[k] >> GX_Q) + 20, 69))
            continue;
        gx_circle_s(CX[k], GS_ROW_CY, Q(17), GX_GROUND, GX_NONE, 0);
        if (!gs.idle)
            gx_circle_s(CX[k], GS_ROW_CY, (int32_t)gs.r[k] * 4, GS_FIELD[k], GX_INK, Q(1.5));
        gx_circle_s(CX[k], GS_ROW_CY, Q(17), GX_NONE, ink, (gs.lit >> k) & 1u ? Q(3) : gs.idle ? Q(1.25) : Q(1.75));
        t.ty = GS_ROW_CY - Q(7);
        switch (gs.icon[k]) {                        /* (round MF: the sign of what it changes; none for the rest) */
        case 0:
            gx_circle(&t, Q(12), Q(12), Q(4.5), GX_NONE, ink, Q(2.6));
            gx_path(&t, GA_SUN_RAYS, GX_NONE, ink, Q(2.6));
            break;
        case 1:
            gx_path(&t, GA_WAVE, GX_NONE, ink, Q(2.6));
            break;
        case 2:
            gx_circle(&t, Q(5), Q(12), Q(3), GX_NONE, ink, Q(2.6));
            gx_circle(&t, Q(12.5), Q(12), Q(2.2), GX_NONE, ink, Q(2.6));
            gx_circle(&t, Q(19), Q(12), Q(1.4), GX_NONE, ink, Q(2.6));
            break;
        case 3:
            gx_path(&t, GA_ARCH, GX_NONE, ink, Q(2.6));
            break;
        default:
            break;
        }
    }
}

static uint32_t gs_card_sig(const char *label, const char *value);
static void parts_knobs(void)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) {                        /* (each knob's box covers its card too: K4's is wider) */
        uint32_t sig = (gs.idle ? 1u : gs.r[k] + 2u) + gs.icon[k] * 0x01000000u;
        if (gs.role[k] != KR_SOUND) {
            char l[12], v[12];
            gs_knob_text(k, l, v);
            sig = gs_card_sig(l, v) | 0x80000000u;
        }
        sig ^= ((gs.lit >> k) & 1u) * 0x40000000u;     /* (round SEQ-1: lit while it turns) */
        if (k == 3u)
            gs_part(177, 31, 236, 69, sig);
        else
            gs_part(3 + 58 * (int32_t)k, 31, 61 + 58 * (int32_t)k, 69, sig);
    }
}

/* four labelled cards (the loop page, the volumes): knob k's colour, its label above, its value below */
static void gs_card(uint32_t k, const char *label, const char *value)
{
    int32_t x = Q(4) + (int32_t)k * Q(58);
    uint32_t c = gs_on(k);
    if (!gx_visible((x >> GX_Q) - 1, 31, (x >> GX_Q) + 56, 69))
        return;
    if ((gs.lit >> k) & 1u)                          /* round SEQ-1: turning, a 3 px ink border */
        gx_rrect(x + Q(0.75), Q(34.75), Q(54.5), Q(30.5), Q(6.25), GS_FIELD[k], GX_INK, Q(3));
    else
        gx_rrect(x, Q(34), Q(56), Q(32), Q(7), GS_FIELD[k], GX_INK, Q(1.5));
    gx_text(x + Q(28), Q(45), &GS_SMALL, 1, label, c);
    {
        static const gx_type_t NARROW = {Q(10), Q(1.5), 30};   /* a long value (REPLACE, STEP 12) */
        static const gx_type_t SMALLER = {Q(8), Q(1.3), 0};    /* a longer one (CLOSED HAT, TAMBOURINE: UX-2) */
        gx_text(x + Q(28), Q(61), gx_text_w(&GS_HEAD, value) <= Q(50) ? &GS_HEAD :
                                  gx_text_w(&NARROW, value) <= Q(50) ? &NARROW : &SMALLER, 1, value, c);
    }
}

static uint32_t gs_card_sig(const char *label, const char *value)
{
    uint32_t h = 2166136261u;
    for (; *label; label++)
        h = (h ^ (uint8_t)*label) * 16777619u;
    for (h ^= 0xFFu; *value; value++)
        h = (h ^ (uint8_t)*value) * 16777619u;
    return h;
}

static void parts_card(uint32_t k, const char *label, const char *value)
{
    gs_part(3 + 58 * (int32_t)k, 31, 61 + 58 * (int32_t)k, 69, gs_card_sig(label, value) ^ ((gs.lit >> k) & 1u) * 0x40000000u);
}

/* the band: whether the loop plays (a triangle) or records (the red disc), and a line of what the buttons
 * do here, each button named in brackets (pick 7) */
/* armed, counting in, recording: the band says so, whatever the page (REC never leaves it) */
static const char *const GS_REC_HINT[4] = {"", "READY: START (PLAY)", "GET READY", "RECORDING  STOP (REC)"};
/* round UX-4: SHIFT shows what it changes on this page before anything is done (the alternates, in words) */
static const char GS_HINT_MUTE[] = "MUTE: FX SCL ENV LFO";
static const char *gs_shift_hint(void)
{
    switch (gs.view) {
    case GV_HOME:
        return GS_HINT_MUTE;
    case GV_STEPS: case GV_RECORD:
        return gs.drums ? "K1 STEP  K2 DRUM  K3 STRENGTH" : "K1 STEP  K2 NOTE  K3 STRENGTH";
    case GV_MIX:
#if GN_ENGINES   /* round EW: no pan on the mono core */
        return GS_HINT_MUTE;
#else
        return "K1 TO K4: PAN  MUTE: FX..LFO";
#endif
    default:
        return GS_HINT_MUTE;
    }
}

static const char *gs_hint(const char *hint)
{
    if (gs.rec == 1u && gs.play)
        return "READY: FROM BAR 1";
    static const char *const UNDID[] = {"NOTHING TO UNDO", "UNDID: LAST TAKE", "UNDID: STEP EDIT", "UNDID: BARS",
                                        "UNDID: SNAP", "UNDID: STEPS"};
    static char chg[] = "TRACK 1 * SOUND CHANGED";
    if (gs.view == GV_TAP && gs.tempo)              /* (round SEQ-1: EDIT held + SELECT says so, not SHIFT's) */
        return hint;
    if (gs.undid)                                    /* round FLOW-1, M3: SAVE held names what it undid, 2 s */
        return UNDID[(gs.undid - 1u) % NELEM(UNDID)];
    if (gs.chg) {                                    /* .. a sound changed while playing: on which track, 1.5 s */
        chg[6] = (char)('1' + gs.trk % NTRK);
        return chg;
    }
    if (gs.clk & 2u) {                               /* round FLOW-1: EDIT held, K3 the click's: on or off, said */
        if (!gs.rec && gs.shift && gs_shift_hint() == GS_HINT_MUTE)   /* (with SHIFT's MUTE where it has it) */
            return gs.clk & 1u ? "CLICK ON (K3)  MUTE: FX..LFO" : "CLICK OFF (K3)  MUTE: FX..LFO";
        return gs.clk & 1u ? "CLICK ON (EDIT + K3)" : "CLICK OFF (EDIT + K3)";
    }
    if (gs.clk & 4u)                                 /* round FLOW-1: GLO held names its two gestures */
        return "FILL (KEYS)  STOP AT BAR (PLAY)";
    if (gs.clk & 8u)                                 /* .. M2: FX held, an audition */
        return "AUDITION (KEYS)  SOUND (K1-K4)";
    if (!gs.rec && gs.shift && gs_shift_hint())
        return gs_shift_hint();
    return gs.rec ? GS_REC_HINT[gs.rec & 3u] : hint;
}

/* round UX-5: the band is the status strip, on every page: what the buttons do here (in brackets, the first line),
 * then where you are and what the loop does (PLAYING, STOPPED, ARMED, COUNT-IN, RECORDING; HOME NOTES SOUND MIX
 * TEMPO) and whether the music is saved: SAVED only while it is what the last save read back from flash held (the
 * panel's SAVE or the app's), else ON SYNTH (it is on the synth, not saved). Never SAVED on a guess */
static const char *gs_where(void)
{
    switch (gs.view) {
    case GV_HOME: return "HOME";
    case GV_LOOP: case GV_STEPS: case GV_RECORD: case GV_COUNT: return "NOTES";
    case GV_MIX: return "MIX";
    case GV_TAP: return "TEMPO";
    case GV_GROOVES: return "GROOVES";
    default: return "SOUND";
    }
}

/* round FLOW-1: what records, in words with the track (or the count-in's beat): ARMED T1, COUNT-IN 3, RECORDING T1, in
 * the header and the band alike; a queued transport replaces only the word PLAYING (b holds 32) */
static void gs_rec_words(char *b)
{
    uint32_t n;
    str_cpy(b, gs.rec == 3u ? "RECORDING T" : gs.rec == 2u ? "COUNT-IN " : "ARMED T", 32);
    n = str_len(b);
    b[n] = (char)('0' + (gs.rec == 2u ? gs.count % 10u : gs.trk % NTRK + 1u));
    b[n + 1u] = 0;
}
static void gs_state_words(char *b)               /* b holds 32 */
{
    if (gs.rec)
        gs_rec_words(b);
    else
        str_cpy(b, gs.tq == GN_TQ_RESTART ? "RESTART NEXT BAR" : gs.tq && gs.tile_q ? "GROOVE NEXT BAR" :
                   gs.tq ? "STOP AT BAR END" : gs.play ? "PLAYING" : "STOPPED", 32);
    str_cpy(b + str_len(b), " * ", 32 - str_len(b));
    str_cpy(b + str_len(b), gs.rec == 1u || gs.rec == 3u ? (gs.replace ? "REPLACE" : "KEEP") : gs_where(),
            32 - str_len(b));                         /* (armed, recording: KEEP or REPLACE, the page is plain) */
}

static void gs_band(const char *hint)
{
    char b[32];
    hint = gs_hint(hint);
    if (!gx_visible(0, 208, 240, 240))
        return;
    gx_rect(0, Q(210.75), Q(240), Q(29.25), GX_YELLOW);
    gx_rect(0, Q(209.25), Q(240), Q(1.5), GX_INK);
    if (gs.play == 2)
        gx_circle_s(Q(15), Q(225), Q(5), GX_RED, GX_INK, Q(1.5));
    else if (gs.play == 1)
        gx_path(&GX_ID, "M11 218.5 L19.5 225 L11 231.5 Z", GX_INK, GX_NONE, 0);
    else
        gx_path(&GX_ID, "M11 218.5 L19.5 225 L11 231.5 Z", GX_NONE, GX_INK, Q(1.5));
    gx_text(Q(130), Q(222), &GS_SMALL, 1, hint, GX_INK);
    gs_state_words(b);
    gx_text(Q(28), Q(235.5), &GS_SMALL, 0, b, GX_INK);
    if (gs.stored) {                                 /* (a check just before the word) */
        gx_xf_t t = {Q(234) - gx_text_w(&GS_SMALL, "SAVED") - Q(11), Q(229.5), 4096};
        gx_path(&t, "M0 2.4l2.4 2.4 4.6-4.8", GX_NONE, GX_INK, Q(1.5));
    }
    gx_text(Q(234), Q(235.5), &GS_SMALL, 2, gs.stored ? "SAVED" : "ON SYNTH", GX_INK);
}

static void parts_band(const char *hint)
{
    char b[32];
    gs_state_words(b);
    gs_part(0, 209, 240, 240, gs_card_sig(gs_hint(hint), b) + gs.play * 3u + gs.stored * 977u + 1u);
}

/* the picture's name on its field (Easy's sound: the picture's; another sound: its own), and its place among
 * the nine pictures */
static void gs_name(uint32_t pic, uint32_t line)
{
    uint32_t i;
    if (!gx_visible(16, 178, 224, 206))
        return;
    gx_text(Q(120), Q(194), &GS_NAME, 1, pic < GN_NPICS ? GN_PIC_NAME[pic] : gs.name, line);
    for (i = 0; i < GN_NPICS; i++) {
        int32_t x = Q(120) - (int32_t)(GN_NPICS - 1u) * Q(3) + (int32_t)i * Q(6);   /* (centred: round CR) */
        if (i == pic)
            gx_circle_s(x, Q(202), Q(2), line, GX_NONE, 0);
        else
            gx_circle_s(x, Q(202), Q(1.6), GX_NONE, line, Q(1.5));
    }
}

/* the field: colour c from the knob row's keyline to the band's */
static void gs_field(uint32_t c)
{
    if (gx_visible(0, 69, 240, 211))
        gx_rect(0, Q(70.75), Q(240), Q(138.5), c);
}

/* the pictures (Easy-Sounds), in lines of colour c, placed by t; their filled parts too */
static void gs_picture(const gx_xf_t *t, uint32_t sound, uint32_t c, int32_t sw)
{
    uint32_t i, r;
    switch (sound) {
    case 1:
        gx_path(t, GA_ORGAN, GX_NONE, c, sw);
        break;
    case 2:
        gx_path(t, GA_BELLS, GX_NONE, c, sw);
        break;
    case 3:
        gx_path(t, GA_LASER, GX_NONE, c, sw);
        for (r = 0; r < 8u; r++)
            for (i = 0; i < 11u; i++)
                if ((GA_INVADER[r] >> (10u - i)) & 1u) {
                    int32_t x = t->tx + (int32_t)(((int64_t)((17 + 8 * (int32_t)i) * 64) * t->s) >> 12);
                    int32_t y = t->ty + (int32_t)(((int64_t)((27 + 8 * (int32_t)r) * 64) * t->s) >> 12);
                    gx_rect(x, y, (Q(6) * t->s) >> 12, (Q(6) * t->s) >> 12, c);
                }
        break;
    case 4: {   /* the Whale where FM1-Easy-Sounds puts him: (70, 74) at .8 where the others are (68, 72) at .87 */
        gx_xf_t w;
        w.tx = t->tx + Q(2) * t->s / 3564;
        w.ty = t->ty + Q(2) * t->s / 3564;
        w.s = t->s * 3277 / 3564;
        gx_path(&w, GA_WHALE_SPOUT, GX_NONE, c, sw * 87 / 80);
        gx_circle(&w, Q(30), Q(7), Q(2), GX_NONE, c, sw * 87 / 80);
        gx_circle(&w, Q(40), Q(5), Q(2), GX_NONE, c, sw * 87 / 80);
        gx_circle(&w, Q(50), Q(7), Q(2), GX_NONE, c, sw * 87 / 80);
        gx_path(&w, GA_WHALE_BODY, GX_NONE, c, sw * 87 / 80);
        gx_circle(&w, Q(27), Q(65), Q(2.8), c, GX_NONE, 0);
        gx_path(&w, GA_WHALE_LINES, GX_NONE, c, sw * 87 / 80);
        break;
    }
    case 5:
        gx_path(t, GA_CHOIR, GX_NONE, c, sw);
        gx_circle(t, Q(24.4), Q(38), Q(2.6), c, GX_NONE, 0);
        gx_circle(t, Q(95.4), Q(33), Q(2.6), c, GX_NONE, 0);
        break;
    case 6:
        gx_path(t, GA_CLOUD, GX_NONE, c, sw);
        for (i = 0; i < 9u; i++)
            gx_circle(t, (int32_t)GA_RAIN[i][0] * 64, (int32_t)GA_RAIN[i][1] * 64, (int32_t)GA_RAIN[i][2] * 64 / 10, c,
                      GX_NONE, 0);
        break;
    case 7:
        gx_path(t, GA_PIANO, GX_NONE, c, sw);
        gx_path(t, GA_PIANO_KEYS, c, GX_NONE, 0);
        break;
    case GN_FUNNY:                                   /* FUNNY (round CR): the laughing face */
        gx_path(t, GA_FUNNY, GX_NONE, c, sw);
        gx_path(t, GA_FUNNY_TONGUE, c, GX_NONE, 0);
        break;
    case GN_KIT: {                                   /* the drums: four pads, kick snare hats clap */
        static const uint8_t PAD[4] = {0, 2, 6, 5};
        for (i = 0; i < 4u; i++) {
            int32_t x = Q(12) + (int32_t)(i % 2u) * Q(46), y = Q(16) + (int32_t)(i / 2u) * Q(46);
            gx_xf_t p = {t->tx + (int32_t)(((int64_t)(x + Q(4)) * t->s) >> 12),
                         t->ty + (int32_t)(((int64_t)(y + Q(4)) * t->s) >> 12), t->s * 4 / 3};
            gx_rrect(t->tx + (int32_t)(((int64_t)x * t->s) >> 12), t->ty + (int32_t)(((int64_t)y * t->s) >> 12),
                     (int32_t)(((int64_t)Q(40) * t->s) >> 12), (int32_t)(((int64_t)Q(40) * t->s) >> 12),
                     (int32_t)(((int64_t)Q(8) * t->s) >> 12), GX_NONE, c, (int32_t)(((int64_t)sw * t->s) >> 12));
            gx_path(&p, GA_PAD[PAD[i]], GX_NONE, c, sw * 3 / 4);
        }
        break;
    }
    case GN_OTHER:                                   /* another sound: a wave in a circle */
        gx_path(t, GA_OTHER, GX_NONE, c, sw);
        break;
    default:
        break;
    }
}

/* the Gnome (Gnome-Sheet), placed by t: pose 0 singing (vowel 0 shut, 1..5 A E I O U), 1 cheering */
static void gs_gnome(const gx_xf_t *t, uint32_t vowel, int cheer, int32_t kl)
{
    gx_path(t, GA_G_COAT, GX_BLUE, GX_INK, kl);
    gx_path(t, GA_G_BELT, GX_NONE, GX_INK, kl);
    gx_ellipse(t, Q(47), Q(150), Q(13), Q(6), GX_INK);
    gx_ellipse(t, Q(73), Q(150), Q(13), Q(6), GX_INK);
    gx_path(t, cheer ? GA_G_ARMS_CHEER : GA_G_ARMS_SING, GX_NONE, GX_INK, kl);
    if (cheer) {
        gx_circle(t, Q(26), Q(70), Q(5), GX_GREEN, GX_INK, kl);
        gx_circle(t, Q(94), Q(70), Q(5), GX_GREEN, GX_INK, kl);
    } else {
        gx_circle(t, Q(20), Q(130), Q(5), GX_GREEN, GX_INK, kl);
        gx_circle(t, Q(100), Q(130), Q(5), GX_GREEN, GX_INK, kl);
    }
    gx_path(t, GA_G_FACE, GX_PAPER, GX_INK, kl);
    gx_path(t, GA_G_BEARD, GX_PAPER, GX_INK, kl);
    gx_rrect(t->tx + ((Q(55) * t->s) >> 12), t->ty + ((Q(130) * t->s) >> 12), (Q(10) * t->s) >> 12,
             (Q(8) * t->s) >> 12, (Q(2) * t->s) >> 12, GX_YELLOW, GX_INK, (kl * t->s) >> 12);
    gx_circle(t, Q(46), Q(90), Q(3), GX_RED, GX_NONE, 0);
    gx_circle(t, Q(74), Q(90), Q(3), GX_RED, GX_NONE, 0);
    gx_circle(t, Q(60), Q(88), Q(6), GX_PAPER, GX_INK, kl);
    gx_path(t, GA_G_EYES_SING, GX_NONE, GX_INK, kl);
    switch (vowel) {
    case 1:
        gx_ellipse(t, Q(60), Q(111), Q(5), Q(7), GX_INK);
        break;
    case 2:
        gx_ellipse(t, Q(60), Q(110), Q(8), Q(3.5), GX_INK);
        break;
    case 3:
        gx_path(t, GA_G_MOUTH_I, GX_INK, GX_NONE, 0);
        break;
    case 4:
        gx_circle(t, Q(60), Q(111), Q(5.5), GX_INK, GX_NONE, 0);
        break;
    case 5:
        gx_ellipse(t, Q(60), Q(111), Q(3), Q(3.5), GX_INK);
        break;
    default:
        gx_path(t, GA_G_MOUTH_SHUT, GX_NONE, GX_INK, kl);
        break;
    }
    gx_path(t, GA_G_HAT, GX_RED, GX_INK, kl);
    gx_path(t, GA_G_TIP, GX_NONE, GX_INK, kl);
}

/* a note floating up (FM1-Gnome gnR): its head at (x, y), paper on the field at alpha a */
static void gs_float_note(int32_t x, int32_t y, uint32_t field, uint32_t a)
{
    uint32_t c = gs_mix(GX_PAPER, field, a);
    gx_xf_t t = {x, y, 4096};
    gx_circle_s(x, y, Q(2.2), c, GX_NONE, 0);
    gx_path(&t, GA_NOTE_FLAG, GX_NONE, c, Q(1.5));
}

/* ------------------------------------------------- the sound (round UX-3) --- */
/* Board GN-05 option A, "macros on sound": the selected track's picture in its tile, its name and its place among
 * the nine pictures under it, and the four knobs named, each a bar in its knob's colour as far as it is turned:
 * TONE (Bright), MOTION (Wobble), ECHO, SPACE: the four the sound has (the Gnomad engines' macros, Easy's own sounds',
 * another sound's engine's map). Another sound the knobs do not move: the bars rest, empty. The Gnome is his
 * picture's picture, small in his tile, his mouth singing what he sings (one dry touch) */
static const char *const GS_MACRO[4] = {"TONE", "MOTION", "ECHO", "SPACE"};
static const char GS_HINT_PLAY[] = "SOUNDS (PRESETS)  PAGES (SELECT)";   /* (round FLOW-1, M2: no deep page) */
static const char GS_HINT_OTHER[] = "NOT EASY: PICK ONE (ALGORITHM)";    /* (M3: another sound, PRESETS rests) */
/* round MF: a knob's sign by its name: the sun what brightens, the wave what wobbles, the echoes, the arch the room;
 * a sound's own other knobs (PUNCH, DECAY, ...) have their name beside them and no sign */
static uint32_t gs_icon_of(const char *l)
{
    static const char *const SIGN[] = {"TONE", "BRIGHT", "MOTION", "WOBBLE", "VIBRATO", "ECHO", "SPACE"};
    static const uint8_t ICON[] = {0, 0, 1, 1, 1, 2, 3};
    uint32_t i;
    for (i = 0; i < NELEM(SIGN); i++)
        if (str_eq(l, SIGN[i]))
            return ICON[i];
    return 4u;
}
#define GS_MACRO_Y(k) (82 + 31 * (int32_t)(k))

/* picture pic (0: the Gnome) in a square tile at x, y (1/64 px) of side size, lines of colour line */
static void gs_tile_pic(uint32_t pic, int32_t x, int32_t y, int32_t size, uint32_t line, uint32_t vowel)
{
    if (pic == 0u) {                                 /* the Gnome: 120 x 160 units, 0.86 of the tile high */
        int32_t sc = size * 43 / 125;                    /* = size * 4096 * 86 / (100 * Q(160)) exactly, in 32 bits (no __divdi3 on the chip) */
        gx_xf_t g = {x + size / 2 - (int32_t)(((int64_t)Q(60) * sc) >> 12), y + size * 6 / 100, sc};
        gs_gnome(&g, vowel, 0, Q(1.3) * 4096 / (sc ? sc : 1));
    } else {                                         /* a picture: 100 x 110 units from x 10, 0.8 of the tile high */
        int32_t sc = size * 128 / 275;                   /* = size * 4096 * 80 / (100 * Q(110)) exactly */
        gx_xf_t t = {x + size / 2 - (int32_t)(((int64_t)Q(60) * sc) >> 12), y + size / 10, sc};
        gs_picture(&t, pic, line, Q(1.6) * 4096 / (sc ? sc : 1));
    }
}

static void scene_play(void)
{
    uint32_t c = gs.ink ? GX_INK : GX_PAPER, k, i, pic = gs.pic;
    gs_head();
    gs_knobs();
    gs_field(GX_PAPER);
    if (gx_visible(6, 76, 98, 172)) {
        gx_rrect(Q(8), Q(78), Q(88), Q(92), Q(14), GS_FIELD[gs.field], GX_INK, Q(1.5));
        gs_tile_pic(pic, Q(10), Q(80), Q(84), c, gs.vowel);
    }
    if (gx_visible(4, 174, 100, 204)) {
        gx_text(Q(52), Q(188), &GS_LABEL, 1, pic < GN_NPICS ? GN_PIC_NAME[pic] : gs.name, GX_INK);
        for (i = 0; i < GN_NPICS; i++) {
            int32_t x = Q(52) - (int32_t)(GN_NPICS - 1u) * Q(3) + (int32_t)i * Q(6);
            if (i == pic)
                gx_circle_s(x, Q(198), Q(2), GX_INK, GX_NONE, 0);
            else
                gx_circle_s(x, Q(198), Q(1.6), GX_NONE, GX_INK2, Q(1.25));
        }
    }
    if (gs.idle && gx_visible(102, 76, 238, 206)) {  /* round FLOW-1, M3: not an Easy sound, no knob map: said */
        gx_text(Q(170), Q(98), &GS_LABEL, 1, "NOT AN EASY SOUND", GX_INK);
        gx_text(Q(170), Q(120), &GS_SMALL, 1, "THE KNOBS REST HERE", GX_INK2);
        gx_text(Q(170), Q(162), &GS_SMALL, 1, "TURN ALGORITHM", GX_INK);
        gx_text(Q(170), Q(176), &GS_SMALL, 1, "FOR AN EASY SOUND", GX_INK);
    }
    for (k = 0; !gs.idle && k < 4u; k++) {           /* the four knobs, named, as far as each is turned */
        int32_t y = GS_MACRO_Y(k), w = Q(128) * gs.amt[k] / GN_STEPS;
        if (!gx_visible(104, y - 1, 238, y + 26))
            continue;
        if (!gs.idle && ((gs.lit >> k) & 1u)) {      /* round MF: turning, its name lit in its colour */
            gx_rrect(Q(104), QI(y - 2), gx_text_w(&GS_SMALL, gs.lbl[k]) + Q(6), Q(13), Q(4), GS_FIELD[k], GX_INK,
                     Q(1));
            gx_text(Q(107), QI(y + 8), &GS_SMALL, 0, gs.lbl[k], gs_on(k));
        } else
            gx_text(Q(106), QI(y + 8), &GS_SMALL, 0, gs.lbl[k], gs.idle ? GX_INK2 : GX_INK);
        gx_rrect(Q(106), QI(y + 12), Q(128), Q(11), Q(5.5), GX_WELL, GX_NONE, 0);
        if (!gs.idle && w > Q(2))
            gx_rrect(Q(106), QI(y + 12), w, Q(11), Q(5.5), GS_FIELD[k], GX_NONE, 0);
        gx_rrect(Q(106), QI(y + 12), Q(128), Q(11), Q(5.5), GX_NONE, gs.idle ? GX_INK2 : GX_INK, Q(1));
    }
    gs_band(pic < GN_NPICS ? GS_HINT_PLAY : GS_HINT_OTHER);
}

static void parts_play(void)
{
    uint32_t k, field = gs.pic * 7u + gs.field + 1u + gs.namesig;
    parts_head();
    parts_knobs();
    gs_part(0, 70, 240, 210, gs.idle + 1u);
    gs_part(6, 76, 98, 172, field * 131u + gs.vowel);
    gs_part(4, 174, 100, 204, field);
    for (k = 0; k < 4u; k++)
        gs_part(102, GS_MACRO_Y(k) - 3, 238, GS_MACRO_Y(k) + 26, (gs.amt[k] * 3u + gs.idle + 1u) ^
                gs.lblsig * (k + 1u) ^ (!gs.idle && ((gs.lit >> k) & 1u)) * 0x40000000u);
    parts_band(gs.pic < GN_NPICS ? GS_HINT_PLAY : GS_HINT_OTHER);
}

/* ---------------------------------------------- the picture wheel (round UX-3) --- */
/* ALGORITHM (board GN-05 A): the pictures as a wheel, the one picked in the middle, big, its neighbours smaller to
 * each side (the outer ones half off the screen), each in its colour; under it the picture's name and how many sounds
 * it holds (PRESETS goes through them), then the other pictures in the wheel's order */
static const char GS_HINT_PICK[] = "TURN ALGORITHM: PICTURE";   /* (round FLOW-1, M2: the wheel stays) */
static const char GS_HINT_PEEK[] = "TURN PRESETS: SOUND";       /* (.. and the sound list) */
static void scene_pick(void)
{
    static const int8_t ORD[5] = {-2, 2, -1, 1, 0};  /* the outer tiles first, the middle one last, on top */
    uint32_t cur = gs.pic % GN_NPICS, i;
    char b[32];
    gs_head();
    gs_knobs();
    gs_field(GX_GROUND);
    for (i = 0; i < 5u; i++) {
        int32_t d = ORD[i], ad = d < 0 ? -d : d;
        uint32_t pic = (cur + 2u * GN_NPICS + (uint32_t)d) % GN_NPICS;   /* (round CR: any count of pictures) */
        int32_t size = ad == 0 ? Q(66) : ad == 1 ? Q(46) : Q(34), cx = Q(120) + d * (ad == 2 ? Q(51) : Q(60));
        int32_t x = cx - size / 2, y = Q(113) - size / 2;
        if (!gx_visible((x >> GX_Q) - 2, (y >> GX_Q) - 2, ((x + size) >> GX_Q) + 3, ((y + size) >> GX_Q) + 3))
            continue;
        gx_rrect(x, y, size, size, size / 5, GS_FIELD[gs_pic_field(pic)], GX_INK, ad ? Q(1.25) : Q(2.5));
        gs_tile_pic(pic, x, y, size, gs_on(gs_pic_field(pic)), 1);
    }
    if (gs.pic < GN_NPICS) {
        str_cpy(b, GN_PIC_NAME[cur], sizeof b);
        str_cpy(b + str_len(b), " * ", sizeof b - str_len(b));
        fmt_int(b + str_len(b), GN_GROUP[cur + 1u] - GN_GROUP[cur]);
        str_cpy(b + str_len(b), " SOUNDS", sizeof b - str_len(b));
    } else {
        str_cpy(b, gs.name, sizeof b);
    }
    if (gx_visible(0, 152, 240, 206)) {
        uint32_t line;
        gx_text(Q(120), Q(166), &GS_LABEL, 1, b, GX_INK);
        for (line = 0; line < 2u; line++) {          /* the others, in the wheel's order */
            b[0] = 0;
            for (i = 1u + line * 4u; (i <= 4u + line * 4u || line) && i < GN_NPICS; i++) {   /* (the rest on line 2) */
                if (b[0])
                    str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
                str_cpy(b + str_len(b), GN_PIC_NAME[(cur + i) % GN_NPICS], sizeof b - str_len(b));
            }
            gx_text(Q(120), QI(184 + 14 * (int32_t)line), &GS_SMALL, 1, b, GX_INK2);
        }
    }
    gs_band(GS_HINT_PICK);
}

static void parts_pick(void)
{
    parts_head();
    parts_knobs();
    gs_part(0, 70, 240, 210, gs.pic + 1u + gs.namesig * 16u);   /* (another sound: its name too) */
    parts_band(GS_HINT_PICK);
}

/* ------------------------------------------------------ the peek list --- */
#define GS_PEEK_Y0(n) (74 + (7 - (int32_t)(n)) * 19 / 2)
/* the grooves' list (gn_grooves.c): PRESETS turned on the loop page; the peek list's look (board GN-05 A, gn/ux
 * catch-up: rows across the page), the five grooves, the one loaded inverted; the band says SAVE FIRST when the turn was refused */
#if GN_DEMO_GROOVES
static int gs_grooves(void) { return gn.page == GP_LOOP && (gn.grv & GN_GV_LIST); }
static const char *gs_gv_hint(void) { return gs.grv & GN_GV_NO ? "SAVE FIRST (SAVE)" : "GROOVES (PRESETS)"; }
static void scene_grooves(void)
{
    uint32_t i, c = gs_on(gs.field), cur = (gs.grv & GN_GV_NUM) - 1u;
    gs_head();
    gs_knobs();
    gs_field(GS_FIELD[gs.field]);
    for (i = 0; i < GN_NGROOVES; i++) {
        int32_t y = QI(GS_PEEK_Y0(GN_NGROOVES)) + (int32_t)i * Q(19);
        if (!gx_visible(4, (y >> GX_Q) - 1, 236, (y >> GX_Q) + 19))
            continue;
        if (i == cur)
            gx_rrect(Q(8), y, Q(224), Q(18), Q(6), GX_INK, GX_NONE, 0);
        gx_text(Q(18), y + Q(13.5), &GS_LABEL, 0, GN_GROOVES[i].name, i == cur ? GX_PAPER : c);
    }
    gs_band(gs_gv_hint());
}

static void parts_grooves(void)
{
    uint32_t i, cur = (gs.grv & GN_GV_NUM) - 1u;
    parts_head();
    parts_knobs();
    gs_part(0, 70, 240, 210, 32u + gs.field);      /* (the selected track's colour) */
    for (i = 0; i < GN_NGROOVES; i++)
        gs_part(4, GS_PEEK_Y0(GN_NGROOVES) + (int32_t)i * 19 - 1, 236, GS_PEEK_Y0(GN_NGROOVES) + (int32_t)i * 19 + 20,
                (i == cur) + 1u);
    parts_band(gs_gv_hint());
}
#else
static int gs_grooves(void) { return 0; }
#endif

/* PRESETS (board GN-05 A): the picture's sounds, a row each across the page, the one playing now inverted; 2 s after
 * the last turn the page it came from is back (the sound stays: PRESETS loaded it) */
#define GS_PEEK_Y0(n) (74 + (7 - (int32_t)(n)) * 19 / 2)
/* (round CR: FUNNY's eight rows: 17 px apart from y 72, the band below untouched; up to seven as before) */
#define GS_PEEKS_DY(n) ((n) > 7u ? 17 : 19)
#define GS_PEEKS_Y0(n) ((n) > 7u ? 72 : GS_PEEK_Y0(n))
static void scene_peek(void)
{
    uint32_t pic = gs.pic % GN_NPICS, n0 = GN_GROUP[pic], n = GN_GROUP[pic + 1u] - n0, i;
    uint32_t c = gs_on(gs.field);
#if GN_DEMO_GROOVES
    if (gs.grv & GN_GV_LIST) {
        scene_grooves();
        return;
    }
#endif
    gs_head();
    gs_knobs();
    gs_field(GS_FIELD[gs.field]);
    for (i = 0; i < n; i++) {
        int32_t y = QI(GS_PEEKS_Y0(n)) + (int32_t)i * QI(GS_PEEKS_DY(n));
        if (!gx_visible(4, (y >> GX_Q) - 1, 236, (y >> GX_Q) + GS_PEEKS_DY(n)))
            continue;
        if (n0 + i == gs.snd)
            gx_rrect(Q(8), y, Q(224), QI(GS_PEEKS_DY(n) - 1), Q(6), GX_INK, GX_NONE, 0);
        gx_text(Q(18), y + Q(13.5) - QI(19 - GS_PEEKS_DY(n)) / 2, &GS_LABEL, 0, GN_SOUNDS[n0 + i].name, n0 + i == gs.snd ? GX_PAPER : c);
    }
    gs_band(GS_HINT_PEEK);
}

static void parts_peek(void)
{
    uint32_t pic = gs.pic % GN_NPICS, n0 = GN_GROUP[pic], n = GN_GROUP[pic + 1u] - n0, i;
#if GN_DEMO_GROOVES
    if (gs.grv & GN_GV_LIST) {
        parts_grooves();
        return;
    }
#endif
    parts_head();
    parts_knobs();
    gs_part(0, 70, 240, 210, pic + 1u);
    for (i = 0; i < n; i++)
        gs_part(4, GS_PEEKS_Y0(n) + (int32_t)i * GS_PEEKS_DY(n) - 1, 236, GS_PEEKS_Y0(n) + (int32_t)i * GS_PEEKS_DY(n) + 20, (n0 + i == gs.snd) + 1u);
    parts_band(GS_HINT_PEEK);
}

/* -------------------------------------------- the loop's pages (round UX-2) --- */
/* Board GN-02, option A: one row language on every page, left to right, no circle. The loop: a block a bar (a row of
 * up to four; two rows for eight), each with the bar's sixteen places marked where something sounds, the bar playing
 * in the track's colour with the playhead in it; under them what the loop is (its bars, playing or stopped) and how
 * it records (SNAP, KEEP or REPLACE). Its knobs: BARS SWING SNAP VOLUME (pick 7) */
static const char GS_HINT_LOOP[] = "STEPS (SEQ)  HOME (HOME)";
static const char GS_HINT_STEPS[] = "WRITE (SEQ)  HOME (HOME)";

/* the loop pages' cards (pick 7): BARS SWING SNAP VOLUME, each in its knob's colour; the steps page: BAR (the bar
 * switch) and, on a kit, DRUM (round UX-2); the record screen stopped: MOVE and NOTE / REST (a kit: DRUM, NOTE /
 * REST); anywhere armed or recording K4 KEEP / REPLACE */
static void gs_loop_card(uint32_t k, char *label, char *value)
{
    static const char *const L[4] = {"BARS", "SWING", "SNAP", "VOLUME"};
    str_cpy(label, L[k & 3u], 12);
    value[0] = '-';
    value[1] = 0;
    if (gs.role[k] == KR_MOVE) {
        str_cpy(label, k ? "NOTE/REST" : "MOVE", 12);
        if (k)
            str_cpy(value, gs.curnote ? "NOTE" : "REST", 8);
        else {                                       /* (the steps page counts its places: UX-4) */
            str_cpy(value, "STEP ", 8);
            fmt_int(value + 5, gs.view == GV_STEPS && gs.ccur != 0xFFu ? (int32_t)gs.bar * gs.cells + gs.ccur + 1
                                                                        : (int32_t)gs.bar * gs.spb + gs.cur + 1);
        }
        return;
    }
    if (gs.role[k] == KR_DRUM) {
        str_cpy(label, "DRUM", 12);
        str_cpy(value, gs.dname ? gs.dname : "-", 12);
        return;
    }
    if (gs.role[k] == KR_BAR) {
        str_cpy(label, "BAR", 12);
        fmt_int(value, gs.bar + 1);
        str_cpy(value + str_len(value), " OF ", 12 - str_len(value));
        fmt_int(value + str_len(value), gs.bars ? gs.bars : 1);
        return;
    }
    if (gs.role[k] == KR_CLICK_ON || gs.role[k] == KR_CLICK_OFF) {   /* round 26: EDIT held */
        str_cpy(label, "CLICK", 12);
        str_cpy(value, gs.role[k] == KR_CLICK_ON ? "ON" : "OFF", 4);
        return;
    }
    if (gs.role[k] == KR_KEEP) {
        str_cpy(label, "K4", 12);
        str_cpy(value, gs.replace ? "REPLACE" : "KEEP", 8);
        return;
    }
    if (gs.role[k] == KR_PITCH || gs.role[k] == KR_VEL) {   /* round UX-4: SHIFT, the cursor's step */
        str_cpy(label, gs.role[k] == KR_PITCH ? "NOTE" : "STRENGTH", 12);
        if (!gs.son)
            return;
        if (gs.role[k] == KR_PITCH)
            note_name(value, gs.snote);
        else
            fmt_int(value, gs.svel);
        return;
    }
    if (gs.role[k] == KR_PEN || gs.role[k] == KR_HNOTE) {   /* round SEQ-1: the pen note; a held step's note */
        str_cpy(label, gs.role[k] == KR_PEN ? "PEN" : "NOTE", 12);
        if (gs.role[k] == KR_PEN)
            note_name(value, gs.lnote);
        else if (gs.son)
            note_name(value, gs.snote);
        return;
    }
    if (gs.role[k] == KR_HVEL || gs.role[k] == KR_HLEN || gs.role[k] == KR_HWHEN) {   /* .. its strength, length, */
        str_cpy(label, gs.role[k] == KR_HVEL ? "STRENGTH" : gs.role[k] == KR_HLEN ? "LENGTH" : "WHEN", 12);   /* when */
        if (!gs.son)
            return;
        if (gs.role[k] == KR_HVEL)
            fmt_int(value, gs.svel ? gs.svel : 100);
        else if (gs.role[k] == KR_HLEN) {
            fmt_int(value, gs.slen);
            str_cpy(value + str_len(value), gs.slen == 1u ? " STEP" : " STEPS", 12 - str_len(value));
        } else
            str_cpy(value, gn_seq_when_word(gs.swhen), 12);
        return;
    }
    if (gs.role[k] == KR_REST) {                      /* .. a knob that rests here */
        str_cpy(label, "-", 12);
        return;
    }
    if (k == 1u || k == 3u) {
        fmt_int(value, k == 1u ? gs.swing : gs.vol);
        str_cpy(value + str_len(value), "%", 2);
    } else if (gs.grid) {
        if (k == 0u)
            fmt_int(value, gs.bars);
        else
            str_cpy(value, gs.snap ? "ON" : "OFF", 4);
    }
}

/* the card's words for knob k by what it does now (gs.role) */
static void gs_mix_card(uint32_t k, char *label, char *value);
static void gs_knob_text(uint32_t k, char *label, char *value)
{
    if (gs.role[k] == KR_MIX || gs.role[k] == KR_PAN)
        gs_mix_card(k, label, value);
    else
        gs_loop_card(k, label, value);
}

static uint32_t gs_nbars(void) { return gs.bars > 8u ? 8u : gs.bars ? gs.bars : 1u; }   /* blocks shown */

static void gs_bar_box(uint32_t i, int32_t *x, int32_t *y, int32_t *w, int32_t *h)   /* bar i's block, 1/64 px */
{
    uint32_t n = gs_nbars(), per = n > 4u ? 4u : n;
    *w = (Q(224) - (int32_t)(per - 1u) * Q(6)) / (int32_t)per;
    *h = n > 4u ? Q(38) : Q(60);
    *x = Q(8) + (int32_t)(i % per) * (*w + Q(6));
    *y = Q(80) + (int32_t)(i / per) * (*h + Q(6));
}

static void gs_loop_words(char *a, char *b)       /* the loop in two lines (each holds 24) */
{
    if (gs.playing) {
        str_cpy(a, "PLAYING: BAR ", 24);
        fmt_int(a + str_len(a), gs.bar + 1);
        str_cpy(a + str_len(a), " OF ", 24 - str_len(a));
        fmt_int(a + str_len(a), gs_nbars());
    } else {
        str_cpy(a, "STOPPED: ", 24);
        fmt_int(a + str_len(a), gs_nbars());
        str_cpy(a + str_len(a), gs_nbars() == 1u ? " BAR" : " BARS", 24 - str_len(a));
    }
    if (!gs.grid) {
        str_cpy(b, "ANOTHER GRID: SEE ADVANCED", 27);
        return;
    }
    str_cpy(b, gs.snap ? "SNAP ON * " : "SNAP OFF * ", 24);
    str_cpy(b + str_len(b), gs.replace ? "REPLACE" : "KEEP", 24 - str_len(b));
}

/* round SEQ-1 (phase 1, layout A): the four tracks as lanes across one bar, the selected
 * lane tall (a melody's notes by pitch, a kit's three most used drums as rows), the others slim with their hits; the
 * bar strip above (BAR 2 OF 4, a block a bar); ONE playhead line through every lane (each lane's place under it in
 * ink). In line: K1 BARS lights the bar blocks blue, K2 SWING the off-beat places green, K3 SNAP the grid yellow,
 * K4 VOLUME the tall lane's level vermilion, while the knob turns and 1.2 s after */
#define GS_LX Q(26)                               /* the lanes' places: x 26 .. 234 */
#define GS_LW Q(208)
static uint32_t gs_lane_n(uint32_t t) { return gs.ln_cells[t] ? gs.ln_cells[t] : 16u; }
static int32_t gs_lane_h(uint32_t t) { return t == gs.trk ? 44 : 18; }
static int32_t gs_lane_y(uint32_t t)
{
    int32_t y = 95;
    uint32_t i;
    for (i = 0; i < t; i++)
        y += gs_lane_h(i) + 4;
    return y;
}
static int32_t gs_place_x(uint32_t n, uint32_t j) { return GS_LX + (int32_t)j * (GS_LW / (int32_t)n); }
static const char GS_HINT_LOOPA[] = "PAGES (SELECT)  STEPS (SEQ)";

/* the knobs lit that light the field: the page's own (BARS SWING SNAP VOLUME, the bar, the pen, the drum); a held
 * step's knobs light the held place, SHIFT's and KEEP / REPLACE only their cards */
static uint32_t gs_flit(void)
{
    uint32_t k, m = 0;
    for (k = 0; k < 4u; k++)
        if (((gs.lit >> k) & 1u) && (gs.role[k] == KR_LOOP || gs.role[k] == KR_BAR || gs.role[k] == KR_PEN ||
                                     gs.role[k] == KR_DRUM))
            m |= 1u << k;
    return m;
}

/* the strip over the lanes: BAR n OF m, a block a bar (K1 BARS turning: outlined blue) */
static void gs_strip(uint32_t bar, uint32_t bars, uint32_t lit)
{
    char b[16];
    uint32_t i, n = bars > 8u ? 8u : bars ? bars : 1u;
    if (!gx_visible(0, 71, 240, 93))
        return;
    str_cpy(b, "BAR ", 16);
    fmt_int(b + 4, (int32_t)bar + 1);
    str_cpy(b + str_len(b), " OF ", 16 - str_len(b));
    fmt_int(b + str_len(b), (int32_t)n);
    gx_text(Q(8), Q(87), &GS_LABEL, 0, b, GX_INK);
    for (i = 0; i < n; i++) {
        int32_t x = Q(236) - (int32_t)(n - i) * Q(19);
        char nb[2] = {(char)('1' + i), 0};
        gx_rrect(x, Q(75), Q(17), Q(15), Q(4), i == bar ? (lit ? GX_BLUE : GX_INK) : GX_PAPER, lit ? GX_BLUE : GX_INK,
                 lit ? Q(2.25) : Q(1.25));
        gx_text(x + Q(8.5), Q(86.5), &GS_SMALL, 1, nb, i == bar ? GX_PAPER : lit ? GX_BLUE : GX_INK);
    }
}

/* the tall melody lane's notes by pitch: lowest at the foot, highest at the top */
static void gs_pitch_range(uint32_t *lo, uint32_t *hi)
{
    uint32_t j;
    *lo = 127u;
    *hi = 0u;
    for (j = 0; j < 16u; j++)
        if (gs.cnote[j] > 1u) {
            if (gs.cnote[j] < *lo)
                *lo = gs.cnote[j];
            if (gs.cnote[j] > *hi)
                *hi = gs.cnote[j];
        }
}

static void gs_lane(uint32_t t)
{
    int32_t y = gs_lane_y(t), h = gs_lane_h(t), w, ylast = QI(y + h / 2 - 4);
    uint32_t n = gs_lane_n(t), j, tall = t == gs.trk, muted = (gs.mute >> t) & 1u, lo, hi;
    char nb[2] = {(char)('1' + t), 0};
    if (!gx_visible(0, y - 3, 240, y + h + 3))
        return;
    gx_rrect(Q(4), QI(y), Q(18), QI(h), Q(4), muted ? GX_PAPER : GS_FIELD[t], GX_INK, Q(1.25));   /* the badge */
    gx_text(Q(13), QI(y) + Q(12.5), &GS_HEAD, 1, nb, muted ? GX_INK2 : gs_on(t));
    if (tall) {                                      /* its level (K4 VOLUME turning: vermilion) */
        int32_t lh = QI(h - 22), fh = lh * (int32_t)(gs.vol > 100u ? 100u : gs.vol) / 100;
        gx_rect(Q(11), QI(y + 18), Q(4), lh, GX_PAPER);
        gx_rect(Q(11), QI(y + 18) + lh - fh, Q(4), fh, (gs_flit() & 8u) ? GX_RED : GX_INK);
        gx_rrect(Q(10.5), QI(y + 18) - Q(0.5), Q(5), lh + Q(1), Q(1), GX_NONE, GX_INK, Q(1));
    }
    gs_pitch_range(&lo, &hi);
    w = GS_LW / (int32_t)n;
    for (j = 0; j < n; j++) {
        int32_t x = gs_place_x(n, j);
        uint32_t bg = j % (n / 4u) ? GX_GROUND : GX_WELL, on = (gs.ln_occ[t] >> j) & 1u, held = (gs.ln_tie[t] >> j) & 1u;
        uint32_t ph = j == gs.ln_col[t], c = ph ? GX_INK : muted ? GX_WELL : GS_FIELD[t];
        if (!gx_visible((x >> GX_Q) - 2, y - 3, ((x + w) >> GX_Q) + 2, y + h + 3))
            continue;
        if ((gs_flit() & 2u) && (j & 1u))               /* K2 SWING: the off-beats green */
            bg = gs_mix(GX_GREEN, bg, 120);
        gx_rrect(x + Q(1), QI(y), w - Q(2), QI(h), Q(2.5), bg, (gs_flit() & 4u) ? GX_YELLOW : GX_NONE, Q(1.5));
        if (!tall) {
            if (on)
                gx_rrect(x + Q(2.5), QI(y + 3), w - Q(5), QI(h - 6), Q(2), c, GX_INK, Q(1));
            else if (held)
                gx_rect(x - Q(1), QI(y + h / 2) - Q(1.5), w + Q(2), Q(3), c);
        } else if (gs.drums) {
            uint32_t r;
            for (r = 0; r < gs.trows; r++)
                if ((gs.trow[r] >> j) & 1u)
                    gx_rrect(x + Q(2), QI(y + 4 + (int32_t)r * 12), w - Q(4), Q(10), Q(2), c, GX_INK, Q(1));
        } else {
            uint32_t v = j < 16u ? gs.cnote[j] : 0u;
            if (v > 1u) {
                int32_t yy = hi > lo ? QI(y + h - 12) - (int32_t)(v - lo) * QI(h - 18) / (int32_t)(hi - lo) : QI(y + h / 2 - 4);
                gx_rrect(x + Q(1.5), yy, w - Q(3), Q(8), Q(2), c, GX_INK, Q(1));
                ylast = yy;
            } else if (v == 1u)
                gx_rect(x - Q(1.5), ylast + Q(2.5), w + Q(3), Q(3), c);
        }
    }
    if (gs.ln_col[gs.trk] != 0xFFu) {                /* the one playhead: the selected track's place, every lane */
        uint32_t ns = gs_lane_n(gs.trk);
        int32_t x = gs_place_x(ns, gs.ln_col[gs.trk]) + GS_LW / (int32_t)ns / 2;
        gx_rect(x - Q(1), QI(y - 2), Q(2), QI(h + 4), gs.rec == 3u ? GX_RED : GX_INK);
    }
}

static void scene_loop(void)
{
    uint32_t t;
    gs_head();
    gs_knobs();
    gs_field(GX_PAPER);
    gs_strip(gs.bar, gs.bars, gs_flit() & 1u);
    for (t = 0; t < NTRK; t++)
        gs_lane(t);
    gs_band(GS_HINT_LOOPA);
}

/* a lane's part g (a quarter of the bar): what is drawn there */
static uint32_t gs_lane_sig(uint32_t t, uint32_t g)
{
    uint32_t n = gs_lane_n(t), q = n / 4u, m = (1u << q) - 1u, sh = g * q, h, j, lo, hi, ns = gs_lane_n(gs.trk);
    uint32_t col = gs.ln_col[gs.trk];
    h = ((gs.ln_occ[t] >> sh) & m) * 3u + ((gs.ln_tie[t] >> sh) & m) * 7919u + n * 13u + (t == gs.trk) * 101u +
        ((gs.mute >> t) & 1u) * 211u + (gs_flit() & 6u) * 1009u + (gs.rec == 3u) * 17u + gs.trk * 31u;
    if (col != 0xFFu && col * 4u / ns == g)
        h += (col + 1u) * 65537u;
    if (gs.ln_col[t] != 0xFFu && gs.ln_col[t] / q == g)
        h += (gs.ln_col[t] + 1u) * 257u;
    if (t == gs.trk) {
        gs_pitch_range(&lo, &hi);
        h = h * 31u + lo * 977u + hi * 7u + gs.drums + gs.trows * 5u;
        for (j = sh; j < sh + q && j < 16u; j++)
            h = h * 31u + gs.cnote[j];
        for (j = 0; j < 3u; j++)
            h = h * 31u + ((gs.trow[j] >> sh) & m);
    }
    return h + 1u;
}

static void parts_loop(void)
{
    uint32_t t, g;
    parts_head();
    parts_knobs();
    gs_part(0, 70, 240, 210, gs.trk * 7u + 1u);       /* the ground: drawn with the screen */
    gs_part(0, 71, 240, 93, gs.bar * 3u + gs.bars * 37u + (gs_flit() & 1u) * 1009u + 1u);
    for (t = 0; t < NTRK; t++) {
        int32_t y = gs_lane_y(t), h = gs_lane_h(t);
        gs_part(0, y - 2, 26, y + h + 2, (t == gs.trk) * 3u + ((gs.mute >> t) & 1u) * 7u +
                (t == gs.trk ? gs.vol * 11u + (gs_flit() & 8u) * 1009u : 0u) + t * 131u + 1u);
        for (g = 0; g < 4u; g++)
            gs_part(26 + 52 * (int32_t)g, y - 2, g == 3u ? 236 : 26 + 52 * (int32_t)(g + 1u), y + h + 2, gs_lane_sig(t, g));
    }
    parts_band(GS_HINT_LOOPA);
}

/* ------------------------------------------------------- the steps (round UX-2) --- */
/* One bar up close, straight (board GN-02 A): over it the bar switch (K1; while the loop plays it follows the
 * playhead, 3 s after a turn). A melody: a row of up to sixteen places (with SNAP off two 32nds to a place), each
 * with the name of the note it begins over it (staggered, so each name has two places' room), a held note shown
 * held. A kit: the list of its drums, eight at a time (the half K2's drum is in), each with its note's name and its
 * row; K2's drum marked. The playhead is a mark under the bar switch */
#define GS_CELL_X(n, j) (Q(8) + (int32_t)(j) * (Q(224) / (int32_t)(n)))
#define GS_DCELL_X(n, j) (Q(96) + (int32_t)(j) * (Q(140) / (int32_t)(n)))
static const gx_type_t GS_LIST = {Q(8), Q(1.2), 0};       /* the drum list's names (TAMBOURINE fits its column) */
static uint32_t gs_ncells(void) { return gs.cells ? gs.cells : 16u; }

static void gs_bar_line(void)                     /* BAR 1 OF 2, the bar switch, the playhead's mark */
{
    char b[16];
    uint32_t i, n = gs_ncells();
    if (!gx_visible(0, 72, 240, 97))
        return;
    str_cpy(b, "BAR ", 16);
    fmt_int(b + 4, gs.bar + 1);
    str_cpy(b + str_len(b), " OF ", 16 - str_len(b));
    fmt_int(b + str_len(b), gs_nbars());
    gx_text(Q(8), Q(88), &GS_LABEL, 0, b, GX_INK);
    for (i = 0; i < gs_nbars(); i++) {
        int32_t x = Q(236) - (int32_t)(gs_nbars() - i) * Q(19);
        char nb[2] = {(char)('1' + i), 0};
        gx_rrect(x, Q(75), Q(17), Q(15), Q(4), i == gs.bar ? GX_INK : GX_PAPER, GX_INK, Q(1.25));
        gx_text(x + Q(8.5), Q(86.5), &GS_SMALL, 1, nb, i == gs.bar ? GX_PAPER : GX_INK);
    }
    if (gs.ccol != 0xFFu) {
        int32_t x = gs.drums ? GS_DCELL_X(n, gs.ccol) + Q(140) / (int32_t)n / 2 : GS_CELL_X(n, gs.ccol) + Q(224) / (int32_t)n / 2;
        gx.nseg = 0;
        gx.open = 0;
        gx_to_s(x, Q(96));
        gx_to_s(x + Q(4), Q(91.5));
        gx_to_s(x - Q(4), Q(91.5));
        gx_sub_end(1);
        gx_fill_paint(gs.rec == 3u ? GX_RED : GX_INK);
    }
}

/* round SEQ-1 (phase 1, layout A's STEPS): the other three tracks as slim strips over it (the playhead keeps
 * its context), then the selected track's bar up close: sixteen places in line with the sixteen white keys (place n
 * over key n), the cursor (the last step tapped) outlined, the playhead's place marked under it, the step numbers
 * 1 5 9 13 under the places. A melody: each note's name over its place, its strength as the block's height; a kit:
 * K2's drum as blocks, a dot for each other drum on that step. A step that plays only sometimes wears a ring. Tap a
 * key: on; a lit one: off; hold it and turn K1..K4: that step's NOTE, STRENGTH, LENGTH, WHEN, the step outlined in the
 * colour of the knob turning */
#define GS_SY(r) (95 + 14 * (int32_t)(r))         /* the strips: three, 11 px each */
#define GS_CY0 137                                /* the places: y 137 .. 187 */
static const char GS_HINT_STEPSA[] = "KEYS: STEPS  HOLD A KEY + KNOB";
static uint32_t gs_strip_track(uint32_t r) { return r < gs.trk ? r : r + 1u; }   /* strip r: the r-th other track */

static void gs_steps_strip(uint32_t r)
{
    uint32_t t = gs_strip_track(r), n = gs_lane_n(t), j;
    int32_t y = GS_SY(r), w = Q(224) / (int32_t)n;
    if (!gx_visible(0, y - 2, 240, y + 13))
        return;
    gx_rrect(Q(2), QI(y), Q(4), Q(11), Q(1.5), GS_FIELD[t], GX_NONE, 0);
    for (j = 0; j < n; j++) {
        int32_t x = GS_CELL_X(n, j);
        uint32_t on = (gs.ln_occ[t] >> j) & 1u, ph = j == gs.ln_col[t];
        gx_rrect(x + Q(1), QI(y), w - Q(2), Q(11), Q(2), j % (n / 4u) ? GX_GROUND : GX_WELL, GX_NONE, 0);
        if (on)
            gx_rrect(x + Q(2), QI(y + 2), w - Q(4), Q(7), Q(1.5), ph ? GX_INK : (gs.mute >> t) & 1u ? GX_WELL : GS_FIELD[t],
                     GX_NONE, 0);
        else if ((gs.ln_tie[t] >> j) & 1u)
            gx_rect(x - Q(1), QI(y + 4), w + Q(2), Q(3), GS_FIELD[t]);
    }
    if (gs.ccol != 0xFFu) {                          /* the playhead through the strips */
        int32_t x = GS_CELL_X(gs_ncells(), gs.ccol) + Q(224) / (int32_t)gs_ncells() / 2;
        gx_rect(x - Q(1), QI(y - 2), Q(2), Q(15), gs.rec == 3u ? GX_RED : GX_INK);
    }
}

static uint32_t gs_hold_colour(void) { return gs.hlit < 4u ? GS_FIELD[gs.hlit] : GX_INK; }


static void gs_steps_cell(uint32_t j)
{
    uint32_t n = gs_ncells(), f = gs.trk % NTRK, on = (gs.pon >> j) & 1u, v = gs.cnote[j];
    int32_t x = GS_CELL_X(n, j), w = Q(224) / (int32_t)n, top;
    if (!gx_visible((x >> GX_Q) - 2, GS_CY0 - 3, ((x + w) >> GX_Q) + 2, 197))
        return;
    gx_rrect(x + Q(1), QI(GS_CY0), w - Q(2), Q(50), Q(3), j % (n / 4u) ? GX_GROUND : GX_WELL,
             (gs_flit() & 4u) ? GX_YELLOW : GX_NONE, Q(1.5));
    top = QI(GS_CY0 + 50) - (Q(10) + Q(18) * (int32_t)(gs.pvel[j] ? gs.pvel[j] : 100u) / 127);   /* the strength */
    if (on) {
        gx_rrect(x + Q(2), top, w - Q(4), QI(GS_CY0 + 49) - top, Q(2.5), GS_FIELD[f], GX_INK, Q(1.25));
        if ((gs.pmaybe >> j) & 1u)                   /* only sometimes */
            gx_circle_s(x + w / 2, top + Q(7), Q(3), GX_PAPER, GX_INK, Q(1.25));
    } else if (!gs.drums && v == 1u) {               /* held on from the note before */
        gx_rect(x - Q(1), QI(GS_CY0 + 40), w + Q(2), Q(4), GS_FIELD[f]);
    }
    if (!gs.drums && v > 1u) {                       /* its name over it (staggered: two places' room) */
        char nm[8];
        note_name(nm, v);
        gx_text(x + w / 2, (j & 1u) && n > 8u ? Q(GS_CY0 + 9) : Q(GS_CY0 + 18), &GS_SMALL, 1, nm, GX_INK);
    }
    if (gs.drums && gs.podr[j]) {                    /* the other drums on this step */
        uint32_t k, m = gs.podr[j] > 3u ? 3u : gs.podr[j];
        for (k = 0; k < m; k++)
            gx_circle_s(x + w / 2 + ((int32_t)k - (int32_t)(m - 1u) / 2) * Q(4) - ((m - 1u) & 1u ? Q(2) : 0), Q(GS_CY0 + 6),
                        Q(1.4), GX_INK2, GX_NONE, 0);
    }
    if (gs.hold == j + 1u)                           /* held: outlined in the turning knob's colour */
        gx_rrect(x + Q(1.5), QI(GS_CY0) + Q(0.5), w - Q(3), Q(49), Q(3), GX_NONE, gs_hold_colour(), Q(3));
    else if (gs.ccur == j)                           /* the cursor */
        gx_rrect(x + Q(1), QI(GS_CY0), w - Q(2), Q(50), Q(3), GX_NONE, GX_INK, Q(2));
    if (gs.ccol == j) {                              /* the playhead's place */
        int32_t cx = x + w / 2;
        gx.nseg = 0;
        gx.open = 0;
        gx_to_s(cx, Q(189));
        gx_to_s(cx + Q(4.5), Q(195.5));
        gx_to_s(cx - Q(4.5), Q(195.5));
        gx_sub_end(1);
        gx_fill_paint(gs.rec == 3u ? GX_RED : GX_INK);
    }
}

static void scene_steps(void)
{
    uint32_t j, n = gs_ncells(), r;
    gs_head();
    gs_knobs();
    gs_field(GX_PAPER);
    if (!gs.grid) {
        gx_text(Q(120), Q(146), &GS_LABEL, 1, "ANOTHER GRID: SEE ADVANCED", GX_INK2);
        gs_band(GS_HINT_STEPS);
        return;
    }
    gs_strip(gs.bar, gs.bars, gs_flit() & 1u);
    if (gx_visible(80, 71, 170, 93)) {               /* what a key writes: K2's drum, or the pen note */
        char b[20];
        if (gs.drums)
            str_cpy(b, gs.dname ? gs.dname : "-", 20);
        else {
            str_cpy(b, "PEN ", 20);
            note_name(b + 4, gs.lnote);
        }
        if (gs_flit() & 2u)
            gx_rrect(Q(122) - gx_text_w(&GS_SMALL, b) / 2 - Q(5), Q(76), gx_text_w(&GS_SMALL, b) + Q(10), Q(14), Q(7),
                     GX_GREEN, GX_NONE, 0);
        gx_text(Q(122), Q(86.5), &GS_SMALL, 1, b, (gs_flit() & 2u) ? GX_PAPER : GX_INK2);
    }
    for (r = 0; r < 3u; r++)
        gs_steps_strip(r);
    for (j = 0; j < n; j++)
        gs_steps_cell(j);
    if (gx_visible(0, 196, 240, 209))
        for (j = 0; j < n; j += n / 4u) {
            char b[4];
            fmt_int(b, (int32_t)j + 1);
            gx_text(GS_CELL_X(n, j) + Q(224) / (int32_t)n / 2, Q(205), &GS_SMALL, 1, b, GX_INK2);
        }
    gs_band(GS_HINT_STEPSA);
}

static uint32_t gs_bar_sig(void)
{
    uint32_t i, h = gs.bar * 7u + gs.drums * 3u + gs.lanes * 131u + gs.spb + gs.grid * 977u + gs.bars * 13u;
    for (i = 0; i < 32u; i++)
        h = h * 31u + gs.pitch[i] * 3u + gs.lane[i];
    return h;
}

static void parts_steps(void)
{
    uint32_t j, r, n = gs_ncells();
    char b[20];
    parts_head();
    parts_knobs();
    gs_part(0, 70, 240, 210, gs.grid + gs.drums * 2u + 1u);
    if (!gs.grid) {
        parts_band(GS_HINT_STEPS);
        return;
    }
    if (gs.drums)
        str_cpy(b, gs.dname ? gs.dname : "-", 20);
    else
        note_name(b, gs.lnote);
    gs_part(0, 71, 240, 93, gs.bar * 3u + gs.bars * 37u + (gs_flit() & 3u) * 1009u + gs_card_sig(b, "") + 1u);
    for (r = 0; r < 3u; r++) {
        uint32_t t = gs_strip_track(r);
        gs_part(0, GS_SY(r) - 2, 240, GS_SY(r) + 13, gs.ln_occ[t] * 3u + gs.ln_tie[t] * 7919u + gs_lane_n(t) * 13u +
                (gs.ln_col[t] + 1u) * 257u + (gs.ccol + 1u) * 65537u + ((gs.mute >> t) & 1u) * 211u + t + gs.rec * 5u + 1u);
    }
    for (j = 0; j < n; j++) {
        int32_t x = GS_CELL_X(n, j) >> GX_Q, w = (Q(224) / (int32_t)n) >> GX_Q;
        uint32_t h = ((gs.pon >> j) & 1u) * 3u + ((gs.pmaybe >> j) & 1u) * 5u + gs.pvel[j] * 7u + gs.cnote[j] * 1031u +
                     gs.podr[j] * 13u + (gs.hold == j + 1u) * (gs_hold_colour() + 7u) * 3u + (gs.ccur == j) * 977u +
                     (gs.ccol == j) * (gs.rec == 3u ? 300007u : 200003u) + (gs_flit() & 4u) * 1009u + n * 17u + gs.drums + gs.trk * 31u;
        gs_part(x - 1, GS_CY0 - 2, x + w + 2, 197, h + 1u);
    }
    gs_part(0, 197, 240, 209, n + 1u);
    parts_band(GS_HINT_STEPSA);
}

/* ------------------------------------------------ the record screen (choice 10) --- */
/* Playing (option A): hold REC to record into the selected track, let go to stop; K4 KEEP or REPLACE; the bars of the
 * loop, the one playing in the track's colour, a dot for each step that sounds. Stopped (round UX-2, board GN-02 A's
 * step entry): STEP n and NOTE / REST (what the step at the cursor holds), the note NOTE writes (a melody: the last a
 * key wrote) or the drum K2 picked (a kit), the bar's steps with the cursor under them; a key writes its note (a kit:
 * its drum) at the cursor and the cursor moves on; K1 moves it; NOTE / REST (a melody's K2, a kit's K3) writes in
 * place (round 24: it never moves the cursor) */
static void scene_record(void)
{
    uint32_t f = gs.trk % NTRK, i, n = gs_nbars();
    gs_head();
    gs_knobs();
    gs_field(GX_PAPER);
    if (gs.playing) {
        char r[16] = "RECORD INTO T1";
        r[13] = (char)('1' + f);                      /* (round FLOW-1: the track it records) */
        gx_text(Q(8), Q(88), &GS_LABEL, 0, r, GX_INK);
        gx_rrect(Q(146), Q(76), Q(88), Q(18), Q(9), gs.punch || gs.rec == 3u ? GX_RED : GX_PAPER, GX_INK, Q(1.3));
        gx_text(Q(190), Q(89), &GS_SMALL, 1, gs.punch ? "HOLDING (REC)" : gs.rec == 3u ? "RECORDING" : "HOLD (REC)", GX_INK);
    }
    if (!gs.grid) {
        gx_text(Q(120), Q(146), &GS_LABEL, 1, "ANOTHER GRID: SEE ADVANCED", GX_INK2);
    } else if (gs.playing) {
        int32_t w = (Q(224) - (int32_t)(n - 1u) * Q(4)) / (int32_t)n;
        for (i = 0; i < n; i++) {
            int32_t x = Q(8) + (int32_t)i * (w + Q(4));
            uint32_t j, cur = i == gs.bar;
            gx_rrect(x, Q(102), w, Q(60), Q(6), cur ? GS_FIELD[f] : GX_GROUND, cur ? GX_INK : GX_INK2, Q(1));
            for (j = 0; j < 16u; j++)
                if ((gs.occ[i] >> j) & 1u)
                    gx_circle_s(x + Q(4) + (int32_t)j * (w - Q(8)) / 15, Q(132), Q(2.2), GX_PAPER, GX_INK, Q(1));
            if (n <= 4u) {
                char b[8] = "BAR ";
                fmt_int(b + 4, (int32_t)i + 1);
                gx_text(x + w / 2, Q(176), &GS_SMALL, 1, b, GX_INK);
            }
        }
        if (gs.col != 0xFFu) {
            int32_t x = Q(8) + (int32_t)gs.bar * (w + Q(4)) + Q(4) + (int32_t)gs.col * (w - Q(8)) / (int32_t)(gs.spb - 1u);
            gx_rect(x - Q(1.5), Q(96), Q(3), Q(72), GX_RED);
        }
        gx_text(Q(120), Q(200), &GS_LABEL, 1, "ONLY WHILE YOU HOLD (REC)", GX_INK);
    } else {
        uint32_t cols = gs.spb > 32u ? 32u : gs.spb;
        int32_t cw = Q(224) / (int32_t)cols;
        char b[24];
        str_cpy(b, "T1 * STEP ", 24);
        b[1] = (char)('1' + f);                       /* (round FLOW-1: the track the keys write) */
        fmt_int(b + 10, (int32_t)gs.bar * gs.spb + gs.cur + 1);
        gx_text(Q(8), Q(89), &GS_LABEL, 0, b, GX_INK);
        gx_rrect(Q(140), Q(76), Q(46), Q(18), Q(6), gs.curnote ? GX_INK : GX_PAPER, GX_INK, Q(1.3));
        gx_text(Q(163), Q(89), &GS_SMALL, 1, "NOTE", gs.curnote ? GX_PAPER : GX_INK);
        gx_rrect(Q(190), Q(76), Q(46), Q(18), Q(6), gs.curnote ? GX_PAPER : GX_INK, GX_INK, Q(1.3));
        gx_text(Q(213), Q(89), &GS_SMALL, 1, "REST", gs.curnote ? GX_INK : GX_PAPER);
        if (gs.drums) {
            str_cpy(b, "DRUM: ", 24);
            str_cpy(b + 6, gs.dname ? gs.dname : "-", 18);
            str_cpy(b + str_len(b), " (K2)", 24 - str_len(b));
        } else {
            str_cpy(b, "NOTE: ", 24);
            note_name(b + 6, gs.lnote);
        }
        gx_text(Q(8), Q(110), &GS_LABEL, 0, b, GX_INK);
        for (i = 0; i < cols; i++) {
            int32_t x = Q(8) + (int32_t)i * cw;
            uint32_t used = gs.pitch[i] || gs.lane[i];
            gx_rrect(x + Q(1), Q(118), cw - Q(2), Q(46), Q(3), used ? GS_FIELD[f] : GX_GROUND,
                     i == gs.cur ? GX_INK : GX_INK2, i == gs.cur ? Q(2.5) : Q(1));
        }
        {
            int32_t x = Q(8) + (int32_t)gs.cur * cw + cw / 2;
            gx.nseg = 0;
            gx.open = 0;
            gx_to_s(x, Q(167));
            gx_to_s(x + Q(5), Q(175));
            gx_to_s(x - Q(5), Q(175));
            gx_sub_end(1);
            gx_fill_paint(GX_INK);
        }
        gx_text(Q(120), Q(198), &GS_LABEL, 1, gs.rec == 1u ? "ARMED: PLAY OPENS THE LOOP" :
                                              gs.drums ? "A KEY WRITES ITS DRUM HERE" : "A KEY WRITES ITS NOTE HERE", GX_INK);
    }
    gs_band(gs.playing ? "HOLD (REC)  LOOP (SEQ)" : gs.drums ? "K2 DRUM  K3 NOTE/REST" : "K1 MOVE  K2 NOTE/REST");
}

static void parts_record(void)
{
    uint32_t i, h = gs_bar_sig() + gs.playing * 999u + gs.punch * 77u + gs.rec * 13u + gs.bars * 5u + gs.curnote * 3u +
                    gs.lnote * 257u + gs.drum * 4099u;
    for (i = 0; i < 8u; i++)
        h = h * 31u + gs.occ[i];
    parts_head();
    parts_knobs();
    gs_part(0, 70, 240, 210, h + (gs.playing ? gs.col : gs.cur) * 65537u);
    parts_band(gs.playing ? "HOLD (REC)  LOOP (SEQ)" : gs.drums ? "K2 DRUM  K3 NOTE/REST" : "K1 MOVE  K2 NOTE/REST");
}

/* ---------------------------------------------------- the kit (round UX-3) --- */
/* Board GN-05 option A, "kit, bank A of 8": a kit's pads eight at a time, named, in two rows of four, each in its
 * column's colour and dark while it sounds; under them the banks A (pads 1-8) and B (9-16), the one shown dark (a dot
 * on the other while one of its pads sounds). OCT- and OCT+ show A and B (a kit plays its sixteen pads on the
 * sixteen white keys in every bank: the bank is what the screen shows, it never moves a pad off its key). A kit of
 * eight (Felucca's DRUM engine) has bank A only */
#define GS_PAD_X(i) (8 + 57 * (int32_t)((i) % 4u))
#define GS_PAD_Y(i) (78 + 46 * (int32_t)((i) / 4u))
static void scene_drums(void)
{
    uint32_t i, banks = gs.kn > 8u ? 2u : 1u;
    gs_head();
    gs_knobs();
    gs_field(GX_GROUND);
    for (i = 0; i < 8u && gs.bank * 8u + i < gs.kn; i++) {
        uint32_t id = gs.bank * 8u + i, lit = (gs.pads >> id) & 1u, col = i % 4u;
        int32_t x = QI(GS_PAD_X(i)), y = QI(GS_PAD_Y(i));
        char nb[4];
        if (!gx_visible(GS_PAD_X(i) - 1, GS_PAD_Y(i) - 1, GS_PAD_X(i) + 55, GS_PAD_Y(i) + 42))
            continue;
        gx_rrect(x, y, Q(53), Q(40), Q(8), lit ? GX_INK : GS_FIELD[col], GX_INK, Q(1.5));
        fmt_int(nb, (int32_t)id + 1);
        gx_text(x + Q(5), y + Q(12), &GS_SMALL, 0, nb, lit ? GX_PAPER : gs_on(col));
        gx_text(x + Q(26.5), y + Q(31), &GS_LIST, 1, gs.kname[i] ? gs.kname[i] : "", lit ? GX_PAPER : gs_on(col));
    }
    if (gx_visible(0, 170, 240, 206)) {
        for (i = 0; i < banks; i++) {
            int32_t x = Q(8) + (int32_t)i * Q(46);
            uint32_t other = i != gs.bank && (gs.pads >> (8u * i)) & 0xFFu;
            gx_rrect(x, Q(174), Q(40), Q(26), Q(7), i == gs.bank ? GX_INK : GX_PAPER, GX_INK, Q(1.5));
            gx_text(x + Q(20), Q(192), &GS_HEAD, 1, i ? "B" : "A", i == gs.bank ? GX_PAPER : GX_INK);
            if (other)
                gx_circle_s(x + Q(34), Q(180), Q(2.5), GX_RED, GX_NONE, 0);
        }
        gx_text(Q(104), Q(184), &GS_SMALL, 0, gs.kn > 8u ? "16 PADS ON THE WHITE KEYS" : "8 PADS ON THE WHITE KEYS",
                GX_INK);
        gx_text(Q(104), Q(197), &GS_SMALL, 0, banks > 1u ? "BANK: OCT- A, OCT+ B" : "ONE BANK", GX_INK2);
    }
    gs_band(GS_HINT_PLAY);
}

static void parts_drums(void)
{
    uint32_t i;
    parts_head();
    parts_knobs();
    gs_part(0, 70, 240, 210, gs.kn + 1u);
    for (i = 0; i < 8u && gs.bank * 8u + i < gs.kn; i++) {
        uint32_t id = gs.bank * 8u + i, h = 5u;
        const char *c;
        for (c = gs.kname[i]; c && *c; c++)
            h = h * 31u + (uint8_t)*c;
        gs_part(GS_PAD_X(i) - 1, GS_PAD_Y(i) - 1, GS_PAD_X(i) + 55, GS_PAD_Y(i) + 42, h * 3u + ((gs.pads >> id) & 1u) + id * 7u);
    }
    gs_part(0, 170, 240, 206, gs.bank + gs.kn * 3u + (gs.bank ? gs.pads & 0xFFu : gs.pads >> 8) * 17u + 1u);
    parts_band(gs.pic < GN_NPICS ? GS_HINT_PLAY : GS_HINT_OTHER);
}

/* ---------------------------------------------------- the volumes (GLO) --- */
/* GLO's page (pick 2): the four tracks' volumes on the four knobs, each a card in its knob's colour and a
 * bar in the field; the selected track's bar outlined */
static void gs_pan_text(int32_t v, char *b)       /* L32, C, R31 (round UX-4) */
{
    if (!v) {
        str_cpy(b, "CENTRE", 8);
        return;
    }
    b[0] = v < 0 ? 'L' : 'R';
    fmt_int(b + 1, v < 0 ? -v : v);
}

static void gs_mix_card(uint32_t k, char *label, char *value)
{
    static const char *const BTN[NTRK] = {"(FX)", "(SCL)", "(ENV)", "(LFO)"};
    if (gs.role[k] == KR_PAN) {                       /* (round UX-4: SHIFT, the pan) */
        label[0] = (char)('1' + k);
        str_cpy(label + 1, " PAN", 11);
        gs_pan_text(gs.pan[k % NTRK], value);
        return;
    }
    label[0] = (char)('1' + k);
    label[1] = ' ';
    str_cpy(label + 2, BTN[k % NTRK], 8);
    fmt_int(value, (int32_t)gs.lvl[k] * 100 / 127);
    str_cpy(value + str_len(value), "%", 2);
}

static void scene_mix(void)
{
    uint32_t k;
    gs_head();
    gs_knobs();
    gs_field(GX_GROUND);
    for (k = 0; k < NTRK; k++) {
        int32_t x = Q(19.5) + (int32_t)k * Q(57), h = Q(104) * gs.lvl[k] / 127;
        char b[2] = {(char)('1' + k), 0};
        if (!gx_visible((x >> GX_Q) - 2, 78, (x >> GX_Q) + 32, 206))
            continue;
        gx_rrect(x, Q(80), Q(30), Q(104), Q(6), GX_WELL, GX_NONE, 0);
        if (h > Q(1))
            gx_rrect(x, Q(184) - h, Q(30), h, Q(6), GS_FIELD[k], GX_NONE, 0);
        gx_rrect(x, Q(80), Q(30), Q(104), Q(6), GX_NONE, GX_INK, k == gs.trk ? Q(3) : Q(1.5));
        if (gs.shift && !GN_ENGINES) {               /* (round UX-4: SHIFT, the pan: a dot on a line; EW: none) */
            gx_rect(x, Q(191), Q(30), Q(1.5), GX_INK2);
            gx_circle_s(x + Q(15) + (int32_t)gs.pan[k] * Q(14) / 64, Q(191.75), Q(3.5), GS_FIELD[k], GX_INK, Q(1.25));
        } else {
            gx_text(x + Q(15), Q(202), &GS_HEAD, 1, b, GX_INK);
        }
        if ((gs.mute >> k) & 1u)                     /* (muted: said, not only seen) */
            gx_text(x + Q(15), Q(96), &GS_SMALL, 1, "MUTE", GX_INK);
    }
    gs_band("VOLUMES  BACK (GLO)");
}

static void parts_mix(void)
{
    uint32_t k;
    parts_head();
    parts_knobs();
    for (k = 0; k < NTRK; k++) {
        gs_part(17 + 57 * (int32_t)k, 78, 52 + 57 * (int32_t)k, 206, gs.lvl[k] * 2u + (k == gs.trk) + 1u +
                                     (gs.shift ? (uint32_t)(gs.pan[k] + 64) * 977u + 7u : 0u) + ((gs.mute >> k) & 1u) * 131071u);
    }
    parts_band("VOLUMES  BACK (GLO)");
}

/* ------------------------------------------------------------ Home (round UX-1) --- */
/* Board GN-03, option A: the four tracks at a glance, a row each: its colour and number, its sound, its level,
 * MUTE, and whether it sounds now (a triangle; the record disc while it records, a ring while armed). The header
 * keeps the selected track's badge and the tempo, and says that track's BARS and SWING; the knob row stays the
 * selected sound's four (round 7: SELECT is the tempo, BARS and SWING are the loop page's knobs). Muted, sounding,
 * selected and recording differ by shape and words as well as colour. An empty kit row wears the Gnome's hat until
 * a hit is dropped in (the board's one dry touch, on one row at most) */
#define GS_ROW_Y(k) (73 + 34 * (int32_t)(k))
static const char GS_HINT_HOME[] = "SOUND (HOME)  NOTES (SEQ)";

static void gs_row_live(uint32_t k, int32_t y)    /* the row's last box: sounding, recording, armed or quiet */
{
    uint32_t live = (gs.live >> k) & 1u;
    gx_xf_t t = {Q(207), QI(y + 10), 4096};
    if (!gx_visible(193, y + 4, 229, y + 28))
        return;
    gx_rrect(Q(194), QI(y + 5), Q(34), Q(22), Q(6), live ? GX_INK : GX_PAPER, GX_INK, Q(1.5));
    if (gs.rec == 3u && k == gs.trk)
        gx_circle_s(Q(211), QI(y + 16), Q(6), GX_RED, live ? GX_PAPER : GX_INK, Q(1.5));
    else if ((gs.armed >> k) & 1u)
        gx_circle_s(Q(211), QI(y + 16), Q(5.5), GX_NONE, GX_RED, Q(2.5));
    else if (live)
        gx_path(&t, "M0 0L10 6L0 12Z", GX_PAPER, GX_NONE, 0);
    else
        gx_circle_s(Q(211), QI(y + 16), Q(1.8), GX_INK2, GX_NONE, 0);
}

static void scene_home(void)
{
    uint32_t k;
    gs_head();
    gs_knobs();
    gs_field(GX_PAPER);
    for (k = 0; k < NTRK; k++) {
        int32_t y = GS_ROW_Y(k), w = Q(118) * gs.lvl[k] / 127;
        uint32_t muted = (gs.mute >> k) & 1u;
        char b[2] = {(char)('1' + k), 0};
        if (!gx_visible(0, y - 2, 240, y + 34))
            continue;
        gx_rect(0, QI(y), Q(6), Q(32), GS_FIELD[k]);                       /* the track's colour */
        gx_rect(Q(6), QI(y), Q(1.5), Q(32), GX_INK);
        if (k + 1u < NTRK)
            gx_rect(Q(10), (QI(y) + Q(32.5)), Q(226), Q(1), GX_WELL);
        gx_text(Q(18), QI(y + 21), &GS_HEAD, 1, b, GX_INK);
        gx_text(Q(30), QI(y + 14), &GS_LABEL, 0, gs.row[k], muted ? GX_INK2 : GX_INK);
        gx_rrect(Q(30), QI(y + 19), Q(118), Q(8), Q(4), GX_WELL, GX_NONE, 0);   /* the level */
        if (w > Q(2))
            gx_rrect(Q(30), QI(y + 19), w, Q(8), Q(4), muted ? GX_INK2 : GS_FIELD[k], GX_NONE, 0);
        gx_rrect(Q(30), QI(y + 19), Q(118), Q(8), Q(4), GX_NONE, GX_INK, Q(1));
        gx_rrect(Q(156), QI(y + 5), Q(30), Q(22), Q(6), muted ? GX_INK : GX_PAPER, GX_INK, Q(1.5));   /* MUTE */
        gx_text(Q(171), QI(y + 21), &GS_LABEL, 1, "M", muted ? GX_PAPER : GX_INK);
        gs_row_live(k, y);
        if (k == gs.hat) {                           /* nothing in the kit yet: the Gnome's hat, small */
            gx_xf_t h = GX_XF(134, 0, 0.17);
            h.ty = QI(y) - Q(1);
            gx_path(&h, GA_G_HAT, GX_RED, GX_INK, Q(7));
        }
        if (k == gs.trk)                             /* the selected track: outlined */
            gx_rrect(Q(10), QI(y + 1), Q(226), Q(30), Q(8), GX_NONE, GX_INK, Q(2));
    }
    gs_band(GS_HINT_HOME);
}

static void parts_home(void)
{
    uint32_t k;
    parts_head();
    parts_knobs();
    gs_part(0, 70, 240, 210, 1);                      /* the ground: drawn with the screen */
    for (k = 0; k < NTRK; k++) {
        int32_t y = GS_ROW_Y(k);
        uint32_t live = ((gs.live >> k) & 1u) + (gs.rec == 3u && k == gs.trk) * 2u + ((gs.armed >> k) & 1u) * 4u;
        /* the whole row: the selection's outline runs x 10..236, past the last box (round UX-6: a row part that
         * stopped at x 192 left the outline's right end behind on a selection move) */
        gs_part(0, y - 1, 240, y + 34, gs.rowsig[k] * 3u + (k == gs.trk) + ((gs.mute >> k) & 1u) * 7u +
                                       gs.lvl[k] * 131u + (k == gs.hat) * 977u + 1u);
        gs_part(192, y + 3, 230, y + 29, live + 1u);
    }
    parts_band(GS_HINT_HOME);
}

/* ------------------------------------------------ GROOVES (round FLOW-1, M2) --- */
/* the shelf (spec 6.4): nine tiles, the five factory grooves above (keys 1 to 5), the four project slots below (MINE 1
 * to 4, keys 6 to 9); the one loaded filled in the track's colour with the loop's place as a bar under its name, the
 * one queued blinking NEXT BAR, an empty MINE in the second ink. A key loads its tile at the next bar line, the key
 * again: now (gn_grooves.c gn_tile_press) */
#define GS_NTILES (GN_NGROOVES + 4u)
#define GS_TILE_X(i) ((i) < GN_NGROOVES ? 8 + 45 * (int32_t)(i) : 8 + 57 * (int32_t)((i) - GN_NGROOVES))
#define GS_TILE_Y(i) ((i) < GN_NGROOVES ? 86 : 160)
static const char *gs_tiles_hint(void)
{
    return gs.tile_ask & 0x40u ? "EMPTY: KEEP A LOOP THERE (SAVE)" : gs.tile_ask ? "NOT KEPT YET: STOP IT OR PRESS AGAIN" :
           gs.tile_q ? "NEXT BAR  (THE KEY AGAIN: NOW)" : "KEYS 1 TO 9: PLAY A GROOVE";
}
static void scene_tiles(void)
{
    uint32_t i;
    gs_head();
    gs_knobs();
    gs_field(GX_PAPER);
    if (gx_visible(0, 70, 240, 158)) {
        gx_text(Q(8), Q(81), &GS_SMALL, 0, "FACTORY", GX_INK2);
        gx_text(Q(8), Q(155), &GS_SMALL, 0, "MINE", GX_INK2);
    }
    for (i = 0; i < GS_NTILES; i++) {
        int32_t x = QI(GS_TILE_X(i)), y = QI(GS_TILE_Y(i)), w = i < GN_NGROOVES ? Q(42) : Q(53), h = i < GN_NGROOVES ? Q(48) : Q(40);
        uint32_t on = gs.tile == i + 1u, q = gs.tile_q == i + 1u && gs.blink, j, n;
        uint32_t empty = i >= GN_NGROOVES && !((gs.mine >> (i - GN_NGROOVES)) & 1u);
        uint32_t ink = q ? GX_PAPER : on ? gs_on(gs.trk) : empty ? GX_INK2 : GX_INK;
        char a[14], b[8] = {(char)('1' + i), 0};
        const char *l2;
        if (!gx_visible((x >> GX_Q) - 2, (y >> GX_Q) - 2, ((x + w) >> GX_Q) + 3, ((y + h) >> GX_Q) + 3))
            continue;
        gx_rrect(x, y, w, h, Q(6), q ? GX_INK : on ? GS_FIELD[gs.trk % NTRK] : GX_PAPER, empty ? GX_INK2 : GX_INK,
                 on ? Q(2.5) : Q(1.25));
        gx_text(x + Q(5), y + Q(11), &GS_SMALL, 0, b, ink);
        if (q) {
            str_cpy(a, "NEXT", sizeof a);
            l2 = "BAR";
        } else if (i < GN_NGROOVES) {                 /* the groove's name on two lines (HEAD / NOD) */
            str_cpy(a, GN_GROOVES[i].name, sizeof a);
            for (j = 0; a[j] && a[j] != ' '; j++)
                ;
            l2 = a[j] ? &GN_GROOVES[i].name[j + 1u] : "";
            a[j] = 0;
        } else {
            str_cpy(a, "MINE 1", sizeof a);
            a[5] = (char)('1' + i - GN_NGROOVES);
            l2 = empty ? "EMPTY" : "SAVED";
        }
        n = i < GN_NGROOVES ? 26u : 22u;
        gx_text(x + w / 2, y + QI(n), &GS_SMALL, 1, a, ink);
        gx_text(x + w / 2, y + QI(n + 10u), &GS_SMALL, 1, l2, ink);
        if (on && gs.playing && !q)                  /* the loop's place, under the name */
            gx_rect(x + Q(5), y + h - Q(6), (w - Q(10)) * (int32_t)gs.prog / 255, Q(2.5), ink);
    }
    gs_band(gs_tiles_hint());
}

static void parts_tiles(void)
{
    uint32_t i;
    parts_head();
    parts_knobs();
    gs_part(0, 70, 240, 84, 1u);
    gs_part(0, 140, 240, 160, 1u);
    for (i = 0; i < GS_NTILES; i++)
        gs_part(GS_TILE_X(i) - 2, GS_TILE_Y(i) - 2, GS_TILE_X(i) + (i < GN_NGROOVES ? 45 : 56), GS_TILE_Y(i) + 51,
                (gs.tile == i + 1u) * (2u + gs.trk + (gs.playing ? gs.prog : 255u) * 8u) + (gs.tile_q == i + 1u) * (3u + gs.blink) * 4096u +
                (i >= GN_NGROOVES ? ((gs.mine >> (i - GN_NGROOVES)) & 1u) * 65536u : 0u) + 1u);
    gs_part(0, 202, 240, 210, 1u);
    parts_band(gs_tiles_hint());
}

/* ------------------------------------------------------- tap tempo (EDIT) --- */
/* EDIT (pick 4): the taps set the tempo (Felucca's own tap tempo: from the third, the last four); the big
 * number shows while tapping, a dot for each tap counted */
static void scene_tap(void)
{
    char b[8];
    uint32_t k;
    gs_head();
    gs_knobs();
    gs_field(GX_INK);
    fmt_int(b, gs.bpm);
    if (gx_visible(0, 70, 240, 150))
        gx_text(Q(120), Q(140), &GS_HUGE, 1, b, GX_PAPER);
    if (gs.clock) {
        gx_text(Q(120), Q(178), &GS_LABEL, 1, "THE CLOCK SETS IT", GX_PAPER);
    } else if (gs.tempo) {                           /* round SEQ-1: EDIT held + SELECT */
        gx_text(Q(120), Q(178), &GS_LABEL, 1, "TURN SELECT", GX_PAPER);
        gx_text(Q(120), Q(196), &GS_SMALL, 1, "A BEAT A CLICK", GX_PAPER);
    } else {
        for (k = 0; k < 4u; k++)
            gx_circle_s(Q(84) + (int32_t)k * Q(24), Q(166), Q(6), k < gs.taps ? GX_RED : GX_NONE, GX_PAPER, Q(1.5));
        gx_text(Q(120), Q(194), &GS_LABEL, 1, "TAP IN TIME", GX_PAPER);
    }
    gs_band(gs.tempo ? "TEMPO (SELECT)  LET GO (EDIT)" : "TAP (EDIT)  BACK (HOME)");
}

static void parts_tap(void)
{
    parts_head();
    parts_knobs();
    gs_part(0, 70, 240, 150, (uint32_t)gs.bpm + 1u);
    gs_part(0, 150, 240, 210, gs.taps + gs.clock * 7u + gs.tempo * 31u + 1u);
    parts_band(gs.tempo ? "TEMPO (SELECT)  LET GO (EDIT)" : "TAP (EDIT)  BACK (HOME)");
}

/* ------------------------------------------------------- the count-in --- */
/* PLAY while armed and stopped (pick 5, option A): 4, 3, 2, 1 in big digits in the track's colour, then the
 * loop starts recording */
static void scene_count(void)
{
    char b[2] = {(char)('0' + (gs.count % 10u)), 0};
    uint32_t f = gs.trk % NTRK;
    gs_head();
    gs_knobs();
    gs_field(GX_GROUND);
    if (gx_visible(0, 70, 240, 170)) {
        gx_type_t big = {Q(80), Q(9), 0};
        gx_text(Q(120), Q(156), &big, 1, b, GS_FIELD[f]);
    }
    gx_text(Q(120), Q(190), &GS_HEAD, 1, "4 * 3 * 2 * 1", GX_INK);
    gs_band("GET READY");
}

static void parts_count(void)
{
    parts_head();
    parts_knobs();
    gs_part(0, 70, 240, 210, gs.count + 1u);
    parts_band("GET READY");
}

/* -------------------------------------------------------- switch (3h) --- */
static void scene_switch(void)
{
    const char *from = gs.to_easy ? "ADVANCED" : "EASY", *to = gs.to_easy ? "EASY" : "ADVANCED";
    gx_rect(0, 0, Q(240), Q(150), GX_BLUE);
    gx_text(Q(120), Q(18), &GS_LABEL, 1, from, GX_PAPER);
    gx_path(&GX_ID, "M120 24 V38 M115.5 33.5 L120 38 L124.5 33.5", GX_NONE, GX_PAPER, Q(1.75));
    gx_path(&GX_ID, "M81.82 138.18 A54 54 0 1 1 158.18 138.18", GX_NONE, GX_PAPER, Q(1.5));
    if (gs.sweep) {                                  /* the ring fills as the buttons are held */
        uint32_t i, n = 1u + gs.sweep * 48u / 1000u;
        gx.nseg = 0;
        gx.open = 0;
        for (i = 0; i <= n; i++) {
            uint32_t a = 0x60000000u + 3221225u * gs.sweep / n * i;   /* from 135 deg, 270 deg in all */
            int32_t x = Q(120) + (int32_t)(((int64_t)Q(54) * sine_i(a + 0x40000000u)) >> 15);
            int32_t y = Q(100) + (int32_t)(((int64_t)Q(54) * sine_i(a)) >> 15);
            gx_to(x - (gx.x0 << GX_Q), y - (gx.y0 << GX_Q));
        }
        gx_stroke_paint(GX_PAPER, Q(4.5) / 2);
    }
    {
        gx_type_t big = {Q(16), Q(2.1), 20};
        gx_text(Q(120), Q(100), &big, 1, to, GX_PAPER);
    }
    gx_text(Q(120), Q(116), &GS_LABEL8, 1, "KEEP HOLDING", GX_PAPER);
    gx_rect(0, Q(150), Q(240), Q(90), GX_PAPER);
    gx_rect(0, Q(149.25), Q(240), Q(1.5), GX_INK);
    gx_rrect(Q(66), Q(160), Q(48), Q(26), Q(9), GX_INK, GX_NONE, 0);
    gx_text(Q(90), Q(176), &GS_LABEL, 1, "HOME", GX_PAPER);
    gx_path(&GX_ID, "M120 170 V176 M117 173 H123", GX_NONE, GX_INK, Q(1.75));
    gx_rrect(Q(126), Q(160), Q(48), Q(26), Q(9), GX_INK, GX_NONE, 0);
    gx_text(Q(150), Q(176), &GS_LABEL, 1, "SAVE", GX_PAPER);
    gx_text(Q(120), Q(202), &GS_LABEL, 1, "HOLD 2 S", GX_INK);
}

static void parts_switch(void)
{
    gs_part(0, 0, 240, 150, gs.to_easy + 1u);
    gs_part(62, 42, 179, 143, gs.sweep * 3u + gs.to_easy + 1u);    /* the ring as it fills, and its words */
    gs_part(0, 150, 240, 240, gs.to_easy + 1u);
}

/* ------------------------------------------------------------- the frame --- */
static uint32_t gn_view(void)
{
    if (gn.sw_t0 && !gn.sw_done)
        return GV_SWITCH;                             /* the hold that leaves, filling */
    if (gn.boot_t0 && gn_since(gn.boot_t0) < GN_BOOT_MS)
        return GV_BOOT;
    gn.boot_t0 = 0;
    if (gn.saved_t && gn_since(gn.saved_t) < GN_SAVED_MS)
        return GV_SAVED;
    gn.saved_t = 0;
    if (gn.tempo_hold && gn_btn(B_EDIT))
        return GV_TAP;                                /* (round SEQ-1: EDIT held + SELECT, the tempo) */
    if (gn.tap_t && gn_since(gn.tap_t) < GN_TAP_MS)
        return GV_TAP;
    gn.tap_t = 0;
    if (gn.pick_t)                                    /* (round FLOW-1, M2: the wheel and the sound list stay, till HOME, */
        return GV_PICK;                               /* PLAY, a key or the other encoder: gn_input) */
    if (gn.peek_t && (gs_grooves() ? gn_since(gn.peek_t) < GN_PEEK_MS : gn_picture() < GN_NPICS))
        return GV_PEEK;                               /* (the loop page's grooves list: 2 s, as before) */
    gn.peek_t = 0;
    if (gn.arm == GA_COUNT)
        return GV_COUNT;
    if (gn.page == GP_MIX)
        return GV_MIX;
    if (gn.page == GP_HOME)
        return GV_HOME;
    if (gn.page == GP_LOOP)
        return GV_LOOP;
    if (gn.page == GP_STEPS)
        return GV_STEPS;
    if (gn.page == GP_RECORD)
        return GV_RECORD;
    if (gn.page == GP_GROOVES)
        return GV_GROOVES;
    {
        uint32_t snd = gn_shown(gn_part());           /* (round UX-3: any kit shows its pads) */
        return (snd < GN_NSOUNDS && (GN_SOUNDS[snd].flags & GNF_PADS)) || gn_kit_n(gn_part()) ? GV_DRUMS : GV_PLAY;
    }
}

/* the Gnome's mouth: the vowel he sings now (track 1's voice, gliding from VOWL to VOWL2 over TALK
 * as eng_formant.c does it), shown as the nearest of A E I O U; shut when he is silent */
static uint32_t gs_vowel(void)
{
    const track_t *t = &trk[gn_part()];
    const voice_t *v = 0;
    uint32_t i;
    int32_t k = 0, pos;
    if (GN_DROP_VOICE || t->engine != GE_VOICE || t->eng_req != GE_VOICE)
        return 0;                                     /* (VOICE dropped: its slot never sounds; the Gnomad GNOME's own) */
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && (t->v[i].gate || t->v[i].env_out > 4000) && (!v || t->v[i].age > v->age))
            v = &t->v[i];
    if (!v)
        return 0;
    if (t->p[P_E2]) {
        uint32_t g = (uint32_t)v->s[7] & FMT_TALK_MASK;
        k = (int32_t)((g > (1u << 24) ? 1u << 24 : g) >> 9);
        k = (k * (65536 - k)) >> 15;
    }
    pos = t->p[P_E0] + (((t->p[P_E1] - t->p[P_E0]) * k) >> 15);
    return pos < 16 ? 1u : pos < 48 ? 2u : pos < 80 ? 3u : pos < 112 ? 4u : 5u;
}

static void gs_sound_name(uint32_t p, char *b);

/* the header's words: Easy's sound, the picture's name; another sound, its own (b holds 24) */
static void gs_head_words(uint32_t p, uint32_t pic, char *b)
{
    uint32_t snd = gn_shown(p);
    (void)pic;
    if (snd == GN_NOSND) {
        gs_sound_name(p, b);
        return;
    }
    str_cpy(b, GN_PIC_NAME[gn_pic_of(snd)], 24);
    if (!str_eq(GN_SOUNDS[snd].name, b)) {            /* "BELLS * GLASS BELL": the picture, the sound */
        str_cpy(b + str_len(b), " * ", 24 - str_len(b));
        str_cpy(b + str_len(b), GN_SOUNDS[snd].name, 24 - str_len(b));
        if (gx_text_w(&GS_HEAD, b) > Q(140))           /* round CR: too long for the header ("FUNNY * WHOOPEE CUSHION"): */
            str_cpy(b, GN_SOUNDS[snd].name, 24);       /* the sound's whole name alone, never cut (the picture is below) */
    }
}

/* Home's rows (round UX-1): each track's sound as much as fits, muted, sounding (a gated voice, or a note begun in
 * the last 150 ms: a drum's hit is short), armed; the first kit with no hit in it wears the hat; the header says
 * the selected track's BARS and SWING */
static int gs_is_kit(uint32_t p);
static int gs_kit_track(uint32_t k)
{
    uint32_t snd = gn_shown(k);
    return gs_is_kit(k) || (snd < GN_NSOUNDS && (GN_SOUNDS[snd].flags & GNF_PADS));
}

static void gs_home_rows(uint32_t p, uint32_t now)
{
    uint32_t k, i;
    char b[8];
    gs.hat = 0xFFu;
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        uint32_t snd = gn_shown(k), live = 0, newest = gn.live_age[k];
        const char *c;
        if (snd < GN_NSOUNDS)
            str_cpy(gs.row[k], GN_SOUNDS[snd].name, sizeof gs.row[k]);
        else
            gs_sound_name(k, gs.row[k]);
        while (gs.row[k][0] && gx_text_w(&GS_LABEL, gs.row[k]) > Q(100))
            gs.row[k][str_len(gs.row[k]) - 1u] = 0;
        for (c = gs.row[k], gs.rowsig[k] = 5u; *c; c++)
            gs.rowsig[k] = gs.rowsig[k] * 31u + (uint8_t)*c;
        for (i = 0; i < NVOICE; i++) {
            if (t->v[i].active && t->v[i].gate)
                live = 1;
            if (t->v[i].active && t->v[i].age > gn.live_age[k]) {
                gn.live_ms[k] = (uint16_t)(now | 1u);
                if (t->v[i].age > newest)
                    newest = t->v[i].age;
            }
        }
        gn.live_age[k] = newest;
        if (live || (gn.live_ms[k] && (uint16_t)(now - gn.live_ms[k]) < 150u))
            gs.live |= (uint8_t)(1u << k);
        if (t->p[P_MUTE])
            gs.mute |= (uint8_t)(1u << k);
        if (gs.hat == 0xFFu && gs_kit_track(k) && seq_is_empty(t))
            gs.hat = (uint8_t)k;
    }
    if (gn.arm || gn.prov)
        gs.armed = (uint8_t)(1u << p);
    str_cpy(gs.head, "BARS ", sizeof gs.head);
    fmt_int(b, (int32_t)(gn_grid_ok(p) ? gn_bars(p) : ((uint32_t)clamp(trk[p].p[P_SLEN], 1, NSTEP) + 15u) / 16u));
    str_cpy(gs.head + str_len(gs.head), b, sizeof gs.head - str_len(gs.head));
    str_cpy(gs.head + str_len(gs.head), " * SWING ", sizeof gs.head - str_len(gs.head));
    fmt_int(b, clamp(trk[p].p[P_SSWING], 0, 100));
    str_cpy(gs.head + str_len(gs.head), b, sizeof gs.head - str_len(gs.head));
    str_cpy(gs.head + str_len(gs.head), "%", sizeof gs.head - str_len(gs.head));
    for (k = 0, gs.headsig = 13u; gs.head[k]; k++)
        gs.headsig = gs.headsig * 31u + (uint8_t)gs.head[k];
}

/* the steps page's row and the record screen's words (round UX-2): the bar's places (16; 8 on the 1/8 grid; two
 * 32nds to a place with SNAP off), the note each begins or a note held on, the playhead's place in the bar shown; a
 * kit: its drums, K2's, the half of the list that holds it, each row's places; the cursor's step, NOTE's note */
static void gs_steps_row(uint32_t p, uint32_t view, uint32_t idx, uint32_t len)
{
    const track_t *t = &trk[p];
    uint32_t fold, c, j, r;
    gs.cells = (uint8_t)(gs.spb > 16u ? 16u : gs.spb);
    fold = gs.spb / gs.cells;
    gs.ccol = song.playing && idx / gs.spb == gs.bar ? (uint8_t)((idx % gs.spb) / fold) : 0xFFu;
    (void)view;
    for (c = 0; c < gs.cells; c++)
        for (j = 0; j < fold; j++) {
            uint32_t i = gs.bar * gs.spb + c * fold + j;
            const step_t *st = &t->step[i % NSTEP];
            if (i >= len || gs.cnote[c] > 1u)
                continue;
            if (st->time == ST_NOTE && st->n)
                gs.cnote[c] = (uint8_t)(st->note[0] > 1u ? st->note[0] : 2u);
            else if (st->time == ST_TIE && !gs.cnote[c])
                gs.cnote[c] = 1;
        }
    {
        const step_t *st = &t->step[(gn.cursor % len) % NSTEP];
        gs.curnote = (uint8_t)(st->time == ST_NOTE && (st->n || st->hit));
    }
    gs.lnote = (uint8_t)(gn.last_note ? gn.last_note : 60u);
    {   /* round UX-4 (SHIFT): the cursor's place in the bar shown, and its step: note, strength, length */
        uint32_t cur = gn_cursor_step(p), n = 1;
        const step_t *st = &t->step[cur % NSTEP];
        gs.ccur = cur / gs.spb == gs.bar ? (uint8_t)((cur % gs.spb) / fold) : 0xFFu;
        gs.son = (uint8_t)(st->time == ST_NOTE && (st->n || st->hit));
        gs.snote = (uint8_t)(st->n ? st->note[0] : st->hit ? DRUM_LANE_NOTE[__builtin_ctz(st->hit) % NLANE] : 0u);
        gs.svel = st->vel;
        while (n < len && t->step[(cur + n) % len].time == ST_TIE)
            n++;
        gs.slen = (uint8_t)n;
    }
    gs.ndrum = (uint8_t)gn_kit_n(p);
    if (!gs.ndrum)
        return;
    gs.drum = (uint8_t)(gn.drum % gs.ndrum);
    gs.dbank = (uint8_t)(gs.drum / 8u);
    gs.dname = gn_kit_name(p, gs.drum);
    for (r = 0; r < 8u && gs.dbank * 8u + r < gs.ndrum; r++) {
        uint32_t id = gs.dbank * 8u + r;
        gs.drn[r] = gn_kit_name(p, id);
        gs.dnote[r] = (uint8_t)gn_kit_note(p, id);
        for (c = 0; c < gs.cells; c++)
            for (j = 0; j < fold; j++) {
                uint32_t i = gs.bar * gs.spb + c * fold + j;
                if (i < len && gn_kit_has(p, &t->step[i % NSTEP], id))
                    gs.drow[r] |= (uint16_t)(1u << c);
            }
    }
}

/* round SEQ-1: every track's lane (layout A) through gn_seq_*: its bar shown (the selected track's: the page's; the
 * others': the one playing, else the same bar as far as they reach), the places that begin and hold on, the playhead's
 * place; the selected kit's three most used drums; on STEPS each place's strength, chance, other drums and whether
 * it is lit (K2's drum, a note) */
#define GN_LIT_MS 1200u                           /* a card stays lit this long after its knob's last turn */
static void gs_lanes(uint32_t p, uint32_t view)
{
    uint32_t t, d, j, best[3] = {0, 0, 0}, cnt[3] = {0, 0, 0};
    for (t = 0; t < NTRK; t++) {
        uint32_t spb = gn_seq_spb(t), pos = gn_seq_pos(t), bars = gn_seq_bars(t), b, f = gn_seq_fold(t);
        b = t == p ? gs.bar : pos != GN_SEQ_NONE ? pos / spb : gs.bar % (bars ? bars : 1u);
        gs.ln_cells[t] = (uint8_t)gn_seq_cells(t);
        gs.ln_occ[t] = (uint16_t)gn_seq_bar_mask(t, b, 0u, 0u);
        gs.ln_tie[t] = (uint16_t)(gn_seq_bar_mask(t, b, 1u, 0u) & ~gs.ln_occ[t]);
        gs.ln_col[t] = pos != GN_SEQ_NONE && pos / spb == b ? (uint8_t)((pos % spb) / f) : 0xFFu;
        if (trk[t].p[P_MUTE])
            gs.mute |= (uint8_t)(1u << t);
    }
    if (gs.drums && gn_kit_n(p)) {                   /* the tall kit lane: its three most used drums, in kit order */
        for (d = 0; d < gn_kit_n(p); d++) {
            uint32_t m = gn_seq_bar_mask(p, gs.bar, 2u, d), c = 0, r;
            for (r = m; r; r &= r - 1u)            /* (its places: no libgcc call on the chip) */
                c++;
            for (r = 0; r < 3u && cnt[r] >= c; r++)
                ;
            if (!c || r == 3u)
                continue;
            for (j = 2u; j > r; j--)
                best[j] = best[j - 1u], cnt[j] = cnt[j - 1u];
            best[r] = d, cnt[r] = c;
        }
        for (j = 0; j < 3u && cnt[j]; j++)
            ;
        gs.trows = (uint8_t)j;
        if (gs.trows > 1u && best[0] > best[1]) { d = best[0]; best[0] = best[1]; best[1] = d; }
        if (gs.trows > 2u && best[1] > best[2]) { d = best[1]; best[1] = best[2]; best[2] = d; }
        if (gs.trows > 1u && best[0] > best[1]) { d = best[0]; best[0] = best[1]; best[1] = d; }
        for (j = 0; j < gs.trows; j++)
            gs.trow[j] = (uint16_t)gn_seq_bar_mask(p, gs.bar, 2u, best[j]);
    }
    if (view != GV_STEPS)
        return;
    gs.hold = gn.steps_keys ? gn.hold_key : 0u;
    gs.swhen = (uint8_t)gn_seq_when(p, gn_cursor_step(p));
    for (j = 0; j < gs_ncells() && j < 16u; j++) {
        uint32_t i = gn_seq_place_step(p, gs.bar, j);
        if (i >= gn_seq_len(p))
            continue;
        if (gs.drums ? gn_seq_drum(p, i, gn.drum % (gn_kit_n(p) ? gn_kit_n(p) : 1u)) : gn_seq_sounds(p, i))
            gs.pon |= (uint16_t)(1u << j);
        if (gn_seq_sounds(p, i) && gn_seq_when(p, i) < 100u)
            gs.pmaybe |= (uint16_t)(1u << j);
        gs.pvel[j] = (uint8_t)gn_seq_vel(p, i);
        if (gs.drums)
            for (d = 0; d < gn_kit_n(p); d++)
                if (d != gn.drum % gn_kit_n(p) && gn_seq_drum(p, i, d))
                    gs.podr[j]++;
    }
}

static void gs_snapshot(uint32_t view)
{
    uint32_t k, now = fm1_ms, p = gn_part(), pic = gn_picture();
    memset(&gs, 0, sizeof gs);
    gs.view = (uint8_t)view;
    gs.pic = (uint8_t)pic;
    gs.field = (uint8_t)gs_pic_field(pic);
    gs.ink = pic >= GN_OTHER || gs.field >= 2u;
    gs.idle = !gn_map(p, 0);                          /* (round 13: another sound has its engine's map) */
    gs.trk = (uint8_t)p;
    gs.clock = song.g[G_CLOCK] != 0;
    gs.bpm = (int16_t)(gs.clock && midi_beat_samples ? (FS * 60u + midi_beat_samples / 2u) / midi_beat_samples : (uint32_t)song.g[G_BPM]);
    if (pic >= GN_OTHER) {                           /* another sound: its own name (ui_graph.c) */
        const char *c;
        trk_short_name(p, gs.name);
        for (c = gs.name, gs.namesig = 7u; *c; c++)
            gs.namesig = gs.namesig * 31u + (uint8_t)*c;
    }
    {
        const char *c;
        gs_head_words(p, pic, gs.head);
        while (gs.head[0] && gx_text_w(&GS_HEAD, gs.head) > Q(140))
            gs.head[str_len(gs.head) - 1u] = 0;
        for (c = gs.head, gs.headsig = 11u; *c; c++)
            gs.headsig = gs.headsig * 31u + (uint8_t)*c;
    }
    for (k = 0; k < NTRK; k++)
        gs.lvl[k] = (uint8_t)trk_level(k);
    gs.taps = (uint8_t)(lys.ntap > 4u ? 4u : lys.ntap);
    gs.saved = gn.saved_mine;
    gs.mark = gn.auto_dirty == GN_LOOP_SAVED;
    gs.snd = (uint8_t)gn_shown(p);
    gs.grv = (uint8_t)(gs_grooves() ? gn.grv : 0u);
    for (k = 0; k < 4u; k++)
        gs.role[k] = (uint8_t)gn_knob_role(k);
    for (k = 0; k < 4u; k++) {                       /* round MF: the knobs' names, a Gnomad sound's own */
        const char *l = 0, *c;
#if GN_ENGINES
        l = gn_knob_label(p, k);
#endif
        str_cpy(gs.lbl[k], l && *l ? l : GS_MACRO[k], sizeof gs.lbl[k]);
        gs.icon[k] = (uint8_t)gs_icon_of(gs.lbl[k]);
        for (c = gs.lbl[k]; *c; c++)
            gs.lblsig = gs.lblsig * 31u + (uint8_t)*c;
        gs.lblsig = gs.lblsig * 31u + k;
    }
    gs.rec = (uint8_t)((song.rec >> p) & 1u && !gn.prov ? 3u : gn.arm == GA_COUNT ? 2u : gn.arm || gn.prov ? 1u : 0u);
    gs.replace = gn.replace;
    gs.blink = (now % 1000u) < 500u;
    if (gn.arm == GA_COUNT) {
        uint32_t beat = gn.cnt_ms / 4u ? gn.cnt_ms / 4u : 1u, e = fm1_ms - (gn.cnt_t0 & ~1u);
        gs.count = (uint8_t)(e / beat >= 4u ? 1u : 4u - e / beat);
    }
    for (k = 0; k < 4u; k++) {                       /* 14 px * (0.3 + 0.7 amount) */
        gs.r[k] = (uint8_t)((14 * 16 * (3 * GN_STEPS + 7 * gn.amount[p][k])) / (10 * GN_STEPS));
        gs.amt[k] = (uint8_t)(gn.amount[p][k] > GN_STEPS ? GN_STEPS : gn.amount[p][k]);
    }
    if (!gs.idle && gn.amount[p][1] && view != GV_SWITCH) {   /* Wobble breathes on the beat */
        uint32_t beat = 60000u / (uint32_t)(song.g[G_BPM] > 0 ? song.g[G_BPM] : 120), ph = now % beat;
        uint32_t tri = ph < beat / 2u ? ph * 6u / (beat / 2u) : (beat - ph) * 6u / (beat / 2u);
        gs.r[1] = (uint8_t)(gs.r[1] + gs.r[1] * tri / 100u);
    }
    gs.play = (song.rec >> p) & 1u ? 2u : song.playing ? 1u : 0u;
    gs.linked = usb.config && !usb.suspended;
    if (view == GV_PLAY && pic == 0u)
        gs.vowel = (uint8_t)gs_vowel();
    if (view == GV_LOOP || view == GV_STEPS || view == GV_RECORD || GN_LOOP_PAGE(gn.page)) {   /* the track's loop */
        const track_t *t = &trk[p];
        uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), idx = song.playing ? t->seq_idx % len : 0u, i;
        gs.grid = (uint8_t)gn_grid_ok(p);
        gs.spb = (uint8_t)(gs.grid ? gn_spb((uint32_t)t->p[P_SDIV]) : 16u);
        gs.bars = (uint8_t)(gs.grid ? gn_bars(p) : (len + 15u) / 16u);
        gs.snap = (uint8_t)gn_snap(p);
        gs.swing = (uint8_t)clamp(t->p[P_SSWING], 0, 100);
        gs.vol = (uint8_t)(trk_level(p) * 100u / 127u);
        gs.drums = (uint8_t)gn_is_drums(p);
        gs.punch = gn.punch;
        gs.playing = song.playing;
        gs.step = song.playing ? (uint8_t)((idx % gs.spb) * 16u / gs.spb) : 0xFFu;   /* the playhead, of 16 */
        for (i = 0; i < len; i++) {                  /* each bar's sixteen places that sound */
            const step_t *st = &t->step[i];
            if (st->time == ST_NOTE && (st->n || st->hit) && i / gs.spb < 8u)
                gs.occ[i / gs.spb] |= (uint16_t)(1u << ((i % gs.spb) * 16u / gs.spb));
        }
        /* one bar up close: the steps page's (K1, or the playhead's: gn_view_bar), the cursor's (the record screen
         * stopped), the one playing */
        gs.bar = (uint8_t)(view == GV_STEPS ? gn_view_bar(p) : song.playing ? idx / gs.spb :
                           view == GV_RECORD ? (gn.cursor % len) / gs.spb : 0u);
        gs.col = song.playing ? (uint8_t)(idx % gs.spb) : 0xFFu;
        gs.cur = (uint8_t)((gn.cursor % len) % gs.spb);
        for (i = 0; i < gs.spb && i < 32u; i++) {
            uint32_t j = gs.bar * gs.spb + i, k;
            const step_t *st = &t->step[j % NSTEP];
            if (j >= len)
                continue;
            if (st->time == ST_TIE)
                gs.pitch[i] = 1;
            if (st->time != ST_NOTE)
                continue;
            gs.lane[i] = st->hit;
            for (k = 0; k < st->n && k < 4u; k++)
                gs.lane[i] |= (uint8_t)(1u << drum_lane(st->note[k]));
            if (st->n)
                gs.pitch[i] = (uint8_t)(st->note[0] > 1u ? st->note[0] : 2u);
            gs.lanes |= gs.lane[i];
        }
        if (!gs.lanes)
            gs.lanes = 0x0Fu;                        /* (an empty bar: kick, snare, clap, hat) */
        gs_steps_row(p, view, idx, len);
        gs_lanes(p, view);                           /* round SEQ-1: layout A's lanes, the STEPS tap grid */
    }
    if (view == GV_DRUMS) {                          /* the kit (round UX-3): its pads light when they sound */
        const voice_t *dv = trk[p].v;
        uint32_t newest = gn.drum_age, id;
        gs.kn = (uint8_t)gn_kit_n(p);
        gs.bank = (uint8_t)(gs.kn > 8u && gn.bank);
        for (k = 0; k < 8u && gs.bank * 8u + k < gs.kn; k++)
            gs.kname[k] = gn_kit_name(p, gs.bank * 8u + k);
        for (k = 0; k < NVOICE; k++) {
            if (dv[k].active && dv[k].age > gn.drum_age && (id = gn_kit_of(p, dv[k].note)) < 16u)
                gn.pad_ms[id] = (uint16_t)(now | 1u);
            if (dv[k].age > newest)
                newest = dv[k].age;
        }
        gn.drum_age = newest;
        for (k = 0; k < 27u; k++)                    /* the keys down: their pads */
            if (((fm1_in.notes >> k) & 1u) && (id = gn_kit_of(p, gn_key_note(p, k))) < 16u)
                gs.pads |= (uint16_t)(1u << id);
        for (k = 0; k < 16u; k++)
            if (gn.pad_ms[k] && (uint16_t)(now - gn.pad_ms[k]) < 200u)
                gs.pads |= (uint16_t)(1u << k);
    }
    gs.shift = gn.shift;                             /* round UX-4: SHIFT's depth */
    gs.hlit = 0xFFu;                                 /* round SEQ-1: the knobs turning now, lit in line */
    for (k = 0; k < 4u; k++)
        if (gn.klit_t[k] && gn_since(gn.klit_t[k]) < GN_LIT_MS) {
            gs.lit |= (uint8_t)(1u << k);
            if (gs.hlit == 0xFFu || (int32_t)(gn.klit_t[k] - gn.klit_t[gs.hlit]) > 0)
                gs.hlit = (uint8_t)k;
        }
    gs.tempo = gn.tempo_hold;
    if (view == GV_HOME || view == GV_PLAY || view == GV_DRUMS || view == GV_LOOP || view == GV_STEPS ||
        view == GV_RECORD || view == GV_MIX || view == GV_GROOVES)
        gs.ring = (uint8_t)(gn_ring_of(gn.page) + 1u);
    gs.stored = (uint8_t)(gn.save_ok && gn.music_sig == gn.saved_sig);   /* round UX-5: SAVED, or ON SYNTH */
    if (view == GV_MIX)
        for (k = 0; k < NTRK; k++) {
            gs.pan[k] = (int8_t)clamp(trk[k].p[P_PAN], -64, 63);
            if (trk[k].p[P_MUTE])
                gs.mute |= (uint8_t)(1u << k);
        }
    if (view == GV_HOME)
        gs_home_rows(p, now);
    if (view == GV_SWITCH) {
        gs.sweep = (uint16_t)clamp((int32_t)gn_since(gn.sw_t0) * 1000 / (int32_t)GN_HOLD_MS, 0, 1000);
        gs.sweep = (uint16_t)(gs.sweep / 25u * 25u);  /* (40 steps: the ring's part is redrawn 20 times a second) */
        gs.to_easy = gn.sw_to_easy;
    }
    if (view == GV_BOOT)
        gs.boot = (uint16_t)(clamp((int32_t)gn_since(gn.boot_t0), 0, 60000) / 66 * 66);   /* the parts' frames */
    gs.tq = gn.tq;                                   /* round FLOW-1 */
    gs.clk = (uint8_t)(gn_click_on() | (gn_btn(B_EDIT) && !gn_k3_shift_job()) << 1 | gn_btn(B_GLO) << 2 |
                       (gn_btn(B_FX) && !gn.shift) << 3);
    if (gn.undid_t && gn_since(gn.undid_t) < 2000u)  /* M3 */
        gs.undid = (uint8_t)(gn.undid + 1u);
    else
        gn.undid_t = 0;
    gs.chg = (uint8_t)(song.playing && ((gn.pick_t && gn_since(gn.pick_t) < 1500u) ||
                                        (gn.peek_t && !(gn.grv & 0x80u) && gn_since(gn.peek_t) < 1500u)));
    gs.tile = gn.tile;                               /* M2: GROOVES */
    gs.tile_q = gn.tile_q;
    gs.tile_ask = gn.tile_ask;
    gs.mine = gn.mine;
    if (view == GV_GROOVES && song.playing) {
        gs.playing = 1;
        gs.prog = (uint8_t)(trk[p].seq_idx % gn_seq_len(p) * 255u / gn_seq_len(p));
    }
    if (gs.rec && view != GV_SWITCH && view != GV_BOOT) {   /* the header names what records, on every page */
        const char *c;
        gs_rec_words(gs.head);
        for (c = gs.head, gs.headsig = 19u; *c; c++)
            gs.headsig = gs.headsig * 31u + (uint8_t)*c;
    }
}

#include "gn_screens2.c"

/* the screen Easy has, a box at a time: what changed (or all of it), at most GS_BUDGET_PX pixels a
 * call, then back to the main loop (the editor, the update entry and USB are served between two
 * calls); see the top. Felucca's screen when Easy is off, and while main.c counts down to the update
 * mode (OCT- + OCT+ held): Felucca draws then, and Easy draws all of its screen again after. */
/* each view's scene and its parts, in GV_* order (the tests draw every view through these) */
static void (*const GS_SCENE[GV_NONE])(void) = {scene_boot, scene_play, scene_pick, scene_peek, scene_loop, scene_steps,
                                                scene_record, scene_drums, scene_mix, scene_tap, scene_count,
                                                scene_saved, scene_switch, scene_home, scene_tiles};
static void (*const GS_PARTS[GV_NONE])(void) = {parts_boot, parts_play, parts_pick, parts_peek, parts_loop, parts_steps,
                                                parts_record, parts_drums, parts_mix, parts_tap, parts_count,
                                                parts_saved, parts_switch, parts_home, parts_tiles};

static int gn_draw(void)
{
    static uint32_t gs_frame_ms;                      /* Easy's last frame, fm1_ms | 1 */
    static uint32_t gs_frame_n;                       /* .. and Felucca's frame count then (ui_draw: ui.frame) */
    uint32_t v, i;
    gs_owned = 0;
    if (!gn.on || ui.uboot) {
        /* Felucca's pages again: its ground, as main.c leaves the screen before Felucca's first frame,
         * a strip of GS_BUDGET_PX a call like any of Easy's frames (Felucca's input runs meanwhile, its
         * drawing waits), then all of the screen Felucca's (ui.force). In Easy, or during that, the
         * update countdown (ui.uboot): Felucca's own frames, which fill all below the header and draw
         * the header (ui_draw.c draw_uboot); Easy draws all of its screen again after (gn_draw's
         * frame check), and a ground still to clear is the countdown's now */
        if (!gn.drawn || gn.on)
            return 0;
        if (ui.uboot || gn.exit_y >= 240u) {
            gn.drawn = 0;
            gn.exit_y = 0;
            ui.force = 1;
            return 0;
        }
        if (!gn.exit_y) {
            gs_job.on = 0;
            gs_shown_view = GV_NONE;
        }
        v = 240u - gn.exit_y < GS_BUDGET_PX / 240u ? 240u - gn.exit_y : GS_BUDGET_PX / 240u;
        lcd_fill(0, gn.exit_y, 240, v, T_BG);
        gn.exit_y = (uint8_t)(gn.exit_y + v);
        gs_owned = 1;
        return 1;
    }
    v = gn_view();
    gs_snapshot(v);
    gs_n = 0;
    GS_PARTS[v]();
    /* All of the screen is drawn for a new view, after a frame that was not Easy's (Felucca drew the
     * update countdown), and after the main loop stood still for a second with ui.force set (an
     * update session that returned has drawn its own screen, and main.c cleared it). Felucca's
     * ui.force alone asks for a whole screen after anything the editor sets: Easy's parts show those
     * changes already (finding 6). */
    if (v != gs_shown_view || !gn.drawn || ui.frame != gs_frame_n + 1u || (ui.force && gn_since(gs_frame_ms) >= 1000u)) {
        gs_shown_n = 0;
        gs_shown_view = (uint8_t)v;
        gs_job_start(0, 0, 240, 240);
    }
    ui.force = 0;
    gs_frame_ms = fm1_ms | 1u;
    gs_frame_n = ui.frame;
    gn.drawn = 1;
    gs_px_call = 0;
    for (;;) {
        uint32_t h, w, px;
        if (!gs_job.on) {                             /* the next part not on the screen */
            for (i = 0; i < gs_n && gs_is_shown(&gs_now[i]); i++)
                ;
            if (i == gs_n)
                break;
            gs_job_start(gs_now[i].x0, gs_now[i].y0, gs_now[i].x1, gs_now[i].y1);
        }
        w = (uint32_t)(gs_job.x1 - gs_job.x0);
        h = GS_BUDGET_PX / w < GX_MH ? GS_BUDGET_PX / w : GX_MH;   /* (w >= 1: gs_part keeps no empty box) */
        if (h > (uint32_t)(gs_job.y1 - gs_job.y))
            h = (uint32_t)(gs_job.y1 - gs_job.y);
        px = w * h;
        if (gs_px_call && gs_px_call + px > GS_BUDGET_PX)
            break;                                    /* the rest in the next frame */
        gx_begin(gs_job.x0, gs_job.y, (int32_t)w, (int32_t)h, GX_GROUND);
        GS_SCENE[v]();
        gx_end();
        gs_pass_note(gs_job.x0, gs_job.y, gs_job.x1, gs_job.y + h);
        gs_px_call += px;
        gs_px_total += px;
        gs_job.y = (uint8_t)(gs_job.y + h);
        if (gs_job.y >= gs_job.y1)
            gs_job_end();
    }
    gs_owned = 1;
    return 1;
}
