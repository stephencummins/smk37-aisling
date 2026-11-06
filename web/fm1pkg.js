// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// FM-1 .fwsc packages in the browser: the package identity and the image the
// device reads during an update. (Also the SMK-37 Elite's: the same format.)

const BLOCKS = 20, MAX_BLOCKS = 64, BLK = 0x30, KEEP = 0x2F;

function crc16(d) {                                  // CRC-16/XMODEM
  let crc = 0;
  for (const b of d) {
    crc ^= b << 8;
    for (let k = 0; k < 8; k++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xFFFF : (crc << 1) & 0xFFFF;
  }
  return crc;
}

// how many 0x2F-byte blocks carry a marker byte: the file's length less the logical length in the
// package header (M-VAVE's FM-1 and SMK-37 Elite packages have 20, the SMK-37 Pro's and AISLING's
// SMK-37 Elite packages 36). The header is the logical image's first 0x40 bytes, ENC'd.
export function markerBlocks(fwsc) {
  if (fwsc.length < BLOCKS * BLK) throw new Error("not an FM-1 package (too short)");
  const h = new Uint8Array(0x40);
  h.set(fwsc.subarray(0, KEEP), 0);
  h.set(fwsc.subarray(BLK, BLK + 0x40 - KEEP), KEEP);
  for (let i = 0, key = 0xFFFF; i < 0x40; i++) {     // JieLi's ENC
    h[i] ^= key & 0xFF;
    key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF;
  }
  const v = new DataView(h.buffer);
  const n = fwsc.length - v.getUint32(4, true);
  const chip = String.fromCharCode(...h.subarray(16, 22));
  if (v.getUint16(0, true) !== crc16(h.subarray(2, 0x40)) || chip !== "AC791N" ||
      !(n >= BLOCKS && n <= MAX_BLOCKS) || n * BLK > fwsc.length)
    throw new Error("not an FM-1 package (no valid header)");
  return n;
}

// the package identity ("FM-1_904"): one marker byte after each of the first n blocks
export function productOf(fwsc) {
  const n = markerBlocks(fwsc);
  let s = "";
  for (let i = 0; i < n; i++) {
    const m = fwsc[i * BLK + KEEP];
    if (m !== 0x7D) s += String.fromCharCode((m - i - 1) & 0xFF);
  }
  return s;
}

// the image the device addresses during an update: the .fwsc without its n marker bytes
export function logicalImage(fwsc) {
  const n = markerBlocks(fwsc);
  const out = new Uint8Array(fwsc.length - n);
  for (let i = 0; i < n; i++) out.set(fwsc.subarray(i * BLK, i * BLK + KEEP), i * KEEP);
  out.set(fwsc.subarray(n * BLK), n * KEEP);
  return out;
}
