#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the SMK-37 Elite's site (tools/make_pages_smk37.py) and publish it to the gh-pages branch of
# stephencummins/smk37-aisling (git remote smk), replacing what was there (the branch holds only the generated site):
# https://stephencummins.github.io/smk37-aisling/. Only there: the FM-1's site is tools/publish_pages.sh's, and nothing
# here touches origin or Cloudflare.
#   sh tools/publish_pages_smk37.sh
set -e
cd "$(dirname "$0")/.."
REPO=stephencummins/smk37-aisling
REMOTE="$(git remote get-url smk 2>/dev/null)" || {
    echo "no git remote smk: git remote add smk https://github.com/$REPO.git"; exit 1; }
case "$REMOTE" in
    "https://github.com/$REPO.git" | "https://github.com/$REPO" | "git@github.com:$REPO.git") ;;
    *) echo "remote smk is $REMOTE, not $REPO: not published"; exit 1 ;;
esac
sh host/build_host.sh
python3 tools/make_pages_smk37.py build/pages_smk37
# nothing to install on this site until AISLING has run on an Elite
if find build/pages_smk37 \( -name '*.fwsc' -o -name '*.bin' -o -name '*.ufw' -o -name '*.zip' \) | grep -q .; then
    echo "the site has a firmware file in it: not published"; exit 1
fi
SHA="$(git rev-parse --short HEAD)"
cd build/pages_smk37
rm -rf .git
git init -q
git checkout -q -b gh-pages               # (not init -b: older gits lack it)
git add -A
git commit -q -m "Site for AISLING on the SMK-37 Elite (from $SHA)"
git push -q -f "$REMOTE" gh-pages
rm -rf .git
# Pages on, from gh-pages, the first time; then ask it to build now (a push alone has left a first deploy on "building").
# GitHub may turn Pages on by itself for a new gh-pages branch, just after the first check: so a refused POST (409,
# "already enabled") is checked again before it is reported.
gh api "repos/$REPO/pages" >/dev/null 2>&1 ||
    gh api -X POST "repos/$REPO/pages" -f "source[branch]=gh-pages" -f "source[path]=/" >/dev/null 2>&1 ||
    gh api "repos/$REPO/pages" >/dev/null 2>&1 ||
    echo "(could not turn GitHub Pages on: check the repository's Pages settings)"
gh api -X POST "repos/$REPO/pages/builds" >/dev/null 2>&1 || echo "(could not ask GitHub Pages to build: check its settings)"
echo "pushed the SMK-37 Elite's site to gh-pages of $REPO: https://stephencummins.github.io/smk37-aisling/"
