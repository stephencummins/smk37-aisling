// SPDX-License-Identifier: GPL-3.0-only
// The wall: AISLING's bit-art down the margins of a page, as the gnomes' pages have theirs, and as the visualiser paints
// its own from what is played. Here the page's name is the seed: its bits are the first row of 16 cells, and an elementary
// cellular automaton it chooses grows them 16 rows down, each column in a layer's colours (Heather, the visualiser's
// first palette) and one live cell, the spark, in the FM-1's orange. The same picture for a page every time.
{
  const wall = document.querySelector('.wall')
  const RULES = [30, 45, 73, 86, 89, 105, 110, 124, 135, 137, 149, 150, 193]
  const STOPS = [['#1d2230', '#3e2a22', '#6a2a3c', '#9a4a63'], ['#2e3b20', '#536a2a', '#7f8a35', '#b0ab55'],
                 ['#7a3a1c', '#b8622a', '#d99b5a', '#e8cfa0'], ['#262a4a', '#4a4272', '#7a7898', '#a7a6b0']]
  const fnv = (s) => {
    let h = 0x811c9dc5
    for (let i = 0; i < s.length; i++) h = Math.imul(h ^ s.charCodeAt(i), 0x01000193)
    return h >>> 0
  }
  const grow = (seed) => {
    const rows = [Array.from({ length: 16 }, (_, x) => (seed >>> x) & 1)]
    const rule = RULES[(seed >>> 16) % RULES.length]
    for (let y = 1; y < 16; y++) {
      const p = rows[y - 1]
      rows.push(p.map((_, x) => (rule >> ((p[(x + 15) % 16] << 2) | (p[x] << 1) | p[(x + 1) % 16])) & 1))
    }
    return rows
  }
  if (wall) {
    // some seeds die out or fill up: the page takes the next that leaves a picture
    let seed = fnv(wall.dataset.art || 'aisling'), rows = grow(seed), lit = 0
    for (let i = 0; i < 32; i++) {
      lit = rows.flat().filter(Boolean).length
      if (lit >= 70 && lit <= 170) break
      seed = fnv(seed.toString(36)); rows = grow(seed)
    }
    const hot = []
    rows.forEach((r, y) => r.forEach((on, x) => { if (on) hot.push([x, y]) }))
    const spark = hot[seed % hot.length] || [-1, -1]
    const c = document.createElement('canvas'), n = 28
    c.width = c.height = 16 * n
    const g = c.getContext('2d')
    rows.forEach((r, y) => r.forEach((on, x) => {
      if (!on) return
      g.fillStyle = x === spark[0] && y === spark[1] ? '#cb6b45' : STOPS[x >> 2][x & 3]
      g.fillRect(x * n, y * n, n, n)
    }))
    wall.style.backgroundImage = `url(${c.toDataURL()})`
    wall.classList.add('on')
  }
}
