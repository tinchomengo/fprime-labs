# F´ Labs

Repository ("Lab") for Flight Software and Embedded Systems projects.

The foundation for the projects within this repository is the Official NASA F´ Software (https://github.com/nasa/fprime).

The intention here is to create amazing projects anyone in the Aerospace Industry can use, based on reliable standards and useful ideas.

## Projects

- **[getting-started/](getting-started/)** — A vanilla F´ project (`FprimeGettingstarted`) scaffolded from the F´ bootstrap tool, used to learn the framework. Contains a single example `HiComponent` and a `FirstDeployment` topology.

- **[attitude-control/](attitude-control/)** — Closed-loop attitude stabilization on a Raspberry Pi Pico. `firmware/pico-attitude-control` reads roll/pitch/yaw from an MPU6050 (I2C) and drives a servo (PWM) to counteract deviation from level, entirely standalone on the Pico for a tight control loop. The F´ side (`AttitudeControl`, a `TelemetryDeployment`) is a supervisory layer for monitoring status over USB serial — it's not in the firmware's hot path, and its components are still being built out.

- **[imu-altitude-visualization/](imu-altitude-visualization/)** — End-to-end IMU telemetry pipeline with live 3D visualization. An `ImuReader`/`ImuSim` F´ component (`ImuAltitudeVisualization` deployment) ingests IMU data (from Pico firmware in `firmware/pico-imu`, or a simulator), a Python `bridge/telemetry_bridge.py` relays telemetry from F´ GDS to a WebSocket, and a three.js page in `visualization/` renders it live in the browser. See [COMMANDS.md](imu-altitude-visualization/COMMANDS.md) for how to run the full stack.
