import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { STLLoader } from "three/addons/loaders/STLLoader.js";

import { TimeSeriesChart } from "./chart.js";

import {
  CAMERA_FOV,
  CAMERA_NEAR,
  CAMERA_FAR,
  CAMERA_POSITION,
  MIN_CAMERA_DISTANCE,
  AMBIENT_LIGHT_INTENSITY,
  SUN_LIGHT_INTENSITY,
  SUN_LIGHT_POSITION,
  STAR_COUNT,
  STAR_FIELD_RADIUS,
  EARTH_RADIUS_KM,
  KM_TO_SCENE_UNITS,
  EARTH_SEGMENTS,
  EARTH_MAP_PATH,
  EARTH_SPECULAR_MAP_PATH,
  EARTH_NORMAL_MAP_PATH,
  EARTH_CLOUDS_MAP_PATH,
  CLOUD_LAYER_ALTITUDE_KM,
  CLOUD_ROTATION_RATE_RAD_S,
  ATMOSPHERE_ALTITUDE_KM,
  ATMOSPHERE_COLOR,
  ORBIT_PATH_COLOR,
  ORBIT_PATH_SEGMENTS,
  MODEL_PATH,
  CRAFT_BODY_COLOR,
  MODEL_TARGET_SIZE,
  MODEL_ROTATION_CORRECTIONS_DEG,
  AXIS_INDICATOR_LENGTH,
  VELOCITY_VECTOR_LENGTH,
  INITIAL_TELEMETRY,
  ATTITUDE_SLERP_RATE,
  POSITION_LERP_RATE,
  SCALAR_SMOOTHING_RATE,
  CHART_WINDOW_S,
  CHART_LINE_COLOR,
  BRIDGE_WS_URL,
  WS_RECONNECT_DELAY_MS,
} from "../constants.js";

// ----------------------------------------------------------------------
// Scene setup
// ----------------------------------------------------------------------

const sceneContainer = document.getElementById("scene");

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x000000);

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

// Follows the craft rather than staying fixed on Earth's center (set each
// frame in applyTelemetry(), below) - lets you orbit the camera around the
// craft itself, which is what makes tracking it comfortable regardless of
// orbital speed.
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;
controls.minDistance = MIN_CAMERA_DISTANCE;

window.addEventListener("resize", () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});

// Lighting - low ambient/high directional, so the sunlit-vs-night side of
// Earth actually reads, closer to how it really looks from orbit.
scene.add(new THREE.AmbientLight(0xffffff, AMBIENT_LIGHT_INTENSITY));
const sun = new THREE.DirectionalLight(0xffffff, SUN_LIGHT_INTENSITY);
sun.position.set(
  SUN_LIGHT_POSITION.x,
  SUN_LIGHT_POSITION.y,
  SUN_LIGHT_POSITION.z,
);
scene.add(sun);

// Starfield backdrop
{
  const positions = new Float32Array(STAR_COUNT * 3);
  for (let i = 0; i < STAR_COUNT; i++) {
    // Random point on a sphere shell, so stars stay a fixed backdrop
    // distance away regardless of camera orbit.
    const theta = Math.random() * Math.PI * 2;
    const phi = Math.acos(2 * Math.random() - 1);
    const r = STAR_FIELD_RADIUS * (0.6 + 0.4 * Math.random());
    positions[i * 3] = r * Math.sin(phi) * Math.cos(theta);
    positions[i * 3 + 1] = r * Math.sin(phi) * Math.sin(theta);
    positions[i * 3 + 2] = r * Math.cos(phi);
  }
  const starGeometry = new THREE.BufferGeometry();
  starGeometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
  const starMaterial = new THREE.PointsMaterial({ color: 0xffffff, size: 0.05, sizeAttenuation: true });
  scene.add(new THREE.Points(starGeometry, starMaterial));
}

// ----------------------------------------------------------------------
// Earth - textured (color/specular/normal maps, see models/earth/NOTICE
// for attribution) rather than a flat color, plus an independently-rotating
// cloud layer. Radius uses the SAME km-to-scene-unit scale as the orbit
// path/craft position below, so the real (thin) margin between the
// surface and a 500km LEO orbit is preserved rather than exaggerated.
// ----------------------------------------------------------------------

