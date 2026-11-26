// المشاة: شخصيات سعودية تمشي على الأرصفة مع أنيميشن + بائعون وحراس
import * as THREE from 'three';
import { CITY } from './world.js';

const NAMES = ['أبو فهد', 'أبو محمد', 'خالد', 'عبدالله', 'سعد', 'ناصر', 'فهد', 'ماجد', 'تركي', 'سلطان'];
const COLORS = [0xf5f0e1, 0xd8d2c0, 0x3a5a8c, 0x5a3a2a, 0x2e5e4e, 0x7a3b3b, 0x4a4a6a];

function makePerson(color) {
  const g = new THREE.Group();
  const thobe = new THREE.MeshStandardMaterial({ color, roughness: 0.9 });
  const skin = new THREE.MeshStandardMaterial({ color: 0xc98a5a, roughness: 0.7 });
  const torso = new THREE.Mesh(new THREE.BoxGeometry(0.55, 0.9, 0.32), thobe);
  torso.position.y = 1.12; torso.castShadow = true; g.add(torso);
  const skirt = new THREE.Mesh(new THREE.CylinderGeometry(0.3, 0.4, 0.65, 8), thobe);
  skirt.position.y = 0.42; g.add(skirt);
  const head = new THREE.Mesh(new THREE.SphereGeometry(0.19, 10, 8), skin);
  head.position.y = 1.8; head.castShadow = true; g.add(head);
  const ghutra = new THREE.Mesh(new THREE.SphereGeometry(0.22, 10, 8, 0, Math.PI * 2, 0, Math.PI * 0.6),
    new THREE.MeshStandardMaterial({ color: 0xffffff, roughness: 0.95 }));
  ghutra.position.y = 1.86; g.add(ghutra);
  const mkArm = (side) => {
    const p = new THREE.Group(); p.position.set(side * 0.36, 1.48, 0);
    const a = new THREE.Mesh(new THREE.BoxGeometry(0.14, 0.66, 0.14), thobe);
    a.position.y = -0.3; a.castShadow = true; p.add(a); g.add(p); return p;
  };
  const armL = mkArm(-1), armR = mkArm(1);
  return { group: g, armL, armR, phase: Math.random() * 10 };
}

export class NPCs {
  constructor(scene) {
    this.scene = scene;
    this.list = [];
    const half = CITY.size / 2;
    for (let i = 0; i < 46; i++) {
      const p = makePerson(COLORS[i % COLORS.length]);
      // نقطة بداية على رصيف
      const roadIdx = Math.floor(Math.random() * (CITY.blocks + 1));
      const rp = -half + roadIdx * CITY.block;
      const horiz = Math.random() < 0.5;
      const along = -half + Math.random() * CITY.size;
      const off = CITY.roadW / 2 + 1.5;
      p.group.position.set(horiz ? along : rp + off, 0, horiz ? rp + off : along);
      p.dir = horiz ? (Math.random() < 0.5 ? 1 : -1) : 0;
      p.dirZ = horiz ? 0 : (Math.random() < 0.5 ? 1 : -1);
      p.speed = 1.4 + Math.random() * 1.2;
      p.name = NAMES[i % NAMES.length];
      scene.add(p.group);
      this.list.push(p);
    }
    // بائعون ثابتون عند السوق والمحلات
    this.vendors = [];
    const vendorSpots = [[0, CITY.size / 2 - CITY.block * 0.5 + 15], [-CITY.size / 2 + CITY.block * 1.5, -CITY.size / 2 + CITY.block * 6.5 + 17]];
    for (const [x, z] of vendorSpots) {
      const p = makePerson(0x2e5e4e);
      p.group.position.set(x, 0, z); p.group.rotation.y = Math.PI;
      scene.add(p.group); this.vendors.push(p);
    }
  }

  update(dt, world) {
    const half = CITY.size / 2;
    for (const p of this.list) {
      const g = p.group;
      g.position.x += p.dir * p.speed * dt;
      g.position.z += p.dirZ * p.speed * dt;
      // انعكاس عند الحواف أو التصادم
      if (Math.abs(g.position.x) > half || Math.abs(g.position.z) > half || (world && world.collides(g.position.x, g.position.z, 0.4))) {
        g.position.x -= p.dir * p.speed * dt * 2;
        g.position.z -= p.dirZ * p.speed * dt * 2;
        p.dir *= -1; p.dirZ *= -1;
      }
      g.rotation.y = Math.atan2(p.dir, p.dirZ);
      p.phase += dt * 6;
      const s = Math.sin(p.phase);
      p.armL.rotation.x = -s * 0.5; p.armR.rotation.x = s * 0.5;
      g.position.y = Math.abs(Math.cos(p.phase)) * 0.04;
    }
    for (const v of this.vendors) {
      v.phase += dt * 2;
      v.armR.rotation.x = -1.2 + Math.sin(v.phase) * 0.3;
    }
  }

  nearest(pos, maxDist = 4) {
    let best = null, bd = maxDist;
    for (const p of [...this.list, ...this.vendors]) {
      const d = p.group.position.distanceTo(pos);
      if (d < bd) { bd = d; best = p; }
    }
    return best;
  }
}
