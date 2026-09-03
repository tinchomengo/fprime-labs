# F´ Labs

Repository ("Lab") for Flight Software and Embedded Systems projects.

The foundation for the projects within this repository is the Official NASA F´ Software (https://github.com/nasa/fprime).

The intention here is to create amazing projects anyone in the Aerospace Industry can use, based on reliable standards and useful ideas.

Each project is independent from each other, meaning that there's no 'inter-project' dependencies to run each of them.

## Projects

- **[getting-started/](getting-started/)** The official F´ 'Hello World' project (`FprimeGettingstarted`) scaffolded from the F´ bootstrap tool, used to learn the framework. Contains a single example `HiComponent` and a `FirstDeployment` topology.

- **[attitude-control/](attitude-control/)** Attitude stabilization on a Raspberry Pi Pico. `firmware/pico-attitude-control` reads roll/pitch/yaw from an MPU6050 (I2C) and drives a servo (PWM) to counteract deviation from level, entirely standalone on the Pico for a tight control loop. The F´ side (`AttitudeControl`, a `TelemetryDeployment`) is a supervisory layer for monitoring status over USB serial — it's not in the firmware's hot path, and its components are still being built out.

- **[imu-altitude-visualization/](imu-altitude-visualization/)** IMU telemetry pipeline with live 3D visualization. An `ImuReader`/`ImuSim` F´ component (`ImuAltitudeVisualization` deployment) ingests IMU data (from Pico firmware in `firmware/pico-imu`, or a simulator), a Python `bridge/telemetry_bridge.py` relays telemetry from F´ GDS to a WebSocket, and a three.js page in `visualization/` renders it live in the browser.

  <video src="https://raw.githubusercontent.com/tinchomengo/fprime-labs/main/imu-altitude-visualization/media/imu-altitude-visualization.mp4" controls width="700"></video>

- **[detumble-sil/](detumble-sil/)** ADCS demo, entirely in F´, no hardware. `DetumbleSim` simulates a tumbling rigid body under a rate-damping control torque, closing the loop every cycle (`KICK` re-tumbles it on demand); `OrbitPropagator` independently propagates a circular Kepler orbit, radius/period derived at runtime. The same telemetry-bridge + three.js pattern as `imu-altitude-visualization` renders a real CubeSat STL live in the browser, orbiting a to-scale Earth, with rolling analytics charts.

  <video src="https://raw.githubusercontent.com/tinchomengo/fprime-labs/main/detumble-sil/media/detumble-sil.mp4" controls width="700"></video>
