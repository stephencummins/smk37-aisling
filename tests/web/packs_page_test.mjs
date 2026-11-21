// SPDX-License-Identifier: GPL-3.0-only
// The Packs page (web/packs) in a browser, against an FM-1 made up here: a WebMIDI port that answers AISLING's link
// as firmware/src/app/link.c does for packs (HELLO with level, bank and pack; area 10 written, read and emptied;
// BUSY while it "plays"; BANK). The real firmware's side of the same is tests/scenarios/pack.amb.
//   python3 tools/make_pack.py packs/saints-sinners.json && node tests/web/packs_page_test.mjs
import { createRequire } from "node:module";
import http from "node:http";
import fs from "node:fs";
import path from "node:path";
import { execFileSync } from "node:child_process";
const { chromium } = createRequire(import.meta.url)("playwright");   // (wherever it is installed)

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), "../..");
const page0 = execFileSync("python3", ["-c",
  "import sys; sys.path.insert(0,'tools'); from site_brand import stamp; print(stamp(open('web/packs/index.html',encoding='utf8').read(), '../', 'packs'))"],
  { cwd: ROOT, encoding: "utf8" });
const pack = JSON.parse(fs.readFileSync(path.join(ROOT, "build/packs/saints-sinners.json"), "utf8"));
const FILES = {
  "/packs/": [page0, "text/html"],
  "/packs/index.json": [JSON.stringify([pack]), "application/json"],
  "/packs/packs.js": [fs.readFileSync(path.join(ROOT, "web/packs/packs.js")), "text/javascript"],
  "/packs/worklet.js": [fs.readFileSync(path.join(ROOT, "web/packs/worklet.js")), "text/javascript"],
  "/editor/engine.js": [fs.readFileSync(path.join(ROOT, "web/editor/engine.js")), "text/javascript"],
  "/editor/link.js": [fs.readFileSync(path.join(ROOT, "web/editor/link.js")), "text/javascript"],
  "/site.css": [fs.readFileSync(path.join(ROOT, "web/brand/site.css")), "text/css"],
};
const server = http.createServer((req, res) => {
  const f = FILES[req.url.split("?")[0]];
  if (!f) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { "content-type": f[1] });
  res.end(f[0]);
}).listen(0);
const base = `http://127.0.0.1:${server.address().port}`;

let bad = 0;
const check = (name, ok, got) => { console.log(`${ok ? "  ok  " : "FAIL  "}${name}${ok ? "" : `: ${got}`}`); bad += !ok; };

const browser = await chromium.launch();
const page = await browser.newPage();
const errors = [];
page.on("pageerror", (e) => errors.push(e.message));
await page.addInitScript(() => {
  // the made-up FM-1: link.c's part in packs, no more
  const dev = { level: 2, bank: 0, pack: null, playing: true, buf: null, seq: -1, last: null, mode: 0, log: [] };
  const out = { name: "FM-1", send(bytes) { setTimeout(() => take(Uint8Array.from(bytes)), 1); } };
  const inp = { name: "FM-1", onmidimessage: null };
  const say = (cmd, seq, args) => { const f = [0xF0, 0x7D, 0x41, 0x4C, cmd, ...(seq == null ? [] : [seq]), ...args, 0xF7];
    setTimeout(() => inp.onmidimessage && inp.onmidimessage({ data: Uint8Array.from(f) }), 1); };
  const put14 = (v) => [v & 0x7F, v >> 7 & 0x7F];
  const text = (s) => [s.length, ...[...s].map((c) => c.charCodeAt(0) & 0x7F)];
  const hello = () => say(65, 0, [1, 3, ...put14(3112), ...put14(352), ...put14(133), 8, ...text("1.5"), dev.level, dev.bank,
    ...put14(dev.pack ? dev.pack[10] | dev.pack[11] << 8 : 0), ...text(dev.pack ? String.fromCharCode(...dev.pack.subarray(12, 32)).replace(/\0+$/, "") : "")]);
  setInterval(() => inp.onmidimessage && say(67, 0, new Array(35).fill(0)), 100);     // STATE: it is alive
  function take(d) {
    if (d[1] !== 0x7D || d[2] !== 0x41 || d[3] !== 0x4C) return;
    const cmd = d[4];
    if (cmd === 1) { dev.seq = -1; hello(); return; }
    if (cmd === 2 || cmd === 3) return;
    const seq = d[5], a = d.subarray(6, d.length - 1);
    if (cmd === 21) {                                // READ
      const off = a[0] | a[1] << 7;
      say(69, seq, [...put14(off), ...AislingLink.pack(dev.buf.subarray(off, off + 128))]);
      return;
    }
    if (seq === dev.seq) { say(66, seq, dev.last); return; }
    dev.seq = seq;
    let st = 0, x = [];
    if (cmd === 20) {                                // OPEN
      if (a[0] !== 10 || !dev.pack) st = 5;
      else { dev.buf = dev.pack; const s = AislingLink.sum(dev.buf); x = [...put14(dev.buf.length), s & 0x7F, s >> 7 & 0x7F, s >> 14 & 0x7F]; }
    } else if (cmd === 22) { dev.buf = new Uint8Array(3840); dev.mode = a[0];
    } else if (cmd === 23) { dev.buf.set(AislingLink.unpack(a.subarray(2)), a[0] | a[1] << 7);
    } else if (cmd === 24) {                         // COMMIT
      const len = dev.buf[8] | dev.buf[9] << 8, b = dev.buf.slice(0, len), s = AislingLink.sum(b);
      if (s !== (a[0] | a[1] << 7 | a[2] << 14)) st = 3;
      else if (dev.playing) st = 4;
      else if (len && (b[5] > dev.level || String.fromCharCode(...b.subarray(0, 4)) !== "APK1")) st = 2;
      else { dev.pack = len ? b : null; dev.bank = 0; dev.log.push(len ? "installed" : "removed"); }
    } else if (cmd === 25) {                         // BANK
      if (a[0] && !dev.pack) st = 5; else dev.bank = a[0];
    } else st = 1;
    dev.last = [st, ...x];
    say(66, seq, dev.last);
  }
  globalThis.fakeFM1 = dev;
  navigator.requestMIDIAccess = async () => ({ inputs: new Map([["i", inp]]), outputs: new Map([["o", out]]) });
  globalThis.confirm = () => true;
});
await page.goto(`${base}/packs/`);
await page.waitForSelector(".pk-pack");
check("one pack listed, by name", await page.locator(".pk-pack h2").innerText() === "SAINTS & SINNERS" ||
  (await page.locator(".pk-pack h2").innerText()).toLowerCase() === "saints & sinners", await page.locator(".pk-pack h2").innerText());
