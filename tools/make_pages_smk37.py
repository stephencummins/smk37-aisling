#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The SMK-37 Elite's site: AISLING on the M-VAVE SMK-37 Elite, in the FM-1 site's dress (web/brand, tools/site_brand.py
with this site's own nav), for the gh-pages branch of stephencummins/smk37-aisling (tools/publish_pages_smk37.sh). It
says where the port stands, and where its release is.

  sh host/build_host.sh && python3 tools/make_pages_smk37.py build/pages_smk37
  python3 tools/make_pages_smk37.py --marks build/pages_smk37_marks
                      the same three pages with their two marks left in and no brand files, for the FM-1 site to
                      take in as its smk37/ (tools/make_pages.py --smk37 DIR stamps them with its own bar)

  index.html          where the port stands, what it is, its screens
  controls/           the Elite's controls and the FM-1 job each does: the README's "## In short" under the Elite's names
                      (firmware/hal/smk37/smk_input.h), with what the Elite lacks and what it doesn't use yet
  manual/             docs/SMK37.md, drawn as the FM-1 site draws its manual (tools/manual_html.py)
  img/                the screens, rendered here by the Elite's simulator (build/host/amb_host_smk37) from the manual's
                      own scenario (tests/listen/docshots.amb, renamed by tests/smk37_scenario.py); docs/card-smk37.png, the harp with 37
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
SITE = "https://aisling.stephen8n.com/smk37/"
FM1 = "https://aisling.stephen8n.com/"
BY = 'by CU_STE · <a href="https://www.stephen8n.com">www.stephen8n.com</a>'
# (the page's key, its label, where it goes, what it is), as site_brand.NAV
NAV = (("controls", "controls", "controls/", "The Elite's buttons, knobs and keys, and the FM-1 job each does"),
       ("manual", "manual", "manual/", "How the port works: the flash, the pins, building, testing, and what's still open"),
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
__STYLE__/* The rest is the bench's magazine parts (web/brand/site.css, mg-*) */
.mg-pic.px { align-items: center; justify-content: center; background: #146c41; }
.mg-pic.px img { width: min(100%, 384px); height: auto; aspect-ratio: 1; }
.mg-in p { margin: 0 0 10px; }
.mg-in p:last-child { margin-bottom: 0; }
.mg-in p b { font-weight: 400; color: var(--brand-green); }
.mg-in strong { font: 600 16px/1.3 var(--brand-display); letter-spacing: .02em; color: var(--brand-rust); }
.mg-draw.dark { background: #1c1610; }
/* the film: a dark tile in type (there is no photograph of an Elite here yet), a play mark in its corner */
.film { position: relative; display: grid; place-items: center; min-height: 250px; padding: 24px; background: #1c1610; color: #f3ead7;
        text-align: center; text-decoration: none; }
.film::before { content: ""; position: absolute; top: 16px; left: 18px; border-style: solid; border-width: 11px 0 11px 18px;
                border-color: transparent transparent transparent #e9a23b; }
.film span { font: 600 clamp(30px, 4.4vw, 54px)/.98 var(--brand-display); text-transform: uppercase; color: #e9a23b; transition: color .2s; }
.film small { display: block; padding: .5em 0; font: 400 clamp(11px, 1.1vw, 13px)/1.2 var(--brand-mono); letter-spacing: .1em;
              text-transform: none; color: #c9b99a; }
.film:hover span, .film:focus-visible span { color: #f3ead7; }
@media (prefers-reduced-motion: reduce) { .film span { transition: none; } }
ol.stages { list-style: none; padding: 0; margin: 0; counter-reset: stage; }
ol.stages li { position: relative; padding: 0 0 10px 92px; counter-increment: stage; }
ol.stages li:last-child { padding-bottom: 0; }
ol.stages li::before { content: counter(stage) " · " attr(data-state); position: absolute; left: 0; top: 1px; width: 80px;
                       font: 400 12px/1.5 var(--brand-mono); color: var(--brand-muted); }
ol.stages li.done::before { color: var(--brand-green); }
ol.stages b.lead { font-weight: 600; color: var(--brand-ink); }
@media (max-width: 26rem) {
  ol.stages li { padding-left: 0; } ol.stages li::before { position: static; display: block; width: auto; }
}
</style>
<!--AISLING-HEAD-->
</head>
<body>
<!--AISLING-BAR-->
<main class="bench">
<div class="strip">
<p class="lab"><b>aisling</b> · Irish ambient for the M-VAVE SMK-37 Elite</p>
<p class="lab">v1.3.1 · __BY__</p>
</div>

<div class="mg-cover">
<div class="mg-words">
<div>
<h1>Aisling <span>SMK-37</span></h1>
<p class="mg-gloss">For the SMK-37 Elite only (not the SMK-37 Pro), on M-VAVE's firmware V016.</p>
</div>
<p class="mg-deck">The FM-1's Irish ambient on a 37-key keyboard: the same engine, the same eight pieces, with pads for the effects and faders for the mix.</p>
</div>
<div class="mg-pic px"><img src="img/card-smk37.png" width="240" height="240" alt="A gold 8-bit harp on green, with 37 beside it"></div>
</div>

<nav class="mg-jump" aria-label="On this page">
<a href="#watch"><i>01</i>Watch it</a><a href="#try"><i>02</i>Try it</a><a href="#controls"><i>03</i>Controls</a><a href="#install"><i>04</i>Install</a>
</nav>

<section class="mg-feat" id="watch" aria-labelledby="watchh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">01</div>
<div><p class="mg-kick">The film</p><h2 id="watchh">See it played</h2></div></div>
<div class="mg-split">
<div>
<p class="mg-stand">AISLING on an SMK-37 Elite: <em>the keys, the pads and the faders at work</em>, with its sound.</p>
<p class="mg-go" style="margin:0"><a class="btn go big" href="https://youtu.be/869_HoE3s1g">Watch on YouTube</a></p>
</div>
<a class="film" href="https://youtu.be/869_HoE3s1g" tabindex="-1" aria-hidden="true"><span><small>AISLING on the SMK-37</small>Lots more<br>pots and pans<small>a film by CÚ_STE</small></span></a>
</div>
</section>

<section class="mg-feat" id="try" aria-labelledby="tryh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">02</div>
<div><p class="mg-kick">The emulator</p><h2 id="tryh">Try it in your browser</h2></div></div>
<div class="mg-split">
<div>
<p class="mg-stand">The FM-1 version's emulator: <em>the same code, with sound</em>, on the FM-1's panel. No Elite needed.</p>
<p class="mg-go"><a class="btn go big" href="__FM1__emu/">Open the emulator</a></p>
<p class="mg-small">The Elite's screens, from its simulator: the same, pixel for pixel, as the FM-1's.</p>
</div>
<div class="mg-shots">
  <img src="img/screen-launch.png" width="240" height="240" alt="The launch screen: a green triskele, AISLING, by CU_STE, www.stephen8n.com">
  <img src="img/screen-main.png" width="240" height="240" alt="The main screen: the chord's name among orbiting dots, the five layers' meters, the two harps' lanes">
  <img src="img/screen-perform.png" width="240" height="240" alt="PERFORM: Filter, Wet, Energy and Sweep on the knobs">
</div>
</div>
</section>

<section class="mg-feat" id="controls" aria-labelledby="controlsh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">03</div>
<div><p class="mg-kick">The controls</p><h2 id="controlsh">Every knob, pad, fader and key</h2></div></div>
<a class="mg-draw dark" href="controls/" title="The controls page">__PANEL__</a>
<p class="mg-stand" style="margin-top:18px">Its buttons and knobs take the FM-1's jobs, its middle 27 keys are the FM-1's, <em>its pads play the effects and its faders set the mix</em>.</p>
<p class="mg-go" style="margin:0"><a class="btn go big" href="controls/">See the controls</a></p>
</section>

<section class="mg-feat" id="install" aria-labelledby="installh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">04</div>
<div><p class="mg-kick">Install</p><h2 id="installh">Put it on your Elite</h2></div></div>
<ol class="mg-steps">
<li><b>From a computer,</b> for now: the web installer is still to come.</li>
<li><b>An SMK-37 Elite,</b> on M-VAVE's firmware V016.</li>
<li><b>The way back</b> to M-VAVE's firmware is proven: off, and on again.</li>
</ol>
<p class="mg-go"><a class="btn go big" href="manual/#s11">How to install</a></p>
<p class="mg-small"><a href="__REPO__/releases/latest">Download 1.3.1</a> · <a href="manual/">The manual</a> · <a href="__REPO__">Source</a> · <a href="__FM1__">The FM-1 version</a></p>
</section>

<section class="mg-fold" aria-label="More about AISLING on the Elite">
<details><summary>1.3.1, released <small>what was done on an Elite</small></summary><div class="mg-in">
<p><strong>1.3.1, released.</strong> In November 2026 AISLING 1.3.1 was installed on an SMK-37 Elite, played, taken
off again with M-VAVE's own firmware, and installed again. The package is in its
<a href="__REPO__/releases/latest">release</a>; it installs from a computer for now, as the
<a href="manual/#s11">manual</a> says, and it says what was checked and what is still open.</p>
<p>It is for the <b>SMK-37 Elite</b> only (not the SMK-37 Pro), on M-VAVE's firmware V016.</p>
</div></details>
<details><summary>Where it stands <small>six done, the web installer next</small></summary><div class="mg-in">
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
  <li class="done" data-state="done"><b class="lead">An hour's run.</b> An hour playing on an Elite with no
  dropouts.</li>
  <li class="done" data-state="done"><b class="lead">A release.</b> 1.3.1's package on GitHub, installed from a
  computer.</li>
  <li data-state="next"><b class="lead">The web installer.</b> AISLING put on an Elite from Chrome or Edge, as on the
  FM-1.</li>
  <li data-state="to do"><b class="lead">Later.</b> The pitch wheel bends the Lead, the mod wheel opens the filters,
  the outer white keys extend the Lead's scale.</li>
</ol>
</div></details>
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
svg.skp { display: block; width: 100%; min-width: 1100px; height: auto; }
/* the headline over the drawing */
.ct-head { display: grid; grid-template-columns: auto minmax(0, 1fr); align-items: end; gap: 6px 26px; }
.ct-head h1 { margin: 0; font: 700 clamp(40px, 7.4vw, 84px)/.86 var(--brand-display); letter-spacing: .01em; text-transform: uppercase; color: var(--brand-ink); }
.ct-head h1 span { color: var(--brand-rust); }
.pane.ct-head p { margin: 0 0 3px; max-width: 36ch; font: 600 clamp(16px, 1.8vw, 20px)/1.3 var(--brand-display); letter-spacing: .01em; }
@media (max-width: 47.99rem) { .ct-head { grid-template-columns: minmax(0, 1fr); } }
.skins { margin: 0 0 10px; }
.skins button { font: inherit; margin-left: 6px; padding: 2px 10px; border: 1px solid var(--brand-rule); border-radius: 4px;
                background: transparent; color: inherit; cursor: pointer; }
.skins button[aria-pressed="true"] { border-color: var(--brand-green); box-shadow: inset 0 0 0 1px var(--brand-green); }
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
<p class="lab"><b>aisling</b> · the controls on the SMK-37 Elite</p>
<p class="lab">__BY__</p>
</div>
<div class="pane ct-head">
<h1>The <span>controls</span></h1>
<p>Every knob, button, pad, fader and key, drawn. The table under the drawing says the same in words.</p>
</div>
<div class="pane notice" role="note">
<p><strong>Checked on an Elite.</strong> Each button, knob, key, pad and fader here was tried by hand on an SMK-37
Elite in November 2026. The FM-1
version's <a href="__FM1__manual/">manual</a> and <a href="__FM1__cheat/">cheat sheet</a> say more about each job; where
they or the screen name a button, it is the FM-1's name, in the second column here.</p>
</div>
<div class="pane wrap">
<p class="skins" id="skins" hidden>Your Elite's colours: __SKINS__</p>
__PANEL__
<p class="small"><a href="../img/smk37-panel.svg">The diagram alone</a>, at full size.</p>
<script>
(function () {
  var svg = document.querySelector("svg.skp"), bar = document.getElementById("skins"), key = "aisling-smk37-skin";
  function set(k) {
    svg.setAttribute("data-skin", k);
    bar.querySelectorAll("button").forEach(function (b) { b.setAttribute("aria-pressed", b.value === k ? "true" : "false"); });
  }
  var k = "black";
  try { k = localStorage.getItem(key) || k; } catch (e) {}
  bar.hidden = false;
  set(bar.querySelector('button[value="' + k + '"]') ? k : "black");
  bar.addEventListener("click", function (e) {
    if (e.target.tagName !== "BUTTON") return;
    set(e.target.value);
    try { localStorage.setItem(key, e.target.value); } catch (x) {}
  });
})();
</script>
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
  lit: Filter, Wet, Energy and Duck. A fader does nothing until it is moved. The mix is saved with the piece; Filter,
  Wet, Energy and Duck are not.</li>
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
__HEAD_COMMON__<title>AISLING on the SMK-37 Elite · the manual</title>
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
<p class="lab"><b>manual</b> · AISLING on the M-VAVE SMK-37 Elite</p>
<a class="btn" href="../controls/">the controls</a>
</div>
<div class="mg-cover">
<div class="mg-words">
<div><h1>The <span>Manual</span></h1>
<p class="mg-gloss">AISLING on the M-VAVE SMK-37 Elite.</p></div>
<p class="mg-deck">What differs from the FM-1, the flash and the pins, building, testing, installing and going back, in __N__ sections.</p>
</div>
<div class="mg-pic px"><img src="../img/screen-main.png" width="240" height="240" alt="AISLING's main screen, as the Elite shows it"></div>
</div>
<nav class="mg-jump" aria-label="Sections">__TILES__</nav>
"""
COUNTS = "no one two three four five six seven eight nine ten eleven twelve thirteen fourteen fifteen sixteen".split()


def docs_body():
    """docs/SMK37.md as the FM-1 site's manual is drawn (the contents down the side), under this site's strip; its links
    up to the repo's other documents go to this repo, not the FM-1's."""
    body = manual_html.on_the_bench(manual_html.convert((SRC / "docs" / "SMK37.md").read_text(encoding="utf-8")))
    tiles = re.findall(r'<a href="#s(\d+)">(.*?)</a>', body.split("</aside>")[0])
    strip = (DOCS_STRIP.replace("__N__", COUNTS[len(tiles)] if len(tiles) < len(COUNTS) else str(len(tiles)))
             .replace("__TILES__", "".join(f'<a href="#s{n}"><i>{int(n):02d}</i>{t}</a>' for n, t in tiles)))
    body, n = re.subn(r'<div class="strip">\n.*?</div>\n', lambda m: strip, body, count=1, flags=re.S)
    if n != 1 or "<aside" not in body or not tiles:
        raise SystemExit("docs/SMK37.md: manual_html.py no longer draws a strip and contents as this expects")
    # a path in a table may break after a slash, so the files table fits a phone
    body = re.sub(r"<td><code>([^<]*)</code>",
                  lambda m: "<td><code>" + m.group(1).replace("/", "/<wbr>") + "</code>", body)
    return body.replace("https://github.com/stephencummins/fm1-aisling/blob/main/", REPO + "/blob/main/")


def panel_svg():
    """The panel's drawing, inline, so the page's buttons can change its colours (tools/gen_smk37_figure.py)."""
    return (SRC / "docs" / "img" / "smk37-panel.svg").read_text(encoding="utf-8")


def skin_buttons():
    import gen_smk37_figure
    return "".join(f'<button type="button" value="{k}" aria-pressed="false">{name}</button>'
                   for k, (name, _) in gen_smk37_figure.SKINS.items())


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


def main(out, marks=False):
    out = Path(out)
    st = (lambda p, root, current: p) if marks else (lambda p, root, current: stamp(p, root, current, NAV, SITE))
    if out.exists():
        shutil.rmtree(out)
    (out / "img").mkdir(parents=True)
    (out / "controls").mkdir()
    (out / "manual").mkdir()
    shots(out)
    shutil.copy(SRC / "docs" / "card-smk37.png", out / "img" / "card-smk37.png")
    shutil.copy(SRC / "docs" / "img" / "smk37-panel.svg", out / "img" / "smk37-panel.svg")   # tools/gen_smk37_figure.py
    common = dict(HEAD_COMMON=HEAD_COMMON, STYLE=STYLE, BY=BY, FM1=FM1, REPO=REPO, SITE=SITE)
    (out / "index.html").write_text(st(page(LANDING, PANEL=panel_svg(), **common), "", None), encoding="utf-8")
    (out / "controls" / "index.html").write_text(
        st(page(CONTROLS, TABLE=controls_table(), PANEL=panel_svg(), SKINS=skin_buttons(), **common), "../", "controls"), encoding="utf-8")
    (out / "manual" / "index.html").write_text(
        st(page(DOCS, HEAD_COMMON=HEAD_COMMON, CSS=manual_html.CSS, BODY=docs_body()), "../", "manual"),
        encoding="utf-8")
    if marks:
        print(f"pages with their marks: {out}: index.html, controls/, manual/, img/")
        return
    shutil.copytree(SRC / "web" / "brand", out, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("_headers", "_redirects"))
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, controls/, manual/, img/ ({', '.join(SHOTS)} from the Elite's simulator)")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--marks"]
    if len(args) != 1:
        sys.exit(__doc__)
    main(args[0], "--marks" in sys.argv[1:])
