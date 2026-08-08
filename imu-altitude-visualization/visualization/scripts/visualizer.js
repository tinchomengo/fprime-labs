import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { STLLoader } from "three/addons/loaders/STLLoader.js";

import {
  CAMERA_FOV,
  CAMERA_NEAR,
  CAMERA_FAR,
  CAMERA_POSITION,
  AMBIENT_LIGHT_INTENSITY,
  SUN_LIGHT_INTENSITY,
  SUN_LIGHT_POSITION,
  GRID_SIZE,
  GRID_DIVISIONS,
  GRID_Y_POSITION,
  MODEL_PATH,
  CRAFT_BODY_COLOR,
  MODEL_TARGET_SIZE,
  MODEL_ROTATION_CORRECTIONS_DEG,
  INITIAL_TELEMETRY,
  ALTITUDE_BASELINE_M,
  ALTITUDE_SCENE_SCALE,
  SMOOTHING_RATE,
  BRIDGE_WS_URL,
  WS_RECONNECT_DELAY_MS,
} from "../constants.js";

// ----------------------------------------------------------------------
// Scene setup
// ----------------------------------------------------------------------

const sceneContainer = document.getElementById("scene");
const sceneBg = getComputedStyle(document.documentElement)
  .getPropertyValue("--scene-bg")
  .trim();
const gridColor = getComputedStyle(document.documentElement)
  .getPropertyValue("--grid-color")
  .trim();

const scene = new THREE.Scene();
scene.background = new THREE.Color(sceneBg);

const camera = new THREE.PerspectiveCamera(
  CAMERA_FOV,
  window.innerWidth / window.innerHeight,
  CAMERA_NEAR,
  CAMERA_FAR,
);
camera.position.set(CAMERA_POSITION.x, CAMERA_POSITION.y, CAMERA_POSITION.z);

const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.setPixelRatio(window.devicePixelRatio);
sceneContainer.appendChild(renderer.domElement);

const controls = new OrbitControls(camera, renderer.domElement);
controls.target.set(0, 0, 0);
controls.enableDamping = true;

window.addEventListener("resize", () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});

// Lighting
scene.add(new THREE.AmbientLight(0xffffff, AMBIENT_LIGHT_INTENSITY));
const sun = new THREE.DirectionalLight(0xffffff, SUN_LIGHT_INTENSITY);
sun.position.set(
  SUN_LIGHT_POSITION.x,
  SUN_LIGHT_POSITION.y,
  SUN_LIGHT_POSITION.z,
);
scene.add(sun);

// Reference grid, standing in for the ground
const grid = new THREE.GridHelper(
  GRID_SIZE,
  GRID_DIVISIONS,
  gridColor,
  gridColor,
);
grid.position.y = GRID_Y_POSITION;
scene.add(grid);

// ----------------------------------------------------------------------
// Craft representation: loaded from an STL model. `craft` is the group
// that live telemetry (roll/pitch/yaw/altitude) rotates and repositions
// every frame (the STL mesh is added as its child once loaded)
// ----------------------------------------------------------------------

const craft = new THREE.Group();

const bodyMaterial = new THREE.MeshStandardMaterial({
  color: CRAFT_BODY_COLOR,
});

new STLLoader().load(
  MODEL_PATH,
  (geometry) => {
    geometry.computeBoundingBox();
    const size = new THREE.Vector3();
    geometry.boundingBox.getSize(size);
    const center = new THREE.Vector3();
    geometry.boundingBox.getCenter(center);
    geometry.translate(-center.x, -center.y, -center.z);

    const scale = MODEL_TARGET_SIZE / Math.max(size.x, size.y, size.z);

    const mesh = new THREE.Mesh(geometry, bodyMaterial);
    mesh.scale.setScalar(scale);
    for (const { axis, deg } of MODEL_ROTATION_CORRECTIONS_DEG) {
      mesh[`rotate${axis.toUpperCase()}`](THREE.MathUtils.degToRad(deg));
    }
    craft.add(mesh);
  },
  undefined,
  (error) => console.error("Failed to load craft model:", error),
);

scene.add(craft);

// ----------------------------------------------------------------------
// Telemetry state (must be declared before animate() runs, since its
// first call happens synchronously below)
//
// F' only sends a new reading once per second (rateGroup1 @ 1Hz), which
// looks like an abrupt jump if applied directly. So `target` holds the
// latest raw reading (also what the HUD numbers show), and `display`
// eases toward it every frame for smooth on-screen motion.
// ----------------------------------------------------------------------

const target = { ...INITIAL_TELEMETRY };
const display = { ...INITIAL_TELEMETRY };

const clock = new THREE.Clock();

// Shortest-path angular lerp, so yaw doesn't spin the long way around when
// crossing the 0/360 wrap-around point.
function lerpAngleDeg(current, target, t) {
  const delta = ((target - current + 540) % 360) - 180;
  return current + delta * t;
}

function applyTelemetry() {
  const t = 1 - Math.exp(-SMOOTHING_RATE * clock.getDelta());

  display.roll = THREE.MathUtils.lerp(display.roll, target.roll, t);
  display.pitch = THREE.MathUtils.lerp(display.pitch, target.pitch, t);
  display.yaw = lerpAngleDeg(display.yaw, target.yaw, t);
  display.altitude = THREE.MathUtils.lerp(display.altitude, target.altitude, t);

  craft.rotation.set(
    THREE.MathUtils.degToRad(display.pitch),
    THREE.MathUtils.degToRad(display.yaw),
    THREE.MathUtils.degToRad(display.roll),
  );
  craft.position.y =
    (display.altitude - ALTITUDE_BASELINE_M) * ALTITUDE_SCENE_SCALE;
}

function animate() {
  requestAnimationFrame(animate);
  applyTelemetry();
  controls.update();
  renderer.render(scene, camera);
}
animate();

// ----------------------------------------------------------------------
// HUD + WebSocket connection to the telemetry bridge
// ----------------------------------------------------------------------

const statusEl = document.getElementById("status");
const statusTextEl = document.getElementById("status-text");
const valueEls = {
  roll: document.getElementById("val-roll"),
  pitch: document.getElementById("val-pitch"),
  yaw: document.getElementById("val-yaw"),
  altitude: document.getElementById("val-altitude"),
};

function setConnected(connected) {
  statusEl.classList.toggle("connected", connected);
  statusTextEl.textContent = connected ? "Connected" : "Disconnected";
}

function connect() {
  const ws = new WebSocket(BRIDGE_WS_URL);

  ws.onopen = () => setConnected(true);
  ws.onclose = () => {
    setConnected(false);
    setTimeout(connect, WS_RECONNECT_DELAY_MS); // keep retrying so the page recovers if the bridge restarts
  };
  ws.onerror = () => ws.close();

  ws.onmessage = (event) => {
    const { channel, value } = JSON.parse(event.data);
    if (!(channel in target)) return;
    target[channel] = value;
    if (valueEls[channel]) {
      valueEls[channel].textContent = value.toFixed(1);
    }
  };
}
connect();
