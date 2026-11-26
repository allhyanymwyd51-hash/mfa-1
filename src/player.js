// شخصية اللاعب: إنسان آلي (ثوب سعودي) مع أنيميشن مشي/ركض/قفز إجرائي
import * as THREE from 'three';
import { CITY } from './world.js';

export class Player {
  constructor(scene) {
    this.scene = scene;
    this.group = new THREE.Group();
    this.velocity = new THREE.Vector3();
    this.yaw = 0; this.vy = 0; this.onGround = true;
    this.walkPhase = 0; this.speed = 0;
    this.buildMesh();
    scene.add(this.group);
  }

  buildMesh() {
    const g = this.group;
    const thobe = new THREE.MeshStandardMaterial({ color: 0xf5f0e1, roughness: 0.9 });
    const skin = new THREE.MeshStandardMaterial({ color: 0xc98a5a, roughness: 0.7 });
    const dark = new THREE.MeshStandardMaterial({ color: 0x222222, roughness: 0.6 });

    // الجذع (ثوب طويل)
    this.torso = new THREE.Mesh(new THREE.BoxGeometry(0.62, 0.95, 0.36), thobe);
    this.torso.position.y = 1.15; this.torso.castShadow = true; g.add(this.torso);
    // تنورة الثوب
    const skirt = new THREE.Mesh(new THREE.CylinderGeometry(0.34, 0.44, 0.7, 10), thobe);
    skirt.position.y = 0.45; skirt.castShadow = true; g.add(skirt);
    // الرأس
    this.head = new THREE.Mesh(new THREE.SphereGeometry(0.21, 14, 12), skin);
    this.head.position.y = 1.86; this.head.castShadow = true; g.add(this.head);
    // الغترة والعقال
    const ghutra = new THREE.Mesh(new THREE.SphereGeometry(0.24, 14, 10, 0, Math.PI * 2, 0, Math.PI * 0.62),
      new THREE.MeshStandardMaterial({ color: 0xffffff, roughness: 0.95 }));
    ghutra.position.y = 1.92; g.add(ghutra);
    const agal = new THREE.Mesh(new THREE.TorusGeometry(0.2, 0.035, 8, 16), dark);
    agal.position.y = 2.02; agal.rotation.x = Math.PI / 2; g.add(agal);
    // الأذرع
    const mkArm = (side) => {
      const pivot = new THREE.Group(); pivot.position.set(side * 0.4, 1.52, 0);
      const arm = new THREE.Mesh(new THREE.BoxGeometry(0.16, 0.72, 0.16), thobe);
      arm.position.y = -0.34; arm.castShadow = true; pivot.add(arm);
      const hand = new THREE.Mesh(new THREE.SphereGeometry(0.09, 8, 8), skin);
      hand.position.y = -0.74; pivot.add(hand);
      g.add(pivot); return pivot;
    };
    this.armL = mkArm(-1); this.armR = mkArm(1);
    // الأرجل (تظهر تحت الثوب)
    const mkLeg = (side) => {
      const pivot = new THREE.Group(); pivot.position.set(side * 0.16, 0.42, 0);
      const leg = new THREE.Mesh(new THREE.BoxGeometry(0.2, 0.42, 0.22), dark);
      leg.position.y = -0.2; leg.castShadow = true; pivot.add(leg);
      g.add(pivot); return pivot;
    };
    this.legL = mkLeg(-1); this.legR = mkLeg(1);
  }

  get position() { return this.group.position; }
  set position(v) { this.group.position.copy(v); }

  update(dt, move, running, jumping, world) {
    const maxSpeed = running ? 11 : 5.2;
    const accel = this.onGround ? 40 : 8;
    const target = new THREE.Vector3(move.x, 0, move.z).multiplyScalar(maxSpeed);
    this.velocity.x += (target.x - this.velocity.x) * Math.min(1, accel * dt / maxSpeed);
    this.velocity.z += (target.z - this.velocity.z) * Math.min(1, accel * dt / maxSpeed);
    this.speed = Math.hypot(this.velocity.x, this.velocity.z);

    // مواجهة اتجاه الحركة
    if (this.speed > 0.4) {
      const targetYaw = Math.atan2(this.velocity.x, this.velocity.z);
      let diff = targetYaw - this.yaw;
      while (diff > Math.PI) diff -= Math.PI * 2;
      while (diff < -Math.PI) diff += Math.PI * 2;
      this.yaw += diff * Math.min(1, dt * 10);
    }
    this.group.rotation.y = this.yaw;

    // قفز وجاذبية
    if (jumping && this.onGround) { this.vy = 7.5; this.onGround = false; }
    this.vy -= 22 * dt;
    this.group.position.y += this.vy * dt;
    if (this.group.position.y <= 0) { this.group.position.y = 0; this.vy = 0; this.onGround = true; }

    // حركة + تصادم
    let nx = this.group.position.x + this.velocity.x * dt;
    let nz = this.group.position.z + this.velocity.z * dt;
    if (world && !world.collides(nx, this.group.position.z)) this.group.position.x = nx; else this.velocity.x = 0;
    if (world && !world.collides(this.group.position.x, nz)) this.group.position.z = nz; else this.velocity.z = 0;
    // حدود المدينة
    const H = CITY.size / 2;
    this.group.position.x = THREE.MathUtils.clamp(this.group.position.x, -H, H);
    this.group.position.z = THREE.MathUtils.clamp(this.group.position.z, -H, H);

    // ===== أنيميشن المشي =====
    this.walkPhase += dt * (4 + this.speed * 1.6);
    const s = Math.sin(this.walkPhase), amp = Math.min(0.75, this.speed * 0.09);
    this.legL.rotation.x = s * amp; this.legR.rotation.x = -s * amp;
    this.armL.rotation.x = -s * amp * 0.8; this.armR.rotation.x = s * amp * 0.8;
    this.group.position.y += 0; // القفز يُحسب أعلاه
    const bob = Math.abs(Math.cos(this.walkPhase)) * 0.05 * Math.min(1, this.speed / 5);
    this.torso.position.y = 1.15 + bob;
    this.head.position.y = 1.86 + bob;
    if (!this.onGround) { this.legL.rotation.x = 0.5; this.legR.rotation.x = -0.3; this.armL.rotation.x = -2.4; this.armR.rotation.x = -2.4; }
  }
}
