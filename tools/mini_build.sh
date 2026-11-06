#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build AISLING on the Mac Mini (its Docker runs the JieLi toolchain's linux/amd64 container; this machine has
# no Docker) and fetch the package back into build/.
#   tools/mini_build.sh [--release X.Y] [--board fm1|smk37]
#   MINI       the ssh host (default: mini)
#   MINI_DIR   the build copy there, relative to its home (default: fm1-aisling-build): a plain copy of this
#              working tree, not a checkout, so what is built is what is here, committed or not
# The Mini needs, once: tools/get_toolchain.sh (~/.jieli/toolchain), the AC79 SDK in ~/fw-AC79_AIoT_SDK
# (BUILDING.md) and python3 with Pillow.
set -e
cd "$(dirname "$0")/.."
MINI="${MINI:-mini}"
MINI_DIR="${MINI_DIR:-fm1-aisling-build}"
# the package's name, as tools/build.py gives it
board=fm1 rel= prev=
for a in "$@"; do
    case "$prev" in --board) board="$a" ;; --release) rel="$a" ;; esac
    case "$a" in --board=*) board="${a#--board=}" ;; --release=*) rel="${a#--release=}" ;; esac
    prev="$a"
done
case "$board" in
    fm1)   dir=build       name=amb.fwsc; [ -z "$rel" ] || name="aisling-$rel.fwsc" ;;
    smk37) dir=build/smk37 name=amb.fwsc; [ -z "$rel" ] || name="aisling-smk37-elite-$rel.fwsc" ;;
    *)     echo "mini_build: --board fm1|smk37" >&2; exit 2 ;;
esac
rsync -a --delete --exclude .git/ --exclude .venv/ --exclude build/ --exclude node_modules/ ./ "$MINI:$MINI_DIR/"
ssh "$MINI" "export PATH=/opt/homebrew/bin:/usr/local/bin:\$PATH; cd ~/$MINI_DIR && sh build.sh $*"
mkdir -p "$dir"
scp -q "$MINI:$MINI_DIR/$dir/$name" "$dir/$name"
echo "mini_build: $dir/$name"