const textureLoader = new THREE.TextureLoader();

const earthMap = textureLoader.load(EARTH_MAP_PATH);
earthMap.colorSpace = THREE.SRGBColorSpace;

const earthRadiusScene = EARTH_RADIUS_KM * KM_TO_SCENE_UNITS;
const earth = new THREE.Mesh(
  new THREE.SphereGeometry(earthRadiusScene, EARTH_SEGMENTS, EARTH_SEGMENTS),
  new THREE.MeshPhongMaterial({
    map: earthMap,
    specularMap: textureLoader.load(EARTH_SPECULAR_MAP_PATH),
    normalMap: textureLoader.load(EARTH_NORMAL_MAP_PATH),
    normalScale: new THREE.Vector2(0.6, 0.6),
    shininess: 8,
  }),
);
scene.add(earth);

const cloudsRadiusScene = (EARTH_RADIUS_KM + CLOUD_LAYER_ALTITUDE_KM) * KM_TO_SCENE_UNITS;
const clouds = new THREE.Mesh(
  new THREE.SphereGeometry(cloudsRadiusScene, EARTH_SEGMENTS, EARTH_SEGMENTS),
  new THREE.MeshLambertMaterial({
    map: textureLoader.load(EARTH_CLOUDS_MAP_PATH),
    transparent: true,
  }),
);
scene.add(clouds);

// Atmosphere rim glow - a thin shell just outside the clouds, lit only near
// the limb via a Fresnel-style falloff (brightest where the surface faces
// edge-on to the camera, invisible looking straight at the surface - the
// same reason a real atmosphere glows brightest at the horizon). Rendered
// back-face-only with additive blending so it reads as a soft halo rather
// than an opaque shell.
const atmosphereRadiusScene = (EARTH_RADIUS_KM + ATMOSPHERE_ALTITUDE_KM) * KM_TO_SCENE_UNITS;
const atmosphere = new THREE.Mesh(
  new THREE.SphereGeometry(atmosphereRadiusScene, EARTH_SEGMENTS, EARTH_SEGMENTS),
  new THREE.ShaderMaterial({
    uniforms: {
      glowColor: { value: new THREE.Vector3(ATMOSPHERE_COLOR.r, ATMOSPHERE_COLOR.g, ATMOSPHERE_COLOR.b) },
    },
    vertexShader: `
      varying vec3 vViewNormal;
      void main() {
        vViewNormal = normalize(normalMatrix * normal);
        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
      }
    `,
    fragmentShader: `
      varying vec3 vViewNormal;
      uniform vec3 glowColor;
      void main() {
        // Camera looks down -Z in view space, so a normal near (0,0,1) faces
        // the camera (limb-facing surface points), scaled up sharply so only
        // a thin rim near the edge is bright.
        float rim = pow(1.0 - abs(vViewNormal.z), 3.0);
        gl_FragColor = vec4(glowColor, rim);
      }
    `,
    side: THREE.BackSide,
    blending: THREE.AdditiveBlending,
    transparent: true,
    depthWrite: false,
  }),
);
scene.add(atmosphere);

// ----------------------------------------------------------------------
// Craft representation - the real CubeSat STL model (see models/NOTICE for
// attribution). `craft` is what live telemetry rotates AND translates
// every frame: quaternion from DetumbleSim, position from OrbitPropagator.
// Its rendered SIZE is exaggerated for visibility (see constants.js);
// its position is not.
// ----------------------------------------------------------------------

const craft = new THREE.Group();

const bodyMaterial = new THREE.MeshStandardMaterial({ color: CRAFT_BODY_COLOR, metalness: 0.6, roughness: 0.4 });

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

// Body-frame axis indicators (X=red, Y=green, Z=blue - the usual
// convention) - added as children of `craft`, so they rotate with it for
// free rather than needing their own quaternion updates. Purely a
// reference overlay for reading attitude at a glance, not telemetry.
for (const [dir, color] of [
  [new THREE.Vector3(1, 0, 0), 0xe6483c],
  [new THREE.Vector3(0, 1, 0), 0x4cae4c],
  [new THREE.Vector3(0, 0, 1), 0x3c78e6],
]) {
  craft.add(
    new THREE.ArrowHelper(
      dir,
      new THREE.Vector3(0, 0, 0),
      AXIS_INDICATOR_LENGTH,
      color,
      AXIS_INDICATOR_LENGTH * 0.3,
      AXIS_INDICATOR_LENGTH * 0.2,
    ),
  );
}

