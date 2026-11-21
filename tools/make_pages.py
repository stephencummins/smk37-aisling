#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make the AISLING site, as NACHTFAHRT's is made. Its home is https://aisling.stephen8n.com/ (Cloudflare Pages); the same
files go to GitHub Pages too (https://stephencummins.github.io/fm1-aisling/), where the links already out there point
(tools/publish_pages.sh). Its links between pages are relative, and every page names the home as its canonical address.

  favicon.svg, favicon-32.png, apple-touch-icon.png, mark.png, site.css, wall.js, fonts/
                              the harp, the bar, the look and the fonts every page shares (web/brand, tools/site_brand.py)
  _headers, _redirects        Cloudflare's (web/brand); GitHub Pages serves them as two stray files
  404.html                    the page for an address the site does not have (web/brand); both hosts use it
  visualiser/                 the visualiser (web/visualiser)
  index.html                  the root: what AISLING is, then the three things it runs on (site_brand.PRODUCTS), each
                              drawn, with how far it has got and its pages; the bar's switch goes between them too
  fm1/index.html              the FM-1's page: what AISLING is, and the ways in: install, try, the visualiser; then the
                              help (manual, controls, cheat sheet, troubleshooting), the download and the source
  fm1/controls/index.html     the FM-1's controls drawn (docs/img/fm1-controls.svg, tools/gen_fm1_figure.py, in the
                              FM-1's colours, chosen on the page), then the README's "In short"
  fx1/index.html, controls/, manual/, install/
                              AISLING FX-1, the FM-1 as a USB effects box: its page, its controls drawn
                              (docs/img/fx1-controls.svg) with docs/FX1.md's table, docs/FX1.md as a manual, and its
                              installer: the FM-1's page and code, worded for FX-1, writing FX-1's release package
  smk37/                      the SMK-37 Elite's pages, from its own tree (tools/make_pages_smk37.py --marks DIR, on its
                              branch): given --smk37 DIR (or SMK37_PAGES=DIR), they are copied in and given this site's
                              bar; without, the bar and the root point at where they are published on their own
  img/                        screenshots (docs/img), the card (docs/card.png, tools/make_card.py) and the
                              visualiser's painting (web/visualiser/share.jpg, also the page's preview image)
  emu/                        AISLING in the browser (build/emu, from web/emu/build.sh), with its screen reader's
                              words and voice in emu/reader/ (fm1-reader, at the commit web/emu/reader.pin names)
  editor/                     the editor (web/editor/index.html; its code from build/editor, web/editor/build.sh)
  packs/                      the sound packs' page (web/packs) and index.json: every packs/*.json, made by tools/make_pack.py
  words/                      the screen reader's words files (web/words, from firmware/words.tsv)
  install/index.html          the web installer (web/amb_installer.html, with fm1pkg.js, fm1ota.js and
                              the package's metadata inlined; Chrome or Edge, Web MIDI)
  manual/index.html, img/     the manual (tools/manual_html.py, from docs/MANUAL.md)
  cheat/index.html            the cheat sheet: the flows, four drawings of the steps (web/cheat/), then the README's
                              "In short" (each control drawn as it looks) and pieces table; to print, those on one
                              sheet, then a page for each flow
  firmware/aisling-VERSION.fwsc   the package the installer writes; also the download
  firmware/aislingfx-1.1.fwsc     FX-1's, for fx1/install/: the release's asset, byte for byte (FX1_SHA256), from
                                  build/ or given with --fx1 FILE (or FX1_PKG=FILE)

  tools/make_pages.py build/aisling-0.1.fwsc 0.1 OUT_DIR [--smk37 DIR] [--fx1 FILE]

The package must be a release build (tools/mini_build.sh --release X.Y): its identity, FM-1_353XXYYZZ, is
what the installer checks the download against and what the FM-1 reports after the install. FX-1's is
FM-1_391XXYYZZ."""
import hashlib
import html
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
sys.path.insert(0, str(SRC / "tools"))
import site_brand  # noqa: E402
from site_brand import NAV, PRODUCTS, SITE, stamp  # noqa: E402
from make_site import product_of, strip_module  # noqa: E402  (Felucca's: the package format)

REPO = "https://github.com/stephencummins/fm1-aisling"
FX1_REPO, FX1_VERSION = "https://github.com/stephencummins/fx1-aisling", "1.1"           # its release
FX1_SHA256 = "41ae37d3d478737c73df32b87de1661d3cb7efd8883319f5b26c2baa011e0d1c"           # aislingfx-1.1.fwsc, its asset
SMK37_REPO, SMK37_VERSION = "https://github.com/stephencummins/smk37-aisling", "1.4.1"  # the Elite's
BY = 'by CÚ_STE · <a href="https://www.stephen8n.com">www.stephen8n.com</a>'

LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AISLING · Irish ambient for the FM-1</title>
<meta name="description" content="A generative Irish ambient instrument for the M-VAVE FM-1: harps and a flute over a held drone, in the old modes, with the weather of the west coast behind them. Let it play for hours, or play the flute with it on the keys.">
<meta property="og:title" content="AISLING · Irish ambient for the FM-1">
<meta property="og:description" content="A generative Irish ambient instrument for the M-VAVE FM-1. Install it from the browser, try it without one, and watch it paint an Irish landscape.">
<meta property="og:url" content="__SITE__fm1/">
<meta property="og:image" content="__SITE__img/visualiser.jpg">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta name="twitter:card" content="summary_large_image">
<style>
/* The FM-1's page's own: the layers' colours on the list (3:1 or more on the vellum), what's new in two columns.
   The rest is the site's (web/brand/site.css): its magazine parts, mg-* */
:root { --drone: #6a44b3; --seq: #b02f7c; --lead: #9a6400; --seq2: #1d6a94; --atmos: #7a6a54; }
.mg-pic.px { align-items: center; justify-content: center; background: #146c41; }
.mg-pic.px img { width: min(100%, 384px); height: auto; aspect-ratio: 1; }
.pane p, .mg-in p { margin: 0 0 10px; }
.pane p:last-child, .mg-in p:last-child { margin-bottom: 0; }
ul.new { list-style: square; padding-left: 1.2rem; margin: 0; columns: 2; column-gap: 32px; }
ul.new li { margin: 0 0 6px; break-inside: avoid; }
ul.new li::marker { color: var(--brand-green); }
ul.new b { font-weight: 400; color: var(--brand-green); }
ul.layers { list-style: square; padding-left: 1.2rem; margin: 0 0 10px; }
ul.layers li { margin: 0 0 10px; }
ul.layers li.d::marker { color: var(--drone); } ul.layers li.s::marker { color: var(--seq); }
ul.layers li.l::marker { color: var(--lead); } ul.layers li.t::marker { color: var(--seq2); }
ul.layers li.a::marker { color: var(--atmos); }
p.small { color: var(--brand-muted); font-size: 12.5px; }
.mg-in p b { font-weight: 400; color: var(--brand-green); }
@media (max-width: 47.99rem) { ul.new { columns: 1; } }
</style>
<!--AISLING-HEAD-->
</head>
<body>
<!--AISLING-BAR-->
<main class="bench">
<div class="strip">
<p class="lab"><b>aisling</b> · Irish ambient for the M-VAVE FM-1</p>
<p class="lab">v__VERSION__ · __BY__</p>
</div>

<div class="mg-cover">
<div class="mg-words">
<div>
<h1>Aisling</h1>
<p class="mg-gloss">Said ASH-ling: Irish for a dream or a vision, and a pun on <i>AI Synth Link</i>.</p>
</div>
<p class="mg-deck">Irish ambient for the FM-1. Harps and a flute over a held drone. Let it play for hours, or play the flute with it.</p>
</div>
<div class="mg-pic px"><img src="img/card.png" width="240" height="240" alt="A gold 8-bit harp on green"></div>
</div>

<nav class="mg-jump" aria-label="On this page">
<a href="#try"><i>01</i>Try it</a><a href="#controls"><i>02</i>Controls</a><a href="#install"><i>03</i>Install</a><a href="#visualiser"><i>04</i>Visualiser</a><a href="#voxbox"><i>05</i>Voxbox</a><a href="#cheat"><i>06</i>Cheat sheet</a>
</nav>

<section class="mg-feat" id="try" aria-labelledby="tryh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">01</div>
<div><p class="mg-kick">The emulator</p><h2 id="tryh">Try it in your browser</h2></div></div>
<div class="mg-split">
<div>
<p class="mg-stand">The same code the FM-1 runs, <em>with sound</em>. No FM-1 needed.</p>
<p class="mg-go"><a class="btn go big" href="emu/">Open the emulator</a></p>
<p class="mg-small">Mouse, touch or keyboard. Slide the visualiser up beneath it to watch what it plays painted.</p>
<p class="mg-small">New: on a phone it is <a href="emu/?app">an app</a>, full screen from the home screen. <a href="./#new">How</a>. Tested on an iPhone, in Safari and Chrome.</p>
</div>
<div class="mg-shots">
  <img src="img/screen-launch.png" width="240" height="240" alt="The launch screen: a green triskele, AISLING, by CU_STE, www.stephen8n.com">
  <img src="img/screen-main.png" width="240" height="240" alt="The main screen: the chord's name among orbiting dots, the five layers' meters, the two harps' lanes">
  <img src="img/screen-perform.png" width="240" height="240" alt="PERFORM at rest: a map of the 27 effects, a green triskele, and Filter, Wet, Energy and Chorus on the knobs">
</div>
</div>
</section>

<section class="mg-feat" id="controls" aria-labelledby="controlsh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">02</div>
<div><p class="mg-kick">The controls</p><h2 id="controlsh">Every knob, button and key</h2></div></div>
<a class="mg-draw" href="fm1/controls/" title="The controls page">__PANEL__</a>
<p class="mg-stand" style="margin-top:18px">The FM-1 drawn, each control labelled with what it does, <em>in your FM-1's colours</em>.</p>
<p class="mg-go" style="margin:0"><a class="btn go big" href="fm1/controls/">See the controls</a></p>
</section>

<section class="mg-feat" id="install" aria-labelledby="installh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">03</div>
<div><p class="mg-kick">Install</p><h2 id="installh">Put it on your FM-1</h2></div></div>
<ol class="mg-steps">
<li><b>Chrome or Edge,</b> on a computer.</li>
<li><b>The FM-1,</b> connected by a USB data cable.</li>
<li><b>Nothing to install:</b> the page does it from the browser.</li>
</ol>
<p class="mg-go"><a class="btn go big" href="install/">Install v__VERSION__</a></p>
<p class="mg-small"><a href="manual/#s13">If it goes wrong</a> · <a href="install/#stock">Back to stock</a> ·
<a href="firmware/__PKG__">Download __PKG__</a> · No FM-1 yet? <a href="https://store.stephen8n.com/#g-fm1">Where to buy one</a></p>
</section>

<div class="mg-three">
<section id="visualiser" aria-labelledby="vish">
<div class="mg-num" aria-hidden="true">04</div>
<a class="mg-tile" href="visualiser/" tabindex="-1"><img src="img/visualiser.jpg" width="1200" height="630" loading="lazy" alt="The visualiser: the FM-1's layers painted in heather and peat colours on vellum"></a>
<h2 id="vish">Visualiser</h2>
<p>The FM-1 paints an Irish landscape from its MIDI: the flute the sky, the harps the hills, the drone the bog. Demo paints without an FM-1.</p>
<p class="mg-go"><a class="btn go" href="visualiser/">Open the visualiser</a></p>
</section>
<section id="voxbox" aria-labelledby="voxboxh">
<div class="mg-num" aria-hidden="true">05</div>
<a class="mg-tile" href="screen/" tabindex="-1"><img src="screen/img/over-fm1.jpg" width="1280" height="720" loading="lazy" alt="A small touch screen on a stand above the FM-1, showing the landscape in mauve, pink and gold"></a>
<h2 id="voxboxh">Voxbox</h2>
<p>A touch screen beside the FM-1: the landscape, the FM-1's own screen bigger, its knobs under your finger. A kit you put together.</p>
<p class="mg-go"><a class="btn go" href="screen/">See the screen</a></p>
</section>
<section id="cheat" aria-labelledby="cheath">
<div class="mg-num" aria-hidden="true">06</div>
<a class="mg-tile type" href="cheat/" tabindex="-1" aria-hidden="true"><span><small>more what you'd call</small>Guidelines<small>than actual rules</small></span></a>
<h2 id="cheath">Cheat sheet</h2>
<p>The steps drawn as flows, then every control and the eight pieces, to print on one sheet.</p>
<p class="mg-go"><a class="btn go" href="cheat/">Open the cheat sheet</a></p>
</section>
</div>

<nav class="mg-also" aria-label="More"><span>also</span>
<a href="manual/">Manual</a><a href="editor/">Editor</a><a href="https://gear.stephen8n.com/">Gear</a><a href="__REPO__">Source</a>
</nav>

<section class="mg-fold" aria-label="More about AISLING">
<details><summary>New in v__VERSION__</summary><div class="mg-in" style="max-width:none">
__NEW__
</div></details>
<details><summary>Listen, and play <small>the white keys, the black keys, PERFORM</small></summary><div class="mg-in">
<p>AISLING plays itself from the moment it is switched on, and it never ends: each piece opens from the drone,
flows, gives the flute its turn, crosses a bridge and starts its long rise again. You are invited in. The <b>white
keys</b> play the flute in the piece's scale, so every note fits; play a phrase and it becomes the flute's theme. The
<b>black keys</b> are effects while held: Ceo (KYOH, fog), Macalla (MOCK-al-a, an echo), Fras (FRASS, a burst of rain), Rise and Close sweep
every filter, Transpose lifts the harps, and more. SELECT turns the level of the layer you are looking at; in PERFORM (SEL)
every key is an effect on the whole music while held, 27 of them, and the knobs push Filter, Wet, Energy and Chorus.</p>
</div></details>
<details><summary>What it plays <small>five layers, eight pieces</small></summary><div class="mg-in">
<ul class="layers">
  <li class="d"><b class="lead">Drone.</b> Organ, strings, a reed drone, glass or a choir whose vowel moves, with the root an octave below; its chords swing slowly between two tonics, as the old tunes do.</li>
  <li class="s"><b class="lead">Sequence.</b> The harp: a wire-strung harp's ostinato, often twelve notes in a jig's threes, repeated by the echo, its filter wandering.</li>
  <li class="t"><b class="lead">Sequence 2.</b> A second harp, a step shorter and often an octave down, so the two drift apart and meet again.</li>
  <li class="l"><b class="lead">Lead.</b> The flute, far back in the reverb, phrasing in calls and answers; or your own melody on the keys.</li>
  <li class="a"><b class="lead">Atmosphere.</b> Surf, wind, rain, a turf fire or a curlew, where a piece wants it.</li>
</ul>
<p>Eight pieces: Skellig, Burren, Aran, Carolan, Blasket, Samhain (SOW-in), Connemara and Tir na nOg (teer na NOHG), in
the old modes and the pipes' keys, each remembering how you left it. Every layer has its sound pages and a Move page, where its filter wanders
relative to the knob you set.</p>
</div></details>
</section>

<div class="pane">
<p class="small">AISLING is by CÚ_STE (<a href="https://www.stephen8n.com">www.stephen8n.com</a>). It is a fork of
<a href="https://github.com/FartyPants/fm1-nachtfahrt">NACHTFAHRT</a> by FartyPants, based on
<a href="https://github.com/sbegueria/fm1-dub">DUB-1</a> by sbegueria, itself based on
<a href="https://github.com/charlesvestal/fm1-amb">AMB-1</a> by Charles Vestal; its granular cloud follows JIANT's;
the platform is Felucca by Leo Kuroshita (Hügelton Instruments). GPL-3.0.
M-VAVE and FM-1 are trademarks of their owners. AISLING is not affiliated with M-VAVE, Hügelton Instruments or
NACHTFAHRT's author. Its faces are Barlow Semi Condensed, IBM Plex Mono and Uncial Antiqua (SIL OFL 1.1).</p>
</div>
</main>
</body>
</html>
"""

CHEAT = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AISLING cheat sheet</title>
<meta name="description" content="AISLING's steps drawn as flows (changing a sound, playing the effects, every button's pages), then every control and the eight pieces, to print on one sheet.">
<style>
/* On the site's bench on screen (web/brand/site.css); black on white to print, small enough for one sheet */
.wrap { overflow-x: auto; }
table { border-collapse: collapse; width: 100%; }
th, td { text-align: left; vertical-align: top; padding: 5px 8px 5px 0; border-top: 1px solid var(--brand-rule); }
th { font-weight: 400; font-size: 12.5px; color: var(--brand-muted); border-top: 0; }
td b, td i { font-weight: 400; font-style: normal; color: var(--brand-green); }
td:first-child { white-space: nowrap; }
.jump { line-height: 1.8; }
.vh { position: absolute; width: 1px; height: 1px; overflow: hidden; clip-path: inset(50%); white-space: nowrap; }
/* The controls, each drawn as the FM-1's panel draws it (docs/img/fm1-panel.svg), beside its job in short lines. In the
   jobs, a button's name is the button and the knobs' names come after a knob */
.fm-ctl { margin: 0; columns: 2 25rem; column-gap: 36px; }
.fm-c { break-inside: avoid; display: grid; grid-template-columns: 7.6rem minmax(0, 1fr); gap: 0 12px; padding: 9px 0 10px;
        border-top: 1px solid var(--brand-rule); }
.fm-c dt { padding-top: 1px; display: flex; flex-wrap: wrap; gap: 4px; align-content: flex-start; align-items: flex-start; }
.fm-c dd { margin: 0; font-size: 12.5px; line-height: 1.55; }
.fm-c p, .fm-pages { margin: 0; }
.fm-c dd > * + * { margin-top: 3px; }
.fm-c b { font: 600 15px/1.3 var(--brand-display); letter-spacing: .01em; color: var(--brand-ink); }
.fm-pages { padding: 0; list-style: none; counter-reset: fm-pg; }
.fm-pages li { counter-increment: fm-pg; display: grid; grid-template-columns: 15px minmax(0, 1fr); gap: 0 7px; }
.fm-pages li + li { margin-top: 2px; }
.fm-pages li::before { content: counter(fm-pg); width: 15px; height: 15px; margin-top: 2px; border-radius: 50%;
        background: var(--brand-green); color: #fff; font: 600 11px/15px var(--brand-display); text-align: center; }
.fm-kn, .fm-note { color: var(--brand-muted); }
.fm-kn { white-space: normal; }
.fm-kn::before, .fm-knob::before { content: ""; display: inline-block; box-sizing: border-box; width: 11px; height: 11px;
        margin: 0 6px 0 3px; vertical-align: -1px; border-radius: 50%; box-shadow: 0 0 0 1.5px #ab5636;
        background: linear-gradient(#fbf0e4, #fbf0e4) 50% 1px / 2px 4px no-repeat, #1c1614;
        -webkit-print-color-adjust: exact; print-color-adjust: exact; }
.fm-tag { display: inline-block; min-width: 4.2em; font: 600 11px/1 var(--brand-display); letter-spacing: .09em;
        text-transform: uppercase; color: var(--brand-muted); }
/* a button: rust, a darker edge, its name in cream capitals, as on the panel */
.fm-btn { display: inline-block; box-sizing: border-box; padding: .22em .5em .16em; border: 1.5px solid #5a2c23;
        border-radius: .45em; background: #8f4d40; color: #fbf0e4; box-shadow: inset 0 1px 0 rgb(255 255 255 / .14);
        font: 600 .78em/1.15 "Helvetica Neue", Helvetica, Arial, sans-serif; letter-spacing: .05em; text-align: center;
        white-space: nowrap; vertical-align: .06em; -webkit-print-color-adjust: exact; print-color-adjust: exact; }
.fm-btn + .fm-btn { margin-left: 4px; }
dt .fm-btn { min-width: 58px; padding: 6px 9px 5px; border-radius: 8px; font-size: 13px; }
dt .fm-btn + .fm-btn { margin-left: 0; }
dt .fm-btn:not(:only-child) { min-width: 0; padding-inline: 6px; }
.fm-key + .fm-tag { min-width: 0; margin-left: 2px; }
/* a key: white or black, square at the top, as the keybed's */
.fm-key { display: inline-block; box-sizing: border-box; padding: .4em .45em .14em; border: 1.5px solid #2b2219;
        border-radius: 0 0 .35em .35em; background: #fff; color: #2b2219;
        font: 600 .78em/1.15 "Helvetica Neue", Helvetica, Arial, sans-serif; letter-spacing: .04em; white-space: nowrap;
        vertical-align: .06em; -webkit-print-color-adjust: exact; print-color-adjust: exact; }
.fm-key.black { background: #1c1614; border-color: #1c1614; color: #fbf0e4; }
dt .fm-key { padding: 14px 9px 5px; font-size: 13px; }
/* a knob: black on a rust ring with a cream pointer, its name in capitals beside it */
.fm-knob { display: inline-flex; align-items: center; font: 600 13px/1.2 "Helvetica Neue", Helvetica, Arial, sans-serif;
        letter-spacing: .06em; color: #24110a; }
.fm-knob::before { width: 22px; height: 22px; margin: 2px 8px 2px 3px; box-shadow: 0 0 0 3px #ab5636;
        background-size: 3px 7px; background-position: 50% 2px; }
@media (max-width: 30rem) {
  .fm-c { grid-template-columns: minmax(0, 1fr); }
  .fm-c dt { margin-bottom: 6px; }
}
@media (min-width: 56.25rem) { .jump-hint { display: none; } }
/* The flows: four drawings, each drawn at 1123 by 794 (A4 landscape) and scaled to the bench by the script at the end of the
   page. They stop shrinking while they can still be read, and from there each scrolls sideways in its own box */
.flow { overflow-x: auto; overflow-y: hidden; }
.flow:focus-visible { outline: 2px solid var(--brand-ink); outline-offset: -2px; }
.flow-size { width: calc(1123px * var(--s, 1)); height: calc(794px * var(--s, 1)); }
.flow-board { position: relative; width: 1123px; height: 794px; overflow: hidden; background: #fff; color: #2b2219;
              font: 400 12px/16px var(--brand-mono); transform: scale(var(--s, 1)); transform-origin: 0 0;
              -webkit-print-color-adjust: exact; print-color-adjust: exact; }
__FLOWS_CSS__@media print {
  body.cheat { font-size: 7.5pt; line-height: 1.3; }
  body.cheat .pane { padding: 0; margin-top: .6rem; }
  body.cheat .bench > * + * { border-top: 0; }
  body.cheat .lab, body.cheat .bh { font-size: 7.5pt; }
  body.cheat .bh { margin-bottom: 2px; }
  th { font-size: 7.5pt; }
  th, td { padding: 2px 6px 2px 0; }
  tr { break-inside: avoid; }
  /* the sheet first, as before the flows were put at the top: the controls and the pieces, then a page each for them */
  body.cheat .bench { display: flex; flex-direction: column; }
  body.cheat .strip { order: -2; }
  body.cheat .ref { order: -1; }
  .fm-ctl { columns: 2; column-gap: 18px; }
  .fm-c { grid-template-columns: 5.4rem minmax(0, 1fr); gap: 0 8px; padding: 3px 0; }
  .fm-c dd { font-size: 7.5pt; line-height: 1.3; }
  .fm-c dd > * + *, .fm-pages li + li { margin-top: 0; }
  .fm-c b { font-size: 8.5pt; }
  .fm-tag { font-size: 6.5pt; }
  dt .fm-btn { min-width: 40px; padding: 3px 6px 2px; border-radius: 5px; font-size: 7.5pt; }
  dt .fm-btn:not(:only-child) { min-width: 0; padding-inline: 4px; }
  dt .fm-key { padding: 8px 6px 2px; font-size: 7.5pt; }
  .fm-knob { font-size: 7.5pt; }
  .fm-knob::before { width: 13px; height: 13px; margin: 2px 6px 2px 2px; box-shadow: 0 0 0 2px #ab5636; background-size: 2px 4px; background-position: 50% 1px; }
  @page { margin: 10mm; }
  /* after the sheet, a landscape page each. (A hair under A4, or each brings a blank page with it) */
  @page flow { size: A4 landscape; margin: 0; }
  body.cheat .jump { display: none; }
  body.cheat .flow { page: flow; break-before: page; overflow: visible; }
  .flow-size { width: auto; height: auto; }
  .flow-board { width: 1122px; height: 793px; transform: none; }
}
</style>
<!--AISLING-HEAD-->
</head>
<body class="cheat">
<!--AISLING-BAR-->
<main class="bench">
<div class="strip">
<h1 class="lab"><b>cheat sheet</b> · AISLING v__VERSION__</h1>
<p class="lab">__BY__</p>
</div>
<nav class="pane lab jump" aria-label="On this page">The steps, drawn: <a href="#flow-lead">change the Lead's instrument</a> · <a href="#flow-layers">any layer's sound</a> · <a href="#flow-perform">play the effects</a> · <a href="#flow-pages">every button's pages</a><span class="jump-hint"> (on a small screen, scroll each one sideways)</span>. Under them, in words: <a href="#controls">every control</a> · <a href="#pieces">the pieces</a></nav>
__FLOWS__
<section class="pane ref" id="controls">
<h2 class="bh">Every control</h2>
<dl class="fm-ctl">
__CONTROLS__
</dl>
</section>
<section class="pane ref" id="pieces">
<h2 class="bh">The pieces</h2>
<div class="wrap">__PIECES__</div>
</section>
</main>
<script>
// each flow to the bench's width, as far down as it can still be read
(function () {
  var flows = document.querySelectorAll(".flow");
  function fit() {
    flows.forEach(function (f) { f.style.setProperty("--s", Math.max(f.clientWidth / 1123, 0.8)); });
  }
  fit();
  addEventListener("resize", fit);
}());
</script>
</body>
</html>
"""


CHOOSER = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<script>{ const q = new URLSearchParams(location.search); if (['embed', 'clean', 'scene', 'ground', 'palette', 'demo'].some((k) => q.has(k))) location.replace('visualiser/' + location.search + location.hash) }</script>
<title>AISLING · Irish ambient for the FM-1 and the SMK-37</title>
<meta name="description" content="AISLING is a generative Irish ambient instrument: harps and a flute over a held drone, in the old modes. It runs on the M-VAVE FM-1 and the SMK-37 Elite, and AISLING FX-1 makes an FM-1 an effects box for a computer's sound.">
<meta property="og:title" content="AISLING · Irish ambient for the FM-1 and the SMK-37">
<meta property="og:description" content="A generative Irish ambient instrument for the M-VAVE FM-1 and the SMK-37 Elite, and AISLING FX-1, an FM-1 as a USB effects box.">
<meta property="og:url" content="__SITE__">
<meta property="og:image" content="__SITE__img/visualiser.jpg">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta name="twitter:card" content="summary_large_image">
<style>
/* The root's own: a cover, then the three things it runs on side by side, numbered and pictured (the drawings without
   their words: here they only tell them apart; FX-1's is a photo of an FM-1 running it), with how far each has got and
   its pages; then the emulator, the screen, the store. The rest is the site's (web/brand/site.css), its mg-* parts */
.mg-pic.px { position: relative; align-items: center; justify-content: center; background: #146c41; }
.mg-pic.px > img { width: min(100%, 384px); height: auto; aspect-ratio: 1; }
.ask { margin: 0; padding: 22px 22px 18px; font: 600 clamp(30px, 5vw, 58px)/.98 var(--brand-display); text-transform: uppercase; }
.ask small { display: block; margin: 0 0 6px; font: 400 12px/1.3 var(--brand-mono); letter-spacing: .12em; color: var(--brand-rust); }
.band { border-top: 1px solid var(--brand-rule); }
.band .mg-head { margin-bottom: 18px; }
#new .mg-num { font-size: clamp(56px, 8vw, 104px); text-transform: uppercase; }
#new .how { margin: 22px 0 10px; font: 600 22px/1.1 var(--brand-display); text-transform: uppercase; }
#new .mg-split { align-items: center; }
.band .shot { display: block; border: 1px solid var(--brand-rule); }
.band .shot img { display: block; width: 100%; height: auto; max-height: 340px; object-fit: cover; }
/* the tip cup, in the cover's corner: our own drawing. Steam all the time; every 14 seconds a sugar lump goes in and it wiggles */
.cup { position: absolute; top: 14px; right: 16px; display: block; width: 64px; text-align: center; text-decoration: none; color: #f3ead7;
       font: 400 11px/1.2 var(--brand-mono, monospace); }
.cup svg { display: block; width: 64px; height: 64px; overflow: visible; }
.cup:hover { color: #e9a23b; }
.cup .mug { transform-origin: 8px 14px; animation: cup-wiggle 14s ease-in-out infinite; }
.cup:hover .mug { animation: cup-wiggle 1.4s ease-in-out infinite; }
.cup .steam rect { animation: cup-steam 3.2s linear infinite; opacity: 0; }
.cup .steam rect:nth-child(2) { animation-delay: 1.1s; } .cup .steam rect:nth-child(3) { animation-delay: 2.1s; }
.cup .lump { animation: cup-lump 14s linear infinite; opacity: 0; }
.cup .plop { animation: cup-plop 14s linear infinite; opacity: 0; }
@keyframes cup-wiggle { 0%, 52%, 64%, 100% { transform: rotate(0); } 54% { transform: rotate(-7deg); } 57% { transform: rotate(6deg); }
                        60% { transform: rotate(-4deg); } 62% { transform: rotate(2deg); } }
@keyframes cup-steam { 0% { transform: translateY(1px); opacity: 0; } 25% { opacity: .75; } 100% { transform: translateY(-4px); opacity: 0; } }
@keyframes cup-lump { 0%, 44% { transform: translateY(-7px); opacity: 0; } 45% { transform: translateY(-7px); opacity: 1; }
                      51% { transform: translateY(0); opacity: 1; } 52%, 100% { transform: translateY(0); opacity: 0; } }
@keyframes cup-plop { 0%, 51% { transform: translateY(0); opacity: 0; } 52% { opacity: 1; } 55% { transform: translateY(-2px); opacity: 0; } 100% { opacity: 0; } }
@media (prefers-reduced-motion: reduce) { .cup .mug, .cup .lump, .cup .plop, .cup .steam rect { animation: none; } .cup .steam rect { opacity: .6; } }
.parts { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 1px; background: var(--brand-rule); }
.part { display: flex; flex-direction: column; min-width: 0; background: var(--brand-ground); }
.part > a { display: block; flex: 1; padding: 16px 16px 12px; color: var(--brand-ink); text-decoration: none; }
.part > a:hover { background: var(--brand-strip); color: var(--brand-ink); }
.part > a:focus-visible { outline-offset: -3px; }
.pic { height: 168px; margin: 0 0 14px; }
.pic svg { display: block; width: 100%; height: 100%; }
.pic svg text { display: none; }
.pic img { display: block; width: 100%; height: 100%; object-fit: contain; }
.part .mg-num { margin: 0 0 14px; font-size: clamp(60px, 7vw, 92px); -webkit-text-stroke-width: 1.5px; }
.part h2 { margin: 0 0 6px; font: 600 clamp(30px, 3.4vw, 42px)/1 var(--brand-display); letter-spacing: .04em; text-transform: uppercase; }
.part > a:hover h2 { color: var(--brand-green); }
.state { margin: 0 0 8px; font-size: 12px; letter-spacing: .03em; color: var(--brand-rust); }
.state.out { color: var(--brand-green); }
.does { margin: 0; font-size: 13px; }
.part ul { display: flex; flex-wrap: wrap; gap: 6px; list-style: none; margin: 0; padding: 0 16px 16px; }
p.small { margin: 0; color: var(--brand-muted); font-size: 12.5px; }
/* the store's skins, each on the case it suits best: the store's own pictures, in web/brand/skins/ */
.skins { display: grid; grid-template-columns: repeat(7, minmax(0, 1fr)); gap: 10px; list-style: none; margin: 14px 0 0; padding: 0; }
.skins a { display: block; text-decoration: none; color: inherit; }
.skins img { display: block; width: 100%; height: auto; aspect-ratio: 640 / 382; border-radius: 6px; background: var(--brand-line, #ddd);
  transition: transform .15s ease; }
.skins a:hover img, .skins a:focus-visible img { transform: translateY(-3px) rotate(-1deg); }
.skins span { display: block; margin-top: 6px; font: 400 12px/1.2 var(--brand-mono, monospace); color: var(--brand-muted); }
@media (max-width: 59.99rem) { .skins { grid-template-columns: repeat(4, minmax(0, 1fr)); } }
@media (max-width: 29.99rem) { .skins { grid-template-columns: repeat(2, minmax(0, 1fr)); } }
@media (prefers-reduced-motion: reduce) { .skins img { transition: none; } .skins a:hover img, .skins a:focus-visible img { transform: none; } }
/* narrower: one under another, the drawing beside the words; a phone: the drawing above them */
@media (max-width: 59.99rem) {
  .parts { grid-template-columns: minmax(0, 1fr); }
  .part > a { display: grid; grid-template-columns: minmax(0, 2fr) minmax(0, 3fr); column-gap: 20px; align-content: start; }
  .part > a > * { grid-column: 2; }
  .part > a > .pic { grid-column: 1; grid-row: 1 / span 4; height: 150px; margin: 0; align-self: center; }
}
@media (max-width: 29.99rem) {
  .part > a { display: block; }
  .part > a > .pic { height: 130px; margin: 0 0 12px; }
  .cup { width: 48px; top: 10px; right: 10px; } .cup svg { width: 48px; height: 48px; }
}
</style>
<!--AISLING-HEAD-->
</head>
<body>
<!--AISLING-BAR-->
<main class="bench">
<div class="strip">
<p class="lab"><b>aisling</b> · generative Irish ambient for M-VAVE's FM-1 and SMK-37 Elite</p>
<p class="lab">__BY__</p>
</div>

<div class="mg-cover">
<div class="mg-words">
<div>
<h1>Aisling</h1>
<p class="mg-gloss">Said ASH-ling: Irish for a dream or a vision.</p>
</div>
<p class="mg-deck">Harps and a flute over a held drone, playing themselves, after the harpers and the west of Ireland.</p>
</div>
<div class="mg-pic px"><img src="img/card.png" width="240" height="240" alt="A gold 8-bit harp on green">
<a class="cup" href="https://ko-fi.com/stejcu" title="Love my firmware? Tip me on Ko-fi">
<svg viewBox="0 0 16 16" shape-rendering="crispEdges" aria-hidden="true">
<g class="steam" fill="#5b4a3a"><rect x="5" y="2" width="1" height="2"/><rect x="7" y="1" width="1" height="2"/><rect x="9" y="2" width="1" height="2"/></g>
<rect class="lump" x="7" y="4" width="2" height="2" fill="#fff" stroke="#2b2219" stroke-width=".4"/>
<g class="plop" fill="#8c3510"><rect x="5" y="5" width="1" height="1"/><rect x="10" y="5" width="1" height="1"/></g>
<g class="mug">
<rect x="2" y="14" width="11" height="1" fill="#2b2219"/><rect x="3" y="13" width="9" height="1" fill="#d9ccb1"/>
<rect x="3" y="6" width="9" height="7" fill="#2b2219"/><rect x="4" y="7" width="7" height="5" fill="#fff8ea"/>
<rect x="4" y="7" width="7" height="1" fill="#8c3510"/><rect x="4" y="10" width="7" height="1" fill="#146c41"/>
<rect x="12" y="7" width="2" height="1" fill="#2b2219"/><rect x="14" y="8" width="1" height="2" fill="#2b2219"/><rect x="12" y="10" width="2" height="1" fill="#2b2219"/>
</g></svg>tip me</a>
</div>
</div>

<section class="mg-feat band" id="new" aria-labelledby="newh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">New</div>
<div><p class="mg-kick">What's new · November 2026</p><h2 id="newh">Aisling, as an app on your phone</h2></div></div>
<div class="mg-split">
<div>
<p class="mg-stand">The whole instrument, full screen, <em>from your home screen</em>. It plays with no connection, and it can wear any of the store's skins.</p>
<p class="mg-go"><a class="btn go big" href="emu/?app">Open the app</a></p>
<p class="mg-small"><b>Tested on an iPhone, in Safari and Chrome.</b> It may well work on an iPad or an Android phone; nobody has tried it there yet.</p>
</div>
<a class="shot" href="emu/?app" tabindex="-1"><img src="img/app-phone.webp" width="1688" height="780" loading="lazy" alt="AISLING filling a phone's screen, turned on its side: the FM-1 drawn with a map skin on it, its screen playing"></a>
</div>
<h3 class="how">An icon on your home screen</h3>
<ol class="mg-steps">
<li><b>Open the app in Safari</b> on your iPhone, with the button above.</li>
<li><b>Tap Share,</b> the square with an arrow pointing up. On a newer iPhone it may be under the three dots.</li>
<li><b>Choose Add to Home Screen,</b> then Add. You may need to scroll down the list to find it.</li>
<li><b>Open AISLING from its icon.</b> It fills the screen. Tap the FM-1 once to start the sound.</li>
</ol>
<p class="mg-small">It sounds even with the phone's silent switch on. For a skin, tap the menu button in the corner and choose one under Decal. Once it has been opened from its icon, it works with no connection.</p>
</section>

<h2 class="ask band"><small>three firmwares</small>Which have you got?</h2>
<div class="parts">
__PARTS__
</div>

<section class="mg-feat band" aria-labelledby="tryh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">04</div>
<div><p class="mg-kick">None of them?</p><h2 id="tryh">Try it in your browser</h2></div></div>
<div class="mg-split">
<div>
<p class="mg-stand">The same code the FM-1 runs, <em>with sound</em>. No FM-1 needed.</p>
<p class="mg-go" style="margin:0"><a class="btn go big" href="emu/">Open the emulator</a></p>
</div>
<div class="mg-shots">
  <img src="img/screen-launch.png" width="240" height="240" loading="lazy" alt="The launch screen: a green triskele, AISLING, by CU_STE, www.stephen8n.com">
  <img src="img/screen-main.png" width="240" height="240" loading="lazy" alt="The main screen: the chord's name among orbiting dots, the five layers' meters, the two harps' lanes">
  <img src="img/screen-perform.png" width="240" height="240" loading="lazy" alt="PERFORM at rest: a map of the 27 effects, a green triskele, and Filter, Wet, Energy and Chorus on the knobs">
</div>
</div>
</section>

<section class="mg-feat band" aria-labelledby="voxboxh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">05</div>
<div><p class="mg-kick">The add-on screen</p><h2 id="voxboxh">Voxbox</h2></div></div>
<div class="mg-split">
<div>
<p class="mg-stand">A touch screen for the FM-1 that <em>paints what it plays</em>. A kit you put together.</p>
<p class="mg-go" style="margin:0"><a class="btn go big" href="screen/">See the screen</a></p>
</div>
<a class="shot" href="screen/" tabindex="-1"><img src="screen/img/over-fm1.jpg" width="1280" height="720" loading="lazy" alt="A small touch screen on a stand above the FM-1, showing the landscape in mauve, pink and gold"></a>
</div>
</section>

<section class="mg-feat band store" aria-labelledby="storeh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">06</div>
<div><p class="mg-kick">The store · coming soon</p><h2 id="storeh">Skins for the FM-1</h2></div></div>
<p class="mg-stand">Vinyl skins, printed and cut by hand, <em>for any case colour and any firmware</em>, with the gear that goes with it.</p>
<ul class="skins">
<li><a href="https://store.stephen8n.com/#p-swatch"><img src="skins/swatch.webp" width="640" height="382" loading="lazy" alt="The Swatch skin on a cool grey FM-1"><span>Swatch</span></a></li>
<li><a href="https://store.stephen8n.com/#p-flightdeck"><img src="skins/flightdeck.webp" width="640" height="382" loading="lazy" alt="The Flight Deck skin on a cool grey FM-1"><span>Flight Deck</span></a></li>
<li><a href="https://store.stephen8n.com/#p-kinetic"><img src="skins/kinetic.webp" width="640" height="382" loading="lazy" alt="The Kinetic skin on a cool grey FM-1"><span>Kinetic</span></a></li>
<li><a href="https://store.stephen8n.com/#p-map"><img src="skins/map.webp" width="640" height="382" loading="lazy" alt="The Map skin on a white-blue FM-1"><span>Map</span></a></li>
<li><a href="https://store.stephen8n.com/#p-neonrain"><img src="skins/neonrain.webp" width="640" height="382" loading="lazy" alt="The Neon Rain skin on a black FM-1"><span>Neon Rain</span></a></li>
<li><a href="https://store.stephen8n.com/#p-synthwave"><img src="skins/synthwave.webp" width="640" height="382" loading="lazy" alt="The Synthwave skin on a black FM-1"><span>Synthwave</span></a></li>
<li><a href="https://store.stephen8n.com/#p-ticker"><img src="skins/ticker.webp" width="640" height="382" loading="lazy" alt="The Ticker skin on a black FM-1"><span>Ticker</span></a></li>
</ul>
<p class="mg-go" style="margin:18px 0 8px"><a class="btn go big" href="https://store.stephen8n.com/">Look around the store</a></p>
<p class="mg-small">The test prints come first; checkout isn't open yet.</p>
</section>
<div class="pane tip">
<p>Love my firmware? Support its development by <a href="https://ko-fi.com/stejcu">tipping me on Ko-fi</a>. :)</p>
</div>
<div class="pane">
<p class="small">AISLING is by CÚ_STE (<a href="https://www.stephen8n.com">www.stephen8n.com</a>), a fork of
<a href="https://github.com/FartyPants/fm1-nachtfahrt">NACHTFAHRT</a> by FartyPants, by way of
<a href="https://github.com/sbegueria/fm1-dub">DUB-1</a> and <a href="https://github.com/charlesvestal/fm1-amb">AMB-1</a>;
GPL-3.0. Each part's page has the rest. M-VAVE, FM-1 and SMK-37 are trademarks of their owners; AISLING is not
affiliated with M-VAVE.</p>
</div>
</main>
</body>
</html>
"""

# a part on the root: its key, the state it's in (and whether that's out), what it does, its links (label, where, go?)
PART_CARDS = (
    ("fm1", "v__VERSION__ · released", True,
     "The instrument: eight pieces that play themselves for hours, the flute on the white keys, the effects on the black "
     "ones. Install it from Chrome or Edge, or try it here without an FM-1.",
     (("install", "install/", True), ("try it", "emu/", False), ("manual", "manual/", False), ("controls", "fm1/controls/", False))),
    ("fx1", "v__FX1__ · released", True,
     "An FM-1 as an effects box: a computer's sound in over USB, through AISLING's filter, echo and reverb and its "
     "effects, out of the FM-1's socket and back to the computer.",
     (("install", "fx1/install/", True), ("download", FX1_REPO + "/releases/tag/v__FX1__", False),
      ("manual", "fx1/manual/", False), ("controls", "fx1/controls/", False))),
    ("smk37", "v__SMK37__ · released · Elite only", True,
     "The same AISLING on M-VAVE's 37-key keyboard: the FM-1's 27 keys in the middle, its pads playing the effects, its "
     "faders the mix.",
     (("download", SMK37_REPO + "/releases/tag/v__SMK37__", True), ("controls", "smk37/controls/", False),
      ("manual", "smk37/manual/", False))),
)

# the controls pages: an FM-1 drawn, in its colours, then a table of the controls
CONTROLS = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>__TITLE__</title>
<meta name="description" content="__DESC__">
<style>
/* The controls page's own (as the SMK-37 Elite's): the drawing, which scrolls sideways rather than shrink past reading,
   its colours, and the table. The rest is the site's (web/brand/site.css) */
.wrap { overflow-x: auto; }
svg.fmp { display: block; width: 100%; min-width: 900px; height: auto; }
/* the headline over the drawing */
.ct-head { display: grid; grid-template-columns: auto minmax(0, 1fr); align-items: end; gap: 6px 26px; }
.ct-head h1 { margin: 0; font: 700 clamp(40px, 7.4vw, 84px)/.86 var(--brand-display); letter-spacing: .01em; text-transform: uppercase; color: var(--brand-ink); }
.ct-head h1 span { color: var(--brand-rust); }
.pane.ct-head p { margin: 0 0 3px; max-width: 36ch; font: 600 clamp(16px, 1.8vw, 20px)/1.3 var(--brand-display); letter-spacing: .01em; }
@media (max-width: 47.99rem) { .ct-head { grid-template-columns: minmax(0, 1fr); } }
.skins { margin: 0 0 10px; }
.skins button { font: inherit; margin-left: 6px; padding: 2px 10px; border: 1px solid var(--brand-rule); border-radius: 0;
                background: transparent; color: inherit; cursor: pointer; }
.skins button[aria-pressed="true"] { border-color: var(--brand-green); box-shadow: inset 0 0 0 1px var(--brand-green); }
.notice { border-left: 4px solid var(--brand-rust); background: var(--brand-strip); }
.notice strong { font: 600 16px/1.3 var(--brand-display); letter-spacing: .02em; color: var(--brand-rust); }
.pane p { margin: 0 0 10px; }
.pane p:last-child { margin-bottom: 0; }
p.small { color: var(--brand-muted); font-size: 12.5px; }
table { border-collapse: collapse; width: 100%; }
th, td { text-align: left; vertical-align: top; padding: 6px 12px 6px 0; border-top: 1px solid var(--brand-rule); }
thead th { font-weight: 400; font-size: 12.5px; color: var(--brand-muted); border-top: 0; }
td:first-child { font: 600 15px/1.4 var(--brand-display); letter-spacing: .02em; white-space: nowrap; }
td b, td i { font-weight: 400; font-style: normal; color: var(--brand-green); }
thead:has(th:empty) { display: none; }
@media (max-width: 47.99rem) {
  table, tbody, tr, td { display: block; }
  thead { display: none; }
  tr { padding: 6px 0; border-top: 1px solid var(--brand-rule); }
  td { padding: 0; border-top: 0; }
  td:first-child { white-space: normal; }
}
</style>
<!--AISLING-HEAD-->
</head>
<body>
<!--AISLING-BAR-->
<main class="bench">
<div class="strip">
<p class="lab"><b>controls</b> · __STRIP__</p>
<a class="btn" href="__BUTTON_HREF__">__BUTTON__</a>
</div>
<div class="pane ct-head">
<h1>The <span>controls</span></h1>
<p>Every knob, button and key, drawn. The table under the drawing says the same in words.</p>
</div>
__NOTICE__<div class="pane wrap">
<p class="skins" id="skins" hidden>Your FM-1's colours: __SKINS__</p>
__PANEL__
<p class="small"><a href="__SVG__">The drawing alone</a>, at full size.</p>
<script>
(function () {
  var svg = document.querySelector("svg.fmp"), bar = document.getElementById("skins"), key = "aisling-fm1-skin";
  function set(k) {
    svg.setAttribute("data-skin", k);
    bar.querySelectorAll("button").forEach(function (b) { b.setAttribute("aria-pressed", b.value === k ? "true" : "false"); });
  }
  var k = "orange";
  try { k = localStorage.getItem(key) || k; } catch (e) {}
  bar.hidden = false;
  set(bar.querySelector('button[value="' + k + '"]') ? k : "orange");
  bar.addEventListener("click", function (e) {
    if (e.target.tagName !== "BUTTON") return;
    set(e.target.value);
    try { localStorage.setItem(key, e.target.value); } catch (x) {}
  });
})();
</script>
</div>
<section class="pane wrap" aria-labelledby="shorth">
<h2 class="bh" id="shorth">__TABLE_HEAD__</h2>
__TABLE__
</section>
<div class="pane">
<p>__AFTER__</p>
</div>
</main>
</body>
</html>
"""

FM1_CONTROLS = dict(
    title="AISLING on the FM-1 · the controls",
    desc="The M-VAVE FM-1 running AISLING, drawn: every knob, button and key labelled with what it does, in the FM-1's colours, then all of it in short.",
    strip="AISLING on the M-VAVE FM-1", button=("../../cheat/", "cheat sheet"), notice="",
    svg="fm1-controls.svg", table_head="In short", table=("README.md", "In short"),
    after='The <a href="../../manual/">manual</a> has the rest; the <a href="../../cheat/">cheat sheet</a> draws the steps '
          'as flows, then this table with each control as it looks, to print on one sheet.')
FX1_CONTROLS = dict(
    title="AISLING FX-1 · the controls",
    desc="An M-VAVE FM-1 running AISLING FX-1, the USB effects box, drawn: what each knob, button and key does to a computer's sound.",
    strip="AISLING FX-1, an FM-1 as a USB effects box", button=("../manual/", "manual"),
    notice='<div class="pane notice" role="note">\n<p><strong>1.1.</strong> What is drawn here is what the firmware '
           'does, measured in AISLING\'s simulator; the <a href="../manual/">manual</a> says which of it has been tried on '
           'the FM-1 itself.</p>\n</div>\n',
    svg="fx1-controls.svg", table_head="The controls", table=("docs/FX1.md", "The controls"), manual="../manual/",
    after='The <a href="../manual/">manual</a> has the rest: setting it up with a computer, the screen, and going back to '
          'AISLING.')

FX1_LANDING = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AISLING FX-1 · an FM-1 as a USB effects box</title>
<meta name="description" content="AISLING FX-1 makes an M-VAVE FM-1 an effects box for a computer: its sound in over USB, through AISLING's filter, echo, reverb and effects, out of the FM-1's socket. A guitar goes through it by way of the computer. 1.1, released.">
<meta property="og:title" content="AISLING FX-1 · an FM-1 as a USB effects box">
<meta property="og:description" content="An M-VAVE FM-1 as an effects box for a computer's sound, over USB. 1.1, released.">
<meta property="og:url" content="__SITE__fx1/">
<style>
/* FX-1's page's own. The rest is the site's (web/brand/site.css): its magazine parts, mg-* */
.pane p, .mg-in p { margin: 0 0 10px; }
.pane p:last-child, .mg-in p:last-child { margin-bottom: 0; }
.mg-in p b, .mg-steps li code { font-weight: 400; }
.mg-in strong { font: 600 16px/1.3 var(--brand-display); letter-spacing: .02em; color: var(--brand-rust); }
.mg-steps li code { overflow-wrap: anywhere; }
ul.plain { list-style: square; padding-left: 1.2rem; margin: 0 0 10px; }
ul.plain li { margin: 0 0 6px; }
ul.plain li::marker { color: var(--brand-green); }
p.small { color: var(--brand-muted); font-size: 12.5px; }
/* the guitar's way through: boxes in a row with the lead between each named, a column on a phone */
.chain { list-style: none; display: flex; flex-wrap: wrap; align-items: stretch; gap: 0; margin: 0 0 20px; padding: 0; }
.chain li { flex: 1 1 120px; display: flex; flex-direction: column; justify-content: center; padding: 12px 12px 13px; border: 1px solid var(--brand-rule);
            background: var(--brand-ground); font-size: 12.5px; line-height: 1.4; }
.chain li b { display: block; margin: 0 0 3px; font: 600 19px/1.15 var(--brand-display); letter-spacing: .02em; }
.chain li.fx { background: var(--brand-green); border-color: var(--brand-green); color: var(--brand-ground); }
.chain li.by { flex: 0 0 auto; justify-content: center; align-items: center; padding: 0 8px; border: 0; background: none; color: var(--brand-muted);
               font: 400 11px/1.3 var(--brand-mono); text-align: center; }
.chain li.by::after { content: "→"; font-size: 18px; line-height: 1; color: var(--brand-rust); }
@media (max-width: 47.99rem) {
  .chain { flex-direction: column; }
  .chain li { flex: 0 0 auto; }
  .chain li.by { flex-direction: row; gap: 8px; padding: 6px 0; }
  .chain li.by::after { content: "↓"; }
}
</style>
<!--AISLING-HEAD-->
</head>
<body>
<!--AISLING-BAR-->
<main class="bench">
<div class="strip">
<p class="lab"><b>aisling fx-1</b> · an M-VAVE FM-1 as a USB effects box</p>
<p class="lab">v1.1 · __BY__</p>
</div>

<div class="mg-cover">
<div class="mg-words">
<div>
<h1>Aisling <span>FX-1</span></h1>
<p class="mg-gloss">AISLING with its voices taken out and a computer's sound let in.</p>
</div>
<p class="mg-deck">An FM-1 as a USB effects box: a guitar, or anything your computer plays, in over the cable and out through AISLING's filter, echo and reverb.</p>
</div>
<div class="mg-pic"><img src="../img/fx1-photo.webp" width="984" height="590" alt="An FM-1 in a printed skin, running AISLING FX-1"></div>
</div>

<nav class="mg-jump" aria-label="On this page">
<a href="#guitar"><i>01</i>Guitar</a><a href="#controls"><i>02</i>Controls</a><a href="#install"><i>03</i>Install</a><a href="#manual"><i>04</i>Manual</a>
</nav>

<section class="mg-feat" id="guitar" aria-labelledby="guitarh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">01</div>
<div><p class="mg-kick">What it is for</p><h2 id="guitarh">A guitar through the FM-1</h2></div></div>
<p class="mg-stand">Plug a guitar into the computer, and the computer into the FM-1: you play through <em>the Filter, the Echo and the Reverb</em>, with a hand free for the knobs.</p>
<ol class="chain" aria-label="The guitar's way through">
  <li><b>Guitar</b>or a bass, or a microphone</li>
  <li class="by">guitar lead</li>
  <li><b>Interface</b>an iRig, or any sound interface with a guitar input</li>
  <li class="by">its lead</li>
  <li><b>Computer</b>music software, with the track's monitor on</li>
  <li class="by">USB</li>
  <li class="fx"><b>FM-1</b>running FX-1: the knobs and keys are the effects</li>
  <li class="by">3.5&nbsp;mm</li>
  <li><b>Headphones</b>or a speaker or an amp</li>
</ol>
<ol class="mg-steps">
  <li>Plug the guitar into the interface and the interface into the computer. The guitar never goes into the FM-1:
      <b>its socket is an output only</b>.</li>
  <li>In your music software, choose the interface as the sound input and <b>AISLING FX-1</b> as the sound output, at
      44.1&nbsp;kHz, with the smallest buffer that plays cleanly (64 samples here).</li>
  <li>Make an audio track that listens to the guitar and turn its monitor on (in Ableton Live, Monitor: <b>In</b>).</li>
  <li>Listen at the FM-1's socket and play. Turn the Echo and the Reverb up at HOME; in PERFORM (SEL) hold a key for
      an effect.</li>
</ol>
<p class="mg-small">Tried by its author: a guitar through an iRig 2, and through the first iRig, into a MacBook's headphone
socket, Ableton Live 12 on macOS 27, out to the FM-1. The sound goes through the computer, so there is a short delay
between the string and the speaker; it has not been measured. A bass and a microphone go the same way and have not been tried.</p>
</section>

<section class="mg-feat" id="controls" aria-labelledby="controlsh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">02</div>
<div><p class="mg-kick">The controls</p><h2 id="controlsh">Every knob, button and key</h2></div></div>
<a class="mg-draw" href="controls/" title="The controls page">__PANEL__</a>
<p class="mg-stand" style="margin-top:18px">At HOME the knobs are <em>the Filter, the Echo, the Reverb and the Dry</em>. In PERFORM the keys are effects while held.</p>
<p class="mg-go" style="margin:0"><a class="btn go big" href="controls/">See the controls</a></p>
</section>

<section class="mg-feat" id="install" aria-labelledby="installh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">03</div>
<div><p class="mg-kick">Install</p><h2 id="installh">Put it on your FM-1</h2></div></div>
<ol class="mg-steps">
  <li>Put FX-1 on the FM-1 from its <a href="install/">install page</a>, in Chrome or Edge, over AISLING, another
      firmware or the stock one (or download <code>aislingfx-1.1.fwsc</code> from the release and install it with
      <code>tools/fm1_install.py</code>).</li>
  <li>Connect the FM-1 to the computer with a USB data cable. It shows up as a sound device, <b>AISLING FX-1</b>.</li>
  <li>Make it the computer's sound output, or a track's in your music software, and play something. The screen says
      PLAYING and its meters move.</li>
  <li>Listen at the FM-1's socket. At HOME the knobs are the Filter, the Echo, the Reverb and the Dry; in PERFORM (SEL)
      the keys are effects while held.</li>
  <li>To record what comes out, choose the FM-1 as the computer's input as well.</li>
</ol>
<p class="mg-go"><a class="btn go big" href="install/">Install v1.1</a></p>
<p class="mg-small"><a href="__FX1_REPO__/releases/tag/v1.1">Download 1.1</a> · Going back: <a href="../install/">install AISLING</a>, or the stock firmware.</p>
</section>

<section class="mg-feat" id="manual" aria-labelledby="manualh">
<div class="mg-head"><div class="mg-num" aria-hidden="true">04</div>
<div><p class="mg-kick">The manual</p><h2 id="manualh">Setting it up</h2></div></div>
<div class="mg-split">
<div>
<p class="mg-stand">Choose <em>AISLING FX-1</em> as the computer's sound output. It comes out of the FM-1's 3.5&nbsp;mm socket, and goes back up the cable to record.</p>
<p class="mg-go" style="margin:0"><a class="btn go big" href="manual/">Read the manual</a></p>
</div>
__SHOTS__
</div>
</section>

<section class="mg-fold" aria-label="More about AISLING FX-1">
<details><summary>1.1, released <small>where it has run</small></summary><div class="mg-in">
<p><strong>1.1, released.</strong> It is 1.0 with a light screen to choose (the Journey page's Screen) and the knob
strip tidied; the sound is 1.0's. It installs from Chrome or Edge on its <a href="install/">install page</a>, and its
package, <code>aislingfx-1.1.fwsc</code>, is on <a href="__FX1_REPO__/releases/tag/v1.1">the 1.1 release</a> for the
command-line installer. 1.0 and 1.1 have each gone on one FM-1 from the command line (1.1's screen looked right, and the computer's
sound played through its effects). The install page has not been used with FX-1: it does the same as AISLING's. The way back is the usual one: <a href="../install/">install AISLING</a>, or the stock firmware.</p>
</div></details>
<details><summary>Tried, and not yet <small>what works, what's untested</small></summary><div class="mg-in">
<ul class="plain">
  <li>On the FM-1: the sound plays with no gaps and no packets lost, it sounds right, the screen keeps up, and turning
      the Dry down leaves the effects' share as it was. It went on over AISLING and over the stock firmware, and
      AISLING went back over it with its pieces as they were.</li>
  <li>In the simulator, every control and key measured: 17 of PERFORM's 27 effects change a computer's sound, and four
      of the black keys' eleven (the rest were made for AISLING's own voices). The manual and the controls drawing
      say which.</li>
  <li>Not yet: how late the sound comes out (the latency), a long session on the FM-1, a newer FM-1, and putting FX-1
      on from the install page.</li>
  <li>To know: FX-1 doesn't save by itself (hold GLO to save), and saves where AISLING does, as AISLING's piece;
      Setup's sleep timer, if set, quiets the socket while the computer still plays.</li>
</ul>
<p class="small">The <a href="manual/">manual</a> keeps this list as it changes.</p>
</div></details>
</section>

<div class="pane">
<p class="small">AISLING FX-1 is AISLING's own code with a second USB sound stream, the one coming in, and the voices
left out; everything else is AISLING's, a fork of <a href="https://github.com/FartyPants/fm1-nachtfahrt">NACHTFAHRT</a>
by FartyPants by way of <a href="https://github.com/sbegueria/fm1-dub">DUB-1</a> and
<a href="https://github.com/charlesvestal/fm1-amb">AMB-1</a>, on Felucca by Leo Kuroshita (Hügelton Instruments).
GPL-3.0. M-VAVE and FM-1 are trademarks of their owners; AISLING is not affiliated with M-VAVE.</p>
</div>
</main>
</body>
</html>
"""

def md_rows(section, path="README.md"):
    """The first table under a markdown file's "## SECTION" (or "## N. SECTION"): its rows, each a list of its cells as
    written (markdown); the first is the head, the second the rule."""
    lines = (SRC / path).read_text(encoding="utf-8").splitlines()
    heads = [i for i, line in enumerate(lines) if re.fullmatch(r"## (\d+\. )?" + re.escape(section), line)]
    if len(heads) != 1:
        raise SystemExit(f"{path}: {len(heads)} headings ## {section}")
    i = heads[0]
    rows = []
    for line in lines[i + 1:]:
        if line.startswith("|"):
            rows.append([c.strip() for c in line.strip().strip("|").split("|")])
        elif rows:
            break
    if len(rows) < 3:
        raise SystemExit(f"{path}: no table under ## {section}")
    return rows


def md_table(section, path="README.md", manual=None):
    """The first table under a markdown file's "## SECTION", as HTML (the cells' **bold**, *italics* and `code` kept, and
    their links: one to a section of the same file, [x](#N-slug), goes to MANUAL#sN, where that file is the manual)."""
    rows = md_rows(section, path)

    def link(m):
        text, to = m.groups()
        if to.startswith("#"):
            if manual is None:
                raise SystemExit(f"{path}: {section}: a link to {to} with no manual for it to go to")
            to = manual + "#s" + to[1:].split("-")[0]
        return f'<a href="{html.escape(to)}">{text}</a>'

    def cell(c):
        c = html.escape(c, quote=False)
        c = re.sub(r"`([^`]+)`", r"<code>\1</code>", c)
        c = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", link, c)
        return re.sub(r"\*([^*]+)\*", r"<i>\1</i>", re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", c))
    head = "".join(f"<th scope=\"col\">{cell(c)}</th>" for c in rows[0])
    body = "".join("<tr>" + "".join(f"<td>{cell(c)}</td>" for c in r) + "</tr>\n" for r in rows[2:])
    return f"<table>\n<thead><tr>{head}</tr></thead>\n<tbody>\n{body}</tbody>\n</table>"


# The FM-1's controls as its panel draws them (docs/img/fm1-panel.svg), for the cheat sheet to draw each one as it looks
BUTTONS = ("HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "OCT−", "OCT+")
KNOBS = ("MASTER", "SELECT", "PRESETS", "ALGORITHM")
BUTTON_NAME = re.compile(r"(?<![\w−+])(" + "|".join(re.escape(b) for b in BUTTONS) + r")(?![\w−+])")


def split_top(text, sep):
    """TEXT cut at each SEP outside brackets."""
    parts, depth, start, i = [], 0, 0, 0
    while i < len(text):
        if text[i] in "()":
            depth += 1 if text[i] == "(" else -1
        elif depth == 0 and text.startswith(sep, i):
            parts.append(text[start:i])
            start = i = i + len(sep)
            continue
        i += 1
    return parts + [text[start:]]


def drawn(text):
    """TEXT as HTML, the buttons it names drawn as buttons, and "white 1–5" or "black 6" as the keys."""
    t = html.escape(text, quote=False)
    t = re.sub(r"\b(white|black) (\d+(?:–\d+)?)\b", r'<span class="fm-key \1"><span class="vh">\1 </span>\2</span>', t)
    return BUTTON_NAME.sub(r'<kbd class="fm-btn">\1</kbd>', t)


def control(name):
    """A control's name in the README's "In short", drawn as the control looks: a button, a knob, the keys."""
    if name in BUTTONS:
        return f'<kbd class="fm-btn">{html.escape(name)}</kbd>'
    if name in KNOBS:
        return f'<span class="fm-knob">{name}</span>'
    if re.fullmatch(r"(White|Black) keys", name):
        return f'<span class="fm-key {name.split()[0].lower()}">{name}</span>'
    if " / " in name:
        return " ".join(control(n) for n in name.split(" / "))
    raise SystemExit(f"README.md ## In short: no drawing for the control {name!r}")


def job(text, row):
    """A control's job, from its cell in the README's "In short", in short lines: the pages a button steps through,
    numbered, each with its knobs; what a tap does and what holding it does, each on its own line; or the cell's own list,
    a line each. ROW names the row when the cell can't be read."""
    def cap(t):
        return t if re.match(r"(white|black) \d", t) else t[:1].upper() + t[1:]

    def knobs(t):
        return f'<span class="fm-kn">{drawn(t)}</span>'

    def page(p):
        m = re.fullmatch(r"(.+?) \(([^()]+)\)", p)
        if not m:
            return f"<li><span><b>{drawn(cap(p))}</b></span></li>"
        name, inside = m.groups()
        # the knobs, by their names; anything else in brackets is a note on the page
        rest = knobs(inside) if all(k[:1].isupper() for k in inside.split(", ")) else f'<span class="fm-note">{drawn(inside)}</span>'
        return f"<li><span><b>{drawn(cap(name))}</b> {rest}</span></li>"

    def line(t):
        head, *rest = split_top(t, ": ")
        if not rest:
            return f"<p>{drawn(cap(t))}</p>"
        rest = ": ".join(rest)
        if head == "a tap":
            return f'<p><span class="fm-tag">tap</span> {drawn(rest)}</p>'
        held = re.fullmatch(r"((?:white|black) \d+(?:–\d+)?) held", head)     # "white 6 held": the key, then held
        if held:
            label = f'{drawn(held.group(1))} <span class="fm-tag">held</span>'
        else:                                       # a note in brackets at the end, lighter: "The top key (G)"
            note = re.fullmatch(r"(.+?) (\([^()]+\))", head)
            label = f'<b>{drawn(cap(note.group(1)))}</b> <span class="fm-note">{drawn(note.group(2))}</span>' if note \
                else f"<b>{drawn(cap(head))}</b>"
        if rest.startswith("the knobs "):
            return f"<p>{label} {knobs(rest[len('the knobs '):])}</p>"
        return f"<p>{label} {drawn(rest)}</p>"

    first, *more = split_top(text, ". ")
    tap = split_top(first, ", held: ")          # "a tap: …, held: …"
    if len(tap) == 2:
        first, more = tap[0], ["Held: " + tap[1]] + more
    pages = split_top(first, "; again, ")
    out = ('<ol class="fm-pages">' + "".join(page(p) for p in pages) + "</ol>") if len(pages) > 1 else \
        "".join(line(i) for i in split_top(first, "; "))
    for sentence in more:
        for h in split_top(sentence, "; "):
            m = re.fullmatch(r"(?i)(held(?: on)?)(?:, |: )(.+)", h)
            if not m:
                raise SystemExit(f"README.md ## In short: row {row!r}: {h!r} isn't what holding it does")
            out += f'<p><span class="fm-tag">{m.group(1).lower()}</span> {drawn(m.group(2))}</p>'
    return out


def controls():
    """The README's "## In short" as the cheat sheet's controls: each control drawn as it looks, beside its job in short
    lines. A row for several controls ("PLAY · REC · EDIT") whose job splits the same way is a control each."""
    out = []
    for key, text in md_rows("In short")[2:]:
        if "*" in key + text:
            raise SystemExit(f"README.md ## In short: row {key!r}: the cheat sheet draws no bold or italics")
        names, jobs = key.split(" · "), split_top(text, " · ")
        if len(names) > 1 and len(jobs) != len(names):
            raise SystemExit(f"README.md ## In short: row {key!r} doesn't split into {names}")
        for name, j in zip(names, jobs if len(names) > 1 else [text]):
            out.append(f'<div class="fm-c"><dt>{control(name)}</dt><dd>{job(j, key)}</dd></div>')
    return "\n".join(out)


def whats_new(version):
    """The landing page's list: the bullets under the README's "## What's new in VERSION", so it's written once. Stops if
    this version has none, or a release would go out with the last one's news."""
    lines = (SRC / "README.md").read_text(encoding="utf-8").splitlines()
    head = "## What's new in " + version
    if head not in lines:
        raise SystemExit(f"README.md: no {head}")
    items = []
    for line in lines[lines.index(head) + 1:]:
        if line.startswith("## "):
            break
        if line.startswith("- "):
            items.append(line[2:])
        elif items and line.strip():
            items[-1] += " " + line.strip()
    if not items:
        raise SystemExit(f"README.md: nothing under {head}")

    def item(t):
        t = html.escape(t, quote=False)
        t = re.sub(r"`([^`]+)`", r"<code>\1</code>", t)
        t = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", lambda m: f'<a href="{m.group(2).removeprefix(SITE)}">{m.group(1)}</a>', t)
        return re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", t)
    return '<ul class="new">\n' + "".join(f"  <li>{item(t)}</li>\n" for t in items) + "</ul>"


def flows():
    """The cheat sheet's flows: the drawings. Their screens are docs/img/flow-*.png (tests/listen/flowshots.amb)."""
    page = (SRC / "web" / "cheat" / "flows.html").read_text(encoding="utf-8")
    for name in re.findall(r'src="\.\./img/([^"]+)"', page):
        if not (SRC / "docs" / "img" / name).is_file():
            raise SystemExit(f"web/cheat/flows.html: no docs/img/{name}")
    return page


SMK37_AWAY = "https://aisling.stephen8n.com/smk37/"   # where its pages are, for a build made without them (github.io's copy is retired)


def drawing(path, alt=None):
    """A drawing (an svg of tools/gen_fm1_figure.py's or the SMK-37 Elite's) to put inline: described by `alt`, or, with
    none, hidden from a screen reader (the root's, inside a link that says what it is)."""
    svg = Path(path).read_text(encoding="utf-8")
    if " id=" in svg:
        raise SystemExit(f"{path}: an id in a drawing that goes inline beside others")
    head = re.match(r'<svg [^>]*?role="img" aria-label="[^"]*"', svg)
    if not head:
        raise SystemExit(f"{path}: not one of the drawings")
    say = f'role="img" aria-label="{html.escape(alt)}"' if alt else 'aria-hidden="true" focusable="false"'
    return re.sub(r'role="img" aria-label="[^"]*"', say, svg, count=1)


def where(h):
    """A link on the root to one of the parts' pages: relative, unless that part's pages are published elsewhere."""
    for key, _, start, _ in PRODUCTS:
        if h.startswith(key + "/") and start.startswith("https:"):
            return start + h[len(key) + 1:]
    return h


def vers(s, version):
    """The parts' versions put into a card's words or links."""
    return (s.replace("__VERSION__", html.escape(version)).replace("__FX1__", FX1_VERSION)
            .replace("__SMK37__", SMK37_VERSION))


def part_cards(version, pics):
    cards = []
    for n, (key, state, out, does, links) in enumerate(PART_CARDS, 1):
        label, start, what = next((label, start, what) for k, label, start, what in PRODUCTS if k == key)
        state = vers(state, version)
        btns = "".join(f'<li><a class="btn{" go" if go else ""}" href="{where(vers(h, version))}">{t}</a></li>'
                       for t, h, go in links)
        cards.append(f'<section class="part" aria-labelledby="p-{key}">\n'
                     f'<a href="{start}" title="{html.escape(what)}"><div class="mg-num" aria-hidden="true">0{n}</div><div class="pic">{pics[key]}</div>'
                     f'<h2 id="p-{key}">{label}</h2><p class="state{" out" if out else ""}">{state}</p>'
                     f'<p class="does">{does}</p></a>\n<ul>{btns}</ul>\n</section>')
    return "\n".join(cards)


def controls_page(kind, svg_path):
    """One of the FM-1's controls pages (FM1_CONTROLS, FX1_CONTROLS): its drawing, its skins' buttons, its table."""
    svg = svg_path.read_text(encoding="utf-8")
    skins = ["orange"] + sorted(set(re.findall(r'data-skin="(\w+)"', svg)))
    buttons = "".join(f'<button type="button" value="{k}" aria-pressed="false">{k}</button>' for k in skins)
    btn_href, btn = kind["button"]
    return (CONTROLS.replace("__TITLE__", kind["title"]).replace("__DESC__", html.escape(kind["desc"]))
            .replace("__STRIP__", kind["strip"]).replace("__BUTTON_HREF__", btn_href).replace("__BUTTON__", btn)
            .replace("__NOTICE__", kind["notice"]).replace("__SKINS__", buttons)
            .replace("__PANEL__", drawing(svg_path, re.search(r'aria-label="([^"]*)"', svg).group(1)))
            .replace("__SVG__", "../../img/" + svg_path.name).replace("__TABLE_HEAD__", kind["table_head"])
            .replace("__TABLE__", md_table(kind["table"][1], kind["table"][0], kind.get("manual"))).replace("__AFTER__", kind["after"]))


# FX-1's installer: the FM-1's page (web/amb_installer.html) and its code as they are, FX-1's words where the page has
# AISLING's. Each is changed where it is once, so a change to the page that moves one stops the build instead of leaving
# AISLING's words on FX-1's page. Its "Back to the stock firmware" becomes "Going back", pointing at AISLING's page
FX1_INSTALL_EDITS = (
    ("<title>Install · AISLING</title>", "<title>Install · AISLING FX-1</title>"),
    ("put AISLING on your FM-1</p>", "put AISLING FX-1 on your FM-1</p>"),
    ("<h1>Install <span>AISLING</span></h1>", "<h1>Install <span>FX-1</span></h1>"),
    ('href="../manual/#recovering', 'href="../../manual/#recovering'),
    ("AISLING is by CÚ_STE", "AISLING FX-1 is by CÚ_STE"),
    ('"https://github.com/stephencummins/fm1-aisling" target="_blank" rel="noopener">github.com/stephencummins/fm1-aisling<',
     f'"{FX1_REPO}" target="_blank" rel="noopener">{FX1_REPO.removeprefix("https://")}<'),
)
FX1_INSTALL_BACK = """<details id="back">
  <summary data-t="backh"></summary>
  <div class="mg-in">
  <p data-t="back1"></p>
  <p data-t="back2"></p>
  <p class="btns"><a class="btn" href="../../install/" data-t="backgo1"></a>
  <a class="btn" href="../../install/#stock" data-t="backgo2"></a></p>
  </div>
  </details>"""
FX1_INSTALL_TEXT = {
    "en": {"ready": "Ready to install AISLING FX-1 ", "manual": "Back to AISLING FX-1",
           "chipother": "not one AISLING FX-1 knows", "backh": "Going back",
           "back1": "AISLING goes back over FX-1 from its own install page, as over any firmware. When that was tried, with "
                    "no save made in FX-1, AISLING's saved pieces were as they had been.",
           "back2": "The stock firmware goes back as it does from AISLING, and only in the version whose bootloader your "
                    "FM-1 already has: V16 with M-VAVE's M-UPGRADE, V15 from The Firmware Hub. What FX-1 saved may be lost.",
           "backgo1": "Install AISLING", "backgo2": "Back to the stock firmware"},
    "ja": {"ready": "インストールできます: AISLING FX-1 ", "manual": "AISLING FX-1 のページへ",
           "chipother": "AISLING FX-1 の知らないチップ", "backh": "元に戻す",
           "back1": "AISLING は、ほかのファームウェアと同じように、AISLING のインストールページから FX-1 の上に入れられます。"
                    "FX-1 で保存せずに試したところ、AISLING で保存した曲はそのまま残っていました。",
           "back2": "純正ファームウェアも AISLING からと同じように戻せますが、FM-1 にすでに入っているブートローダーと同じバージョンに"
                    "限ります（V16 は M-VAVE の M-UPGRADE で、V15 は The Firmware Hub から）。FX-1 で保存したものは消えることがあります。",
           "backgo1": "AISLING をインストールする", "backgo2": "純正ファームウェアに戻す"},
}


def fx1_installer(page):
    """FX-1's installer page from the FM-1's (before its code and metadata go in)."""
    for old, new in FX1_INSTALL_EDITS:
        if page.count(old) != 1:
            raise SystemExit(f"web/amb_installer.html: {old!r} is not there once, for FX-1's installer")
        page = page.replace(old, new)
    page, n = re.subn(r'<details id="stock">.*?</details>', FX1_INSTALL_BACK, page, flags=re.S)
    if n != 1:
        raise SystemExit("web/amb_installer.html: no one stock section, for FX-1's installer")
    words = "".join(f"Object.assign(TEXT.{lang}, {json.dumps(t, ensure_ascii=False)});\n" for lang, t in FX1_INSTALL_TEXT.items())
    mark = 'let lang = "en";'
    if page.count(mark) != 1:
        raise SystemExit(f"web/amb_installer.html: {mark!r} is not there once, for FX-1's installer")
    return page.replace(mark, "/* AISLING FX-1's words (tools/make_pages.py) */\n" + words + mark)


def fx1_package(path):
    """FX-1's package: the release's asset, byte for byte, with its identity and its loader -> (bytes, identity)."""
    if not path.exists():
        raise SystemExit(f"no {path}: FX-1's package, from its release (gh release download v{FX1_VERSION} "
                         f"-R stephencummins/fx1-aisling -p '*.fwsc' -D build), or give one with --fx1 FILE")
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != FX1_SHA256:
        raise SystemExit(f"{path}: not FX-1 {FX1_VERSION}'s release package (its sha256 differs from FX1_SHA256)")
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_391(?!000000)\d{6}", product) or b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{path}: identity {product!r}, or no update loader: not a release of FX-1")
    return raw, product


def fx1_landing():
    """FX-1's page, with its screens if there are any in docs/img (fx1-*.png, from the simulator: tests/scenarios-fx)."""
    shots = [(f, a) for f, a in FX1_SHOTS if (SRC / "docs" / "img" / f).exists()]
    pics = "".join(f'  <img src="../img/{f}" width="240" height="240" alt="{html.escape(a)}">\n' for f, a in shots)
    return (FX1_LANDING.replace("__SHOTS__", f'<div class="mg-shots">\n{pics}</div>' if shots else "")
            .replace("__PANEL__", drawing(SRC / "docs" / "img" / "fx1-controls.svg"))
            .replace("__REPO__", REPO).replace("__SITE__", SITE).replace("__BY__", BY).replace("__FX1_REPO__", FX1_REPO))


# FX-1's screens, as the simulator draws them, for its page
FX1_SHOTS = (("fx1-screen.png", "AISLING FX-1's screen while the computer's sound plays: PLAYING, the level in, left and "
                                "right, and the stream's counts"),
             ("fx1-idle.png", "The screen with a computer that hasn't opened the stream: NOTHING SENT YET"),
             ("fx1-perform.png", "PERFORM: the keys' effects on the computer's sound"))


def manual_images(md, dest):
    """The images a manual names (docs/img/...), beside it."""
    dest.mkdir(parents=True, exist_ok=True)
    for name in sorted(set(re.findall(r"\]\(img/([^)\s]+)", (SRC / "docs" / md).read_text(encoding="utf-8")))):
        shutil.copy(SRC / "docs" / "img" / name, dest / name)


def cheat_sheet(version):
    return (CHEAT.replace("__VERSION__", html.escape(version)).replace("__BY__", BY)
            .replace("__CONTROLS__", controls()).replace("__PIECES__", md_table("The pieces"))
            .replace("__FLOWS_CSS__", (SRC / "web" / "cheat" / "flows.css").read_text(encoding="utf-8"))
            .replace("__FLOWS__", flows()))


def check_visualiser_bar():
    """The visualiser carries its own copy of the bar, because it fetches nothing. Stop if it no longer says what the
    switch (PRODUCTS) and the FM-1's NAV say (a new page in NAV, say): it would be published with one page missing.
    (Before site_brand.away: its copy points at the parts' places here, and main moves the SMK-37's if it must.)"""
    page = (SRC / "web" / "visualiser" / "index.html").read_text(encoding="utf-8")
    for cls, items in (("sitebar-kit", PRODUCTS), ("sitebar-nav", NAV)):
        nav = re.search(rf'<nav class="{cls}".*?</nav>', page, re.S)
        found = re.findall(r'href="([^"]+)"[^>]*>([^<]+)</a>', nav.group(0)) if nav else []
        want = [(href if href.startswith("https:") else SITE + href, label) for _, label, href, _ in items]
        if found != want:
            raise SystemExit(f"web/visualiser/index.html: its {cls} differs from tools/site_brand.py's:\n  {found}\n  {want}")


def main(pkg, version, out, smk37=None, fx1=None):
    check_visualiser_bar()
    pkg, out = Path(pkg), Path(out)
    fx1 = Path(fx1) if fx1 else SRC / "build" / f"aislingfx-{FX1_VERSION}.fwsc"
    fx1_raw, fx1_product = fx1_package(fx1)
    fx1_name = f"aislingfx-{FX1_VERSION}.fwsc"
    if smk37:
        smk37 = Path(smk37)
        for f in ("index.html", "controls/index.html", "manual/index.html", "img/smk37-panel.svg"):
            if not (smk37 / f).exists():
                raise SystemExit(f"{smk37}: no {f}: make the SMK-37 Elite's pages with tools/make_pages_smk37.py --marks DIR")
    else:
        site_brand.away("smk37", SMK37_AWAY)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_353\d{6}", product):
        raise SystemExit(f"{pkg}: identity {product!r}: make a release build (tools/mini_build.sh --release X.Y)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    name = f"aisling-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for d in ("install", "manual", "cheat", "firmware", "fm1/controls", "fx1/controls", "fx1/manual", "fx1/install"):
        (out / d).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    (out / "firmware" / fx1_name).write_bytes(fx1_raw)
    # the installer, and FX-1's: the same page and code, each with its package's metadata
    page = (SRC / "web" / "amb_installer.html").read_text(encoding="utf-8")
    lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
    for mark in ("/*LIB*/", "/*META*/"):
        if page.count(mark) != 1:
            raise SystemExit(f"amb_installer.html must contain {mark} once")
    meta = json.dumps({"version": version, "product": product, "pkg": "../firmware/" + name})
    (out / "install" / "index.html").write_text(stamp(page.replace("/*LIB*/", lib).replace("/*META*/", meta), "../", "install"),
                                                encoding="utf-8")
    meta = json.dumps({"version": FX1_VERSION, "product": fx1_product, "pkg": "../../firmware/" + fx1_name})
    (out / "fx1" / "install" / "index.html").write_text(
        stamp(fx1_installer(page).replace("/*LIB*/", lib).replace("/*META*/", meta), "../../", "install", "fx1"), encoding="utf-8")
    # the manual (a complete page; its images beside it). It's made by its own process, so it's told what isn't here
    away = [] if smk37 else ["--away", f"smk37={SMK37_AWAY}"]
    subprocess.run([sys.executable, str(SRC / "tools" / "manual_html.py"), str(SRC / "docs" / "MANUAL.md"),
                    str(out / "manual" / "index.html")] + away, check=True, capture_output=True)
    shutil.copytree(SRC / "docs" / "img", out / "manual" / "img")
    # the pages' pictures: the screenshots and the drawings (docs/img), the cards, the visualiser's painting
    shutil.copytree(SRC / "docs" / "img", out / "img")
    shutil.copy(SRC / "docs" / "card.png", out / "img" / "card.png")
    shutil.copy(SRC / "docs" / "card-fx1.png", out / "img" / "card-fx1.png")
    shutil.copy(SRC / "web" / "visualiser" / "share.jpg", out / "img" / "visualiser.jpg")
    # the root: the three parts, each drawn
    pics = {"fm1": drawing(SRC / "docs" / "img" / "fm1-controls.svg"),
            "fx1": '<img src="img/fx1-photo.webp" width="984" height="590" alt="">',     # docs/img: CÚ_STE's FM-1, skinned
            "smk37": drawing(smk37 / "img" / "smk37-panel.svg").replace('<svg ', '<svg data-skin="grey" ', 1) if smk37
            else f'<img src="{SMK37_AWAY}img/smk37-panel.svg" alt="">'}
    (out / "index.html").write_text(stamp(CHOOSER.replace("__PARTS__", part_cards(version, pics)).replace("__SITE__", SITE)
                                          .replace("__BY__", BY), "", None, None), encoding="utf-8")
    # the FM-1's page, written as if at the root (as it was) and moved down one
    page = (LANDING.replace("__VERSION__", html.escape(version)).replace("__PKG__", name).replace("__REPO__", REPO)
            .replace("__SITE__", SITE).replace("__BY__", BY).replace("__NEW__", whats_new(version)))
    page = re.sub(r'\b(href|src)="(?!https?:|#|/|mailto:)', r'\1="../', page)
    page = page.replace("__PANEL__", drawing(SRC / "docs" / "img" / "fm1-controls.svg"))
    (out / "fm1" / "index.html").write_text(stamp(page, "../", None, "fm1"), encoding="utf-8")
    # the controls pages, the FM-1's and FX-1's
    (out / "fm1" / "controls" / "index.html").write_text(
        stamp(controls_page(FM1_CONTROLS, SRC / "docs" / "img" / "fm1-controls.svg"), "../../", "controls", "fm1"), encoding="utf-8")
    (out / "fx1" / "controls" / "index.html").write_text(
        stamp(controls_page(FX1_CONTROLS, SRC / "docs" / "img" / "fx1-controls.svg"), "../../", "controls", "fx1"), encoding="utf-8")
    # FX-1's page and its manual (docs/FX1.md, its images beside it)
    (out / "fx1" / "index.html").write_text(stamp(fx1_landing(), "../", None, "fx1"), encoding="utf-8")
    subprocess.run([sys.executable, str(SRC / "tools" / "manual_html.py"), str(SRC / "docs" / "FX1.md"),
                    str(out / "fx1" / "manual" / "index.html"), "--part", "fx1"] + away, check=True, capture_output=True)
    manual_images("FX1.md", out / "fx1" / "manual" / "img")
    # the SMK-37 Elite's pages, made on its branch, given this site's bar
    if smk37:
        shutil.copytree(smk37, out / "smk37")
        for f, root, key in (("index.html", "../", None), ("controls/index.html", "../../", "controls"),
                             ("manual/index.html", "../../", "manual")):
            page = stamp((out / "smk37" / f).read_text(encoding="utf-8"), root, key, "smk37")
            # its docs name its pages where they're published on their own: here, a link to one of them goes to its
            # copy beside it, but for the one that shows that address as its text
            page = re.sub(rf'<a href="{re.escape(SMK37_AWAY)}([^"]*)">(?!stephencummins\.github\.io)',
                          lambda m: f'<a href="{root}smk37/{m.group(1)}">', page)
            (out / "smk37" / f).write_text(page, encoding="utf-8")
    # the cheat sheet
    (out / "cheat" / "index.html").write_text(stamp(cheat_sheet(version), "../", "cheat"), encoding="utf-8")
    emu = SRC / "build" / "emu"
    if not (emu / "amb.wasm").exists():
        raise SystemExit("no build/emu/amb.wasm: run web/emu/build.sh (needs Emscripten)")
    shutil.copytree(emu, out / "emu")
    (out / "emu" / "index.html").write_text(stamp((emu / "index.html").read_text(encoding="utf-8"), "../", "emu"), encoding="utf-8")
    # the editor: its own files (build/editor's copies of the harp, the style and the fonts are for serving it alone)
    ed = SRC / "build" / "editor"
    if not (ed / "amb.wasm").exists():
        raise SystemExit("no build/editor/amb.wasm: run web/editor/build.sh (needs Emscripten)")
    (out / "editor").mkdir()
    for f in ("amb.wasm", "engine.js", "worklet.js", "link.js", "editor.js", "editor.css"):
        shutil.copy2(ed / f, out / "editor" / f)
    (out / "editor" / "index.html").write_text(stamp((SRC / "web" / "editor" / "index.html").read_text(encoding="utf-8"), "../", "editor"), encoding="utf-8")
    # the sound packs: the page, and every pack in packs/ made afresh (tools/make_pack.py), listed in index.json
    (out / "packs").mkdir()
    for f in ("packs.js", "worklet.js"):
        shutil.copy2(SRC / "web" / "packs" / f, out / "packs" / f)
    (out / "packs" / "index.html").write_text(stamp((SRC / "web" / "packs" / "index.html").read_text(encoding="utf-8"), "../", "packs"),
                                              encoding="utf-8")
    listed = []
    for src in sorted((SRC / "packs").glob("*.json")):
        subprocess.check_call([sys.executable, str(SRC / "tools" / "make_pack.py"), str(src), "-o", str(SRC / "build" / "packs")])
        listed.append(json.loads((SRC / "build" / "packs" / src.name).read_text(encoding="utf-8")))
    (out / "packs" / "index.json").write_text(json.dumps(listed), encoding="utf-8")
    # the harp, the bar's style and the fonts, at the root, for every page; Cloudflare's two files with them
    shutil.copytree(SRC / "web" / "brand", out, dirs_exist_ok=True)
    # the visualiser, as it is, but for its switch's SMK-37 if those pages aren't here
    shutil.copytree(SRC / "web" / "visualiser", out / "visualiser")
    if not smk37:
        v = out / "visualiser" / "index.html"
        v.write_text(v.read_text(encoding="utf-8").replace(f'href="{SITE}smk37/"', f'href="{SMK37_AWAY}"'), encoding="utf-8")
    # the add-on screen's page and its pictures
    shutil.copytree(SRC / "web" / "screen", out / "screen")
    (out / "screen" / "index.html").write_text(stamp((SRC / "web" / "screen" / "index.html").read_text(encoding="utf-8"), "../", "screen"),
                                               encoding="utf-8")
    # the support page: M-Flasher and m-bootloader
    (out / "support").mkdir()
    (out / "support" / "index.html").write_text(stamp((SRC / "web" / "support" / "index.html").read_text(encoding="utf-8"), "../", "support"),
                                                encoding="utf-8")
    # the screen reader's words files (fm1-reader): every one ever made, for the devices that point at them
    shutil.copytree(SRC / "web" / "words", out / "words")
    (out / ".nojekyll").write_text("")
    print(f"site: {out}: index.html, fm1/ (and controls/), fx1/ (controls/, manual/, install/ ({fx1_product})), "
          f"smk37/ {'(from ' + str(smk37) + ')' if smk37 else '(not here: ' + SMK37_AWAY + ')'}, install/ ({product}), "
          f"manual/, cheat/, emu/, editor/, visualiser/, screen/, words/, firmware/{name} ({len(raw)} B) "
          f"and {fx1_name} ({len(fx1_raw)} B)")


if __name__ == "__main__":
    import os
    args = sys.argv[1:]
    smk = os.environ.get("SMK37_PAGES") or None
    if "--smk37" in args:
        i = args.index("--smk37")
        if i + 1 >= len(args):
            sys.exit(__doc__)
        smk = args[i + 1]
        del args[i:i + 2]
    fx = os.environ.get("FX1_PKG") or None
    if "--fx1" in args:
        i = args.index("--fx1")
        if i + 1 >= len(args):
            sys.exit(__doc__)
        fx = args[i + 1]
        del args[i:i + 2]
    if len(args) != 3:
        sys.exit(__doc__)
    main(*args, smk37=smk, fx1=fx)
