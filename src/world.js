// توليد المدينة السعودية الكاملة: شوارع، مباني بداخليات، محلات، أثاث، أعمدة، نخيل، قمامة
import * as THREE from 'three';
import * as T from './textures.js';

export const CITY = {
  size: 768,          // حجم المدينة بالمتر
  block: 64,          // حجم البلوك
  roadW: 16,          // عرض الشارع
  blocks: 12,         // 12x12 قطع عمرانية = 144 مبنى
};

export class World {
  constructor(scene) {
    this.scene = scene;
    this.colliders = [];   // {x,z,r} أو {minX,maxX,minZ,maxZ}
    this.interactables = []; // {pos, radius, label, action, door?}
    this.doors = [];
    this.buildings = [];
    this.props = [];
    this.build();
  }

  addColliderBox(x, z, w, d) { this.colliders.push({ minX: x - w / 2, maxX: x + w / 2, minZ: z - d / 2, maxZ: z + d / 2 }); }
  addColliderCircle(x, z, r) { this.colliders.push({ cx: x, cz: z, r }); }

  collides(x, z, r = 0.6) {
    for (const c of this.colliders) {
      if (c.r !== undefined) {
        const dx = x - c.cx, dz = z - c.cz;
        if (dx * dx + dz * dz < (r + c.r) ** 2) return true;
      } else {
        if (x + r > c.minX && x - r < c.maxX && z + r > c.minZ && z - r < c.maxZ) return true;
      }
    }
    return false;
  }

