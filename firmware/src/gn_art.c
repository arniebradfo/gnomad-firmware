/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kyle Palaniuk */
/* Gnomad (FORK.md): the pictures on the synth's Easy screens, as the design boards draw them
 * (the design boards): SVG path data copied from the boards' markup and script
 * data, drawn by gn_gfx.c. Ours, drawn for Gnomad; none of it comes from Felucca's assets.
 *   the four knob icons (FM1-Easy-Play): Bright (a sun), Wobble (a wave), Echo (three rings),
 *     Space (an arch), 24 units, drawn at 14 px
 *   the status band's icons (FM1-Easy-Play, FM1-Easy-Sounds, FM1-Easy-Loop)
 *   the eight sounds' pictures (Easy-Sounds: 100 x 110 units; the Whale as FM1-Easy-Sounds
 *     draws it) and the Gnome (Gnome-Sheet, FM1-Gnome: 120 x 160 units)
 *   the sixteen drum pads' pictures (Easy-Loop: 24 units)
 *   the Gnomad mark (Gnome-Sheet, FM1-Saved, FM1-Boot) */

/* ------------------------------------------------------------- icons --- */
static const char GA_SUN_RAYS[] = "M12 2.5v2.5 M12 19v2.5 M2.5 12h2.5 M19 12h2.5 M5.3 5.3l1.8 1.8 M16.9 16.9l1.8 1.8 "
                                  "M5.3 18.7l1.8-1.8 M16.9 7.1l1.8-1.8";
static const char GA_WAVE[] = "M2 12 Q6.5 3 11 12 T20 12 T22 10";
static const char GA_ARCH[] = "M3.5 21V12a8.5 8.5 0 0 1 17 0v9 M8 21v-6a4 4 0 0 1 8 0v6";
static const char GA_LOOP[] = "M19 12a7 7 0 1 1-2.05-4.95 M19.5 4v4h-4";
static const char GA_NOTE[] = "M9 18V6.5l10-2.5v12";
static const char GA_DRUM[] = "M4 9c0-1.6 3.6-3 8-3s8 1.4 8 3-3.6 3-8 3-8-1.4-8-3z M4 9v7c0 1.6 3.6 3 8 3s8-1.4 8-3V9";
static const char GA_METRO[] = "M7.5 21h9l-3.2-17h-2.6z";
static const char *const GA_METRO_ARM[3] = {"M12 15.5L14.2 7.2", "M12 15.5l5-7", "M12 15.5L19 10.6"};   /* slow, medium, fast */
static const char GA_LINK[] = "M10 14a4 4 0 0 0 5.7 0l3-3a4 4 0 0 0-5.7-5.7l-1 1 M14 10a4 4 0 0 0-5.7 0l-3 3a4 4 0 0 0 5.7 5.7l1-1";
static const char GA_DIAL_ARROWS[] = "M16.34 229.43 A10 10 0 0 1 16.34 216.57 M13.44 217.34 L16.34 216.57 L16.08 219.56 "
                                     "M31.66 229.43 A10 10 0 0 0 31.66 216.57 M34.56 217.34 L31.66 216.57 L31.92 219.56";
static const char GA_CHECK[] = "M5 12.5l4.5 4.5L19 7.5";
static const char GA_LOCK[] = "M6.5 11h11v9h-11z M8.5 11V8a3.5 3.5 0 0 1 7 0v3";
static const char GA_KEYTAR[] = "M3 6h18v12H3z M8 6v7 M13 6v7 M18 6v7 M5.5 6v5 M10.5 6v5 M15.5 6v5";
static const char GA_RIBBON[] = "M3 10h18v4H3z M7 10v4";
static const char GA_PADS[] = "M4 4h7v7H4z M13 4h7v7h-7z M4 13h7v7H4z M13 13h7v7h-7z";

/* --------------------------------------------- the eight sounds' pictures --- */
/* Easy-Sounds draws them in a 100 x 110 box (x 10..110); the FM-1 screen places them with
 * translate(68 72) scale(.87), the Robot's place on FM1-Easy-Play, in lines 2.2 wide */
