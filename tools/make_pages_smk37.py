#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The SMK-37 Elite's site: AISLING on the M-VAVE SMK-37 Elite, in the FM-1 site's dress (web/brand, tools/site_brand.py
with this site's own nav), for the gh-pages branch of stephencummins/smk37-aisling (tools/publish_pages_smk37.sh). It
says where the port stands; it offers no download until a release is made for the Elite.

  sh host/build_host.sh && python3 tools/make_pages_smk37.py build/pages_smk37

  index.html          where the port stands, what it is, its screens
  controls/           the Elite's controls and the FM-1 job each does: the README's "## In short" under the Elite's names
                      (firmware/hal/smk37/smk_input.h), with what the Elite lacks and what it doesn't use yet
  docs/               docs/SMK37.md, drawn as the FM-1 site draws its manual (tools/manual_html.py)
  img/                the screens, rendered here by the Elite's simulator (build/host/amb_host_smk37) from the manual's
                      own scenario (tests/listen/docshots.amb, renamed by tests/smk37_scenario.py); docs/card.png
  the rest            web/brand: the harp, site.css, wall.js, the fonts (not Cloudflare's _headers and _redirects)

Stops if the README's table, or the header's names, no longer say what this expects."""
import html
import importlib.util
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(SRC / "tools"))
from site_brand import stamp  # noqa: E402
import manual_html  # noqa: E402

_spec = importlib.util.spec_from_file_location("smk37_scenario", SRC / "tests" / "smk37_scenario.py")
smk37_scenario = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(smk37_scenario)

REPO = "https://github.com/stephencummins/smk37-aisling"
SITE = "https://stephencummins.github.io/smk37-aisling/"
FM1 = "https://aisling.stephen8n.com/"
BY = 'by CU_STE · <a href="https://www.stephen8n.com">www.stephen8n.com</a>'
# (the page's key, its label, where it goes, what it is), as site_brand.NAV
NAV = (("controls", "controls", "controls/", "The Elite's buttons, knobs and keys, and the FM-1 job each does"),
       ("docs", "docs", "docs/", "How the port works: the flash, the pins, building, testing, and what's still open"),
       ("fm1", "fm-1 version", FM1, "AISLING on the FM-1: install it, the manual, the emulator, the visualiser"),
       ("source", "source", REPO, "The code, on GitHub: one tree for the FM-1 and the Elite"),
       ("stephen8n", "stephen8n", "https://www.stephen8n.com", "Stephen Cummins's other things"))
SHOTS = ("screen-launch.png", "screen-main.png", "screen-perform.png")

# The rows of the README's "## In short" table, under the Elite's names: (the FM-1's button, the README's row for it,
# which part of a row that names several, joined by " · "). The Elite's names come from smk_input.h.
BUTTONS = (("PLAY", "PLAY · REC · EDIT", 0), ("REC", "PLAY · REC · EDIT", 1), ("EDIT", "PLAY · REC · EDIT", 2),
           ("SEL", "SEL", None), ("HOME", "HOME", None), ("ENV", "ENV", None), ("ARP", "ARP", None),
           ("LFO", "LFO", None), ("SEQ", "SEQ", None), ("SAVE", "SAVE", None), ("FX", "FX", None),
           ("GLO", "GLO", None))
KNOBS = (("SELECT", "SELECT"), ("ALGORITHM", "ALGORITHM"), ("PRESETS", "PRESETS"))
# what the README says of the FM-1's lit keys, which the Elite doesn't have: taken out of its rows
UNLIT = {"White keys": " (the key it sounds is lit)", "Black keys": " (lit; dark while held)"}


def readme_rows():
    """The README's "## In short" as {first cell: second cell}, the cells as written (markdown)."""
    lines = (SRC / "README.md").read_text(encoding="utf-8").splitlines()
    i = lines.index("## In short")
    rows = {}
    for line in lines[i + 1:]:
        if line.startswith("|"):
            c = [x.strip() for x in line.strip().strip("|").split("|")]
            if len(c) == 2 and c[0] and not set(c[0]) <= set("-"):
                rows[c[0]] = c[1]
        elif rows:
            break
    if not rows:
        raise SystemExit("README.md: no table under ## In short")
    return rows


def smk_names():
    """{the FM-1's name: the Elite's} for the buttons and the knobs, from the comments of smk_input.h's SMK_BTN and
    SMK_ENC, where each line names both."""
    h = (SRC / "firmware" / "hal" / "smk37" / "smk_input.h").read_text(encoding="utf-8")
    btn = re.search(r"SMK_BTN\[SMK_NBTN\] = \{(.*?)\};", h, re.S).group(1)
    names = {fm1: smk for smk, fm1 in re.findall(r"/\*\s*\d+ (\S+(?: \S+)?)\s{2,}(\S+) \*/", btn)}
    enc = re.search(r"SMK_ENC\[FM1_NENC\]\[2\] = \{(.*?)\};", h, re.S).group(1)
    names.update({fm1: k for k, fm1 in re.findall(r"/\*\s*\d (K\d)\s+(\S+(?: \d)?) \*/", enc)})
    want = [b for b, _, _ in BUTTONS] + ["OCT-", "OCT+", "MASTER", "KNOB 1", "KNOB 4"] + [k for k, _ in KNOBS]
    missing = [n for n in want if n not in names]
    if missing:
        raise SystemExit(f"smk_input.h: no Elite name for {missing}")
    return names


def md(cell):
    """A README cell as HTML: **bold**, *italics*, a capital to start."""
    c = html.escape(cell, quote=False)
    c = re.sub(r"\*([^*]+)\*", r"<i>\1</i>", re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", c))
    return c[:1].upper() + c[1:]


def controls_table():
    rows, names = readme_rows(), smk_names()
    out = []

    def row(elite, fm1, what):
        out.append(f'<tr><th scope="row">{elite}</th><td class="fm1">{fm1}</td><td>{what}</td></tr>\n')

    def readme(key, part=None):
        if key not in rows:
            raise SystemExit(f"README.md ## In short: no row {key!r}")
        text = rows[key]
        if part is not None:
            parts = text.split(" · ")
            if len(parts) != len(key.split(" · ")):
                raise SystemExit(f"README.md ## In short: row {key!r} doesn't split into {key.split(' · ')}")
            text = parts[part]
        return md(text)

    out.append('<tbody>\n<tr class="sub"><th scope="rowgroup" colspan="3">The buttons</th></tr>\n')
    for fm1, key, part in BUTTONS:
        row(names[fm1], fm1, readme(key, part))
    oct_ = f"{names['OCT-'].replace('-', '−')} / {names['OCT+']}"
    row(oct_, "OCT− / OCT+", readme("OCT− / OCT+"))
    row("FADER BANK", "", "The faders: the mix, or (lit) Filter, Wet, Energy and Duck.")
    row("&gt;", "", "Held, with a pad: latches that pad's effect. Lit while any is latched.")
    row("&lt;", "", "Lets every pad and latch go.")
    row("KNOB BANK", "", "Nothing yet.")
    out.append('</tbody>\n<tbody>\n<tr class="sub"><th scope="rowgroup" colspan="3">The knobs</th></tr>\n')
    row(names["MASTER"], "MASTER",
        "The volume. The Elite's knobs turn without end, so this one starts at half (−6&nbsp;dB) each time it is "
        "switched on; 43 clicks take it from silence to full.")
    for fm1, key in KNOBS:
        row(names[fm1], fm1, readme(key))
    row(f"{names['KNOB 1']}–{names['KNOB 4']}", "K1–K4", "The four settings the screen shows, in order.")
    out.append('</tbody>\n<tbody>\n<tr class="sub"><th scope="rowgroup" colspan="3">The keys</th></tr>\n')
    for key, elite, fm1 in (("White keys", "White keys F3–G5", "the 16 white"),
                            ("Black keys", "Black keys F♯3–F♯5", "the 11 black")):
        text = rows.get(key, "")
        if UNLIT[key] not in text:
            raise SystemExit(f"README.md ## In short: row {key!r} no longer says {UNLIT[key]!r}")
        row(elite, fm1, md(text.replace(UNLIT[key], "")))
    row("C3–E3, G♯5–C6", "", "Silent for now: the FM-1 has 27 keys, and these are the Elite's other ten.")
    out.append("</tbody>\n")
    return ('<table>\n<thead><tr><th scope="col">Elite</th><th scope="col">FM-1</th><th scope="col">What it does</th>'
            '</tr></thead>\n' + "".join(out) + "</table>")


HEAD_COMMON = """<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
"""

STYLE = """/* This site's own, on the FM-1 site's bench (web/brand/site.css) */
.notice { border-left: 4px solid var(--brand-rust); background: var(--brand-strip); }
.notice p { margin: 0 0 8px; }
.notice p:last-child { margin-bottom: 0; }
.notice strong { font: 600 16px/1.3 var(--brand-display); letter-spacing: .02em; color: var(--brand-rust); }
.pane p { margin: 0 0 10px; }
.pane p:last-child { margin-bottom: 0; }
.pane p b, td b, td i { font-weight: 400; font-style: normal; color: var(--brand-green); }
.bh { margin: 0 0 10px; }
p.small { color: var(--brand-muted); font-size: 12.5px; }
"""

LANDING = """<!doctype html>
<html lang="en">
<head>
__HEAD_COMMON__<title>AISLING · Irish ambient for the SMK-37 Elite</title>
<meta name="description" content="AISLING, the generative Irish ambient instrument for the M-VAVE FM-1, ported to the M-VAVE SMK-37 Elite: the same engine and pieces on a 37-key keyboard. It runs on an Elite; a download follows.">
<meta property="og:title" content="AISLING · Irish ambient for the SMK-37 Elite">
<meta property="og:description" content="The FM-1's AISLING, ported to the M-VAVE SMK-37 Elite. It runs on an Elite; a download follows.">
<meta property="og:url" content="__SITE__">
<style>
__STYLE__.what { display: flex; gap: 16px; align-items: flex-start; }
.card { flex: none; width: 96px; height: 96px; image-rendering: pixelated; border: 1px solid var(--brand-rule); }
.lede { margin: 0; font-size: 14.5px; }
.shots { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 10px; align-content: start; }
.shots img { display: block; box-sizing: border-box; width: 100%; height: auto; image-rendering: pixelated; border: 1px solid var(--brand-rule); }
.shots p { grid-column: 1 / -1; margin: 0; }
.ways { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 1px; background: var(--brand-rule); }
.ways a { display: block; padding: 12px 16px 14px; background: var(--brand-ground); color: var(--brand-ink); text-decoration: none; }
.ways a:hover { background: var(--brand-strip); color: var(--brand-ink); }
.ways a:focus-visible { outline-offset: -3px; }
.ways strong { display: block; margin-bottom: 4px; font: 600 16px/1.3 var(--brand-display); letter-spacing: .02em; }
.ways span { display: block; font-size: 12.5px; color: var(--brand-muted); }
.ways a.go, .ways a.go span { background: var(--brand-green); color: var(--brand-ground); }
.ways a.go:hover, .ways a.go:hover span { background: #0f5332; }
.ways a.go:focus-visible { outline-color: var(--brand-ground); }
ol.stages { list-style: none; padding: 0; margin: 0; counter-reset: stage; }
ol.stages li { position: relative; padding: 0 0 10px 92px; counter-increment: stage; }
ol.stages li:last-child { padding-bottom: 0; }
ol.stages li::before { content: counter(stage) " · " attr(data-state); position: absolute; left: 0; top: 1px; width: 80px;
                       font: 400 12px/1.5 var(--brand-mono); color: var(--brand-muted); }
ol.stages li.done::before { color: var(--brand-green); }
ol.stages b.lead { font-weight: 600; color: var(--brand-ink); }
@media (max-width: 59.99rem) {
  .two.hero { grid-template-columns: minmax(0, 1fr); }
  .two.hero > * + * { border-left: 0; border-top: 1px solid var(--brand-rule); }
}
@media (max-width: 47.99rem) { .ways { grid-template-columns: minmax(0, 1fr); } }
@media (max-width: 26rem) {
  .what { display: block; } .card { float: left; margin: 0 12px 4px 0; width: 72px; height: 72px; }
  ol.stages li { padding-left: 0; } ol.stages li::before { position: static; display: block; width: auto; }
}
</style>
<!--AISLING-HEAD-->
</head>
<body>
<!--AISLING-BAR-->
<main class="bench">
<div class="strip">
<h1 class="lab"><b>aisling</b> · Irish ambient for the M-VAVE SMK-37 Elite</h1>
<p class="lab">__BY__</p>
</div>
<div class="pane notice" role="note">
<p><strong>It runs on an Elite.</strong> In November 2026 AISLING 1.3.1 was installed on an SMK-37 Elite, played, taken
off again with M-VAVE's own firmware, and installed again. There is no download here yet: that comes with its release.
The <a href="docs/">docs</a> say what was checked and what is still open.</p>
<p>It is for the <b>SMK-37 Elite</b> only (not the SMK-37 Pro), on M-VAVE's firmware V016.</p>
</div>
<div class="two hero">
<div class="pane what">
<img class="card" src="img/card.png" width="240" height="240" alt="A gold 8-bit harp on green">
<p class="lede"><a href="__FM1__">AISLING</a> is a generative instrument for the M-VAVE FM-1, after the harpers and
the west of Ireland: a harp's ostinato over a held drone, a second harp drifting against it, a flute far back in the
reverb, with surf, wind or rain behind. The SMK-37 Elite has the FM-1's chip, so it can run the same code: the same
engine, the same eight pieces, saved the same way, on a 37-key keyboard. Its buttons and knobs take the FM-1's jobs,
its middle 27 keys are the FM-1's, its pads play the effects and its faders set the mix.</p>
</div>
<div class="pane shots">
  <img src="img/screen-launch.png" width="240" height="240" alt="The launch screen: a green triskele, AISLING, by CU_STE, www.stephen8n.com">
  <img src="img/screen-main.png" width="240" height="240" alt="The main screen: the chord's name among orbiting dots, the five layers' meters, the two harps' lanes">
  <img src="img/screen-perform.png" width="240" height="240" alt="PERFORM: Filter, Wet, Energy and Sweep on the knobs">
  <p class="small">The Elite's screens, from its simulator: the same, pixel for pixel, as the FM-1's.</p>
</div>
</div>
<nav class="ways" aria-label="AISLING on the Elite">
  <a class="go" href="controls/"><strong>The controls</strong><span>Which of the Elite's buttons, knobs and keys does each of the FM-1's jobs, its pads and faders, and what it doesn't use yet.</span></a>
  <a href="docs/"><strong>The docs</strong><span>How the port works: the Elite's flash and pins, building and testing it, installing and going back, and what is still open.</span></a>
  <a href="__FM1__emu/"><strong>Try it in the browser</strong><span>The FM-1 version's emulator: the same code, on the FM-1's panel, with sound. No Elite needed.</span></a>
  <a href="__REPO__"><strong>Source</strong><span>GitHub, GPL-3.0: one tree that builds AISLING for the FM-1 and for the Elite.</span></a>
</nav>
<section class="pane" aria-labelledby="stagesh">
<h2 class="bh" id="stagesh">Where it stands</h2>
<ol class="stages">
  <li class="done" data-state="done"><b class="lead">On a computer.</b> AISLING builds for the Elite from the FM-1's
  tree, and the FM-1's own build is unchanged to the byte. The Elite's simulator plays all 34 of the FM-1's test
  scenarios with byte-identical results, and its own: MASTER on a knob, the 37 keys, the battery. The installer and the
  rescue tool know the Elite's package, and never write the parts of its memory that are the Elite's own.</li>
  <li class="done" data-state="done"><b class="lead">A backup.</b> A whole copy of an Elite's memory, read twice and
  compared, before anything was written to it.</li>
  <li class="done" data-state="done"><b class="lead">The first install.</b> Every button, knob and key checked on the
  Elite, and the way back to M-VAVE's firmware proven: off, and on again.</li>
  <li class="done" data-state="done"><b class="lead">The pads and faders.</b> The pads play the sixteen effects, and
  the faders set the mix.</li>
  <li data-state="next"><b class="lead">The last checks.</b> An hour's run, the battery's reading, the web installer
  on an Elite.</li>
  <li data-state="to do"><b class="lead">A download.</b> The package on this page.</li>
  <li data-state="to do"><b class="lead">Later.</b> The pitch wheel bends the Lead, the mod wheel opens the filters,
  the outer white keys extend the Lead's scale.</li>
</ol>
</section>
<div class="pane">
<p class="small">AISLING is by CU_STE (<a href="https://www.stephen8n.com">www.stephen8n.com</a>). Its board layer
for the Elite follows M. Geisler's (genkigenki) SMK-37 ports, on the <code>smk37</code> branches of
<a href="https://github.com/genkigenki/fm1-amb/tree/smk37">fm1-amb</a> and
<a href="https://github.com/genkigenki/fm1-fomni/tree/smk37">fm1-fomni</a>, written again for AISLING; the hardware's
facts are from M. Geisler's guide, <a href="https://github.com/genkigenki/fm1-to-smk37">FM-1 to SMK-37 Pro</a>
(CC BY-NC 4.0), cited, not copied. AISLING itself is a fork of
<a href="https://github.com/FartyPants/fm1-nachtfahrt">NACHTFAHRT</a> by FartyPants, based on
<a href="https://github.com/sbegueria/fm1-dub">DUB-1</a> by sbegueria, itself based on
<a href="https://github.com/charlesvestal/fm1-amb">AMB-1</a> by Charles Vestal; the platform is Felucca by Leo Kuroshita
(Hügelton Instruments). GPL-3.0; <a href="__REPO__/blob/main/LICENSING.md">LICENSING.md</a> has the rest.
M-VAVE, FM-1 and SMK-37 are trademarks of their owners. AISLING is not affiliated with M-VAVE, Hügelton Instruments or
the authors of the ports it follows. Its faces are Barlow Semi Condensed and IBM Plex Mono (SIL OFL 1.1).</p>
</div>
</main>
</body>
</html>
"""

CONTROLS = """<!doctype html>
<html lang="en">
<head>
__HEAD_COMMON__<title>AISLING on the SMK-37 Elite · the controls</title>
<meta name="description" content="Which of the SMK-37 Elite's buttons, knobs and keys does each of the FM-1's jobs in AISLING, its pads and faders, and what it doesn't use yet.">
<style>
__STYLE__.wrap { overflow-x: auto; }
table { border-collapse: collapse; width: 100%; }
th, td { text-align: left; vertical-align: top; padding: 5px 10px 5px 0; border-top: 1px solid var(--brand-rule); }
thead th { font-weight: 400; font-size: 12.5px; color: var(--brand-muted); border-top: 0; }
tbody th { font-weight: 600; white-space: nowrap; }
td.fm1 { font: 400 12.5px/1.6 var(--brand-mono); color: var(--brand-muted); white-space: nowrap; }
tr.sub th { padding-top: 14px; font: 600 16px/1.3 var(--brand-display); letter-spacing: .02em; border-top: 0; }
tbody:first-of-type tr.sub th { padding-top: 4px; }
ul.plain { list-style: square; padding-left: 1.2rem; margin: 0 0 10px; }
ul.plain li { margin: 0 0 6px; }
ul.plain li::marker { color: var(--brand-green); }
@media (max-width: 47.99rem) {
  /* a phone: each row's names on one line, what it does under them */
  thead { position: absolute; width: 1px; height: 1px; overflow: hidden; clip: rect(0 0 0 0); }
  table, tbody { display: block; }
  tr { display: grid; grid-template-columns: auto minmax(0, 1fr); column-gap: 12px; padding: 6px 0;
       border-top: 1px solid var(--brand-rule); }
  tr.sub { border-top: 0; padding: 0; }
  th, td { padding: 0; border-top: 0; }
  tbody th { white-space: normal; }
  td.fm1:not(:empty)::before { content: "FM-1 "; }
  td:last-child { grid-column: 1 / -1; padding-top: 2px; }
  tr.sub th { grid-column: 1 / -1; }
}
</style>
<!--AISLING-HEAD-->
</head>
<body>
<!--AISLING-BAR-->
<main class="bench">
<div class="strip">
<h1 class="lab"><b>aisling</b> · the controls on the SMK-37 Elite</h1>
<p class="lab">__BY__</p>
</div>
<div class="pane notice" role="note">
<p><strong>Checked on an Elite.</strong> Each button, knob, key, pad and fader here was tried by hand on an SMK-37
Elite in November 2026. The FM-1
version's <a href="__FM1__manual/">manual</a> and <a href="__FM1__cheat/">cheat sheet</a> say more about each job; where
they or the screen name a button, it is the FM-1's name, in the second column here.</p>
</div>
<div class="pane wrap">
__TABLE__
</div>
<div class="two">
<section class="pane">
<h2 class="bh">The pads and the faders</h2>
<ul class="plain">
  <li><b>The 16 pads:</b> PERFORM's effects, on any page, each on while its pad is down. Top row: Stutter, Half time,
  Gate, Roll, Repeat, Macalla, Tape stop, Reverse. Bottom row: Reverb, Shimmer, Freeze, Wash, Flange, Phaser, Drive,
  Crush.</li>
  <li><b>The pads' lights:</b> each glows in its group's colour: orange for the rhythm effects, sea green for the
  echoes, heather for the space, yellow for the colour. Bright while held, beating while latched, dark asleep.</li>
  <li><b>&gt; held, a pad hit:</b> that effect stays on; the same again lets it go. &gt; is lit while any is
  latched.</li>
  <li><b>&lt; tapped:</b> every pad and latch let go.</li>
  <li><b>A pad down, K5 to K8 turned:</b> a throw of Filter, Wet, Energy or Duck, which go back when the last pad is
  up.</li>
  <li><b>The 4 faders:</b> the mix, left to right: the Drone, the Sequence, Sequence 2 and the Lead. With FADER BANK
  lit: Filter, Wet, Energy and Duck. A fader does nothing until it is moved.</li>
</ul>
<p><b>Not used yet:</b> the pitch and mod wheels, the pedal, KNOB BANK, the outer keys and
Bluetooth.</p>
</section>
<section class="pane">
<h2 class="bh">What the Elite doesn't have</h2>
<ul class="plain">
  <li><b>Lit keys.</b> The FM-1 lights the key the Lead sounds, and the black keys (dark while held); the Elite's keys
  have no lights. Its buttons do, and AISLING uses them as on the FM-1.</li>
  <li><b>MIDI in on the TRS jack.</b> On the Elite, MIDI comes in over USB only.</li>
</ul>
</section>
</div>
</main>
</body>
</html>
"""


DOCS = """<!doctype html>
<html lang="en">
<head>
__HEAD_COMMON__<title>AISLING on the SMK-37 Elite · the docs</title>
<meta name="description" content="How AISLING runs on the M-VAVE SMK-37 Elite: the controls, the flash, the pins, building, testing, installing and going back, and what is still open.">
<style>__CSS__/* this page's own: on a phone, a test's or a file's name may break, so its table fits */
@media (max-width: 47.99rem) { .prose td:first-child, .prose td:first-child code { white-space: normal; } }
</style>
<!--AISLING-HEAD-->
</head>
<body>
<a class="skip" href="#content">Skip to content</a>
<!--AISLING-BAR-->
<main id="content" class="bench">
__BODY__
</main>
</body>
</html>
"""
DOCS_STRIP = """<div class="strip">
<h1 class="lab"><b>docs</b> · AISLING on the M-VAVE SMK-37 Elite</h1>
<a class="btn" href="../controls/">the controls</a>
</div>
"""


def docs_body():
    """docs/SMK37.md as the FM-1 site's manual is drawn (the contents down the side), under this site's strip; its links
    up to the repo's other documents go to this repo, not the FM-1's."""
    body = manual_html.on_the_bench(manual_html.convert((SRC / "docs" / "SMK37.md").read_text(encoding="utf-8")))
    body, n = re.subn(r'<div class="strip">\n.*?</div>\n', lambda m: DOCS_STRIP, body, count=1, flags=re.S)
    if n != 1 or "<aside" not in body:
        raise SystemExit("docs/SMK37.md: manual_html.py no longer draws a strip and contents as this expects")
    # a path in a table may break after a slash, so the files table fits a phone
    body = re.sub(r"<td><code>([^<]*)</code>",
                  lambda m: "<td><code>" + m.group(1).replace("/", "/<wbr>") + "</code>", body)
    return body.replace("https://github.com/stephencummins/fm1-aisling/blob/main/", REPO + "/blob/main/")


def page(template, **marks):
    for k, v in marks.items():
        template = template.replace(f"__{k}__", v)
    left = re.findall(r"__[A-Z0-9_]+__", template)
    if left:
        raise SystemExit(f"a page still has {sorted(set(left))}")
    return template


def shots(out):
    """The landing page's screens, from the Elite's simulator playing the manual's screenshot scenario."""
    sim = SRC / "build" / "host" / "amb_host_smk37"
    if not sim.exists():
        raise SystemExit("no build/host/amb_host_smk37: run sh host/build_host.sh")
    work = SRC / "build" / "smk37_shots"
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    with open(SRC / "tests" / "listen" / "docshots.amb") as f:
        (work / "docshots.amb").write_text("".join(smk37_scenario.line(s, n) for n, s in enumerate(f, 1)))
    subprocess.run([str(sim), str(work / "docshots.amb"), str(work)], check=True, capture_output=True)
    for name in SHOTS:
        shutil.copy(work / name, out / "img" / name)


def main(out):
    out = Path(out)
    if out.exists():
        shutil.rmtree(out)
    (out / "img").mkdir(parents=True)
    (out / "controls").mkdir()
    (out / "docs").mkdir()
    shots(out)
    shutil.copy(SRC / "docs" / "card.png", out / "img" / "card.png")
    common = dict(HEAD_COMMON=HEAD_COMMON, STYLE=STYLE, BY=BY, FM1=FM1, REPO=REPO, SITE=SITE)
    (out / "index.html").write_text(stamp(page(LANDING, **common), "", None, NAV, SITE), encoding="utf-8")
    (out / "controls" / "index.html").write_text(
        stamp(page(CONTROLS, TABLE=controls_table(), **common), "../", "controls", NAV, SITE), encoding="utf-8")
    (out / "docs" / "index.html").write_text(
        stamp(page(DOCS, HEAD_COMMON=HEAD_COMMON, CSS=manual_html.CSS, BODY=docs_body()), "../", "docs", NAV, SITE),
        encoding="utf-8")
    shutil.copytree(SRC / "web" / "brand", out, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("_headers", "_redirects"))
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, controls/, docs/, img/ ({', '.join(SHOTS)} from the Elite's simulator)")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
