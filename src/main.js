import * as THREE from 'three';
import { World, CITY } from './world.js';
import { Player } from './player.js';
import { NPCs } from './npc.js';
import { Vehicles } from './vehicles.js';
import { Missions } from './missions.js';
import { UI } from './ui.js';
import { AudioFX } from './audio.js';

const ui = new UI();
const audio = new AudioFX();
const input = { forward:false, back:false, left:false, right:false, run:false, jump:false };
const clock = new THREE.Clock();
let renderer, scene, camera, world, player, npcs, vehicles, missions;
let started = false, interactCandidate = null, currentInterior = null;
let cameraYaw = Math.PI, cameraPitch = 0.25, cameraDistance = 12;
let lastStep = 0, fpsFrames = 0, fpsTime = 0;
const raycaster = new THREE.Raycaster();

function init() {
  ui.setLoading(8, 'جاري تشغيل محرك MFA...');
  scene = new THREE.Scene();
  scene.background = new THREE.Color(0x9ec8e8);
  scene.fog = new THREE.Fog(0x9ec8e8, 180, 680);
  camera = new THREE.PerspectiveCamera(62, innerWidth / innerHeight, 0.1, 1000);
  renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
  renderer.setPixelRatio(Math.min(devicePixelRatio || 1, 1.7));
  renderer.setSize(innerWidth, innerHeight);
  renderer.shadowMap.enabled = true;
  renderer.shadowMap.type = THREE.PCFSoftShadowMap;
  renderer.outputColorSpace = THREE.SRGBColorSpace;
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 1.05;
  document.getElementById('game').appendChild(renderer.domElement);

  const hemi = new THREE.HemisphereLight(0xcde6ff, 0x8a6844, 2.1); scene.add(hemi);
  const sun = new THREE.DirectionalLight(0xffe0a6, 3.0);
  sun.position.set(-180, 260, 150); sun.castShadow = true;
  sun.shadow.mapSize.set(2048, 2048);
  sun.shadow.camera.left = sun.shadow.camera.bottom = -420;
  sun.shadow.camera.right = sun.shadow.camera.top = 420;
  sun.shadow.bias = -0.0003; scene.add(sun);

  ui.setLoading(20, 'رسم الشوارع والأحياء السعودية...');
  world = new World(scene);
  ui.setLoading(55, 'إنشاء الشخصيات والمباني القابلة للدخول...');
  player = new Player(scene);
  player.position.set(0, 0, -4);
  world.player = player;
  npcs = new NPCs(scene);
  vehicles = new Vehicles(scene, world);
  missions = new Missions({ scene, player, vehicles, ui, addMoney });
  wireWorld();
  ui.setLoading(92, 'تجهيز المهام والوظائف...');
  setupInput();
  setupTouch();
  ui.onStart = () => {
    started = true; audio.init();
    ui.start();
    missions.setTarget(0, -18, '📍 بداية الرحلة', 'تحدث مع أبو فهد وابدأ أول مهمة.');
    ui.toast('حياك الله في مدينة MFA! اقترب من أبو فهد واضغط E');
    renderer.domElement.requestPointerLock?.();
  };
  ui.onBuy = buyItem;
  setTimeout(() => { ui.setLoading(100, 'المدينة جاهزة — حيّاك الله!'); ui.showMenu(); }, 250);
  window.addEventListener('resize', onResize);
  renderer.domElement.addEventListener('click', () => { if (started && !ui.shopOpen() && !ui.dialogOpen() && document.pointerLockElement !== renderer.domElement) renderer.domElement.requestPointerLock?.(); });
  animate();
}

function addMoney(n) { ui.addMoney(n); audio.coin(); }

