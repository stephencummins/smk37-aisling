#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# DUB-1 in the browser: the host simulator compiled to WebAssembly (Emscripten), plus its page.
#   [EN_VERSION=0.1] web/emu/build.sh  ->  build/emu/{index.html, worklet.js, amb.wasm, visualiser.html, reader/,
#                                                     manifest.webmanifest, sw.js, icons/}: the last three make it an app
#                                                     (from the home screen, or index.html?app: the FM-1 alone, full screen, offline)
# visualiser.html: AISLING's visualiser (web/visualiser/), the page's drawer, painting what the emulator plays.
# reader/: the screen reader's words and voice, from Charles Vestal's fm1-reader (github.com/charlesvestal/fm1-reader):
# words.js and tts.js (MIT, its LICENSE), and the voice, tts_worker.js and espeak/ (eSpeak NG in WebAssembly, with the
# wrapper's source and its build script: GPL-3.0-or-later), plus SOURCE.txt, saying where they come from. Exactly the
# files web/emu/reader.pin lists, checked against its sums, from build/fm1-reader (sh tools/get_reader.sh fetches the
# commit the pin names), a sibling checkout ../fm1-reader, or FM1_READER=a checkout; files that differ stop the build.
# Without one the emulator still works: the screen is said in the screen's own text, to the computer's screen reader.
# EMU_NEED_READER=1 makes that an error (tools/publish_pages.sh).
# Same sources and flags as host/build_host.sh (-ffp-contract=off, like the device).
set -e
cd "$(dirname "$0")/../.."
mkdir -p build/gen build/emu
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
python3 tools/gen_words.py firmware/words.tsv web/words build/gen/ann_words.h >/dev/null
emcc -O2 -ffp-contract=off -std=gnu99 -Wall -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable \
    -DEN_HOST -DEN_WEB "-DEN_VERSION=\"$(printf %s "${EN_VERSION:-DEV}" | tr a-z A-Z)\"" -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o build/emu/amb.wasm web/emu/amb_web.c firmware/src/dsp/amb.c
cp web/emu/index.html web/emu/worklet.js web/emu/manifest.webmanifest web/emu/sw.js build/emu/
mkdir -p build/emu/decals && cp web/emu/decals/*.webp build/emu/decals/    # the skins' art, for the Decal menu
python3 tools/make_app_icons.py build/emu/icons    # the home-screen icons: the site's harp
cp docs/card.png build/emu/card.png    # make_artifact.py's harp; on the site, the bar has it (tools/site_brand.py)
cp web/visualiser/index.html build/emu/visualiser.html
PIN=web/emu/reader.pin
if [ -n "$FM1_READER" ]; then READER="$FM1_READER"
elif [ -d build/fm1-reader/web ]; then READER=build/fm1-reader
else READER=../fm1-reader; fi
R=build/emu/reader
rm -rf "$R"
if [ -f "$READER/web/words.js" ]; then
    sed -e '/^#/d' -e '/^commit /d' -e '/^$/d' "$PIN" | (cd "$READER" && shasum -a 256 -c --quiet -) \
        || { echo "emu: $READER is not the fm1-reader $PIN names: sh tools/get_reader.sh fetches that one"; exit 1; }
    sed -e '/^#/d' -e '/^commit /d' -e '/^$/d' "$PIN" | while read -r sum f; do
        case "$f" in
            web/*) to="${f#web/}" ;;
            tools/build_espeak_web.sh) to=espeak/build_espeak_web.sh ;;   # how espeak.wasm was built: its source
            *) to="$f" ;;
        esac
        mkdir -p "$(dirname "$R/$to")"
        cp "$READER/$f" "$R/$to"
    done
    C="$(sed -n 's/^commit //p' "$PIN")"
    E="$(sed -n 's/^COMMIT=//p' "$READER/tools/build_espeak_web.sh")"
    [ ${#C} -eq 40 ] && [ ${#E} -eq 40 ] || { echo "emu: no commit for fm1-reader or eSpeak NG"; exit 1; }
    cat > "$R/SOURCE.txt" <<EOF
The emulator's screen reader: its words and its voice, from fm1-reader by Charles Vestal,
https://github.com/charlesvestal/fm1-reader/tree/$C

words.js and tts.js: MIT (LICENSE).

The voice, tts_worker.js and espeak/: GPL-3.0-or-later (espeak/LICENSE). espeak/espeak.js and
espeak/espeak.wasm are eSpeak NG, English only, compiled to WebAssembly from eSpeak NG's commit
$E,
https://github.com/espeak-ng/espeak-ng/tree/$E
with espeak/tts_wrap.c and espeak/build_espeak_web.sh, both here (espeak/README.md says how).

AISLING, and this page: GPL-3.0-only, https://github.com/stephencummins/fm1-aisling
EOF
    echo "emu: the screen reader's voice, fm1-reader $(printf %s "$C" | cut -c1-8)"
else
    [ -z "$EMU_NEED_READER" ] || { echo "emu: no fm1-reader at $READER: sh tools/get_reader.sh fetches it"; exit 1; }
    echo "emu: no fm1-reader at $READER (sh tools/get_reader.sh fetches it): the screen reader says the screen's own text, with no voice of its own"
fi
echo "emu: build/emu ($(wc -c < build/emu/amb.wasm) B wasm)"