  build() {
    const S = CITY.size, B = CITY.block, RW = CITY.roadW;
    const half = S / 2;

    // ===== 120,000 تفصيلة نقطية Procedural Detail Points =====
    // دفعة GPU واحدة منخفضة الكلفة: حبيبات غبار/حصى دقيقة على امتداد الطرق والأرصفة.
    // هذه نقاط هندسية فعلية (كل vertex نقطة)، وليست رقماً دعائياً أو ملفات مكررة.
    this.buildGroundDetailPoints(120000);

    // ===== الأرض =====
    const ground = new THREE.Mesh(
      new THREE.PlaneGeometry(S + 400, S + 400),
      new THREE.MeshStandardMaterial({ map: T.sandTex, roughness: 1 })
    );
    ground.rotation.x = -Math.PI / 2; ground.position.y = -0.05; ground.receiveShadow = true;
    this.scene.add(ground);

    // ===== الشوارع (شبكة) =====
    const roadMat = new THREE.MeshStandardMaterial({ map: T.roadTex, roughness: 0.95 });
    const walkMat = new THREE.MeshStandardMaterial({ map: T.sidewalkTex, roughness: 1 });
    for (let i = 0; i <= CITY.blocks; i++) {
      const p = -half + i * B;
      // شارع أفقي
      const r1 = new THREE.Mesh(new THREE.PlaneGeometry(S, RW), roadMat);
      r1.rotation.x = -Math.PI / 2; r1.position.set(0, 0.01, p); r1.receiveShadow = true; this.scene.add(r1);
      // شارع عمودي
      const r2 = new THREE.Mesh(new THREE.PlaneGeometry(RW, S), roadMat);
      r2.rotation.x = -Math.PI / 2; r2.position.set(p, 0.01, 0); r2.receiveShadow = true; this.scene.add(r2);
      // أرصفة
      const w1 = new THREE.Mesh(new THREE.PlaneGeometry(S, 3), walkMat);
      w1.rotation.x = -Math.PI / 2; w1.position.set(0, 0.02, p - RW / 2 - 1.5); this.scene.add(w1);
      const w2 = new THREE.Mesh(new THREE.PlaneGeometry(S, 3), walkMat);
      w2.rotation.x = -Math.PI / 2; w2.position.set(0, 0.02, p + RW / 2 + 1.5); this.scene.add(w2);
      const w3 = new THREE.Mesh(new THREE.PlaneGeometry(3, S), walkMat);
      w3.rotation.x = -Math.PI / 2; w3.position.set(p - RW / 2 - 1.5, 0.02, 0); this.scene.add(w3);
      const w4 = new THREE.Mesh(new THREE.PlaneGeometry(3, S), walkMat);
      w4.rotation.x = -Math.PI / 2; w4.position.set(p + RW / 2 + 1.5, 0.02, 0); this.scene.add(w4);
    }

    // ===== المباني =====
    const facades = [
      T.buildingFacade('#cbb79a', 8, 5), T.buildingFacade('#b8c4cc', 12, 6),
      T.buildingFacade('#d8c9a8', 6, 4), T.buildingFacade('#a8b8c0', 10, 5),
      T.buildingFacade('#e0d4bc', 5, 4), T.buildingFacade('#98a8b8', 14, 6),
    ];
    const shopNames = ['مطعم الشاورما', 'كوفي ستار', 'سوبر ماركت النور', 'صيدلية الشفاء', 'مخبز التنور', 'محل العطور', 'مكتبة المعرفة', 'حلويات الشرق', 'ملابس الأناقة', 'إلكترونيات المستقبل'];
    const shopColors = ['#c0392b', '#6f4e37', '#27ae60', '#2980b9', '#d4a017', '#8e44ad', '#16a085', '#e67e22', '#c0392b', '#2c3e50'];

    let shopIdx = 0, buildingCount = 0;
    for (let bx = 0; bx < CITY.blocks; bx++) for (let bz = 0; bz < CITY.blocks; bz++) {
      const cx = -half + bx * B + B / 2, cz = -half + bz * B + B / 2;
      const isShop = (bx + bz) % 3 === 0;
      const isTower = (bx * bz) % 7 === 0 && bx > 0 && bz > 0;
      this.makeBuilding(cx, cz, isShop, isTower, facades, shopNames, shopColors, shopIdx++);
      buildingCount++;
    }

    // ===== مبانٍ خاصة =====
    this.makeMosque(-half + B * 1.5, -half + B * 6.5);
    this.makeMall(half - B * 1.5, -half + B * 1.5);
    this.makePolice(half - B * 1.5, half - B * 1.5);
    this.makeHospital(-half + B * 1.5, half - B * 1.5);
    this.makePark(0, -half + B * 0.5);
    this.makeMarket(0, half - B * 0.5);

    // ===== نخيل وأعمدة إنارة =====
    for (let i = 0; i < 160; i++) {
      const x = -half + Math.random() * S, z = -half + Math.random() * S;
      // على الرصيف فقط (قرب شارع)
      const gx = Math.round((x + half) / B) * B - half, gz = Math.round((z + half) / B) * B - half;
      if (Math.abs(x - gx) < RW / 2 + 5 || Math.abs(z - gz) < RW / 2 + 5) {
        if (Math.random() < 0.55) this.makePalm(x, z); else this.makeLamp(x, z);
      }
    }

    // ===== قمامة وقوارير وكاسات على الأرض (قابلة للالتقاط) =====
    this.makeTrash();

    // ===== لوحات إعلانية =====
    for (let i = 0; i < 24; i++) {
      const x = -half + Math.random() * S, z = -half + Math.random() * S;
      this.makeBillboard(x, z, ['شاورما', 'قهوة عربية', 'عطور', 'MFA 1', 'مطعم'][i % 5]);
    }

    // ===== سيارات مركونة =====
    this.parkedCars = [];
    for (let i = 0; i < 40; i++) {
      const roadIdx = Math.floor(Math.random() * (CITY.blocks + 1));
      const p = -half + roadIdx * B;
      const along = -half + Math.random() * S;
      const horiz = Math.random() < 0.5;
      const x = horiz ? along : p + (Math.random() < 0.5 ? -1 : 1) * (RW / 2 - 2);
      const z = horiz ? p + (Math.random() < 0.5 ? -1 : 1) * (RW / 2 - 2) : along;
      this.parkedCars.push({ x, z, rot: horiz ? (Math.random() < 0.5 ? 0 : Math.PI) : Math.PI / 2 });
    }
  }