function wireWorld() {
  world.onTrash = (value) => { addMoney(value); audio.pickup(); ui.toast(`♻️ التقطت مخلفات وبعتها بـ ${value} ريال`); };
  world.onPray = () => ui.dialog('إمام المسجد', 'تقبل الله طاعتك. المسجد مفتوح للصلاة والراحة.', [{ label:'جزاك الله خير', fn:() => ui.heal(8) }]);
  world.onMall = () => openGeneralShop('🛍️ مول الرياض', 'تسوّق وارتاح في المول', [
    { name:'وجبة برجر', desc:'استعد الشبع والصحة', price:35, action:()=>{ui.feed(35);ui.heal(8);} },
    { name:'قهوة سعودية', desc:'ترفع النشاط', price:12, action:()=>ui.feed(12) },
    { name:'حقيبة إسعاف', desc:'استعادة 45 صحة', price:65, action:()=>ui.heal(45) },
  ]);
  world.onPolice = () => ui.dialog('مركز الشرطة', 'أهلاً بك. نبحث عن متعاونين للمساعدة في تنظيم المدينة.', [
    { label:'التطوع للشرطة', fn:()=>{ ui.toast('تم تسجيلك متعاوناً — تابع المهام الجديدة'); missions.startJob('delivery'); } },
    { label:'مغادرة', fn:()=>{} },
  ]);
  world.onHospital = () => ui.dialog('موظف الاستقبال', `العلاج الكامل بـ 200 ريال. رصيدك ${Math.floor(ui.money)} ريال.`, [
    { label:'أتعالج (200)', fn:()=>{if(ui.money>=200){ui.money-=200;ui.heal(100);ui.updateStats();ui.toast('تم العلاج، سلامتك!');}else ui.toast('رصيدك لا يكفي للعلاج');} },
    { label:'لا شكراً', fn:()=>{} },
  ]);
  world.onPark = () => { ui.heal(12); ui.toast('استرحت في الحديقة 🌴 +12 صحة'); };
  world.onMarket = () => openGeneralShop('🧺 سوق البطحاء', 'أكل شعبي وبضائع من السوق', [
    {name:'كبسة دجاج',desc:'وجبة مشبعة',price:28,action:()=>{ui.feed(40);ui.heal(5);}},
    {name:'تميس وفول',desc:'فطور شعبي',price:15,action:()=>ui.feed(25)},
    {name:'ماء بارد',desc:'يروي العطش',price:4,action:()=>ui.feed(8)},
  ]);
}

function openGeneralShop(title, sub, items) { ui.openShop(title, sub, items); }
function buyItem(item) {
  if (ui.money < item.price) { audio.error(); ui.toast('ما معك مبلغ كافي!'); return; }
  ui.money -= item.price; ui.updateStats(); item.action?.(); audio.coin(); ui.toast(`تم شراء ${item.name}`);
}

function interact() {
  if (!started || ui.dialogOpen() || ui.shopOpen()) return;
  audio.init();
  if (currentInterior) {
    const b = currentInterior;
    world.toggleInterior(b);
    currentInterior = null;
    audio.door();
    ui.toast('خرجت إلى الشارع');
    return;
  }
  if (vehicles.driven) {
    const c = vehicles.driven;
    vehicles.exit();
    player.position.set(c.group.position.x + 2.2, 0, c.group.position.z);
    ui.toast('نزلت من السيارة'); return;
  }
  if (interactCandidate?.type === 'world') {
    const item = interactCandidate.item;
    if (item.building) {
      const inside = world.toggleInterior(item.building);
      currentInterior = inside ? item.building : null;
      audio.door();
      if (inside) {
        ui.toast(`دخلت ${item.building.name}`);
        if (item.building.isShop) setTimeout(() => offerJob(item.building.name), 250);
      } else ui.toast('خرجت إلى الشارع');
      return;
    }
    item.action?.(); return;
  }
  if (interactCandidate?.type === 'car') {
    vehicles.enter(interactCandidate.car); audio.door();
    ui.toast('ركبت السيارة — W للتقدم، S للرجوع، A/D للتوجيه، E للنزول'); return;
  }
  if (interactCandidate?.type === 'npc') {
    const p = interactCandidate.npc;
    ui.dialog(p.name || 'أحد أهل الحي', 'السلام عليكم! تدور شغل؟ عندنا توصيل طلبات أو وظيفة تاكسي. وأهل المحلات يطلبون موظفين.', [
      {label:'أشتغل توصيل (120 ريال)',fn:()=>missions.startJob('delivery')},
      {label:'أشتغل تاكسي (200 ريال)',fn:()=>missions.startJob('taxi')},
      {label:'أدور محل أتوظف فيه',fn:()=>ui.toast('ادخل محلاً واضغط E للتقديم على وظيفة')},
    ]); return;
  }
}

