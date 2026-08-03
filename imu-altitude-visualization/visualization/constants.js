// Scene / camera

export const CAMERA_FOV = 50;
export const CAMERA_NEAR = 0.1;
export const CAMERA_FAR = 100;
export const CAMERA_POSITION = { x: 6, y: 4, z: 8 };

export const AMBIENT_LIGHT_INTENSITY = 0.6;
export const SUN_LIGHT_INTENSITY = 1.0;
export const SUN_LIGHT_POSITION = { x: 5, y: 8, z: 3 };

// Reference grid (the ground)

export const GRID_SIZE = 20;
export const GRID_DIVISIONS = 20;
export const GRID_Y_POSITION = -2;

// 3D model

export const MODEL_PATH = "models/F22jet.stl";
export const CRAFT_BODY_COLOR = 0x2a78d6;

// STL files carry no notion of real-world units
// Model is auto-scaled to this longest-dimension size and auto-centered on its
// own bounding-box center, regardless of how the source file was modeled
export const MODEL_TARGET_SIZE = 2.4;

// STL files also carry no notion of which way is "up" or "forward"
// This axis correction, points the nose along -Z with the canopy up.
export const MODEL_ROTATION_CORRECTIONS_DEG = [{ axis: "x", deg: -90 }];

// Telemetry / animation

export const INITIAL_TELEMETRY = { roll: 0, pitch: 0, yaw: 0, altitude: 100 };
export const ALTITUDE_BASELINE_M = 100;
export const ALTITUDE_SCENE_SCALE = 0.1; // 1 scene unit per 10m of altitude deviation
export const SMOOTHING_RATE = 3;

// Bridge WebSocket connection

export const BRIDGE_WS_URL = "ws://127.0.0.1:8765";
export const WS_RECONNECT_DELAY_MS = 1000;
