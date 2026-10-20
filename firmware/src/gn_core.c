/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md, "Gnomad engines"): the Gnomad sound core in the firmware, and its adapter. Built only with
 * GN_ENGINES (gn_engines.h); engines.c includes this file before its table, which puts the descriptors below in
 * the slots of the engines the swap retires.
 *
 * The core (firmware/gnomad-core: the copyright holder's code, GPL-3.0-only inside this firmware; its API is
 * gnomad-core/include/gnomad_core.h, the only part of it this file uses) is one instrument of four tracks and
 * eight voices with its own effects. One instance renders every track whose engine is a Gnomad engine, as track
 * t of the core; its mono int16 output is added to the dry mix of the block (fx.c mix_block's hook), after
 * Felucca's parts and before the FX layer and the master. The blocks are Felucca's: CTL (32) frames, four per
 * 128-frame half, so a Gnomad note starts exactly when a Felucca note would.
 *
 * Notes: every note source (keys, sequencer, ARP, chords, USB and TRS MIDI) reaches voice.c, which starts a
 * voice of the track's engine. A Gnomad engine's voice is only a token in Felucca's shared budget of 8: its
 * note_on queues a core NOTE_ON (note ID per track, monotonically increasing, never 0; key = the MIDI note, a
 * DRUMS pad = (note - 36) mod 16; velocity 1..127), and its done() (voice.c calls it once a block, before the
 * render) queues the NOTE_OFF when the gate falls and ends the token at once: the release and the tail are the
 * core's, in the core's own 8 voices. A legato pitch change (no new note_on) is an OFF and an ON. The render
 * writes nothing (the part's buffer stays 0).
 *
 * Sounds and controls, once a block before the render (gnc_sync): a track whose engine or preset changed gets
 * ALL_OFF (voice.c clears Felucca's voices of a switched engine without calling done) and, on a Gnomad engine, its
 * preset's record from the factory banks (round FW-CONTENT: firmware/gnomad-content/gn_content.h; DRUMS: a kit with
 * the core's drum patch, gn_core_records.h), bound between two renders as SPEC/40's instant sound change; the four
 * HOME knobs are the core's four macros: BRIT and WOBL (EDIT 1) Bright P5 and Wobble P6, the
 * track's DLY and REV sends (the FX page's too) Echo P7 and Space P8; CHOR is the core's chorus send P9, LEVEL and
 * MUTE its track gain: streaming PARAMETER events, sent when they change. The core's
 * master controls stay at their defaults. No editor-protocol command is added: the engines are table entries.
 *
 * Per-sound knobs (round MF; core SPEC/CHANGES-1.3.0-macros): a record names its four knobs (its
 * macro map, record bytes 10, 11, 54, 55). A knob whose recipe is one of the four controls above (the default map,
 * or ECHO / SPACE in any place: the sends stay the sends) is that control, as before. Any other is the sound's own:
 * its amount is kept in EDIT 1's K1..K4 (P_E0 + its place: BRIT and WOBL are K1 and K2), and a change of it sends
 * gc_macro_apply's writes for that amount on the record as bound, the flash record (never one the macro moved: the
 * map applies on bind): worked out on the main loop (gn_core_host, host rate), posted, and sent by the interrupt. A control a knob of the sound's own owns (its setting, or a parameter its writes reach) is
 * not sent from its setting. The knob's rest (gc_macro_rest) sits exactly on a knob place, so a preset and Easy's
 * sound load start where the sound was authored (gnc_slot, gnc_amount). DRUMS keep the kit four.
 *
 * Presets (round FW-CONTENT): each Gnomad engine's presets are the authored slots of the banks for that engine, in
 * bank order (tools/gn_gen_content.py skips zero and malformed records): NAMES and the screens read the names from
 * the records themselves (bytes 16..31 in flash, no copy), PRESET k binds the k-th listed record, and a preset sets the
 * knobs to the record's stored Bright, Wobble, Echo, Space and Chorus (as near as the knobs' steps reach), so the
 * controls the shim then sends are the record's own. SAMPLE's assets are the factory PCM's three. A preset costs its
 * bank ID (a uint16_t) and nothing else: no preset_t (round FW-CONTENT-7: gn_preset_get, gn_preset_name).
 *
 * Memory: the core's state arena (7,168 B, .bss), its effect pool (65,536 B, the .pool section the retired
 * engines leave), the adapter's queue and buffer; no malloc. The core is initialised in audio_init (audio.c's
 * hook), before the audio interrupt runs. Everything else runs in the audio interrupt: the core's calls are
 * serialized there, as SPEC/01 requires. */

#include "../gnomad-core/gn_core_unity.h"      /* the core, one unit (its macros #undef'd after it) */
#include "../gnomad-core/gn_core_records.h"    /* the core's drum patch (DRUMS binds it with a kit; generated, checked) */
#include "../gnomad-content/gn_content.h"      /* the factory banks and PCM in flash (generated, checked: FW-CONTENT) */

/* the slots (engines.c ENGINES[]: the stores and the protocol hold the numbers, so the Gnomad engines take
 * retired engines' numbers; NENGINES stays 14: favourites' row 15 is Easy's, gn_easy.c) */
#define GN_ENGI_GNOME 7u           /* was WHEEL */
#define GN_ENGI_HIDDEN 8u          /* was GRAIN: SAMPLER's descriptor, not offered (eng_ok): GRAIN sounds play */
#define GN_ENGI_VA 9u              /* was PHYS */
#define GN_ENGI_DRUMS 10u          /* was DRUM (ENGI_DRUM: its project and preset migrations land here) */
#define GN_ENGI_FM 12u             /* was FM6 (ENGI_FM6: DIGITAL's sounds land here) */
#define GN_ENGI_SAMPLE 13u         /* was SLICE */
_Static_assert(NENGINES == 14 && GN_ENGI_DRUMS == ENGI_DRUM && GN_ENGI_VA == ENGI_PHYS, "the Gnomad slots");

#define GN_CORE_QUIET 1400u        /* silent blocks (1 s) with no voice before the core stops being rendered */

static union {                     /* the arenas: 8-byte aligned (the union holds even where hostsim.c defines */
    uint64_t align;                /* __attribute__ away) */
    uint8_t b[GC_STATE_BYTES];
} gnc_state __attribute__((aligned(8)));
static union {
    uint64_t align;
    uint8_t b[GC_FX_BYTES];
} gnc_fx __attribute__((aligned(8), section(".pool")));

