# Running the IMU / Altitude Visualization

The full system is four pieces running at once, each in its own terminal:
`ImuSim` (F´) → GDS → Python bridge → static file server → browser (three.js).

## 1. Start F´ + the ground system

```bash
cd fprime-labs/imu-altitude-visualization/ImuAltitudeVisualization/TelemetryDeployment
source ../../fprime-venv/bin/activate
fprime-gds --no-zmq
```

`--no-zmq` is required — the bridge script connects over plain TCP, not ZeroMQ.

## 2. Start the telemetry-to-WebSocket bridge

In a new terminal:

```bash
cd fprime-labs/imu-altitude-visualization
source fprime-venv/bin/activate
python3 bridge/telemetry_bridge.py --dictionary build-artifacts/Darwin/ImuAltitudeVisualization_TelemetryDeployment/dict/TelemetryDeploymentTopologyDictionary.json
```

Wait for `[bridge] connected to F' GDS at 127.0.0.1:50050` before continuing.

## 3. Serve the three.js page

In a new terminal:

```bash
cd /fprime-labs/imu-altitude-visualization/visualization
python3 -m http.server 8000
```

## 4. View it

Open **http://localhost:8000/** in a browser.

---

## Stopping everything

Press `Ctrl-C` in each of the three terminals (order doesn't matter). Closing
the browser tab alone is not enough — the bridge and `fprime-gds` keep running
until stopped explicitly.

## If you changed F´ source code (`ImuSim`, topology, etc.)

Rebuild before restarting `fprime-gds`:

```bash
cd /fprime-labs/imu-altitude-visualization
fprime-util build
```
