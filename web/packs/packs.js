// SPDX-License-Identifier: GPL-3.0-only
// The Packs page, a pack manager: the site's packs (index.json, from tools/make_pack.py by tools/make_pages.py) and
// any added from a file (kept in this browser), each heard in the page (worklet.js: the editor's firmware with the
// pack in its store) and put on an FM-1 or taken off it over the USB link (../editor/link.js: area 10). What is on
// the FM-1 is read back and named; before a pack leaves it, its eight pieces as the player left them are read
// (areas 1-8, the pack's bank) and kept here, to bring back when that pack goes on again.
// The track browser ("On your FM-1") lists the sixteen places: AISLING's own eight, which stay, and the pack's eight,
// which are the player's to change. A piece removed or added there is a change waiting (S.mix); putting it on the
// FM-1 makes a new pack of the eight in the page (packfile.js: write) and installs it, and the pieces that stay
// are written back as the player left them, wherever they now sit.
(function () {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const el = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text != null) e.textContent = text; return e; };
  const PF = globalThis.AislingPackFile, LK = globalThis.AislingLink;
  const S = { packs: [], link: null, hello: null, state: null, dev: null, busy: false, ac: null, node: null, loaded: null, playing: null,
    wasm: null, edits: {}, pristine: null, level: 2, mix: null, mixOf: "" };
  const OWN = ["Skellig", "Burren", "Aran", "Carolan", "Blasket", "Samhain", "Connemara", "Tir na nOg"];   // (amb.c's PIECES)
  const MIX = "My mix";
  const WHY = { 2: "The FM-1 refused it: this pack needs a newer AISLING than the FM-1 has.", 3: "It arrived damaged. Try again.",
    5: "There is no pack on the FM-1." };
  const STOP = "Press PLAY on the FM-1 to stop it, and let the sound die away.";
  const SHELF = "aisling-packs-shelf", EDITS = "aisling-packs-edits", PRISTINE = "aisling-packs-pristine", KEEP = 3;
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));
  const b64 = (b) => { let s = ""; for (let i = 0; i < b.length; i++) s += String.fromCharCode(b[i]); return btoa(s); };
  const unb64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));
  const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
  const day = (iso) => { const d = new Date(iso); return isNaN(d) ? "an earlier day" : d.toLocaleDateString(undefined, { day: "numeric", month: "long", year: "numeric" }); };
  // this browser's memory: never needed for the page to work (a private window has none, or it is full)
  const store = {
    get(k, d) { try { const v = localStorage.getItem(k); return v ? JSON.parse(v) : d; } catch (e) { return d; } },
    set(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); return true; } catch (e) { return false; } },
  };
  const keyOf = (name, version, bytes) => `${name}|${version}|${LK.sum(bytes)}`;

  /* ---------- the packs ---------- */
  function fromSite(j) {                             // index.json's: its words, its icons, its bytes
    const bytes = unb64(j.data);
    return Object.assign({}, j, { bytes, key: keyOf(j.name, j.version, bytes), mine: false, raws: PF.read(bytes, { level: 255 }).pieces.map((pc) => pc.raw) });
  }
  function fromBytes(bytes, info) {                  // a file's, or one read off an FM-1: what its bytes say
    const key = keyOf(info.name, info.version, bytes);
    return { id: `mine-${LK.sum(bytes).toString(36)}`, name: info.name, version: info.version, level: info.level, needs: "", bytes, key, mine: true,
      raws: info.pieces.map((pc) => pc.raw), about: "Added to this browser from a file or an FM-1.",
      pieces: info.pieces.map((pc) => Object.assign({ icon: "", about: pc.air ? "With an air of its own." : "" }, pc)) };
  }
  function shelve() { return store.set(SHELF, S.packs.filter((p) => p.mine).map((p) => b64(p.bytes))); }
  function add(bytes) {                              // a pack's bytes into My packs: the pack, or why not (a string)
    const info = PF.read(bytes, { level: 255 });
    if (info.why) return info.reason;
    const was = S.packs.find((p) => p.key === keyOf(info.name, info.version, bytes));
    if (was) return was;
    const p = fromBytes(bytes, info);
    S.packs.push(p);
    $("packs").append(card(p));
    p.kept = shelve();
    return p;
  }

  /* ---------- the device ---------- */
  function devText() {
    const h = S.hello;
    if (!S.link || !h) return "No FM-1 connected. You can listen without one.";
    const old = S.packs.some((p) => p.level > h.level) ? " Its AISLING is older than a pack here needs: update it first (the link is at the foot of the page)." : "";
    return `FM-1 connected: AISLING ${h.version}.${old}`;
  }
  const savedFor = (key) => (S.edits[key] || [])[0] || null;
  function fits(e) { return !!(e && S.hello && e.format === S.hello.format && e.npars === S.hello.npars && e.slot === S.hello.slot && e.slots.length === 8); }
  function fm1Text() {
    const d = S.dev, h = S.hello;
    if (!h) return "";
    if (!h.pack) return "No pack is on it: its own eight pieces only. Install one from the list below.";
    const where = h.bank ? " Its pieces are playing now." : " The FM-1's own eight are playing; turn PRESET past the eighth for the pack's.";
    if (!d) return `${h.pack.name}, version ${h.pack.version}, is on it.${where}`;
    const newer = S.packs.find((p) => p.name === d.name && p.version > d.version);
    const what = d.pack ? (d.pack.mine ? "one of your own packs" : "the same as in the list below")
      : newer ? `an older one than version ${newer.version} below: Install puts the new one on` : "a pack this page doesn't have: copy it to My packs to keep it";
    const e = savedFor(d.key);
    return `${d.name}, version ${d.version}, is on it: ${what}.${where}${e ? ` Its pieces as you left them on ${day(e.saved)} are kept in this browser.` : ""}`;
  }
  /* ---------- the track browser: the sixteen places ---------- */
  const emptyPlace = () => ({ raw: PF.empty(), name: "", from: -1, level: 1 });
  const devKey = () => S.dev ? S.dev.key : "";
  function onDevice() {                              // the pack's eight as the FM-1 has them: null if it has one unread
    if (!S.hello) return null;
    if (!S.hello.pack) return Array.from({ length: 8 }, emptyPlace);
    if (!S.dev) return null;
    return S.dev.pieces.map((pc, i) => PF.isEmpty(pc.raw) ? emptyPlace() : Object.assign({}, pc, { from: i, level: S.dev.level }));
  }
  function places() {                                // what the list shows: the change waiting, or the FM-1 as it is
    if (S.mix && S.mixOf !== devKey()) S.mix = null; // (the FM-1's pack changed under it: nothing to change any more)
    return S.mix || onDevice();
  }
  const sameRaw = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
  function settle(mix) {                             // a change that comes to no change is none
    const base = onDevice();
    S.mix = mix.every((e, i) => e.from === base[i].from && sameRaw(e.raw, base[i].raw)) ? null : mix;
    S.mixOf = devKey();
    render();
  }
  function removePlace(i) {
    const now = places();
    if (!now || S.busy) return;
    const mix = now.slice();
    mix[i] = emptyPlace();
    $("mix-note").textContent = "";
    settle(mix);
  }
  function addPiece(p, i) {                          // piece i of pack p into the first free place
    const now = places();
    if (!now || S.busy) return;
    const k = now.findIndex((e) => PF.isEmpty(e.raw));
    if (k < 0) { say(p, "All eight of the pack's places on the FM-1 are taken: remove a piece in the list at the top first."); return; }
    const raw = p.raws[i], base = onDevice();
    // one the FM-1 has already, taken out a moment ago: the same piece, so it keeps what the player made of it
    const from = base.findIndex((e, j) => e.from >= 0 && sameRaw(e.raw, raw) && !now.some((n) => n.from === j));
    const mix = now.slice();
    mix[k] = Object.assign({}, p.pieces[i], { raw, from, level: p.level });
    say(p, `${p.pieces[i].name} is in place ${9 + k} of the list at the top. "Put these on the FM-1" there makes it so.`);
    $("mix-note").textContent = "";
    settle(mix);
  }
  function renderPlaces() {
    const now = places(), base = onDevice(), list = $("fm1-rows");
    $("fm1-tracks").hidden = !now;
    if (!now) return;
    const rows = OWN.map((name, i) => {
      const li = el("li", "pk-row own");
      li.append(el("span", "pk-n", String(i + 1)), el("span", "pk-rname", name), el("span", "pk-rmeta", "AISLING's own"));
      return li;
    });
    now.forEach((e, i) => {
      const none = PF.isEmpty(e.raw), changed = e.from !== base[i].from || !sameRaw(e.raw, base[i].raw);
      const li = el("li", `pk-row${none ? " none" : ""}${changed ? " changed" : ""}`);
      li.append(el("span", "pk-n", String(9 + i)), el("span", "pk-rname", none ? "Empty" : e.name));
      const was = changed && !PF.isEmpty(base[i].raw) ? base[i].name : "";
      li.append(el("span", "pk-rmeta", none ? (was ? `${was} removed` : "add a piece from a pack below")
        : `${e.key} ${e.mode} · ${e.tempo} bpm${changed ? (e.from >= 0 ? " · moved" : " · new") : ""}`));
      if (!none) {
        const b = el("button", "btn pk-rm", "Remove");
        b.setAttribute("aria-label", `Remove ${e.name}`);
        b.disabled = S.busy;
        b.onclick = () => removePlace(i);
        li.append(b);
      }
      rows.push(li);
    });
    list.replaceChildren(...rows);
    $("mix-go").hidden = $("mix-undo").hidden = !S.mix;
    $("mix-go").disabled = $("mix-undo").disabled = S.busy;
    const n = S.mix ? S.mix.filter((e, i) => e.from !== base[i].from || !sameRaw(e.raw, base[i].raw)).length : 0;
    $("mix-what").textContent = S.mix ? `${n === 1 ? "One place is" : `${n} places are`} changed here, and not yet on the FM-1.`
      : "The first eight are AISLING's own and stay. The other eight are the pack's, and yours to change: remove one here, add one from any pack below.";
  }
  function render() {
    const h = S.hello, d = S.dev, on = !!(S.link && h);
    $("dev").textContent = devText();
    $("connect").textContent = S.link ? "Disconnect" : "Connect your FM-1";
    $("fm1").hidden = !on;
    if (on) {
      const has = !!h.pack, e = d && savedFor(d.key);
      $("fm1-what").textContent = fm1Text();
      for (const id of ["fm1-go", "fm1-keep", "fm1-file", "fm1-off"]) { $(id).hidden = !has; $(id).disabled = S.busy; }
      $("fm1-go").textContent = h.bank ? "Back to the eight" : "Play the pack";
      $("fm1-back").hidden = !fits(e);
      $("fm1-back").disabled = S.busy;
      $("fm1-load").hidden = !has;
      $("fm1-load").disabled = S.busy;
      $("fm1-copy").hidden = !(d && !d.pack);
      $("fm1-copy").disabled = S.busy;
      renderPlaces();
    }
    const now = on ? places() : null, free = !!now && now.some((e) => PF.isEmpty(e.raw));
    for (const p of S.packs) {
      const here = d && d.key === p.key, old = h && h.level < p.level;
      p.ui.install.disabled = !S.link || S.busy || old;
      p.ui.install.textContent = here ? "Install again" : d && d.name === p.name ? "Put this version on the FM-1" : "Install on the FM-1";
      p.ui.tag.textContent = here ? "On your FM-1" : p.mine ? "Yours" : "";
      p.ui.tag.hidden = !p.ui.tag.textContent;
      for (const [i, b] of p.ui.adds.entries()) {
        const there = !!now && now.some((e) => sameRaw(e.raw, p.raws[i]));
        b.hidden = !now;
        b.disabled = S.busy || old || there || !free;
        b.textContent = there ? (S.mix ? "In the list above" : "On your FM-1") : free ? "Add to my FM-1" : "No free place";
      }
      for (const [i, b] of p.ui.plays.entries()) {
        const mine = S.playing && S.playing.pack === p && S.playing.i === i;
        b.setAttribute("aria-pressed", mine ? "true" : "false");
        b.textContent = mine ? "■" : "▶";
        b.setAttribute("aria-label", `${mine ? "Stop" : "Hear"} ${p.pieces[i].name}`);
      }
    }
  }
  const say = (p, text) => { p.ui.note.textContent = text; };
  const sayDev = (text) => { $("fm1-note").textContent = text; };

  async function connect() {
    if (S.link) { S.link.close(); S.link = null; S.hello = null; S.dev = null; S.state = null; render(); return; }
    if (!navigator.requestMIDIAccess) { $("dev").textContent = "Connecting needs Chrome or Edge: this browser has no USB MIDI. You can still listen."; return; }
    $("dev").textContent = "Looking for an FM-1. The browser may ask to allow MIDI.";
    let access;
    try { access = await navigator.requestMIDIAccess({ sysex: true }); }
    catch (e) { $("dev").textContent = "MIDI wasn't allowed, so nothing is connected. You can allow it from the padlock by the address."; return; }
    const ports = LK.findPorts(access);
    if (!ports) { $("dev").textContent = "No FM-1 found. Plug it in over USB with AISLING running, then connect again."; return; }
    const link = new LK(ports.input, ports.output, {
      onState: (st) => { if (S.link === link) S.state = st; },
      onGone: () => { if (S.link === link) { S.link = null; S.hello = null; S.dev = null; S.state = null; render(); $("dev").textContent = "The FM-1 has gone: the cable, or it stopped answering."; } },
    });
    const hello = await link.open();
    if (!hello) { $("dev").textContent = "This FM-1 doesn't answer. Packs need AISLING 1.5 or later on it: update it first."; return; }
    S.link = link;
    S.hello = hello;
    sayDev("");
    S.busy = true;
    render();
    await look();
    S.busy = false;
    render();
  }
  async function look() {                            // what is on it: the pack's own bytes, read back and named
    S.dev = null;
    const bytes = S.link && S.hello && S.hello.pack ? await S.link.pack() : null;
    if (bytes) {
      const info = PF.read(bytes, { level: 255 });
      if (!info.why) {
        const key = keyOf(info.name, info.version, bytes);
        S.dev = { bytes, key, name: info.name, version: info.version, level: info.level, pieces: info.pieces, pack: S.packs.find((p) => p.key === key) || null };
      }
    }
    render();
  }
  async function refresh() {
    const h = S.link && await S.link.again();
    if (h) S.hello = h;
    else if (S.link) { S.link = null; S.hello = null; S.dev = null; }
    await look();
  }
  // the bank can't be changed under music, nor a pack kept or dropped: wait for the FM-1 to stop (STATE says)
  async function stopped(tell, what) {
    for (let n = 0; n < 80; n++) {
      if (!S.link) return false;
      if (S.state && !S.state.playing) return true;
      if (S.state) tell(`${what}: waiting for the FM-1 to stop. ${STOP}`);
      await wait(500);
    }
    return false;
  }
  // a transfer the FM-1 only takes at rest: asked again while it says it is busy (its sound dying away)
  async function atRest(tell, what, fn) {
    for (let n = 0; n < 40; n++) {
      const s = await fn();
      if (s !== 4) return s;
      tell(`${what}: waiting for the FM-1 to be still. ${STOP}`);
      await wait(1000);
      if (!S.link) return -1;
    }
    return 4;
  }

  /* ---------- the pack's pieces as the player left them ---------- */
  // the eight slots of the pack's bank (areas 1-8 are the bank's that plays): null if it can't be read
  async function readPieces() {
    const prev = S.hello.bank;
    if (!prev && await S.link.bank(1) !== 0) return null;
    const slots = [];
    for (let i = 0; i < 8 && slots; i++) {
      const b = await S.link.read(1 + i);
      if (!b || b.length !== S.hello.slot) { if (!prev) await S.link.bank(0); return null; }
      slots.push(b);
    }
    if (!prev) await S.link.bank(0);
    return slots;
  }
  // what makes a piece the player's: its settings and its scale, not the patterns it happens to have grown
  const settings = (slots) => { const n = 2 * S.hello.npars + 2, o = new Uint8Array(8 * n); slots.forEach((b, i) => o.set(b.subarray(0, n), i * n)); return o; };
  function entry(slots) {
    return { name: S.dev.name, version: S.dev.version, format: S.hello.format, npars: S.hello.npars, slot: S.hello.slot,
      saved: new Date().toISOString(), slots: slots.map(b64) };
  }
  // before the pack on the FM-1 changes or goes: its pieces kept here, unless they are still as the pack made them.
  // "kept", "same" (nothing of the player's to keep), "none" (no pack), or "failed"
  async function keep(tell, what, always, got) {
    if (!S.link || !S.hello || !S.hello.pack) return "none";
    if (!S.dev) await look();                        // (not named yet: read it first, or its pieces have no home)
    if (!S.dev) return "failed";
    if (!await stopped(tell, what)) return "failed";
    const slots = await readPieces();
    if (!slots) return "failed";
    if (got) got.slots = slots;
    const now = settings(slots);
    if (!always && S.pristine && S.pristine.key === S.dev.key && same(unb64(S.pristine.pars), now)) return "same";
    const list = S.edits[S.dev.key] || [];
    if (list[0] && same(settings(list[0].slots.map(unb64)), now)) return "kept";   // (already here, as they are)
    S.edits[S.dev.key] = [entry(slots), ...list].slice(0, KEEP);
    return store.set(EDITS, S.edits) ? "kept" : "memory";
  }
  async function noteNew() {                         // a pack just installed: its pieces as it made them, to tell changes by
    const slots = S.dev && await readPieces();
    S.pristine = slots ? { key: S.dev.key, pars: b64(settings(slots)) } : null;
    store.set(PRISTINE, S.pristine);
  }
  async function bringBack(e, tell) {                // a kept set written back, piece by piece: Tune, Lights and the rest stay
    if (!fits(e)) { tell("These pieces were saved from a different AISLING, and its settings don't line up with this one's."); return false; }
    if (!await stopped(tell, "Bringing them back")) { tell("The FM-1 didn't stop, so nothing was changed."); return false; }
    const prev = S.hello.bank;
    if (!prev && await S.link.bank(1) !== 0) { tell("The FM-1 didn't answer, so nothing was changed."); return false; }
    let ok = true;
    for (let i = 0; i < 8 && ok; i++) ok = await S.link.write(1 + i, unb64(e.slots[i])) === 0;
    if (ok) S.link.post({ type: "save" });
    if (!prev) await S.link.bank(0);
    tell(ok ? `Your pieces from ${day(e.saved)} are back on the FM-1.` : "The FM-1 refused them part-way: some pieces are as you left them, the rest as the pack made them.");
    return ok;
  }

  /* ---------- on, off, and between ---------- */
  async function install(p) {
    if (!S.link || S.busy) return;
    const tell = (t) => say(p, t);
    S.busy = true;
    render();
    const was = S.dev ? S.dev.name : S.hello.pack ? S.hello.pack.name : "";
    const k = await keep(tell, "Before installing");
    S.kept = k;                                    // (for a test to look at)
    if (k === "failed" && !confirm(`The pieces of ${was} as you left them couldn't be read off the FM-1. Install ${p.name} anyway? They would be lost.`)) {
      S.busy = false;
      tell("Nothing was changed.");
      render();
      return;
    }
    tell(`Sending ${p.name} to the FM-1.`);
    const s = await atRest(tell, "Installing", () => S.link.install(p.bytes));
    if (s === 0) {
      await refresh();
      await noteNew();
      const e = savedFor(p.key), keptOld = k === "kept" || k === "memory" ? ` Your changes to ${was} are kept in this browser.` : "";
      tell(`${p.name} is on the FM-1. Turn PRESET past the eighth piece to play it.${keptOld}${fits(e) ? ` Your own pieces from ${day(e.saved)} can come back: "Bring back my pieces", above.` : ""}`);
    } else {
      tell(s === 4 ? "The FM-1 never came to rest, so nothing was changed. Stop it and try again." : WHY[s] || "The FM-1 stopped answering, so nothing was changed.");
      await refresh();
    }
    S.busy = false;
    render();
  }
  async function takeOff() {
    if (!S.link || S.busy) return;
    const name = S.hello.pack.name;
    S.busy = true;
    render();
    const k = await keep(sayDev, "Before taking it off");
    if (k === "failed" && !confirm(`The pieces of ${name} as you left them couldn't be read off the FM-1. Take it off anyway? They would be lost.`)) {
      S.busy = false;
      sayDev("Nothing was changed.");
      render();
      return;
    }
    const s = await atRest(sayDev, "Taking it off", () => S.link.remove());
    S.busy = false;
    sayDev(s === 0 ? `${name} is off the FM-1. Its own eight are untouched.${k === "kept" || k === "memory" ? " Your changes to its pieces are kept in this browser, for when it goes on again." : ""}`
      : "Nothing was changed: the FM-1 wasn't still, or stopped answering.");
    await refresh();
  }
  // the change waiting, made so: a pack of the eight built here and installed, then the pieces that stay written back
  // as the player left them (a pack going on sets its bank to its recipes: project.c's pack_install)
  async function mixGo() {
    const mix = S.mix, tell = (t) => { $("mix-note").textContent = t; };
    if (!S.link || S.busy || !mix) return;
    if (mix.every((e) => PF.isEmpty(e.raw))) {       // nothing left in it: that is the pack taken off
      S.mix = null;
      tell("");
      if (S.hello.pack) await takeOff(); else render();
      return;
    }
    const whole = S.packs.find((p) => p.raws.every((r, i) => sameRaw(r, mix[i].raw)));   // (a pack as it is made: itself)
    const bytes = whole ? whole.bytes : PF.write({ name: MIX, version: 1, level: mix.reduce((m, e) => Math.max(m, e.level), 1), pieces: mix.map((e) => e.raw) });
    if (typeof bytes === "string") { tell(bytes); return; }
    S.busy = true;
    render();
    const got = {}, was = S.hello.pack ? S.hello.pack.name : "";
    const k = await keep(tell, "Before changing the pack", false, got);
    if (k === "failed" && !confirm(`The pieces of ${was} as you left them couldn't be read off the FM-1. Change the pack anyway? What you made of them would be lost.`)) {
      S.busy = false;
      tell("Nothing was changed.");
      render();
      return;
    }
    if (k !== "failed" && k !== "none" && !got.slots) got.slots = await readPieces();
    tell("Sending the eight to the FM-1.");
    const s = await atRest(tell, "Changing the pack", () => S.link.install(bytes));
    if (s !== 0) {
      tell(s === 4 ? "The FM-1 never came to rest, so nothing was changed. Stop it and try again." : WHY[s] || "The FM-1 stopped answering, so nothing was changed.");
      await refresh();
      S.busy = false;
      render();
      return;
    }
    S.mix = null;
    await refresh();
    const fresh = S.dev && await readPieces();       // the eight as the pack made them: to tell changes by, from here on
    S.pristine = fresh ? { key: S.dev.key, pars: b64(settings(fresh)) } : null;
    store.set(PRISTINE, S.pristine);
    const stay = mix.map((e, i) => [i, e.from]).filter(([i, j]) => j >= 0 && got.slots && fresh && !same(got.slots[j], fresh[i]));
    let ok = !!fresh || !got.slots;
    if (stay.length && await S.link.bank(1) === 0) {
      for (const [i, j] of stay) ok = ok && await S.link.write(1 + i, got.slots[j]) === 0;
      if (ok) S.link.post({ type: "save" });
      await S.link.bank(0);
    } else if (stay.length) ok = false;
    const n = mix.filter((e) => !PF.isEmpty(e.raw)).length, name = S.dev ? S.dev.name : MIX;
    tell(`Done: ${name} is on the FM-1, with ${n === 8 ? "all eight places" : `${n} of its eight places`} filled. Turn PRESET past the eighth piece to play it.`
      + (!ok ? " The pieces that stayed could not all be written back as you left them: they are as their pack made them. What you had made of them is kept in this browser all the same."
        : stay.length ? " The pieces that stayed are as you left them." : ""));
    await refresh();
    S.busy = false;
    render();
  }
  function mixUndo() { S.mix = null; $("mix-note").textContent = ""; render(); }

  async function act(fn) {                           // one thing at a time on the wire
    if (!S.link || S.busy) return;
    S.busy = true;
    render();
    try { await fn(); } finally { S.busy = false; render(); }
  }
  const go = () => act(async () => {
    const s = await S.link.bank(S.hello.bank ? 0 : 1);
    if (s !== 0) sayDev(WHY[s] || "The FM-1 didn't answer.");
    await refresh();
  });
  const keepNow = () => act(async () => {
    const k = await keep(sayDev, "Keeping your pieces", true);
    sayDev(k === "kept" ? "Your pieces as they are now are kept in this browser." : k === "memory" ? "They were read, but this browser has no room to keep them: save them to a file instead."
      : "They couldn't be read: the FM-1 didn't stop, or didn't answer.");
  });
  const backNow = () => act(async () => { await bringBack(savedFor(S.dev.key), sayDev); await refresh(); });
  function download(name, bytes, type) {
    const a = el("a"), url = URL.createObjectURL(new Blob([bytes], { type }));
    a.href = url;
    a.download = name;
    document.body.append(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }
  const fileName = (s) => s.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "") || "pack";
  const toFile = () => act(async () => {             // the pieces as they are now, as a file to keep or pass on
    if (!await stopped(sayDev, "Saving your pieces")) { sayDev("The FM-1 didn't stop, so nothing was read."); return; }
    const slots = await readPieces();
    if (!slots) { sayDev("They couldn't be read off the FM-1."); return; }
    const e = entry(slots);
    download(`${fileName(S.dev.name)}-my-pieces.json`, JSON.stringify(Object.assign({ type: "aisling-pack-pieces", pack: S.dev.key }, e)), "application/json");
    sayDev("Saved as a file. \"Load pieces from a file\" puts them back on any FM-1 with this pack.");
  });
  async function fromFile(file) {
    let e;
    try { e = JSON.parse(await file.text()); } catch (x) { e = null; }
    if (!e || e.type !== "aisling-pack-pieces" || !Array.isArray(e.slots)) { sayDev("That isn't a file of pieces saved from this page."); return; }
    if (!S.dev || e.pack !== S.dev.key) { sayDev(`Those pieces are for ${e.name || "another pack"}, version ${e.version}: put that pack on the FM-1 first.`); return; }
    await act(async () => { await bringBack(e, sayDev); await refresh(); });
  }
  function copyOff() {
    const p = S.dev && add(S.dev.bytes);
    if (typeof p === "string" || !p) { sayDev(p || "There is nothing to copy."); return; }
    S.dev.pack = p;
    sayDev(`${p.name} is in My packs now${p.kept === false ? ", until this page is closed: this browser has no room to keep it" : ""}.`);
    render();
  }

  /* ---------- hearing one ---------- */
  async function hear(p, i) {
    if (S.playing && S.playing.pack === p && S.playing.i === i) { stop(); return; }
    try {
      if (!S.ac) {
        S.ac = new AudioContext({ sampleRate: 44100, latencyHint: "playback" });
        await S.ac.audioWorklet.addModule("../editor/engine.js");
        await S.ac.audioWorklet.addModule("worklet.js");
      }
      await S.ac.resume();
      if (!S.wasm) {
        const res = await fetch("../editor/amb.wasm");
        if (!res.ok) throw new Error(`amb.wasm ${res.status}`);
        S.wasm = await res.arrayBuffer();
      }
      if (S.loaded !== p) {                          // another pack: a firmware started afresh with it in its flash
        if (S.node) S.node.disconnect();
        const node = new AudioWorkletNode(S.ac, "aisling-pack", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
        const ready = new Promise((res) => { node.port.onmessage = (e) => { if (e.data.type === "ready") res(e.data); }; });
        node.port.postMessage({ type: "load", wasm: S.wasm.slice(0), pack: p.bytes, obj: 2, master: 2800 });
        const r = await ready;
        if (!r.ok) throw new Error("the page's copy of AISLING is older than this pack");
        node.connect(S.ac.destination);
        S.node = node;
        S.loaded = p;
      }
      S.node.port.postMessage({ type: "play", i });
      if (S.playing && S.playing.pack !== p) say(S.playing.pack, "");
      S.playing = { pack: p, i };
      say(p, `Playing ${p.pieces[i].name} in this page. It builds slowly: give it a minute.`);
    } catch (e) {
      say(p, `It can't play here: ${e.message}.`);
      S.playing = null;
    }
    render();
  }
  function stop() {
    if (S.node) S.node.port.postMessage({ type: "stop" });
    if (S.playing) say(S.playing.pack, "");
    S.playing = null;
    render();
  }

  /* ---------- the page ---------- */
  function forget(p) {                               // one of the player's own, off the shelf (not off the FM-1)
    if (!confirm(`Take ${p.name} out of My packs? If it is on the FM-1 it stays there.`)) return;
    if (S.playing && S.playing.pack === p) stop();
    S.packs = S.packs.filter((q) => q !== p);
    p.ui.sec.remove();
    if (S.dev && S.dev.pack === p) S.dev.pack = null;
    shelve();
    render();
  }
  function card(p) {
    const sec = el("section", "pk-pack");
    sec.setAttribute("aria-labelledby", `pk-${p.id}`);
    const h = el("h2", null, p.name), tag = el("span", "pk-tag");
    h.id = `pk-${p.id}`;
    h.append(" ", tag);
    const acts = el("div", "pk-acts"), list = el("ul", "pk-list");
    p.ui = { sec, tag, install: el("button", "btn go", "Install on the FM-1"), file: el("button", "btn", "Download the pack"),
      note: el("p", "lab pk-note"), plays: [], adds: [] };
    p.ui.note.setAttribute("role", "status");
    p.ui.install.onclick = () => install(p);
    p.ui.file.onclick = () => download(`${fileName(p.name)}-v${p.version}.aispack`, p.bytes, "application/octet-stream");
    const share = el("button", "btn", "Copy a link to it");
    share.onclick = () => copyLink(p);
    acts.append(p.ui.install, p.ui.file, share);
    if (p.mine) {
      const out = el("button", "btn", "Take it out of My packs");
      out.onclick = () => forget(p);
      acts.append(out);
    }
    p.pieces.forEach((pc, i) => {
      const li = el("li", `pk-piece ${pc.side}`), b = el("button", "pk-play", "▶");
      b.onclick = () => hear(p, i);
      p.ui.plays.push(b);
      const NS = "http://www.w3.org/2000/svg", ico = document.createElementNS(NS, "svg"), path = document.createElementNS(NS, "path");
      ico.setAttribute("viewBox", "0 0 24 24");      // the piece's own icon: a line drawing, the pack's (index.json)
      ico.setAttribute("class", "pk-ico");
      ico.setAttribute("aria-hidden", "true");
      path.setAttribute("d", pc.icon || "M12 4v16M4 12h16");
      ico.append(path);
      li.append(b, ico, el("h3", null, pc.name),
        el("p", "pk-meta", `${(p.sides || ["Light", "Dark"])[pc.side === "saint" ? 0 : 1]} · ${pc.key} ${pc.mode} · ${pc.tempo} bpm`));
      if (pc.about) li.append(el("p", "pk-what", pc.about));
      const plus = el("button", "btn pk-addp", "Add to my FM-1");
      plus.hidden = true;
      plus.setAttribute("aria-label", `Add ${pc.name} to my FM-1`);
      plus.onclick = () => addPiece(p, i);
      p.ui.adds.push(plus);
      li.append(plus);
      list.append(li);
    });
    sec.append(h, el("p", "pk-about", `${p.about} Version ${p.version}.`), acts, p.ui.note, list);
    return sec;
  }
  async function addFiles(files) {
    const said = [];
    for (const f of files) {
      let p;
      try { p = f.size > PF.MAX ? "It is too big to be a sound pack." : add(new Uint8Array(await f.arrayBuffer())); }
      catch (e) { p = "It couldn't be read."; }
      said.push(typeof p === "string" ? `${f.name}: ${p}` : `${p.name}, version ${p.version}, is in My packs${p.kept === false ? " until this page is closed (this browser has no room to keep it)" : ""}.`);
    }
    $("add-note").textContent = said.join(" ");
    render();
  }
  /* ---------- a pack by a link ---------- */
  // packs/?add=URL fetches a .aispack (this site's, or any https address that lets other sites read it); packs/#pack=...
  // carries a whole pack in the link itself (base64url: a pack is under 4 KB), so one can be passed on with nowhere to
  // put a file. Either way it is checked as a file is and only joins My packs: nothing goes to an FM-1 by a link.
  const b64url = (b) => b64(b).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
  const unb64url = (t) => unb64(t.replace(/-/g, "+").replace(/_/g, "/"));
  function linkTo(p) {
    const here = location.origin + location.pathname;
    return p.mine ? `${here}#pack=${b64url(p.bytes)}` : `${here}#pk-${p.id}`;
  }
  async function copyLink(p) {
    const url = linkTo(p);
    try { await navigator.clipboard.writeText(url); say(p, p.mine ? "A link to this pack is copied: the whole pack is in the link itself." : "A link to this pack is copied."); }
    catch (e) { prompt("Copy this link to the pack:", url); }
  }
  async function fromLinks() {
    const q = new URLSearchParams(location.search), h = new URLSearchParams(location.hash.slice(1)), said = [];
    const told = (name, p) => said.push(typeof p === "string" ? `${name}: ${p}` : `${p.name}, version ${p.version}, came by a link and is in My packs${p.kept === false ? " until this page is closed" : ""}.`);
    for (const t of h.getAll("pack")) {
      let p;
      try { p = t.length > 2 * PF.MAX ? "It is too big to be a sound pack." : add(unb64url(t)); } catch (e) { p = "The link is damaged."; }
      told("The pack in the link", p);
    }
    for (const u of q.getAll("add").slice(0, 8)) {
      let url, p;
      try { url = new URL(u, location.href); } catch (e) { url = null; }
      const where = url ? (url.origin === location.origin ? url.pathname.split("/").pop() : url.host) : "The link";
      if (!url || (url.origin !== location.origin && url.protocol !== "https:")) p = "It isn't an address this page will fetch a pack from.";
      else {
        try {
          const res = await fetch(url, { credentials: "omit", redirect: "follow" });
          if (!res.ok) throw new Error(String(res.status));
          const buf = await res.arrayBuffer();
          p = buf.byteLength > PF.MAX ? "It is too big to be a sound pack." : add(new Uint8Array(buf));
        } catch (e) { p = "It couldn't be fetched: the address is wrong, or its site doesn't let this page read it."; }
      }
      told(where, p);
    }
    if (!said.length) return;
    $("add-note").textContent = said.join(" ");
    q.delete("add");                                 // the address tidied: a reload doesn't fetch again
    h.delete("pack");
    const rest = q.toString(), frag = h.toString();
    history.replaceState(null, "", location.pathname + (rest ? `?${rest}` : "") + (frag ? `#${frag}` : ""));
    const last = S.packs[S.packs.length - 1];
    if (last && last.mine) last.ui.sec.scrollIntoView({ block: "start" });
  }

  async function start() {
    $("connect").onclick = connect;
    $("fm1-go").onclick = go;
    $("fm1-off").onclick = takeOff;
    $("fm1-keep").onclick = keepNow;
    $("fm1-back").onclick = backNow;
    $("fm1-file").onclick = toFile;
    $("fm1-copy").onclick = copyOff;
    $("mix-go").onclick = mixGo;
    $("mix-undo").onclick = mixUndo;
    $("fm1-load").onclick = () => $("fm1-load-file").click();
    $("fm1-load-file").onchange = (e) => { const f = e.target.files[0]; e.target.value = ""; if (f) fromFile(f); };
    $("add").onclick = () => $("add-file").click();
    $("add-file").onchange = (e) => { const fs = [...e.target.files]; e.target.value = ""; addFiles(fs); };
    const drop = $("add-box");
    drop.ondragover = (e) => { e.preventDefault(); drop.classList.add("over"); };
    drop.ondragleave = () => drop.classList.remove("over");
    drop.ondrop = (e) => { e.preventDefault(); drop.classList.remove("over"); addFiles([...e.dataTransfer.files]); };
    S.edits = store.get(EDITS, {}) || {};
    S.pristine = store.get(PRISTINE, null);
    let site = [];
    try {
      const res = await fetch("index.json");
      if (!res.ok) throw new Error(String(res.status));
      site = (await res.json()).map(fromSite);
    } catch (e) {
      $("packs").textContent = "The site's packs didn't load. Try the page again. Packs you added from files are below.";
    }
    S.packs = site;
    S.level = site.reduce((m, p) => Math.max(m, p.level), 2);
    if (site.length) $("packs").replaceChildren(...site.map(card));
    for (const s of store.get(SHELF, []) || []) { try { add(unb64(s)); } catch (e) { /* (not a pack any more: left out) */ } }
    $("need").textContent = site.length ? site[0].needs || "1.5" : "1.5";
    await fromLinks();
    render();
  }
  globalThis.AislingPacks = S;                       // (for a test to look at)
  start();
})();
