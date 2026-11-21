// SPDX-License-Identifier: GPL-3.0-only
// The Packs page: the packs (index.json, from tools/make_pack.py by tools/make_pages.py), each heard in the page
// (worklet.js: the editor's firmware with the pack in its store) and put on an FM-1 or taken off it over the USB
// link (../editor/link.js: area 10).
(function () {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const el = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text != null) e.textContent = text; return e; };
  const S = { packs: [], link: null, hello: null, busy: false, ac: null, node: null, loaded: null, playing: null, wasm: null };
  const WHY = { 2: "The FM-1 refused it: this pack needs a newer AISLING than the FM-1 has.", 3: "It arrived damaged. Try again.",
    5: "There is no pack on the FM-1." };
  const bytesOf = (p) => Uint8Array.from(atob(p.data), (c) => c.charCodeAt(0));
  const wait = (ms) => new Promise((r) => setTimeout(r, ms));

  /* ---------- the device ---------- */
  function devText() {
    const h = S.hello;
    if (!S.link || !h) return "No FM-1 connected. You can listen without one.";
    const on = h.pack ? `${h.pack.name} (version ${h.pack.version}) is on it` : "no pack is on it";
    const which = h.pack ? (h.bank ? ", and playing" : ", the eight playing") : "";
    const old = S.packs.some((p) => p.level > h.level) ? " Its AISLING is older than a pack here needs: update it first (the link is at the foot of the page)." : "";
    return `FM-1 connected: AISLING ${h.version}; ${on}${which}.${old}`;
  }
  function installed(p) { return !!(S.hello && S.hello.pack && S.hello.pack.name === p.name); }
  function render() {
    $("dev").textContent = devText();
    $("connect").textContent = S.link ? "Disconnect" : "Connect your FM-1";
    for (const p of S.packs) {
      const on = installed(p), old = S.hello && S.hello.level < p.level;
      p.ui.install.disabled = !S.link || S.busy || old;
      p.ui.install.textContent = on ? (S.hello.pack.version === p.version ? "Install again" : "Update on the FM-1") : "Install on the FM-1";
      p.ui.remove.hidden = !on;
      p.ui.remove.disabled = S.busy;
      p.ui.go.hidden = !on;
      p.ui.go.disabled = S.busy;
      p.ui.go.textContent = S.hello && S.hello.bank ? "Back to the eight" : "Play it on the FM-1";
      for (const [i, b] of p.ui.plays.entries()) {
        const mine = S.playing && S.playing.pack === p && S.playing.i === i;
        b.setAttribute("aria-pressed", mine ? "true" : "false");
        b.textContent = mine ? "■" : "▶";
        b.setAttribute("aria-label", `${mine ? "Stop" : "Hear"} ${p.pieces[i].name}`);
      }
    }
  }
  function say(p, text) { p.ui.note.textContent = text; p.ui.note.dataset.said = text ? "1" : ""; }

  async function connect() {
    if (S.link) { S.link.close(); S.link = null; S.hello = null; render(); return; }
    if (!navigator.requestMIDIAccess) { $("dev").textContent = "Connecting needs Chrome or Edge: this browser has no USB MIDI. You can still listen."; return; }
    $("dev").textContent = "Looking for an FM-1. The browser may ask to allow MIDI.";
    let access;
    try { access = await navigator.requestMIDIAccess({ sysex: true }); }
    catch (e) { $("dev").textContent = "MIDI wasn't allowed, so nothing is connected. You can allow it from the padlock by the address."; return; }
    const ports = globalThis.AislingLink.findPorts(access);
    if (!ports) { $("dev").textContent = "No FM-1 found. Plug it in over USB with AISLING running, then connect again."; return; }
    const link = new globalThis.AislingLink(ports.input, ports.output, {
      onGone: () => { if (S.link === link) { S.link = null; S.hello = null; render(); $("dev").textContent = "The FM-1 has gone: the cable, or it stopped answering."; } },
    });
    const hello = await link.open();
    if (!hello) { $("dev").textContent = "This FM-1 doesn't answer. Packs need a newer AISLING on it: update it first."; return; }
    S.link = link;
    S.hello = hello;
    render();
  }
  async function refresh() {
    const h = S.link && await S.link.again();
    if (h) S.hello = h;
    else if (S.link) { S.link = null; S.hello = null; }
    render();
  }
  // a transfer the FM-1 only takes at rest: asked again while it says it is busy (playing, or its sound dying away)
  async function atRest(p, what, fn) {
    for (let n = 0; n < 40; n++) {
      const s = await fn();
      if (s !== 4) return s;
      say(p, `${what}: waiting for the FM-1 to be still. Press PLAY on it to stop, and let the sound die away.`);
      await wait(1000);
      if (!S.link) return -1;
    }
    return 4;
  }
  async function install(p) {
    if (!S.link || S.busy) return;
    const other = S.hello.pack && S.hello.pack.name !== p.name ? S.hello.pack.name : null;
    if (other && !confirm(`${other} is on the FM-1. Installing ${p.name} replaces it, with any changes you made to its pieces. Go on?`)) return;
    S.busy = true;
    say(p, `Sending ${p.name} to the FM-1.`);
    render();
    const s = await atRest(p, "Installing", () => S.link.install(bytesOf(p)));
    S.busy = false;
    say(p, s === 0 ? `${p.name} is on the FM-1. Turn PRESET past the eighth piece, or press "Play it on the FM-1".`
      : s === 4 ? "The FM-1 never came to rest, so nothing was changed. Stop it and try again."
      : WHY[s] || "The FM-1 stopped answering, so nothing was changed.");
    await refresh();
  }
  async function remove(p) {
    if (!S.link || S.busy) return;
    if (!confirm(`Take ${p.name} off the FM-1? Changes you made to its pieces on the FM-1 go with it.`)) return;
    S.busy = true;
    render();
    const s = await atRest(p, "Removing", () => S.link.remove());
    S.busy = false;
    say(p, s === 0 ? `${p.name} is off the FM-1. Its own eight are untouched.` : "Nothing was changed: the FM-1 wasn't still, or stopped answering.");
    await refresh();
  }
  async function go(p) {
    if (!S.link || S.busy) return;
    const s = await S.link.bank(S.hello.bank ? 0 : 1);
    if (s !== 0) say(p, WHY[s] || "The FM-1 didn't answer.");
    await refresh();
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
        node.port.postMessage({ type: "load", wasm: S.wasm.slice(0), pack: bytesOf(p), obj: 2, master: 2800 });
        const r = await ready;
        if (!r.ok) throw new Error("the page's copy of AISLING is older than this pack");
        node.connect(S.ac.destination);
        S.node = node;
        S.loaded = p;
      }
      S.node.port.postMessage({ type: "play", i });
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
  function card(p) {
    const sec = el("section", "pk-pack");
    sec.setAttribute("aria-labelledby", `pk-${p.id}`);
    const h = el("h2", null, p.name);
    h.id = `pk-${p.id}`;
    const acts = el("div", "pk-acts"), list = el("ul", "pk-list");
    p.ui = { install: el("button", "btn go", "Install on the FM-1"), remove: el("button", "btn", "Take it off the FM-1"),
      go: el("button", "btn", "Play it on the FM-1"), note: el("p", "lab pk-note"), plays: [] };
    p.ui.note.setAttribute("role", "status");
    p.ui.install.onclick = () => install(p);
    p.ui.remove.onclick = () => remove(p);
    p.ui.go.onclick = () => go(p);
    acts.append(p.ui.install, p.ui.go, p.ui.remove);
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
        el("p", "pk-meta", `${pc.side === "saint" ? "Saint" : "Sinner"} · ${pc.key} ${pc.mode} · ${pc.tempo} bpm`), el("p", "pk-what", pc.about));
      list.append(li);
    });
    sec.append(h, el("p", "pk-about", `${p.about} Version ${p.version}.`), acts, p.ui.note, list);
    return sec;
  }
  async function start() {
    $("connect").onclick = connect;
    let packs;
    try {
      const res = await fetch("index.json");
      if (!res.ok) throw new Error(String(res.status));
      packs = await res.json();
    } catch (e) {
      $("packs").textContent = "The packs didn't load. Try the page again.";
      return;
    }
    S.packs = packs;
    $("packs").replaceChildren(...packs.map(card));
    $("need").textContent = packs.length ? packs[0].needs || "1.5" : "1.5";
    render();
  }
  globalThis.AislingPacks = S;                       // (for a test to look at)
  start();
})();
