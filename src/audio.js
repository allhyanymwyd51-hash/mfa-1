// أصوات إجرائية عبر WebAudio — بدون موسيقى (حسب الطلب)، أصوات محرك وخطوات وباب ونقود
export class AudioFX {
  constructor() { this.ctx = null; this.engineOsc = null; this.engineGain = null; }
  init() {
    if (this.ctx) return;
    try { this.ctx = new (window.AudioContext || window.webkitAudioContext)(); } catch (e) { }
  }
  beep(freq = 800, dur = 0.12, type = 'sine', vol = 0.15) {
    if (!this.ctx) return;
    const o = this.ctx.createOscillator(), g = this.ctx.createGain();
    o.type = type; o.frequency.value = freq;
    g.gain.setValueAtTime(vol, this.ctx.currentTime);
    g.gain.exponentialRampToValueAtTime(0.001, this.ctx.currentTime + dur);
    o.connect(g).connect(this.ctx.destination);
    o.start(); o.stop(this.ctx.currentTime + dur);
  }
  coin() { this.beep(1200, 0.1); setTimeout(() => this.beep(1600, 0.15), 70); }
  door() { this.beep(180, 0.25, 'triangle', 0.2); }
  step() { this.beep(90 + Math.random() * 40, 0.06, 'square', 0.05); }
  crash() { this.beep(70, 0.4, 'sawtooth', 0.3); }
  pickup() { this.beep(900, 0.08, 'sine', 0.12); }
  error() { this.beep(220, 0.2, 'square', 0.12); }
  engine(on, speed = 0) {
    if (!this.ctx) return;
    if (on && !this.engineOsc) {
      this.engineOsc = this.ctx.createOscillator();
      this.engineGain = this.ctx.createGain();
      this.engineOsc.type = 'sawtooth'; this.engineOsc.frequency.value = 60;
      this.engineGain.gain.value = 0.03;
      this.engineOsc.connect(this.engineGain).connect(this.ctx.destination);
      this.engineOsc.start();
    } else if (!on && this.engineOsc) {
      this.engineOsc.stop(); this.engineOsc = null;
    }
    if (this.engineOsc) this.engineOsc.frequency.value = 60 + Math.abs(speed) * 6;
  }
}
