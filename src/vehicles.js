// السيارات: قابلة للقيادة + حركة مرور ذكية على الشوارع
import * as THREE from 'three';
import { CITY } from './world.js';

const CAR_COLORS = [0xe74c3c, 0x3498db, 0xf1c40f, 0x2ecc71, 0xecf0f1, 0x1a1a1a, 0x8e44ad, 0xe67e22, 0x16a085, 0x95a5a6];

function makeCar(color, type = 'sedan') {
  const g = new THREE.Group();
  const bodyMat = new THREE.MeshStandardMaterial({ color, roughness: 0.3, metalness: 0.6 });
  const glassMat = new THREE.MeshStandardMaterial({ color: 0x1a2a3a, roughness: 0.1, metalness: 0.8 });
  const L = type === 'suv' ? 4.6 : 4.2, W = type === 'suv' ? 2 : 1.8, H = type === 'suv' ? 1.1 : 0.75;
  const body = new THREE.Mesh(new THREE.BoxGeometry(W, H, L), bodyMat);
  body.position.y = 0.55; body.castShadow = true; g.add(body);
  const cabin = new THREE.Mesh(new THREE.BoxGeometry(W * 0.85, 0.55, L * 0.5), glassMat);
  cabin.position.set(0, 0.55 + H / 2 + 0.25, -0.2); cabin.castShadow = true; g.add(cabin);
  // عجلات
  const wheelGeo = new THREE.CylinderGeometry(0.34, 0.34, 0.24, 12);
  const wheelMat = new THREE.MeshStandardMaterial({ color: 0x111111, roughness: 0.9 });
  const wheels = [];
  for (const [x, z] of [[-W / 2, L / 2 - 0.8], [W / 2, L / 2 - 0.8], [-W / 2, -L / 2 + 0.8], [W / 2, -L / 2 + 0.8]]) {
    const w = new THREE.Mesh(wheelGeo, wheelMat);
    w.rotation.z = Math.PI / 2; w.position.set(x, 0.34, z); w.castShadow = true;
    g.add(w); wheels.push(w);
  }
  // مصابيح
  const lightMat = new THREE.MeshStandardMaterial({ color: 0xfff8c0, emissive: 0xffee88, emissiveIntensity: 0.7 });
  for (const x of [-W / 3, W / 3]) {
    const l = new THREE.Mesh(new THREE.BoxGeometry(0.4, 0.18, 0.08), lightMat);
    l.position.set(x, 0.7, L / 2 + 0.01); g.add(l);
  }
  return { group: g, wheels, type };
}

export class Vehicles {
  constructor(scene, world) {
    this.scene = scene; this.world = world;
    this.cars = [];
    this.driven = null;
    // سيارات مركونة قابلة للسرقة/القيادة
    for (const p of world.parkedCars) {
      const c = makeCar(CAR_COLORS[Math.floor(Math.random() * CAR_COLORS.length)], Math.random() < 0.3 ? 'suv' : 'sedan');
      c.group.position.set(p.x, 0, p.z); c.group.rotation.y = p.rot;
      c.speed = 0; c.steer = 0; c.parked = true;
      scene.add(c.group); this.cars.push(c);
    }
    // حركة المرور
    this.traffic = [];
    const half = CITY.size / 2;
    for (let i = 0; i < 14; i++) {
      const c = makeCar(CAR_COLORS[Math.floor(Math.random() * CAR_COLORS.length)]);
      const horiz = Math.random() < 0.5;
      const roadIdx = Math.floor(Math.random() * (CITY.blocks + 1));
      const rp = -half + roadIdx * CITY.block;
      const lane = (Math.random() < 0.5 ? -1 : 1) * (CITY.roadW / 4);
      c.group.position.set(horiz ? -half + Math.random() * CITY.size : rp + lane, 0, horiz ? rp + lane : -half + Math.random() * CITY.size);
      c.horiz = horiz; c.dir = Math.random() < 0.5 ? 1 : -1;
      c.speed = 8 + Math.random() * 5;
      c.group.rotation.y = horiz ? (c.dir > 0 ? Math.PI / 2 : -Math.PI / 2) : (c.dir > 0 ? 0 : Math.PI);
      scene.add(c.group); this.traffic.push(c);
    }
    // سيارة شرطة
    const police = makeCar(0x1a3a5c);
    police.group.position.set(half - CITY.block * 1.5, 0, half - CITY.block * 1.5 + 14);
    scene.add(police.group); this.cars.push(police);
  }

  nearestCar(pos, maxDist = 4.5) {
    let best = null, bd = maxDist;
    for (const c of this.cars) {
      const d = c.group.position.distanceTo(pos);
      if (d < bd) { bd = d; best = c; }
    }
    return best;
  }

  enter(car) { this.driven = car; car.parked = false; }
  exit() { if (this.driven) { this.driven.parked = true; this.driven.speed = 0; this.driven = null; } }

  update(dt, input, player) {
    // ===== السيارة المقودة =====
    if (this.driven) {
      const c = this.driven;
      const accel = input.forward ? 22 : input.back ? -14 : 0;
      c.speed += accel * dt;
      c.speed *= (1 - Math.min(0.9, dt * (input.forward || input.back ? 0.6 : 2.2)));
      c.speed = THREE.MathUtils.clamp(c.speed, -14, 38);
      if (input.left) c.steer = Math.min(0.5, c.steer + dt * 2.4); else if (input.right) c.steer = Math.max(-0.5, c.steer - dt * 2.4); else c.steer *= (1 - dt * 5);
      const turn = c.steer * c.speed * 0.055;
      c.group.rotation.y += turn * dt * 3;
      const fx = Math.sin(c.group.rotation.y), fz = Math.cos(c.group.rotation.y);
      let nx = c.group.position.x + fx * c.speed * dt;
      let nz = c.group.position.z + fz * c.speed * dt;
      if (!this.world.collides(nx, c.group.position.z, 1.2)) c.group.position.x = nx; else c.speed *= 0.4;
      if (!this.world.collides(c.group.position.x, nz, 1.2)) c.group.position.z = nz; else c.speed *= 0.4;
      for (const w of c.wheels) w.rotation.x += c.speed * dt / 0.34;
      // اللاعب يتبع السيارة
      player.position.copy(c.group.position);
      // صدم المشاة؟ (نظام المطلوب)
      if (Math.abs(c.speed) > 6 && this.onHitPedestrian) this.onHitPedestrian(c.group.position);
    }
    // ===== المرور =====
    const half = CITY.size / 2;
    for (const c of this.traffic) {
      if (c.horiz) c.group.position.x += c.dir * c.speed * dt; else c.group.position.z += c.dir * c.speed * dt;
      if (Math.abs(c.group.position.x) > half || Math.abs(c.group.position.z) > half) {
        // انعكاس: غيّر الشارع
        const roadIdx = Math.floor(Math.random() * (CITY.blocks + 1));
        const rp = -half + roadIdx * CITY.block;
        const lane = (Math.random() < 0.5 ? -1 : 1) * (CITY.roadW / 4);
        if (c.horiz) { c.group.position.z = rp + lane; c.group.position.x = -half + Math.random() * CITY.size; }
        else { c.group.position.x = rp + lane; c.group.position.z = -half + Math.random() * CITY.size; }
        c.dir = Math.random() < 0.5 ? 1 : -1;
        c.group.rotation.y = c.horiz ? (c.dir > 0 ? Math.PI / 2 : -Math.PI / 2) : (c.dir > 0 ? 0 : Math.PI);
      }
      for (const w of c.wheels) w.rotation.x += c.speed * dt / 0.34;
    }
  }
}