function offerJob(shopName) {
  if (missions.job) return;
  ui.dialog(shopName, `السلام عليكم! عندنا شاغر في ${shopName}. تبي تتوظف؟ دوام قصير ومكافأة 100 ريال.`, [
    {label:'إيه، أبدأ الدوام',fn:()=>missions.startJob('shop')},
    {label:'خلها مرة ثانية',fn:()=>{}},
  ]);
}

function setupInput() {
  const keys = new Set();
  addEventListener('keydown', e => {
    keys.add(e.code);
    if (['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','Space'].includes(e.code)) e.preventDefault();
    if (e.code === 'KeyE' && !e.repeat) interact();
    if (e.code === 'Escape') { if (ui.dialogOpen()) ui.closeDialog(); document.exitPointerLock?.(); }
    updateKeys();
  });
  addEventListener('keyup', e => { keys.delete(e.code); updateKeys(); });
  function updateKeys() {
    input.forward = keys.has('KeyW') || keys.has('ArrowUp');
    input.back = keys.has('KeyS') || keys.has('ArrowDown');
    input.left = keys.has('KeyA') || keys.has('ArrowLeft');
    input.right = keys.has('KeyD') || keys.has('ArrowRight');
    input.run = keys.has('ShiftLeft') || keys.has('ShiftRight');
    input.jump = keys.has('Space');
  }
  document.addEventListener('mousemove', e => {
    if (document.pointerLockElement === renderer.domElement) {
      cameraYaw -= e.movementX * 0.0025;
      cameraPitch = THREE.MathUtils.clamp(cameraPitch + e.movementY * 0.0018, -0.05, 0.8);
    }
  });
  renderer.domElement.addEventListener('wheel', e => { cameraDistance = THREE.MathUtils.clamp(cameraDistance + e.deltaY * 0.008, 5, 22); });
}

function setupTouch() {
  const joy = document.getElementById('joy'), knob = document.getElementById('joyKnob');
  let joyId = null;
  joy.addEventListener('pointerdown', e => { joyId=e.pointerId; joy.setPointerCapture(joyId); moveJoy(e); });
  joy.addEventListener('pointermove', e => { if(e.pointerId===joyId) moveJoy(e); });
  const resetJoy = e => { if(e.pointerId===joyId){joyId=null;input.forward=input.back=input.left=input.right=false;knob.style.transform='translate(-50%,-50%)';} };
  joy.addEventListener('pointerup', resetJoy); joy.addEventListener('pointercancel', resetJoy);
  function moveJoy(e) {
    const r=joy.getBoundingClientRect(), dx=e.clientX-(r.left+r.width/2), dy=e.clientY-(r.top+r.height/2);
    const len=Math.hypot(dx,dy), max=42, f=Math.min(1,max/(len||1));
    const x=dx*f,y=dy*f; knob.style.transform=`translate(calc(-50% + ${x}px),calc(-50% + ${y}px))`;
    input.left=x < -12; input.right=x > 12; input.forward=y < -12; input.back=y > 12;
    cameraYaw += (e.movementX||0)*0.003;
  }
  const hold=(id,fn)=>{const el=document.getElementById(id);el.addEventListener('pointerdown',e=>{e.preventDefault();el.setPointerCapture(e.pointerId);fn(true)});el.addEventListener('pointerup',()=>fn(false));el.addEventListener('pointercancel',()=>fn(false));};
  hold('btnJump',v=>input.jump=v); hold('btnRun',v=>input.run=v);
  document.getElementById('btnAct').addEventListener('pointerdown',e=>{e.preventDefault();interact();});
  document.getElementById('btnCam').addEventListener('pointerdown',()=>{cameraYaw+=Math.PI/4;});
}