enum { GNC_BRIGHT, GNC_WOBBLE, GNC_ECHO, GNC_SPACE, GNC_CHORUS, GNC_GAIN, GNC_NCTL };
#define GNC_RING 64u               /* the note queue (a power of two) */
/* interim mix (FORK.md, "Gnomad engines": owner decisions): the core's output into Felucca's mix, Q8 (x5: +14 dB, the
 * measured gap between a Gnomad and a Felucca note at the same LEVEL), the Space send's gain (x2, saturating) and the
 * core's master wet level (its maximum: +6 dB over the core's default) */
#define GN_CORE_GAIN_Q8 1280
#define GN_CORE_SPACE_X 2
#define GN_CORE_WET 65535
#define GN_CORE_TAIL_MAX_MS 4000u  /* the longest a released note's token is kept */
static struct {
    gc_core *core;
    uint8_t tried;                 /* gc_init was called (once) */
    uint8_t bound[NTRK];           /* Felucca engine index + 1 the core track is bound to, 0 = none */
    uint8_t preset[NTRK];
    uint16_t tail[NTRK];           /* blocks a released note's token is kept (the bound patch's release) */
    int32_t sent[NTRK][GNC_NCTL];  /* the controls as last sent, -1 = send */
    /* round MF: each bind's number (never 0), and whether its record has a map of its own (gnh plans it) */
    volatile uint32_t bseq[NTRK];
    uint8_t mapped[NTRK];
    uint32_t next_id[NTRK];
    uint32_t quiet;                /* silent blocks in a row (no voice, no event): the render is skipped */
    int16_t emix;                  /* round EW: DLY's MIX, every Echo send x (256 + emix) / 256 (the main loop's) */
    /* the note queue: written by the note paths (voice.c's hooks), read by gn_core_mix. One producer context
     * (today every note path runs in the audio interrupt, as the consumer does); the slot is written, then
     * RING_PUBLISH, then the index: a consumer that runs in between never sees half an event */
    volatile uint32_t w;
    uint32_t r;
    gc_event ring[GNC_RING];
    uint16_t nev;                  /* the block's list (the consumer's only) */
    gc_event ev[GC_MAX_EVENTS];
    int16_t out[CTL];
    gc_report rep;
    /* telemetry (console, tests) */
    uint32_t renders, errors, lost, last_status, last_detail, init_status, orphans, steals;
} gnc;

/* the core engine of Felucca engine slot e, -1 = a Felucca engine */
static int32_t gnc_engine(uint32_t e)
{
    switch (e) {
    case GN_ENGI_VA: return GC_VA;
    case GN_ENGI_FM: return GC_FM;
    case GN_ENGI_DRUMS: return GC_DRUMS;
    case GN_ENGI_GNOME: return GC_GNOME;
    case GN_ENGI_SAMPLE:
    case GN_ENGI_HIDDEN: return GC_SAMPLE;
    default: return -1;
    }
}

static void gnc_fill(gc_event *e, uint8_t type, uint32_t ti, uint32_t id, uint32_t key, uint32_t vel, uint32_t par,
                     int32_t val)
{
    e->offset = 0;
    e->type = type;
    e->track = (uint8_t)ti;
    e->note_id = id;
    e->key = (uint16_t)key;
    e->velocity = (uint16_t)vel;
    e->parameter = (uint16_t)par;
    e->reserved = 0;
    e->value = val;
}

/* producer: a note event into the queue (full: counted, dropped; never in Felucca's use, 64 a block at most) */
static void gnc_push(uint8_t type, uint32_t ti, uint32_t id, uint32_t key, uint32_t vel)
{
    uint32_t w = gnc.w;
    if (w - gnc.r >= GNC_RING) {
        gnc.lost++;
        return;
    }
    gnc_fill(&gnc.ring[w & (GNC_RING - 1u)], type, ti, id, key, vel, 0, 0);
    RING_PUBLISH();                                     /* the slot before the index (core.h) */
#ifdef GN_CORE_PREEMPT
    GN_CORE_PREEMPT();                                  /* (tests: the consumer interrupting here) */
#endif
    gnc.w = w + 1u;
}

/* consumer: an event onto the block's list (offsets: at most 16 at one sample, the 17th a sample later); 0 = full */
static int gnc_add(uint8_t type, uint32_t ti, uint32_t id, uint32_t par, int32_t val)
{
    if (gnc.nev >= GC_MAX_EVENTS)
        return 0;
    gnc_fill(&gnc.ev[gnc.nev], type, ti, id, 0, 0, par, val);
    gnc.ev[gnc.nev].offset = (uint16_t)(gnc.nev / GC_MAX_AT_OFFSET);
    gnc.nev++;
    return 1;
}

/* consumer: an event at the head of the block's list, the others a place later (offsets as gnc_add's); 0 = full.
 * The sweep's PANIC (round UX-R): a track's stolen tails are cut before the block's new notes, which took their
 * tokens, ask the core for a voice (else, with every core voice in a tail, the core has none to give them) */
static int gnc_add_first(uint8_t type, uint32_t ti)
{
    uint32_t i;
    if (gnc.nev >= GC_MAX_EVENTS)
        return 0;
    for (i = gnc.nev; i > 0u; i--) {
        gnc.ev[i] = gnc.ev[i - 1u];
        gnc.ev[i].offset = (uint16_t)(i / GC_MAX_AT_OFFSET);
    }
    gnc_fill(&gnc.ev[0], type, ti, 0, 0, 0, 0, 0);
    gnc.nev++;
    return 1;
}

static uint32_t gnc_new_id(uint32_t ti)
{
    uint32_t id = ++gnc.next_id[ti];
    if (!id)
        id = gnc.next_id[ti] = 1;
    return id;
}

/* a voice of a Gnomad engine (a token in Felucca's budget): s[0] its core note ID, s[1] the key sent, s[2] the mark
 * (HELD: the core holds the note; REL: released, the token kept while the core's release sounds), s[3] the release's
 * blocks left. A voice that last played a Felucca engine has other values there. */
#define GNC_HELD 0x474E4300
#define GNC_REL 0x474E4301
static uint32_t gnc_key(int32_t ce, uint32_t note)
{
    return ce == GC_DRUMS ? (note + 16u * 8u - 36u) % 16u : note & 127u;
}
/* round EW-FIXES: a kit's ghost block, MIDI 4..19 (pads 0..15 as above, below every General MIDI drum): the pad at half
 * the velocity (Easy's black keys on a Gnomad kit, gn_state.c GN_GC_GHOST; Advanced's keys past the sixteenth) */
#define GNC_GHOST 20u
static uint32_t gnc_vel(int32_t ce, const voice_t *v)   /* the velocity the core gets: 1..127 */
{
    uint32_t vel = v->vel > 127u ? 127u : v->vel;
    if (ce == GC_DRUMS && v->note < GNC_GHOST)
        vel >>= 1;
    return vel ? vel : 1u;
}
static void gnc_clear(voice_t *v) { v->s[0] = v->s[1] = v->s[2] = v->s[3] = 0; }

