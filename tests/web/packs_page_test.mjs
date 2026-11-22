// SPDX-License-Identifier: GPL-3.0-only
// The Packs page (web/packs), a pack manager, in a browser, against an FM-1 made up here: a WebMIDI port that
// answers AISLING's link as firmware/src/app/link.c does for packs (HELLO with level, bank and pack; STATE's playing
// flag; area 10 written, read and emptied, BUSY while it "plays"; areas 1-8, the eight pieces of the bank that plays;
// BANK; SAVE). The real firmware's side of the same is tests/scenarios/pack.amb.
//   python3 tools/make_pack.py packs/saints-sinners.json && node tests/web/packs_page_test.mjs
import { createRequire } from "node:module";
import http from "node:http";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { execFileSync } from "node:child_process";
const { chromium } = createRequire(import.meta.url)("playwright");   // (wherever it is installed)

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), "../..");
const page0 = execFileSync("python3", ["-c",
  "import sys; sys.path.insert(0,'tools'); from site_brand import stamp; print(stamp(open('web/packs/index.html',encoding='utf8').read(), '../', 'packs'))"],
  { cwd: ROOT, encoding: "utf8" });
const pack = JSON.parse(fs.readFileSync(path.join(ROOT, "build/packs/saints-sinners.json"), "utf8"));
const web = (f) => fs.readFileSync(path.join(ROOT, "web", f));
const FILES = {
  "/packs/": [page0, "text/html"],
  "/packs/index.json": [JSON.stringify([pack]), "application/json"],
  "/packs/packs.js": [web("packs/packs.js"), "text/javascript"],
  "/packs/packfile.js": [web("packs/packfile.js"), "text/javascript"],
  "/packs/worklet.js": [web("packs/worklet.js"), "text/javascript"],
  "/editor/engine.js": [web("editor/engine.js"), "text/javascript"],
  "/editor/link.js": [web("editor/link.js"), "text/javascript"],
  "/site.css": [web("brand/site.css"), "text/css"],
};
const WASM = process.env.AISLING_WASM;               // a built editor/amb.wasm, if there is one: the preview really plays
if (WASM) FILES["/editor/amb.wasm"] = [fs.readFileSync(WASM), "application/wasm"];
const server = http.createServer((req, res) => {
  const f = FILES[req.url.split("?")[0]];
  if (!f) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { "content-type": f[1] });
  res.end(f[0]);
}).listen(0);
const base = `http://127.0.0.1:${server.address().port}`;

// packs to add from files: the site's under another name (a second pack), and two that must be refused
const good = Uint8Array.from(Buffer.from(pack.data, "base64"));
const named = (name) => { const b = good.slice(); b.fill(0, 12, 32); b.set(Buffer.from(name), 12); return b; };
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "packs-"));
const file = (n, bytes) => { const f = path.join(tmp, n); fs.writeFileSync(f, bytes); return f; };
const otherFile = file("other.aispack", named("Other Pack")), cutFile = file("cut.aispack", good.slice(0, 900)), notFile = file("notes.aispack", Buffer.from("hello"));
const third = Array.from(named("Third Pack"));

let bad = 0;
const check = (name, ok, got) => { console.log(`${ok ? "  ok  " : "FAIL  "}${name}${ok ? "" : `: ${got}`}`); bad += !ok; };

