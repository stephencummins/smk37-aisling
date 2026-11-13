#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The SMK-37 Elite's map of the controls, drawn (no photographs: M-VAVE's are not ours to use):

  docs/img/smk37-panel.svg   the Elite's top panel, AISLING's real screen in it, each knob, pad, fader, button and
                             key labelled with what it does in AISLING

  tools/gen_smk37_figure.py [SCREEN.png]     (default docs/img/screen-main.png: the Elite's screen is the FM-1's,
                                             pixel for pixel)

As tools/gen_manual_figures.py draws the FM-1's. Where the controls sit follows the Elite: the two wheels at the
left, the screen with six small buttons under it, eight knobs over two rows of eight pads, four faders over two rows
of six buttons, and the 37 keys, C3 to C6, below (checked against the panel drawing in genkigenki's fm1-to-smk37
guide; the drawing here is our own). The pads are in the colours their lights take (ui.c's PAD_RGB)."""
import base64
import io
import sys
from pathlib import Path

from PIL import Image

SRC = Path(__file__).resolve().parents[1]
OUT = SRC / "docs" / "img"
FONT = "'Helvetica Neue', Helvetica, Arial, sans-serif"

BODY, TRAY, EDGE = "#1b1d21", "#26292f", "#3d4149"
INK, MUTED, GREEN = "#f1efe6", "#b4b1a4", "#3fce86"
KNOB, KNOB_EDGE, BTN = "#0e0f11", "#565b65", "#33373f"
KEY_W, KEY_W_OFF, KEY_B, KEY_B_OFF = "#f1efe6", "#7d8087", "#0e0f11", "#3a3d44"
GROUPS = (("Rhythm", "#FF5A10"), ("Echo", "#00D2AA"), ("Space", "#A050FF"), ("Colour", "#FFBE00"))
PADS = (("Stutter", "Half time", "Gate", "Roll", "Repeat", "Macalla", "Tape stop", "Reverse"),
        ("Reverb", "Shimmer", "Freeze", "Wash", "Flange", "Phaser", "Drive", "Crush"))
BLACK_FX = ("Ceo", "Macalla", "Fras", "Repeat", "Shimmer", "Freeze", "Octave", "Return", "Rise", "Close", "Transpose")
WHITE = "C D E F G A B".split()
BLACK_AFTER = (0, 1, 3, 4, 5)                    # in an octave: a black key after these white keys
FIRST, LAST = 3, 18                              # the white keys that sound: F3 (the 4th) to G5 (the 19th)

W, H = 1900, 924


def png_data_uri(path):
    b = io.BytesIO()
    Image.open(path).convert("RGB").save(b, "PNG", optimize=True)
    return "data:image/png;base64," + base64.b64encode(b.getvalue()).decode()


def text(x, y, t, cls, anchor="middle"):
    return f'<text x="{x:.1f}" y="{y:.1f}" text-anchor="{anchor}" class="{cls}">{t}</text>'


def lines(x, y, t, cls, anchor="middle", lead=19):
    """one or two lines, split at |"""
    return "".join(text(x, y + i * lead, part, cls, anchor) for i, part in enumerate(t.split("|")))


def knob(x, y, r, label, does=None):
    return (f'<circle cx="{x}" cy="{y}" r="{r}" fill="{KNOB}" stroke="{KNOB_EDGE}" stroke-width="2.5"/>'
            f'<line x1="{x}" y1="{y - r + 5}" x2="{x}" y2="{y - r + 16}" stroke="{INK}" stroke-width="3" stroke-linecap="round"/>'
            + text(x, y - r - 11, label, "lbl") + (lines(x, y + r + 23, does, "what") if does else ""))


def button(x, y, w, h, label):
    parts = label.split("|")
    t = (text(x + w / 2, y + h / 2 + 5, parts[0], "btn") if len(parts) == 1 else
         text(x + w / 2, y + h / 2 - 2, parts[0], "btn") + text(x + w / 2, y + h / 2 + 13, parts[1], "btn"))
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="7" fill="{BTN}" stroke="{EDGE}" stroke-width="1.5"/>{t}'


def wheels():
    s = []
    for i, name in enumerate(("PITCH", "MOD")):
        x = 44 + i * 64
        s.append(f'<rect x="{x}" y="70" width="48" height="190" rx="12" fill="{KNOB}" stroke="{KNOB_EDGE}" stroke-width="2"/>')
        s += [f'<line x1="{x + 10}" y1="{y}" x2="{x + 38}" y2="{y}" stroke="{EDGE}" stroke-width="2"/>'
              for y in range(96, 250, 23)]
        s.append(text(x + 24, 284, name, "lbl small"))
    s.append(text(100, 308, "Not used yet", "what"))
    return "".join(s)


def screen(png):
    left = [("KNOB|BANK", "Nothing yet"), ("&lt;", "All pads let go"), ("OCT−", "Octave down")]
    right = [("FADER|BANK", "Faders' bank"), ("&gt;", "With a pad: latch"), ("OCT+", "Octave up")]
    s = [f'<rect x="268" y="34" width="216" height="216" rx="14" fill="#08090a" stroke="{EDGE}" stroke-width="2"/>',
         f'<image x="286" y="52" width="180" height="180" href="{png_data_uri(png)}" style="image-rendering:pixelated"/>']
    for i in range(3):
        y = 268 + i * 44
        s.append(button(286, y, 84, 36, left[i][0]) + text(276, y + 23, left[i][1], "what", "end"))
        s.append(button(382, y, 84, 36, right[i][0]) + text(476, y + 23, right[i][1], "what", "start"))
    return "".join(s)


def knobs_pads():
    x0, pitch = 672, 90
    does = ("Volume", "Level|(a layer's page)", "Voice", "Piece")
    s = [knob(x0 + i * pitch, 80, 23, f"K{i + 1}", does[i] if i < 4 else None) for i in range(8)]
    a, b = x0 + 4 * pitch - 30, x0 + 7 * pitch + 30
    s.append(f'<path d="M{a} 116 v8 H{b} v-8" fill="none" stroke="{MUTED}" stroke-width="1.5"/>'
             + text((a + b) / 2, 145, "The four values on the screen", "what"))
    for r in range(2):
        for i in range(8):
            c = GROUPS[r * 2 + i // 4][1]
            x, y = x0 + i * pitch - 41, 172 + r * 76
            s.append(f'<rect x="{x}" y="{y}" width="82" height="66" rx="9" fill="{c}" fill-opacity=".3" stroke="{c}" stroke-width="2.5"/>'
                     + text(x + 41, y + 39, PADS[r][i], "pad"))
    y = 362
    for g, (name, c) in enumerate(GROUPS):
        x = x0 - 41 + g * 150
        s.append(f'<rect x="{x}" y="{y - 15}" width="20" height="20" rx="5" fill="{c}"/>' + text(x + 30, y + 1, name, "what", "start"))
    s.append(text(x0 - 41, y + 34, "A pad: its effect on the whole music while it is down. It glows in its group's colour, bright while held.", "what", "start"))
    s.append(text(x0 - 41, y + 58, "&gt; held, a pad hit: the effect stays on (its light beats).   &lt;: every pad let go.", "what", "start"))
    s.append(text(x0 - 41, y + 82, "A pad down, K5 to K8 turned: a throw of Filter, Wet, Energy or Duck.", "what", "start"))
    return "".join(s)


def faders_buttons():
    x0 = 1392
    mix = ("Drone", "Sequence", "Sequence 2", "Lead")
    bank = ("Filter", "Wet", "Energy", "Duck")
    s = []
    for i in range(4):
        x = x0 + 42 + i * 128
        s.append(text(x, 46, f"F{i + 1}", "lbl")
                 + f'<rect x="{x - 4}" y="58" width="8" height="118" rx="4" fill="{KNOB}" stroke="{EDGE}" stroke-width="1.5"/>'
                 + f'<rect x="{x - 27}" y="104" width="54" height="22" rx="5" fill="{BTN}" stroke="{KNOB_EDGE}" stroke-width="2"/>'
                 + text(x, 198, mix[i], "what") + text(x, 220, bank[i], "what green"))
    s.append(text(x0 + 3 * 82 + 35, 244, "The mix; in green, with FADER BANK lit", "what small"))
    top = [("ARP", "Sequence"), ("NOTE|REPEAT", "Lead"), ("SCALE", "Perform|held: Lock"), ("CHORD", "Drone"),
           ("GLOBE", "Setup, Arc|held: save"), ("BT", "Atmosphere|held: save")]
    bot = [("PATCH", "Aisling,|Key"), ("PARA", "Chords|held: next"), ("FX", "Space"), ("SEQ", "Sequence 2"),
           ("SEQ|PLAY", "Play /|rest"), ("SEQ|REC", "Hold|held: effects")]
    s.append(f'<rect x="{x0 - 16}" y="258" width="{6 * 82 + 20}" height="226" rx="14" fill="{TRAY}"/>')
    for r, row in enumerate((top, bot)):
        for i, (name, does) in enumerate(row):
            x, y = x0 + i * 82, 272 + r * 106
            s.append(button(x, y, 70, 42, name) + lines(x + 35, y + 62, does, "what tray", lead=17))
    return "".join(s)


def keybed():
    x0, y0, w, h = 40, 548, W - 80, 300
    n = 22
    pitch = w / n
    s = [f'<rect x="{x0 - 8}" y="{y0 - 8}" width="{w + 16}" height="{h + 16}" rx="10" fill="{TRAY}"/>']
    for i in range(n):
        on = FIRST <= i <= LAST
        x = x0 + i * pitch
        s.append(f'<rect x="{x + 1.5:.1f}" y="{y0}" width="{pitch - 3:.1f}" height="{h}" rx="5" fill="{KEY_W if on else KEY_W_OFF}"/>')
        note = WHITE[i % 7] + (str(3 + i // 7) if WHITE[i % 7] == "C" or i in (FIRST, LAST) else "")
        s.append(text(x + pitch / 2, y0 + h - 16, note, "key" if on else "key off"))
    k = 0
    for i in range(n - 1):
        if i % 7 not in BLACK_AFTER:
            continue
        on = FIRST <= i < LAST
        x, bw, bh = x0 + (i + 1) * pitch, 48, 186
        s.append(f'<rect x="{x - bw / 2:.1f}" y="{y0}" width="{bw}" height="{bh}" rx="4" fill="{KEY_B if on else KEY_B_OFF}"/>')
        if on:
            s.append(f'<text transform="translate({x + 6:.1f} {y0 + bh - 14}) rotate(-90)" class="fx">{BLACK_FX[k]}</text>')
            k += 1
    a, b = x0 + FIRST * pitch, x0 + (LAST + 1) * pitch
    s.append(f'<path d="M{a + 2:.1f} {y0 - 14} v-10 H{b - 2:.1f} v10" fill="none" stroke="{GREEN}" stroke-width="2"/>')
    s.append(text((a + b) / 2, y0 - 34, "F3 to G5 play. White keys: the Lead, in the piece's scale. Black keys: an effect each while held.", "what"))
    s.append(text(x0 + FIRST * pitch / 2, y0 - 34, "Silent for now", "what dim"))
    s.append(text(b + (x0 + w - b) / 2, y0 - 34, "Silent for now", "what dim"))
    s.append(text(W / 2, y0 + h + 44, "SCALE (PERFORM): every one of the 27 keys an effect while held.   "
                                      "OCT− with OCT+, five seconds: the update mode.", "what"))
    return "".join(s)


STYLE = (f"<style>text{{font-family:{FONT}}}.lbl{{font-weight:600;font-size:18px;fill:{INK};letter-spacing:.06em}}"
         f".lbl.small{{font-size:15px}}.btn{{font-weight:600;font-size:13px;fill:{INK};letter-spacing:.04em}}"
         f".what{{font-size:16px;fill:{MUTED}}}.what.tray{{font-size:14px;fill:{INK}}}.what.small{{font-size:14px}}.what.green{{fill:{GREEN}}}"
         f".what.dim{{fill:#8a8d94}}.pad{{font-weight:600;font-size:15px;fill:{INK}}}"
         f".key{{font-weight:600;font-size:17px;fill:#1b1d21}}.key.off{{fill:#2b2d32}}"
         f".fx{{font-weight:600;font-size:15px;fill:{INK}}}</style>")


def panel(screen_png):
    return "".join([f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" role="img" '
                    f'aria-label="The SMK-37 Elite\'s panel running AISLING, each control labelled with what it does">',
                    STYLE, f'<rect x="4" y="4" width="{W - 8}" height="{H - 8}" rx="30" fill="{BODY}" stroke="{EDGE}" stroke-width="2"/>',
                    wheels(), screen(screen_png), knobs_pads(), faders_buttons(), keybed(), "</svg>"])


def main():
    png = Path(sys.argv[1]) if len(sys.argv) > 1 else OUT / "screen-main.png"
    (OUT / "smk37-panel.svg").write_text(panel(png), encoding="utf-8")
    print(f"figure: {OUT / 'smk37-panel.svg'}")


if __name__ == "__main__":
    main()