static const char GA_ORGAN[] =
    "M21.5 80V44a4 4 0 0 1 8 0V80 M33 80V32a4 4 0 0 1 8 0V80 M44.5 80V22a4 4 0 0 1 8 0V80 M56 80V14a4 4 0 0 1 8 0V80 "
    "M67.5 80V22a4 4 0 0 1 8 0V80 M79 80V32a4 4 0 0 1 8 0V80 M90.5 80V44a4 4 0 0 1 8 0V80 "
    "M23.5 70l2 2.5 2-2.5 M35 70l2 2.5 2-2.5 M46.5 70l2 2.5 2-2.5 M58 70l2 2.5 2-2.5 M69.5 70l2 2.5 2-2.5 "
    "M81 70l2 2.5 2-2.5 M92.5 70l2 2.5 2-2.5 "
    "M16 80H104V102a4 4 0 0 1-4 4H20a4 4 0 0 1-4-4z M24 88H96V97H24z M33 88v9 M42 88v9 M51 88v9 M60 88v9 M69 88v9 "
    "M78 88v9 M87 88v9";
static const char GA_BELLS[] =
    "M16 86C21 80 23 72 23 60a15 15 0 0 1 30 0C53 72 55 80 60 86Z M34.5 90a3.5 3.5 0 1 0 7 0a3.5 3.5 0 1 0-7 0 "
    "M38 45V39 M35 36a3 3 0 1 0 6 0a3 3 0 1 0-6 0 "
    "M60 86C65 80 67 72 67 60a15 15 0 0 1 30 0C97 72 99 80 104 86Z M78.5 90a3.5 3.5 0 1 0 7 0a3.5 3.5 0 1 0-7 0 "
    "M82 45V39 M79 36a3 3 0 1 0 6 0a3 3 0 1 0-6 0 "
    "M38 33Q48 20 60 22Q72 20 82 33 M60 22c-4-7-13-8-13-2s9 4 13 2 M60 22c4-7 13-8 13-2s-9 4-13 2 M60 22l-5 9 "
    "M60 22l5 9 M14 56q-4 7 0 14 M106 56q4 7 0 14";
static const char GA_LASER[] = "M60 95v4 M60 103v4";
static const uint16_t GA_INVADER[8] = {                 /* Easy-Sounds' LASER pixels: 11 wide, bit 10 = left */
    0x104, 0x088, 0x1FC, 0x376, 0x7FF, 0x5FD, 0x505, 0x0D8,
};
static const char GA_CHOIR[] =
    "M15 66a15 15 0 1 0 30 0a15 15 0 1 0-30 0 M45 54a15 15 0 1 0 30 0a15 15 0 1 0-30 0 "
    "M75 66a15 15 0 1 0 30 0a15 15 0 1 0-30 0 "
    "M23 62q2.5-3 5 0 M32 62q2.5-3 5 0 M53 50q2.5-3 5 0 M62 50q2.5-3 5 0 M83 62q2.5-3 5 0 M92 62q2.5-3 5 0 "
    "M26.5 73a3.5 3.5 0 1 0 7 0a3.5 3.5 0 1 0-7 0 M56.5 61a3.5 3.5 0 1 0 7 0a3.5 3.5 0 1 0-7 0 "
    "M86.5 73a3.5 3.5 0 1 0 7 0a3.5 3.5 0 1 0-7 0 "
    "M16 98c0-8 6-13 14-13s14 5 14 13 M46 98V86c0-7 6-12 14-12s14 5 14 12v12 M76 98c0-8 6-13 14-13s14 5 14 13 "
    "M12 98H108 M27 26v12 M27 26q5 2 6 7 M98 20v13 M98 20q5 2 6 7";
static const char GA_CLOUD[] = "M34 76A13 13 0 1 1 38 51A19 19 0 0 1 75 48.3A15 15 0 1 1 84 76Z";
static const uint8_t GA_RAIN[9][3] = {                  /* x, y, r x 10 */
    {36, 86, 20}, {52, 88, 22}, {68, 87, 18}, {84, 88, 20}, {44, 96, 18}, {60, 98, 22}, {76, 97, 18},
    {52, 106, 16}, {68, 106, 16},
};
static const char GA_PIANO[] =
    "M28 44H92L106 84H14Z M14 84V94H106V84 M28 44V30a4 4 0 0 1 4-4H88a4 4 0 0 1 4 4V44 "
    "M37.1 44L27.1 84 M46.3 44L40.3 84 M55.4 44L53.4 84 M64.6 44L66.6 84 M73.7 44L79.7 84 M82.9 44L92.9 84 "
    "M22 94v12 M98 94v12";
