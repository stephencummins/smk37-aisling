#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The bar along the top of every page of AISLING's site, and what goes in each page's head: written once, here.
A page carries the two marks, once each (<!--AISLING-HEAD--> last in its head, so web/brand/site.css comes after the
page's own style, and <!--AISLING-BAR--> first in its body); tools/make_pages.py and tools/manual_html.py fill them.
The links between the pages are relative, so each place the site is served from is whole in itself, and every page names
its home at SITE as its canonical address. The visualiser fetches nothing, so it has its own copy of the harp
(tools/make_brand.py) and of this nav: a change here goes there too, and tools/make_pages.py stops if they differ.
Another site in the same dress (tools/make_pages_smk37.py, the SMK-37 Elite's) passes its own nav and site."""

NAME, BY = "AISLING", "by CU_STE"
SITE = "https://aisling.stephen8n.com/"
# (the page's key, its label, where it goes: relative to the site's root unless it's a whole URL, what it is)
NAV = (("emu", "emulator", "emu/", "AISLING in the browser: the same code the FM-1 runs"),
       ("editor", "editor", "editor/", "Every setting of a piece on one panel, and your FM-1 over USB"),
       ("install", "install", "install/", "Put AISLING on your FM-1 from Chrome or Edge"),
       ("manual", "manual", "manual/", "Getting started in a few minutes, then everything else"),
       ("cheat", "cheat sheet", "cheat/", "Every control and the eight pieces on one page, to print, and the steps drawn as flows"),
       ("visualiser", "visualiser", "visualiser/", "An FM-1 running AISLING paints an Irish landscape from its MIDI"),
       ("screen", "screen", "screen/", "A touch screen beside the FM-1: the landscape, the FM-1's own screen, and its knobs"),
       ("gear", "gear", "https://gear.stephen8n.com/", "What plugs into an FM-1 running AISLING, and how: keyboards, cables, power, cases"),
       ("video", "video", "https://youtu.be/3aUARUvwT_Y", "A short film of AISLING and its visualiser, on YouTube"),
       ("source", "source", "https://github.com/stephencummins/fm1-aisling", "The code, on GitHub"),
       ("stephen8n", "stephen8n", "https://www.stephen8n.com", "Stephen Cummins's other things"))


def canonical(current=None, nav=NAV, site=SITE):
    """The page's address at the site's home. `current` is its key in NAV; the landing page has none."""
    return site + next((href for key, _, href, _ in nav if key == current), "")


def head(root, current=None, nav=NAV, site=SITE):
    """The head's share, for a page `root` away from the site's root ("" or "../")."""
    return (f'<link rel="canonical" href="{canonical(current, nav, site)}">\n'
            f'<meta name="theme-color" content="#f3ead7">\n'
            f'<link rel="icon" type="image/svg+xml" href="{root}favicon.svg">\n'
            f'<link rel="icon" type="image/png" sizes="32x32" href="{root}favicon-32.png">\n'
            f'<link rel="apple-touch-icon" href="{root}apple-touch-icon.png">\n'
            f'<link rel="preload" as="font" type="font/woff2" crossorigin href="{root}fonts/BarlowSemiCondensed-SemiBold.woff2">\n'
            f'<link rel="preload" as="font" type="font/woff2" crossorigin href="{root}fonts/IBMPlexMono-Regular.woff2">\n'
            f'<link rel="stylesheet" href="{root}site.css">\n'
            f'<script src="{root}wall.js" defer></script>\n')


def bar(root, current=None, nav=NAV):
    """The bar itself. `current` is the page's key in NAV; the landing page has none, and is the home link's."""
    links = []
    for key, label, href, what in nav:
        url = href if href.startswith("https:") else root + href
        now = ' aria-current="page"' if key == current else ""
        links.append(f'<a href="{url}" title="{what}"{now}>{label}</a>')
    home = root or "./"
    now = ' aria-current="page"' if current is None else ""
    return (f'<div class="wall" data-art="{current or "home"}" aria-hidden="true"></div>\n'
            f'<div class="sitebar"><div class="sitebar-in">\n'
            f'<a class="sitebar-home" href="{home}" title="{NAME}: the home page"{now}>'
            f'<img class="sitebar-mark" src="{root}mark.png" width="30" height="30" alt="">'
            f'<span class="sitebar-name">{NAME}</span><span class="sitebar-by">{BY}</span></a>\n'
            f'<nav class="sitebar-nav" aria-label="{NAME}\'s pages">{"".join(links)}</nav>\n'
            f'</div></div>\n')


def stamp(page, root, current=None, nav=NAV, site=SITE):
    """Fills a page's two marks, and refuses a page without them."""
    for mark in ("<!--AISLING-HEAD-->\n", "<!--AISLING-BAR-->\n"):
        if page.count(mark) != 1:
            raise SystemExit(f"a page must contain {mark.strip()} once, on its own line")
    return (page.replace("<!--AISLING-HEAD-->\n", head(root, current, nav, site))
            .replace("<!--AISLING-BAR-->\n", bar(root, current, nav)))
