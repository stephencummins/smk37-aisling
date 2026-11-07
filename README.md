# AISLING for the M-VAVE FM-1

<img src="docs/card.png" width="240" height="240" alt="AISLING: a gold 8-bit harp on green">

**[Install](https://aisling.stephen8n.com/install/)** (Chrome or Edge, also Chrome on Android or
Chromebook; the FM-1 on USB) · **[Try it in the browser](https://aisling.stephen8n.com/emu/)** ·
**[Manual](https://aisling.stephen8n.com/manual/)** ·
**[Cheat sheet](https://aisling.stephen8n.com/cheat/)** ·
**[Visualiser](https://aisling.stephen8n.com/visualiser/)** ·
**[Website](https://aisling.stephen8n.com/)**

By CU_STE · [www.stephen8n.com](https://www.stephen8n.com)

The same source also builds for the M-VAVE SMK-37 Elite, not yet tried on one: see [docs/SMK37.md](docs/SMK37.md).

## What?

AISLING turns the M-VAVE FM-1 into a generative Irish ambient instrument: harps over a held drone, and a flute, a
fiddle or a voice leading, in the old modes, with the weather of the west coast behind them. It plays
itself from the moment it's switched on, and it never ends. You can play with it: the white keys give you the Lead,
always in the piece's scale, and it answers you. (An *aisling*, said "ash-ling", is an Irish vision poem.)

It is a fork of [NACHTFAHRT](https://github.com/FartyPants/fm1-nachtfahrt), FartyPants's Berlin School firmware.
AISLING keeps NACHTFAHRT's whole machine (its five layers, its arc, its slowly moving filters, its lead that
remembers and answers) and changes what it plays:

- **the harp**: NACHTFAHRT's plucked string restrung as a wire-strung harp, with a long, bright ring. It plays
  every piece's Sequence and, an octave down, its Sequence 2, like the harper's other hand;
- **the Lead's four voices**: a flute (a soft tone, breath, and a chiff as each note starts); a fiddle (a bowed
  string through its body, the bow's hiss loudest as each note starts); a chanter (a nasal reed, each note stopped
  and opened again); and a voice (a singer's, its vowel easing from note to note), matched in loudness;
- **the modes**: major, Mixolydian, Dorian and minor, and two gapped scales of six notes (one with no seventh,
  one with no sixth), in the pipes' and the whistle's keys (D, G, A, E, B);
- **the double tonic**: the chords swing between two, D and C or E minor and D, a long while each, as the old
  tunes do;
- **the jig's threes**: harps of 12 and 9 steps, with every third step accented;
- **eight pieces** named for places in the west and for a harper;
- three of the black keys' effects named in Irish: **Ceo** (fog), **Macalla** (an echo) and **Fras** (a burst
  of rain);
- a green triskele on the launch screen and the main page.

**Status: 1.0, the first version out of testing** (see [the releases](https://github.com/stephencummins/fm1-aisling/releases)).
It runs on the FM-1, installed from the [web installer](https://aisling.stephen8n.com/install/), and
in the [browser emulator](https://aisling.stephen8n.com/emu/), from the same code.

**[The visualiser](https://aisling.stephen8n.com/visualiser/)** paints what the FM-1 plays as a west-coast
landscape in oils: the Lead is the sky, Sequence 2 the hills, the Sequence the fields, the Drone the bog. Or as
the Atlantic from above, eddies spun up by Sequence 2, or as deep space, a comet drawn by the Lead. No FM-1? Demo plays a
recording of Skellig and paints it as you hear it. Plug the FM-1
into a computer or Android phone, open the page in Chrome or Edge and press Listen; it reads the FM-1's MIDI out
(on by default) in your browser, and nothing is sent anywhere. You can paint into it with a finger or the mouse, save a
picture, record a video with the FM-1's own sound from its USB audio, and drop a sound file on it to mix in: it loops in
time with the FM-1 and paints as the weather. It is one file,
[web/visualiser/index.html](web/visualiser/index.html), with the demo's recording beside it, and it also works opened
straight from disk, offline (there the demo paints without sound).

**[The editor](https://aisling.stephen8n.com/editor/)** lays out a piece's settings on one web page, with a Play page and a Library of the eight pieces. With no
FM-1 it plays by itself in the browser. Press Connect in Chrome or Edge and it works the FM-1 over USB instead, with the
page's own sound off. The FM-1 says nothing on that link until the page connects. The
[manual](docs/MANUAL.md#the-editor) has the details.

**MIDI.** A note on any channel, over USB or the TRS jack, plays the Lead as a white key does, and the sequences
follow MIDI clock, start, continue and stop. MIDI out goes over USB: the Drone, the Sequence, the Lead and Sequence 2
on channels 1 to 4, and clock. The TRS jack has been tried with Type A, from a Korg nanoKEY Fold; Type B hasn't been
tried. The [manual](docs/MANUAL.md#11-midi) has the details.

**Saved pieces.** AISLING saves its eight pieces in the same place in the FM-1's memory as NACHTFAHRT, DUB-1 and
AMB-1. It doesn't read what they saved (it starts with its own eight recipes), and its first save, a few seconds
after you change something, replaces theirs; they ignore AISLING's in the same way and replace it when they save.
Installing another firmware may lose what AISLING saved. AISLING uses no samples.

## What's new in 1.0

- **A fix for the crackle.** The FM-1 now works out its sound one 5.8 ms block ahead, so a chord change or a string strike that takes too long has the next block's time to catch up. Until now any one slow block was a dropout, heard as a click: at their recipes, Skellig, Aran, Samhain and Blasket had them. The sound itself doesn't change.
- **5.8 ms more delay** between a key and its sound, and USB MIDI out now leads the audio by that much more.
- **Out of testing.** Pieces saved by 0.2 to 0.5.1 carry over, and the controls are as in 0.5.1.
- **Still new from 0.5 and 0.5.1:** [the editor](https://aisling.stephen8n.com/editor/), undo, hold SAVE to save, USB audio before MASTER, and newer FM-1s can save and update.

## What's new in 0.5.1

- **Newer FM-1s can save and update.** Units sold since autumn 2026 carry a different flash chip (0xB36014). AISLING now accepts it beside the first one, so pieces survive switching off and updates start. Nothing else changes. We have only the first chip, so this has been tried by us on that one alone.
- **On a newer FM-1 with an earlier AISLING?** Those versions refuse updates, so reaching this one takes [the chip's own download mode](https://thefirmwarehub.com/tools/rescue/#chip), once. The [manual](https://aisling.stephen8n.com/manual/#a-newer-fm-1-on-an-earlier-aisling) has the steps.
- **Still new from 0.5:** [the editor](https://aisling.stephen8n.com/editor/), undo, hold SAVE to save, and USB audio before MASTER.

## What's new in 0.5

- **[The editor](https://aisling.stephen8n.com/editor/):** every setting of a piece on one page, played in the browser or, in Chrome or Edge, on your FM-1 over USB.
- **Undo:** hold HOME and tap OCT− to undo, OCT+ to redo.
- **Hold SAVE to save,** as GLO already did.
- **SEL leaves PERFORM,** back to the page you came from.
- **Where you are:** a dot for each of the layer's pages on the screen, and the page's name large for a moment.
- **A gentler MASTER,** with more room at the quiet end.
- **USB audio before MASTER,** so recording works at any volume.

## The pieces

| # | Piece | Named for | Key, tempo | What you hear |
|---|---|---|---|---|
| 1 | Skellig | Skellig Michael, off Kerry | D minor, 76 | The pipes' drones, one harp in a jig's threes, a flute, the surf; Dm and C in turn |
| 2 | Burren | The Burren, Clare | G Mixolydian, 84 | A choir, two harps of 12 and 11 drifting apart, a fiddle's slow air, the wind |
| 3 | Aran | Inis Mór | A Dorian, 96 | A reed drone, a bouzouki's jig of 12 over a bodhrán's slip jig of 9, a flute, a turf fire; Am and G in turn |
| 4 | Carolan | Turlough O'Carolan, the harper | D major, 88 | Strings, two harps of 16 and 15, rain, a planxty's chords |
| 5 | Blasket | The Blasket Islands, Kerry | E Dorian, 70 | A choir, slow dulcimers, a singer's slow air over the surf; Em and D in turn |
| 6 | Samhain | The year's dark turn | B minor, 64 | The pipes' drones, low and beating, one low harp over a bodhrán's heartbeat, a low voice, the wind |
| 7 | Connemara | Bog, lake and rain | G hexatonic minor, 92 | An organ, a harp's slip jig of 9 over a low bodhrán's 8, a low flute held back in a long echo, rain; Gm and F in turn |
| 8 | Tir na nOg | The land of the young, over the western sea | D hexatonic major, 80 | A phased glass drone, two harps, a long reverb, a curlew far out over the bog; D and G in turn |

The keys, modes and tempos are starting points, tuned by ear. Each piece's Lead opens on an air, the first phrase of a
public-domain tune from O'Neill's *Music of Ireland* (Chicago, 1903), played as written; its own ideas grow from it.
The eight are listed in the manual's [The pieces](docs/MANUAL.md#7-the-pieces).

## In short

| | |
|---|---|
| White keys | Play the Lead (the piece's flute, fiddle or voice), in the piece's scale (the key it sounds is lit) |
| Black keys | Eleven effects, each while held (lit; dark while held): Ceo, Macalla, Fras, Repeat, Shimmer, Freeze, Octave, Return, then Rise, Close, Transpose (in PERFORM: the solos) |
| PLAY · REC · EDIT | Play / rest · a tap: hold (the echo repeats what it holds), held: the effects grid (16 effects on the white keys) · a tap: the Chords page, held: the next chord |
| SEL | PERFORM (again: back to the page you came from): the knobs Filter, Wet, Energy, Sweep; white 1–5 mute the Drone, Sequence, Lead, Sequence 2, Atmosphere; black 1–5 solo them; white 6 held: the levels; white 7–11 held: Sweep, Open, Shut, Drop (both sequences out), Echo; the top key (G): the Lead forgets its ideas and its theme. Held: Lock |
| HOME | The Aisling page (Filter, Evolve, Motion, Drift); again, the Key page (Tempo, Root, Mode, Transpose). Held, with OCT− or OCT+: undo or redo |
| ENV | The Drone (Pedal, Reso, Phaser, Attack); again, its sound (Colour, Release, Reverb, Echo); again, its Move page (Width, Time, Rate, Intro) |
| ARP | The Sequence (Density, Octave, Length, Grid); again, its sound (Cutoff, Decay, Reverb, Echo); again, its Repeats (Repeat A, A time, Repeat B, B time); again, its Move page |
| LFO | The Lead (Phrases, Octave, Ornament, Vibrato); again, its sound (Tone, Attack, Reverb, Echo); again, its style (Ornament, Glide, Vibrato, Memory); again, its Move page |
| SEQ | Sequence 2 (Chance, Octave, Length, Level); again, its sound; again, the Pattern page (write the steps on the keys); again, its Move page |
| SAVE | The Atmosphere (Density, Width, Drift, Air); again, its sound (Colour, Ring, Reverb, Echo); again, its Move page. Held: save |
| FX | Space (Echo, Echo time, Feedback, Reverb); again, the Echo page (Blur, Tone, Width, Size) |
| GLO | Setup (Tune, MIDI out, Lights, Save); again, the Arc (Lead in, Bridge, Arc, Memory). Held: save; held on: the piece back to its recipe |
| ALGORITHM | On a layer's page, its voice (the Atmosphere: its character) |
| PRESETS | The piece |
| SELECT | On a layer's pages, its level; on the Aisling page, Energy (elsewhere nothing) |
| OCT− / OCT+ | On the Sequence's, the Lead's and Sequence 2's pages, its octave (lit while it's above or below where the piece was loaded) |

The [manual](docs/MANUAL.md) has the rest; the [cheat sheet](https://aisling.stephen8n.com/cheat/)
is this table on one page, to print, with the steps drawn as flows.

## Building and testing

The toolchain is the same as NACHTFAHRT's and DUB-1's (see [BUILDING.md](BUILDING.md) and [EMULATOR.md](EMULATOR.md)):

```
tests/run_tests.sh      # the engine, the platform pieces, the app in a simulator, the web builds
web/emu/build.sh        # the browser emulator: build/emu (serve it, open index.html)
web/visualiser/         # the visualiser: one file, no build (published with the site: tools/publish_pages.sh)
./build.sh              # build/amb.fwsc, for the FM-1
python3 tools/fm1_install.py build/amb.fwsc     # install it over USB
```

`build/host/amb_host SCRIPT OUTDIR` runs the whole app on a computer from a script
(`tests/scenarios/*.amb`), with audio and screenshots coming out; `build/host/en_render OUT.wav SECS piece=N`
renders the engine alone. `tools/make_card.py` draws the harp on the card ([docs/card.png](docs/card.png)), and `tools/make_brand.py` makes the
site's icons from it.

## Credits

AISLING is by CU_STE ([www.stephen8n.com](https://www.stephen8n.com)). It is a fork of
[NACHTFAHRT](https://github.com/FartyPants/fm1-nachtfahrt) by FartyPants (GPL-3.0-only): the engine, the layers,
the arc, the filter motion, the remembering lead, the pages, the effects grid, the emulator and the site come from
there. NACHTFAHRT is a fork of [DUB-1](https://github.com/sbegueria/fm1-dub), a fork of
[AMB-1](https://github.com/charlesvestal/fm1-amb) by Charles Vestal (GPL-3.0-only): the echo chain, the plate,
the pieces system, the chord walk, the UI and the platform come from there. The reverb is Jon Dattorro's plate
("Effect Design, Part 1", JAES 1997), as in [FoMni-1](https://github.com/charlesvestal/fm1-fomni); Shimmer's
octave shifter follows OCT UP and its SHIMMER in [Felucca](https://github.com/hugelton/Felucca)'s performance layer
(Leo Kuroshita, Hügelton Instruments; also in [Skiff](https://github.com/illuminus5/skiff)); the Drone's Choir
follows VOICE in Felucca (`eng_formant.c`, GPL-3.0; Klatt's cascade; its vowel table, after Hillenbrand et al.); the
granular cloud (Freeze) follows the design of CLOUD in [JIANT](https://github.com/juanjiant-bit/JIANT) (GPL-3.0);
the Harp's string follows `pluck.c` in M-VAVE's [fm1_open](https://github.com/m-vave/fm1_open) (after DaisySP's
String, Electrosmith and Emilie Gillet; MIT); Wind's pink and Surf's brown noise and Rain's drops (and Fire's pops, timed as they are) follow NOISE in
[Skiff](https://github.com/illuminus5/skiff) (`eng_noise.c`, Leo Kuroshita, Hügelton Instruments; GPL-3.0; Paul
Kellet's pink filter); the Tape atmosphere is DUB-1's crackle, after Bahadirhan Kocer's "Using Resonators in Live".
NACHTFAHRT's way of playing a sequence (the root accented, sparse notes, a dotted-eighth repeat as loud as the
note, two delays), which AISLING's harps keep, owes much to Synth Seeker's videos on Berlin School sequencing. The
platform (hardware layer, USB, update loader, storage, installer) is [Felucca](https://github.com/hugelton/Felucca)'s
by Leo Kuroshita (Hügelton Instruments), by way of [X0X](https://github.com/charlesvestal/fm1-x0x), FoMni-1, AMB-1,
DUB-1 and NACHTFAHRT. The idea of running FM-1 firmware in the browser comes from
[Groove OS](https://www.groove-os.com/). GPL-3.0-only; see [LICENSING.md](LICENSING.md).

M-VAVE and FM-1 are trademarks of their owners. AISLING is not affiliated with M-VAVE, Hügelton Instruments or
NACHTFAHRT's author.
