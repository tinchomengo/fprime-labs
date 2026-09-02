# Running the Detumble SIL demo

The full system is three pieces running at once, each in its own terminal:
`DetumbleSim` (F´) → GDS → Python bridge → static file server → browser (three.js).

## 1. Start F´ + the ground system

```bash
cd fprime-labs/detumble-sil/DetumbleSil/TelemetryDeployment
source ../../fprime-venv/bin/activate
fprime-gds --no-zmq
```

`--no-zmq` is required — the bridge script connects over plain TCP, not ZeroMQ.

## 2. Start the telemetry-to-WebSocket bridge

In a new terminal:

```bash
cd fprime-labs/detumble-sil
source fprime-venv/bin/activate
python3 bridge/telemetry_bridge.py --dictionary build-artifacts/Darwin/DetumbleSil_TelemetryDeployment/dict/TelemetryDeploymentTopologyDictionary.json
```

Wait for `[bridge] WebSocket server listening on ws://127.0.0.1:8765` before continuing.

## 3. Serve the three.js page

In a new terminal:

```bash
cd fprime-labs/detumble-sil/visualization
python3 -m http.server 8000
```

## 4. View it

Open **http://localhost:8000/** in a browser. The body starts already
tumbling (the deployment kicks it off on construction) - if it's already
settled to STABLE by the time the page connects, click **Re-kick** to
re-trigger the sequence for filming.

---

## Stopping everything

Press `Ctrl-C` in each of the three terminals (order doesn't matter). Closing
the browser tab alone is not enough — the bridge and `fprime-gds` keep running
until stopped explicitly.

## If you changed F´ source code (`DetumbleSim`, topology, etc.)

Rebuild before restarting `fprime-gds`:

```bash
cd fprime-labs/detumble-sil
fprime-util build
```
