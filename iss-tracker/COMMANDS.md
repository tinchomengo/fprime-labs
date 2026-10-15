# Running IssTracker

Two ways to run the mount, through the same components:

- **Simulated pass test**: a fixed pass, replayed by a simulated clock for a
  virtual observer. The same result every time; checks the whole chain.
- **Real pass**: the current TLE, the real clock and your location.

## Build and compile the sequences

```bash
cd iss-tracker
source fprime-venv/bin/activate
fprime-util build

cd IssTracker/TrackerDeployment
DICT=../../build-artifacts/Darwin/IssTracker_TrackerDeployment/dict/TrackerDeploymentTopologyDictionary.json
fprime-seqgen --dictionary $DICT sequences/sim_pass_10x.seq sequences/sim_pass_10x.bin
fprime-seqgen --dictionary $DICT sequences/sim_pass_1x.seq sequences/sim_pass_1x.bin
fprime-seqgen --dictionary $DICT sequences/real_pass_prepare.seq sequences/real_pass_prepare.bin
```

Compile them again after any change to the F´ commands: the compiled files
refer to command opcodes.

## Start the deployment

With everything off:

1. Close any `screen` session on the Pico's port.
2. Plug in USB only. The Pico must be running `firmware/pico-tracker`.

```bash
cd iss-tracker/IssTracker/TrackerDeployment
source ../../fprime-venv/bin/activate
fprime-gds
```

The terminal prints `[PicoLink] Opened /dev/cu.usbmodem…`. Run `fprime-gds`
from this folder: the sequence and TLE paths, and the saved parameters, are
relative to it.

`picoLink.State` in the Channels tab (filter by `picoLink`) updates every
second while the Pico is talking. If a `PicoSilent` warning appears instead,
switch off 5 V and 12 V, stop `fprime-gds`, unplug and replug the Pico's USB
cable, and start again.

Open http://127.0.0.1:5000. If the page says "Access denied", macOS AirPlay
Receiver is using port 5000: turn it off in System Settings, General,
AirDrop & Handoff.

Then power on 12 V, then 5 V. The stick goes vertical.

## Simulated pass test

Replays a real ISS pass (TLE from 2026-10-11) for a virtual observer placed
so the pass peaks at 70° due north. Only the observer and the clock are
simulated.

Before powering on, point the stick at the direction that will be azimuth 0.
The pass uses -88° to +88° around it.

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
- At the end, the simulation stops (real clock and saved observer again) and the stepper is disabled.

Compare `antennaController.TargetAzimuth` / `TargetElevation` (commanded)
with `picoLink.Azimuth` / `Elevation` (reported by the Pico) in the
Channels tab.

## Real pass

### Once: save your location

Send these with your coordinates (decimal degrees, south and west negative;
altitude in metres):

1. `TrackerDeployment.sgp4Propagator.OBSERVER_LAT_DEG_PRM_SET`
2. `TrackerDeployment.sgp4Propagator.OBSERVER_LON_DEG_PRM_SET`
3. `TrackerDeployment.sgp4Propagator.OBSERVER_ALT_M_PRM_SET`
4. `TrackerDeployment.sgp4Propagator.OBSERVER_LAT_DEG_PRM_SAVE`, `…_LON_DEG_PRM_SAVE`, `…_ALT_M_PRM_SAVE`
5. `FileHandling.prmDb.PRM_SAVE_FILE`

They're written to `IssTracker/TrackerDeployment/PrmDb.dat`, which git
ignores, and loaded at every start (`ObserverSet` event).

### Each session

1. Fetch the current TLE (no venv needed):

   ```bash
   cd iss-tracker
   python3 tools/fetch_tle.py
   ```

2. Start the deployment and power on, as above.
3. Run `TrackerDeployment.cmdSeq.CS_RUN` with `sequences/real_pass_prepare.bin` and `NO_BLOCK`. It:
   - stops any simulation or tracking,
   - loads `tle/iss.tle` (`TleLoaded`; `TleOld` if it's over 3 days old),
   - predicts the next pass peaking at 10° or more (`PassPredicted`, times in
     UTC; Buenos Aires is UTC-3). Change the minimum with
     `TrackerDeployment.sgp4Propagator.PASS_MIN_ELEVATION_DEG_PRM_SET` and send
     `PREDICT_PASS` again,
   - lays the stick horizontal at the mount's zero.
4. Read `PassAzimuthRange`: the compass direction to face the mount's zero.
   The mount only turns ±90°, so each pass needs the mount facing its middle.
5. Turn the whole mount on the desk until the stick points that way. Use a
   phone compass set to true north (on iPhone: Settings, Compass, Use True
   North), held away from the stepper's magnet. In Buenos Aires a magnetic
   compass reads about 10° west of true north.
6. Send `TrackerDeployment.antennaController.MOUNT_ZERO_AZ_DEG_PRM_SET` with that direction (`MountZeroSet`).
7. About 2 minutes before AOS, send `TrackerDeployment.antennaController.TRACK_START`.
   The mount waits on the horizon where the ISS will rise, follows the pass,
   and stows at LOS.
8. `TrackerDeployment.picoLink.MOUNT_DISABLE`, then power off.

Near the horizon a pass can be slightly outside ±90°; the mount then holds at
the limit (under 5° from the target). A target further out is skipped and
reported as `AzimuthOutOfRange`: the mount is probably not facing the pass.

## Stopping

Any of these:

- `TrackerDeployment.cmdSeq.CS_CANCEL`, then `TrackerDeployment.antennaController.TRACK_STOP`
- `TrackerDeployment.picoLink.MOUNT_DISABLE`
- Ctrl-C on `fprime-gds`: the Pico stows and disables itself 3 s later
- Switch off the 12 V

Power off: 5 V, then 12 V, then USB.

## Commands

| Component | Command | Purpose |
|---|---|---|
| `sgp4Propagator` | `LOAD_TLE` / `LOAD_TLE_FILE` | Load a TLE from two 69-character lines / from a file |
| `sgp4Propagator` | `PREDICT_PASS` | Report the next pass and where to face the mount |
| `sgp4Propagator` | `OBSERVER_*_PRM_SET` | Observer latitude, longitude and altitude |
| `sgp4Propagator` | `PASS_MIN_ELEVATION_DEG_PRM_SET` | Lowest peak elevation `PREDICT_PASS` reports (default 10°) |
| `sgp4Propagator` | `SIM_CLOCK_START`, `SIM_OBSERVER` | Simulate a UTC time (1x to 100x) and an observer location |
| `sgp4Propagator` | `SIM_CLOCK_STOP` | End the simulation: real clock and observer parameters |
| `antennaController` | `MOUNT_ZERO_AZ_DEG_PRM_SET` | Compass direction the mount's zero faces |
| `antennaController` | `TRACK_START` / `TRACK_STOP` | Track the next pass / stop and stow |
| `picoLink` | `MOUNT_ENABLE`, `MOUNT_DISABLE`, `MOUNT_STOW`, `MOUNT_ZERO_AZ`, `MOUNT_SET` | Operate the mount by hand |