function fakeFM1() {
  // the made-up FM-1: link.c's part in packs, no more
  const dev = { level: 2, bank: 0, pack: null, playing: true, buf: null, seq: -1, last: null, area: 0, log: [], saves: 0,
    slots: [0, 1].map((b) => Array.from({ length: 8 }, (_, i) => new Uint8Array(352).fill(16 * b + i + 1))) };
  const out = { name: "FM-1", send(bytes) { setTimeout(() => take(Uint8Array.from(bytes)), 1); } };
  const inp = { name: "FM-1", onmidimessage: null };
  const say = (cmd, seq, args) => { const f = [0xF0, 0x7D, 0x41, 0x4C, cmd, ...(seq == null ? [] : [seq]), ...args, 0xF7];
    setTimeout(() => inp.onmidimessage && inp.onmidimessage({ data: Uint8Array.from(f) }), 1); };
  const put14 = (v) => [v & 0x7F, v >> 7 & 0x7F];
  const text = (s) => [s.length, ...[...s].map((c) => c.charCodeAt(0) & 0x7F)];
  const hello = () => say(65, 0, [1, 3, ...put14(3112), ...put14(352), ...put14(133), 8, ...text("1.5"), dev.level, dev.bank,
    ...put14(dev.pack ? dev.pack[10] | dev.pack[11] << 8 : 0), ...text(dev.pack ? String.fromCharCode(...dev.pack.subarray(12, 32)).replace(/\0+$/, "") : "")]);
  setInterval(() => { if (inp.onmidimessage) { const s = new Array(35).fill(0); s[1] = dev.playing ? 1 : 0; say(67, 0, s); } }, 100);   // STATE
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
    const opened = (b) => { dev.buf = b; const s = AislingLink.sum(b); x = [...put14(b.length), s & 0x7F, s >> 7 & 0x7F, s >> 14 & 0x7F]; };
    if (cmd === 20) {                                // OPEN
      if (a[0] === 10) { if (dev.pack) opened(dev.pack); else st = 5; }
      else if (a[0] >= 1 && a[0] <= 8) opened(dev.slots[dev.bank][a[0] - 1].slice());
      else st = 5;
    } else if (cmd === 22) { dev.buf = new Uint8Array(3840); dev.area = a[0];
    } else if (cmd === 23) { dev.buf.set(AislingLink.unpack(a.subarray(2)), a[0] | a[1] << 7);
    } else if (cmd === 24 && dev.area === 10) {      // COMMIT: the pack
      const len = dev.buf[8] | dev.buf[9] << 8, b = dev.buf.slice(0, len), s = AislingLink.sum(b);
      if (s !== (a[0] | a[1] << 7 | a[2] << 14)) st = 3;
      else if (dev.playing) st = 4;
      else if (len && (b[5] > dev.level || String.fromCharCode(...b.subarray(0, 4)) !== "APK1")) st = 2;
      else {                                         // kept: the eight play, the pack's bank its recipes again
        dev.pack = len ? b : null;
        dev.bank = 0;
        dev.slots[1] = Array.from({ length: 8 }, (_, i) => new Uint8Array(352).fill(len ? (b[12 + 1] + i) & 255 : 0));
        dev.log.push(len ? `on ${String.fromCharCode(...b.subarray(12, 32)).replace(/\0+$/, "")}` : "off");
      }
    } else if (cmd === 24 && dev.area >= 1 && dev.area <= 8) {   // COMMIT: a piece
      const b = dev.buf.slice(0, 352);
      if (AislingLink.sum(b) !== (a[0] | a[1] << 7 | a[2] << 14)) st = 3;
      else { dev.slots[dev.bank][dev.area - 1] = b; dev.log.push(`piece ${dev.bank}.${dev.area}`); }
    } else if (cmd === 24) { st = 1; dev.log.push(`area ${dev.area}`);
    } else if (cmd === 25) {                         // BANK
      if (a[0] && !dev.pack) st = 5; else dev.bank = a[0];
    } else if (cmd === 15) { dev.saves++;            // SAVE
    } else st = 1;
    dev.last = [st, ...x];
    say(66, seq, dev.last);
  }
  globalThis.fakeFM1 = dev;
  navigator.requestMIDIAccess = async () => ({ inputs: new Map([["i", inp]]), outputs: new Map([["o", out]]) });
  globalThis.confirm = () => true;
}

const browser = await chromium.launch({ args: ["--autoplay-policy=no-user-gesture-required"] });
const page = await browser.newPage({ acceptDownloads: true });
const errors = [];
page.on("pageerror", (e) => errors.push(e.message));
await page.addInitScript(fakeFM1);
await page.goto(`${base}/packs/`);
await page.waitForSelector(".pk-pack");
const note = (i) => page.locator(".pk-pack .pk-note").nth(i);
const installBtn = (i) => page.locator(".pk-pack").nth(i).locator("button", { hasText: /Install|Put this/ });
const fm1 = () => page.locator("#fm1-what").innerText();
const dev = (fn, arg) => page.evaluate(fn, arg);
const until = (fn, arg, ms = 20000) => page.waitForFunction(fn, arg, { timeout: ms });

