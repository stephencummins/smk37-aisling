// SPDX-License-Identifier: GPL-3.0-only
// web/packs/packfile.js against the firmware's checker (firmware/src/app/pack.h, pk_check): the same packs, good and
// broken every way the checker looks for, must get the same answer from both.
//   python3 tools/make_pack.py packs/saints-sinners.json && node tests/web/packfile_test.mjs
import { createRequire } from "node:module";
import { execFileSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), "../..");
createRequire(import.meta.url)(path.join(ROOT, "web/packs/packfile.js"));
const out = path.join(ROOT, "build/host");
fs.mkdirSync(out, { recursive: true });
execFileSync("cc", ["-O1", "-I" + path.join(ROOT, "firmware/src/app"), "-o", path.join(out, "pk_check_main"), path.join(ROOT, "tests/web/pk_check_main.c")]);
const good = new Uint8Array(fs.readFileSync(path.join(ROOT, "build/packs/saints-sinners.aispack")));
const vary = (fn) => { const b = good.slice(); return fn(b) || b; };
const first = 32, air = first + 20, airN = good[first + 17], pairs = air + 2 * airN;
const CASES = {
  "as made": good,
  "empty": new Uint8Array(0),
  "cut short": good.slice(0, good.length - 7),
  "a byte too many": (() => { const b = new Uint8Array(good.length + 1); b.set(good); return b; })(),
  "cut short, its length put right": vary((b) => { const c = b.slice(0, b.length - 7); c[8] = c.length & 255; c[9] = c.length >> 8; return c; }),
  "not APK1": vary((b) => { b[0] = 0x58; }),
  "another format": vary((b) => { b[4] = 2; }),
  "a newer engine's": vary((b) => { b[5] = 4; }),
  "patches said, level 2": vary((b) => { b[first + 19] = 4; }),
  "patches said, none there": vary((b) => { b[5] = 3; b[first + 19] = 4; }),
  "seven pieces": vary((b) => { b[6] = 7; }),
  "no name": vary((b) => { b[12] = 0; }),
  "a name with no end": vary((b) => { b.fill(65, 12, 32); }),
  "a piece with no name": vary((b) => { b[first] = 0; }),
  "a piece's name with no end": vary((b) => { b.fill(66, first, first + 16); }),
  "a piece's name not plain": vary((b) => { b[first + 1] = 200; }),
  "seventeen notes in an air": vary((b) => { b[first + 17] = 17; }),
  "an air's note of no length": vary((b) => { b[air + airN] = 0; }),
  "an air's note too high": vary((b) => { b[air] = 60; }),
  "an air's note too low": vary((b) => { b[air] = 256 - 30; }),
  "a parameter the engine has not": vary((b) => { b[pairs] = 133; }),
  "the air as a parameter": vary((b) => { b[pairs] = 132; }),
  "more pairs than there are": vary((b) => { b[first + 18] = 250; }),
  "too big": (() => { const b = new Uint8Array(4000); b.set(good); b[8] = 4000 & 255; b[9] = 4000 >> 8; return b; })(),
};
let bad = 0;
for (const [name, bytes] of Object.entries(CASES)) {
  const f = path.join(out, "pk_case.aispack");
  fs.writeFileSync(f, bytes);
  const c = +execFileSync(path.join(out, "pk_check_main"), [f], { encoding: "utf8" }), js = AislingPackFile.read(bytes).why;
  const ok = c === js;
  console.log(`${ok ? "  ok  " : "FAIL  "}${name}: firmware ${c}, page ${js}`);
  bad += !ok;
}
const r = AislingPackFile.read(good);
const named = r.name === "Saints & Sinners" && r.pieces.length === 8 && r.pieces[1].name === "Rocky Road" && r.pieces[1].side === "sinner" &&
  r.pieces[1].tempo === 160 && r.pieces[1].key === "A" && r.pieces[1].mode === "Dorian" && r.pieces[0].side === "saint";
console.log(`${named ? "  ok  " : "FAIL  "}the pack's words read from its bytes`);
bad += !named;
// a pack put together in the page (write): the firmware must take it, and the pieces must come back as they went in
const PF = AislingPackFile, raws = r.pieces.map((p) => p.raw);
const fw = (bytes) => { const f = path.join(out, "pk_case.aispack"); fs.writeFileSync(f, bytes); return +execFileSync(path.join(out, "pk_check_main"), [f], { encoding: "utf8" }); };
const eq = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const say = (name, ok) => { console.log(`${ok ? "  ok  " : "FAIL  "}${name}`); bad += !ok; };
const again = PF.write({ name: "Saints & Sinners", version: r.version, level: r.level, pieces: raws });
say("the pack's own pieces written again are the pack, byte for byte", typeof again !== "string" && eq(again, good));
const mixed = PF.write({ name: "My mix", level: 2, pieces: [raws[3], PF.empty(), raws[0], PF.empty(), PF.empty(), raws[7], PF.empty(), PF.empty()] });
const back = typeof mixed === "string" ? null : PF.read(mixed);
say("a mix with empty places: the firmware takes it", !!back && fw(mixed) === 0 && back.why === 0);
say("its pieces are where they were put, the empty ones known", !!back && back.name === "My mix" && back.pieces[0].name === r.pieces[3].name && eq(back.pieces[5].raw, raws[7]) &&
  PF.isEmpty(back.pieces[1].raw) && !PF.isEmpty(back.pieces[0].raw));