static const char GA_PIANO_KEYS[] = "M34.4 44H39.9L34.6 68H27.7Z M43.6 44H49.1L46.2 68H39.3Z M61.9 44H67.4L69.3 68H62.4Z "
                                    "M71 44H76.5L80.8 68H73.9Z M80.2 44H85.7L92.4 68H85.5Z";
/* FUNNY (round CR): drawn for Gnomad in the pictures' box and line: a face laughing so hard its eyes squeeze shut, the
 * mouth wide open (the tongue filled: GA_FUNNY_TONGUE), shaking with it (the marks to each side) */
static const char GA_FUNNY[] =
    "M26 62a34 34 0 1 0 68 0a34 34 0 1 0-68 0 M40 45l10 6-10 6 M80 45l-10 6 10 6 M37 67H83Q81 93 60 93Q39 93 37 67Z "
    "M16 50q-5 8 0 16 M104 50q5 8 0 16 M20 35l-6-5 M100 35l6-5 M20 89l-6 5 M100 89l6 5";
static const char GA_FUNNY_TONGUE[] = "M49 86Q60 77 71 86Q67 92 60 92Q53 92 49 86Z";
/* another sound (none of the eight): a wave in a circle, drawn for Gnomad in the pictures' box */
static const char GA_OTHER[] = "M60 18a44 44 0 1 1 0 88a44 44 0 1 1 0-88 M30 62c6-20 14-20 20 0s14 20 20 0 14-20 20 0";
/* the Whale as FM1-Easy-Sounds draws it: translate(70 74) scale(.8) */
static const char GA_WHALE_SPOUT[] = "M40 41 V25 M40 25 C37 17 31 14 26 16 M40 25 C43 17 49 14 54 16";
static const char GA_WHALE_BODY[] =
    "M8 74 C8 54 28 42 52 42 C72 42 84 52 90 64 C94 58 98 52 100 44 C94 42 90 37 89 31 C95 31 100 34 103 39 "
    "C106 34 111 31 117 31 C116 37 112 42 106 45 C104 56 98 66 92 78 C88 94 72 102 52 102 C28 102 8 92 8 74 Z";
static const char GA_WHALE_LINES[] = "M11 81 C18 87 29 88 38 84 M46 84 C47 92 53 97 61 97 C58 93 54 88 52 84";

/* ---------------------------------------------------------- the Gnome --- */
/* Gnome-Sheet: 120 x 160 units, drawn back to front; FM1-Gnome places him with
 * translate(81.6 66.1) scale(.64) and keylines 2.4 wide */
static const char GA_G_COAT[] = "M36 104 L28 146 H92 L84 104 Z";
static const char GA_G_BELT[] = "M30 134 H90";
static const char GA_G_ARMS_SING[] = "M37 107 Q24 119 21 128 M83 107 Q96 119 99 128";
static const char GA_G_ARMS_CHEER[] = "M37 104 Q22 88 26 72 M83 104 Q98 88 94 72";
static const char GA_G_FACE[] = "M38 76 Q38 102 60 102 Q82 102 82 76 Z";
static const char GA_G_BEARD[] = "M35 86 Q34 116 60 128 Q86 116 85 86 Q72 99 60 99 Q48 99 35 86 Z";
static const char GA_G_EYES_SING[] = "M47 82 Q50 79 53 82 M67 82 Q70 79 73 82";
static const char GA_G_MOUTH_SHUT[] = "M54 111 Q60 114 66 111";
static const char GA_G_MOUTH_I[] = "M51 108 Q60 117 69 108 Q60 111 51 108 Z";
static const char GA_G_HAT[] = "M26 70 Q60 80 94 70 L68 12 Z";
static const char GA_G_TIP[] = "M68 12 Q59 3 48 9";
static const char GA_NOTE_FLAG[] = "M2.2 0 V-9 Q6.2 -7.8 6.8 -4.4";   /* a floating note: head at the origin */

/* the Gnomad mark (Gnome-Sheet): the hat as a sail on a hull, in a circle; 132 x 150 units */
static const char GA_MARK[] = "M44 84 L90 84 L70 26 Z M70 26 Q61 15 50 21 M34 92 Q66 106 98 92";