check("one pack listed, by name", (await page.locator(".pk-pack h2").first().innerText()).toLowerCase().startsWith("saints & sinners"));
check("eight pieces, four of each side", await page.locator(".pk-piece").count() === 8 && await page.locator(".pk-piece.saint").count() === 4);
check("its own words for the sides", /^Saint/.test(await page.locator(".pk-piece.saint .pk-meta").first().innerText()));
check("install waits for an FM-1", await installBtn(0).isDisabled());
check("every play button named", await page.locator('.pk-play[aria-label^="Hear "]').count() === 8);
check("the FM-1's panel hidden till there is one", await page.locator("#fm1").isHidden());

// --- connected: what is on it
await page.click("#connect");
await until(() => /FM-1 connected/.test(document.getElementById("dev").textContent));
await until(() => !document.getElementById("fm1").hidden);
check("connected: no pack on it, said in its panel", /No pack is on it/.test(await fm1()), await fm1());
check("nothing to take off or bring back yet", await page.locator("#fm1-off").isHidden() && await page.locator("#fm1-back").isHidden());

// --- installing: it plays, so the page waits and says how; then it is on, and named back from its own bytes
await installBtn(0).click();
await until(() => /waiting for the FM-1/.test(document.querySelector(".pk-pack .pk-note").textContent));
check("busy: it says to stop the FM-1", true);
await dev(() => { globalThis.fakeFM1.playing = false; });
await until(() => /is on the FM-1\./.test(document.querySelector(".pk-pack .pk-note").textContent));
const got = await dev(() => Array.from(globalThis.fakeFM1.pack));
check("the FM-1 has the pack, byte for byte", got.length === good.length && got.every((v, i) => v === good[i]), `${got.length} bytes`);
await until(() => /the same as in the list below/.test(document.getElementById("fm1-what").textContent));
check("read back and recognised", /Saints & Sinners, version 1, is on it/.test(await fm1()), await fm1());
check("its card says so", /On your FM-1/i.test(await page.locator(".pk-pack h2 .pk-tag").first().innerText()));
check("the eight play still (bank put back after the look)", await dev(() => globalThis.fakeFM1.bank === 0));

await page.click("#fm1-go");
await until(() => globalThis.fakeFM1.bank === 1 && /Its pieces are playing now/.test(document.getElementById("fm1-what").textContent));
check("bank: the pack's", true);
await page.click("#fm1-go");
await until(() => globalThis.fakeFM1.bank === 0 && /own eight are playing/.test(document.getElementById("fm1-what").textContent));