static void gnc_note_on(track_t *t, voice_t *v)
{
    uint32_t ti = (uint32_t)(t - trk) % NTRK, key;
    int32_t ce = gnc_engine(t->engine);
    if (v->s[2] == GNC_HELD && v->s[0])                 /* a retrigger of a held token: its note ends */
        gnc_push(GC_NOTE_OFF, ti, (uint32_t)v->s[0], 0, 0);
    gnc_clear(v);
    if (ce < 0 || !gnc.core)
        return;
    key = gnc_key(ce, v->note);
    v->s[0] = (int32_t)gnc_new_id(ti);
    v->s[1] = (int32_t)key;
    v->s[2] = GNC_HELD;
    gnc_push(GC_NOTE_ON, ti, (uint32_t)v->s[0], key, gnc_vel(ce, v));
}

/* voice.c, once a block, before the render: the token's life. The gate falls: NOTE_OFF, and the token stays (stage 2,
 * so the ADSR never ends it) for the release the bound patch states, so Felucca's budget counts the core's releasing
 * voice; then it ends. A kill (stage 4: given to another part) ends it at once (gnc_sweep sees the rest). */
static int gnc_done(track_t *t, voice_t *v)
{
    uint32_t ti = (uint32_t)(t - trk) % NTRK;
    int32_t ce = gnc_engine(t->engine);
    if (v->s[2] == GNC_REL) {
        if (v->stage == 4u || v->s[3]-- <= 0) {
            gnc_clear(v);
            return 1;
        }
        v->stage = 2;
        return 0;
    }
    if (v->s[2] != GNC_HELD || !v->s[0])
        return !v->gate;                                /* (no core note: the token ends with its gate) */
    if (!v->gate || v->stage >= 3u || ce < 0) {
        gnc_push(GC_NOTE_OFF, ti, (uint32_t)v->s[0], 0, 0);
        if (v->stage == 4u || ce < 0) {
            gnc_clear(v);
            return 1;
        }
        v->gate = 0;
        v->s[2] = GNC_REL;
        v->s[3] = gnc.tail[ti];
        v->stage = 2;
        return 0;
    }
    if ((uint32_t)v->s[1] != gnc_key(ce, v->note)) {    /* legato / glide to another note: OFF, then ON */
        gnc_push(GC_NOTE_OFF, ti, (uint32_t)v->s[0], 0, 0);
        v->s[0] = (int32_t)gnc_new_id(ti);
        v->s[1] = (int32_t)gnc_key(ce, v->note);
        gnc_push(GC_NOTE_ON, ti, (uint32_t)v->s[0], (uint32_t)v->s[1], gnc_vel(ce, v));
    }
    return 0;
}

static void gnc_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;     /* the core renders the track (gn_core_mix) */
}

static int32_t gnc_drum_keys(const track_t *t, uint32_t k)
{
    (void)t;
    return (int32_t)(k < 16u ? 36u + k : k - 12u);      /* key k: pad k (MIDI 36..51 -> pads 0..15); past the sixteenth */
                                                        /* (round EW-FIXES) pad k - 16 soft: the ghost block (GNC_GHOST) */
}

/* 0..127 -> 0..65535; BRIGHT's middle (64) is the core's neutral 32768 */
static int32_t gnc_macro(int32_t v, int centred)
{
    v = v < 0 ? 0 : v > 127 ? 127 : v;
    if (centred)
        return v <= 64 ? v * 512 : 32768 + (v - 64) * 32767 / 63;
    return (v * 65535 + 63) / 127;
}

static int32_t gnc_ctl(const track_t *t, uint32_t k)
{
    switch (k) {
    case GNC_BRIGHT: return gnc_macro(t->p[P_E0], 1);
    case GNC_WOBBLE: return gnc_macro(t->p[P_E1], 0);
    case GNC_ECHO: {                                   /* the track's sends: the core's own (Felucca's buses */
        int32_t e = gnc_macro(t->p[P_DLY], 0);         /* never see a Gnomad track: its part buffer is 0); */
        e += (e * gnc.emix) >> 8;                      /* round EW: DLY's MIX scales every Echo send */
        return e > 65535 ? 65535 : e;
    }
    case GNC_SPACE: {
        int32_t s = gnc_macro(t->p[P_REV], 0) * GN_CORE_SPACE_X;
        return s > 65535 ? 65535 : s;
    }
    case GNC_CHORUS: return gnc_macro(t->p[P_CHOR], 0);
    default: {                                          /* LEVEL (square law) and MUTE: the track gain */
        int32_t l = t->p[P_MUTE] ? 0 : t->p[P_LEVEL] < 0 ? 0 : t->p[P_LEVEL] > 127 ? 127 : t->p[P_LEVEL];
        return l * l * 65535 / (127 * 127);
    }
    }
}
static const uint16_t GNC_PARAM[GNC_NCTL] = {5, 6, 7, 8, 9, 0x100};

/* ------------------------------------------------- per-sound knobs (round MF) --- */
/* knob place s (0..3) of record rec, engine ce: the track setting its amount is kept in (*store), and whether it is
 * the sound's own (1: gc_macro_apply's writes, *params the core parameters they reach, bits, and *rest its rest
 * amount) or one of the controls (0: the default map's Bright, Wobble, Echo, Space, or an ECHO / SPACE recipe in any
 * place, which is that send). DRUMS: the kit's four (no map); a record without a map: the controls, as before */
static __attribute__((noinline)) int gnc_slot(const uint8_t *rec, int32_t ce, uint32_t s, uint32_t *store,
                                              uint64_t *params, uint32_t *rest)
{
    static const uint8_t NATIVE[4] = {P_E0, P_E1, P_DLY, P_REV};
    gc_macro_write w0[GC_MACRO_WRITES], w1[GC_MACRO_WRITES];
    uint32_t n, i;
    s &= 3u;
    *store = NATIVE[s];
    *params = (uint64_t)1 << (5u + s);
    *rest = 0;
    if (ce < 0 || ce == GC_DRUMS || !rec || !(rec[10] | rec[11] | rec[54] | rec[55]))
        return 0;
    n = gc_macro_apply((const gc_patch *)rec, s, 0, w0);
    if (!n || gc_macro_apply((const gc_patch *)rec, s, 65535, w1) != n)
        return 0;
    if (n == 1u && w0[0].parameter == w1[0].parameter && w0[0].value == 0 && w1[0].value == 65535) {
        if (w0[0].parameter == 7u || w0[0].parameter == 8u) {   /* ECHO, SPACE: the send itself */
            *store = w0[0].parameter == 7u ? P_DLY : P_REV;
            *params = (uint64_t)1 << w0[0].parameter;
            return 0;
        }
        if (w0[0].parameter == 5u + s && s < 2u)                /* the default Bright, Wobble */
            return 0;
    }
    *store = P_E0 + s;
    for (*params = 0, i = 0; i < n; i++)
        *params |= (uint64_t)1 << (w0[i].parameter & 63u);
    *rest = gc_macro_rest((const gc_patch *)rec, s);
    return 1;
}

