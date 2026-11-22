#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The bar along the top of every page of AISLING's site, and what goes in each page's head: written once, here.
A page carries the two marks, once each (<!--AISLING-HEAD--> last in its head, so web/brand/site.css comes after the
page's own style, and <!--AISLING-BAR--> first in its body); tools/make_pages.py and tools/manual_html.py fill them.
The links between the pages are relative, so each place the site is served from is whole in itself, and every page names
its home at SITE as its canonical address. The visualiser fetches nothing, so it has its own copy of the harp
(tools/make_brand.py) and of this bar: a change here goes there too, and tools/make_pages.py stops if they differ.
Another site in the same dress (tools/make_pages_smk37.py's own copy of the SMK-37 Elite's pages, when it is built apart
from this site) passes its own `nav` and `site`: then the bar has that nav and no switch.

AISLING runs on three things, so the site has three parts, PRODUCTS, and its root chooses between them. The bar has a
switch between the three (the page's own filled), then the page's part's nav: NAVS[part]. The root belongs to none of
them and has the nav every part shares, COMMON. The FM-1's pages are where they always were (emu/, install/, manual/ ...)
so the links already out there still work; only its landing page moved, to fm1/."""

NAME, BY = "AISLING", "by CÚ_STE"
SITE = "https://aisling.stephen8n.com/"
REPO = "https://github.com/stephencummins/fm1-aisling"
# the three parts: (key, label, where it starts, relative to the site's root unless a whole URL, what it is)
PRODUCTS = [["fm1", "FM-1", "fm1/", "AISLING on the M-VAVE FM-1: the instrument, to install"],
            ["fx1", "FX-1", "fx1/", "AISLING FX-1: an FM-1 as a USB effects box for a computer's sound"],
            ["smk37", "SMK-37", "smk37/", "AISLING on the M-VAVE SMK-37 Elite: 37 keys, pads and faders"]]
# (the page's key, its label, where it goes: relative to the site's root unless it's a whole URL, what it is)
STORE = ("store", "store", "https://store.stephen8n.com/", "Skins for the FM-1, printed and cut by hand, and the gear that goes with it: coming soon")
DIY = ("diy", "make a skin", "https://store.stephen8n.com/diy/", "How to make your own skin for the FM-1 at home: what to buy, and how to print and cut it")
SEER = ("seer", "seer", "https://seer.stephen8n.com/", "The Flash Seer: a fortune cabinet that asks strange questions and deals the FM-1 firmware you should flash")
APP = ("app", "app", "emu/?app", "AISLING as an app on a phone: full screen, from the home screen, with no connection")
SUPPORT = ("support", "support", "support/", "Back an FM-1 up, switch its firmware, and bring one back that won't start: M-Flasher and m-bootloader")
STEPHEN8N = ("stephen8n", "stephen8n", "https://www.stephen8n.com", "Stephen Cummins's other things")
GEAR = ("gear", "gear", "https://gear.stephen8n.com/", "What plugs into an FM-1 running AISLING, and how: keyboards, cables, power, cases")
VIDEO = ("video", "video", "https://youtu.be/3aUARUvwT_Y", "A short film of AISLING and its visualiser, on YouTube")
NAV = (STORE,
       ("emu", "emulator", "emu/", "AISLING in the browser: the same code the FM-1 runs"),
       APP,
       ("editor", "editor", "editor/", "Every setting of a piece on one panel, and your FM-1 over USB"),
       ("install", "install", "install/", "Put AISLING on your FM-1 from Chrome or Edge"),
       ("packs", "sound packs", "packs/", "Eight more pieces at a time: hear a pack, then put it on your FM-1 over USB"),
       ("manual", "manual", "manual/", "Getting started in a few minutes, then everything else"),
       ("controls", "controls", "fm1/controls/", "Every knob, button and key of the FM-1 and what it does, drawn"),
       ("cheat", "cheat sheet", "cheat/", "Every control and the eight pieces on one page, to print, and the steps drawn as flows"),
       SUPPORT,
       ("visualiser", "visualiser", "visualiser/", "An FM-1 running AISLING paints an Irish landscape from its MIDI"),
       ("screen", "screen", "screen/", "A touch screen beside the FM-1: the landscape, the FM-1's own screen, and its knobs"),
       GEAR,
       DIY,
       SEER,
       VIDEO,
       ("source", "source", REPO, "The code, on GitHub"),
       STEPHEN8N)
FX1_NAV = (STORE,
           ("install", "install", "fx1/install/", "Put AISLING FX-1 on your FM-1 from Chrome or Edge"),
           ("controls", "controls", "fx1/controls/", "Every knob, button and key of the FM-1 running FX-1 and what it does, drawn"),
           ("manual", "manual", "fx1/manual/", "Setting it up with a computer, the effects, the screen, and going back"),
           SUPPORT,
           DIY,
           SEER,
           ("source", "source", "https://github.com/stephencummins/fx1-aisling", "FX-1's code, on GitHub"),
           STEPHEN8N)
SMK37_NAV = [["controls", "controls", "smk37/controls/", "Every key, pad, knob, fader and button of the Elite and what it does"],
             ["manual", "manual", "smk37/manual/", "How the port works: the flash, the pins, building, testing, and what's still open"],
             ["source", "source", "https://github.com/stephencummins/smk37-aisling", "The SMK-37 Elite's code, on GitHub"],
             STEPHEN8N]
COMMON = (STORE,
          APP,
          SUPPORT,
          GEAR,
          DIY,
          SEER,
          VIDEO,
          ("source", "source", REPO, "The code, on GitHub"),
          STEPHEN8N)
NAVS = {None: COMMON, "fm1": NAV, "fx1": FX1_NAV, "smk37": SMK37_NAV}
# The store leads every nav that has it, on the bar itself: site.css draws it as a button (.sitebar-nav > a[href^=store]).
# How the bar shows a nav: a page's key is a link on the bar itself; (label, keys) is a menu that opens under its label.
# A nav's pages keep their order, so a group is a run of neighbours; a part with no entry here shows every page.
MORE = ("more", ("seer", "video", "source", "stephen8n"))
KIT = ("kit", ("gear", "diy"))
GROUPS = {None: ("store", "app", "support", KIT, MORE),
          "fm1": ("store", ("play", ("emu", "app", "editor")), "install", "packs", ("docs", ("manual", "controls", "cheat", "support")),
                  ("visuals", ("visualiser", "screen")), KIT, MORE),
          "fx1": ("store", "install", "controls", "manual", "support", KIT, MORE)}


def grouped(product):
    """The part's nav as the bar shows it: a page (key, label, href, what), or (label, [pages]) for a menu. A key a
    group names that the nav hasn't (FX-1 has no video) is passed over; a page no group names stops the build."""
    nav = {n[0]: n for n in NAVS[product]}
    if product not in GROUPS:
        return [tuple(n) for n in NAVS[product]]
    out = []
    for g in GROUPS[product]:
        if isinstance(g, str):
            out.append(tuple(nav[g]))
        else:
            out.append((g[0], [tuple(nav[k]) for k in g[1] if k in nav]))
    flat = [n[0] for g in out for n in (g[1] if isinstance(g[1], list) else [g])]
    if flat != [n[0] for n in NAVS[product]]:
        raise SystemExit(f"site_brand.GROUPS[{product!r}] must name every page of its nav once, in the nav's order: {flat}")
    return out


def nav_html(product, current, link):
    """The part's nav, inside its <nav>. `link(page, now)` writes one <a>. The menu holding the page you're on is marked."""
    out = []
    for g in grouped(product):
        if isinstance(g[1], list):
            here = ' class="sitebar-group has-now"' if any(n[0] == current for n in g[1]) else ' class="sitebar-group"'
            out.append(f'<details{here} name="sitebar"><summary>{g[0]}</summary><div class="sitebar-menu">'
                       + "".join(link(n, n[0] == current) for n in g[1]) + '</div></details>')
        else:
            out.append(link(g, g[0] == current))
    return "".join(out)


def away(key, url):
    """A part whose pages aren't in this build: its switch and its nav point at `url`, where they are."""
    for p in PRODUCTS:
        if p[0] == key:
            p[2] = url
    if key == "smk37":
        for n in SMK37_NAV:
            if n[2].startswith("smk37/"):
                n[2] = url + n[2][len("smk37/"):]


def href(root, h):
    return h if h.startswith("https:") else root + h


def canonical(current=None, product="fm1", nav=None, site=None):
    """The page's address at the site's home. `current` is its key in its part's nav; a part's first page has none,
    and the root is no part's. With a `nav` and `site` of its own (another site in this dress), the address there."""
    if nav is not None:
        return (site or SITE) + next((h for key, _, h, _ in nav if key == current), "")
    h = next((h for key, _, h, _ in NAVS[product] if key == current), None)
    if h is None:
        h = next((h for key, _, h, _ in PRODUCTS if key == product), "")
    return h if h.startswith("https:") else SITE + h


def head(root, current=None, product="fm1", nav=None, site=None):
    """The head's share, for a page `root` away from the site's root ("", "../" or "../../")."""
    return (f'<link rel="canonical" href="{canonical(current, product, nav, site)}">\n'
            f'<meta name="theme-color" content="#f3ead7">\n'
            f'<link rel="icon" type="image/svg+xml" href="{root}favicon.svg">\n'
            f'<link rel="icon" type="image/png" sizes="32x32" href="{root}favicon-32.png">\n'
            f'<link rel="apple-touch-icon" href="{root}apple-touch-icon.png">\n'
            f'<link rel="preload" as="font" type="font/woff2" crossorigin href="{root}fonts/BarlowSemiCondensed-SemiBold.woff2">\n'
            f'<link rel="preload" as="font" type="font/woff2" crossorigin href="{root}fonts/IBMPlexMono-Regular.woff2">\n'
            f'<link rel="stylesheet" href="{root}site.css">\n'
            f'<script src="{root}wall.js" defer></script>\n')


def bar(root, current=None, product="fm1", nav=None):
    """The bar itself: the home mark (the root, where the three parts are chosen), the switch, then the part's nav.
    `current` is the page's key in that nav; a part's first page has none, and is its switch's. With a `nav` of its
    own (another site in this dress) the bar has that nav, and no switch."""
    kit = []
    for key, label, h, what in () if nav is not None else PRODUCTS:
        now = (' aria-current="page"' if current is None else ' aria-current="true"') if key == product else ""
        kit.append(f'<a href="{href(root, h)}" title="{what}"{now}>{label}</a>')
    one = lambda n, now: (f'<a href="{href(root, n[2])}" title="{n[3]}"'
                          + (' aria-current="page"' if now else "") + f'>{n[1]}</a>')
    links = nav_html(product, current, one) if nav is None else "".join(one(n, n[0] == current) for n in nav)
    home = root or "./"
    now = ' aria-current="page"' if product is None else ""
    # the wall's seed: the FM-1's pages keep theirs from before there were three parts
    art = "aisling" if product is None else (current or "home") if product == "fm1" else f"{product}-{current or 'home'}"
    return (f'<div class="wall" data-art="{art}" aria-hidden="true"></div>\n'
            f'<div class="sitebar"><div class="sitebar-in">\n'
            f'<a class="sitebar-home" href="{home}" title="{NAME}: the FM-1, FX-1 and SMK-37"{now}>'
            f'<img class="sitebar-mark" src="{root}mark.png" width="30" height="30" alt="">'
            f'<span class="sitebar-name">{NAME}</span><span class="sitebar-by">{BY}</span></a>\n'
            f'<nav class="sitebar-kit" aria-label="{NAME} for">{"".join(kit)}</nav>\n'
            f'<nav class="sitebar-nav" aria-label="{NAME}\'s pages">{links}</nav>\n'
            f'</div></div>\n')


def stamp(page, root, current=None, product="fm1", nav=None, site=None):
    """Fills a page's two marks, and refuses a page without them. `product` is the page's part (None: the root);
    `nav` and `site` are another site's own, in this dress (tools/make_pages_smk37.py)."""
    for mark in ("<!--AISLING-HEAD-->\n", "<!--AISLING-BAR-->\n"):
        if page.count(mark) != 1:
            raise SystemExit(f"a page must contain {mark.strip()} once, on its own line")
    return (page.replace("<!--AISLING-HEAD-->\n", head(root, current, product, nav, site))
            .replace("<!--AISLING-BAR-->\n", bar(root, current, product, nav)))
