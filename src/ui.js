// واجهة اللعب العربية: صحة/جوع/فلوس/مطلوب/خريطة/حوارات/متجر
export class UI {
  constructor() {
    this.el = (id) => document.getElementById(id);
    this.hp = 100; this.hunger = 100; this.money = 500; this.wanted = 0;
    this.toastTimer = null;
    this.bind();
  }

  bind() {
    this.el('btnStart').onclick = () => this.onStart && this.onStart();
    this.el('btnHow').onclick = () => { const h = this.el('howText'); h.style.display = h.style.display === 'none' ? 'block' : 'none'; };
    this.el('btnNew').onclick = () => location.reload();
    this.el('shopClose').onclick = () => this.closeShop();
    document.addEventListener('keydown', (e) => { if (e.key === 'Escape') this.closeShop(); });
  }

  showMenu() { this.el('menu').style.display = 'flex'; this.el('loading').classList.add('hide'); }
  start() {
    this.el('menu').style.display = 'none';
    this.el('hud').classList.add('on');
    if ('ontouchstart' in window) this.el('touch').classList.add('on');
  }

  setLoading(pct, tip) { this.el('loadBar').style.width = pct + '%'; if (tip) this.el('loadTip').textContent = tip; }

  updateStats() {
    this.el('hpFill').style.width = Math.max(0, this.hp) + '%';
    this.el('hungerFill').style.width = Math.max(0, this.hunger) + '%';
    this.el('money').textContent = `💰 ${Math.floor(this.money)} ريال`;
    this.el('wanted').textContent = this.wanted > 0 ? '⭐'.repeat(this.wanted) + ' مطلوب!' : '';
  }

  addMoney(n) { this.money += n; this.updateStats(); }
  heal(n) { this.hp = Math.min(100, this.hp + n); this.updateStats(); }
  feed(n) { this.hunger = Math.min(100, this.hunger + n); this.updateStats(); }

  toast(msg, ms = 2600) {
    const t = this.el('toast'); t.textContent = msg; t.classList.add('show');
    clearTimeout(this.toastTimer);
    this.toastTimer = setTimeout(() => t.classList.remove('show'), ms);
  }

  setMission(title, text) { this.el('mTitle').textContent = title; this.el('mText').dataset.base = text; this.el('mText').innerHTML = text; }
  setMissionDist(d) {
    const base = this.el('mText').dataset.base || this.el('mText').innerHTML;
    this.el('mText').dataset.base = base;
    this.el('mText').innerHTML = `${base} <span class="dist">📍 ${Math.floor(d)}م</span>`;
  }

  interact(label) {
    const i = this.el('interact');
    if (label) { i.innerHTML = `اضغط <b>E</b> — ${label}`; i.classList.add('on'); }
    else i.classList.remove('on');
  }

  dialog(who, text, options = []) {
    this.el('dWho').textContent = who;
    this.el('dText').textContent = text;
    const box = this.el('dOpts'); box.innerHTML = '';
    for (const o of options) {
      const b = document.createElement('button');
      b.className = 'opt'; b.textContent = o.label;
      b.onclick = () => { this.closeDialog(); o.fn && o.fn(); };
      box.appendChild(b);
    }
    if (!options.length) {
      const b = document.createElement('button');
      b.className = 'opt'; b.textContent = 'حسناً';
      b.onclick = () => this.closeDialog();
      box.appendChild(b);
    }
    this.el('dialog').classList.add('on');
  }
  closeDialog() { this.el('dialog').classList.remove('on'); }
  dialogOpen() { return this.el('dialog').classList.contains('on'); }

  openShop(title, sub, items) {
    this.el('shopTitle').textContent = title;
    this.el('shopSub').textContent = sub;
    const box = this.el('shopItems'); box.innerHTML = '';
    for (const it of items) {
      const row = document.createElement('div'); row.className = 'item';
      row.innerHTML = `<div><div class="nm">${it.name}</div><div class="ds">${it.desc || ''}</div></div>`;
      const btn = document.createElement('button'); btn.className = 'buy'; btn.textContent = `${it.price} ريال`;
      btn.onclick = () => { this.onBuy && this.onBuy(it); };
      row.appendChild(btn); box.appendChild(row);
    }
    this.el('shop').classList.add('on');
  }
  closeShop() { this.el('shop').classList.remove('on'); }
  shopOpen() { return this.el('shop').classList.contains('on'); }

  drawMinimap(playerPos, targets, worldSize = 768) {
    const c = this.el('mapCanvas'), g = c.getContext('2d');
    const S = c.width, half = worldSize / 2, scale = S / worldSize;
    g.fillStyle = '#0d1520'; g.fillRect(0, 0, S, S);
    // شوارع
    g.strokeStyle = '#2a3f52'; g.lineWidth = 3;
    for (let i = 0; i <= 8; i++) {
      const p = (i * 64) * scale;
      g.beginPath(); g.moveTo(p, 0); g.lineTo(p, S); g.stroke();
      g.beginPath(); g.moveTo(0, p); g.lineTo(S, p); g.stroke();
    }
    const toMap = (x, z) => [(x + half) * scale, (z + half) * scale];
    // الهدف
    for (const t of targets) {
      const [mx, my] = toMap(t.x, t.z);
      g.fillStyle = t.color || '#ffd700';
      g.beginPath(); g.arc(mx, my, 7, 0, 7); g.fill();
      g.strokeStyle = '#fff'; g.lineWidth = 2; g.stroke();
    }
    // اللاعب
    const [px, py] = toMap(playerPos.x, playerPos.z);
    g.fillStyle = '#2ed573';
    g.beginPath(); g.arc(px, py, 6, 0, 7); g.fill();
    g.strokeStyle = '#fff'; g.lineWidth = 2; g.stroke();
  }
}