/* the knob place (0..127) a sound's own knob rests at: the place nearest its rest amount, never an end the rest is
 * not (so both halves of the knob move it) */
static uint32_t gnc_kr(uint32_t rest)
{
    uint32_t kr = (rest * 127u + 32767u) / 65535u;
    if (rest && !kr)
        kr = 1;
    if (rest < 65535u && kr == 127u)
        kr = 126;
    return kr;
}

/* a sound's own knob at place v (0..127): 0 at 0, its rest amount exactly at its rest place kr, 65535 at 127 */
static int32_t gnc_amount(int32_t v, uint32_t rest, uint32_t kr)
{
    v = v < 0 ? 0 : v > 127 ? 127 : v;
    if ((uint32_t)v <= kr)
        return kr ? (int32_t)(((uint32_t)v * rest + kr / 2u) / kr) : 0;
    return (int32_t)(rest + (((uint32_t)v - kr) * (65535u - rest) + (127u - kr) / 2u) / (127u - kr));
}

/* the main loop's side of the sound's own knobs (gn_core_host): the plan of each track's bind, and a mailbox of one move
 * for the interrupt (gnc_sync_controls). One writer each way: the main loop writes a plan, then plan; a move, then
 * pending; the interrupt only clears pending (the interrupt runs inside the main loop, never the other way) */
static struct {
    volatile uint32_t plan[NTRK];          /* the bind (gnc.bseq) the plan below is for */
    uint8_t owned[NTRK];                   /* controls (GNC_* bits) the sound's own knobs own */
    uint64_t reach[NTRK];                  /* round EW: the parameters P0..P47 its own knobs' writes reach (no page's), */
    uint64_t kreach[NTRK][4];              /* .. each knob's, */
    uint8_t store[NTRK][4];                /* .. and the setting each knob's amount is kept in (EDIT 1, gn_core_pages.c) */
    volatile uint8_t pending[NTRK];        /* writes posted (0: none) */
    uint8_t g[NTRK];                       /* .. how many of them are the record group (first) */
    uint32_t mfor[NTRK];                   /* .. the bind they are for */
    gc_macro_write w[NTRK][GC_MACRO_WRITES];
    /* the main loop's own */
    uint32_t seen[NTRK];
    const uint8_t *rec[NTRK];
    uint8_t own[NTRK], kr[NTRK][4];
    uint16_t rest[NTRK][4];
    int32_t hsent[NTRK][4];
} gnh;

static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }

/* ------------------------------------------------------------ the presets (FW-CONTENT) --- */
/* each engine's authored slots, bank order: the bank IDs (gn_content.h's lists, generated from the banks). A preset is
 * its bank ID and nothing else (round FW-CONTENT-7: no preset_t a sound): its name, and the knobs it sets, are read
 * from the record when they are asked for (gn_preset_get, gn_preset_name below) */
/* Interim (FORK.md, round FW-CONTENT-5): an engine offers at most GN_PRESETS_MAX presets, the first in bank order,
 * to everything that reads a preset number (the screens, NAMES, PRESET, projects, favourites, the bind): NAMES and
 * PRESET carry 7-bit numbers. Until it is decided how the rest of a larger list is reached, they are not. */
#define GN_PRESETS_MAX 127u
#define GNC_CAP(n) ((n) < GN_PRESETS_MAX ? (n) : GN_PRESETS_MAX)
#define GNC_ID(id, b, w, c, d, r) (id),
static const uint16_t GNC_VA_IDS[] = {GN_CONTENT_VA(GNC_ID)};
static const uint16_t GNC_FM_IDS[] = {GN_CONTENT_FM(GNC_ID)};
static const uint16_t GNC_KIT_IDS[] = {GN_CONTENT_KITS(GNC_ID)};
static const uint16_t GNC_GNOME_IDS[] = {GN_CONTENT_GNOME(GNC_ID)};
static const uint16_t GNC_SAMPLE_IDS[] = {GN_CONTENT_SAMPLE(GNC_ID)};
/* n: the presets offered (capped); all: every authored one (round FW-CONTENT-6: Easy's sounds past the offered list,
 * gn_easy.c gn_preset_put, put the track's preset at their place in the whole list, which the bind reads) */
#define GNC_L(ids) {ids, GNC_CAP(NELEM(ids)), NELEM(ids)}
static const struct { const uint16_t *id; uint8_t n, all; } GNC_LIST[5] = {  /* by gc_engine: VA FM DRUMS GNOME SAMPLE */
    GNC_L(GNC_VA_IDS), GNC_L(GNC_FM_IDS), GNC_L(GNC_KIT_IDS), GNC_L(GNC_GNOME_IDS), GNC_L(GNC_SAMPLE_IDS)};
_Static_assert(GC_VA == 0 && GC_FM == 1 && GC_DRUMS == 2 && GC_GNOME == 3 && GC_SAMPLE == 4, "GNC_LIST's order");
_Static_assert(GN_PRESETS_MAX <= 127u, "a preset number is 7-bit on the wire (NAMES, PRESET) and a uint8_t "
               "(engine_t.npresets, track_t.preset)");
_Static_assert(NELEM(GNC_VA_IDS) < 255u && NELEM(GNC_FM_IDS) < 255u && NELEM(GNC_KIT_IDS) < 255u &&
               NELEM(GNC_GNOME_IDS) < 255u && NELEM(GNC_SAMPLE_IDS) < 255u, "a place in the whole list is a uint8_t "
               "(track_t.preset)");
_Static_assert(NELEM(GNC_VA_IDS) == GN_CONTENT_VA_COUNT && NELEM(GNC_FM_IDS) == GN_CONTENT_FM_COUNT &&
               NELEM(GNC_KIT_IDS) == GN_CONTENT_KITS_COUNT && NELEM(GNC_GNOME_IDS) == GN_CONTENT_GNOME_COUNT &&
               NELEM(GNC_SAMPLE_IDS) == GN_CONTENT_SAMPLE_COUNT, "each list is the generated census (gn_state.c's "
               "Easy checks, GN_NAUTH, count the same)");

/* engine ce's preset pi (as the track holds it: an offered preset modulo the offered count, as the UI and the
 * projects read it; an Easy sound past the offered list at its place in the whole list): its patch, and for DRUMS
 * its kit (the patch is then the core's drum patch) */
