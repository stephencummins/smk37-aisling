#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The FM-1's map of the controls, drawn as the SMK-37 Elite's is (no photographs: M-VAVE's are not ours to use):

  docs/img/fm1-controls.svg   the FM-1 running AISLING: each knob, button and key labelled with what it does, the
                              black keys with their effects, the white keys with the grid's (REC held)
  docs/img/fx1-controls.svg   the same FM-1 running AISLING FX-1, the USB effects box: what each control does to the
                              computer's sound, the voices' pages dimmed (they make nothing it can hear)

  tools/gen_fm1_figure.py [FM1_SCREEN.png [FX1_SCREEN.png]]
                              (defaults docs/img/screen-main.png and docs/img/fx1-screen.png)

Where the controls sit follows tools/gen_manual_figures.py's panel (X0X's): two knob pairs and the octave buttons at
the left, the screen in the middle, four knobs over two rows of six buttons at the right, 16 white keys (F to G) and
11 black keys in groups of 3, 2, 3, 2, 1 below, the keys pills the colour of the buttons. The grid's groups are in the
colours the grid screen gives them (ui.c's grid_band): Echo the sea, Filter gorse, Space heather, Moves the flag's
orange.

The FM-1 is sold in more than one colour, so, as on the Elite's drawing, the colours are CSS variables on the svg, with
SKINS their sets: data-skin="black" on the svg (the controls pages' buttons set it, tools/make_pages.py) redraws it.
Orange is the emulator's (web/emu/index.html); the black is an all-black FM-1's, by eye. Every rule is under
svg.fmp, so a drawing can sit inline in a page."""
import base64
import io
import sys
from pathlib import Path

from PIL import Image

SRC = Path(__file__).resolve().parents[1]
OUT = SRC / "docs" / "img"
FONT = "'Helvetica Neue', Helvetica, Arial, sans-serif"


# a skin: the body, the trays, the edges; the words on the body (ink, muted, dim) and on a tray (trayink, traydim);
# a knob, its edge, its foot and its mark; a button (the keys are buttons too), its edge and its word; the accent
def skin(**over):
    base = dict(body="#cb6b45", tray="#85473c", edge="#a3532f", ink="#24110a", muted="#2e150b", dim="#5e2a16",
                trayink="#ffe8c2", traydim="#d3a58f", knob="#1c1614", knobedge="#3a2a24", knobfoot="#ab5636",
                mark="#fbf0e4", btn="#8f4d40", btnedge="#5a2c23", btnink="#fbf0e4", btndim="#c99486",
                accent="#24110a")
    assert not set(over) - set(base)
    return {**base, **over}


SKINS = {
    "orange": ("Orange", skin()),
    "black": ("Black", skin(body="#1d1d20", tray="#2b2b2f", edge="#3c3c42", ink="#efece4", muted="#c9c5ba",
                            dim="#8d8a83", trayink="#efece4", traydim="#8d8a83", knob="#0c0c0d", knobedge="#55555c",
                            knobfoot="#2a2a2e", mark="#efece4", btn="#3a3a40", btnedge="#55555c", btnink="#f3f0e8",
                            btndim="#8d8a83", accent="#3fce86")),
}
VARS = tuple(skin())
# the grid's groups (REC held), in the grid screen's colours
GROUPS = (("Echo", "#48BCB0"), ("Filter", "#F4C838"), ("Space", "#C696D2"), ("Moves", "#FF9248"))
GRID = ("Repeat", "Time", "Tape stop", "Shimmer", "Low-pass", "High-pass", "Sweep", "Phaser",
        "Reverb", "Ceo", "Freeze", "Wash", "Octave", "Return", "Fras", "Macalla")
BLACK_FX = ("Ceo", "Macalla", "Fras", "Repeat", "Shimmer", "Freeze", "Octave", "Return", "Rise", "Close", "Transpose")
WHITE_NOTES = "F G A B C D E F G A B C D E F G".split()
BLACK_AFTER = (0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14)        # black key k sits after white key BLACK_AFTER[k]

W, H = 1500, 980

# What each control does, in each build. A "-" before a job draws it dimmed: a control that does nothing there.
FM1 = dict(
    file="fm1-controls.svg", screen="screen-main.png",
    aria="The FM-1's panel running AISLING, each control labelled with what it does",
    knobs=(("MASTER", "Volume"), ("SELECT", "Level|(a layer's page)"), ("PRESETS", "Piece"), ("ALGORITHM", "Voice")),
    k=(None, None, None, None), k_rule="The four values on the screen",
    octave=("Octave down", "Octave up"),
    top=(("FX", "Space"), ("SEL", "Perform|held: Lock"), ("ENV", "Drone"), ("LFO", "Lead"),
         ("EDIT", "Chords|held: next"), ("GLO", "Setup, Arc|held: save")),
    bot=(("HOME", "Aisling,|Key"), ("SAVE", "Atmosphere|held: save"), ("ARP", "Sequence"), ("SEQ", "Sequence 2"),
         ("PLAY|STOP", "Play /|rest"), ("REC", "Hold|held: effects")),
    keys_top="White keys: the Lead, in the piece's scale (the key it sounds is lit).   "
             "Black keys: an effect each while held.",
    black=BLACK_FX, grid=GRID,
    foot=("REC held: the sixteen effects on the white keys, as long as each is held:",
          "SEL (PERFORM): every one of the 27 keys an effect while held.   HOME held with OCT− or OCT+: undo or redo."),
)

# AISLING FX-1 (fx1-aisling; the --fx build): the same panel, the computer's sound in place of the voices. K1-K4 are drawn
# with HOME's four; the voices' pages are dimmed, and so are the effects that change nothing in the computer's sound
# (each measured in the simulator, amb_host built with AISLING_FX, against the same run without it: 2026-11-13)
FX1 = dict(
    file="fx1-controls.svg", screen="fx1-screen.png",
    aria="The FM-1's panel running AISLING FX-1, each control labelled with what it does to the computer's sound",
    knobs=(("MASTER", "Level at|the socket"), ("SELECT", "-A layer's level"), ("PRESETS", "Piece: its echo,|reverb, tempo"),
           ("ALGORITHM", "-A voice")),
    k=("Filter", "Echo", "Reverb", "Dry"), k_rule=None,
    octave=("-A layer's octave", "-A layer's octave"),
    top=(("FX", "Space,|Echo"), ("SEL", "Perform"), ("ENV", "-Drone"), ("LFO", "-Lead"),
         ("EDIT", "-Chords"), ("GLO", "Setup|held: save")),
    bot=(("HOME", "Home,|Key"), ("SAVE", "-Atmosphere"), ("ARP", "-Sequence"), ("SEQ", "-Sequence 2"),
         ("PLAY|STOP", "Rest: the|drift stops"), ("REC", "Hold|held: effects")),
    keys_top="White keys: nothing (they play the Lead, and FX-1 has none).   "
             "Black keys: an effect each while held; the dim ones change nothing here.",
    black=("-Ceo", "Macalla", "Fras", "-Repeat", "Shimmer", "Freeze", "-Octave", "-Return", "-Rise", "-Close", "-Transpose"),
    grid=("-Repeat", "Time", "Tape stop", "Shimmer", "Low-pass", "High-pass", "-Sweep", "Phaser",
          "Reverb", "-Ceo", "Freeze", "Wash", "-Octave", "-Return", "Fras", "Macalla"),
    foot=("REC held: sixteen effects on the white keys, as long as each is held:",
          "SEL (PERFORM): 17 of the 27 keys' effects work here.   K1–K4 as drawn at HOME; elsewhere, the page's four."),
)


def png_data_uri(path):
    b = io.BytesIO()
    Image.open(path).convert("RGB").save(b, "PNG", optimize=True)
    return "data:image/png;base64," + base64.b64encode(b.getvalue()).decode()


def text(x, y, t, cls, anchor="middle"):
    return f'<text x="{x:.1f}" y="{y:.1f}" text-anchor="{anchor}" class="{cls}">{t}</text>'


def lines(x, y, t, cls, anchor="middle", lead=18):
    """one or two lines, split at |; a job that starts with - is dimmed"""
    if t.startswith("-"):
        t, cls = t[1:], cls + " dim"
    return "".join(text(x, y + i * lead, part, cls, anchor) for i, part in enumerate(t.split("|")))


def knob(x, y, r, label, does=None):
    return (f'<circle cx="{x}" cy="{y}" r="{r + 6}" class="foot"/>'
            f'<circle cx="{x}" cy="{y}" r="{r}" class="knob" stroke-width="2.5"/>'
            f'<line x1="{x}" y1="{y - r + 5}" x2="{x}" y2="{y - r + 16}" class="mark" stroke-width="3" stroke-linecap="round"/>'
            + text(x, y - r - 14, label, "lbl") + (lines(x, y + r + 28, does, "what") if does else ""))


def button(x, y, w, h, label, dim=False):
    parts = label.split("|")
    cls = "btn dim" if dim else "btn"
    t = (text(x + w / 2, y + h / 2 + 5, parts[0], cls) if len(parts) == 1 else
         text(x + w / 2, y + h / 2 - 2, parts[0], cls + " small") + text(x + w / 2, y + h / 2 + 12, parts[1], cls + " small"))
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="10" class="b" stroke-width="1.5"/>{t}'


def left(kind):
    s = [knob(x, y, 26, name, does) for (name, does), (x, y) in zip(kind["knobs"], ((110, 104), (256, 104), (110, 262), (256, 262)))]
    s.append(f'<rect x="46" y="372" width="276" height="64" rx="14" class="tray"/>')
    for i, name in enumerate(("OCT−", "OCT+")):
        x = 62 + i * 128
        s.append(button(x, 384, 116, 40, name) + lines(x + 58, 460, kind["octave"][i], "what"))
    return "".join(s)


def screen(png):
    return (f'<rect x="374" y="40" width="300" height="300" rx="28" class="glass" stroke-width="2"/>'
            f'<image x="404" y="70" width="240" height="240" href="{png_data_uri(png)}" style="image-rendering:pixelated"/>')


def right(kind):
    x0, pitch, bw = 736, 118, 92                       # the buttons: the first's left, their pitch, their width
    mid = x0 + (5 * pitch + bw) / 2
    s = [knob(mid + (i - 1.5) * 150, 100, 25, f"K{i + 1}", kind["k"][i]) for i in range(4)]
    if kind["k_rule"]:
        a, b = mid - 1.5 * 150 - 30, mid + 1.5 * 150 + 30
        s.append(f'<path d="M{a} 140 v8 H{b} v-8" class="rule" stroke-width="1.5"/>' + text(mid, 170, kind["k_rule"], "what"))
    top_y = 200 if kind["k_rule"] else 214
    s.append(f'<rect x="{x0 - 18}" y="{top_y}" width="{5 * pitch + bw + 36}" height="236" rx="18" class="tray"/>')
    for r, row in enumerate((kind["top"], kind["bot"])):
        for i, (name, does) in enumerate(row):
            x, y = x0 + i * pitch, top_y + 14 + r * 112
            s.append(button(x, y, bw, 46, name, does.startswith("-")) + lines(x + bw / 2, y + 66, does, "what tray", lead=17))
    return "".join(s)


def keybed(kind):
    x0, y0, w, h = 36, 540, W - 72, 340
    pitch = (w - 24) / 16
    kw = pitch * 0.78
    s = [f'<rect x="{x0}" y="{y0}" width="{w}" height="{h}" rx="20" class="tray"/>']
    wy, wh = y0 + h * 0.43, h * 0.52
    for i in range(16):
        x = x0 + 12 + i * pitch + (pitch - kw) / 2
        s.append(f'<rect x="{x:.1f}" y="{wy:.1f}" width="{kw:.1f}" height="{wh:.1f}" rx="{kw / 2:.1f}" class="b" stroke-width="1.5"/>')
        if kind["grid"]:
            c = GROUPS[i // 4][1]
            g = kind["grid"][i]
            s.append(f'<rect x="{x + kw / 2 - 14:.1f}" y="{wy + 12:.1f}" width="28" height="7" rx="3.5" fill="{c}"/>')
            s.append(f'<text transform="translate({x + kw / 2 + 6:.1f} {wy + wh - 40:.1f}) rotate(-90)" '
                     f'class="{"fx dim" if g.startswith("-") else "fx"}">{g.lstrip("-")}</text>')
        s.append(text(x + kw / 2, wy + wh - 14, WHITE_NOTES[i], "key"))
    by, bh = y0 + h * 0.05, h * 0.34
    for k, a in enumerate(BLACK_AFTER):
        x = x0 + 12 + (a + 1) * pitch - kw / 2
        s.append(f'<rect x="{x:.1f}" y="{by:.1f}" width="{kw:.1f}" height="{bh:.1f}" rx="{kw / 2:.1f}" class="b" stroke-width="1.5"/>')
        if kind["black"]:
            b = kind["black"][k]
            s.append(f'<text transform="translate({x + kw / 2 + 6:.1f} {by + bh - 16:.1f}) rotate(-90)" '
                     f'class="{"fx dim" if b.startswith("-") else "fx"}">{b.lstrip("-")}</text>')
    s.append(lines(W / 2, y0 - 22, kind["keys_top"], "what"))
    y = y0 + h + 40
    if kind["grid"]:
        first = kind["foot"][0]
        s.append(text(x0 + 6, y, first, "what", "start"))
        x = x0 + 6 + len(first) * 8.1 + 24
        for name, c in GROUPS:
            s.append(f'<rect x="{x:.0f}" y="{y - 15}" width="20" height="20" rx="5" fill="{c}"/>' + text(x + 30, y + 1, name, "what", "start"))
            x += 130
        y += 28
    for t in kind["foot"][1:]:
        s.append(text(x0 + 6, y, t, "what", "start"))
        y += 26
    return "".join(s)


def style():
    v = lambda vals: "".join(f"--{k}:{vals[k]};" for k in VARS)
    skins = "".join(f'svg.fmp[data-skin="{k}"]{{{v(vals)}}}' for k, (_, vals) in SKINS.items() if k != "orange")
    rules = (f"text{{font-family:{FONT}}}.body{{fill:var(--body)}}.tray{{fill:var(--tray)}}"
             ".knob{fill:var(--knob);stroke:var(--knobedge)}.foot{fill:var(--knobfoot)}.mark{stroke:var(--mark)}"
             ".glass{fill:#111214;stroke:var(--edge)}.b{fill:var(--btn);stroke:var(--btnedge)}"
             ".rule{fill:none;stroke:var(--muted)}"
             ".lbl{font-weight:600;font-size:17px;fill:var(--ink);letter-spacing:.06em}"
             ".btn{font-weight:600;font-size:14px;fill:var(--btnink);letter-spacing:.05em}.btn.small{font-size:11.5px}"
             ".btn.dim{fill:var(--btndim)}"
             ".what{font-size:16px;fill:var(--muted)}.what.dim{fill:var(--dim)}"
             ".what.tray{font-size:14px;fill:var(--trayink)}.what.tray.dim{fill:var(--traydim)}"
             ".key{font-weight:600;font-size:16px;fill:var(--btnink)}"
             ".fx{font-weight:600;font-size:15px;fill:var(--btnink)}.fx.dim{fill:var(--btndim);font-weight:400}")
    scoped = "".join("svg.fmp " + r + "}" for r in rules.split("}") if r)
    return f"<style>svg.fmp{{{v(SKINS['orange'][1])}}}{skins}{scoped}</style>"


def panel(kind, screen_png):
    return "".join([f'<svg xmlns="http://www.w3.org/2000/svg" class="fmp" viewBox="0 0 {W} {H}" role="img" '
                    f'aria-label="{kind["aria"]}">', style(),
                    f'<rect x="4" y="4" width="{W - 8}" height="{H - 8}" rx="44" class="body"/>',
                    left(kind), screen(screen_png), right(kind), keybed(kind), "</svg>"])


def main():
    shots = [Path(a) for a in sys.argv[1:3]]
    for kind, default in ((FM1, OUT / "screen-main.png"), (FX1, OUT / "fx1-screen.png")):
        png = shots.pop(0) if shots else default
        (OUT / kind["file"]).write_text(panel(kind, png), encoding="utf-8")
        print(f"figure: {OUT / kind['file']}")


if __name__ == "__main__":
    main()
