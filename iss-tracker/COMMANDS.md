# Running IssTracker

## Simulated pass test

Replays a real ISS pass (TLE from 2026-10-11) for a virtual observer placed
so the pass peaks at 70° due north and stays within the mount's ±90°
azimuth range. It runs through the same components as real tracking; only
the observer location and the clock are simulated. The result is the same
pass every time.

### 1. Build and compile the sequences

```bash
cd iss-tracker
source fprime-venv/bin/activate
fprime-util build

cd IssTracker/TrackerDeployment
fprime-seqgen --dictionary ../../build-artifacts/Darwin/IssTracker_TrackerDeployment/dict/TrackerDeploymentTopologyDictionary.json sequences/sim_pass_10x.seq sequences/sim_pass_10x.bin
fprime-seqgen --dictionary ../../build-artifacts/Darwin/IssTracker_TrackerDeployment/dict/TrackerDeploymentTopologyDictionary.json sequences/sim_pass_1x.seq sequences/sim_pass_1x.bin
```

Compile them again after any change to the F´ commands: the compiled files
refer to command opcodes.

### 2. Prepare the mount

With everything off:

1. Point the stick at the direction that will be azimuth 0. The pass uses
   -88° to +88° around it; check the servo cable has slack both ways.
2. Close any `screen` session on the Pico's port.
3. Plug in USB only. The Pico must be running `firmware/pico-tracker`.

### 3. Start the deployment

```bash
cd iss-tracker/IssTracker/TrackerDeployment
source ../../fprime-venv/bin/activate
fprime-gds
```

The terminal prints `[PicoLink] Opened /dev/cu.usbmodem…`. Run `fprime-gds`
from this folder: the sequence paths below are relative to it.

Open http://127.0.0.1:5000. If the page says "Access denied", macOS AirPlay
Receiver is using port 5000: turn it off in System Settings, General,
AirDrop & Handoff.

Then power on 12 V, then 5 V. The stick goes vertical.

### 4. Run the test

Send `TrackerDeployment.cmdSeq.CS_RUN` with:

| Argument | Value |
|---|---|
| `fileName` | `sequences/sim_pass_10x.bin` (about 1.5 min) or `sequences/sim_pass_1x.bin` (about 13 min) |
| `block` | `NO_BLOCK` |

Expected:

- `PassPredicted`: AOS 17:41:19, TCA 17:46:50 at el 70.0, LOS 17:52:20 (simulated UTC).
- The mount turns to about -88° with the stick horizontal and waits.
- `PassStarted`: the stick climbs, crosses azimuth 0 at about 70° and goes down to about +88°.
- `PassComplete`: the mount stows (az 0, el 90).
- At the end of the sequence the simulated clock stops and the stepper is disabled.

Compare `antennaController.TargetAzimuth` / `TargetElevation` (commanded)
with `picoLink.Azimuth` / `Elevation` (reported by the Pico) in the
Channels tab.

The test leaves the virtual observer and the test TLE loaded. Restart the
deployment to start from the saved settings.

### Stopping

Any of these:

- `TrackerDeployment.cmdSeq.CS_CANCEL`, then `TrackerDeployment.antennaController.TRACK_STOP`
- `TrackerDeployment.picoLink.MOUNT_DISABLE`
- Ctrl-C on `fprime-gds`: the Pico stows and disables itself 3 s later
- Switch off the 12 V

Power off: 5 V, then 12 V, then USB.

## Commands

| Component | Command | Purpose |
|---|---|---|
| `sgp4Propagator` | `LOAD_TLE` | Load a TLE (two 69-character lines) |
| `sgp4Propagator` | `SIM_CLOCK_START` / `SIM_CLOCK_STOP` | Replay a chosen UTC time at 1x to 100x / return to the real clock |
| `sgp4Propagator` | `PREDICT_PASS` | Report the next pass |
| `sgp4Propagator` | `OBSERVER_*_PRM_SET` | Observer latitude, longitude and altitude |
| `antennaController` | `TRACK_START` / `TRACK_STOP` | Track the next pass / stop and stow |
| `picoLink` | `MOUNT_ENABLE`, `MOUNT_DISABLE`, `MOUNT_STOW`, `MOUNT_ZERO_AZ`, `MOUNT_SET` | Operate the mount by hand |