static const uint8_t *gnc_record(int32_t ce, uint32_t pi, const uint8_t **kit)
{
    uint32_t id = GNC_LIST[ce].id[pi < GNC_LIST[ce].all ? pi : pi % GNC_LIST[ce].n];
    if (ce == GC_DRUMS) {
        *kit = &GN_CONTENT_KITS[GC_KIT_BYTES * id];
        return GN_CORE_PATCH[GC_DRUMS];
    }
    *kit = 0;
    return &GN_CONTENT_PITCHED[GC_PATCH_BYTES * id];
}

/* blocks a released note's token is kept: the patch's Release (P4); FM's longest operator release (SPEC/11: P18+5j+4
 * bits 8..15, code x 40 ms, 0 = P4); a kit's longest pad decay; at most GN_CORE_TAIL_MAX_MS */
static uint16_t gnc_tail(int32_t ce, const uint8_t *patch, const uint8_t *kit)
{
    uint32_t ms = 0, p, rel = rd16(&patch[32u + 2u * 4u]);
    if (ce == GC_DRUMS)
        for (p = 0; p < 16u; p++) {
            uint32_t d = rd16(&kit[32u + 24u * p + 4u]);
            ms = d > ms ? d : ms;
        }
    else if (ce == GC_FM)
        for (p = 0; p < 6u; p++) {
            uint32_t code = rd16(&patch[32u + 2u * (18u + 5u * p + 4u)]) >> 8, d = code ? code * 40u : rel;
            ms = d > ms ? d : ms;
        }
    else
        ms = rel;
    ms = (ms > GN_CORE_TAIL_MAX_MS ? GN_CORE_TAIL_MAX_MS : ms) + 30u;
    return (uint16_t)(ms * (FS / 1000u) / CTL + 1u);
}

#include "gn_core_adv.c"                  /* round EW: the Advanced pages on the core (the main loop's side) */

/* once a block, first: engines and sounds of the four tracks (ALL_OFF for a switched track's old notes: before the
 * queue's notes, which are its new engine's) */
static void gnc_sync_engines(void)
{
    uint32_t ti, k;
    const uint8_t *patch, *kit;
    for (ti = 0; ti < NTRK; ti++) {
        const track_t *t = &trk[ti];
        int32_t ce = gnc_engine(t->engine);
        uint32_t want = ce < 0 ? 0u : t->engine + 1u;
        if (gnc.bound[ti] == want && (!want || gnc.preset[ti] == t->preset))
            continue;
        if (gnc.bound[ti])
            gnc_add(GC_ALL_OFF, ti, 0, 0, 0);
        gnc.bound[ti] = (uint8_t)want;
        gnc.preset[ti] = t->preset;
        for (k = 0; k < GNC_NCTL; k++)
            gnc.sent[ti][k] = -1;
        if (!++gnc.bseq[ti])                            /* round MF: a new bind for the main loop to plan */
            gnc.bseq[ti] = 1;
        gnc.mapped[ti] = 0;
        if (!want)
            continue;
        patch = gnc_record(ce, t->preset, &kit);
        gnc.mapped[ti] = (uint8_t)(ce != GC_DRUMS && (patch[10] | patch[11] | patch[54] | patch[55]));
        gnc.tail[ti] = gnc_tail(ce, patch, kit);
        if (gc_bind_track(gnc.core, (uint8_t)ti, (const gc_patch *)patch, (const gc_kit *)kit, &gnc.rep) != GC_OK) {
            gnc.errors++;
            gnc.last_status = gnc.rep.status;
            gnc.last_detail = gnc.rep.detail;
            gnc.bound[ti] = 0;                          /* (not bound: no control is sent; tried again next block) */
        }
#ifdef GN_CORE_BOUND
        else
            GN_CORE_BOUND(ti, patch, kit);              /* (tests: the record the core was given) */
#endif
    }
}

/* the queue into the block's list; 1 = all of it */
static int gnc_drain(void)
{
    uint32_t w = gnc.w;
    while (gnc.r != w && gnc.nev < GC_MAX_EVENTS) {
        gc_event *e = &gnc.ring[gnc.r & (GNC_RING - 1u)];
        gnc.ev[gnc.nev] = *e;
        gnc.ev[gnc.nev].offset = (uint16_t)(gnc.nev / GC_MAX_AT_OFFSET);
        gnc.nev++;
        gnc.r++;
    }
    return gnc.r == w;
}

/* after the queue: tokens voice.c ended without done() (its ADSR or a kill in the same block, an engine switch's
 * clear): a held note gets its NOTE_OFF; a releasing token given to another part, on a track that holds no note any
 * more, cuts the track's tails (PANIC), so the core never sounds more than Felucca's budget allows */
static void gnc_sweep(void)
{
    uint32_t ti, i;
    for (ti = 0; ti < NTRK; ti++) {
        int stolen = 0, held = 0;
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[ti].v[i];
            if (v->s[2] != GNC_HELD && v->s[2] != GNC_REL)
                continue;
            if (v->active) {
                held |= v->s[2] == GNC_HELD;
                continue;
            }
            if (v->s[2] == GNC_HELD) {
                if (!gnc_add(GC_NOTE_OFF, ti, (uint32_t)v->s[0], 0, 0))
                    continue;                           /* (the list is full: next block) */
                gnc.orphans++;
            } else {
                stolen = 1;
            }
            gnc_clear(v);
        }
        if (stolen && !held && gnc.bound[ti] && gnc_add_first(GC_PANIC, ti))   /* (before the block's new notes) */
            gnc.steals++;
    }
}

/* round EW: a posted batch w[0..n) for track ti (g0 first as one group, then g1 as one group, the rest streaming) onto
 * the block's list, whole or not at all: each group at one offset (SPEC/01: at most 16 events at an offset, and a
 * group is one only at one offset; gnc_add's offsets are the list's sixteens), else it waits a block */
static int gnc_batch(uint32_t ti, const gc_macro_write *w, uint32_t n, uint32_t g0, uint32_t g1)
{
    uint32_t i;
    if (gnc.nev + n > GC_MAX_EVENTS || gnc.nev % GC_MAX_AT_OFFSET + g0 > GC_MAX_AT_OFFSET ||
        (gnc.nev + g0) % GC_MAX_AT_OFFSET + g1 > GC_MAX_AT_OFFSET)
        return 0;
    for (i = 0; i < n; i++)
        gnc_add(GC_PARAMETER, ti, 0, w[i].parameter, w[i].value);
    return 1;
}

