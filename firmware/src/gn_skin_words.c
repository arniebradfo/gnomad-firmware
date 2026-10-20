/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): the skin's words, phase 1 (option C, section 3's catalogue and section 10's wording, the
 * cell forms; the longer row forms come with phase 2's rows). Included by gn_skin.c. One lookup serves every
 * word site: gsk_word(context, id, form, fallback).
 *
 * Keys are never naked PAGES[] offsets. A knob's label is keyed by what it edits: a track parameter (P_*), a
 * shared one (G_*), an engine's own setting (its engine number and slot 0..7) or a card a page assembles itself
 * (the page's graph and the column), and it is trusted only while upstream still labels it with the code the
 * entry names: a descriptor variant is found by its code (LOFI's DUTY or WAV#), and a label upstream renames
 * falls back to upstream's text. A page title is keyed by the page's family, graph and first parameter (an
 * engine's EDIT pages by the engine and its page_title, HOME by the engine's name), again checked against the
 * code. A phrase (a message, a menu item, a hint, a confirmation) is found by upstream's own text: '#' in a
 * pattern stands for one character and '*' for the rest, both carried into the words unchanged, so names and
 * numbers keep their text. Anything not here is shown as upstream writes it; user content is never looked up
 * (cv_text_flags is not touched). */

enum { GSKW_LABEL, GSKW_TITLE, GSKW_PHRASE, GSKW_UNIT };          /* gsk_word's contexts */
enum { GSKW_CELL, GSKW_ROW };                                  /* its forms: phase 1 has the cell form (ROW = CELL) */

/* ---------------------------------------------------------- knob labels --- */
#define GSKK_TRK(id) ((uint16_t)(id))                        /* a track parameter, P_* */
#define GSKK_GLB(id) ((uint16_t)(0x100u | (id)))             /* a shared one, G_* */
#define GSKK_ENG(e, k) ((uint16_t)(0x200u | (e) << 3 | (k))) /* engine e's own setting k (P_E0 + k) */
#define GSKK_GR(g, c) ((uint16_t)(0x300u | (g) << 2 | (c)))  /* card c of a page that draws graph g itself */
#define GSKG_DRUM 22u                                        /* SEQ > STEP on a drum part: the grid's cards */
#define GSKG_LAYER(l) (24u + (l))                            /* the cards of a hold layer (LAYER_FX GLO EDIT) */
typedef struct { uint16_t key; const char *code, *word; } gsk_label_t;
static const gsk_label_t GSKW_LABELS[] = {
    /* ENV, ENV DEST; the dormant OP pages (FELUCCA_FM4) */
    {GSKK_TRK(P_ATK), "ATK", "Attack"}, {GSKK_TRK(P_DEC), "DEC", "Decay"}, {GSKK_TRK(P_SUS), "SUS", "Sustain"},
    {GSKK_TRK(P_REL), "REL", "Release"}, {GSKK_TRK(P_ED_FLT), "FLT", "Filter"}, {GSKK_TRK(P_ED_PIT), "PIT", "Pitch"},
    {GSKK_TRK(P_ED_SHP), "SHP", "Shape"},
#define GSKW_OP(k) {GSKK_TRK(P_FM##k##_ATK), "ATK", "Attack"}, {GSKK_TRK(P_FM##k##_DEC), "DEC", "Decay"}, \
                 {GSKK_TRK(P_FM##k##_SUS), "SUS", "Sustain"}, {GSKK_TRK(P_FM##k##_REL), "REL", "Release"}, \
                 {GSKK_TRK(P_FM##k##_LEVEL), "LVL", "Level"}
#if FELUCCA_FM4                                            /* (the OP pages: DIGITAL's, dormant in the default build) */
    GSKW_OP(1), GSKW_OP(2), GSKW_OP(3), GSKW_OP(4),
#endif
#undef GSKW_OP
    /* LFO, LFO DEST */
    {GSKK_TRK(P_LRATE), "RATE", "Speed"}, {GSKK_TRK(P_LWAVE), "WAVE", "Wave"}, {GSKK_TRK(P_LPHASE), "PHS", "Start"},
    {GSKK_TRK(P_LFADE), "FADE", "Fade"}, {GSKK_TRK(P_LD_PIT), "PIT", "Pitch"}, {GSKK_TRK(P_LD_FLT), "FLT", "Filter"},
    {GSKK_TRK(P_LD_SHP), "SHP", "Shape"}, {GSKK_TRK(P_LD_AMP), "AMP", "Volume"},
    /* FX, SLICER */
    {GSKK_TRK(P_DIST), "DST", "Drive"}, {GSKK_TRK(P_CHOR), "CHO", "Chorus"}, {GSKK_TRK(P_DLY), "DLY", "Delay"},
    {GSKK_TRK(P_REV), "REV", "Reverb"}, {GSKK_TRK(P_SLCR), "SLCR", "Mode"}, {GSKK_TRK(P_SLPAT), "PAT", "Pattern"},
    {GSKK_TRK(P_SLRATE), "RATE", "Division"}, {GSKK_TRK(P_SLDEPTH), "DEPTH", "Depth"},
    /* SCL, CHORD (and the SCL layer's four) */
    {GSKK_TRK(P_ROOT), "ROOT", "Root"}, {GSKK_TRK(P_SCALE), "SCL", "Scale"}, {GSKK_TRK(P_QUANT), "QNT", "Quantise"},
    {GSKK_TRK(P_TRANS), "TRN", "Shift"}, {GSKK_TRK(P_CHRD), "CHRD", "Chord"}, {GSKK_TRK(P_VOIC), "VOIC", "Voicing"},
    /* VOICE, VOICE 2 */
    {GSKK_TRK(P_VOICE), "VCE", "Voices"}, {GSKK_TRK(P_GLIDE), "GLD", "Glide"}, {GSKK_TRK(P_GLMODE), "GLMOD", "Mode"},
    {GSKK_TRK(P_PRIO), "PRIO", "Priority"}, {GSKK_TRK(P_ALLOC), "ALLOC", "Order"}, {GSKK_TRK(P_DETUNE), "DTUNE", "Detune"},
    {GSKK_TRK(P_PAN), "PAN", "Pan"}, {GSKK_TRK(P_MUTE), "MUTE", "Mute"},
    /* ARP, ARP 2, PATTERN */
    {GSKK_TRK(P_AMODE), "MODE", "Direction"}, {GSKK_TRK(P_ARATE), "RATE", "Division"}, {GSKK_TRK(P_AOCT), "OCT", "Octaves"},
    {GSKK_TRK(P_AGATE), "GATE", "Gate"}, {GSKK_TRK(P_ASWING), "SWG", "Swing"}, {GSKK_TRK(P_APROB), "PROB", "Chance"},
    {GSKK_TRK(P_AHOLD), "HOLD", "Hold"}, {GSKK_TRK(P_AORDER), "ORD", "Order"}, {GSKK_TRK(P_SLEN), "LEN", "Length"},
    {GSKK_TRK(P_SDIV), "DIV", "Division"}, {GSKK_TRK(P_SSWING), "SWG", "Swing"}, {GSKK_TRK(P_SGATE), "GATE", "Gate"},
    /* shared: DLY, REVERB, CHORUS, GLOBAL, SYSTEM (knob 1's card shows USB, its label upstream's), PROJECT */
    {GSKK_GLB(G_DTIME), "TIME", "Time"}, {GSKK_GLB(G_DFDBK), "FDBK", "Repeat"}, {GSKK_GLB(G_DCOLOR), "COLR", "Colour"},
    {GSKK_GLB(G_DMIX), "MIX", "Mix"}, {GSKK_GLB(G_RTYPE), "TYPE", "Type"}, {GSKK_GLB(G_RSIZE), "SIZE", "Size"},
    {GSKK_GLB(G_RDAMP), "DAMP", "Damping"}, {GSKK_GLB(G_CRATE), "CRT", "Speed"}, {GSKK_GLB(G_CDEPTH), "CDP", "Depth"},
    {GSKK_GLB(G_BPM), "BPM", "Tempo"}, {GSKK_GLB(G_SWING), "SWG", "Swing"}, {GSKK_GLB(G_CLOCK), "CLK", "Clock"},
    {GSKK_GLB(G_TUNE), "TUNE", "Tuning"}, {GSKK_GLB(G_MIDI), "USB", "USB"}, {GSKK_GLB(G_SYNC), "SYNC", "Sync"},
    {GSKK_GLB(G_ROUTE), "ROUT", "Routing"}, {GSKK_GLB(G_INFO), "CPU", "CPU load"}, {GSKK_GLB(G_SLOT), "SLOT", "Slot"},
    {GSKK_GLB(G_LOAD), "LOAD", "Load"}, {GSKK_GLB(G_SAVE), "SAVE", "Save"},
#if !GN_DROP_ANALOG
    /* the engines' own. Gnomad (FORK.md, size diet): a retired engine's (GN_DROP_<ENGINE>) never match: its slot's
     * descriptor labels each setting "-" (gn_size_stub.h) or, a Gnomad engine's, BRIT WOBL K3 K4 (gn_core.c) */
    /* 0 ANALOG */
    {GSKK_ENG(0, 0), "WAVE", "Wave"}, {GSKK_ENG(0, 1), "DTN", "Detune"}, {GSKK_ENG(0, 2), "MIX", "Mix"},
    {GSKK_ENG(0, 3), "NOIS", "Noise"}, {GSKK_ENG(0, 4), "CUT", "Cutoff"}, {GSKK_ENG(0, 5), "RES", "Peak"},
    {GSKK_ENG(0, 6), "DRV", "Drive"}, {GSKK_ENG(0, 7), "KTR", "Key track"},
#endif
#if !GN_DROP_PHASE
    /* 2 PHASE */
    {GSKK_ENG(2, 0), "WAVE", "Wave"}, {GSKK_ENG(2, 1), "WAVE2", "Wave 2"}, {GSKK_ENG(2, 2), "DCW", "Shape"},
    {GSKK_ENG(2, 3), "ENV", "Envelope"}, {GSKK_ENG(2, 4), "DTN", "Detune"}, {GSKK_ENG(2, 5), "LINE", "Blend"},
    {GSKK_ENG(2, 6), "SUB", "Sub"},
#endif
#if !GN_DROP_LOFI
    /* 3 LOFI: DUTY is WAV# on the wave bank, CRSH is DCY on the stepped chip */
    {GSKK_ENG(3, 0), "CHIP", "Chip"}, {GSKK_ENG(3, 1), "WAVE", "Wave"}, {GSKK_ENG(3, 2), "DUTY", "Duty"},
    {GSKK_ENG(3, 2), "WAV#", "Bank"}, {GSKK_ENG(3, 3), "CRSH", "Crush"}, {GSKK_ENG(3, 3), "DCY", "Decay"},
    {GSKK_ENG(3, 4), "SWP", "Sweep"}, {GSKK_ENG(3, 5), "VIB", "Vibrato"}, {GSKK_ENG(3, 6), "ARP", "Arpeggio"},
    {GSKK_ENG(3, 7), "TONE", "Tone"},
#endif
#if !GN_DROP_SAMPLE
    /* 4 SAMPLE */
    {GSKK_ENG(4, 0), "SET", "Set"}, {GSKK_ENG(4, 1), "TUNE", "Tune"}, {GSKK_ENG(4, 2), "BITS", "Crush"},
    {GSKK_ENG(4, 3), "LOOP", "Loop"}, {GSKK_ENG(4, 4), "CUT", "Cutoff"}, {GSKK_ENG(4, 6), "DRV", "Drive"},
#endif
#if !GN_DROP_VOICE
    /* 5 VOICE */
    {GSKK_ENG(5, 0), "VOWL", "Vowel"}, {GSKK_ENG(5, 1), "VOWL2", "Vowel 2"}, {GSKK_ENG(5, 2), "TALK", "Talk"},
    {GSKK_ENG(5, 3), "SHIFT", "Shift"}, {GSKK_ENG(5, 4), "BUZZ", "Buzz"}, {GSKK_ENG(5, 5), "BRTH", "Breath"},
    {GSKK_ENG(5, 6), "Q", "Sharp"}, {GSKK_ENG(5, 7), "RAND", "Random"},
#endif
#if !GN_DROP_TRIO
    /* 6 TRIO */
    {GSKK_ENG(6, 0), "WAVE", "Waves"}, {GSKK_ENG(6, 1), "INT2", "Interval 2"}, {GSKK_ENG(6, 2), "INT3", "Interval 3"},
    {GSKK_ENG(6, 3), "DTN", "Detune"}, {GSKK_ENG(6, 4), "MODE", "Filter"}, {GSKK_ENG(6, 5), "CUT", "Cutoff"},
    {GSKK_ENG(6, 6), "RES", "Peak"}, {GSKK_ENG(6, 7), "PW", "Width"},
#endif
#if !GN_DROP_WHEEL
    /* 7 WHEEL */
    {GSKK_ENG(7, 0), "REG", "Set"}, {GSKK_ENG(7, 1), "SUB", "Sub"}, {GSKK_ENG(7, 2), "BODY", "Body"},
    {GSKK_ENG(7, 3), "TOP", "Top"}, {GSKK_ENG(7, 4), "PERC", "Strike"}, {GSKK_ENG(7, 5), "CLICK", "Click"},
    {GSKK_ENG(7, 6), "DRV", "Drive"}, {GSKK_ENG(7, 7), "ROTR", "Rotor"},
#endif
#if !GN_DROP_GRAIN
    /* 8 GRAIN */
    {GSKK_ENG(8, 0), "SRC", "Source"}, {GSKK_ENG(8, 1), "POS", "Position"}, {GSKK_ENG(8, 2), "SIZE", "Size"},
    {GSKK_ENG(8, 3), "DENS", "Density"}, {GSKK_ENG(8, 4), "PTCH", "Pitch"}, {GSKK_ENG(8, 5), "SPRD", "Spread"},
    {GSKK_ENG(8, 6), "RAND", "Random"}, {GSKK_ENG(8, 7), "TONE", "Tone"},
#endif
#if !GN_DROP_PHYS
    /* 9 PHYS: the membrane's HARM and BEND, the sympathetic model's CHRD SYMP BUZZ */
    {GSKK_ENG(9, 0), "MODEL", "Model"}, {GSKK_ENG(9, 1), "STRC", "Shape"}, {GSKK_ENG(9, 1), "HARM", "Harm"},
    {GSKK_ENG(9, 1), "CHRD", "Chord"}, {GSKK_ENG(9, 2), "BRIT", "Bright"}, {GSKK_ENG(9, 3), "DAMP", "Damping"},
    {GSKK_ENG(9, 4), "POS", "Position"}, {GSKK_ENG(9, 4), "SYMP", "Coupling"}, {GSKK_ENG(9, 5), "ACC", "Accent"},
    {GSKK_ENG(9, 6), "BOW", "Bow"}, {GSKK_ENG(9, 6), "BEND", "Bend"}, {GSKK_ENG(9, 6), "BUZZ", "Buzz"},
    {GSKK_ENG(9, 7), "EXC", "Exciter"},
#endif
#if !GN_DROP_DRUM
    /* 10 DRUM */
    {GSKK_ENG(10, 0), "KIT", "Kit"}, {GSKK_ENG(10, 1), "TUNE", "Tune"}, {GSKK_ENG(10, 2), "TONE", "Tone"},
    {GSKK_ENG(10, 3), "DECY", "Decay"}, {GSKK_ENG(10, 4), "SNAP", "Snap"}, {GSKK_ENG(10, 5), "ACC", "Accent"},
    {GSKK_ENG(10, 6), "KICK", "Kick"}, {GSKK_ENG(10, 7), "DRV", "Drive"},
#endif
#if !GN_DROP_NOISE
    /* 11 NOISE: the register modes' LEN and CLK */
    {GSKK_ENG(11, 0), "MODE", "Mode"}, {GSKK_ENG(11, 1), "COLR", "Colour"}, {GSKK_ENG(11, 2), "FREQ", "Filter"},
    {GSKK_ENG(11, 3), "RES", "Peak"}, {GSKK_ENG(11, 4), "TRK", "Key track"}, {GSKK_ENG(11, 5), "DENS", "Density"},
    {GSKK_ENG(11, 5), "LEN", "Length"}, {GSKK_ENG(11, 6), "DRFT", "Drift"}, {GSKK_ENG(11, 7), "CRSH", "Crush"},
    {GSKK_ENG(11, 7), "CLK", "Clock"},
#endif
#if !GN_DROP_FM6
    /* 12 FM6 */
    {GSKK_ENG(12, 0), "ALG", "Algorithm"}, {GSKK_ENG(12, 1), "FB", "Repeat"}, {GSKK_ENG(12, 2), "MLVL", "Mod level"},
    {GSKK_ENG(12, 3), "MRAT", "Mod ratio"}, {GSKK_ENG(12, 4), "MEG", "Mod time"}, {GSKK_ENG(12, 5), "VMOD", "Velocity"},
    {GSKK_ENG(12, 6), "DTUN", "Detune"}, {GSKK_ENG(12, 7), "PTCH", "Patch"},
#endif
#if !GN_DROP_SLICE
    /* 13 SLICE */
    {GSKK_ENG(13, 0), "SRC", "Source"}, {GSKK_ENG(13, 1), "DIV", "Slices"}, {GSKK_ENG(13, 2), "START", "Start"},
    {GSKK_ENG(13, 3), "PTCH", "Pitch"}, {GSKK_ENG(13, 4), "MODE", "Mode"}, {GSKK_ENG(13, 5), "REV", "Reverse"},
    {GSKK_ENG(13, 6), "DCAY", "Decay"}, {GSKK_ENG(13, 7), "TONE", "Tone"},
#endif
#if GN_ENGINES
    /* the Gnomad engines' EDIT 2 (round EW, gn_core.c): 9 VA, 12 FM, 7 GNOME, 10 DRUMS, 13 SAMPLER; EDIT 1's four are
     * the sound's own names (gn_skin.c gsk_gn_name) */
    {GSKK_ENG(9, 4), "CUT", "Cutoff"}, {GSKK_ENG(9, 5), "RES", "Resonance"}, {GSKK_ENG(9, 6), "DTUN", "Detune"},
    {GSKK_ENG(9, 7), "SUB", "Sub"},
    {GSKK_ENG(12, 4), "INDX", "Depth"}, {GSKK_ENG(12, 5), "FDBK", "Grit"}, {GSKK_ENG(12, 6), "RATIO", "Ratio"},
    {GSKK_ENG(12, 7), "TOUCH", "Touch"},
    {GSKK_ENG(7, 4), "VOWEL", "Vowel"}, {GSKK_ENG(7, 5), "SIZE", "Size"}, {GSKK_ENG(7, 6), "BRTH", "Breath"},
    {GSKK_ENG(7, 7), "TALK", "Syllables"},
    {GSKK_ENG(10, 4), "TONE", "Tone"}, {GSKK_ENG(10, 5), "DECAY", "Decay"}, {GSKK_ENG(10, 6), "SNAP", "Snap"},
    {GSKK_ENG(10, 7), "KICK", "Kick pitch"},
    {GSKK_ENG(13, 4), "START", "Start"}, {GSKK_ENG(13, 5), "END", "End"}, {GSKK_ENG(13, 6), "LOOP", "Loop"},
    {GSKK_ENG(13, 7), "PITCH", "Pitch"},
#endif
    /* cards the pages assemble themselves (ui_draw.c draw_columns), by graph and column */
    {GSKK_GR(GR_SONG, 0), "ROW", "Row"}, {GSKK_GR(GR_SONG, 1), "PAT", "Pattern"}, {GSKK_GR(GR_SONG, 2), "REPS", "Repeat"},
    {GSKK_GR(GR_CHANCE, 0), "STEP", "Step"}, {GSKK_GR(GR_CHANCE, 1), "CHANCE", "Chance"},
    {GSKK_GR(GR_MOTION, 0), "PLAY", "Play"}, {GSKK_GR(GR_MOTION, 1), "EVENT", "Events"},
    {GSKK_GR(GR_MOTION, 3), "CLEAR", "Clear"},
    {GSKK_GR(GR_TOOLS, 0), "PAT", "Pattern"}, {GSKK_GR(GR_TOOLS, 1), "SOUND", "Sound"}, {GSKK_GR(GR_TOOLS, 2), "ROW", "Row"},
    {GSKK_GR(GR_TOOLS, 3), "SONG", "Song"},
    {GSKK_GR(GR_TRK, 0), "LEVEL", "Level"}, {GSKK_GR(GR_TRK, 1), "PAN", "Pan"}, {GSKK_GR(GR_TRK, 2), "REV", "Reverb"},
    {GSKK_GR(GR_TRK, 3), "MUTE", "Mute"},
    {GSKK_GR(GR_BROWSE, 0), "No.", "Sound"}, {GSKK_GR(GR_BROWSE, 1), "ENG", "Engine"},
    {GSKK_GR(GR_BROWSE, 2), "FAV", "Favourite"}, {GSKK_GR(GR_BROWSE, 3), "LIST", "List"},
    {GSKK_GR(GR_PATS, 0), "PAT", "Pattern"}, {GSKK_GR(GR_PATS, 1), "LOAD", "Load"},
    {GSKK_GR(GR_USER, 0), "SLOT", "Slot"}, {GSKK_GR(GR_USER, 1), "LOAD", "Load"}, {GSKK_GR(GR_USER, 2), "ERASE", "Erase"},
    {GSKK_GR(GR_USER, 3), "SAVE", "Save"},
    {GSKK_GR(GR_SLICES, 0), "SLICE", "Slice"}, {GSKK_GR(GR_SLICES, 1), "POS", "Position"},
    {GSKK_GR(GR_SLICES, 2), "SPLIT", "Split"}, {GSKK_GR(GR_SLICES, 3), "JOIN", "Join"},
    {GSKK_GR(GR_MOD, 0), "SLOT", "Slot"}, {GSKK_GR(GR_MOD, 1), "SRC", "Source"}, {GSKK_GR(GR_MOD, 2), "DST", "Target"},
    {GSKK_GR(GR_MOD, 3), "AMT", "Amount"},
    {GSKK_GR(GR_ROLL, 0), "STEP", "Step"}, {GSKK_GR(GR_ROLL, 1), "NOTE", "Note"}, {GSKK_GR(GR_ROLL, 2), "TIME", "Kind"},
    {GSKK_GR(GR_ROLL, 3), "FLAG", "Style"},
    {GSKK_GR(GSKG_DRUM, 0), "STEP", "Step"}, {GSKK_GR(GSKG_DRUM, 1), "LANE", "Drum"}, {GSKK_GR(GSKG_DRUM, 2), "HIT", "Hit"},
    {GSKK_GR(GSKG_DRUM, 3), "ACC", "Accent"},
    /* the hold layers' own (ui_layer.c layer_cards; GLO's four: each part's level) */
    {GSKK_GR(GSKG_LAYER(LAYER_FX), 0), "FILTER", "Filter"}, {GSKK_GR(GSKG_LAYER(LAYER_FX), 1), "CRUSH", "Crush"},
    {GSKK_GR(GSKG_LAYER(LAYER_FX), 2), "THROW", "Send"}, {GSKK_GR(GSKG_LAYER(LAYER_FX), 3), "DEPTH", "Depth"},
    {GSKK_GR(GSKG_LAYER(LAYER_FX), 3), "SHIMR", "Shimmer"},
    {GSKK_GR(GSKG_LAYER(LAYER_GLO), 0), "LEVEL", "Part 1"}, {GSKK_GR(GSKG_LAYER(LAYER_GLO), 1), "LEVEL", "Part 2"},
    {GSKK_GR(GSKG_LAYER(LAYER_GLO), 2), "LEVEL", "Part 3"}, {GSKK_GR(GSKG_LAYER(LAYER_GLO), 3), "LEVEL", "Part 4"},
    {GSKK_GR(GSKG_LAYER(LAYER_EDIT), 0), "ENG", "Engine"}, {GSKK_GR(GSKG_LAYER(LAYER_EDIT), 1), "No.", "Sound"},
    {GSKK_GR(GSKG_LAYER(LAYER_EDIT), 2), "FAV", "Favourite"},
};

/* ---------------------------------------------------------- page titles --- */
/* a page {family, graph, its first parameter id}; an engine's {GSKT_EDIT1 / GSKT_EDIT2 / GSKT_HOME, engine, 0} */
#define GSKT_EDIT1 0xE0u
#define GSKT_EDIT2 0xE1u
#define GSKT_HOME 0xE2u
typedef struct { uint8_t a, b, c; const char *code, *word; } gsk_title_t;
static const gsk_title_t GSKW_TITLES[] = {
    {FAM_ENV, GR_ADSR, P_ATK, "ENV", "Envelope"}, {FAM_ENV, GR_NONE, P_ED_FLT, "ENV DEST", "Envelope amounts"},
    {FAM_LFO, GR_LFO, P_LRATE, "LFO", "Wobble"}, {FAM_LFO, GR_NONE, P_LD_PIT, "LFO DEST", "Wobble amounts"},
    {FAM_LFO, GR_MOD, 0xFF, "MOD", "Modulation"}, {FAM_FX, GR_FX, P_DIST, "FX", "Effects"},
    {FAM_FX, GR_SLCR, P_SLCR, "SLICER", "Rhythmic cuts"}, {FAM_FX, GR_NONE, G_DTIME, "DLY", "Delay"},
    {FAM_FX, GR_NONE, G_RTYPE, "REVERB", "Reverb"}, {FAM_FX, GR_NONE, G_CRATE, "CHORUS", "Chorus"},
    {FAM_SCL, GR_SCALE, P_ROOT, "SCL", "Scale"}, {FAM_SCL, GR_CHORD, P_CHRD, "CHORD", "Chord keys"},
    {FAM_EDIT, GR_SLICES, 0xFF, "SLICES", "Sample slices"},
#if FELUCCA_FM4
    {FAM_EDIT, GR_ADSR, P_FM1_ATK, "OP1 ENV", "Operator 1 envelope"},
    {FAM_EDIT, GR_ADSR, P_FM2_ATK, "OP2 ENV", "Operator 2 envelope"},
    {FAM_EDIT, GR_ADSR, P_FM3_ATK, "OP3 ENV", "Operator 3 envelope"},
    {FAM_EDIT, GR_ADSR, P_FM4_ATK, "OP4 ENV", "Operator 4 envelope"},
    {FAM_EDIT, GR_NONE, P_FM1_LEVEL, "OP LEVEL", "Operator levels"},
#endif
    {FAM_EDIT, GR_NONE, P_VOICE, "VOICE", "Voices"}, {FAM_EDIT, GR_NONE, P_ALLOC, "VOICE 2", "Voice mix"},
    {FAM_GLO, GR_NONE, G_BPM, "GLOBAL", "Global settings"}, {FAM_GLO, GR_NONE, G_MIDI, "SYSTEM", "System"},
    {FAM_SAVE, GR_BROWSE, 0xFF, "PRESETS", "Sounds"}, {FAM_SAVE, GR_USER, 0xFF, "USER", "User sounds"},
    {FAM_SAVE, GR_SLOTS, G_SLOT, "PROJECT", "Projects"}, {FAM_SAVE, GR_TOOLS, G_CLRSEQ, "TOOLS", "Clear and reset"},
    {FAM_ARP, GR_ARP, P_AMODE, "ARP", "Arpeggio"}, {FAM_ARP, GR_NONE, P_ASWING, "ARP 2", "Arpeggio options"},
    {FAM_SEQ, GR_ROLL, 0, "STEP", "Notes"}, {FAM_SEQ, GR_ROLL, 0, "GRID", "Drum grid"},
    {FAM_SEQ, GR_STEPS, P_SLEN, "PATTERN", "Pattern"}, {FAM_SEQ, GR_PATS, 0xFF, "PHRASES", "Phrases"},
    {FAM_TRK, GR_TRK, 0, "MIXER", "Mixer"}, {FAM_SEQ, GR_SONG, 0xFF, "SONG", "Song"},
    {FAM_SEQ, GR_CHANCE, 0xFF, "CHANCE", "Step chance"}, {FAM_SEQ, GR_MOTION, 0xFF, "MOTION", "Knob recording"},
    /* the engines: EDIT 1, EDIT 2 (their page_title), HOME (their name). Gnomad (FORK.md, size diet): a retired engine's
     * EDIT titles never match (its slot's descriptor titles its pages "-", a Gnomad engine's "MACRO" and "-"); its HOME
     * title still does (the silent descriptor keeps the name) unless a Gnomad engine took the slot (GN_ENGINES) */
#define GSKW_EDIT(e, t1, w1, t2, w2) {GSKT_EDIT1, e, 0, t1, w1}, {GSKT_EDIT2, e, 0, t2, w2},
#define GSKW_HOME(e, name, home) {GSKT_HOME, e, 0, name, home},
#if !GN_DROP_ANALOG
    GSKW_EDIT(0, "OSC", "Oscillators", "FLT", "Filter")
#endif
    GSKW_HOME(0, "ANALOG", "Analog")
#if !GN_DROP_PHASE
    GSKW_EDIT(2, "PHS", "Phase shape", "LINE", "Wave blend")
#endif
    GSKW_HOME(2, "PHASE", "Phase")
#if !GN_DROP_LOFI
    GSKW_EDIT(3, "CHIP", "Chip", "MOTN", "Pitch motion")
#endif
    GSKW_HOME(3, "LOFI", "Lofi")
#if !GN_DROP_SAMPLE
    GSKW_EDIT(4, "SET", "Sample set", "TONE", "Sample tone")
#endif
    GSKW_HOME(4, "SAMPLE", "Sample")
#if !GN_DROP_VOICE
    GSKW_EDIT(5, "VOWL", "Vowels", "TONE", "Voice tone")
#endif
    GSKW_HOME(5, "VOICE", "Voice")
#if !GN_DROP_TRIO
    GSKW_EDIT(6, "OSC", "Three waves", "TONE", "Trio tone")
#endif
    GSKW_HOME(6, "TRIO", "Trio")
#if !GN_DROP_WHEEL
    GSKW_EDIT(7, "BARS", "Drawbars", "TONE", "Organ tone")
#endif
#if !GN_ENGINES
    GSKW_HOME(7, "WHEEL", "Organ")
#endif
#if !GN_DROP_GRAIN
    GSKW_EDIT(8, "GRAN", "Grains", "SPRY", "Spray")
#endif
#if !GN_ENGINES
    GSKW_HOME(8, "GRAIN", "Grain")
#endif
#if !GN_DROP_PHYS
    GSKW_EDIT(9, "BODY", "Body", "EXCT", "Excitation")
#endif
#if !GN_ENGINES
    GSKW_HOME(9, "PHYS", "Physical")
#endif
#if !GN_DROP_DRUM
    GSKW_EDIT(10, "KIT", "Kit", "HIT", "Drum attack")
#endif
#if !GN_ENGINES
    GSKW_HOME(10, "DRUM", "Drum kit")
#endif
#if !GN_DROP_NOISE
    GSKW_EDIT(11, "SRC", "Noise source", "MOVE", "Noise motion")
#endif
    GSKW_HOME(11, "NOISE", "Noise")
#if !GN_DROP_FM6
    GSKW_EDIT(12, "OPS", "Operators", "PATCH", "Patch")
#endif
#if !GN_ENGINES
    GSKW_HOME(12, "FM6", "Six operators")
#endif
#if !GN_DROP_SLICE
    GSKW_EDIT(13, "SLCE", "Slice source", "PLAY", "Slice playback")
#endif
#if !GN_ENGINES
    GSKW_HOME(13, "SLICE", "Sliced sample")
#endif
#if GN_ENGINES
    /* round EW: the Gnomad engines say what they are, and what their two EDIT pages hold */
    GSKW_EDIT(9, "MACRO", "Synth knobs", "MORE", "Synth filter")
    GSKW_EDIT(12, "MACRO", "FM knobs", "MORE", "FM tone")
    GSKW_EDIT(7, "MACRO", "Gnome knobs", "MORE", "Gnome voice")
    GSKW_EDIT(10, "MACRO", "Kit knobs", "MORE", "Kit shape")
    GSKW_EDIT(13, "MACRO", "Sample knobs", "MORE", "Sample play")
#endif
#undef GSKW_EDIT
#undef GSKW_HOME
};

/* ------------------------------------------------------------ phrases --- */
/* upstream's own fixed text -> ours, looked up only where upstream draws its own wording, in that site's table
 * (GSKC_*); the first match wins; '#' one character, '*' the rest, carried over unchanged (a message's slot,
 * part, keycap or pattern tag and name). User content is never looked up: a dialog's question and NAME's title
 * are built from upstream's kind (GSKW_CONFIRM, GSKW_NAMETITLE), the name beside them is drawn as it is. */
typedef struct { const char *up, *word; } gsk_phrase_t;
static const gsk_phrase_t GSKW_PH_MSG[] = {          /* the header: ui_message / ui_say, the hold layers' names */
    {"NOTHING TO UNDO", "Nothing to undo"}, {"NOTHING TO DELETE", "Nothing to delete"},
    {"NOTHING TO CLEAR", "Nothing to clear"}, {"MOTION FULL", "Knob memory full"}, {"NO FAVORITES", "No favourites"},
    {"ADD A SONG ROW", "Add a song row"}, {"STOP FIRST", "Stop first"}, {"STOP TO SAVE", "Stop to save"},
    {"STOP TO EDIT", "Stop to edit"}, {"STOP TO RECORD", "Stop to record"}, {"STOP TO UNDO", "Stop to undo"},
    {"UPDATE CANCELLED", "Update cancelled"}, {"SETUP CANCELLED", "Setup cancelled"}, {"EMPTY SLOT", "Empty slot"},
    {"LOADED", "Loaded"}, {"SAVED", "Saved"}, {"RENAMED", "Renamed"}, {"SAVED (RAM)", "Temporary save only"},
    {"ERASED (RAM)", "Temporarily erased"}, {"RENAMED (RAM)", "Temporarily renamed"}, {"SAVE ERROR", "Save failed"},
    {"SAVE FORMAT ERROR", "Save failed"}, {"ERASE ERROR", "Erase failed"}, {"SLICES SAVED", "Slices saved"},
    {"TOO LONG TO SAVE", "Too long to save"}, {"KEYS RECORD LIVE", "Keys record live"},
    {"LANE KEYS RECORD", "Drum keys record"}, {"MOTION CLEARED", "Motion cleared"},
    {"PATTERN CLEARED", "Pattern cleared"}, {"STEP CLEARED", "Step cleared"}, {"ROW DELETED", "Row deleted"},
    {"SONG CLEARED", "Song cleared"}, {"SOUND INIT", "Sound reset"}, {"[SEQ] TO RECORD", "Press [SEQ] to record"},
    {"MIX PUT BACK", "Mix restored"}, {"SCALE PUT BACK", "Scale restored"}, {"TAP: CLK IS EXT", "Tap: external clock"},
    {"RESTART: CLK IS EXT", "Restart: external clock"}, {"NAME FULL", "Name full"}, {"DIV MAN", "Manual slices"},
    {"FIRST SLICE", "First slice"}, {"PICK A SLICE", "Pick a slice"}, {"SAMPLE TOO SHORT", "Sample too short"},
    {"32 SLICES AT MOST", "Maximum 32 slices"}, {"SLICE TOO SHORT", "Slice too short"},
    {"SRC USR1-3 TO EDIT", "Choose User1-3 to edit"}, {"USR# EMPTY", "User# empty"},
    {"MIDI IN -> T#", "MIDI to part #"}, {"UNDO/REDO T#", "Undo/redo part #"}, {"TRACK # CLEARED", "Part # cleared"},
    {"PATTERN # EMPTY", "Pattern # empty"}, {"HOLD [*] QUICK", "Hold [*] for shortcuts"},
    {"LOADED *", "Loaded *"}, {"SAVED *", "Saved *"}, {"ERASED *", "Erased *"}, {"RENAMED *", "Renamed *"},
    {"ENGINE *", "Engine *"},
    {"[FX] HOLD", "[FX] Live effects"}, {"[GLO] SET", "[GLO] Quick mix"}, {"[SCL] SET", "[SCL] Key and chord"},
    {"[EDIT] SET", "[EDIT] Choose engine"},
};
static const gsk_phrase_t GSKW_PH_FOOT[] = {         /* the footers' key hints: actions, OCT-, the grid; the layers' */
    {"LOAD", "Load"}, {"SAVE", "Save"}, {"ERASE", "Erase"}, {"CLEAR", "Clear"}, {"INIT", "Reset"},
    {"DELETE", "Delete"}, {"STOP", "Stop"}, {"PLAY", "Play"}, {"SPLIT", "Split"}, {"JOIN", "Join"},
    {"BACK", "Back"}, {"CANCEL", "Cancel"}, {"NAME", "Rename"}, {"PAGE *", "Page *"}, {"ACCENT", "Accent"},
    {"STEPS", "Steps"}, {"EFFECTS", "Effects"}, {"MACROS", "Macros"}, {"RESTART", "Restart"}, {"UNDO", "Undo"},
    {"DONE", "Done"}, {"ROOT", "Root"}, {"ENGINE", "Engine"},
};
static const gsk_phrase_t GSKW_PH_LAYER[] = {        /* the hold layers' cells (ui_layer.c lcell: GLO's, EDIT's INIT) */
    {"SOLO", "Solo"}, {"ALL", "All"}, {"TAP", "Tap"}, {"INIT", "Reset"},
};
static const gsk_phrase_t GSKW_PH_NOTE[] = {         /* the panel's notes (ui_graph.c panel_note) and captions */
    {"NO FAVORITES", "No favourites"}, {"LIST ALL TO ADD SOUNDS", "Choose All to add sounds"},
    {"TURN TO PICK", "Turn to pick"}, {"[OCT+] CONFIRM", "[OCT+] Confirm"}, {"NO SAMPLE", "No sample"},
    {"SRC: BREAK OR USR1-3", "Break or User1-3"}, {"CHORD KEYS OFF", "Chord keys off"},
    {"[K1] DIA3: IN-KEY CHORDS", "[K1] Turn for chords"}, {"NO CHORDS ON KITS", "No chords on kits"},
    {"[K2] ADD PATTERN", "[K2] Add pattern"}, {"[SAVE] PROJECT A-D", "[SAVE] Project A-D"}, {"LAST", "Last"},
    {"ON ROOT", "On root"}, {"ROOT ONLY", "Root only"}, {"+ ADD PATTERN", "+ Add pattern"},
    {"NOT SAVED", "Not saved"}, {"LEFT", "left"}, {"LEN", "Length"},
};
static const gsk_phrase_t GSKW_PH_MENU[] = {         /* the menu (ui_menu.c) */
    {"MENU", "Menu"}, {"ABOUT / CREDITS", "About / Credits"}, {"COLOR", "Colour"}, {"SPEAKER", "Speaker"},
    {"HOLD", "Hold time"}, {"LEDS", "Lights"}, {"CALIBRATION", "Calibration"}, {"ABOUT", "About"}, {"BACK", "Back"},
    {"OFF", "Off"}, {"LOWCUT", "Low cut"}, {"BASS+", "Bass boost"}, {"INV", "Inverted"}, {"DIM", "Dim"},
    {"MOVE", "Move"}, {"OK", "Confirm"}, {"SCROLL", "Scroll"},
};
static const gsk_phrase_t GSKW_PH_NAME[] = {         /* NAME (ui_name.c): its notes and keys */
    {"TAP AGAIN: NEXT", "Tap again: next"}, {"TAP AGAIN: NEXT LETTER", "Tap again: next letter"},
    {"ONE TAP, ONE CHARACTER", "One tap, one character"}, {"SPACE", "Space"}, {"DEL", "Delete"},
    {"SAVE", "Save"}, {"RENAME", "Rename"}, {"CANCEL", "Cancel"}, {"MOVE", "Cursor"}, {"CHAR", "Letter"},
    {"TYPE", "Type"},
};
static const gsk_phrase_t GSKW_PH_SETUP[] = {        /* calibration (ui_input.c setup_title, setup_show) */
    {"HARDWARE CALIBRATION", "Teach controls"}, {"TEACH EACH BUTTON AND KNOB", "Press each button, turn each knob"},
    {"PRESS", "Press"}, {"TURN RIGHT", "Turn right"},
};
static const gsk_phrase_t GSKW_PH_CONFIRM[] = {      /* the dialog's buttons; the unnamed project's explanation */
    {"NO", "No"}, {"YES", "Yes"}, {"SONG PATTERN CHANGES", "Song pattern changes"},
};
/* (an engine dropped: its value is never drawn, gn_skin.c gsk_value: no table) */
#if !GN_DROP_LOFI
static const gsk_phrase_t GSKW_PH_DCY[] = {{"CONST", "Constant"}, {"D*", "Decay*"}, {"L*", "Loop*"}};   /* LOFI's DCY */
#define GSKW_PH_DCY_E {GSKW_PH_DCY, NELEM(GSKW_PH_DCY)}
#else
#define GSKW_PH_DCY_E {0, 0}
#endif
#if !GN_DROP_FM6
static const gsk_phrase_t GSKW_PH_PATCH[] = {{"F*", "Factory*"}, {"B*", "Bank*"}};                       /* FM6's PTCH */
#define GSKW_PH_PATCH_E {GSKW_PH_PATCH, NELEM(GSKW_PH_PATCH)}
#else
#define GSKW_PH_PATCH_E {0, 0}
#endif
static const struct { const gsk_phrase_t *t; uint8_t n; } GSKW_PH[GSKC_N] = {
    {GSKW_PH_MSG, NELEM(GSKW_PH_MSG)}, {GSKW_PH_FOOT, NELEM(GSKW_PH_FOOT)}, {GSKW_PH_LAYER, NELEM(GSKW_PH_LAYER)},
    {GSKW_PH_NOTE, NELEM(GSKW_PH_NOTE)}, {GSKW_PH_MENU, NELEM(GSKW_PH_MENU)}, {GSKW_PH_NAME, NELEM(GSKW_PH_NAME)},
    {GSKW_PH_SETUP, NELEM(GSKW_PH_SETUP)}, {GSKW_PH_CONFIRM, NELEM(GSKW_PH_CONFIRM)}, GSKW_PH_DCY_E,
    GSKW_PH_PATCH_E,
};
/* a dialog's question by its kind (ui.confirm), '#' its part, project or slot; NAME's title by its kind */
static const char *const GSKW_CF[] = {
    [CF_CLEAR_SEQ] = "Clear part # pattern?", [CF_CLEAR_TRK] = "Clear part #?", [CF_OVR_PROJ] = "Replace project #?",
    [CF_OVR_USER] = "Replace #?", [CF_LOAD_PAT] = "Replace part # pattern?", [CF_DEL_ROW] = "Delete song row?",
    [CF_CLEAR_SONG] = "Clear song order?", [CF_INIT_SOUND] = "Reset sound?",
    [CF_CLEAR_MOTION] = "Clear part # knob moves?", [CF_ERASE_USER] = "Erase #?",
};
static const char *const GSKW_NT[] = {
    [NK_USER_SAVE] = "Save #", [NK_USER_RENAME] = "Rename #", [NK_PROJ_SAVE] = "Save project #",
    [NK_PROJ_RENAME] = "Rename project #",
};

/* -------------------------------------------------------------- choices --- */
/* a choice list of upstream's (the descriptor's names) -> its words in the same order, packed ("Off\0On"); an
 * empty word keeps upstream's token, on purpose (no plain word fits Leo's card; FORK.md lists them; phase 2's
 * rows say them in full). Data stays data: sample sets, FM6 patch and algorithm numbers, noise registers. */
typedef struct { const char *const *up; const char *w; uint8_t n; } gsk_choice_t;
#define GSKW_C(a, w) {a, w, NELEM(a)}
static const gsk_choice_t GSKW_CHOICES[] = {
    GSKW_C(N_ONOFF, "Off\0On"), GSKW_C(N_ONOFF_E, "Off\0On"),
    GSKW_C(N_LWAVE, "Sine\0Triangle\0Saw\0Square\0Random"),
    GSKW_C(N_AMODE, "Off\0Up\0Down\0Up/down\0Random\0In order\0Repeat"),
    GSKW_C(N_DIV, "1/4\0001/8\0001/16\0001/32\0001/8T\0001/16T\0001/2\0001/1\0002 bars\0004 bars"),
    GSKW_C(N_SLDIV, "1/8\0001/16\0001/32\0001/8T\0001/16T\0001/32T"),
    GSKW_C(N_SCALE, "Chrom.\0Major\0Minor\0Dorian\0Mixolyd.\0Maj pent\0Min pent\0Harm min\0Phrygian\0Lydian\0"
                    "Locrian\0Mel min\0Blues\0Whole tn\0Dim h/w\0Dim w/h"),
    GSKW_C(N_QUANT, "Off\0Snap\0White\0Seq snap"),
    GSKW_C(N_CHRD, "Off\0Scale3\0Scale7\0Major\0Minor\0Dom7\0Major7\0Minor7\0Sus4\0Power"),
    GSKW_C(N_VOIC, "Close\0Open\0Invert1\0Invert2\0Bass oct"),
    GSKW_C(N_VOICE, "Poly\0Mono\0Legato\0Unison"), GSKW_C(N_GLMODE, "Rate\0Time"),
    GSKW_C(N_PRIO, "Last\0Lowest\0Highest"), GSKW_C(N_ALLOC, "Rotate\0Reuse"), GSKW_C(N_ORDER, "Notes\0Played"),
    GSKW_C(N_CLOCK, "Internal\0USB\0TRS"), GSKW_C(N_ROUTE, "Channels\0Selected"), GSKW_C(N_RTYPE, "Room\0Spring"),
    GSKW_C(N_SLCR, "Off\0Gate\0Stutter"),
    GSKW_C(N_MSRC, "Off\0Wobble\0Envelope\0Velocity\0Key\0Random\0Wheel\0Pressure\0Express"),
    GSKW_C(N_MDST, "Off\0Pitch\0Cutoff\0Shape\0Volume\0Pan\0Drive\0Chorus\0Delay\0Reverb\0Speed\0Vibrato\0\0\0\0\0\0\0\0"),
#if !GN_DROP_ANALOG
    GSKW_C(N_ANALOG_WAVE, "Saw\0Square\0Triangle\0Sine\0PWM"),
#endif
#if !GN_DROP_PHASE
    GSKW_C(N_PD_WAVE, "Saw\0Square\0Pulse\0Dbl sine\0\0Res saw\0Res tri\0Res trap"),
    GSKW_C(N_PD_WAVE2, "Off\0Saw\0Square\0Pulse\0Dbl sine\0\0Res saw\0Res tri\0Res trap"),
    GSKW_C(N_PD_LINE, "Mix\0Ring"),
#endif
#if !GN_DROP_LOFI
    GSKW_C(N_CHIP, "Four bit\0Half rate\0Eight bit\0One bit\0Stepped"),
    GSKW_C(N_RWAVE, "Pulse\0Triangle\0Saw\0Noise\0Bank"), GSKW_C(N_RARP, "Off\0Octave\0Major\0Minor"),
    {N_WRAM, "\0\0\0Sine\0Triangle\0Saw\0Saw oct\0Bass1\0Bass2\0Organ\0Hollow\0Vowel A\0Vowel O\0Vowel E\0Buzz\0"
             "Staircase", 16},
#endif
#if !GN_DROP_TRIO
    GSKW_C(N_TRIO_WAVE, "3 saws\0003 pulses\0Pulse+tri\0Saw+tri\0003 tris\0Saw/tri\0\0\0Noise\0Saw sync\0\0Trio sync\0Ring\0"
                        "Trio ring\0\0"),
    GSKW_C(N_TRIO_MODE, "Low\0Band\0High\0Notch"),
#endif
#if !GN_DROP_WHEEL
    GSKW_C(N_DRW_REG, "Flute\0Mellow\0Hollow\0Smooth\0003 bars\0Blues\0Gospel\0Rock\0Tops\0Clarinet\0Reed\0Strings\0"
                      "Chapel\0Bright\0Bass\0Full"),
    GSKW_C(N_DRW_PERC, "Off\0Second\0Third\0Soft2\0Soft3\0Slow2\0Slow3"), GSKW_C(N_DRW_ROTR, "Off\0Slow\0Fast"),
#endif
#if !GN_DROP_PHYS
    GSKW_C(N_PHYS_MODEL, "Modal\0String\0\0Sympath."), {N_PHYS_CHORD, "Octave\0Fifth\0Fourth\0Major\0Minor\0Sus\0Seventh\0Root", 8},
#endif
#if !GN_DROP_DRUM
    GSKW_C(N_DRUM_KIT, "Standard\0Hand\0Cymbals\0"), GSKW_C(N_DRUM_KICK, "Punch\0Round"),
#endif
#if !GN_DROP_NOISE
    GSKW_C(N_NOISE_MODE, "Analog\0Dust\0Register\0Metal"),
#endif
#if !GN_DROP_FM6
    {N_FM6_ALG, "Patch", 1},
#endif
#if FELUCCA_SLICE && !GN_DROP_SLICE
    GSKW_C(N_SLC_SRC, "Break\0User1\0User2\0User3"), GSKW_C(N_SLC_DIV, "\0\0\0\0Auto\0Manual"),
    GSKW_C(N_SLC_MODE, "One shot\0Gate\0Loop"), GSKW_C(N_SLC_REV, "Off\0On"),
#endif
};
#undef GSKW_C
/* the cards a page assembles itself (ui_draw.c draw_columns, ui_layer.c layer_cards): their upstream tokens, in
 * words ("TOKEN\0Word\0..."); MOD's source and target are the choice lists above */
typedef struct { uint16_t key; const char *pairs; } gsk_cardval_t;
#define GSKW_OO "ON\0On\0OFF\0Off\0"
static const gsk_cardval_t GSKW_CARDS[] = {
    {GSKK_GR(GR_TRK, 0), GSKW_OO}, {GSKK_GR(GR_TRK, 3), GSKW_OO}, {GSKK_GR(GR_BROWSE, 2), GSKW_OO},
    {GSKK_GR(GR_BROWSE, 3), "FAV\0Favs\0ALL\0All\0"}, {GSKK_GR(GR_MOTION, 0), GSKW_OO},
    {GSKK_GR(GR_ROLL, 2), "NOTE\0Note\0TIE\0Tie\0REST\0Rest\0"},
    {GSKK_GR(GR_ROLL, 3), "-\0Plain\0ACC\0Accent\0SLD\0Slide\0A+S\0Both\0"},
    {GSKK_GR(GSKG_DRUM, 1), "KICK\0Kick\0SNARE\0Snare\0CLAP\0Clap\0HATCL\0Hi-hat\0HATOP\0Open hat\0TOM\0Tom\0RIM\0Rim\0"
                            "BELL\0Bell\0CONGA\0Conga\0CLAVE\0Clave\0CYM\0Cymbal\0"},
    {GSKK_GR(GSKG_DRUM, 2), GSKW_OO}, {GSKK_GR(GSKG_DRUM, 3), GSKW_OO}, {GSKK_GR(GR_SLICES, 0), "END\0End\0"},
    {GSKK_GLB(G_MIDI), "OFF\0Off\0WAIT\0Waiting\0"},
    {GSKK_GR(GSKG_LAYER(LAYER_FX), 0), GSKW_OO}, {GSKK_GR(GSKG_LAYER(LAYER_FX), 1), GSKW_OO},
    {GSKK_GR(GSKG_LAYER(LAYER_FX), 2), GSKW_OO}, {GSKK_GR(GSKG_LAYER(LAYER_FX), 3), GSKW_OO},
    {GSKK_GR(GSKG_LAYER(LAYER_GLO), 0), GSKW_OO}, {GSKK_GR(GSKG_LAYER(LAYER_GLO), 1), GSKW_OO},
    {GSKK_GR(GSKG_LAYER(LAYER_GLO), 2), GSKW_OO}, {GSKK_GR(GSKG_LAYER(LAYER_GLO), 3), GSKW_OO},
    {GSKK_GR(GSKG_LAYER(LAYER_EDIT), 2), GSKW_OO},
};
#undef GSKW_OO

/* --------------------------------------------------------------- units --- */
/* (DTN's "ct" stays: "127 cents" is 52 px, wider than Leo's card; the row form, phase 2, says cents) */
static const gsk_phrase_t GSKW_UNITS[] = {{"STEP", "steps"}, {"S", "s"}};

/* s against pattern p ('#': one character, '*': the rest; p's other characters exact): 1 = a match, *wild the
 * text the wildcard stood for (its length in *nw) */
GSK_MIN static int gsk_match(const char *p, const char *s, const char **wild, uint32_t *nw)
{
    *wild = 0;
    *nw = 0;
    for (; *p; p++, s++) {
        if (*p == '*') {
            *wild = s;
            *nw = str_len(s);
            for (p++; *p; p++) {                     /* the pattern's tail: the end of s */
                uint32_t tl = str_len(p);
                if (*nw < tl || !str_eq(s + *nw - tl, p))
                    return 0;
                *nw -= tl;
                return *nw > 0;
            }
            return *nw > 0;
        }
        if (!*s)
            return 0;
        if (*p == '#') {
            *wild = s;
            *nw = 1;
        } else if (*p != *s) {
            return 0;
        }
    }
    return !*s;
}
/* template w with its '#' / '*' replaced by n characters of d, into b (bn bytes) */
GSK_MIN static const char *gsk_fill(const char *w, const char *d, uint32_t n, char *b, uint32_t bn)
{
    uint32_t k = 0, j;
    for (; *w && k + 1u < bn; w++)
        if (*w == '#' || *w == '*')
            for (j = 0; j < n && d[j] && k + 1u < bn; j++)
                b[k++] = d[j];
        else
            b[k++] = *w;
    b[k] = 0;
    return b;
}
/* the k-th word of a packed list of n ("" past its end) */
GSK_MIN static const char *gsk_nth(const char *w, uint32_t n, uint32_t k)
{
    if (!w || k >= n)
        return "";
    while (k--)
        w += str_len(w) + 1u;
    return w;
}

/* the one lookup: gsk_word(context, id, form, fallback) -> the words, or fallback (upstream's text) when there are
 * none. context GSKW_*; id: LABEL the card's key, TITLE GSKW_TKEY, PHRASE the site (GSKC_*), UNIT 0, CHOICE the
 * list (GSKW_CHOICES index) << 8 | the choice's index, CARD the card's key, CONFIRM the dialog's kind, NAMETITLE
 * NAME's kind; form GSKW_CELL / GSKW_ROW (phase 1: the cell form). LABEL, TITLE and CHOICE check fallback against
 * upstream's code; a phrase with a wildcard, CONFIRM and NAMETITLE return a template the caller fills (gsk_fill)
 * or, for PHRASE, made here in gsk.made (valid until the next such lookup: every caller draws or copies it) */
enum { GSKW_CHOICE = GSKW_UNIT + 1, GSKW_CARD, GSKW_CONFIRM, GSKW_NAMETITLE };
#define GSKW_TKEY(a, b, c) ((uint32_t)(a) << 16 | (uint32_t)(b) << 8 | (uint32_t)(c))
GSK_MIN static const char *gsk_word(uint32_t context, uint32_t id, uint32_t form, const char *fallback)
{
    const char *w, *wild;
    uint32_t i, nw;
    (void)form;
    if (!fallback)
        return fallback;
    switch (context) {
    case GSKW_LABEL:
        for (i = 0; i < NELEM(GSKW_LABELS); i++)
            if (GSKW_LABELS[i].key == id && str_eq(GSKW_LABELS[i].code, fallback))
                return GSKW_LABELS[i].word;
        break;
    case GSKW_TITLE:
        for (i = 0; i < NELEM(GSKW_TITLES); i++) {
            const gsk_title_t *t = &GSKW_TITLES[i];
            if (GSKW_TKEY(t->a, t->b, t->c) == id && str_eq(t->code, fallback))
                return t->word;
        }
        break;
    case GSKW_UNIT:
        for (i = 0; i < NELEM(GSKW_UNITS); i++)
            if (str_eq(GSKW_UNITS[i].up, fallback))
                return GSKW_UNITS[i].word;
        break;
    case GSKW_CHOICE:                                /* fallback: the token upstream drew (format: 5 characters) */
        if ((id >> 8) < NELEM(GSKW_CHOICES)) {
            const gsk_choice_t *c = &GSKW_CHOICES[id >> 8];
            uint32_t k = id & 255u;
            char t[6];
            if (k < c->n) {
                str_cpy(t, c->up[k], sizeof t);
                w = gsk_nth(c->w, c->n, k);
                if (w[0] && str_eq(t, fallback))
                    return w;
            }
        }
        break;
    case GSKW_CARD:
        for (i = 0; i < NELEM(GSKW_CARDS); i++)
            if (GSKW_CARDS[i].key == id)
                for (w = GSKW_CARDS[i].pairs; *w; w += str_len(w) + 1u) {
                    const char *v = w + str_len(w) + 1u;
                    if (str_eq(w, fallback))
                        return v;
                    w = v;
                }
        break;
    case GSKW_CONFIRM:
        if (id < NELEM(GSKW_CF) && GSKW_CF[id])
            return GSKW_CF[id];
        break;
    case GSKW_NAMETITLE:
        if (id < NELEM(GSKW_NT) && GSKW_NT[id])
            return GSKW_NT[id];
        break;
    default:                                         /* GSKW_PHRASE: the site's table */
        if (id >= GSKC_N)
            break;
        for (i = 0; i < GSKW_PH[id].n; i++)
            if (gsk_match(GSKW_PH[id].t[i].up, fallback, &wild, &nw))
                return wild ? gsk_fill(GSKW_PH[id].t[i].word, wild, nw, gsk.made, sizeof gsk.made) : GSKW_PH[id].t[i].word;
        break;
    }
    return fallback;
}
/* the index of upstream's choice list names in GSKW_CHOICES, or 255 */
GSK_MIN static uint32_t gsk_choices_of(const char *const *names)
{
    uint32_t i;
    for (i = 0; i < NELEM(GSKW_CHOICES); i++)
        if (GSKW_CHOICES[i].up == names)
            return i;
    return 255u;
}