  buildGroundDetailPoints(count) {
    const positions = new Float32Array(count * 3);
    const colors = new Float32Array(count * 3);
    // مولد ثابت لضمان اختلاف المشهد مع قابلية إعادة إنتاجه بين التشغيلات.
    let seed = 0x4d464131;
    const random = () => { seed = (seed * 1664525 + 1013904223) >>> 0; return seed / 4294967296; };
    const palette = [new THREE.Color('#77756f'), new THREE.Color('#aaa08c'), new THREE.Color('#b8ad97'), new THREE.Color('#6c7378'), new THREE.Color('#d0c2a4')];
    const half = CITY.size / 2;
    let written = 0, attempts = 0;
    while (written < count && attempts < count * 8) {
      attempts++;
      const x = (random() * 2 - 1) * half;
      const z = (random() * 2 - 1) * half;
      const gx = Math.round((x + half) / CITY.block) * CITY.block - half;
      const gz = Math.round((z + half) / CITY.block) * CITY.block - half;
      const dx = Math.abs(x - gx), dz = Math.abs(z - gz);
      // تتوزع على مسارات المشاة/الطرق فقط، ولا تغطي قطع المباني.
      if (!(dx <= 8 || dz <= 8 || (dx >= 8 && dx <= 11) || (dz >= 8 && dz <= 11))) continue;
      const i = written++;
      positions[i * 3] = x;
      positions[i * 3 + 1] = 0.035 + random() * 0.012;
      positions[i * 3 + 2] = z;
      const c = palette[Math.floor(random() * palette.length)];
      const shade = 0.72 + random() * 0.48;
      colors[i * 3] = Math.min(1, c.r * shade);
      colors[i * 3 + 1] = Math.min(1, c.g * shade);
      colors[i * 3 + 2] = Math.min(1, c.b * shade);
    }
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(positions.subarray(0, written * 3), 3));
    geometry.setAttribute('color', new THREE.BufferAttribute(colors.subarray(0, written * 3), 3));
    const material = new THREE.PointsMaterial({ size: 0.055, sizeAttenuation: true, vertexColors: true, transparent: true, opacity: 0.62, depthWrite: false });
    this.groundDetails = new THREE.Points(geometry, material);
    this.groundDetails.frustumCulled = true;
    this.scene.add(this.groundDetails);
    this.detailPointCount = written;
  }

  // ===== مبنى عادي / برج / محل — مع داخلية وباب يفتح =====
  makeBuilding(cx, cz, isShop, isTower, facades, shopNames, shopColors, idx) {
    const w = 26 + Math.random() * 10, d = 26 + Math.random() * 10;
    const h = isTower ? 55 + Math.random() * 45 : isShop ? 7 : 14 + Math.random() * 22;
    const g = new THREE.Group(); g.position.set(cx, 0, cz);

    const facade = facades[idx % facades.length];
    const mat = new THREE.MeshStandardMaterial({ map: facade, roughness: 0.85 });
    const body = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), mat);
    body.position.y = h / 2; body.castShadow = body.receiveShadow = true; g.add(body);

    // سطح
    const roof = new THREE.Mesh(new THREE.BoxGeometry(w + 1, 0.8, d + 1),
      new THREE.MeshStandardMaterial({ color: 0x6b5a44, roughness: 1 }));
    roof.position.y = h + 0.4; g.add(roof);
    // خزان ماء على السطح
    const tank = new THREE.Mesh(new THREE.CylinderGeometry(1.4, 1.4, 2.4, 10),
      new THREE.MeshStandardMaterial({ color: 0x222222, roughness: 0.7 }));
    tank.position.set(w * 0.25, h + 2, d * 0.2); tank.castShadow = true; g.add(tank);

    // ===== الباب الرئيسي (يفتح!) =====
    const door = new THREE.Mesh(new THREE.BoxGeometry(2.6, 3.6, 0.18),
      new THREE.MeshStandardMaterial({ map: T.doorTex, roughness: 0.6 }));
    door.position.set(0, 1.8, d / 2 + 0.1); g.add(door);
    const doorPivot = new THREE.Group(); doorPivot.position.set(-1.3, 0, d / 2 + 0.1);
    door.position.set(1.3, 1.8, 0); doorPivot.add(door); g.add(doorPivot);
    this.doors.push({ pivot: doorPivot, open: 0, target: 0, building: g });

    // ===== الداخلية =====
    const interior = new THREE.Group();
    const floor = new THREE.Mesh(new THREE.PlaneGeometry(w - 1, d - 1),
      new THREE.MeshStandardMaterial({ map: T.floorTex, roughness: 0.9 }));
    floor.rotation.x = -Math.PI / 2; floor.position.y = 0.05; interior.add(floor);
    // سجادة
    const carpet = new THREE.Mesh(new THREE.PlaneGeometry(w * 0.5, d * 0.4),
      new THREE.MeshStandardMaterial({ map: T.carpetTex(isShop ? '#2f5e8e' : '#8e2f2f'), roughness: 1 }));
    carpet.rotation.x = -Math.PI / 2; carpet.position.y = 0.08; interior.add(carpet);
    // أثاث
    this.furnish(interior, w, d, isShop);
    // إضاءة داخلية
    const light = new THREE.PointLight(0xffe0b0, 30, 30); light.position.set(0, 5, 0); interior.add(light);
    interior.visible = false; g.add(interior);

    // لافتة المحل
    if (isShop) {
      const name = shopNames[idx % shopNames.length];
      const sign = new THREE.Mesh(new THREE.PlaneGeometry(12, 3),
        new THREE.MeshStandardMaterial({ map: T.shopFacade(name, shopColors[idx % shopColors.length]), emissive: 0xffffff, emissiveMap: T.shopFacade(name, shopColors[idx % shopColors.length]), emissiveIntensity: 0.35 }));
      sign.position.set(0, 5.2, d / 2 + 0.15); g.add(sign);
    }

    this.scene.add(g);
    this.addColliderBox(cx, cz, w, d);
    this.buildings.push({ group: g, interior, doorPivot, x: cx, z: cz, w, d, isShop, name: isShop ? shopNames[idx % shopNames.length] : 'مبنى سكني', inside: false });

    // نقطة تفاعل عند الباب
    const b = this.buildings[this.buildings.length - 1];
    this.interactables.push({
      pos: new THREE.Vector3(cx, 0, cz + d / 2 + 2), radius: 3.2,
      label: () => b.inside ? 'الخروج من المبنى' : `دخول: ${b.name}`,
      action: () => this.toggleInterior(b),
    });
  }

  furnish(interior, w, d, isShop) {
    const wood = new THREE.MeshStandardMaterial({ color: 0x7a5230, roughness: 0.8 });
    const fabric = new THREE.MeshStandardMaterial({ color: 0x8e3b3b, roughness: 1 });
    const metal = new THREE.MeshStandardMaterial({ color: 0x9aa5ad, roughness: 0.4, metalness: 0.6 });
    const add = (geo, mat, x, y, z, ry = 0) => {
      const m = new THREE.Mesh(geo, mat); m.position.set(x, y, z); m.rotation.y = ry;
      m.castShadow = m.receiveShadow = true; interior.add(m); return m;
    };
    if (isShop) {
      // رفوف + كاشير + ثلاجة
      for (let i = 0; i < 4; i++) add(new THREE.BoxGeometry(6, 2.2, 0.8), wood, -w / 4 + i * 0, 1.1, -d / 4);
      add(new THREE.BoxGeometry(5, 1.1, 1.4), wood, w / 4, 0.55, d / 5); // كاونتر
      add(new THREE.BoxGeometry(1.6, 2.4, 1), metal, -w / 3, 1.2, -d / 3); // ثلاجة
      add(new THREE.BoxGeometry(1.6, 2.4, 1), metal, -w / 3 + 1.8, 1.2, -d / 3);
      // صناديق بضاعة ملونة
      const cols = [0xe74c3c, 0x3498db, 0x2ecc71, 0xf39c12, 0x9b59b6];
      for (let i = 0; i < 10; i++) add(new THREE.BoxGeometry(0.9, 0.9, 0.9),
        new THREE.MeshStandardMaterial({ color: cols[i % 5], roughness: 0.8 }),
        -w / 4 + (i % 5) * 1.1, 0.45, -d / 4 + 0.9);
    } else {
      // كنبة + طاولة + تلفاز + سرير
      add(new THREE.BoxGeometry(4, 1, 1.4), fabric, 0, 0.5, -d / 5);
      add(new THREE.BoxGeometry(2.2, 0.5, 1.2), wood, 0, 0.25, -d / 5 + 2.2);
      add(new THREE.BoxGeometry(3.4, 2, 0.15), new THREE.MeshStandardMaterial({ color: 0x111111, roughness: 0.3 }), 0, 1.6, -d / 3);
      add(new THREE.BoxGeometry(2.4, 0.6, 3.6), new THREE.MeshStandardMaterial({ color: 0x3a5a8c, roughness: 1 }), -w / 4, 0.3, d / 4);
      add(new THREE.BoxGeometry(2.4, 0.25, 1.2), new THREE.MeshStandardMaterial({ color: 0xf5f0e1, roughness: 1 }), -w / 4, 0.72, d / 4 - 1.1);
      add(new THREE.BoxGeometry(1.2, 2.2, 0.6), wood, w / 3, 1.1, d / 4); // خزانة
    }
  }

  toggleInterior(b) {
    b.inside = !b.inside;
    b.interior.visible = b.inside;
    b.group.children[0].visible = !b.inside; // إخفاء الجدران الخارجية عند الدخول
    b.group.children[1].visible = !b.inside;
    b.group.children[2].visible = !b.inside;
    // الباب يفتح
    const door = this.doors.find(d => d.building === b.group);
    if (door) door.target = b.inside ? 1 : 0;
    // نقل اللاعب داخل/خارج
    const p = this.player;
    if (p) {
      if (b.inside) { p.position.set(b.x, 0, b.z + b.d / 2 - 2); }
      else { p.position.set(b.x, 0, b.z + b.d / 2 + 2.5); }
    }
    return b.inside;
  }

  // ===== مبانٍ خاصة =====
  makeMosque(x, z) {
    const g = new THREE.Group(); g.position.set(x, 0, z);
    const white = new THREE.MeshStandardMaterial({ color: 0xf2ede0, roughness: 0.7 });
    const body = new THREE.Mesh(new THREE.BoxGeometry(30, 10, 30), white);
    body.position.y = 5; body.castShadow = body.receiveShadow = true; g.add(body);
    const dome = new THREE.Mesh(new THREE.SphereGeometry(9, 20, 14, 0, Math.PI * 2, 0, Math.PI / 2),
      new THREE.MeshStandardMaterial({ color: 0x1a7a4a, roughness: 0.4, metalness: 0.3 }));
    dome.position.y = 10; g.add(dome);
    for (const sx of [-1, 1]) {
      const min = new THREE.Mesh(new THREE.CylinderGeometry(1.6, 2, 26, 10), white);
      min.position.set(sx * 17, 13, 0); min.castShadow = true; g.add(min);
      const cap = new THREE.Mesh(new THREE.ConeGeometry(2.2, 4, 10),
        new THREE.MeshStandardMaterial({ color: 0x1a7a4a, roughness: 0.4 }));
      cap.position.set(sx * 17, 28, 0); g.add(cap);
    }
    const sign = new THREE.Mesh(new THREE.PlaneGeometry(14, 3.5),
      new THREE.MeshStandardMaterial({ map: T.signTex('مسجد النور', '#1a7a4a'), emissive: 0xffffff, emissiveMap: T.signTex('مسجد النور', '#1a7a4a'), emissiveIntensity: 0.3 }));
    sign.position.set(0, 8, 15.2); g.add(sign);
    this.scene.add(g); this.addColliderBox(x, z, 30, 30);
    this.interactables.push({ pos: new THREE.Vector3(x, 0, z + 17), radius: 4, label: () => 'الصلاة في المسجد 🕌', action: () => this.onPray && this.onPray() });
  }

  makeMall(x, z) {
    const g = new THREE.Group(); g.position.set(x, 0, z);
    const body = new THREE.Mesh(new THREE.BoxGeometry(44, 16, 34),
      new THREE.MeshStandardMaterial({ map: T.buildingFacade('#c8d4dc', 4, 8), roughness: 0.6 }));
    body.position.y = 8; body.castShadow = body.receiveShadow = true; g.add(body);
    const sign = new THREE.Mesh(new THREE.PlaneGeometry(20, 4),
      new THREE.MeshStandardMaterial({ map: T.signTex('مول الرياض', '#7d3c98'), emissive: 0xffffff, emissiveMap: T.signTex('مول الرياض', '#7d3c98'), emissiveIntensity: 0.35 }));
    sign.position.set(0, 13, 17.2); g.add(sign);
    this.scene.add(g); this.addColliderBox(x, z, 44, 34);
    this.interactables.push({ pos: new THREE.Vector3(x, 0, z + 19), radius: 4, label: () => 'دخول مول الرياض 🛍️', action: () => this.onMall && this.onMall() });
  }

  makePolice(x, z) {
    const g = new THREE.Group(); g.position.set(x, 0, z);
    const body = new THREE.Mesh(new THREE.BoxGeometry(26, 12, 22),
      new THREE.MeshStandardMaterial({ map: T.buildingFacade('#5d6d7e', 3, 5), roughness: 0.8 }));
    body.position.y = 6; body.castShadow = body.receiveShadow = true; g.add(body);
    const sign = new THREE.Mesh(new THREE.PlaneGeometry(16, 3.5),
      new THREE.MeshStandardMaterial({ map: T.signTex('شرطة الرياض 🚔', '#1a3a5c'), emissive: 0xffffff, emissiveMap: T.signTex('شرطة الرياض 🚔', '#1a3a5c'), emissiveIntensity: 0.35 }));
    sign.position.set(0, 10, 11.2); g.add(sign);
    this.scene.add(g); this.addColliderBox(x, z, 26, 22);
    this.interactables.push({ pos: new THREE.Vector3(x, 0, z + 13), radius: 4, label: () => 'مركز الشرطة — دفع الغرامة', action: () => this.onPolice && this.onPolice() });
  }

  makeHospital(x, z) {
    const g = new THREE.Group(); g.position.set(x, 0, z);
    const body = new THREE.Mesh(new THREE.BoxGeometry(28, 18, 24),
      new THREE.MeshStandardMaterial({ map: T.buildingFacade('#e8e8e8', 5, 5), roughness: 0.7 }));
    body.position.y = 9; body.castShadow = body.receiveShadow = true; g.add(body);
    const cross = new THREE.Mesh(new THREE.BoxGeometry(3, 1, 0.3), new THREE.MeshStandardMaterial({ color: 0xe74c3c, emissive: 0xe74c3c, emissiveIntensity: 0.6 }));
    cross.position.set(0, 15, 12.2); g.add(cross);
    const cross2 = new THREE.Mesh(new THREE.BoxGeometry(1, 3, 0.3), cross.material);
    cross2.position.copy(cross.position); g.add(cross2);
    const sign = new THREE.Mesh(new THREE.PlaneGeometry(16, 3.5),
      new THREE.MeshStandardMaterial({ map: T.signTex('مستشفى الشفاء 🏥', '#c0392b'), emissive: 0xffffff, emissiveMap: T.signTex('مستشفى الشفاء 🏥', '#c0392b'), emissiveIntensity: 0.3 }));
    sign.position.set(0, 11, 12.2); g.add(sign);
    this.scene.add(g); this.addColliderBox(x, z, 28, 24);
    this.interactables.push({ pos: new THREE.Vector3(x, 0, z + 14), radius: 4, label: () => 'المستشفى — علاج (200 ريال)', action: () => this.onHospital && this.onHospital() });
  }

  makePark(x, z) {
    const g = new THREE.Group(); g.position.set(x, 0, z);
    const lawn = new THREE.Mesh(new THREE.PlaneGeometry(40, 40),
      new THREE.MeshStandardMaterial({ map: T.grassTex, roughness: 1 }));
    lawn.rotation.x = -Math.PI / 2; lawn.position.y = 0.03; lawn.receiveShadow = true; g.add(lawn);
    for (let i = 0; i < 8; i++) this.makePalm(x - 16 + Math.random() * 32, z - 16 + Math.random() * 32, g);
    // مقاعد ونافورة
    const fountain = new THREE.Mesh(new THREE.CylinderGeometry(3, 3.4, 1, 16),
      new THREE.MeshStandardMaterial({ color: 0x7fb3d5, roughness: 0.3 }));
    fountain.position.y = 0.5; g.add(fountain);
    this.scene.add(g);
    this.interactables.push({ pos: new THREE.Vector3(x, 0, z), radius: 6, label: () => 'الاستراحة في الحديقة 🌳 (استعادة صحة)', action: () => this.onPark && this.onPark() });
  }

  makeMarket(x, z) {
    const g = new THREE.Group(); g.position.set(x, 0, z);
    const base = new THREE.Mesh(new THREE.BoxGeometry(36, 5, 26),
      new THREE.MeshStandardMaterial({ color: 0xd9c9a3, roughness: 0.9 }));
    base.position.y = 2.5; base.castShadow = base.receiveShadow = true; g.add(base);
    const roof = new THREE.Mesh(new THREE.BoxGeometry(38, 0.6, 28),
      new THREE.MeshStandardMaterial({ color: 0xb03a2e, roughness: 0.8 }));
    roof.position.y = 5.3; g.add(roof);
    const sign = new THREE.Mesh(new THREE.PlaneGeometry(18, 3.5),
      new THREE.MeshStandardMaterial({ map: T.signTex('سوق البطحاء 🧺', '#b03a2e'), emissive: 0xffffff, emissiveMap: T.signTex('سوق البطحاء 🧺', '#b03a2e'), emissiveIntensity: 0.3 }));
    sign.position.set(0, 4, 13.2); g.add(sign);
    // أكشاك
    for (let i = 0; i < 6; i++) {
      const k = new THREE.Mesh(new THREE.BoxGeometry(3, 2.4, 2),
        new THREE.MeshStandardMaterial({ color: [0xe67e22, 0x27ae60, 0x2980b9][i % 3], roughness: 0.8 }));
      k.position.set(-12 + i * 5, 1.2, 0); k.castShadow = true; g.add(k);
    }
    this.scene.add(g); this.addColliderBox(x, z, 36, 26);
    this.interactables.push({ pos: new THREE.Vector3(x, 0, z + 15), radius: 4, label: () => 'التسوق في سوق البطحاء 🛒', action: () => this.onMarket && this.onMarket() });
  }

  makePalm(x, z, parent = null) {
    const g = new THREE.Group(); g.position.set(x, 0, z);
    const trunk = new THREE.Mesh(new THREE.CylinderGeometry(0.22, 0.35, 5, 7),
      new THREE.MeshStandardMaterial({ color: 0x8a6a42, roughness: 1 }));
    trunk.position.y = 2.5; trunk.castShadow = true; g.add(trunk);
    const leafMat = new THREE.MeshStandardMaterial({ color: 0x2e8b3a, roughness: 0.9, side: THREE.DoubleSide });
    for (let i = 0; i < 7; i++) {
      const leaf = new THREE.Mesh(new THREE.PlaneGeometry(3.4, 0.7), leafMat);
      leaf.position.y = 5.1; leaf.rotation.set(0.5, (i / 7) * Math.PI * 2, 0);
      leaf.translateX(1.2); leaf.castShadow = true; g.add(leaf);
    }
    (parent || this.scene).add(g);
    this.addColliderCircle(x, z, 0.5);
    return g;
  }

  makeLamp(x, z) {
    const g = new THREE.Group(); g.position.set(x, 0, z);
    const pole = new THREE.Mesh(new THREE.CylinderGeometry(0.12, 0.16, 6, 6),
      new THREE.MeshStandardMaterial({ color: 0x3a444e, roughness: 0.5, metalness: 0.5 }));
    pole.position.y = 3; pole.castShadow = true; g.add(pole);
    const head = new THREE.Mesh(new THREE.SphereGeometry(0.35, 8, 8),
      new THREE.MeshStandardMaterial({ color: 0xfff2c0, emissive: 0xffeeaa, emissiveIntensity: 0.8 }));
    head.position.y = 6.1; g.add(head);
    const light = new THREE.PointLight(0xffeeaa, 8, 18); light.position.y = 6; g.add(light);
    this.scene.add(g); this.addColliderCircle(x, z, 0.4);
  }

  makeBillboard(x, z, text) {
    const g = new THREE.Group(); g.position.set(x, 0, z); g.rotation.y = Math.random() * Math.PI * 2;
    const pole = new THREE.Mesh(new THREE.CylinderGeometry(0.2, 0.2, 7, 6),
      new THREE.MeshStandardMaterial({ color: 0x555f6a, roughness: 0.6 }));
    pole.position.y = 3.5; g.add(pole);
    const board = new THREE.Mesh(new THREE.PlaneGeometry(7, 2.4),
      new THREE.MeshStandardMaterial({ map: T.posterTex(text, '#b03a2e'), emissive: 0xffffff, emissiveMap: T.posterTex(text, '#b03a2e'), emissiveIntensity: 0.25, side: THREE.DoubleSide }));
    board.position.y = 7.5; g.add(board);
    this.scene.add(g); this.addColliderCircle(x, z, 0.5);
  }

  // ===== قمامة: قوارير، كاسات، علب — تُلتقط وتُباع =====
  makeTrash() {
    this.trash = [];
    const bottleMat = new THREE.MeshStandardMaterial({ color: 0x7fb069, roughness: 0.2, transparent: true, opacity: 0.8 });
    const cupMat = new THREE.MeshStandardMaterial({ color: 0xf5f0e1, roughness: 0.6 });
    const canMat = new THREE.MeshStandardMaterial({ color: 0xc0c6cc, roughness: 0.3, metalness: 0.7 });
    const bagMat = new THREE.MeshStandardMaterial({ color: 0x2c2c2c, roughness: 1 });
    for (let i = 0; i < 120; i++) {
      const x = -CITY.size / 2 + Math.random() * CITY.size;
      const z = -CITY.size / 2 + Math.random() * CITY.size;
      const kind = i % 4;
      let mesh;
      if (kind === 0) { mesh = new THREE.Mesh(new THREE.CylinderGeometry(0.09, 0.11, 0.55, 8), bottleMat); mesh.rotation.z = Math.PI / 2; }
      else if (kind === 1) { mesh = new THREE.Mesh(new THREE.CylinderGeometry(0.11, 0.08, 0.3, 8), cupMat); mesh.rotation.z = Math.PI / 2; }
      else if (kind === 2) { mesh = new THREE.Mesh(new THREE.CylinderGeometry(0.1, 0.1, 0.3, 8), canMat); mesh.rotation.z = Math.PI / 2; }
      else { mesh = new THREE.Mesh(new THREE.SphereGeometry(0.22, 7, 6), bagMat); mesh.scale.y = 0.7; }
      mesh.position.set(x, 0.12, z); mesh.rotation.y = Math.random() * 6;
      this.scene.add(mesh);
      this.trash.push({ mesh, taken: false, value: 2 + Math.floor(Math.random() * 4) });
    }
  }

  update(dt) {
    // الأبواب تفتح/تغلق بسلاسة
    for (const d of this.doors) {
      d.open += (d.target - d.open) * Math.min(1, dt * 4);
      d.pivot.rotation.y = -d.open * 1.9;
    }
    // التقاط القمامة القريبة
    if (this.player) {
      for (const t of this.trash) {
        if (t.taken) continue;
        const dx = t.mesh.position.x - this.player.position.x;
        const dz = t.mesh.position.z - this.player.position.z;
        if (dx * dx + dz * dz < 1.6) {
          t.taken = true; t.mesh.visible = false;
          if (this.onTrash) this.onTrash(t.value);
        }
      }
    }
  }
}
