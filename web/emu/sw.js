// SPDX-License-Identifier: GPL-3.0-only
// The emulator, kept for no connection: the network first, so a visit with one always gets the site as it is now,
// and whatever was last fetched otherwise. Everything the page asks for is kept as it goes by (the site's stylesheet
// and fonts too); the files below are fetched at once, so the first visit is enough to play offline.
const CACHE = "aisling-emu-2";
const CORE = ["./", "worklet.js", "amb.wasm", "manifest.webmanifest", "icons/touch-180.png", "icons/icon-192.png", "decals/flightdeck.webp"];
self.addEventListener("install", (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => Promise.all(CORE.map((u) => c.add(u).catch(() => {})))).then(() => self.skipWaiting()));
});
self.addEventListener("activate", (e) => e.waitUntil(self.clients.claim()));
self.addEventListener("fetch", (e) => {
  const req = e.request;
  if (req.method !== "GET" || !req.url.startsWith("http")) return;
  e.respondWith((async () => {
    const cache = await caches.open(CACHE);
    try {
      const res = await fetch(req);
      if (res.ok || res.type === "opaque") cache.put(req, res.clone()).catch(() => {});
      return res;
    } catch (err) {
      // the page is the same page whatever its query (?app)
      const hit = await cache.match(req, { ignoreSearch: req.mode === "navigate" });
      if (hit) return hit;
      throw err;
    }
  })());
});
