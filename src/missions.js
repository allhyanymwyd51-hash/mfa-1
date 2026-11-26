// نظام المهام والوظائف: قصة رئيسية + توصيل + تاكسي + عمل في المحلات
import * as THREE from 'three';

export class Missions {
  constructor(game) {
    this.g = game;
    this.stage = 0;
    this.marker = new THREE.Mesh(
      new THREE.CylinderGeometry(2.2, 2.2, 0.3, 20),
      new THREE.MeshStandardMaterial({ color: 0xffd700, emissive: 0xffd700, emissiveIntensity: 0.8, transparent: true, opacity: 0.75 })
    );
    this.marker.visible = false;
    game.scene.add(this.marker);
    this.target = new THREE.Vector3();
    this.job = null; // {type, ...}
  }

  setTarget(x, z, title, text) {
    this.target.set(x, 0, z);
    this.marker.position.set(x, 0.15, z);
    this.marker.visible = true;
    this.g.ui.setMission(title, text);
  }

  clear() { this.marker.visible = false; this.g.ui.setMission('🗺️ عالم مفتوح', 'استكشف المدينة! ادخل المحلات، اركب سيارة، أو تحدث مع الناس (E).'); }

  update(dt) {
    if (this.marker.visible) {
      this.marker.rotation.y += dt * 2;
      this.marker.position.y = 0.15 + Math.sin(performance.now() * 0.004) * 0.08;
      const d = this.g.player.position.distanceTo(this.target);
      this.g.ui.setMissionDist(d);
      if (d < 3.5) this.onArrive();
    }
    if (this.job && this.job.type === 'taxi' && this.g.vehicles.driven) {
      // راكب التاكسي يركب
    }
  }

  onArrive() {
    const s = this.stage;
    if (this.job) { this.finishJob(); return; }
    if (s === 0) {
      this.g.ui.dialog('أبو فهد', 'هلا والله! أنا أبو فهد، عندي لك شغل. وصل هالطلبية لمطعم الشاورما عند السوق. خذ السيارة الحمراء وروح بسرعة!', [
        { label: 'تمام، وش المكافأة؟', fn: () => { this.stage = 1; this.setTarget(0, 240, '🚚 مهمة التوصيل', 'وصل الطلبية إلى سوق البطحاء'); this.g.ui.toast('وصل الطلبية إلى السوق!'); } },
        { label: 'ليش أنا؟', fn: () => { this.g.ui.dialog('أبو فهد', 'لأنك الوحيد اللي فاضي! يلا تحرك، الزبون جوعان 😄', [{ label: 'حاضر!', fn: () => { this.stage = 1; this.setTarget(0, 240, '🚚 مهمة التوصيل', 'وصل الطلبية إلى سوق البطحاء'); } }]); } },
      ]);
    } else if (s === 1) {
      this.g.addMoney(150); this.stage = 2;
      this.g.ui.dialog('صاحب المطعم', 'وصلت الطلبية! تفضل 150 ريال. عندي لك مهمة ثانية: الشرطة تدور على سيارة مسروقة قرب المستشفى، روح شوفها.', [
        { label: 'وش أسوي؟', fn: () => { this.setTarget(-240, 240, '🚔 التحقيق', 'اذهب إلى مستشفى الشفاء'); } },
      ]);
    } else if (s === 2) {
      this.stage = 3;
      this.g.ui.dialog('ضابط الشرطة', 'شكراً لوصولك. السيارة المسروقة هربت باتجاه المول. اقبض عليها وارجعها لنا ولك مكافأة 300 ريال. لا تخالف القانون!', [
        { label: 'تمام يا حضرة الضابط', fn: () => { this.setTarget(240, -240, '🚗 استرجاع السيارة', 'اذهب إلى مول الرياض'); } },
      ]);
    } else if (s === 3) {
      this.g.addMoney(300); this.stage = 4;
      this.g.ui.dialog('أبو فهد', 'عاشت! أنت صرت من أهل المدينة. جرّب الوظائف: توصيل، تاكسي، أو اشتغل في محل. واجمع القوارير من الشوارع وبيعها!', [
        { label: 'كيف أشتغل؟', fn: () => { this.clear(); this.g.ui.toast('تحدث مع الناس أو ادخل المحلات للوظائف!'); } },
      ]);
    }
  }

  // ===== الوظائف =====
  startJob(type) {
    if (this.job) return;
    if (type === 'delivery') {
      const spots = [[-240, -240], [240, 240], [-240, 240], [240, -240]];
      const [x, z] = spots[Math.floor(Math.random() * spots.length)];
      this.job = { type, pay: 120 };
      this.setTarget(x, z, '📦 وظيفة توصيل', 'وصل الطرد إلى الموقع الأصفر');
    } else if (type === 'taxi') {
      this.job = { type, pay: 200 };
      this.setTarget(0, 0, '🚕 وظيفة تاكسي', 'اذهب لاستلام الراكب في وسط المدينة');
    } else if (type === 'shop') {
      this.job = { type, pay: 100, ticks: 0 };
      this.g.ui.toast('اشتغلت في المحل! انتظر... ⏳');
      this.job.timer = setInterval(() => {
        if (!this.job) return;
        this.job.ticks++;
        this.g.ui.toast(`وردية المحل: ${this.job.ticks}/5`);
        if (this.job.ticks >= 5) this.finishJob();
      }, 4000);
    }
  }

  finishJob() {
    if (!this.job) return;
    if (this.job.timer) clearInterval(this.job.timer);
    const pay = this.job.pay;
    this.job = null;
    this.g.addMoney(pay);
    this.g.ui.toast(`💰 استلمت ${pay} ريال!`);
    this.clear();
  }

  cancelJob() {
    if (this.job && this.job.timer) clearInterval(this.job.timer);
    this.job = null; this.clear();
  }
}
