// SPDX-License-Identifier: GPL-3.0-only
// AISLING's editor: the page. Two copies of the firmware run here. A quiet one on the main thread
// describes the FM-1 before anyone has touched anything: its pages, its ranges, the words for every value. It is never
// edited. The other is in the AudioWorklet (worklet.js) and plays; everything you do goes to it, and what it reports
// comes back about 30 times a second. The page keeps no second model of the instrument: names, ranges and help all
// come out of the firmware, so the only table here that could drift is TABS, which is the FM-1's button cycles.
"use strict";
(() => {
  const NS = "http://www.w3.org/2000/svg";
  const $ = (id) => document.getElementById(id);
  function el(tag, props, ...kids) {
    const n = document.createElement(tag);
    for (const [k, v] of Object.entries(props || {})) {
      if (v === false || v == null) continue;
      if (k === "class") n.className = v;
      else if (k.startsWith("on")) n.addEventListener(k.slice(2), v);
      else n.setAttribute(k, v === true ? "" : v);
    }
    for (const c of kids.flat()) if (c != null) n.append(c);
    return n;
  }
  function sv(tag, attrs, ...kids) {
    const n = document.createElementNS(NS, tag);
    for (const [k, v] of Object.entries(attrs || {})) n.setAttribute(k, v);
    for (const c of kids) n.append(c);
    return n;
  }
  const icon = (d, solid) => sv("svg", { class: solid ? "ico solid" : "ico", viewBox: "0 0 16 16", "aria-hidden": "true" }, sv("path", { d }));
  const clamp = (x, a, b) => Math.min(b, Math.max(a, x));
  const now = () => performance.now();

  const BTN = { FX: 0, SEL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, "OCT-": 12, "OCT+": 13 };
  const WHITE_K = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
  const BLACK_K = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];
  const BLACK_AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14];
  const NOTE = ["C", "C sharp", "D", "E flat", "E", "F", "F sharp", "G", "A flat", "A", "B flat", "B"];
  // the air each piece opens on (MANUAL, the airs table; O'Neill's numbers there)
  const AIRS = ["O'Carolan's Lament", "The Coolin", "The Dear Irish Boy", "Blind Mary", "Have You Seen My Valentine?",
    "The Little Black Rose", "Shule Aroon", "Ossian's Lament"];
  // the FM-1's button cycles (firmware/src/app/ui.c). The checks press the real buttons and compare
  const TABS = [
    { name: "Aisling", btn: "HOME", pages: [0, 18] },
    { name: "Drone", btn: "ENV", pages: [1, 8, 12], layer: 0 },
    { name: "Sequence", btn: "ARP", pages: [2, 9, 13, 23], layer: 1 },
    { name: "Lead", btn: "LFO", pages: [3, 10, 26, 14], layer: 2 },
    { name: "Seq 2", btn: "SEQ", pages: [4, 11, 16, 24], layer: 3 },
    { name: "Atmos", btn: "SAVE", pages: [19, 21, 25], layer: 4 },
    { name: "Space", btn: "FX", pages: [5, 20] },
    { name: "Setup", btn: "GLO", pages: [6, 22] },
    { name: "Chords", btn: "EDIT", pages: [15] },
    { name: "Perform", btn: "SEL", pages: [17, 7] },
  ];
  const LAYERS = [
    { name: "Drone", btn: "ENV", tab: 1, page: 1 },
    { name: "Sequence", btn: "ARP", tab: 2, page: 2 },
    { name: "Lead", btn: "LFO", tab: 3, page: 3 },
    { name: "Seq 2", btn: "SEQ", tab: 4, page: 4 },
    { name: "Atmosphere", short: "Atmos", btn: "SAVE", tab: 5, page: 19 },
  ];
  const V_PERF = 17, V_MIX = 7, V_PATTERN = 16, V_KEY = 18, V_CHORDS = 15;
  const tabOfView = [];
  TABS.forEach((t, i) => t.pages.forEach((v) => { tabOfView[v] = i; }));
  const pressHint = (v) => {
    if (v === V_MIX) return "SEL, hold D4";
    const t = TABS[tabOfView[v]], n = t.pages.indexOf(v);
    return n ? `${t.btn} ×${n + 1}` : t.btn;
  };

  const KEY_STORE = "aisling-editor-store", KEY_SHELF = "aisling-editor-shelf", KEY_MASTER = "aisling-editor-master", KEY_LOOK = "aisling-editor-look";
  const lsGet = (k) => { try { return localStorage.getItem(k); } catch (e) { return null; } };
  const lsSet = (k, v) => { try { localStorage.setItem(k, v); return true; } catch (e) { return false; } };
  const b64 = (u8) => {
    let s = "";
    for (let i = 0; i < u8.length; i += 0x8000) s += String.fromCharCode.apply(null, u8.subarray(i, i + 0x8000));
    return btoa(s);
  };
  const unb64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));
  const b64url = (u8) => b64(u8).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
  const unb64url = (s) => unb64(s.replace(/-/g, "+").replace(/_/g, "/") + "===".slice(0, (4 - s.length % 4) % 4));

  const S = {
    wasm: null, started: false, ac: null, node: null, queue: [], ready: false, skipPlay: 0,
    vals: new Map(), hold: new Map(), echo: new Map(), sets: new Map(), raf: 0, viewHold: 0,
    view: 0, tab: 0, main: "play", piece: 0, playing: false, frozen: false, dirty: false,
    keys: 0, buttons: 0, keysDown: 0, btnsDown: 0, muted: 0, solo: 0, sounds: 31, act: [0, 0, 0, 0, 0],
    pieces: [], hist: { undo: null, redo: null }, fb: null, fbDirty: false, master: 2800,
    look: null, lookName: "AISLING", identity: true, pal: null,
    libSel: 0, shelfSel: -1, pick: null, shelf: [],
    link: null, linking: false, access: null, saving: false, pieceHold: 0, linkUn: -1, linkRn: -1, linkGen: -1, linkPiece: -1, linkRaw: -1,
    piecesAt: 0, piecesBy: 0, piecesBusy: false, liftAll: null,
  };
  let Q, Qmem, K_NONE, K_CPU, K_SAVE, K_MODE, P_TEMPO, P_ROOT, P_MODE, NPARAMS;
  const qstr = (at) => globalThis.AislingEngine.str(Qmem, at);
  const valueText = (k, v) => qstr(Q.ed_value_text(k, v));
  const modeOrder = new Map();           // the raw mode in a slot -> K_MODE's position, matched by name

  const controls = new Map();            // knob -> the controls showing it; an echo moves all of them
  const reg = (k, c) => { if (!controls.has(k)) controls.set(k, new Set()); controls.get(k).add(c); return c; };
  const show = (k) => {
    const e = S.vals.get(k);
    if (e) for (const c of controls.get(k) || []) c.render(e.v, e.text);
  };

  /* ---------- the worklet ---------- */
  function post(m) {
    if (S.link) { S.link.post(m); return; }          // connected: the same messages go to the FM-1
    if (S.node) S.node.port.postMessage(m); else S.queue.push(m);
  }

  async function start(rest) {
    if (S.started) return;
    S.started = true;
    try {
      const ac = new AudioContext({ sampleRate: 44100, latencyHint: "interactive" });
      S.ac = ac;
      ac.resume();
      await ac.audioWorklet.addModule("engine.js");
      await ac.audioWorklet.addModule("worklet.js");
      const node = new AudioWorkletNode(ac, "aisling-editor", { numberOfInputs: 0, numberOfOutputs: 1, outputChannelCount: [2] });
      node.connect(ac.destination);
      node.port.onmessage = (e) => { if (S.link && e.data.type === "frame") return; onWorklet(e.data); };
      const wasm = S.wasm.slice(0);
      node.port.postMessage({ type: "load", wasm, store: loadStore(), master: S.master, knobs: S.knobs, rest }, [wasm]);
      S.node = node;
      for (const m of S.queue) node.port.postMessage(m);
      S.queue = [];
      if (!S.link) ac.resume();
    } catch (e) {
      S.started = false;
      note("The sound couldn't start in this browser. Chrome, Edge or Safari will do it.");
    }
  }

  function firstGesture(e) {
    if (S.link) return;                              // connected: this page makes no sound
    if (S.started) { if (S.ac && S.ac.state !== "running") S.ac.resume(); return; }
    const t = e.target.closest && e.target.closest("button, select, input, [role=slider], .k");
    if (!t || !t.closest(".ed") || t.closest("[data-quiet]")) return;
    const plays = t.closest("[data-plays]");
    if (plays && plays.id === "play") S.skipPlay = now() + 2000;
    start(!plays);
  }
  document.addEventListener("pointerdown", firstGesture, true);
  document.addEventListener("keydown", (e) => {
    if (e.metaKey || e.ctrlKey || e.altKey || ["Tab", "Shift", "Control", "Alt", "Meta", "Escape"].includes(e.key)) return;
    firstGesture(e);
  }, true);
  for (const t of ["pointerup", "click", "keydown"]) {
    document.addEventListener(t, () => { if (!S.link && S.ac && S.ac.state !== "running") S.ac.resume(); }, true);
  }

  const pending = new Map();
  let reqId = 0;
  function request(m) {
    if (!S.started && !S.link) start(true);
    return new Promise((res) => { const id = ++reqId; pending.set(id, res); post({ ...m, id }); });
  }

  function takeVals(vals, t) {
    for (let i = 0; i < vals.length; i += 3) {
      const k = vals[i], v = vals[i + 1], text = vals[i + 2];
      if ((S.hold.get(k) || 0) > t) {                   // this control has just been moved: an older echo mustn't move it back
        const cur = S.vals.get(k);
        if (cur && cur.v === v) { cur.text = text; show(k); S.echo.delete(k); } else S.echo.set(k, { v, text });
      } else {
        S.vals.set(k, { v, text });
        show(k);
      }
    }
  }

  function onWorklet(m) {
    if (m.type === "ready") { S.ready = true; return; }
    if (m.type === "reply") { const r = pending.get(m.id); pending.delete(m.id); if (r) r(m); return; }
    if (m.type !== "frame") return;
    const t = now();
    if (m.vals) takeVals(m.vals, t);
    if (m.fb) { S.fb = m.fb; S.fbDirty = true; }
    if (m.hist) showHist(m.hist);
    if (m.store) saveStore(m.store);
    if (m.piece !== S.piece && t > S.pieceHold) { S.piece = m.piece; pieceMoved(); }
    if (m.pieces) { S.pieces = m.pieces; renderPieces(); }
    if (m.view !== S.view && t > S.viewHold) setView(m.view);
    if (m.playing !== S.playing) { S.playing = m.playing; renderPlay(); }
    if (m.frozen !== S.frozen) { S.frozen = m.frozen; renderPlay(); }
    if (m.dirty !== S.dirty) { S.dirty = m.dirty; renderSave(); }
    if (m.keys !== S.keys) { S.keys = m.keys; renderKeys(); }
    if (m.buttons !== S.buttons) { S.buttons = m.buttons; renderLights(); }
    if (m.muted !== S.muted || m.solo !== S.solo || m.sounds !== S.sounds) {
      S.muted = m.muted; S.solo = m.solo; S.sounds = m.sounds; renderStrips();
    }
    S.act = m.act;
    schedule();
  }

  function schedule() { if (!S.raf) S.raf = requestAnimationFrame(tick); }
  function tick() {
    S.raf = 0;
    flushSets();
    const t = now();
    for (const [k, e] of S.echo) {
      if ((S.hold.get(k) || 0) <= t) { S.vals.set(k, e); show(k); S.echo.delete(k); }
    }
    for (let i = 0; i < strips.length; i++) strips[i].meter.style.transform = `scaleY(${clamp(S.act[i] || 0, 0, 1)})`;
    if (S.fbDirty) drawScreen();
  }

  /* ---------- changing things ---------- */
  function change(k, v) {
    const old = S.vals.get(k);
    S.vals.set(k, { v, text: valueText(k, v) || (old ? old.text : "") });
    S.hold.set(k, now() + (S.link ? 400 : 180));
    S.echo.delete(k);
    show(k);
    S.sets.set(k, v);
    schedule();
  }
  function flushSets() {
    for (const [k, v] of S.sets) post({ type: "set", k, v });
    S.sets.clear();
  }
  function goView(v, knob) {
    S.viewHold = now() + (S.link ? 400 : 200);
    post({ type: "view", v, knob: knob == null ? -1 : knob });
    if (v !== S.view) setView(v);
  }
  function setView(v) {
    S.view = v;
    const tab = tabOfView[v];
    if (tab !== S.tab) showTab(tab);
    for (const g of groups) g.el.classList.toggle("here", g.page === v);
    keyLabels();
  }

  // what double-clicking goes back to: this piece's recipe (the FM-1's own settings, Tune and MIDI out, have none)
  function homeOf(k) {
    if (k === K_MODE) { const o = modeOrder.get(Q.ed_recipe(S.piece, P_MODE)); return o == null ? null : o; }
    if (k < NPARAMS && !Q.ed_is_global(k)) return Q.ed_recipe(S.piece, k);
    return null;
  }

  function control(k, label, page, idx, extra) {
    const lo = Q.ed_lo(k), hi = Q.ed_hi(k);
    return { k, label, page, idx, lo, hi, range: Math.max(1, hi - lo), centre: lo < 0 && hi > 0 && hi - lo >= 10, step: 1, undoable: true,
      value: () => { const e = S.vals.get(k); return e ? e.v : lo; }, set: (v) => change(k, v), ...extra };
  }
  const beginEdit = (c) => {
    if (c.undoable) post({ type: "begin", label: c.label });
    if (c.page != null) goView(c.page, c.idx);
  };
  const endEdit = (c) => { flushSets(); if (c.undoable) post({ type: "end" }); };

  // One way to work every slider: a knob drags up and down relatively; a slider or fader follows the pointer, keeps its
  // distance if you pressed near the thumb, and jumps there if you didn't. Shift is fine control. Double-click goes home.
  function bindSlider(node, track, c, axis, render) {
    const pos = (v) => (v - c.lo) / c.range;
    const valAt = (p, fine) => {
      let v = c.lo + clamp(p, 0, 1) * c.range;
      v = Math.round(v / c.step) * c.step;
      if (c.centre && !fine && Math.abs(v) <= Math.max(1, c.range * 0.02)) v = 0;
      return clamp(v, c.lo, c.hi);
    };
    const at = (e) => {
      const r = track.getBoundingClientRect();
      return axis === "h" ? (e.clientX - r.left) / Math.max(1, r.width) : 1 - (e.clientY - r.top) / Math.max(1, r.height);
    };
    const len = () => { const r = track.getBoundingClientRect(); return (axis === "h" ? r.width : r.height) || 1; };
    let drag = null;
    node.addEventListener("pointerdown", (e) => {
      if (e.button) return;
      e.preventDefault();
      node.focus({ preventScroll: true });
      node.setPointerCapture(e.pointerId);
      beginEdit(c);
      const cur = pos(c.value());
      if (axis === "knob") {
        drag = { pos: cur, y: e.clientY };
      } else {
        const p = at(e), near = Math.abs(p - cur) * len() <= 12;
        drag = { pos: near ? cur : p, off: near ? cur - p : 0, p };
        if (!near) { const v = valAt(p, e.shiftKey); if (v !== c.value()) c.set(v); }
      }
    });
    node.addEventListener("pointermove", (e) => {
      if (!drag) return;
      if (axis === "knob") {
        drag.pos = clamp(drag.pos + (drag.y - e.clientY) / 200 * (e.shiftKey ? 0.25 : 1), 0, 1);
        drag.y = e.clientY;
      } else {
        const p = at(e);
        if (e.shiftKey) { drag.pos = clamp(drag.pos + (p - drag.p) * 0.25, 0, 1); drag.off = drag.pos - p; }
        else drag.pos = clamp(p + drag.off, 0, 1);
        drag.p = p;
      }
      const v = valAt(drag.pos, e.shiftKey);
      if (v !== c.value()) c.set(v);
    });
    const up = () => { if (!drag) return; drag = null; endEdit(c); };
    node.addEventListener("pointerup", up);
    node.addEventListener("pointercancel", up);
    node.addEventListener("keydown", (e) => {
      const big = Math.max(c.step, Math.round(c.range / 10));
      const d = { ArrowUp: c.step, ArrowRight: c.step, ArrowDown: -c.step, ArrowLeft: -c.step, PageUp: big, PageDown: -big }[e.key];
      let v;
      if (d) v = c.value() + d;
      else if (e.key === "Home") v = c.lo;
      else if (e.key === "End") v = c.hi;
      else return;
      e.preventDefault();
      v = clamp(v, c.lo, c.hi);
      if (v === c.value()) return;
      beginEdit(c);
      c.set(v);
      endEdit(c);
    });
    if (c.undoable) node.addEventListener("dblclick", () => {
      const h = homeOf(c.k);
      if (h == null || h === c.value()) return;
      beginEdit(c);
      c.set(h);
      endEdit(c);
    });
    Object.assign(node, { tabIndex: 0 });
    node.setAttribute("role", "slider");
    node.setAttribute("aria-label", c.label);
    node.setAttribute("aria-valuemin", c.lo);
    node.setAttribute("aria-valuemax", c.hi);
    c.pos = pos;
    c.render = (v, text) => {
      node.setAttribute("aria-valuenow", v);
      node.setAttribute("aria-valuetext", text);
      render(v, text);
    };
  }
  const zeroOf = (c) => (c.lo < 0 && c.hi > 0 ? -c.lo / c.range : 0);

  function makeKnob(k, label, page, idx) {
    const c = control(k, `${Q_page(page)} ${label}`, page, idx);
    const R = 19, A = 10.565, B = 37.435;
    const arcD = `M${A} ${B} A${R} ${R} 0 1 1 ${B} ${B}`;
    const arc = sv("path", { class: "knob-arc", d: arcD, pathLength: "100" });
    const ptr = sv("line", { class: "knob-ptr", x1: "24", y1: "13", x2: "24", y2: "20" });
    const home = sv("line", { class: "knob-home", x1: "24", y1: "1", x2: "24", y2: "4" });
    const g = sv("svg", { viewBox: "0 0 48 48", "aria-hidden": "true" },
      sv("path", { class: "knob-track", d: arcD }), arc, sv("circle", { class: "knob-face", cx: "24", cy: "24", r: "12.5" }), ptr, home);
    const v = el("span", { class: "knob-v" }), node = el("div", { class: "knob" }, g, el("span", { class: "knob-l" }, label), v);
    const ang = (p) => -135 + 270 * p;
    bindSlider(node, node, c, "knob", (val, text) => {
      const p = c.pos(val), z = zeroOf(c), a = Math.min(p, z), b = Math.max(p, z);
      arc.style.strokeDasharray = `${(b - a) * 100} 100`;
      arc.style.strokeDashoffset = `${-a * 100}`;
      ptr.setAttribute("transform", `rotate(${ang(p)} 24 24)`);
      v.textContent = text;
      c.home();
    });
    c.home = () => {
      const h = homeOf(k), val = c.value();
      home.style.display = h == null || h === val ? "none" : "";
      if (h != null) home.setAttribute("transform", `rotate(${ang(c.pos(h))} 24 24)`);
    };
    reg(k, c);
    help(node, Q_help(page, idx), fmKnob(page, idx));
    return node;
  }

  function makeHS(c, labelText, opts) {
    const fill = el("div", { class: "hs-fill" }), th = el("div", { class: "hs-th" }), home = el("div", { class: "hs-home" });
    const track = el("div", { class: "hs-track" });
    if (c.range <= 12) track.append(el("div", { class: "hs-ticks", style: `--n:${c.range}` }));
    track.append(fill, home, th);
    const val = el("span", { class: "hs-v" });
    const node = el("div", { class: "hs" }, el("div", { class: "hs-top" }, el("span", { class: "hs-l" }, labelText), val), track);
    bindSlider(node, track, c, "h", (v, text) => {
      const p = c.pos(v), z = zeroOf(c);
      fill.style.left = `${Math.min(p, z) * 100}%`;
      fill.style.width = `${Math.abs(p - z) * 100}%`;
      th.style.left = `${p * 100}%`;
      val.textContent = text;
      if (c.home) c.home();
    });
    if (c.undoable) c.home = () => {
      const h = homeOf(c.k), v = c.value();
      home.style.display = h == null || h === v ? "none" : "";
      if (h != null) home.style.left = `${c.pos(h) * 100}%`;
    };
    if (!(opts && opts.free)) reg(c.k, c);
    return node;
  }

  function makeFader(k, label, page) {
    const c = control(k, label, page, null);
    const fill = el("div", { class: "fader-f" }), th = el("div", { class: "fader-th" }), hm = el("div", { class: "fader-home" });
    const inner = el("div", { class: "fader-i" }, hm, th);
    const node = el("div", { class: "fader" }, el("div", { class: "fader-t" }, fill), inner);
    const val = el("span", { class: "fader-v" });
    bindSlider(node, inner, c, "v", (v, text) => {
      const p = c.pos(v);
      fill.style.height = `${p * 100}%`;
      th.style.bottom = `${p * 100}%`;
      val.textContent = text;
      c.home();
    });
    c.home = () => {
      const h = homeOf(k), v = c.value();
      hm.style.display = h == null || h === v ? "none" : "";
      if (h != null) hm.style.bottom = `${c.pos(h) * 100}%`;
    };
    reg(k, c);
    return { node, val };
  }

  function makeSeg(k, label, page, idx, texts) {
    const c = control(k, `${Q_page(page)} ${label}`, page, idx);
    const btns = texts.map((t, i) => el("button", { type: "button", "aria-pressed": "false", title: t,
      onclick: () => { const v = c.lo + i; if (v === c.value()) { beginEdit(c); endEdit(c); return; } beginEdit(c); c.set(v); endEdit(c); } }, t));
    c.render = (v) => btns.forEach((b, i) => b.setAttribute("aria-pressed", c.lo + i === v ? "true" : "false"));
    c.home = () => {};
    reg(k, c);
    const node = el("div", { class: "seg-w", role: "group", "aria-label": label }, el("span", { class: "seg-l" }, label), el("div", { class: "seg" }, btns));
    help(node, Q_help(page, idx), fmKnob(page, idx));
    return node;
  }

  function makeSelect(k, label, labelText, page, idx, onset) {
    const lo = Q.ed_lo(k), hi = Q.ed_hi(k), opts = [];
    for (let v = lo; v <= hi; v++) opts.push(el("option", { value: v }, valueText(k, v)));
    const node = el("select", { class: "sel", "aria-label": labelText, onchange: () => onset(+node.value) }, opts);
    reg(k, { k, render: (v) => { node.value = String(v); }, home: () => {} });
    return node;
  }

  /* ---------- help, in the words of the firmware ---------- */
  const Q_page = (v) => qstr(Q.ed_view_name(v));
  const Q_help = (v, i) => qstr(Q.ed_knob_help(v, i));
  const fmKnob = (v, i) => `On the FM-1: ${pressHint(v)}, then knob ${i + 1}`;
  const HELP_REST = "Touch a control and its page comes up on the screen, as it does on the FM-1. Double-click goes back to the piece's recipe; Shift is fine control.";
  function setHelp(t, fm) {
    $("help-t").textContent = t || HELP_REST;
    $("help-fm").textContent = fm || "";
  }
  function help(node, t, fm) {
    const on = () => setHelp(typeof t === "function" ? t() : t, typeof fm === "function" ? fm() : fm), off = () => setHelp();
    node.addEventListener("pointerenter", on);
    node.addEventListener("focusin", on);
    node.addEventListener("pointerleave", off);
    node.addEventListener("focusout", off);
  }
  function note(text) {
    const n = $("note");
    n.replaceChildren(el("span", null, text), el("button", { class: "ebtn", type: "button", "data-quiet": true, onclick: () => { n.hidden = true; } }, "Dismiss"));
    n.hidden = false;
  }

  /* ---------- the transport and the cards ---------- */
  const badges = [];                     // {el, bit}: lit when the firmware lights that button
  function badge(btn, extra) {
    const b = el("span", { class: extra ? `badge ${extra}` : "badge" }, btn);
    if (BTN[btn] != null) badges.push({ el: b, bit: BTN[btn] });
    return b;
  }
  const dashed = (label, why) => el("button", { class: "ebtn dashed", type: "button", "aria-disabled": "true", title: why, "data-quiet": true, onclick: () => note(why) }, label);

  const ICO_PLAY = "M4 2.5v11l9-5.5z", ICO_REST = "M4 3h3v10H4zM9 3h3v10H9z";
  let playBtn, playIco, playT, holdBtn;
  let masterCtl, volNode;
  function buildTransport() {
    playIco = sv("path", { d: ICO_PLAY });
    playT = el("span", { class: "t-play" }, "Play");
    playBtn = el("button", { class: "t-btn", id: "play", type: "button", "aria-label": "Play", "aria-pressed": "false", "data-plays": true, onclick: () => {
      if (now() < S.skipPlay) { S.skipPlay = 0; return; }
      post({ type: "play" });
    } }, sv("svg", { class: "ico solid", viewBox: "0 0 16 16", "aria-hidden": "true" }, playIco), playT, badge("PLAY"));
    holdBtn = el("button", { class: "t-btn", id: "hold", type: "button", "aria-pressed": "false", onclick: () => post({ type: "hold" }) }, "Hold", badge("REC"));
    help(playBtn, "Play or rest. Rest stops the pieces; the keys still play.", "On the FM-1: PLAY");
    help(holdBtn, "The echo and the cloud repeat what they hold.", "On the FM-1: tap REC");

    const tempo = control(P_TEMPO, "Tempo", V_KEY, 0);
    const tempoN = makeHS(tempo, "Tempo");
    help(tempoN, Q_help(V_KEY, 0), fmKnob(V_KEY, 0));
    const editSel = (k, label, idx) => (v) => { const c = { label, page: V_KEY, idx, undoable: true }; beginEdit(c); change(k, v); endEdit(c); };
    const root = makeSelect(P_ROOT, "Root", "Root", V_KEY, 1, editSel(P_ROOT, "Root", 1));
    const mode = makeSelect(K_MODE, "Mode", "Mode", V_KEY, 2, editSel(K_MODE, "Mode", 2));
    help(root, Q_help(V_KEY, 1), fmKnob(V_KEY, 1));
    help(mode, Q_help(V_KEY, 2), fmKnob(V_KEY, 2));

    const master = masterCtl = { k: -1, label: "Volume", lo: 0, hi: 4096, range: 4096, step: 64, undoable: false, centre: false, value: () => S.master,
      set: (v) => {
        if (S.link) return;                          // the FM-1's own knob: nothing here reaches it
        S.master = v;
        post({ type: "master", v });
        lsSet(KEY_MASTER, String(v));
        master.render(v, `${Math.round(100 * v / 4096)}%`);
      } };
    const vol = volNode = makeHS(master, "Volume", { free: true });
    help(vol, () => (S.link ? "The FM-1's MASTER knob sets its analogue volume; this shows where it is. USB audio isn't affected by it." : "This page's volume, separate from the piece. Not in undo."), "On the FM-1: MASTER");
    master.render(S.master, `${Math.round(100 * S.master / 4096)}%`);

    $("transport").append(playBtn, holdBtn, el("div", { class: "t-group t-slider" }, tempoN),
      el("div", { class: "t-group" }, el("span", null, "Key"), root, mode),
      el("div", { class: "t-group t-slider t-vol" }, vol),
      el("div", { class: "t-group t-right" }, el("span", null, "Pieces"), badge("PRESETS")));
  }
  function renderPlay() {
    playBtn.setAttribute("aria-pressed", S.playing ? "true" : "false");
    playIco.setAttribute("d", S.playing ? ICO_REST : ICO_PLAY);
    playT.textContent = S.playing ? "Playing" : "Play";
    holdBtn.setAttribute("aria-pressed", S.frozen ? "true" : "false");
  }
  const cards = [];
  const nameOf = (i) => qstr(Q.ed_piece_name(i));
  const keyLine = (p) => {
    const m = valueText(P_MODE, p.mode);
    return `${valueText(P_ROOT, p.root)} ${(m || "").toLowerCase()} · ${p.tempo}`;
  };
  function buildCards() {
    for (let i = 0; i < Q.ed_npieces(); i++) {
      const key = el("span", { class: "card-key" }), dot = el("i", { class: "dot-edited", title: "Changed from its recipe", hidden: true });
      const b = el("button", { class: "card", type: "button", "data-plays": true, "aria-pressed": "false", onclick: () => {
        if (i !== S.piece) { post({ type: "piece", i, label: `Piece ${nameOf(i)}` }); S.pieceHold = now() + (S.link ? 400 : 0); S.piece = i; pieceMoved(); }
      } }, el("span", { class: "card-n" }, i + 1), el("span", { class: "card-name" }, nameOf(i)), el("span", { class: "card-air", title: AIRS[i] }, AIRS[i]), key, dot);
      help(b, `${nameOf(i)} opens on ${AIRS[i]}.`, "On the FM-1: the PRESETS knob");
      cards.push({ b, key, dot });
      $("cards").append(b);
    }
  }
  function renderPieces() {
    S.pieces.forEach((p, i) => {
      const c = cards[i];
      c.key.textContent = keyLine(p);
      c.dot.hidden = !p.edited;
    });
    renderLibrary();
  }
  function pieceMoved() {
    cards.forEach((c, i) => {
      c.b.classList.toggle("on", i === S.piece);
      c.b.setAttribute("aria-pressed", i === S.piece ? "true" : "false");
    });
    for (const set of controls.values()) for (const c of set) if (c.home) c.home();
    keyCaption();
    renderLibrary();
  }

  /* ---------- the five strips ---------- */
  const strips = [];
  function buildStrips() {
    LAYERS.forEach((L, i) => {
      const levelK = Q.ed_layer_level(i), voiceK = Q.ed_layer_voice(i);
      const head = el("button", { class: "ch-head", type: "button", "aria-label": `${L.name}: open its pages`, onclick: () => { showTab(L.tab); goView(L.page); } },
        el("span", { class: "ch-name" }, L.short || L.name), badge(L.btn));
      const voice = makeSelect(voiceK, "Voice", `${L.name} voice`, L.page, null, (v) => {
        post({ type: "voice", p: voiceK, v, label: `${L.name} voice` });
        const old = S.vals.get(voiceK);
        S.vals.set(voiceK, { v, text: old ? old.text : "" });
        S.hold.set(voiceK, now() + 180);
        goView(L.page);
      });
      help(voice, i === 4 ? "The Atmosphere's character." : `The ${L.name}'s voice.`, "On the FM-1: the ALGORITHM knob, on this layer's pages");
      const knobs = el("div", { class: "ch-knobs" });
      for (let n = 0; n < 4; n++) knobs.append(makeKnob(Q.ed_view_knob(L.page, n), qstr(Q.ed_knob_label(L.page, n)), L.page, n));
      const f = makeFader(levelK, `${L.name} level`, L.page);
      help(f.node, `The ${L.name}'s level.`, "On the FM-1: the SELECT knob, on this layer's pages");
      const meterFill = el("i");
      const mute = el("button", { class: "mini mute", type: "button", "aria-pressed": "false", onclick: () => { post({ type: "perf", i, down: 1 }); post({ type: "perf", i, down: 0 }); } }, "Mute");
      const solo = el("button", { class: "mini solo", type: "button", "aria-pressed": "false", onclick: () => { post({ type: "perf", i: 10 + i, down: 1 }); post({ type: "perf", i: 10 + i, down: 0 }); } }, "Solo");
      help(mute, `Silence the ${L.name}.`, `On the FM-1: SEL, then white key ${i + 1}`);
      help(solo, `Hear only the ${L.name}.`, `On the FM-1: SEL, then black key ${i + 1}`);
      const node = el("div", { class: "ch", role: "group", "aria-label": L.name, style: `--c:var(--l${i});--ct:var(--t${i})` },
        head, voice, knobs,
        el("div", { class: "ch-low" },
          el("div", { class: "ch-level" }, el("span", { class: "ch-level-l" }, "Level"), f.val),
          el("div", { class: "ch-fader" }, f.node, el("div", { class: "meter", "aria-hidden": "true" }, meterFill)),
          el("div", { class: "ch-btns" }, mute, solo)));
      strips.push({ el: node, meter: meterFill, mute, solo });
      $("strips").append(node);
    });
  }
  function renderStrips() {
    strips.forEach((s, i) => {
      s.mute.setAttribute("aria-pressed", S.muted >> i & 1 ? "true" : "false");
      s.solo.setAttribute("aria-pressed", S.solo >> i & 1 ? "true" : "false");
      s.el.classList.toggle("silent", !(S.sounds >> i & 1));
    });
  }

  /* ---------- the pages, by the buttons that open them ---------- */
  const rwTabs = [], tabPanels = [], groups = [], saveBtns = [];
  function cellFor(v, i) {
    const k = Q.ed_view_knob(v, i), label = qstr(Q.ed_knob_label(v, i));
    if (k === K_NONE || k === K_CPU) return el("div", { class: "empty" });
    if (k === K_SAVE) {
      const b = el("button", { class: "savebtn", type: "button", onclick: () => post({ type: "save" }) }, "Saved");
      saveBtns.push(b);
      const w = el("div", { class: "seg-w" }, el("span", { class: "seg-l" }, label), b);
      help(w, Q_help(v, i), "On the FM-1: hold SAVE or GLO for half a second");
      return w;
    }
    const lo = Q.ed_lo(k), hi = Q.ed_hi(k);
    if (hi - lo <= 4) {
      const texts = [];
      for (let x = lo; x <= hi; x++) texts.push(valueText(k, x));
      const longest = Math.max(...texts.map((t) => t.length));
      if (texts.every(Boolean) && longest * texts.length <= 22) return makeSeg(k, label, v, i, texts);
    }
    const c = control(k, `${Q_page(v)} ${label}`, v, i);
    const n = makeHS(c, label);
    help(n, Q_help(v, i), fmKnob(v, i));
    return n;
  }
  function buildPages() {
    TABS.forEach((t, ti) => {
      const col = t.layer == null ? "--c:var(--vellum);--ct:var(--hair-hi)" : `--c:var(--l${t.layer});--ct:var(--t${t.layer})`;
      const b = el("button", { class: "rw-tab", type: "button", role: "tab", id: `rwt-${ti}`, "aria-controls": `rwp-${ti}`, "aria-selected": "false", tabindex: "-1", style: col,
        onclick: () => { showTab(ti); goView(t.pages[0]); } }, el("span", { class: "rw-tab-n" }, t.name), badge(t.btn));
      b.addEventListener("keydown", (e) => {
        const d = { ArrowRight: 1, ArrowLeft: -1 }[e.key];
        if (!d) return;
        e.preventDefault();
        const n = (ti + d + TABS.length) % TABS.length;
        rwTabs[n].focus();
        rwTabs[n].click();
      });
      rwTabs.push(b);
      $("rw-tabs").append(b);
      const panel = el("div", { class: "tp", id: `rwp-${ti}`, role: "tabpanel", "aria-labelledby": `rwt-${ti}`, style: col, hidden: true });
      for (const v of t.pages) {
        const cells = el("div", { class: "grp-k" });
        for (let i = 0; i < 4; i++) cells.append(cellFor(v, i));
        const g = el("div", { class: "grp" }, el("button", { class: "grp-h", type: "button", onclick: () => goView(v) },
          el("span", { class: "grp-n" }, Q_page(v)), el("span", { class: "badge" }, pressHint(v))), cells);
        groups.push({ el: g, page: v });
        panel.append(g);
      }
      tabPanels.push(panel);
      $("rw-pages").append(panel);
    });
  }
  function showTab(i) {
    S.tab = i;
    rwTabs.forEach((b, j) => { b.setAttribute("aria-selected", j === i ? "true" : "false"); b.tabIndex = j === i ? 0 : -1; });
    tabPanels.forEach((p, j) => { p.hidden = j !== i; });
    strips.forEach((s, j) => s.el.classList.toggle("open", TABS[i].layer === j));
  }
  function renderSave() {
    const b = $("save"), text = S.saving ? "Saves on stop" : S.dirty ? "Save" : "Saved";
    b.textContent = text;
    b.disabled = !S.dirty || S.saving;
    b.title = S.saving ? "The FM-1 saves when it's at rest, so the music isn't interrupted. Stop it, or hold SAVE on the FM-1 to save now." : "Keep this piece. On the FM-1: hold SAVE or GLO for half a second";
    for (const s of saveBtns) { s.textContent = text; s.classList.toggle("dirty", S.dirty); }
  }
  function renderLights() {
    for (const b of badges) b.el.classList.toggle("lit", !!(S.buttons >> b.bit & 1));
    octs.forEach((o) => o.el.classList.toggle("lit", !!(S.buttons >> BTN[o.btn] & 1)));
  }
  function showHist(h) {
    S.hist = h;
    $("undo").disabled = !h.undo;
    $("redo").disabled = !h.redo;
    $("undo-t").textContent = h.undo ? `Undo ${h.undo}` : "Undo";
    $("redo-t").textContent = h.redo ? `Redo ${h.redo}` : "Redo";
    $("undo").title = h.undo ? `Undo ${h.undo}` : "";
    $("redo").title = h.redo ? `Redo ${h.redo}` : "";
  }

  /* ---------- the keyboard: the FM-1's own keys, doing what they do there ---------- */
  const keyEls = [], octs = [];
  const held = new Map();
  let keysHolder, capName, capAir, capHint;
  function buildKeyboard() {
    capName = el("span");
    capAir = el("span");
    capHint = el("span", { class: "kbd-hint" });
    const keys = el("div", { class: "keys", role: "group", "aria-label": "The FM-1's keys" });
    keysHolder = keys;
    const label = (k) => `${NOTE[(5 + k) % 12]} ${3 + Math.floor((5 + k) / 12)}`;
    for (const k of WHITE_K) {
      const b = el("button", { class: "k w", type: "button", tabindex: "-1", "aria-label": label(k), "data-k": k });
      keyEls[k] = b;
      keys.append(b);
    }
    BLACK_K.forEach((k, i) => {
      const b = el("button", { class: "k b", type: "button", tabindex: "-1", "aria-label": label(k), "data-k": k, style: `left:${(BLACK_AFTER[i] + 1) * 100 / 16}%` });
      keyEls[k] = b;
      keys.append(b);
    });
    keys.addEventListener("pointerdown", (e) => {
      const t = e.target.closest(".k");
      if (!t || e.button) return;
      e.preventDefault();
      held.set(e.pointerId, +t.dataset.k);
      keysChanged();
    });
    window.addEventListener("pointermove", (e) => {
      if (!held.has(e.pointerId)) return;
      const hit = document.elementFromPoint(e.clientX, e.clientY);
      const t = hit && hit.closest && hit.closest(".k");
      if (t && +t.dataset.k !== held.get(e.pointerId)) { held.set(e.pointerId, +t.dataset.k); keysChanged(); }
    });
    const lift = (e) => { if (held.delete(e.pointerId)) keysChanged(); };
    window.addEventListener("pointerup", lift);
    window.addEventListener("pointercancel", lift);
    const liftAll = () => {
      if (held.size) { held.clear(); keysChanged(); }
      if (S.btnsDown) { S.btnsDown = 0; octs.forEach((o) => o.el.classList.remove("down")); post({ type: "buttons", mask: 0 }); }
    };
    S.liftAll = liftAll;
    window.addEventListener("blur", liftAll);
    document.addEventListener("visibilitychange", () => { if (document.hidden) liftAll(); });

    const oct = (btn, sign) => {
      const bit = 1 << BTN[btn];
      const press = (down) => {
        const was = S.btnsDown;
        S.btnsDown = down ? was | bit : was & ~bit;
        if (S.btnsDown !== was) post({ type: "buttons", mask: S.btnsDown });
        b.classList.toggle("down", down);
      };
      const b = el("button", { class: "oct", type: "button", "aria-label": btn, onpointerdown: (e) => { if (e.button) return; e.preventDefault(); b.setPointerCapture(e.pointerId); press(true); },
        onpointerup: () => press(false), onpointercancel: () => press(false),
        onkeydown: (e) => { if ((e.key === " " || e.key === "Enter") && !e.repeat) { e.preventDefault(); press(true); } },
        onkeyup: (e) => { if (e.key === " " || e.key === "Enter") press(false); },
        onblur: () => press(false) }, sign, el("small", null, "OCT"));
      help(b, "The layer's octave, a step a press, on the Sequence's, the Lead's and Sequence 2's pages.", `On the FM-1: ${btn}`);
      octs.push({ el: b, btn });
      return b;
    };
    $("kbd").append(
      el("div", { class: "kbd-cap" }, el("div", { class: "kbd-name" }, capName, capAir), capHint),
      el("div", { class: "kbd-row" }, el("div", { class: "kbd-oct" }, oct("OCT-", "−"), oct("OCT+", "+")), el("div", { class: "keys-s" }, keys)));
  }
  function keysChanged() {
    let m = 0;
    for (const k of held.values()) m |= 1 << k;
    m >>>= 0;
    if (m === S.keysDown) return;
    S.keysDown = m;
    post({ type: "keys", mask: m });
    keyEls.forEach((b, k) => b.classList.toggle("down", !!(m >>> k & 1)));
  }
  function renderKeys() {
    const m = S.keys;
    keyEls.forEach((b, k) => b.classList.toggle("lit", !!(m >>> k & 1)));
  }
  function keyCaption() {
    if (!capName) return;
    capName.textContent = nameOf(S.piece);
    capAir.textContent = AIRS[S.piece];
  }
  function keyLabels() {
    if (!keyEls.length) return;
    const perf = S.view === V_PERF || S.view === V_MIX;
    const names = new Map();
    if (perf) {
      for (let i = 0; i < Q.ed_perf_n(); i++) { const n = qstr(Q.ed_perf_name(i)); names.set(Q.ed_perf_key(i), n[0] + n.slice(1).toLowerCase()); }
      if (Q.ed_perf_shift() >= 0) names.set(Q.ed_perf_shift(), "Mix");
      if (Q.ed_perf_forget() >= 0) names.set(Q.ed_perf_forget(), "Forget");
    } else if (S.view === V_CHORDS) {
      keyEls.forEach((b, k) => names.set(k, valueText(P_ROOT, (5 + k) % 12)));
    } else if (S.view === V_PATTERN) {
      WHITE_K.forEach((k, i) => names.set(k, String(i + 1)));
    } else {
      names.set(0, "Root");
      for (let b = 0; b < Q.ed_nblack(); b++) names.set(Q.ed_black_key(b), qstr(Q.ed_black_name(b)));
    }
    const effects = !perf && S.view !== V_CHORDS && S.view !== V_PATTERN;
    keyEls.forEach((b, k) => {
      const t = names.get(k) || "";
      if (t) b.replaceChildren(el("span", { class: t.length <= 3 ? "k-t plain" : "k-t" }, t)); else b.replaceChildren();
      b.title = t && effects && BLACK_K.includes(k) ? `${t}, while held` : t;
      b.classList.toggle("perf", perf);
    });
    capHint.textContent = perf ? "Every key is an effect on the whole music, for as long as it's held"
      : S.view === V_CHORDS ? "The keys set this slot's root"
      : S.view === V_PATTERN ? "White keys are this lane's steps. Hold a black key with one to accent it."
      : "White keys play the Lead, black keys are effects while held";
  }

  /* ---------- the screen, and what colours it might have ---------- */
  const screens = [];
  let img, px;
  const LUT = new Uint32Array(65536);
  const pack = (r, g, b) => (255 << 24 | Math.min(255, b + 0.5 | 0) << 16 | Math.min(255, g + 0.5 | 0) << 8 | Math.min(255, r + 0.5 | 0)) >>> 0;
  const rgbOf565 = (p) => [((p >> 11) & 31) * 255 / 31, ((p >> 5) & 63) * 255 / 63, (p & 31) * 255 / 31];
  const hex = (c) => "#" + c.map((x) => Math.round(clamp(x, 0, 255)).toString(16).padStart(2, "0")).join("");
  const unhex = (s) => [1, 3, 5].map((i) => parseInt(s.slice(i, i + 2), 16));
  const lerp = (a, b, t) => a.map((x, i) => x + (b[i] - x) * t);

  // The firmware's colours are built in, so a look here repaints its picture. Each pixel is matched to the nearest
  // firmware colour at some strength (which also places the antialiased edges) and repainted from the new one.
  function colourOf(v) {
    const p = ((v >> 8) | (v << 8)) & 0xffff, c = rgbOf565(p);
    if (S.identity) return pack(c[0], c[1], c[2]);
    let best = Infinity, e0 = null, t0 = 0;
    for (const e of S.pal.list) {
      const t = clamp((c[0] * e.s[0] + c[1] * e.s[1] + c[2] * e.s[2]) / e.d, 0, 1);
      const dr = c[0] - t * e.s[0], dg = c[1] - t * e.s[1], db = c[2] - t * e.s[2], err = dr * dr + dg * dg + db * db;
      if (err < best) { best = err; e0 = e; t0 = t; }
    }
    const o = lerp(S.pal.bg, e0.t, t0);
    return pack(o[0], o[1], o[2]);
  }
  function drawScreen() {
    S.fbDirty = false;
    const fb = S.fb;
    if (!fb) return;
    for (let i = 0; i < 57600; i++) {
      const v = fb[i];
      let o = LUT[v];
      if (!o) o = LUT[v] = colourOf(v);
      px[i] = o;
    }
    for (const s of screens) if (s.view === S.main) s.ctx.putImageData(img, 0, 0);
  }

  const LOOK_NAMES = ["Background", "Panel", "Line", "Dim", "Text", "Accent", "Drone", "Sequence", "Lead", "Seq 2", "Atmosphere"];
  let fwColours, swatches = [], chips = [], lookShare, lookStatus;
  const lumaOf = (r, g, b) => (0.3 * r + 0.59 * g + 0.11 * b) / 255;
  const ease = (a, b, x) => { const t = clamp((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); };
  const inkOf = (r, g, b) => { const l = lumaOf(r, g, b); return l < 0.12 ? 0 : 0.28 + 0.72 * ease(0.3, 0.5, l); };
  const LOOKS = {
    AISLING: () => fwColours.slice(0, 11).map(hex),
    Vellum: () => ["#f3ead7", "#e4d8c0", "#d9ccb1", "#5b4a3a", "#2b2219", "#146c41", "#6a4bc0", "#b8308f", "#8a6414", "#1f7fa8", "#6f6554"],
    Bright: () => fwColours.slice(0, 11).map(([r, g, b]) => { const top = Math.max(r, g, b, 1), k = 1 + ease(0.3, 0.5, lumaOf(r, g, b)) * (255 / top - 1); return hex([r * k, g * k, b * k]); }),
    Yellow: () => fwColours.slice(0, 11).map(([r, g, b]) => { const i = inkOf(r, g, b); return hex([255 * i, 224 * i, 0]); }),
    White: () => fwColours.slice(0, 11).map(([r, g, b]) => { const i = inkOf(r, g, b) * 255; return hex([i, i, i]); }),
    Paper: () => fwColours.slice(0, 11).map(([r, g, b]) => { const i = (1 - inkOf(r, g, b)) * 255; return hex([i, i, i]); }),
  };
  function applyLook(name, colours, quiet) {
    S.look = colours;
    S.lookName = name;
    const base = LOOKS.AISLING();
    S.identity = colours.every((c, i) => c.toLowerCase() === base[i]);
    const rgb = colours.map(unhex), bg = rgb[0], list = [];
    const add = (s, t) => { const d = s[0] * s[0] + s[1] * s[1] + s[2] * s[2]; if (d) list.push({ s, t, d }); };
    for (let i = 0; i <= 10; i++) add(fwColours[i], rgb[i]);
    for (let i = 0; i < 4; i++) add(fwColours[11 + i], lerp(bg, rgb[6 + i], 0.33));
    for (let i = 15; i < fwColours.length; i++) add(fwColours[i], fwColours[i]);
    S.pal = { bg, list };
    LUT.fill(0);
    S.fbDirty = true;
    schedule();
    // the strips wear the layers' colours, while they would read on the dark panel
    const root = $("ed").style, dark = lumaOf(...bg) < 0.3;
    for (let i = 0; i < 5; i++) {
      const c = dark ? rgb[6 + i] : fwColours[6 + i];
      root.setProperty(`--l${i}`, hex(c));
      if (i < 4) root.setProperty(`--t${i}`, hex(dark ? lerp(bg, rgb[6 + i], 0.33) : fwColours[11 + i]));
    }
    swatches.forEach((s, i) => { s.input.value = colours[i]; s.code.textContent = colours[i]; });
    chips.forEach((c) => c.el.setAttribute("aria-pressed", c.name === name ? "true" : "false"));
    if (lookShare) lookShare.value = "AISLING-LOOK-1:" + colours.map((c) => c.slice(1)).join(".");
    if (!quiet) lsSet(KEY_LOOK, JSON.stringify({ name, colours }));
  }
  function buildLook() {
    const canvas = el("canvas", { width: 240, height: 240, role: "img", "aria-label": "The FM-1's screen, live, in this look" });
    screens.push({ view: "look", ctx: canvas.getContext("2d") });
    chips = Object.keys(LOOKS).map((name) => {
      const c = LOOKS[name]();
      const b = el("button", { class: "chip", type: "button", "aria-pressed": "false", "data-quiet": true, onclick: () => applyLook(name, LOOKS[name]()) },
        el("i", { style: `background:${c[0]}` }), el("i", { style: `background:${c[8]}` }), name);
      return { el: b, name };
    });
    const swatch = (i) => {
      const input = el("input", { type: "color", "aria-label": LOOK_NAMES[i], "data-quiet": true, value: "#000000", oninput: () => {
        const c = S.look.slice();
        c[i] = input.value;
        applyLook("Custom", c);
      } });
      const code = el("code");
      swatches[i] = { input, code };
      return el("label", { class: "swatch" }, LOOK_NAMES[i], code, input);
    };
    lookShare = el("input", { class: "field", type: "text", spellcheck: "false", "aria-label": "This look as one line, to copy or paste", "data-quiet": true });
    lookStatus = el("p", { class: "lib-status", role: "status" });
    const paste = () => {
      const m = /^AISLING-LOOK-1:((?:[0-9a-fA-F]{6}\.){10}[0-9a-fA-F]{6})$/.exec(lookShare.value.trim());
      if (!m) { lookStatus.textContent = "That isn't a look. A look starts AISLING-LOOK-1: and has eleven colours."; lookStatus.classList.add("bad"); return; }
      applyLook("Custom", m[1].split(".").map((c) => "#" + c.toLowerCase()));
      lookStatus.textContent = "Look applied.";
      lookStatus.classList.remove("bad");
    };
    const copy = () => copyText(lookShare.value).then((ok) => {
      lookStatus.textContent = ok ? "Copied." : "Copy didn't work here. The line is selected, so copy it by hand.";
      lookStatus.classList.toggle("bad", !ok);
      if (!ok) lookShare.select();
    });
    $("v-look").append(el("div", { class: "lk" },
      el("div", { class: "well lk-l" }, el("div", { class: "bezel" }, canvas), el("div", { class: "chips", role: "group", "aria-label": "Looks" }, chips.map((c) => c.el))),
      el("div", { class: "well lk-r" },
        el("div", null, el("h2", null, "The screen"), el("div", { class: "swatches" }, [0, 1, 2, 3, 4, 5].map(swatch))),
        el("div", null, el("h2", null, "The layers"), el("div", { class: "swatches" }, [6, 7, 8, 9, 10].map(swatch))),
        el("div", null, el("h2", null, "Share it"), el("div", { class: "lk-share" }, lookShare,
          el("button", { class: "ebtn", type: "button", "data-quiet": true, onclick: copy }, "Copy"),
          el("button", { class: "ebtn", type: "button", "data-quiet": true, onclick: paste }, "Use a pasted line")), lookStatus),
        el("div", { class: "lk-act" }, dashed("Send to FM-1", "Sending a look arrives with a later firmware: for now the FM-1's colours are built in."),
          dashed("Read from FM-1", "Reading a look arrives with a later firmware: for now the FM-1's colours are built in.")),
        el("p", null, "This repaints the picture on this page. For now the FM-1's own colours are built in, so nothing here reaches it yet."))));
  }

  async function copyText(t) {
    try { await navigator.clipboard.writeText(t); return true; } catch (e) { return false; }
  }

  /* ---------- the library ---------- */
  let libRows = [], shelfList, shelfMeta, libStatus, pickBar, pickText, shareBox, shareField, keepBtn, placeBtn, libSub, libConnect;
  const undoLine = (t) => (S.link ? "" : ` ${t}`);   // the FM-1's undo holds the piece playing, not the library
  const noRead = () => status("That couldn't be read from the FM-1. Check the cable and try again.", true);
  const status = (t, bad) => { libStatus.textContent = t; libStatus.classList.toggle("bad", !!bad); };
  const today = () => new Date().toLocaleDateString("en-GB", { day: "numeric", month: "short" });
  const FWV = () => qstr(Q.ed_version());
  function saveShelf() {
    if (!lsSet(KEY_SHELF, JSON.stringify(S.shelf))) status("This browser wouldn't keep that between visits. It's here until you close the page.", true);
  }
  function buildLibrary() {
    const list = el("ol", { class: "lib-list" });
    for (let i = 0; i < Q.ed_npieces(); i++) {
      const meta = el("span", { class: "row-meta" }), sub = el("span", { class: "row-sub" });
      const b = el("button", { class: "row", type: "button", "aria-pressed": "false", onclick: () => pickRow(i) },
        el("span", { class: "row-n" }, i + 1), el("span", { class: "row-name" }, nameOf(i)), meta, sub);
      libRows.push({ b, meta, sub });
      list.append(el("li", null, b));
    }
    shelfList = el("ol", { class: "lib-list" });
    shelfMeta = el("span", { class: "lib-meta" });
    const fileIn = el("input", { type: "file", accept: ".json,application/json", hidden: true, onchange: () => { if (fileIn.files[0]) openFile(fileIn.files[0], "piece"); fileIn.value = ""; } });
    const backupIn = el("input", { type: "file", accept: ".json,application/json", hidden: true, onchange: () => { if (backupIn.files[0]) openFile(backupIn.files[0], "backup"); backupIn.value = ""; } });
    const paste = el("input", { class: "field", type: "text", spellcheck: "false", placeholder: "paste a share line", "aria-label": "Paste a share line", "data-quiet": true });
    keepBtn = el("button", { class: "ebtn", type: "button", title: "Keep the chosen piece on this computer", onclick: keepPiece }, "Keep", icon("M2 8h11M9 4l4 4-4 4"));
    placeBtn = el("button", { class: "ebtn", type: "button", title: "Put what you chose from this computer in the chosen piece's place", onclick: placePiece }, icon("M14 8H3M7 4 3 8l4 4"), "Place");
    pickText = el("span");
    pickBar = el("div", { class: "lib-pick", hidden: true }, pickText, el("button", { class: "ebtn", type: "button", "data-quiet": true, onclick: cancelPick }, "Cancel"));
    shareField = el("input", { class: "field", type: "text", readonly: true, "aria-label": "This piece as one line", "data-quiet": true });
    shareBox = el("div", { class: "lib-share", hidden: true }, shareField, el("button", { class: "ebtn", type: "button", "data-quiet": true, onclick: () => copyText(shareField.value).then((ok) => {
      status(ok ? "Copied." : "Copy didn't work here. The line is selected, so copy it by hand.", !ok);
      if (!ok) shareField.select();
    }) }, "Copy"));
    libStatus = el("p", { class: "lib-status", id: "lib-status", role: "status" });
    $("v-library").append(
      el("div", { class: "lib" },
        el("div", { class: "well lib-w" },
          el("div", { class: "lib-h" }, el("h2", null, "Your eight pieces")),
          libSub = el("p", { class: "lib-sub" }),
          list,
          el("div", { class: "lib-foot" }, libConnect = el("button", { class: "ebtn", type: "button", "data-quiet": true, onclick: connect }, "Connect an FM-1"))),
        el("div", { class: "lib-mid" }, keepBtn, placeBtn),
        el("div", { class: "well lib-w" },
          el("div", { class: "lib-h" }, el("h2", null, "On this computer"), shelfMeta),
          el("div", { class: "lib-tools" },
            el("button", { class: "ebtn", type: "button", onclick: () => fileIn.click(), "data-quiet": true }, "Open a file"), paste,
            el("button", { class: "ebtn", type: "button", "data-quiet": true, onclick: () => { addShared(paste.value); paste.value = ""; } }, "Add")),
          shelfList, fileIn, backupIn)),
      pickBar,
      el("div", { class: "lib-act" },
        el("button", { class: "ebtn", type: "button", onclick: () => startPick("swap") }, "Swap with…"),
        el("button", { class: "ebtn", type: "button", onclick: () => startPick("copy") }, "Copy to…"),
        el("button", { class: "ebtn", type: "button", title: "On the FM-1: hold GLO for 1.5 s", onclick: resetPiece }, "Reset to recipe"),
        el("span", { class: "gap" }),
        el("button", { class: "ebtn", type: "button", onclick: savePieceFile }, "Save as a file"),
        el("button", { class: "ebtn", type: "button", onclick: sharePiece }, "Share"),
        el("span", { class: "gap" }),
        el("button", { class: "ebtn", type: "button", onclick: backup }, "Back up everything"),
        el("button", { class: "ebtn", type: "button", onclick: () => backupIn.click(), "data-quiet": true }, "Install a backup")),
      shareBox, libStatus);
    renderShelf();
  }
  function renderLibrary() {
    if (!libRows.length) return;
    libRows.forEach((r, i) => {
      const p = S.pieces[i];
      r.b.setAttribute("aria-pressed", i === S.libSel ? "true" : "false");
      if (!p) return;
      r.meta.textContent = keyLine(p);
      r.sub.replaceChildren(AIRS[i]);
      if (p.edited) r.sub.append(" · ", el("span", { class: "row-flag" }, "edited"));
      if (i === S.piece) r.sub.append(" · ", el("span", { class: "row-play" }, "open now"));
    });
  }
  function renderShelf() {
    shelfMeta.textContent = S.shelf.length === 1 ? "1 kept" : `${S.shelf.length} kept`;
    shelfList.replaceChildren();
    if (!S.shelf.length) shelfList.append(el("li", { class: "lib-empty" }, "Nothing kept yet. Keep one of your eight, open a file, or paste a line someone shared."));
    S.shelf.forEach((it, i) => {
      const b = el("button", { class: "row", type: "button", "aria-pressed": i === S.shelfSel ? "true" : "false", "data-quiet": true,
        onclick: () => { S.shelfSel = i === S.shelfSel ? -1 : i; renderShelf(); } },
        el("span", { class: "row-n" }, i + 1), el("span", { class: "row-name" }, it.label), el("span", { class: "row-meta" }, it.meta || ""));
      shelfList.append(el("li", { class: "shelf" }, b, el("button", { class: "mini", type: "button", "data-quiet": true, "aria-label": `Delete ${it.label}`, onclick: () => {
        S.shelf.splice(i, 1);
        if (S.shelfSel === i) S.shelfSel = -1; else if (S.shelfSel > i) S.shelfSel--;
        saveShelf();
        renderShelf();
      } }, "Delete")));
    });
  }
  function pickRow(i) {
    if (S.pick) {
      const from = S.pick.from, kind = S.pick.kind;
      cancelPick();
      if (i === from) { status("That's the same piece: nothing changed."); return; }
      if (kind === "swap") {
        post({ type: "swap", a: from, b: i, label: `Swap ${nameOf(from)} and ${nameOf(i)}` });
        status(`Swapped ${nameOf(from)} and ${nameOf(i)}.${undoLine("Undo puts them back.")}`);
      } else {
        post({ type: "copy", from, to: i, label: `Copy ${nameOf(from)} to ${nameOf(i)}` });
        status(`Copied ${nameOf(from)} over ${nameOf(i)}.${undoLine("Undo puts it back.")}`);
      }
      return;
    }
    S.libSel = i;
    renderLibrary();
  }
  function startPick(kind) {
    S.pick = { kind, from: S.libSel };
    pickText.textContent = kind === "swap" ? `Swap ${nameOf(S.libSel)} with which piece?` : `Copy ${nameOf(S.libSel)} over which piece?`;
    pickBar.hidden = false;
  }
  function cancelPick() { S.pick = null; pickBar.hidden = true; }
  async function keepPiece() {
    const i = S.libSel, r = await request({ type: "read-slot", i }), p = S.pieces[i];
    if (!r.bytes) { noRead(); return; }
    addToShelf({ label: `${nameOf(i)}, ${today()}`, data: b64(r.bytes), meta: p ? keyLine(p) : "" });
    status(`Kept ${nameOf(i)} on this computer.`);
  }
  function addToShelf(it) {
    S.shelf.unshift(it);
    if (S.shelf.length > 64) S.shelf.length = 64;
    S.shelfSel = 0;
    saveShelf();
    renderShelf();
  }
  async function placePiece() {
    const it = S.shelf[S.shelfSel];
    if (!it) { status("Choose something on the right to place first.", true); return; }
    const i = S.libSel, r = await request({ type: "install-slot", i, bytes: unb64(it.data), label: `Place ${it.label} in ${nameOf(i)}` });
    if (r.ok < 0) status("That piece wouldn't load: its data doesn't pass the firmware's check.", true);
    else status(`Placed ${it.label} in ${nameOf(i)}.${undoLine("Undo puts it back.")}`);
  }
  function resetPiece() {
    const i = S.libSel;
    post({ type: "reset", i, label: `Reset ${nameOf(i)}` });
    status(`${nameOf(i)} is back to its recipe.${undoLine("Undo puts your version back.")}`);
  }
  function download(name, obj) {
    const url = URL.createObjectURL(new Blob([JSON.stringify(obj, null, 1)], { type: "application/json" }));
    const a = el("a", { href: url, download: name });
    document.body.append(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 30000);
  }
  const stampFile = (format, name, bytes) => ({ format, firmware: S.link ? S.link.hello.version : FWV(), proj_format: Q.ed_format(), nparams: Q.ed_nparams(), name, saved: new Date().toISOString(), size: bytes.length, data: b64(bytes) });
  const slug = (s) => s.toLowerCase().replace(/[^a-z0-9]+/g, "-");
  async function savePieceFile() {
    const i = S.libSel, r = await request({ type: "read-slot", i });
    if (!r.bytes) { noRead(); return; }
    download(`aisling-${slug(nameOf(i))}.json`, { ...stampFile("aisling-piece", nameOf(i), r.bytes), meta: S.pieces[i] ? keyLine(S.pieces[i]) : "" });
    status(`Saved ${nameOf(i)} as a file.`);
  }
  async function backup() {
    const r = await request({ type: "read-proj" });
    if (!r.bytes) { noRead(); return; }
    download(`aisling-backup-${new Date().toISOString().slice(0, 10)}.json`, stampFile("aisling-backup", "All eight pieces", r.bytes));
    status("Backed up all eight pieces and the settings to a file.");
  }
  async function sharePiece() {
    const i = S.libSel, r = await request({ type: "read-slot", i });
    if (!r.bytes) { noRead(); return; }
    shareField.value = `AISLING-PIECE-1:${Q.ed_format()}.${Q.ed_nparams()}:${b64url(r.bytes)}`;
    shareBox.hidden = false;
    shareField.focus();
    shareField.select();
    status(`${nameOf(i)} as one line. Anyone with AISLING can paste it into "On this computer".`);
  }
  function metaOf(bytes) {                       /* a piece's key and tempo, read from its bytes on the quiet copy, not installed */
    new Uint8Array(Qmem.buffer, Q.ed_slot_in(), bytes.length).set(bytes);
    return keyLine({ root: Q.ed_in_par(P_ROOT), mode: Q.ed_in_par(P_MODE), tempo: Q.ed_in_par(P_TEMPO) });
  }
  function addShared(line) {
    const m = /^AISLING-PIECE-1:(\d+)\.(\d+):([A-Za-z0-9_-]+)$/.exec((line || "").trim());
    if (!m) { status("That isn't a piece. A shared piece starts AISLING-PIECE-1: and is one long line.", true); return; }
    if (+m[1] !== Q.ed_format() || +m[2] !== Q.ed_nparams()) { status("That piece is from a different firmware version, so it won't load in this one.", true); return; }
    let bytes;
    try { bytes = unb64url(m[3]); } catch (e) { status("That line is damaged: part of it is missing.", true); return; }
    if (bytes.length !== Q.ed_slot_size()) { status("That line is damaged: it's the wrong length.", true); return; }
    addToShelf({ label: `Shared piece, ${today()}`, data: b64(bytes), meta: metaOf(bytes) });
    status("Added. Choose where it goes on the left, then Place.");
  }
  async function openFile(file, want) {
    let o;
    try { o = JSON.parse(await file.text()); } catch (e) { status("That file isn't an AISLING piece or backup.", true); return; }
    if (!o || (o.format !== "aisling-piece" && o.format !== "aisling-backup") || typeof o.data !== "string") { status("That file isn't an AISLING piece or backup.", true); return; }
    if (o.proj_format !== Q.ed_format() || o.nparams !== Q.ed_nparams()) { status(`That file is from firmware ${o.firmware}, which keeps pieces differently, so it won't load in this one.`, true); return; }
    let bytes;
    try { bytes = unb64(o.data); } catch (e) { status("That file is damaged.", true); return; }
    if (want === "piece") {
      if (o.format !== "aisling-piece") { status("That's a backup of all eight: use Install a backup.", true); return; }
      if (bytes.length !== Q.ed_slot_size()) { status("That file is damaged: it's the wrong length.", true); return; }
      addToShelf({ label: String(o.name || file.name).slice(0, 40), data: b64(bytes), meta: typeof o.meta === "string" ? o.meta.slice(0, 40) : "" });
      status("Added. Choose where it goes on the left, then Place.");
    } else {
      if (o.format !== "aisling-backup") { status("That's a single piece: use Open a file.", true); return; }
      if (bytes.length !== Q.ed_proj_size()) { status("That file is damaged: it's the wrong length.", true); return; }
      const r = await request({ type: "install-proj", bytes, label: "Install backup" });
      if (r.ok < 0) status("That backup wouldn't load: its data doesn't pass the firmware's check, and nothing changed.", true);
      else status(`Backup installed.${undoLine("Undo puts things back as they were.")}`);
    }
  }

  /* ---------- the store, as the FM-1's flash ---------- */
  function loadStore() {
    const out = {};
    try {
      const o = JSON.parse(lsGet(KEY_STORE) || "{}");
      for (const k of Object.keys(o)) out[k] = unb64(o[k]).buffer;
    } catch (e) { /* nothing kept, or unreadable: the recipes */ }
    return out;
  }
  let storeTimer = 0, storeLatest = null;
  function saveStore(store) {
    storeLatest = store;
    if (storeTimer) return;
    storeTimer = setTimeout(() => {
      storeTimer = 0;
      const o = {};
      for (const k of Object.keys(storeLatest)) o[k] = b64(storeLatest[k]);
      lsSet(KEY_STORE, JSON.stringify(o));
    }, 400);
  }

  /* ---------- views ---------- */
  function showMain(name) {
    S.main = name;
    for (const n of ["play", "library", "look"]) {
      const on = n === name, tab = $(`tab-${n}`);
      tab.setAttribute("aria-selected", on ? "true" : "false");
      tab.tabIndex = on ? 0 : -1;
      $(`v-${n}`).hidden = !on;
    }
    S.fbDirty = true;
    schedule();
  }

  /* ---------- the FM-1, over USB (link.js; firmware/src/app/link.c) ---------- */
  async function connect() {
    if (S.link) { disconnect("Disconnected from the FM-1. It carries on by itself; this page's own copy takes over on your next click."); return; }
    if (S.linking) return;
    if (!navigator.requestMIDIAccess) { note("Connecting needs Chrome or Edge: this browser has no USB MIDI."); return; }
    S.linking = true;
    renderLink();
    note("Looking for an FM-1. Chrome may ask to allow MIDI access.");
    const fail = (t) => { S.linking = false; renderLink(); note(t); };
    let access;
    try { access = await navigator.requestMIDIAccess({ sysex: true }); }
    catch (e) { fail("MIDI access wasn't allowed, so nothing is connected. You can allow it from the padlock by the address."); return; }
    const ports = globalThis.AislingLink.findPorts(access);
    if (!ports) { fail("No FM-1 found. Plug it in over USB with AISLING running, then press Connect again."); return; }
    const link = new globalThis.AislingLink(ports.input, ports.output, {
      onState: (st) => { if (S.link === link) linkState(st); },
      onVals: (v) => { if (S.link === link) linkVals(v); },
      onReply: (m) => onWorklet(m),
      onGone: () => { if (S.link === link) disconnect("The FM-1 has gone: the cable, or it stopped answering. This page's own copy takes over on your next click."); },
    });
    const hello = await link.open();
    if (!hello) { fail("This FM-1 doesn't answer. The editor needs AISLING 0.5 or later on it."); return; }
    if (hello.ver !== 1 || hello.format !== Q.ed_format() || hello.npars !== Q.ed_nparams() || hello.proj !== Q.ed_proj_size() || hello.slot !== Q.ed_slot_size()) {
      link.close();
      fail(`This FM-1 runs AISLING ${hello.version}, which keeps pieces differently from this page (${FWV()}), so the editor won't connect to it.`);
      return;
    }
    flushSets();
    if (S.liftAll) S.liftAll();                      // (anything held on this page's own copy lets go before it goes quiet)
    if (S.ac && S.ac.state !== "suspended") S.ac.suspend();
    S.link = link;
    S.linking = false;
    S.access = access;
    access.onstatechange = (e) => {
      if (e.port && (e.port.id === ports.input.id || e.port.id === ports.output.id) && e.port.state === "disconnected") link.gone("unplugged");
    };
    S.hold.clear(); S.echo.clear(); S.sets.clear();
    S.linkUn = S.linkRn = S.linkGen = S.linkPiece = S.linkRaw = -1;
    $("note").hidden = true;
    renderLink();
  }
  function disconnect(text) {
    const link = S.link;
    if (S.liftAll) S.liftAll();                      // while it's still connected, so the release goes out
    flushSets();
    S.link = null;
    link.close();
    if (S.access) S.access.onstatechange = null;
    S.saving = false;
    S.pieceHold = S.viewHold = 0;
    S.piecesAt = S.piecesBy = 0;
    S.hold.clear(); S.echo.clear(); S.sets.clear();
    masterCtl.render(S.master, `${Math.round(100 * S.master / 4096)}%`);
    // The context stays suspended until the next gesture: a pulled cable never starts sound by itself
    if (S.started) post({ type: "resend" });
    else { fromQuiet(); reshow(); }
    renderSave();
    renderLink();
    note(text);
  }
  function linkState(st) {
    const frame = { type: "frame", keys: st.keys, buttons: st.lit, view: st.view, playing: st.playing, frozen: st.frozen, dirty: st.dirty,
      piece: st.piece, moves: st.moves, act: st.act.map((a) => a / 127), muted: st.muted, solo: st.solo, sounds: st.sounds };
    if (st.un !== S.linkUn || st.rn !== S.linkRn) {
      S.linkUn = st.un; S.linkRn = st.rn;
      frame.hist = { undo: st.un ? `(${st.un})` : null, redo: st.rn ? `(${st.rn})` : null };
    }
    onWorklet(frame);
    if (st.saving !== S.saving) { S.saving = st.saving; renderSave(); }
    if (st.raw !== S.linkRaw) {                      // the knob itself: a quiet test build forces what it sets to 0
      S.linkRaw = st.raw;
      masterCtl.render(Math.round(st.raw * 4096 / 1023), `${Math.round(100 * st.raw / 1023)}%`);
    }
    $("link-state").title = `CPU ${st.cpu}%, peak ${st.peak}%, ${st.xruns} dropouts`;
    if (st.gen !== S.linkGen) { S.linkGen = st.gen; wantPieces(0); }
    if (st.piece !== S.linkPiece) { S.linkPiece = st.piece; wantPieces(300); }
    if (S.piecesAt && now() >= S.piecesAt) readPieces();
  }
  function linkVals(vals) {                          // settings moved: read the eight again once they rest, at least every 5 s
    const t = now();
    takeVals(vals, t);
    schedule();
    if (!S.piecesBy) S.piecesBy = t + 5000;
    S.piecesAt = Math.min(t + 1000, S.piecesBy);
  }
  function wantPieces(ms) {
    const at = now() + ms;
    S.piecesAt = S.piecesAt ? Math.min(S.piecesAt, at) : at;
  }
  async function readPieces() {
    const link = S.link;
    S.piecesAt = S.piecesBy = 0;
    if (S.piecesBusy || !link) return;
    S.piecesBusy = true;
    const b = await link.read(0);
    S.piecesBusy = false;
    if (S.link !== link) return;
    if (!b) { wantPieces(1000); return; }
    S.pieces = piecesOf(b);
    renderPieces();
  }
  function piecesOf(b) {                             // the cards' and the library's lines, from a project's bytes
    const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
    const parAt = Q.ed_layout(0), poolAt = Q.ed_layout(1), pieceAt = Q.ed_layout(2), slotAt = Q.ed_layout(3), slotPool = Q.ed_layout(4), slotN = Q.ed_slot_size();
    const playing = b[pieceAt], out = [];
    for (let i = 0; i < Q.ed_npieces(); i++) {
      const slot = slotAt + slotN * i, base = i === playing ? parAt : slot;
      const pool = dv.getUint16(i === playing ? poolAt : slot + slotPool, true);
      const par = (p) => dv.getInt16(base + 2 * p, true);
      let edited = pool !== Q.ed_recipe_pool(i);
      for (let p = 0; p < NPARAMS && !edited; p++) if (!Q.ed_is_global(p) && par(p) !== Q.ed_recipe(i, p)) edited = true;
      const voices = [];
      for (let l = 0; l < Q.ed_nlayers(); l++) voices.push(par(Q.ed_layer_voice(l)));
      out.push({ tempo: par(P_TEMPO), root: par(P_ROOT), mode: par(P_MODE), edited, voices });
    }
    return out;
  }
  function fromQuiet() {                             // what the page shows when nothing has played: the quiet copy
    for (const k of S.knobs) S.vals.set(k, { v: Q.ed_get(k), text: qstr(Q.ed_text(k)) });
    S.pieces = globalThis.AislingEngine.pieces(Q);
    S.piece = Q.ed_piece_now();
    S.view = Q.ed_view_now();
    S.dirty = false;
    S.playing = S.frozen = false;
    S.muted = S.solo = 0;
    S.sounds = 31;
    S.keys = S.buttons = 0;
    S.act = [0, 0, 0, 0, 0];
    for (let i = 0; i < 5; i++) if (!Q.ed_layer_sounds(i)) S.sounds &= ~(1 << i);
  }
  function reshow() {
    for (const k of S.knobs) show(k);
    setView(S.view);
    pieceMoved();
    renderPieces();
    renderPlay();
    renderSave();
    renderStrips();
    renderKeys();
    renderLights();
    showHist({ undo: null, redo: null });
    schedule();
  }
  function renderLink() {
    const on = !!S.link, pill = $("link-state"), b = $("connect");
    pill.classList.toggle("on", on);
    pill.lastChild.textContent = on ? `FM-1 ${S.link.hello.version}` : S.linking ? "Looking…" : "No FM-1";
    if (!on) pill.title = "";
    b.textContent = on ? "Disconnect" : "Connect";
    b.classList.toggle("fill", !on);
    document.body.classList.toggle("ed-linked", on);
    if (on) volNode.setAttribute("aria-disabled", "true"); else volNode.removeAttribute("aria-disabled");
    libSub.textContent = on
      ? "On your FM-1, read over USB. Swap, Copy, Place, Reset and Install change it straight away, and its undo doesn't cover them: Back up everything first."
      : "Held in this page's copy of AISLING. Connect an FM-1 to work on its own pieces.";
    libConnect.hidden = on;
    $("foot").textContent = on
      ? "Connected to your FM-1 over USB. This page makes no sound, and what you change here changes the FM-1."
      : `Everything here runs on a copy of AISLING ${FWV()} in this page. Connect reaches your FM-1 over USB.`;
  }

  /* ---------- starting up ---------- */
  async function init() {
    const stored = parseInt(lsGet(KEY_MASTER) || "", 10);
    if (Number.isFinite(stored)) S.master = clamp(stored, 0, 4096);
    const res = await fetch("amb.wasm");
    if (!res.ok) throw new Error(`amb.wasm ${res.status}`);
    S.wasm = await res.arrayBuffer();
    let t = 0;
    const eng = await globalThis.AislingEngine.start(S.wasm.slice(0), () => t, loadStore(), S.master);
    Q = eng.ex;
    Qmem = eng.mem;
    for (let i = 0; i < 130; i++) {          // about three seconds, unheard, so there's a real screen to show
      t += 1024 / 44100;
      Q.web_render(1024);
      Q.web_midi_clear();
      Q.web_said_clear();
    }
    K_NONE = Q.ed_k_none(); K_CPU = Q.ed_k_cpu(); K_SAVE = Q.ed_k_save(); K_MODE = Q.ed_k_mode();
    P_TEMPO = Q.ed_p_tempo(); P_ROOT = Q.ed_p_root(); P_MODE = Q.ed_p_mode(); NPARAMS = Q.ed_nparams();

    const byName = new Map();
    for (let i = Q.ed_lo(K_MODE); i <= Q.ed_hi(K_MODE); i++) byName.set(valueText(K_MODE, i), i);
    for (let raw = Q.ed_lo(P_MODE); raw <= Q.ed_hi(P_MODE); raw++) {
      const o = byName.get(valueText(P_MODE, raw));
      if (o != null) modeOrder.set(raw, o);
    }

    fwColours = [];
    for (let i = 0; i < Q.ed_ncolours(); i++) fwColours.push(rgbOf565(Q.ed_colour(i)));
    S.look = LOOKS.AISLING();

    const list = new Set();
    for (let v = 0; v < Q.ed_nviews(); v++) {
      for (let i = 0; i < 4; i++) {
        const k = Q.ed_view_knob(v, i);
        if (k !== K_NONE && k !== K_CPU && k !== K_SAVE) list.add(k);
      }
    }
    for (let l = 0; l < Q.ed_nlayers(); l++) { list.add(Q.ed_layer_level(l)); list.add(Q.ed_layer_voice(l)); }
    list.add(P_TEMPO); list.add(P_ROOT); list.add(K_MODE);
    S.knobs = [...list];
    fromQuiet();

    const shelf = lsGet(KEY_SHELF);
    if (shelf) { try { const a = JSON.parse(shelf); if (Array.isArray(a)) S.shelf = a; } catch (e) { /* start with none */ } }

    img = new ImageData(240, 240);
    px = new Uint32Array(img.data.buffer);
    screens.push({ view: "play", ctx: $("screen").getContext("2d") });
    S.fb = new Uint16Array(Qmem.buffer, Q.web_fb(), 57600).slice();

    buildTransport();
    buildCards();
    buildStrips();
    buildPages();
    buildKeyboard();
    buildLibrary();
    buildLook();

    for (const k of S.knobs) show(k);
    for (const set of controls.values()) for (const c of set) if (c.home) c.home();
    showTab(tabOfView[S.view]);
    setView(S.view);
    pieceMoved();
    renderPieces();
    renderPlay();
    renderSave();
    renderStrips();
    keyCaption();
    keyLabels();
    setHelp();
    showHist(S.hist);

    let saved = null;
    try { saved = JSON.parse(lsGet(KEY_LOOK) || "null"); } catch (e) { saved = null; }
    if (saved && Array.isArray(saved.colours) && saved.colours.length === 11 && saved.colours.every((c) => /^#[0-9a-f]{6}$/i.test(c))) applyLook(saved.name || "Custom", saved.colours, true);
    else applyLook("AISLING", S.look, true);

    const mains = ["play", "library", "look"];
    mains.forEach((n, i) => {
      const tab = $(`tab-${n}`);
      tab.addEventListener("click", () => showMain(n));
      tab.addEventListener("keydown", (e) => {
        const d = { ArrowRight: 1, ArrowLeft: -1 }[e.key];
        if (!d) return;
        e.preventDefault();
        const m = mains[(i + d + mains.length) % mains.length];
        showMain(m);
        $(`tab-${m}`).focus();
      });
    });
    $("undo").addEventListener("click", () => post({ type: "undo" }));
    $("redo").addEventListener("click", () => post({ type: "redo" }));
    $("save").addEventListener("click", () => post({ type: "save" }));
    $("connect").addEventListener("click", connect);
    window.addEventListener("pagehide", () => { if (S.link) S.link.close(); });
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape" && S.pick) { cancelPick(); return; }
      if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === "z") {
        if (e.target.matches && e.target.matches("input[type=text], textarea")) return;
        e.preventDefault();
        if (S.started || S.link) post({ type: e.shiftKey ? "redo" : "undo" });
      }
    });
    renderLink();
    document.body.classList.add("ed-ready");
    window.aislingEditor = { S, Q, Qmem: () => Qmem, post, start, TABS, tabOfView, request, connect, disconnect };
  }
  init().catch((e) => {
    $("ed").replaceChildren(el("p", { class: "ed-fail" }, "The editor couldn't load the firmware copy it runs on. Reloading usually fixes this."));
    console.error(e);
  });
})();
