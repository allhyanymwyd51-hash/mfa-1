// توليد خامات واقعية إجرائياً (بدون ملفات خارجية) — واجهات مباني، أسفلت، رمل، لافتات عربية
import * as THREE from 'three';

function canvasTex(w, h, draw, repeatX = 1, repeatY = 1) {
  const c = document.createElement('canvas'); c.width = w; c.height = h;
  draw(c.getContext('2d'), w, h);
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping;
  t.repeat.set(repeatX, repeatY);
  t.anisotropy = 4;
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

const rnd = (a, b) => a + Math.random() * (b - a);

// واجهة مبنى بنوافذ مضيئة/مطفأة
export function buildingFacade(baseColor, floors = 6, cols = 5) {
  return canvasTex(256, 512, (g, w, h) => {
    g.fillStyle = baseColor; g.fillRect(0, 0, w, h);
    // تدرج خفيف
    const grad = g.createLinearGradient(0, 0, 0, h);
    grad.addColorStop(0, 'rgba(255,255,255,0.10)'); grad.addColorStop(1, 'rgba(0,0,0,0.22)');
    g.fillStyle = grad; g.fillRect(0, 0, w, h);
    const cw = w / cols, ch = h / floors;
    for (let f = 0; f < floors; f++) for (let c = 0; c < cols; c++) {
      const x = c * cw + cw * 0.22, y = f * ch + ch * 0.25, ww = cw * 0.56, hh = ch * 0.5;
      const lit = Math.random() < 0.45;
      g.fillStyle = lit ? '#ffd98a' : '#1d2b3a';
      g.fillRect(x, y, ww, hh);
      g.fillStyle = 'rgba(255,255,255,0.18)'; g.fillRect(x, y, ww, hh * 0.3);
      g.strokeStyle = 'rgba(0,0,0,0.35)'; g.strokeRect(x, y, ww, hh);
    }
    // شريط أفقي بين الطوابق
    g.fillStyle = 'rgba(0,0,0,0.18)';
    for (let f = 1; f < floors; f++) g.fillRect(0, f * ch - 2, w, 4);
  });
}

export function shopFacade(name, color) {
  return canvasTex(512, 256, (g, w, h) => {
    g.fillStyle = '#e8e2d5'; g.fillRect(0, 0, w, h);
    // واجهة زجاجية سفلية
    g.fillStyle = '#2a3f52'; g.fillRect(0, h * 0.45, w, h * 0.55);
    g.fillStyle = 'rgba(160,210,240,0.35)';
    for (let i = 0; i < 6; i++) g.fillRect(i * (w / 6) + 6, h * 0.48, w / 6 - 12, h * 0.48);
    // لافتة عربية
    g.fillStyle = color; g.fillRect(0, h * 0.08, w, h * 0.3);
    g.fillStyle = '#fff'; g.font = 'bold 52px Tahoma, Arial'; g.textAlign = 'center'; g.textBaseline = 'middle';
    g.fillText(name, w / 2, h * 0.23);
    g.fillStyle = 'rgba(255,255,255,0.85)'; g.font = '20px Tahoma';
    g.fillText('MFA 1 • السعودية', w / 2, h * 0.33);
  });
}

export const roadTex = canvasTex(256, 256, (g, w, h) => {
  g.fillStyle = '#33383e'; g.fillRect(0, 0, w, h);
  for (let i = 0; i < 900; i++) {
    g.fillStyle = `rgba(${rnd(90,140)|0},${rnd(95,145)|0},${rnd(100,150)|0},0.25)`;
    g.fillRect(rnd(0, w), rnd(0, h), 2, 2);
  }
  // خط وسطي متقطع
  g.fillStyle = '#e8c832';
  for (let y = 8; y < h; y += 42) g.fillRect(w / 2 - 3, y, 6, 24);
}, 1, 4);

export const sidewalkTex = canvasTex(128, 128, (g, w, h) => {
  g.fillStyle = '#9aa0a6'; g.fillRect(0, 0, w, h);
  g.strokeStyle = '#7c8288'; g.lineWidth = 2;
  g.strokeRect(0, 0, w, h);
  for (let i = 0; i < 200; i++) { g.fillStyle = 'rgba(0,0,0,0.08)'; g.fillRect(rnd(0,w), rnd(0,h), 2, 2); }
}, 6, 6);

export const sandTex = canvasTex(256, 256, (g, w, h) => {
  g.fillStyle = '#d9b77c'; g.fillRect(0, 0, w, h);
  for (let i = 0; i < 1400; i++) {
    g.fillStyle = `rgba(${rnd(160,220)|0},${rnd(130,185)|0},${rnd(70,120)|0},0.3)`;
    g.fillRect(rnd(0, w), rnd(0, h), 2, 2);
  }
}, 40, 40);

export const grassTex = canvasTex(128, 128, (g, w, h) => {
  g.fillStyle = '#3f7d3a'; g.fillRect(0, 0, w, h);
  for (let i = 0; i < 700; i++) {
    g.fillStyle = `rgba(${rnd(40,90)|0},${rnd(110,160)|0},${rnd(40,80)|0},0.5)`;
    g.fillRect(rnd(0, w), rnd(0, h), 2, 3);
  }
}, 10, 10);

export function signTex(text, bg = '#0d5c34', fg = '#fff') {
  return canvasTex(512, 128, (g, w, h) => {
    g.fillStyle = bg; g.fillRect(0, 0, w, h);
    g.strokeStyle = fg; g.lineWidth = 6; g.strokeRect(4, 4, w - 8, h - 8);
    g.fillStyle = fg; g.font = 'bold 54px Tahoma, Arial'; g.textAlign = 'center'; g.textBaseline = 'middle';
    g.fillText(text, w / 2, h / 2);
  });
}

export const doorTex = canvasTex(128, 256, (g, w, h) => {
  g.fillStyle = '#5a3d22'; g.fillRect(0, 0, w, h);
  g.fillStyle = '#6e4c2a'; g.fillRect(8, 8, w - 16, h * 0.42); g.fillRect(8, h * 0.55, w - 16, h * 0.4);
  g.fillStyle = '#d9b45c'; g.beginPath(); g.arc(w - 22, h / 2, 7, 0, 7); g.fill();
});

export const floorTex = canvasTex(256, 256, (g, w, h) => {
  g.fillStyle = '#c9bda8'; g.fillRect(0, 0, w, h);
  g.strokeStyle = '#a89a80'; g.lineWidth = 2;
  for (let i = 0; i <= 4; i++) { g.beginPath(); g.moveTo(i * 64, 0); g.lineTo(i * 64, h); g.stroke(); g.beginPath(); g.moveTo(0, i * 64); g.lineTo(w, i * 64); g.stroke(); }
}, 4, 4);

export function carpetTex(color = '#8e2f2f') {
  return canvasTex(256, 256, (g, w, h) => {
    g.fillStyle = color; g.fillRect(0, 0, w, h);
    g.strokeStyle = '#e8c832'; g.lineWidth = 4; g.strokeRect(10, 10, w - 20, h - 20);
    g.strokeStyle = 'rgba(232,200,50,0.5)'; g.lineWidth = 2; g.strokeRect(24, 24, w - 48, h - 48);
    g.fillStyle = 'rgba(232,200,50,0.7)'; g.font = 'bold 60px Arial'; g.textAlign = 'center';
    g.fillText('✦', w / 2, h / 2 + 20);
  });
}

export function posterTex(text, color) {
  return canvasTex(128, 192, (g, w, h) => {
    g.fillStyle = '#f5f0e1'; g.fillRect(0, 0, w, h);
    g.fillStyle = color; g.fillRect(0, 0, w, h * 0.45);
    g.fillStyle = '#fff'; g.font = 'bold 26px Tahoma'; g.textAlign = 'center';
    g.fillText(text, w / 2, h * 0.28);
    g.fillStyle = '#333'; g.font = '15px Tahoma';
    g.fillText('MFA 1', w / 2, h * 0.62);
    g.fillText('السعودية', w / 2, h * 0.74);
    g.fillStyle = '#999'; g.fillRect(14, h * 0.8, w - 28, 6);
  });
}