static void gnc_sync_controls(void)
{
    uint32_t ti, k, n, held, moved;
    for (ti = 0; ti < NTRK; ti++) {
        if (!gnc.bound[ti])
            continue;
        /* round MF: a sound with knobs of its own: the controls they own are not sent from their settings; until the
         * main loop has planned this bind (gn_core_host), none of P5..P9 is (the core plays the record as stored) */
        held = !gnc.mapped[ti] ? 0u : gnh.plan[ti] == gnc.bseq[ti] ? gnh.owned[ti] : 0x1Fu;
        for (k = 0; k < GNC_NCTL; k++) {
            int32_t v;
            if ((held >> k) & 1u)
                continue;
            v = gnc_ctl(&trk[ti], k);
            if (v != gnc.sent[ti][k] && gnc_add(GC_PARAMETER, ti, 0, GNC_PARAM[k], v))
                gnc.sent[ti][k] = v;
        }
        /* round MF: a move of a knob of the sound's own, its writes posted by the main loop for this bind. Its
         * record-group writes (P10..P47, first) are the track's one group of the block, all at one offset (SPEC/01:
         * the core rejects a block with two, and the whole block): a move that does not fit whole waits a block */
        moved = 0;
        n = gnh.pending[ti];
        if (n) {
            if (gnh.mfor[ti] != gnc.bseq[ti] || n > GC_MACRO_WRITES) {
                gnh.pending[ti] = 0;                    /* (planned for a sound no longer bound: dropped) */
            } else if (gnc_batch(ti, gnh.w[ti], n, gnh.g[ti], 0)) {
                moved = gnh.g[ti];
                RING_PUBLISH();
                gnh.pending[ti] = 0;
            } else {
                continue;
            }
        }
        /* round EW: the Advanced pages' batch (gn_core_adv.c): its record group never in a block with a move's */
        n = gna.pend[ti];
        if (!n)
            continue;
        if (gna.mfor[ti] != gnc.bseq[ti] || n > GNA_POST) {
            gna.pend[ti] = 0;                           /* (for a sound no longer bound: the main loop starts again) */
            continue;
        }
        if ((moved && gna.gp[ti]) || !gnc_batch(ti, gna.m[ti], n, gna.gp[ti], gna.gk[ti]))
            continue;
        RING_PUBLISH();
        gna.pend[ti] = 0;
    }
    n = gna.mpend;                                      /* round EW: the master controls (track 255) */
    if (n && n <= GNM_N && gnc_batch(255u, gna.mm, n, 0, 0)) {
        RING_PUBLISH();
        gna.mpend = 0;
    }
}

/* round MF, the main loop (gn_easy.c gn_input, every pass, Easy on or off): the sound's own knobs of each bound track
 * planned from its record (gnc_slot: host rate, as the core's gc_macro_* are; never in the interrupt), and a moved
 * knob's writes (gc_macro_apply on the flash record) posted for the interrupt, one move a track at a time */
static void gn_core_host(void)
{
    uint32_t ti, k;
    for (ti = 0; ti < NTRK; ti++) {
        uint32_t b = gnc.bseq[ti], be = gnc.bound[ti], bp = gnc.preset[ti];
        gna_settle(&trk[ti]);                           /* round EW: the pages' settings at the sound's own */
        if (!b || !be || gnc.bseq[ti] != b || gnc.bound[ti] != be || gnc.preset[ti] != bp)
            continue;                                   /* (none bound, or a bind under way: the next pass) */
        if (gnh.seen[ti] != b) {                        /* a new bind: its plan */
            const uint8_t *kit;
            int32_t ce = gnc_engine(be - 1u);
            uint8_t owned = 0;
            gnh.rec[ti] = ce < 0 ? 0 : gnc_record(ce, bp, &kit);
            gnh.own[ti] = 0;
            gnh.reach[ti] = 0;
            for (k = 0; k < 4u; k++) {
                uint32_t store, rest;
                uint64_t par;
                int own;
                gnh.hsent[ti][k] = -1;
                gnh.kreach[ti][k] = 0;
                own = gnc_slot(gnh.rec[ti], ce, k, &store, &par, &rest);
                gnh.store[ti][k] = (uint8_t)store;
                if (!own)
                    continue;
                gnh.own[ti] |= (uint8_t)(1u << k);
                gnh.rest[ti][k] = (uint16_t)rest;
                gnh.kr[ti][k] = (uint8_t)gnc_kr(rest);
                owned |= (uint8_t)(((par >> 5) & 0x1Fu) | (k < 2u ? 1u << k : 0u));   /* P5..P9; K1, K2's */
                gnh.kreach[ti][k] = par & ~(uint64_t)0x3E0;
                gnh.reach[ti] |= gnh.kreach[ti][k];
            }
            gnh.owned[ti] = owned;
            RING_PUBLISH();
            gnh.plan[ti] = b;
            gnh.seen[ti] = b;
        }
        gna_host_track(ti, b, gnc_engine(be - 1u), gnh.rec[ti], gnh.reach[ti]);   /* round EW: the pages */
        if (gnh.pending[ti])
            continue;                                   /* (the interrupt has not taken the last move yet) */
        for (k = 0; k < 4u; k++) {
            int32_t a;
            uint32_t n, g;
            if (!((gnh.own[ti] >> k) & 1u))
                continue;
            a = gnc_amount(trk[ti].p[P_E0 + k], gnh.rest[ti][k], gnh.kr[ti][k]);
            if (a == gnh.hsent[ti][k])
                continue;
            n = gc_macro_apply((const gc_patch *)gnh.rec[ti], k, (uint32_t)a, gnh.w[ti]);
            for (g = 0; g < n && gnh.w[ti][g].parameter >= 10u; g++)
                ;
            gnh.g[ti] = (uint8_t)g;
            gnh.mfor[ti] = b;
            gnh.hsent[ti][k] = a;
            RING_PUBLISH();
            gnh.pending[ti] = (uint8_t)n;
            break;
        }
    }
    gna_host_master();                                  /* round EW: DLY, REVERB, CHORUS */
}

/* audio.c audio_init: the core, before the audio interrupt runs (also called by the first block if not) */
static void gn_core_boot(void)
{
#define GNC_ASSET(id, off, frames, crc) {(id), &GN_CONTENT_PCM[(off)], (frames), (crc)},
    static const gc_sample_asset assets[] = {GN_CONTENT_ASSETS(GNC_ASSET)};   /* the factory PCM, in flash */
#undef GNC_ASSET
    /* the core's master controls (SPEC/20: 0x300..0x309) at their defaults but the wet level (GN_CORE_WET) */
    static const int32_t master[10] = {65535, 8000, 0, 0, 16538, 24576, 88, 49152, GN_CORE_WET, 35};
    static const uint16_t gains[4] = {65535, 65535, 65535, 65535};
    gc_config cfg;
    if (gnc.tried)
        return;
    gnc.tried = 1;
    cfg.api_version = GC_API_VERSION;
    cfg.profile = GC_PROFILE_FX1;
    cfg.state = gnc_state.b;
    cfg.state_bytes = sizeof gnc_state.b;
    cfg.effects = gnc_fx.b;
    cfg.effects_bytes = sizeof gnc_fx.b;
    cfg.assets = assets;
    cfg.asset_count = NELEM(assets);
    cfg.seed = 0;
    gnc.init_status = gc_init(&cfg, &gnc.core, &gnc.rep);
    if (gnc.init_status == GC_OK)
        gnc.init_status = gc_set_initial(gnc.core, master, gains, &gnc.rep);
    if (gnc.init_status != GC_OK)
        gnc.core = 0;                                   /* (the Gnomad engines stay silent; Felucca's play) */
}