// --- a pack from a file, and files that are not packs
await page.setInputFiles("#add-file", [cutFile, notFile]);
await until(() => /cut\.aispack/.test(document.getElementById("add-note").textContent));
const said = await page.locator("#add-note").innerText();
check("a pack cut short, and a file that isn't one: refused, each with why", /cut\.aispack: This isn't an AISLING sound pack/.test(said) &&
  /notes\.aispack: This isn't an AISLING sound pack/.test(said) && await page.locator(".pk-pack").count() === 1, said);
await page.setInputFiles("#add-file", otherFile);
await until(() => document.querySelectorAll(".pk-pack").length === 2);
check("a good file: listed as yours, its pieces named from its bytes", /Other Pack/i.test(await page.locator(".pk-pack h2").nth(1).innerText()) &&
  /Yours/i.test(await page.locator(".pk-pack h2 .pk-tag").nth(1).innerText()) && await page.locator(".pk-pack").nth(1).locator(".pk-piece").count() === 8 &&
  /Rocky Road/i.test(await page.locator(".pk-pack").nth(1).locator(".pk-piece h3").nth(1).innerText()));
check("kept in this browser", await dev(() => JSON.parse(localStorage.getItem("aisling-packs-shelf")).length === 1));

// --- the player changes a piece on the FM-1; another pack goes on: the changes are kept first, not lost
await dev(() => { globalThis.fakeFM1.slots[1][2][10] = 200; });
await installBtn(1).click();
await until(() => /Other Pack is on the FM-1\./.test(document.querySelectorAll(".pk-pack .pk-note")[1].textContent));
check("the swap says the changes were kept", /Your changes to Saints & Sinners are kept in this browser/.test(await note(1).innerText()), await note(1).innerText());
const kept = await dev(() => { const e = JSON.parse(localStorage.getItem("aisling-packs-edits")); const k = Object.keys(e)[0];
  return { keys: Object.keys(e).length, name: e[k][0].name, n: e[k].length, b: atob(e[k][0].slots[2]).charCodeAt(10), len: atob(e[k][0].slots[0]).length }; });
check("the eight pieces kept, the changed one as changed", kept.keys === 1 && kept.name === "Saints & Sinners" && kept.b === 200 && kept.len === 352, JSON.stringify(kept));
check("the FM-1 has the other pack now", await dev(() => globalThis.fakeFM1.log.join() === "on Saints & Sinners,on Other Pack"), await dev(() => globalThis.fakeFM1.log.join()));
await until(() => /one of your own packs/.test(document.getElementById("fm1-what").textContent));

// --- the first pack back on: its pieces offered back, and written back as they were, a piece at a time
await installBtn(0).click();
await until(() => globalThis.fakeFM1.log.length === 3 && !globalThis.AislingPacks.busy);
check("it says they can come back", /Bring back my pieces/.test(await note(0).innerText()), await note(0).innerText());
check("an untouched pack leaving keeps nothing new", await dev(() => Object.keys(JSON.parse(localStorage.getItem("aisling-packs-edits"))).length === 1));
check("fresh from the pack till then", await dev(() => globalThis.fakeFM1.slots[1][2][10] !== 200));
await until(() => !document.getElementById("fm1-back").hidden);
await page.click("#fm1-back");
await until(() => /are back on the FM-1/.test(document.getElementById("fm1-note").textContent));
const back = await dev(() => ({ b: globalThis.fakeFM1.slots[1][2][10], eight: globalThis.fakeFM1.slots[0].every((s, i) => s.every((v) => v === i + 1)),
  log: globalThis.fakeFM1.log.slice(-8).join(), saves: globalThis.fakeFM1.saves, bank: globalThis.fakeFM1.bank }));
check("the changed piece is back", back.b === 200, back.b);
check("written to the pack's bank only, pieces 1 to 8, then saved", back.log === "piece 1.1,piece 1.2,piece 1.3,piece 1.4,piece 1.5,piece 1.6,piece 1.7,piece 1.8" && back.saves === 1, JSON.stringify(back));
check("the FM-1's own eight untouched, and playing again", back.eight && back.bank === 0);
check("nothing but pieces and packs written (no whole project)", await dev(() => !globalThis.fakeFM1.log.some((l) => l.startsWith("area"))));

// --- off: kept first, again; and an unchanged set isn't kept twice
await page.click("#fm1-off");
await until(() => /is off the FM-1/.test(document.getElementById("fm1-note").textContent));
check("taken off", await dev(() => globalThis.fakeFM1.pack === null));
check("the same pieces not kept twice", await dev(() => { const e = JSON.parse(localStorage.getItem("aisling-packs-edits")); return Object.values(e).every((l) => l.length === 1); }));
await until(() => /No pack is on it/.test(document.getElementById("fm1-what").textContent));

// --- a pack on the FM-1 this page has never seen: named, and copied off it
await page.click("#connect");
await dev((b) => { globalThis.fakeFM1.pack = Uint8Array.from(b); }, third);
await page.click("#connect");
await until(() => /a pack this page doesn't have/.test(document.getElementById("fm1-what").textContent));
check("an unknown pack: said so", /Third Pack, version 1, is on it/.test(await fm1()), await fm1());
await page.click("#fm1-copy");
await until(() => document.querySelectorAll(".pk-pack").length === 3);
check("copied to My packs", /Third Pack/i.test(await page.locator(".pk-pack h2").nth(2).innerText()) && await page.locator("#fm1-copy").isHidden());

// --- the pieces to a file, and a pack as a file
const [d1] = await Promise.all([page.waitForEvent("download"), page.click("#fm1-file")]);
const pieces = JSON.parse(fs.readFileSync(await d1.path(), "utf8"));
check("my pieces as a file", d1.suggestedFilename() === "third-pack-my-pieces.json" && pieces.type === "aisling-pack-pieces" && pieces.slots.length === 8, d1.suggestedFilename());
const [d2] = await Promise.all([page.waitForEvent("download"), page.locator(".pk-pack").first().locator("button", { hasText: "Download the pack" }).click()]);
const dl = fs.readFileSync(await d2.path());
check("a pack as a file, byte for byte", d2.suggestedFilename() === "saints-sinners-v1.aispack" && dl.length === good.length && dl.every((v, i) => v === good[i]), d2.suggestedFilename());
await dev(() => { globalThis.fakeFM1.slots[1][5][20] = 7; });
await page.setInputFiles("#fm1-load-file", await d1.path());
await until(() => /are back on the FM-1/.test(document.getElementById("fm1-note").textContent));
check("pieces loaded from the file", await dev(() => globalThis.fakeFM1.slots[1][5][20] !== 7));

// --- an older FM-1: no install, and why
await dev(() => { globalThis.fakeFM1.level = 1; });
await page.click("#connect");
await page.click("#connect");
await until(() => /older than a pack here needs/.test(document.getElementById("dev").textContent));
check("an older FM-1: install held back, with the reason", await installBtn(0).isDisabled());
await dev(() => { globalThis.fakeFM1.level = 2; });

// --- hearing one
await page.locator(".pk-play").first().click();
await until(() => /can't play here|Playing/.test(document.querySelector(".pk-pack .pk-note").textContent));
if (WASM) {
  check("the preview starts", /Playing Glendalough/.test(await note(0).innerText()), await note(0).innerText());
  check("the firmware in the page has the pack", await dev(() => !!globalThis.AislingPacks.loaded && globalThis.AislingPacks.ac.state === "running"));
  const loud = await dev(async () => {                // sound, not only a node: listen to what it puts out, up to a minute
    const S = globalThis.AislingPacks, an = S.ac.createAnalyser(), buf = new Float32Array(2048);
    S.node.connect(an);
    let peak = 0;
    for (let n = 0; n < 240 && peak < 0.01; n++) {
      await new Promise((r) => setTimeout(r, 250));
      an.getFloatTimeDomainData(buf);
      for (const v of buf) peak = Math.max(peak, Math.abs(v));
    }
    return peak;
  });
  check("and it sounds", loud >= 0.01, `peak ${loud}`);
  await page.locator(".pk-pack").nth(1).locator(".pk-play").nth(1).click();   // another pack: the firmware started afresh with it
  await until(() => /Playing Rocky Road/.test(document.querySelectorAll(".pk-pack .pk-note")[1].textContent));
  check("another pack's piece plays too, the first one's note cleared", (await note(0).innerText()) === "");
  await page.locator(".pk-pack").nth(1).locator(".pk-play").nth(1).click();
  await until(() => !globalThis.AislingPacks.playing);
} else {
  check("no engine to hand: said, not thrown", /can't play here/.test(await note(0).innerText()), await note(0).innerText());
}
check("no page errors", errors.length === 0, errors.join(" | "));

await page.click("#connect");
await page.click("#connect");
await until(() => !document.getElementById("fm1").hidden && /Third Pack/.test(document.getElementById("fm1-what").textContent));
fs.mkdirSync(path.join(ROOT, "build/packs"), { recursive: true });
await page.setViewportSize({ width: 1200, height: 1500 });
await page.screenshot({ path: path.join(ROOT, "build/packs/page.png"), fullPage: true });
await page.setViewportSize({ width: 390, height: 900 });
check("no sideways scroll on a phone", await dev(() => document.documentElement.scrollWidth <= innerWidth + 1));
await page.screenshot({ path: path.join(ROOT, "build/packs/page-phone.png"), fullPage: false });

// --- a browser that keeps nothing (a private window, its storage refused): the page still works
const quiet = await browser.newPage();
const errors2 = [];
quiet.on("pageerror", (e) => errors2.push(e.message));
await quiet.addInitScript(() => { Object.defineProperty(globalThis, "localStorage", { get() { throw new Error("no storage here"); } }); });
await quiet.addInitScript(fakeFM1);
await quiet.goto(`${base}/packs/`);
await quiet.waitForSelector(".pk-pack");
await quiet.setInputFiles("#add-file", otherFile);
await quiet.waitForFunction(() => document.querySelectorAll(".pk-pack").length === 2);
check("no storage: packs listed, a file added, and it says it won't last", /until this page is closed/.test(await quiet.locator("#add-note").innerText()) && errors2.length === 0,
  `${await quiet.locator("#add-note").innerText()} ${errors2.join(" | ")}`);

// --- a pack by a link: fetched from an address, or whole in the link itself; refused ones say why; the address tidied
FILES["/packs/shared/linked.aispack"] = [Buffer.from(named("Linked Pack")), "application/octet-stream"];
const ctx = await browser.newContext();
await ctx.grantPermissions(["clipboard-read", "clipboard-write"]);
const linked = await ctx.newPage();
const errors3 = [];
linked.on("pageerror", (e) => errors3.push(e.message));
await linked.goto(`${base}/packs/?add=shared/linked.aispack&add=http://example.invalid/x.aispack&add=shared/none.aispack`);
await linked.waitForFunction(() => document.querySelectorAll(".pk-pack").length === 2);
const lsaid = await linked.locator("#add-note").innerText();
check("a pack fetched by a link is in My packs", /Linked Pack, version 1, came by a link/.test(lsaid) && /Linked Pack/i.test(await linked.locator(".pk-pack h2").nth(1).innerText()), lsaid);
check("an address that isn't https, and one that isn't there: each refused with why", /example\.invalid: It isn't an address/.test(lsaid) && /none\.aispack: It couldn't be fetched/.test(lsaid), lsaid);
check("the address tidied, so a reload fetches nothing", new URL(linked.url()).search === "", linked.url());
await linked.locator(".pk-pack").nth(1).locator("button", { hasText: "Copy a link" }).click();
const url = await linked.evaluate(() => navigator.clipboard.readText());
check("its link carries the whole pack", /\/packs\/#pack=[A-Za-z0-9_-]+$/.test(url) && url.length < 4000, `${url.length} characters`);
const fresh = await (await browser.newContext()).newPage();
await fresh.goto(url);
await fresh.waitForFunction(() => document.querySelectorAll(".pk-pack").length === 2);
const viaLink = await fresh.evaluate(() => Array.from(globalThis.AislingPacks.packs[1].bytes));
const want = Array.from(named("Linked Pack"));
check("opened elsewhere: the same pack, byte for byte, and the link tidied", viaLink.length === want.length && viaLink.every((v, i) => v === want[i]) && new URL(fresh.url()).hash === "", fresh.url().slice(0, 60));
await fresh.goto(`${base}/packs/#pack=AAAA`);
await fresh.reload();
await fresh.waitForFunction(() => /The pack in the link/.test(document.getElementById("add-note").textContent));
check("a damaged link: said, nothing added", /This isn't an AISLING sound pack/.test(await fresh.locator("#add-note").innerText()) && await fresh.locator(".pk-pack").count() === 2,
  await fresh.locator("#add-note").innerText());   // (the two it had: the site's, and the one the link before brought)
await linked.locator(".pk-pack").first().locator("button", { hasText: "Copy a link" }).click();
check("the site's own pack: a plain link to it on the page", /\/packs\/#pk-saints-sinners$/.test(await linked.evaluate(() => navigator.clipboard.readText())));
check("no page errors by links", errors3.length === 0, errors3.join(" | "));

await browser.close();
server.close();
console.log(bad ? `packs page: ${bad} FAILED` : "packs page: passed");
process.exit(bad ? 1 : 0);