function updateCandidate() {
  interactCandidate = null;
  if (vehicles.driven) { ui.interact('E للنزول من السيارة'); return; }
  if (currentInterior) { ui.interact('E للخروج من المبنى'); return; }
  let best=3.8;
  // عناصر العالم (أبواب ومرافق عامة)
  for (const item of world.interactables) {
    const d=player.position.distanceTo(item.pos);
    if(d<best){best=d;interactCandidate={type:'world',item};}
  }
  const car=vehicles.nearestCar(player.position,4.2);
  if(car && !currentInterior) { const d=car.group.position.distanceTo(player.position); if(d<best){best=d;interactCandidate={type:'car',car};} }
  const npc=npcs.nearest(player.position,3.4);
  if(npc && !currentInterior){const d=npc.group.position.distanceTo(player.position);if(d<best){best=d;interactCandidate={type:'npc',npc};}}
  if(interactCandidate?.type==='world') {
    const item=interactCandidate.item;
    ui.interact(typeof item.label==='function'?item.label():'تفاعل');
  } else if(interactCandidate?.type==='car') ui.interact('E لركوب السيارة');
  else if(interactCandidate?.type==='npc') ui.interact('E للتحدث مع '+(interactCandidate.npc.name||'الشخص'));
  else ui.interact(null);
}

function updateCamera(dt) {
  const target=player.position.clone().add(new THREE.Vector3(0,1.55,0));
  const yaw=cameraYaw, pitch=cameraPitch;
  const offset=new THREE.Vector3(Math.sin(yaw)*cameraDistance*Math.cos(pitch), cameraDistance*Math.sin(pitch)+2, Math.cos(yaw)*cameraDistance*Math.cos(pitch));
  const desired=target.clone().add(offset);
  camera.position.lerp(desired,Math.min(1,dt*7));
  camera.lookAt(target);
}

function animate() {
  requestAnimationFrame(animate);
  const dt=Math.min(clock.getDelta(),0.05);
  if(started) {
    if(!ui.dialogOpen() && !ui.shopOpen()) {
      if(vehicles.driven) vehicles.update(dt,input,player);
      else {
        const forward=new THREE.Vector3(Math.sin(cameraYaw),0,Math.cos(cameraYaw));
        const right=new THREE.Vector3(forward.z,0,-forward.x);
        const move=new THREE.Vector3();
        if(input.forward)move.add(forward); if(input.back)move.sub(forward); if(input.right)move.add(right); if(input.left)move.sub(right);
        if(move.lengthSq()>0) move.normalize();
        player.update(dt,move,input.run,input.jump,world);
        vehicles.update(dt,{forward:false,back:false,left:false,right:false},player);
      }
      world.update(dt); npcs.update(dt,world); missions.update(dt);
      updateCandidate();
      if(vehicles.driven) audio.engine(true,vehicles.driven.speed); else audio.engine(false);
      if(!vehicles.driven && player.speed>1.5 && performance.now()-lastStep>380){audio.step();lastStep=performance.now();}
      ui.hunger=Math.max(0,ui.hunger-dt*0.18);
      if(ui.hunger===0 && Math.random()<dt*0.2) ui.hp=Math.max(1,ui.hp-1);
      ui.updateStats();
      updateCamera(dt);
      ui.drawMinimap(player.position, missions.marker.visible?[{x:missions.target.x,z:missions.target.z}]:[], CITY.size);
    }
    fpsFrames++; fpsTime+=dt;
    if(fpsTime>0.5){document.getElementById('fps').textContent=`MFA Engine • ${Math.round(fpsFrames/fpsTime)} FPS • ${world.detailPointCount.toLocaleString('en-US')} نقاط`;fpsFrames=0;fpsTime=0;}
  }
  renderer.render(scene,camera);
}

function onResize(){if(!renderer)return;camera.aspect=innerWidth/innerHeight;camera.updateProjectionMatrix();renderer.setSize(innerWidth,innerHeight);}

init();