/* --------------------------------------------------- the sixteen pads --- */
/* Easy's pads in GN_PADS order (gn_easy.c), pad 1 at the bottom left; 24 units, drawn at .75 in 27 px
 * pads. Kick to open hat and the crash as Easy-Loop draws them; the tambourine, ride, cowbell,
 * maracas, claves and wood blocks drawn for Gnomad in the same line. */
static const char *const GA_PAD[16] = {
    "M12 4a8 8 0 1 1 0 16a8 8 0 1 1 0-16 M12 9.5a2.5 2.5 0 1 1 0 5a2.5 2.5 0 1 1 0-5",                   /* kick */
    "M5 12a7 7 0 1 1 14 0a7 7 0 1 1-14 0 M5.55 7.05a1.5 1.5 0 1 1 3 0a1.5 1.5 0 1 1-3 0 "
    "M15.45 7.05a1.5 1.5 0 1 1 3 0a1.5 1.5 0 1 1-3 0 M5.55 16.95a1.5 1.5 0 1 1 3 0a1.5 1.5 0 1 1-3 0 "
    "M15.45 16.95a1.5 1.5 0 1 1 3 0a1.5 1.5 0 1 1-3 0",                                                   /* tambourine */
    "M4 8.5c0-1.4 3.6-2.5 8-2.5s8 1.1 8 2.5-3.6 2.5-8 2.5-8-1.1-8-2.5z M4 8.5v7c0 1.4 3.6 2.5 8 2.5s8-1.1 8-2.5v-7 "
    "M6.5 14.5l1.8 1.8 1.8-1.8 1.8 1.8 1.8-1.8 1.8 1.8 1.8-1.8",                                            /* snare */
    "M4 8.5c0-1.4 3.6-2.5 8-2.5s8 1.1 8 2.5-3.6 2.5-8 2.5-8-1.1-8-2.5z M4 8.5v7c0 1.4 3.6 2.5 8 2.5s8-1.1 8-2.5v-7 "
    "M7 14.5h10",                                                                                          /* snare 2 */
    "M4 13c0-1.4 3.6-2.5 8-2.5s8 1.1 8 2.5-3.6 2.5-8 2.5-8-1.1-8-2.5z M4 13v4c0 1.4 3.6 2.5 8 2.5s8-1.1 8-2.5v-4 "
    "M5 4l14 9",                                                                                           /* rim */
    "M12 3.5v4 M12 16.5v4 M3.5 12h4 M16.5 12h4 M6 6l2.8 2.8 M15.2 15.2L18 18 M6 18l2.8-2.8 M15.2 8.8L18 6",  /* clap */
    "M4 9c2-1.6 14-1.6 16 0 M4 11c2 1.6 14 1.6 16 0 M12 12v9 M9 21h6",                                    /* closed hat */
    "M4 7c2-1.6 14-1.6 16 0 M4 9c2 1.6 14 1.6 16 0 M12 10v9 M9 21h8 M9 21l-3-2.5",                         /* pedal hat */
    "M4 7.5c2-1.6 14-1.6 16 0 M4 11.5c2-1.6 14-1.6 16 0 M12 11v10 M9 21h6",                                /* open hat */
    "M2.5 10c3-1.6 16-1.6 19 0c-3 1.6-16 1.6-19 0z M10 9.2c.3-1.6 3.7-1.6 4 0 M12 11.2V21 M9 21h6",        /* ride */
    "M9 5.5h6l3.5 13h-13z M10.5 5.5V3.5h3v2",                                                             /* cowbell */
    "M12 3.5c3 0 5 2.5 5 5.5s-2 5.5-5 5.5-5-2.5-5-5.5 2-5.5 5-5.5z M12 14.5V21 M17.8 3.8l1.7-1.3 M18.8 7h2.2",
                                                                                                           /* maracas */
    "M3 17l8-4 M21 17l-8-4 M12 5v3.2 M7.8 6.6l1.6 2 M16.2 6.6l-1.6 2",                                    /* claves */
    "M3.5 9h17v7h-17z M7 12.5h10",                                                                         /* wood block */
    "M6.5 10h11v5h-11z M9 12.5h6",                                                                         /* wood block, high */
    "M3 10.5l18-4 M3 10.5c3 2 15-1.5 18-4 M12 9v12 M9 21h6",                                              /* crash */
};
