// SPDX-License-Identifier: GPL-3.0-only
/* The page's end of AISLING's USB link: firmware/src/app/link.c is the other. No page in here, so a test can run it alone.
 *
 * Frames are F0 7D 41 4C cmd seq args F7. One request at a time: an ACK or a CHUNK with its seq ends it, and no answer in
 * 200 ms sends it again with the same seq, which the FM-1 answers without doing it twice. A SysEx that arrives before the
 * FM-1 has handled the last is dropped, so this is what makes the link reliable.
 *
 *   const ports = AislingLink.findPorts(midiAccess);
 *   const link = new AislingLink(ports.input, ports.output, { onState, onVals, onGone, onReply });
 *   const hello = await link.open();     // null: it never answered
 *   link.post({ type: "set", k, v });    // the messages the page's worklet takes
 *   await link.read(area);  await link.write(area, bytes);  link.close();
 */
(function () {
  "use strict";
  const H0 = 0x7D, H1 = 0x41, H2 = 0x4C;
  const LK = { HELLO: 1, BYE: 2, BEAT: 3, SET: 4, VIEW: 5, PIECE: 6, PLAY: 7, HOLD: 8, PERF: 9, FORGET: 10, KEYS: 11, BUTTONS: 12,
    UNDO: 13, REDO: 14, SAVE: 15, VOICE: 16, COPY: 17, SWAP: 18, RESET: 19, OPEN: 20, READ: 21, BEGIN: 22, WRITE: 23, COMMIT: 24 };
  const R_HELLO = 65, R_ACK = 66, R_STATE = 67, R_VALS = 68, R_CHUNK = 69;
  const OK = 0, GAVE_UP = -1;
  const VER = 1, CHUNK = 128;
  const RESEND_MS = 200, TRIES = 25, BEAT_MS = 1000, HELLO_MS = 500, NO_STATE_MS = 1500, SILENT_MS = 8000;

  const put14 = (v) => [v & 0x7F, v >> 7 & 0x7F];
  const get14u = (a, i) => a[i] | a[i + 1] << 7;
  const get14s = (a, i) => { const v = get14u(a, i); return v >= 8192 ? v - 16384 : v; };

  function sum(p) {                                  // FNV-1a, in 21 bits: three pieces
    let h = 2166136261;
    for (let i = 0; i < p.length; i++) h = Math.imul(h ^ p[i], 16777619);
    h >>>= 0;
    return (h ^ h >>> 21) & 0x1FFFFF;
  }
  function pack(s) {                                 // each seven: a byte of their top bits, then them
    const o = [];
    for (let i = 0; i < s.length; i += 7) {
      const m = Math.min(s.length - i, 7), at = o.length;
      let hi = 0;
      o.push(0);
      for (let j = 0; j < m; j++) { hi |= (s[i + j] >> 7) << j; o.push(s[i + j] & 0x7F); }
      o[at] = hi;
    }
    return o;
  }
  function unpack(s) {
    const o = [];
    let i = 0;
    while (i < s.length) {
      const hi = s[i++];
      for (let j = 0; j < 7 && i < s.length; j++, i++) o.push(s[i] | (hi >> j & 1) << 7);
    }
    return Uint8Array.from(o);
  }

  function parseHello(a) {
    const n = a[9];
    return { ver: a[0], format: a[1], proj: get14u(a, 2), slot: get14u(a, 4), npars: get14u(a, 6), npieces: a[8],
      version: String.fromCharCode(...a.subarray(10, 10 + n)) };
  }
  function parseState(a) {
    const f = a[1];
    return { view: a[0], playing: !!(f & 1), frozen: !!(f & 2), dirty: !!(f & 4), perf: !!(f & 8), saving: !!(f & 16), piece: a[2],
      moves: get14u(a, 3), act: [a[5], a[6], a[7], a[8], a[9]], muted: a[10], solo: a[11], sounds: a[12], un: a[13], rn: a[14],
      cpu: a[15], peak: a[16], xruns: get14u(a, 17), master: get14u(a, 19), raw: get14u(a, 21), lit: get14u(a, 23),
      keys: (get14u(a, 25) | get14u(a, 27) << 14) >>> 0, gen: a[29],
      fx: a.length >= 34 ? (get14u(a, 30) | get14u(a, 32) << 14) >>> 0 : 0 };   // (1.3: the effects sounding; 1.2 sends none)
  }
  function parseVals(a) {                            // [k, v, text, k, v, text..]: the page's worklet sends the same
    const out = [];
    let at = 0;
    while (at + 5 <= a.length) {
      const k = get14u(a, at), v = get14s(a, at + 2), n = a[at + 4];
      at += 5;
      out.push(k, v, String.fromCharCode(...a.subarray(at, at + n)));
      at += n;
    }
    return out;
  }

  class AislingLink {
    static findPorts(access) {                       // by name: nothing goes to any other device
      const fm1 = (p) => /FM-1|DUB-1/i.test(p.name || "");
      const input = [...access.inputs.values()].find(fm1), output = [...access.outputs.values()].find(fm1);
      return input && output ? { input, output } : null;
    }

    constructor(input, output, h) {
      this.input = input;
      this.output = output;
      this.h = h || {};
      this.seq = 0;
      this.q = [];                                   // the requests waiting; cur: the one the FM-1 has
      this.cur = null;
      this.tx = Promise.resolve();                   // transfers, one after another: they share the FM-1's one buffer
      this.hello = null;
      this.closed = false;
      this.helloWait = null;
      this.tBeat = this.tHello = this.tState = this.tTick = 0;
    }

    /* ---------- the wire ---------- */
    send(bytes) {
      try { this.output.send(bytes); return true; } catch (e) { this.gone("sent"); return false; }
    }
    frame(cmd, seq, args) {
      return seq == null ? [0xF0, H0, H1, H2, cmd, ...(args || []), 0xF7] : [0xF0, H0, H1, H2, cmd, seq, ...(args || []), 0xF7];
    }
    take(d) {
      const n = d.length;
      if (this.closed || n < 7 || d[0] !== 0xF0 || d[n - 1] !== 0xF7 || d[1] !== H0 || d[2] !== H1 || d[3] !== H2) return;
      const cmd = d[4], seq = d[5], a = d.subarray ? d.subarray(6, n - 1) : Uint8Array.from(d).subarray(6, n - 1);
      if (cmd === R_HELLO) {
        this.hello = parseHello(a);
        this.tState = performance.now();
        if (this.helloWait) { const w = this.helloWait; this.helloWait = null; w(this.hello); }
      } else if (cmd === R_STATE) {
        const t = this.tState = performance.now();
        if (this.h.onState && !this.helloWait) this.h.onState(parseState(a));
        if (!this.closed && t - this.tTick >= 200) this.tick();    // a hidden tab's timers slow down; its MIDI doesn't: keep beating
      } else if (cmd === R_VALS) {
        if (this.h.onVals && !this.helloWait) this.h.onVals(parseVals(a));
      } else if (this.cur && seq === this.cur.seq) {
        if (cmd === R_ACK) this.finish({ status: a[0], x: a.slice(1) });
        else if (cmd === R_CHUNK && this.cur.cmd === LK.READ && get14u(a, 0) === get14u(this.cur.args, 0))
          this.finish({ status: OK, data: unpack(a.subarray(2)) });
      }
    }

    /* ---------- opening and closing ---------- */
    open(ms = 3000) {
      return new Promise((res) => {
        this.helloWait = res;
        this.input.onmidimessage = (e) => this.take(e.data);
        const t = performance.now();
        this.tTick = this.tBeat = t;
        this.tHello = t;
        this.tOpenBy = t + ms;
        this.send(this.frame(LK.HELLO, null, [VER]));
        this.timer = setInterval(() => this.tick(), 50);
      });
    }
    close() {                                        // says goodbye: the FM-1 lets go of anything still held
      if (this.closed) return;
      if (this.hello) {                              // (twice: one right behind another frame can be dropped)
        const bye = this.frame(LK.BYE);
        try { this.output.send(bye); this.output.send(bye, performance.now() + 60); } catch (e) { /* the cable's out: it times out by itself */ }
      }
      this.stop();
    }
    stop() {
      this.closed = true;
      clearInterval(this.timer);
      this.input.onmidimessage = null;
      if (this.helloWait) { const w = this.helloWait; this.helloWait = null; w(null); }
      const all = this.cur ? [this.cur, ...this.q] : this.q;
      this.cur = null;
      this.q = [];
      for (const r of all) r.done({ status: GAVE_UP });
    }
    gone(why) {                                      // the cable's out, or it stopped answering
      if (this.closed) return;
      this.stop();
      if (this.h.onGone) this.h.onGone(why);
    }
    tick() {
      if (this.closed) return;
      const t = performance.now(), ran = t - this.tTick < 2000;        // (a hidden tab's timers slow down: that isn't silence)
      this.tTick = t;
      if (this.helloWait) {
        if (t > this.tOpenBy) { this.stop(); return; }
        if (t - this.tHello >= HELLO_MS) { this.tHello = t; this.send(this.frame(LK.HELLO, null, [VER])); }
        return;
      }
      if (t - this.tState >= SILENT_MS && ran) { this.gone("silent"); return; }
      if (t - this.tState >= NO_STATE_MS && t - this.tHello >= NO_STATE_MS) {
        this.tHello = t;
        if (!this.send(this.frame(LK.HELLO, null, [VER]))) return;
      }
      if (t - this.tBeat >= BEAT_MS) {
        this.tBeat = t;
        if (!this.send(this.frame(LK.BEAT))) return;
      }
      if (this.cur && t - this.cur.t >= RESEND_MS) {
        if (++this.cur.tries >= TRIES) { this.finish({ status: GAVE_UP }); return; }
        this.cur.t = t;
        this.send(this.frame(this.cur.cmd, this.cur.seq, this.cur.args));
      }
    }

    /* ---------- requests ---------- */
    pump() {
      if (this.cur || this.closed || !this.q.length) return;
      const r = this.q.shift();
      this.seq = this.seq % 127 + 1;
      r.seq = this.seq;
      r.t = performance.now();
      r.tries = 0;
      this.cur = r;
      this.send(this.frame(r.cmd, r.seq, r.args));
    }
    finish(result) {
      const r = this.cur;
      this.cur = null;
      r.done(result);
      this.pump();
    }
    req(cmd, args, key) {                            // the same key still waiting: only the newest is sent
      if (this.closed) return Promise.resolve({ status: GAVE_UP });
      return new Promise((res) => {
        const was = key ? this.q.find((r) => r.key === key) : null;
        if (was) {
          const d = was.done;
          was.args = args;
          was.done = (x) => { d(x); res(x); };
          return;
        }
        this.q.push({ cmd, args: args || [], key, done: res });
        this.pump();
      });
    }

    /* ---------- whole transfers ---------- */
    exclusive(fn) {
      const p = this.tx.then(fn, fn);
      this.tx = p.catch(() => {});
      return p;
    }
    read(area) {                                     // the bytes, their sum checked; null: not read
      return this.exclusive(async () => {
        const o = await this.req(LK.OPEN, [area]);
        if (o.status !== OK || o.x.length !== 5) return null;
        const len = get14u(o.x, 0), want = o.x[2] | o.x[3] << 7 | o.x[4] << 14, b = new Uint8Array(len);
        for (let off = 0; off < len; off += CHUNK) {
          const c = await this.req(LK.READ, put14(off));
          if (c.status !== OK) return null;
          b.set(c.data, off);
        }
        return sum(b) === want ? b : null;
      });
    }
    write(area, bytes) {                             // 0: taken; else why not (link.c's LK_*, or -1: gave up)
      return this.exclusive(async () => {
        const b = await this.req(LK.BEGIN, [area]);
        if (b.status !== OK) return b.status;
        for (let off = 0; off < bytes.length; off += CHUNK) {
          const w = await this.req(LK.WRITE, [...put14(off), ...pack(bytes.subarray(off, off + CHUNK))]);
          if (w.status !== OK) return w.status;
        }
        const s = sum(bytes), c = await this.req(LK.COMMIT, [s & 0x7F, s >> 7 & 0x7F, s >> 14 & 0x7F]);
        return c.status;
      });
    }

    /* ---------- the messages the page's worklet takes ---------- */
    post(m) {
      const reply = (x) => { if (this.h.onReply) this.h.onReply({ type: "reply", id: m.id, ...x }); };
      switch (m.type) {
        case "set": this.req(LK.SET, [...put14(m.k), ...put14(m.v & 0x3FFF)], `set:${m.k}`); break;
        case "voice": this.req(LK.VOICE, [...put14(m.p), ...put14(m.v & 0x3FFF)], `voice:${m.p}`); break;
        case "view": this.req(LK.VIEW, [m.v, m.knob == null || m.knob < 0 ? 127 : m.knob], "view"); break;
        case "piece": this.req(LK.PIECE, [m.i]); break;
        case "play": this.req(LK.PLAY); break;
        case "hold": this.req(LK.HOLD); break;
        case "perf": this.req(LK.PERF, [m.i, m.down ? 1 : 0]); break;
        case "forget": this.req(LK.FORGET); break;
        case "keys": this.req(LK.KEYS, [m.mask & 0x7F, m.mask >>> 7 & 0x7F, m.mask >>> 14 & 0x7F, m.mask >>> 21 & 0x7F]); break;
        case "buttons": this.req(LK.BUTTONS, put14(m.mask)); break;
        case "undo": this.req(LK.UNDO); break;
        case "redo": this.req(LK.REDO); break;
        case "save": this.req(LK.SAVE); break;
        case "copy": this.req(LK.COPY, [m.from, m.to]); break;
        case "swap": this.req(LK.SWAP, [m.a, m.b]); break;
        case "reset": this.req(LK.RESET, [m.i]); break;
        case "read-slot": this.read(1 + m.i).then((bytes) => reply({ bytes })); break;
        case "read-proj": this.read(0).then((bytes) => reply({ bytes })); break;
        case "install-slot": this.write(1 + m.i, m.bytes).then((s) => reply({ ok: s === OK ? 0 : -1 })); break;
        case "install-proj": this.write(0, m.bytes).then((s) => reply({ ok: s === OK ? 0 : -1 })); break;
        default: break;                              // begin, end: the FM-1 keeps its own undo. master: its own knob
      }
    }
  }
  AislingLink.parseHello = parseHello;
  AislingLink.parseState = parseState;
  AislingLink.sum = sum;
  AislingLink.pack = pack;
  AislingLink.unpack = unpack;
  globalThis.AislingLink = AislingLink;
})();