scene.add(craft);

// ----------------------------------------------------------------------
// Orbit path - a static ring showing the whole trajectory (not just the
// craft's current position), matching OrbitPropagator.cpp's circular,
// inclined-only orbit: radius = ORBIT_RADIUS_KM, tilted by INCLINATION_DEG
// about the X axis. Traced directly from the same math as the F' side
// (not fetched from telemetry - the path itself is fixed for a circular
// orbit) so a change to that component's altitude/inclination should be
// mirrored here too.
// ----------------------------------------------------------------------

// Must match OrbitPropagator.cpp's ALTITUDE_KM/INCLINATION_DEG - shared
// below by both the static path line and the live velocity-vector arrow.
const ORBIT_RADIUS_KM = EARTH_RADIUS_KM + 500.0;
const ORBIT_INCLINATION_RAD = THREE.MathUtils.degToRad(51.6);

{
  const orbitRadiusScene = ORBIT_RADIUS_KM * KM_TO_SCENE_UNITS;

  const points = [];
  for (let i = 0; i <= ORBIT_PATH_SEGMENTS; i++) {
    const angle = (i / ORBIT_PATH_SEGMENTS) * Math.PI * 2;
    const xOrbital = orbitRadiusScene * Math.cos(angle);
    const yOrbital = orbitRadiusScene * Math.sin(angle);
    points.push(
      new THREE.Vector3(
        xOrbital,
        yOrbital * Math.cos(ORBIT_INCLINATION_RAD),
        yOrbital * Math.sin(ORBIT_INCLINATION_RAD),
      ),
    );
  }
  const orbitGeometry = new THREE.BufferGeometry().setFromPoints(points);
  const orbitMaterial = new THREE.LineBasicMaterial({ color: ORBIT_PATH_COLOR, transparent: true, opacity: 0.6 });
  scene.add(new THREE.LineLoop(orbitGeometry, orbitMaterial));
}

// Velocity-direction vector - not a static shape like the path above, but
// live: repositioned to the craft and re-pointed each frame in
// applyTelemetry(). Direction is derived analytically (orbital-plane
// normal cross position, both known from the same circular-orbit math as
// the path line), not fetched from telemetry - OrbitPropagator doesn't
// publish a velocity vector, only speed (OrbitalVelocity).
const orbitalPlaneNormal = new THREE.Vector3(
  0,
  -Math.sin(ORBIT_INCLINATION_RAD),
  Math.cos(ORBIT_INCLINATION_RAD),
);
const velocityArrow = new THREE.ArrowHelper(
  new THREE.Vector3(0, 1, 0),
  new THREE.Vector3(0, 0, 0),
  VELOCITY_VECTOR_LENGTH,
  0xc9cdd3,
  VELOCITY_VECTOR_LENGTH * 0.15,
  VELOCITY_VECTOR_LENGTH * 0.08,
);
scene.add(velocityArrow);

// ----------------------------------------------------------------------
// Telemetry state (must be declared before animate() runs, since its
// first call happens synchronously below)
//
// F' publishes once per rate-group cycle (10Hz - see the deployment's
// Main.cpp), which would look like discrete snapping if applied directly
// at 60fps. `targetQuat`/`targetPosition` hold the latest sample;
// `displayQuat`/`displayPosition` ease toward them every frame. Other
// scalars (omega magnitude, torque, etc.) get a plain lerp.
// ----------------------------------------------------------------------

const targetQuat = new THREE.Quaternion(
  INITIAL_TELEMETRY.qx,
  INITIAL_TELEMETRY.qy,
  INITIAL_TELEMETRY.qz,
  INITIAL_TELEMETRY.qw,
);
const displayQuat = targetQuat.clone();

function kmToScenePosition(x_km, y_km, z_km) {
  return new THREE.Vector3(x_km * KM_TO_SCENE_UNITS, y_km * KM_TO_SCENE_UNITS, z_km * KM_TO_SCENE_UNITS);
}

