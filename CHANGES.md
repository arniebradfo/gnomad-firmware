# What's new

## Release 12: Gnomad Synth V1 115e11 (2026-10-20)

On the synth's ABOUT page and in `--info` it reads `GNOMAD V1 115e11`. It supersedes release 11 (`V1 3487b5`) and every
earlier build. The synth plays, saves and installs as release 11 did. The source's comments and its record of
changes, [FORK.md](FORK.md), are rewritten for publication: every technical change is still listed there.

## Release 11: Gnomad Synth V1 3487b5 (2026-10-13)

This is the first release on GitHub. On the synth's ABOUT page and in `--info` it reads `GNOMAD V1 3487b5`.
It replaces every earlier test build, including release 8 (`V1 407714`, the one published on gnomad.cc)
and releases 9 and 10. The full technical record of every change is in [FORK.md](FORK.md).

### Sounds

- **Gnomad's own sound engines.** The synth now plays Gnomad's own sound core: five engines (Synth, FM,
  Drums, the Gnome's singing voice, and a Sampler) with their own echo, chorus and plate reverb. They replace
  Felucca's engines in this build.
- **EASY mode: 10 pictures, 57 sounds.** The pictures are Gnome, Organ, Bells, Laser, Whale, Choir, Cloud,
  Piano, Funny and Drums. Funny is new: whoopee cushion, boing and friends.
- **Every sound has its own four knobs.** The blue, green, yellow and vermilion knobs do different jobs on
  different sounds: SHIMMER and DECAY on bells, PUNCH, GROWL and SUB on basses, PITCH and CHAOS on funny
  sounds. The screen names each knob's job in its colour, and each one starts where the sound was designed
  to sit.
- **The Gnome sings**, a syllable per key.
- **Drum kits:** the white keys are the drums. The black keys now play a soft "ghost" hit of the drum next
  to them, quieter, darker and shorter, the way grooves use them.

### Playing and performing

- **Grooves page.** Keys 1 to 5 are five ready-made grooves (Head Nod, Floor Filler, Rainy Desk, Night Bus,
  Sky Garden), each with its own drum kit. Keys 6 to 9 are your four saved songs. While one plays, the next
  starts on the next bar line, and pressing the same key again switches now. It never throws away music you
  haven't saved: stopped, it saves first; playing, it asks.
- **Restart on the beat:** hold PLAY and the loop restarts at the next bar line. Hold it again to restart now.
- **Stop at the end of the bar:** GLO + PLAY. Do it again to stop now. A quick tap of PLAY still stops at once.
- **Fills:** hold GLO and hold keys for fast repeats in time with the beat. Fills are never recorded.
- **Try a sound:** hold FX and play keys to hear the sound alone and turn its knobs. Nothing is written.
- **The sound list stays up** while you choose, until you press HOME, PLAY or a key.

### Recording and saving

- The screen says what's happening, from ARMED T1 to COUNT-IN 3 to RECORDING T1, and whether you KEEP
  (add) or REPLACE.
- **A click while you record:** hold EDIT and turn knob 3. It only clicks during the count-in and while
  recording.
- A new blank loop is one bar long.
- **Undo says what it undid:** hold SAVE and the screen says LAST TAKE, STEP EDIT, BARS and so on.
- **Power-on brings back the last thing you saved**, whichever mode you saved it in. Before, a beat saved in
  Advanced could seem lost after a restart.

### Advanced mode

- The Advanced pages now reach Gnomad's engines. If a knob does nothing on a sound, the page says so and why,
  for example "Not on this sound", or "Mono: no pan".
- Fixed: a sound you saved as a user preset now loads back on the sound it came from, as you saved it.
- Fixed: Glide, its mode, and Detune used to look active on Gnomad's sounds but did nothing. They now say
  "Not on this sound".

### Other

- On a computer or phone, the USB port is named **Gnomad Synth V1**. The update loader keeps Felucca's name,
  "Felucca Update". Because of the new name, `fm1_install.py` needs `--port Gnomad` to find a synth that
  already runs Gnomad (see [INSTALL.md](INSTALL.md)).
- The splash and ABOUT screens say GNOMAD. ABOUT credits Felucca by Leo Kuroshita (GPL-3) and has a QR code
  to gnomad.cc.

### Known limits

- The sound core is mono, so pan does nothing.
- Some Advanced effect knobs have no effect on Gnomad's engines, such as Drive, the Slicer and the reverb
  type. The page says so.
- How much processing power the engines use was checked in the simulator and by code analysis. It hasn't
  been measured on a second FM-1 under full load yet. If you hear glitches with many notes and a groove
  playing, please tell us.
- Going back to stock firmware hasn't been proven on real hardware yet. See [INSTALL.md](INSTALL.md).