check("eight pieces", await page.locator(".pk-piece").count() === 8, await page.locator(".pk-piece").count());
check("four Saints, four Sinners", await page.locator(".pk-piece.saint").count() === 4 && await page.locator(".pk-piece.sinner").count() === 4);
check("install waits for an FM-1", await page.locator(".pk-acts button").first().isDisabled());
check("every play button named", await page.locator('.pk-play[aria-label^="Hear "]').count() === 8);

await page.click("#connect");
await page.waitForFunction(() => /FM-1 connected/.test(document.getElementById("dev").textContent));
check("connected: no pack on it", /no pack is on it/.test(await page.locator("#dev").innerText()), await page.locator("#dev").innerText());
const install = page.locator(".pk-acts button").first();
check("install offered", !(await install.isDisabled()));

await install.click();                               // it plays: the page waits, says so, and gets there once it stops
await page.waitForFunction(() => /waiting for the FM-1 to be still/.test(document.querySelector(".pk-note").textContent));
check("busy: it says to stop the FM-1", true);
await page.evaluate(() => { globalThis.fakeFM1.playing = false; });
await page.waitForFunction(() => /is on the FM-1\./.test(document.querySelector(".pk-note").textContent), null, { timeout: 15000 });
const got = await page.evaluate(() => Array.from(globalThis.fakeFM1.pack));
const want = Array.from(Buffer.from(pack.data, "base64"));
check("the FM-1 has the pack, byte for byte", got.length === want.length && got.every((v, i) => v === want[i]), `${got.length} bytes`);
await page.waitForFunction(() => /Saints & Sinners \(version 1\) is on it/.test(document.getElementById("dev").textContent));
check("the page reads it back from HELLO", true);

const go = page.locator(".pk-acts button", { hasText: "Play it on the FM-1" });
await go.click();
await page.waitForFunction(() => globalThis.fakeFM1.bank === 1);
await page.waitForFunction(() => /and playing/.test(document.getElementById("dev").textContent));
check("bank: the pack's", true);

await page.locator(".pk-acts button", { hasText: "Take it off" }).click();
await page.waitForFunction(() => /is off the FM-1/.test(document.querySelector(".pk-note").textContent), null, { timeout: 15000 });
check("removed", await page.evaluate(() => globalThis.fakeFM1.pack === null && globalThis.fakeFM1.log.join() === "installed,removed"));

await page.evaluate(() => { globalThis.fakeFM1.level = 1; });   // an older AISLING: no install, and why
await page.click("#connect");
await page.click("#connect");
await page.waitForFunction(() => /FM-1 connected/.test(document.getElementById("dev").textContent));
check("an older FM-1: install held back, with the reason", await install.isDisabled() &&
  /older than a pack here needs/.test(await page.locator("#dev").innerText()), await page.locator("#dev").innerText());

await page.locator(".pk-play").first().click();       // no amb.wasm here: it must say so, not break
await page.waitForFunction(() => /can't play here|Playing/.test(document.querySelector(".pk-note").textContent), null, { timeout: 15000 });
check("no engine to hand: said, not thrown", /can't play here/.test(await page.locator(".pk-note").innerText()), await page.locator(".pk-note").innerText());
check("no page errors", errors.length === 0, errors.join(" | "));

await page.setViewportSize({ width: 1200, height: 1500 });
await page.screenshot({ path: path.join(ROOT, "build/packs/page.png"), fullPage: true });
await page.setViewportSize({ width: 390, height: 900 });
check("no sideways scroll on a phone", await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth + 1));
await page.screenshot({ path: path.join(ROOT, "build/packs/page-phone.png"), fullPage: false });
await browser.close();
server.close();
console.log(bad ? `packs page: ${bad} FAILED` : "packs page: passed");
process.exit(bad ? 1 : 0);