const targetPosition = kmToScenePosition(
  INITIAL_TELEMETRY.positionX,
  INITIAL_TELEMETRY.positionY,
  INITIAL_TELEMETRY.positionZ,
);
const displayPosition = targetPosition.clone();
const previousDisplayPosition = displayPosition.clone(); // for translating the camera+target together, below
controls.target.copy(displayPosition);

const targetScalars = {
  omegaMagnitude: INITIAL_TELEMETRY.omegaMagnitude,
  controlTorqueMagnitude: INITIAL_TELEMETRY.controlTorqueMagnitude,
  kineticEnergy: INITIAL_TELEMETRY.kineticEnergy,
  altitude: INITIAL_TELEMETRY.altitude,
  orbitAngle: INITIAL_TELEMETRY.orbitAngle,
  orbitalPeriodS: INITIAL_TELEMETRY.orbitalPeriodS,
  orbitalVelocity: INITIAL_TELEMETRY.orbitalVelocity,
  orbitCount: INITIAL_TELEMETRY.orbitCount,
};
const displayScalars = { ...targetScalars };

let mode = INITIAL_TELEMETRY.mode;

const clock = new THREE.Clock();
const startTime = performance.now() / 1000;

// ----------------------------------------------------------------------
// HUD element references - declared before applyTelemetry()/animate()
// below, since animate() calls applyTelemetry() synchronously on this
// script's first pass (not just once the render loop gets going), which
// writes to these immediately.
// ----------------------------------------------------------------------

const statusTextEl = document.getElementById("status-text");
const modeTextEl = document.getElementById("mode-text");
const kickBtn = document.getElementById("kick-btn");
const orbitSummaryEl = document.getElementById("orbit-summary");
const valueEls = {
  omega: document.getElementById("val-omega"),
  torque: document.getElementById("val-torque"),
  energy: document.getElementById("val-energy"),
  orbitAngle: document.getElementById("val-orbit-angle"),
  orbitCount: document.getElementById("val-orbit-count"),
  orbitVelocity: document.getElementById("val-orbit-velocity"),
};

// F' event log terminal - every event from every component (mode changes,
// orbit completions, command dispatch/completion, rate group health, ...),
// forwarded verbatim by the bridge's EventBridge as F's own formatted
// string. Capped to MAX_TERMINAL_LINES so the DOM doesn't grow forever
// over a long-running session.
const terminalLogEl = document.getElementById("terminal-log");
const MAX_TERMINAL_LINES = 300;

function appendTerminalLine(text) {
  const line = document.createElement("div");
  line.className = "terminal-line";
  line.textContent = text;
  terminalLogEl.appendChild(line);

  while (terminalLogEl.childElementCount > MAX_TERMINAL_LINES) {
    terminalLogEl.removeChild(terminalLogEl.firstChild);
  }
  terminalLogEl.scrollTop = terminalLogEl.scrollHeight;
}

const omegaChart = new TimeSeriesChart(document.getElementById("chart-omega"), CHART_LINE_COLOR, CHART_WINDOW_S);
const orbitAngleChart = new TimeSeriesChart(
  document.getElementById("chart-orbit-angle"),
  CHART_LINE_COLOR,
  CHART_WINDOW_S,
  { autoscale: false, yMin: 0, yMax: 360 },
);

let orbitSummaryWritten = false;

