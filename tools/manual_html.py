#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""docs/MANUAL.md -> docs/manual.html. The Markdown is the source; run this after editing it.

  tools/manual_html.py [IN.md [OUT.html]] [--fragment]

--fragment writes the page without its <!doctype>/<html>/<head>/<body> wrapper (for hosts that
add their own); the default is a complete document that opens in any browser.

Handles what the manual uses: headings, paragraphs, **bold**, `code`, lists, tables, fenced
code, rules, and the contents list (linked to the sections by number). An image is
![what it shows](img/x.png "its caption"): the alt text says what the screen shows (for a screen
reader), the caption what it is (for everyone); without a caption, the alt text is both."""
import base64
import html
import re
import sys
from pathlib import Path

from site_brand import stamp

SRC = Path(__file__).resolve().parents[1]
PART_COLOURS = {}                                     # (X0X marks its parts with a swatch; AISLING has none)

CSS = """
/* The manual's own, on the site's bench (web/brand/site.css): the contents down the side, the text beside them */
html { scroll-padding-top: 4rem; }
.prose { font-size: 14px; line-height: 1.65; }
.prose > :first-child { margin-top: 0; }
.prose h2, .prose h3 { font-family: var(--brand-display); font-weight: 600; text-wrap: balance; line-height: 1.25; color: var(--brand-ink); }
.prose h2 { font-size: 19px; letter-spacing: .02em; margin: 2.4rem 0 .7rem; padding-top: .8rem; border-top: 1px solid var(--brand-rule); }
.prose h2 .num { color: var(--brand-green); margin-right: .25em; font-variant-numeric: tabular-nums; }
.prose h3 { font-size: 16px; letter-spacing: .01em; margin: 1.7rem 0 .4rem; }
.prose p, .prose li { max-width: 78ch; }
.prose p { margin: 0 0 .9rem; }
.prose ul, .prose ol { padding-left: 1.3rem; margin: 0 0 .9rem; }
.prose li { margin: .55rem 0; }
.prose li::marker { color: var(--brand-green); }
.skip { position: absolute; left: 16px; top: -100px; background: var(--brand-ground); border: 1px solid var(--brand-ink); padding: 6px 12px; z-index: 30; }
.skip:focus { top: 8px; }
.prose code, .prose pre { font-family: inherit; font-size: .93em; }
.prose code { background: var(--brand-strip); padding: .05em .3em; border: 1px solid var(--brand-rule); }
.prose pre { background: var(--brand-strip); border: 1px solid var(--brand-rule); padding: 12px 14px; overflow-x: auto; line-height: 1.45; margin: 0 0 1.1rem; }
.prose pre code { background: none; padding: 0; border: 0; font-size: inherit; }
.prose pre:focus-visible, .prose summary:focus-visible { outline: 2px solid var(--brand-green); outline-offset: 2px; }
.prose hr { display: none; }
.table { overflow-x: auto; margin: 0 0 1.2rem; }
.prose table { border-collapse: collapse; width: 100%; font-size: 13px; font-variant-numeric: tabular-nums; }
.prose th, .prose td { text-align: left; vertical-align: top; padding: .4rem .7rem .4rem 0; border-bottom: 1px solid var(--brand-rule); }
.prose th { font-weight: 400; font-size: 12.5px; color: var(--brand-muted); border-bottom-color: var(--brand-muted); }
.prose td:first-child { white-space: nowrap; }
.prose .lede { font-size: 14.5px; margin-bottom: 1.2rem; }
.status, .callout { background: var(--brand-strip); border: 1px solid var(--brand-rule); padding: 10px 14px; margin: 0 0 1.1rem; max-width: 78ch; }
.callout { border-left: 3px solid var(--brand-green); }
.status p, .callout p { margin: 0; }
.prose details { border: 1px solid var(--brand-rule); background: var(--brand-strip); padding: 0 14px; margin: 0 0 1.1rem; }
.prose details > summary { cursor: pointer; padding: 9px 0; }
.prose details[open] > summary { margin-bottom: .4rem; }
.prose details > :last-child { margin-bottom: 1rem; }
.swatch { display: inline-block; width: .7em; height: .7em; margin-right: .4em; vertical-align: .02em; box-shadow: 0 0 0 1px var(--brand-ink); }
.figs { display: flex; flex-wrap: wrap; gap: 16px; margin: .4rem 0 1.4rem; }
.prose figure { margin: 0; flex: 0 1 auto; }
.prose figure img { display: block; max-width: 100%; height: auto; }
.prose figure.shot img { width: 300px; image-rendering: pixelated; border: 1px solid var(--brand-rule); }
.prose figure.wide { flex-basis: 100%; }
.prose figure.wide img { width: 100%; }
.prose figcaption { color: var(--brand-muted); font-size: 12.5px; margin-top: .35rem; max-width: 300px; }
.prose figure.wide figcaption { max-width: none; }
/* the contents: down the side, staying in sight as the text goes by; first on a phone */
nav.contents { padding: 14px 16px 16px; font-size: 12.5px; line-height: 1.45; }
nav.contents ol { list-style: none; margin: 0; padding: 0; }
nav.contents li { margin: 0; }
nav.contents a { display: block; padding: 3px 0; color: var(--brand-ink); text-decoration: none; }
nav.contents a:hover { color: var(--brand-green); text-decoration: underline; }
@media (min-width: 48rem) { nav.contents { position: sticky; top: 3.5rem; max-height: calc(100vh - 4rem); overflow-y: auto; box-sizing: border-box; } }
@media (max-width: 47.99rem) { nav.contents ol { columns: 2 12rem; column-gap: 1.2rem; } nav.contents li { break-inside: avoid; } }
@media (prefers-reduced-motion: no-preference) { html { scroll-behavior: smooth; } }
"""
# --fragment: no site.css, so the page brings the little of it the text above stands on
FRAGMENT_CSS = """
:root { --brand-ground: #f3ead7; --brand-strip: #ece2cb; --brand-ink: #2b2219; --brand-muted: #5b4a3a; --brand-rule: #d9ccb1;
        --brand-green: #146c41; --brand-display: "Helvetica Neue", Helvetica, Arial, sans-serif; color-scheme: light; }
body { margin: 0; padding: 0 16px 48px; background: var(--brand-ground); color: var(--brand-ink);
       font: 400 15px/1.6 "Helvetica Neue", Helvetica, Arial, sans-serif; }
main { max-width: 46rem; margin: 0 auto; }
h1 { font-size: 20px; font-weight: 600; }
.lead { font: 600 1.1em/1 var(--brand-display); color: var(--brand-ink); }
"""


def on_the_bench(body):
    """The page's text as the site's bench has it: its title in the strip, the contents down the side."""
    h1 = re.search(r'(?:<a class="home"[^>]*>[^<]*</a>\n)?<h1>(.*?)</h1>\n', body)
    title = h1.group(1) if h1 else NAME
    body = body.replace(h1.group(0), "", 1) if h1 else body
    nav = re.search(r'<nav class="contents".*?</nav>\n(?:<hr>\n)?', body, re.S)
    side = ""
    if nav:
        body = body.replace(nav.group(0), "", 1)
        contents = nav.group(0).replace('<h2 id="contents">Contents</h2>', '<h2 class="bh" id="contents">Contents</h2>')
        contents = contents.replace("<hr>\n", "")
        side = f'<aside class="side">\n{contents}</aside>\n'
    strip = (f'<div class="strip">\n<h1 class="lab"><b>{title.lower()}</b> · {NAME}, Irish ambient for the M-VAVE FM-1</h1>\n'
             f'<a class="btn" href="../cheat/">cheat sheet</a>\n</div>\n')
    text = f'<article class="pane prose">\n{body}\n</article>\n'
    return strip + (f'<div class="cols">\n{side}{text}</div>\n' if side else text)


def inline(t):
    t = html.escape(t, quote=False)
    t = re.sub(r"`([^`]+)`", r"<code>\1</code>", t)
    t = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", t)
    t = re.sub(r"(?<![\w*])\*([^*\s][^*]*)\*(?![\w*])", r"<em>\1</em>", t)
    # a link up to another of the repo's documents (../EMULATOR.md) works on GitHub; from the site it goes to GitHub
    t = re.sub(r"\]\(\.\./([A-Z]+\.md)\)", r"](https://github.com/stephencummins/fm1-aisling/blob/main/\1)", t)
    t = re.sub(r"\]\(#(\d+)-[a-z0-9-]+\)", r"](#s\1)", t)    # (GitHub's section anchors, as slug() names them)
    t = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", lambda m: f'<a href="{m.group(2)}">{m.group(1)}</a>', t)
    for name, col in PART_COLOURS.items():          # a part named at the start of a table cell gets its swatch
        t = re.sub(rf"^<strong>{name}</strong>", f'<span class="swatch" style="background:{col}"></span><strong>{name}</strong>', t)
    return t


def lead(t):
    """Bold at the very start of a paragraph or a list item is its heading (web/brand/site.css)."""
    return re.sub(r"^<strong>", '<strong class="lead">', t)


def slug(title):
    m = re.match(r"(\d+)\.", title)
    return f"s{m.group(1)}" if m else re.sub(r"[^a-z0-9]+", "-", re.sub(r"['’]", "", title.lower())).strip("-")  # (as GitHub: no apostrophes)


NAME = "AISLING"
IMG_BASE = SRC / "docs"
EMBED = False                                         # --fragment: images as data URIs


def figure(alt, src, caption=None):
    caption = caption or alt
    path = IMG_BASE / src
    if EMBED:
        mime = "image/svg+xml" if src.endswith(".svg") else "image/png"
        url = f"data:{mime};base64," + base64.b64encode(path.read_bytes()).decode()
    else:
        url = src
    cls = "wide" if src.endswith(".svg") else "shot"
    return (f'<figure class="{cls}"><img src="{url}" alt="{html.escape(alt)}" loading="lazy">'
            f"<figcaption>{inline(caption)}</figcaption></figure>")


def convert(md):
    lines = md.splitlines()
    out, i, contents, sections = [], 0, None, {}
    for ln in lines:                                  # section numbers -> ids, for the contents
        m = re.match(r"## (\d+)\. (.+)", ln)
        if m:
            sections[m.group(2).strip()] = f"s{m.group(1)}"
    while i < len(lines):
        ln = lines[i]
        if ln.startswith("```"):
            j = i + 1
            while j < len(lines) and not lines[j].startswith("```"):
                j += 1
            # it may scroll sideways on a phone: the keyboard can reach it (tabindex), and it says what it is
            out.append('<pre tabindex="0" aria-label="Command"><code>' + html.escape("\n".join(lines[i + 1:j]), quote=False) +
                       "</code></pre>")
            i = j + 1
            continue
        if ln.startswith("!["):                      # consecutive image lines: one row of figures
            figs = []
            while i < len(lines) and (lines[i].startswith("![") or (not lines[i].strip() and i + 1 < len(lines)
                                                                    and lines[i + 1].startswith("!["))):
                m = re.match(r'!\[(.*)\]\((\S+)(?:\s+"(.*)")?\)', lines[i])
                if m:
                    figs.append(figure(m.group(1), m.group(2), m.group(3)))
                i += 1
            out.append('<div class="figs">' + "".join(figs) + "</div>")
            continue
        if ln.strip().startswith("<details>"):          # a toggle: <details><summary>…</summary> … </details>
            m = re.match(r"\s*<details>\s*<summary>(.*)</summary>\s*$", ln)
            out.append(f"<details><summary>{inline(m.group(1)) if m else ''}</summary>")
            i += 1
            continue
        if ln.strip() == "</details>":
            out.append("</details>")
            i += 1
            continue
        if ln.startswith("> "):                           # a callout: the important bit
            para = []
            while i < len(lines) and lines[i].startswith(">"):
                para.append(lines[i][1:].strip())
                i += 1
            out.append(f'<div class="callout"><p>{inline(" ".join(para))}</p></div>')
            continue
        if ln.startswith("# "):                         # the title; AISLING's own a way home (the site's root)
            t = ln[2:].strip()
            if t == NAME:                               # the site's bar is the page's way home
                out.append('<h1>Manual</h1>')
            else:
                out.append(f'<a class="home" href="../">{NAME}</a>\n<h1>{inline(t)}</h1>')
        elif ln.startswith("## "):
            t = ln[3:].strip()
            m = re.match(r"(\d+)\. (.+)", t)
            if t == "Contents":
                contents = True
                out.append('<nav class="contents" aria-label="Contents"><h2 id="contents">Contents</h2>')
            elif m:
                out.append(f'<h2 id="s{m.group(1)}"><span class="num">{m.group(1)}</span> {inline(m.group(2))}</h2>')
            else:
                out.append(f'<h2 id="{slug(t)}">{inline(t)}</h2>')
        elif ln.startswith("### "):
            out.append(f'<h3 id="{slug(ln[4:])}">{inline(ln[4:])}</h3>')
        elif ln.strip() == "---":
            if contents:
                out.append("</nav>")
                contents = None
            out.append("<hr>")
        elif ln.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].startswith("|"):
                rows.append([c.strip() for c in lines[i].strip().strip("|").split("|")])
                i += 1
            head, body = rows[0], [r for r in rows[1:] if not set("".join(r)) <= set("-: ")]
            out.append('<div class="table"><table><thead><tr>' + "".join(f"<th>{inline(c)}</th>" for c in head) +
                       "</tr></thead><tbody>" + "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>"
                                                         for r in body) + "</tbody></table></div>")
            continue
        elif re.match(r"(\d+\.|-) ", ln):
            ordered = ln[0].isdigit()
            items = []
            while i < len(lines) and (re.match(r"(\d+\.|-) ", lines[i]) or (lines[i].startswith("  ") and items)):
                if lines[i].startswith("  "):
                    items[-1] += " " + lines[i].strip()
                else:
                    items.append(re.sub(r"^(\d+\.|-) ", "", lines[i]))
                i += 1
            if contents:
                items = [f'<a href="#{sections.get(t.strip(), slug(t))}">{inline(t)}</a>' for t in items]
            else:
                items = [lead(inline(t)) for t in items]
            tag = "ol" if ordered else "ul"
            out.append(f"<{tag}>" + "".join(f"<li>{t}</li>" for t in items) + f"</{tag}>")
            continue
        elif ln.strip():
            para = [ln]
            while i + 1 < len(lines) and lines[i + 1].strip() and not re.match(r"(#|\||```|---|\d+\. |- |!\[|> |\s*</?details)", lines[i + 1]):
                i += 1
                para.append(lines[i])
            text = inline(" ".join(p.strip() for p in para))
            if text.startswith("<strong>Status.</strong>"):
                out.append(f'<div class="status"><p>{text}</p></div>')
            elif out and out[-1].startswith("<h1>"):
                out.append(f'<p class="lede">{text}</p>')
            else:
                out.append(f"<p>{lead(text)}</p>")
        i += 1
    return "\n".join(out)


def main():
    args = [a for a in sys.argv[1:] if a != "--fragment"]
    global EMBED
    fragment = "--fragment" in sys.argv
    EMBED = fragment
    src = Path(args[0]) if args else SRC / "docs" / "MANUAL.md"
    dst = Path(args[1]) if len(args) > 1 else SRC / "docs" / "manual.html"
    md = src.read_text()
    h1 = re.search(r"^# (.+)$", md, re.M)
    title, what = (f"Manual · {NAME}", "The manual") if not h1 or h1.group(1) == NAME else (f"{NAME}: {h1.group(1)}", h1.group(1))
    body = convert(md)
    if not fragment:
        body = on_the_bench(body)
    # the site's bar before <main>, and a way past it
    main_class = "" if fragment else ' class="bench"'
    skip = '<a class="skip" href="#content">Skip to content</a>\n'
    page = (f"<title>{html.escape(title)}</title>\n"
            f'<meta name="description" content="{html.escape(what)} for {NAME}, a generative Irish ambient instrument for the M-VAVE FM-1.">\n'
            f'<style>{(FRAGMENT_CSS if fragment else "") + CSS}</style>\n{skip}<!--AISLING-BAR-->\n'
            f'<main id="content"{main_class}>\n{body}\n</main>\n')
    if not fragment:
        head, _, rest = page.partition("</style>\n")
        page = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
                '<meta name="viewport" content="width=device-width, initial-scale=1">\n' + head + "</style>\n" +
                "<!--AISLING-HEAD-->\n</head>\n<body>\n" + rest + "</body>\n</html>\n")
        page = stamp(page, "../", "manual")
    else:
        page = page.replace("<!--AISLING-BAR-->\n", "")
    dst.write_text(page)
    print(f"{dst}: {len(page)} bytes")


if __name__ == "__main__":
    main()