/* fx.c mix_block, after Felucca's parts: the core's block into the dry mix */
static void gn_core_mix(int32_t *mix_l, int32_t *mix_r, uint32_t n)
{
    uint32_t i;
    if (!gnc.tried)
        gn_core_boot();
    if (!gnc.core || n > CTL)
        return;
    gnc.nev = 0;
    gnc_sync_engines();
    if (gnc_drain())
        gnc_sweep();                                    /* (after every queued ON: an OFF never precedes its note) */
    gnc_sync_controls();
    if (!gnc.nev && gnc.quiet >= GN_CORE_QUIET)
        return;                                         /* silent for 1 s, no voice, nothing new: no cost */
    if (gc_render_mono(gnc.core, gnc.out, (uint16_t)n, gnc.nev ? gnc.ev : 0, gnc.nev, &gnc.rep) != GC_OK) {
        gnc.errors++;                                   /* (the block's events are rejected as a whole: SPEC/01) */
        gnc.last_status = gnc.rep.status;
        gnc.last_detail = gnc.rep.detail;
        return;
    }
    gnc.renders++;
    {
        int32_t peak = 0;
        for (i = 0; i < n; i++) {
            int32_t s = (gnc.out[i] * GN_CORE_GAIN_Q8) >> 8;
            mix_l[i] += s;
            mix_r[i] += s;
            peak |= s;
        }
        gnc.quiet = peak || gnc.rep.voices_active ? 0u : gnc.quiet + (gnc.quiet < GN_CORE_QUIET);
    }
}

/* ---------------------------------------------------------------- the descriptors --- */
/* round MF: K3 and K4 keep a sound's own third and fourth knob (BRIT and WOBL are its first two, or Bright and Wobble);
 * round EW: EDIT 2 four more of the engine's own (gn_core_adv.c), each a move from the sound as authored (0: none) */
#define GNC_REL(l) {(l), F_BIPCT, -64, 63, 0, 0, 0}
#define GNC_EDIT(a, b, c, d) {{"BRIT", F_PCT, 0, 127, 64, 0, 0}, {"WOBL", F_PCT, 0, 127, 0, 0, 0}, \
                              {"K3", F_PCT, 0, 127, 0, 0, 0}, {"K4", F_PCT, 0, 127, 0, 0, 0}, a, b, c, d}
static const char *const GNC_N_TALK[] = {"OWN", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"};
static const char *const GNC_N_LOOP[] = {"OWN", "ONCE", "LOOP"};
#define GNC_TALK {"TALK", F_ENUM, 0, 12, 0, GNC_N_TALK, 0}       /* the Gnome's syllables: OWN, a cycle of 1..12 */
#define GNC_LOOP {"LOOP", F_ENUM, 0, 2, 0, GNC_N_LOOP, 0}        /* a sample: OWN, ONCE, LOOP */
#define GNC_PITCH {"PITCH", F_SEMI, -24, 24, 0, 0, 0}
/* the presets: the banks' (above). No preset_t table (.presets 0, round FW-CONTENT-7): every reader of a preset goes
 * through GN_PRESET / GN_PRESET_NAME (gn_size_flags.h), which reach gn_preset_get / gn_preset_name below */
#define GNC_ENGINE(sym, nm, ids, ed, ...) static const engine_t sym = { \
    .name = (nm), .page_title = {"MACRO", "MORE"}, .edit = ed, \
    .presets = 0, .npresets = GNC_CAP(NELEM(ids)), .knob = {P_E0, P_E1, P_DLY, P_REV}, \
    .note_on = gnc_note_on, .render = gnc_render, .ownenv = 1, .done = gnc_done, __VA_ARGS__}
GNC_ENGINE(ENG_GN_VA, "VA", GNC_VA_IDS, GNC_EDIT(GNC_REL("CUT"), GNC_REL("RES"), GNC_REL("DTUN"), GNC_REL("SUB")),
           .poly = 0);
GNC_ENGINE(ENG_GN_FM, "FM", GNC_FM_IDS, GNC_EDIT(GNC_REL("INDX"), GNC_REL("FDBK"), GNC_REL("RATIO"), GNC_REL("TOUCH")),
           .poly = 0);
GNC_ENGINE(ENG_GN_DRUMS, "DRUMS", GNC_KIT_IDS, GNC_EDIT(GNC_REL("TONE"), GNC_REL("DECAY"), GNC_REL("SNAP"),
           GNC_REL("KICK")), .oneshot = 1, .keys = gnc_drum_keys);
GNC_ENGINE(ENG_GN_GNOME, "GNOME", GNC_GNOME_IDS, GNC_EDIT(GNC_REL("VOWEL"), GNC_REL("SIZE"), GNC_REL("BRTH"),
           GNC_TALK), .poly = 0);
GNC_ENGINE(ENG_GN_SAMPLE, "SAMPLER", GNC_SAMPLE_IDS, GNC_EDIT(GNC_REL("START"), GNC_REL("END"),
           GNC_LOOP, GNC_PITCH),
           .poly = 0);

/* ------------------------------------------------- a preset, read from its record (FW-CONTENT-7) --- */
/* the core engine of descriptor e (slot 8 holds SAMPLER's), -1 = a Felucca engine */
static int32_t gnc_engine_of(const engine_t *e)
{
    return e == &ENG_GN_VA ? GC_VA : e == &ENG_GN_FM ? GC_FM : e == &ENG_GN_DRUMS ? GC_DRUMS :
           e == &ENG_GN_GNOME ? GC_GNOME : e == &ENG_GN_SAMPLE ? GC_SAMPLE : -1;
}

/* the knob (0..127) whose sent value comes nearest the stored macro m, the lower knob of two as near: the knob laws
 * of gnc_ctl (BRIGHT centred; Space's send x GN_CORE_SPACE_X, saturating). tools/gn_gen_content.py knobs() is the
 * same search, and gn_content.h's lists keep its answers (X(id, BRIT, WOBL, CHOR, DLY, REV)): gn_core_test checks
 * every preset against them */
