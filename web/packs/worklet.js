// SPDX-License-Identifier: GPL-3.0-only
// The Packs page's preview: the firmware (the editor's amb.wasm), started with a sound pack in its store as an FM-1
// would have it in flash, playing the pack's pieces. Nothing here is a second engine: it is the one the FM-1 runs.
class PackPreview extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.playing = false;
    this.port.onmessage = (e) => this.onMessage(e.data);
  }
  async onMessage(m) {
    if (m.type === "load") {
      const eng = await globalThis.AislingEngine.start(m.wasm, () => currentTime, { [m.obj]: m.pack }, m.master);
      this.ex = eng.ex;
      this.mem = eng.mem;
      this.ex.ed_rest();
      this.playing = false;
      this.port.postMessage({ type: "ready", ok: !!this.ex.ed_pack(), level: this.ex.ed_level() });
    } else if (!this.ex) {
      return;
    } else if (m.type === "play") {                  // piece i of the pack, from its start
      this.ex.ed_pack_piece(m.i);
      if (!this.playing) this.ex.ed_play_toggle();
      this.playing = true;
    } else if (m.type === "stop") {
      if (this.playing) this.ex.ed_play_toggle();
      this.playing = false;
    }
  }
  process(inputs, outputs) {
    if (!this.ex) return true;
    const out = outputs[0], n = out[0].length, ex = this.ex;
    ex.web_render(n);
    out[0].set(new Float32Array(this.mem.buffer, ex.web_out_l(), n));
    if (out[1]) out[1].set(new Float32Array(this.mem.buffer, ex.web_out_r(), n));
    if (ex.web_midi_len()) ex.web_midi_clear();
    if (ex.web_said_len()) ex.web_said_clear();
    return true;
  }
}
registerProcessor("aisling-pack", PackPreview);