function applyTelemetry() {
  const dt = clock.getDelta();

  const attitudeT = 1 - Math.exp(-ATTITUDE_SLERP_RATE * dt);
  displayQuat.slerp(targetQuat, attitudeT);
  craft.quaternion.copy(displayQuat);

  const positionT = 1 - Math.exp(-POSITION_LERP_RATE * dt);
  displayPosition.lerp(targetPosition, positionT);
  craft.position.copy(displayPosition);

  // Translate the camera by the same delta as the target, so the camera
  // rigidly follows the craft while preserving whatever distance/angle the
  // user last set by dragging/zooming (OrbitControls' own offset-from-target
  // math would otherwise leave the camera behind as the target moves).
  const followDelta = displayPosition.clone().sub(previousDisplayPosition);
  camera.position.add(followDelta);
  controls.target.copy(displayPosition);
  previousDisplayPosition.copy(displayPosition);

  velocityArrow.position.copy(displayPosition);
  velocityArrow.setDirection(orbitalPlaneNormal.clone().cross(displayPosition).normalize());

  const scalarT = 1 - Math.exp(-SCALAR_SMOOTHING_RATE * dt);
  for (const key of Object.keys(targetScalars)) {
    displayScalars[key] = THREE.MathUtils.lerp(displayScalars[key], targetScalars[key], scalarT);
  }

  valueEls.omega.textContent = displayScalars.omegaMagnitude.toFixed(1);
  // N*m -> mN*m, easier to read at this simulation's torque scale
  valueEls.torque.textContent = (displayScalars.controlTorqueMagnitude * 1000).toFixed(1);
  valueEls.energy.textContent = displayScalars.kineticEnergy.toFixed(3);
  valueEls.orbitAngle.textContent = displayScalars.orbitAngle.toFixed(1);
  valueEls.orbitCount.textContent = displayScalars.orbitCount.toFixed(2);
  valueEls.orbitVelocity.textContent = displayScalars.orbitalVelocity.toFixed(2);

  if (!orbitSummaryWritten && targetScalars.orbitalPeriodS > 0) {
    const periodMin = targetScalars.orbitalPeriodS / 60;
    orbitSummaryEl.textContent = `${targetScalars.altitude.toFixed(0)}km circular orbit · period ${periodMin.toFixed(1)} min`;
    orbitSummaryWritten = true;
  }

  const nowS = performance.now() / 1000 - startTime;
  omegaChart.push(nowS, targetScalars.omegaMagnitude);
  orbitAngleChart.push(nowS, targetScalars.orbitAngle);
  omegaChart.draw();
  orbitAngleChart.draw();

  clouds.rotation.y += CLOUD_ROTATION_RATE_RAD_S * dt * 60; // independent slow drift, not tied to telemetry
}

function animate() {
  requestAnimationFrame(animate);
  applyTelemetry();
  controls.update();
  renderer.render(scene, camera);
}
animate();

// ----------------------------------------------------------------------
// WebSocket connection to the telemetry bridge
// ----------------------------------------------------------------------

function setConnected(connected) {
  statusTextEl.textContent = connected ? "connected" : "disconnected";
  kickBtn.disabled = !connected;
}

function setMode(newMode) {
  mode = newMode;
  modeTextEl.textContent = mode.toLowerCase();
}
setMode(mode);

let socket = null;

function connect() {
  socket = new WebSocket(BRIDGE_WS_URL);

  socket.onopen = () => setConnected(true);
  socket.onclose = () => {
    setConnected(false);
    setTimeout(connect, WS_RECONNECT_DELAY_MS); // keep retrying so the page recovers if the bridge restarts
  };
  socket.onerror = () => socket.close();

  socket.onmessage = (event) => {
    const data = JSON.parse(event.data);
    if (data.type === "event") {
      appendTerminalLine(data.text);
      return;
    }

    const { channel, value } = data;
    switch (channel) {
      case "qw":
        targetQuat.w = value;
        break;
      case "qx":
        targetQuat.x = value;
        break;
      case "qy":
        targetQuat.y = value;
        break;
      case "qz":
        targetQuat.z = value;
        break;
      case "omegaMagnitude":
        targetScalars.omegaMagnitude = value;
        break;
      case "controlTorqueMagnitude":
        targetScalars.controlTorqueMagnitude = value;
        break;
      case "kineticEnergy":
        targetScalars.kineticEnergy = value;
        break;
      case "mode":
        setMode(value);
        break;
      case "positionX":
        targetPosition.x = value * KM_TO_SCENE_UNITS;
        break;
      case "positionY":
        targetPosition.y = value * KM_TO_SCENE_UNITS;
        break;
      case "positionZ":
        targetPosition.z = value * KM_TO_SCENE_UNITS;
        break;
      case "altitude":
        targetScalars.altitude = value;
        break;
      case "orbitAngle":
        targetScalars.orbitAngle = value;
        break;
      case "orbitalPeriodS":
        targetScalars.orbitalPeriodS = value;
        break;
      case "orbitalVelocity":
        targetScalars.orbitalVelocity = value;
        break;
      case "orbitCount":
        targetScalars.orbitCount = value;
        break;
      default:
        break;
    }
  };
}
connect();

kickBtn.addEventListener("click", () => {
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send("KICK");
  }
});