say("the firmware knows the empty places for what they are, and no other", (() => { const f = path.join(out, "pk_mix.aispack"); fs.writeFileSync(f, mixed);
  return +execFileSync(path.join(out, "pk_check_main"), [f, "empty"], { encoding: "utf8" }) === 0b11011010; })());
// (1.6) patches: sixteen bytes a layer after a piece's pairs, its fourth byte saying whose
const withPatch = (raw, mask) => { const n = [1, 2, 4, 8].filter((m) => mask & m).length, o = new Uint8Array(raw.length + 16 * n);
  o.set(raw); o[19] = mask; for (let i = 0; i < 16 * n; i++) o[raw.length + i] = i % 16 === 3 ? 256 - 12 : i % 50; return o; };
const pRaws = raws.slice(); pRaws[0] = withPatch(raws[0], 4); pRaws[5] = withPatch(raws[5], 11);
const patched = PF.write({ name: "Patched", level: 3, pieces: pRaws });
const pBack = typeof patched === "string" ? null : PF.read(patched);
say("a pack with patches: the firmware takes it, the page reads whose they are", !!pBack && fw(patched) === 0 && pBack.why === 0 &&
  pBack.pieces[0].patches === 4 && pBack.pieces[5].patches === 11 && pBack.pieces[1].patches === 0 && patched.length === good.length + 64);
say("its pieces come back byte for byte", !!pBack && pBack.pieces.every((pc, i) => eq(pc.raw, pRaws[i])));
say("AISLING 1.5 (level 2) is told it needs a newer one", !!pBack && PF.read(patched, { level: 2 }).why === 2);
say("patches in a level 2 pack refused", typeof PF.write({ name: "Patched", level: 2, pieces: pRaws }) === "string");
if (pBack) {
  const five = patched.slice(); five[first + 19] = 16 | 4;
  say("a fifth layer's patch refused by both", fw(five) === 3 && PF.read(five).why === 3);
  const cut = patched.slice(0, patched.length - 16); cut[8] = cut.length & 255; cut[9] = cut.length >> 8;
  say("a patch cut off refused by both", fw(cut) === 3 && PF.read(cut).why === 3);
}
say("seven pieces refused", typeof PF.write({ name: "x", pieces: raws.slice(1) }) === "string");
say("a name too long refused", typeof PF.write({ name: "twenty characters xx", pieces: raws }) === "string");
const fat = new Uint8Array(16 + 4 + 3 * 255); fat.set(raws[0].subarray(0, 16)); fat[18] = 255;
say("too big refused", typeof PF.write({ name: "Big", pieces: Array.from({ length: 8 }, () => fat) }) === "string");
const at = (() => { const p = Array.from({ length: 8 }, () => PF.empty()); p[0] = fat.slice(); p[1] = fat.slice(); p[2] = fat.slice(); p[3] = fat.slice();
  const b = PF.write({ name: "Mark", pieces: p }); return b; })();
say("a big mix still reads through", typeof at !== "string" ? fw(at) === 0 : true);
const mid = new Uint8Array(16 + 4 + 3 * 140); mid.set(raws[0].subarray(0, 16)); mid[18] = 140;
const five = (m) => PF.write({ name: "Mark", pieces: [fat, fat, fat, fat, m, PF.empty(), PF.empty(), PF.empty()] });
const o = 0xE06 - 32 - 4 * fat.length, hit = mid.slice();   // (the fifth piece lies over 0xE06: a value's high byte, then a parameter)
hit[o] = 0x41; hit[o + 1] = 0x54;
const clean = five(mid), marked = five(hit);
say("a mix that passes 0xE06 clean is taken", typeof clean !== "string" && clean.length > 0xE08 && fw(clean) === 0);
say("the update loader's mark at 0xE06 refused", typeof marked === "string" && /order/.test(marked));
console.log(bad ? `packfile: ${bad} FAILED` : "packfile: passed");
process.exit(bad ? 1 : 0);