enum { GNC_LAW_CENTRED, GNC_LAW_PLAIN, GNC_LAW_SPACE };
static __attribute__((noinline)) int8_t gnc_knob(uint32_t m, int law)
{
    int32_t k, best = 0;
    uint32_t near = 0xFFFFFFFFu;
    for (k = 0; k < 128; k++) {
        int32_t s = gnc_macro(k, law == GNC_LAW_CENTRED);
        uint32_t d;
        if (law == GNC_LAW_SPACE)
            s = s * GN_CORE_SPACE_X > 65535 ? 65535 : s * GN_CORE_SPACE_X;
        d = (uint32_t)s > m ? (uint32_t)s - m : m - (uint32_t)s;
        if (d < near) {
            near = d;
            best = k;
        }
    }
    return (int8_t)best;
}

/* the record of preset k of descriptor e, as gnc_record reads the track's preset (and *kit), 0 = a Felucca engine.
 * Out of line, as the readers below and gnc_knob: they serve the screens and the editor, never the audio path, and a
 * call is smaller than a copy (flash is what round FW-CONTENT-7 saves) */
static __attribute__((noinline)) const uint8_t *gnc_preset_rec(const engine_t *e, uint32_t k, const uint8_t **kit)
{
    int32_t ce = gnc_engine_of(e);
    return ce < 0 ? 0 : gnc_record(ce, k, kit);
}

/* GN_PRESET_NAME: preset k of engine e by name; a Gnomad preset's is its record's own (bytes 16..31, in flash) */
static __attribute__((noinline)) const char *gn_preset_name(const engine_t *e, uint32_t k)
{
    const uint8_t *kit, *patch = gnc_preset_rec(e, k, &kit);
    return patch ? (const char *)(kit ? kit : patch) + 16 : e->presets[k].name;
}

/* GN_PRESET_PAT: the pattern preset k of engine e suggests (core.h preset_t.pat); a Gnomad preset's is 0, as its
 * preset_t had it (the screens ask once a frame: no record read) */
static uint8_t gn_preset_pat(const engine_t *e, uint32_t k)
{
    return gnc_engine_of(e) < 0 ? e->presets[k].pat : 0u;
}

/* GN_PRESET: preset k of engine e. A Felucca engine's from its table; a Gnomad engine's written into p (the caller's,
 * GN_PRESET's compound literal: no RAM of its own) from the record, as its preset_t had it until round FW-CONTENT-7:
 * the name; BRIT, WOBL, CHOR, DLY and REV at the knobs nearest the record's stored Bright, Wobble, Chorus, Echo and
 * Space (P5..P9; a kit's are the drum patch's, the patch it binds with); ENV 0 64 127 127, DIST 0, the rest 0 */
static __attribute__((noinline)) const preset_t *gn_preset_get(const engine_t *e, uint32_t k, preset_t *p)
{
    const uint8_t *kit, *patch = gnc_preset_rec(e, k, &kit);
    if (!patch)
        return &e->presets[k];
    memset(p, 0, sizeof *p);
    p->name = (const char *)(kit ? kit : patch) + 16;
    p->e[0] = gnc_knob(rd16(&patch[32u + 2u * 5u]), GNC_LAW_CENTRED);  /* Bright */
    p->e[1] = gnc_knob(rd16(&patch[32u + 2u * 6u]), GNC_LAW_PLAIN);    /* Wobble */
    p->env[1] = 64;                                     /* (round EW: gna_settle puts ENV at the record's own on load) */
    p->env[2] = 127;
    p->env[3] = 127;
    p->fx[0] = 0 + 1;                                                   /* (stored + 1: core.h FX) DIST 0 */
    p->fx[1] = (uint8_t)(gnc_knob(rd16(&patch[32u + 2u * 9u]), GNC_LAW_PLAIN) + 1);  /* CHORUS: Chorus */
    p->fx[2] = (uint8_t)(gnc_knob(rd16(&patch[32u + 2u * 7u]), GNC_LAW_PLAIN) + 1);  /* DELAY: Echo */
    p->fx[3] = (uint8_t)(gnc_knob(rd16(&patch[32u + 2u * 8u]), GNC_LAW_SPACE) + 1);  /* REVERB: Space */
    {                                                   /* round MF: the sound's own knobs at their rest places */
        uint32_t s, store, rest;
        uint64_t par;
        for (s = 0; s < 4u; s++)
            if (!kit && gnc_slot(patch, gnc_engine_of(e), s, &store, &par, &rest))
                p->e[s] = (int8_t)gnc_kr(rest);
    }
    return p;
}

/* the core's figures for the console and the tests */
static void gn_core_report(gc_report *out)
{
    if (gnc.core)
        gc_get_report(gnc.core, out);
    else
        memset(out, 0, sizeof *out);
}

#ifndef GN_CORE_LIBCALLS
#define GN_CORE_LIBCALLS 0 /* the core has no 64-bit division since a624452 (CI 38078834949 reported __udivdi3 before) */
#endif
#if GN_CORE_LIBCALLS
/* Only if the pi32v2 link reports them missing (the firmware links no runtime library): 64-bit unsigned division
 * and remainder by shift and subtract, 32-bit operations only (FORK.md, "Gnomad engines": open questions) */
static uint64_t gnc_udivmod64(uint64_t n, uint64_t d, uint64_t *rem)
{
    uint64_t q = 0, r = 0;
    int i;
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= d) {
            r -= d;
            q |= (uint64_t)1 << i;
        }
    }
    if (rem)
        *rem = r;
    return q;
}
uint64_t __udivdi3(uint64_t n, uint64_t d) { return gnc_udivmod64(n, d, 0); }
uint64_t __umoddi3(uint64_t n, uint64_t d) { uint64_t r; gnc_udivmod64(n, d, &r); return r; }
int64_t __divdi3(int64_t n, int64_t d)
{
    uint64_t q = gnc_udivmod64(n < 0 ? 0u - (uint64_t)n : (uint64_t)n, d < 0 ? 0u - (uint64_t)d : (uint64_t)d, 0);
    return (n < 0) != (d < 0) ? -(int64_t)q : (int64_t)q;
}
int64_t __moddi3(int64_t n, int64_t d)
{
    uint64_t r;
    gnc_udivmod64(n < 0 ? 0u - (uint64_t)n : (uint64_t)n, d < 0 ? 0u - (uint64_t)d : (uint64_t)d, &r);
    return n < 0 ? -(int64_t)r : (int64_t)r;
}
#endif

#include "gn_core_pages.c"                /* round EW: the pages' knobs on a Gnomad sound (params.c, ui.c, the skin) */
