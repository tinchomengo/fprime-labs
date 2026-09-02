// Scene / camera

export const CAMERA_FOV = 50;
export const CAMERA_NEAR = 0.01;
export const CAMERA_FAR = 100;
export const CAMERA_POSITION = { x: 3.2, y: 1.8, z: 3.6 };

// Floor on how close the camera can get to the (moving) target. Earth's
// texture is one static equirectangular image, not zoom-level tiles like a
// real map/globe service - get close enough and its real resolution limit
// becomes visible as blockiness no matter how high-res the source is. This
// doesn't fully solve that (the craft orbits only ~8% of Earth's radius
// above the surface, so some camera angles can still graze close terrain),
// but keeps casual zooming from immediately hitting it.
export const MIN_CAMERA_DISTANCE = 0.4;

export const AMBIENT_LIGHT_INTENSITY = 0.15;
export const SUN_LIGHT_INTENSITY = 1.8;
export const SUN_LIGHT_POSITION = { x: 5, y: 8, z: 3 };

export const STAR_COUNT = 1200;
export const STAR_FIELD_RADIUS = 40;

// Orbit / Earth scale. Real proportions are used throughout: Earth's real
// radius and the orbit's real radius (from OrbitPropagator.cpp - Earth
// radius + a 500km LEO altitude) map to the SAME km-to-scene-unit factor,
// so the (thin!) real margin between the surface and a LEO orbit is
// preserved rather than exaggerated for effect.
export const EARTH_RADIUS_KM = 6371.0;
export const KM_TO_SCENE_UNITS = 2.0 / EARTH_RADIUS_KM; // Earth renders at 2 scene units radius
export const EARTH_SEGMENTS = 96;

// Earth textures - see visualization/models/earth/NOTICE for attribution.
// Day map is real NASA Blue Marble imagery at 5400x2700 (fetched directly
// from NASA, not a resized copy); normal/specular/clouds are 2048px, the
// highest resolution readily available for those three specifically.
export const EARTH_MAP_PATH = "models/earth/earth_daymap_5400.jpg";
export const EARTH_SPECULAR_MAP_PATH = "models/earth/earth_specular_2048.jpg";
export const EARTH_NORMAL_MAP_PATH = "models/earth/earth_normal_2048.jpg";
export const EARTH_CLOUDS_MAP_PATH = "models/earth/earth_clouds_2048.png";
export const CLOUD_LAYER_ALTITUDE_KM = 15; // clouds sit a bit above the surface, to scale
export const CLOUD_ROTATION_RATE_RAD_S = 0.00006; // slow independent drift, not tied to telemetry

// Atmosphere rim glow - a thin shell just outside the cloud layer, lit up
// only near the limb (a Fresnel-style falloff: brightest where the surface
// normal is near-perpendicular to the view direction, invisible looking
// straight at the surface). Purely a lighting/visual effect, not telemetry-driven.
export const ATMOSPHERE_ALTITUDE_KM = 120;
export const ATMOSPHERE_COLOR = { r: 0.3, g: 0.6, b: 1.0 }; // a plain object, not a THREE.Color - this file stays framework-agnostic

export const ORBIT_PATH_COLOR = 0x4a6fa5;
export const ORBIT_PATH_SEGMENTS = 256;

// Craft model (real CubeSat STL - see visualization/models/NOTICE for
// attribution). Its rendered size is deliberately NOT to the same scale as
// Earth/the orbit above - to true scale here it would be many orders of
// magnitude smaller than a pixel. Position (from OrbitPropagator) IS to
// real scale; only the craft mesh itself is exaggerated for visibility.
export const MODEL_PATH = "models/ArduSat.stl";
export const CRAFT_BODY_COLOR = 0x8a8f98;
// Was 0.3 (7.5% of Earth's rendered diameter - way too big); still
// exaggerated (a real 3U CubeSat is ~0.3m, meaningless at this scale) but
// now a much more plausible-looking fraction of Earth's diameter (~0.75%).
export const MODEL_TARGET_SIZE = 0.03;
export const MODEL_ROTATION_CORRECTIONS_DEG = [];

// Body-frame reference axes drawn from the craft (X/Y/Z, red/green/blue) -
// sized relative to the craft model, not to Earth, since they're an
// attitude reference for close-in viewing. Was 3x the model's own size
// (dwarfed it); now about the same length as the craft itself.
export const AXIS_INDICATOR_LENGTH = MODEL_TARGET_SIZE * 1.0;

// Velocity-direction vector. Was a fixed 0.35 (reasoned it should read at
// orbit scale against Earth) - still looked huge, because what it's
// actually seen next to is the (tiny) craft, not Earth. Tied to the craft's
// own scale instead, just long enough to read as visually distinct from
// the body axes.
export const VELOCITY_VECTOR_LENGTH = AXIS_INDICATOR_LENGTH * 2.5;

// Telemetry / animation

export const INITIAL_TELEMETRY = {
  qw: 1,
  qx: 0,
  qy: 0,
  qz: 0,
  omegaMagnitude: 0,
  controlTorqueMagnitude: 0,
  kineticEnergy: 0,
  mode: "TUMBLING",
  positionX: EARTH_RADIUS_KM + 500,
  positionY: 0,
  positionZ: 0,
  altitude: 500,
  orbitAngle: 0,
  orbitalPeriodS: 0,
  orbitalVelocity: 0,
  orbitCount: 0,
};

// Attitude is SLERPed toward each new sample rather than snapped, since the
// F' side only publishes once per rate-group cycle (10Hz - see the
// deployment's Main.cpp) while the page renders at 60fps. Position (much
// slower-changing per cycle, at this orbit's angular rate) gets a plain lerp.
export const ATTITUDE_SLERP_RATE = 14;
export const POSITION_LERP_RATE = 14;
// Other scalar HUD readouts (omega magnitude, torque, etc.) ease at their own rate.
export const SCALAR_SMOOTHING_RATE = 10;

// Live analytics charts (see scripts/chart.js) - each keeps a rolling
// window of recent samples pulled straight from F' telemetry. Plain single
// color for both - the panel is a data readout, not a branded UI.
export const CHART_WINDOW_S = 20;
export const CHART_LINE_COLOR = "#9a9a9a";

// Bridge WebSocket connection

export const BRIDGE_WS_URL = "ws://127.0.0.1:8765";
export const WS_RECONNECT_DELAY_MS = 1000;
