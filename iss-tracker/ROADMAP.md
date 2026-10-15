# IssTracker roadmap

An az/el mount that points at the International Space Station in real
time, with the pointing computed by an F´ deployment. Each stage is
finished before the next one starts.

## Stage 0 — Hardware bring-up and project setup (done)

- Azimuth (stepper) and elevation (servo) axes working on a Raspberry Pi
  Pico 2 W. See [HARDWARE.md](HARDWARE.md).
- `firmware/pico-tracker-bringup`: a fixed test loop on both axes.
- F´ project scaffolded.

## Stage 1 — Orbit propagation (done)

`Sgp4Propagator` computes the ISS azimuth and elevation for an observer
from a TLE using SGP4, and predicts passes. Its results are checked against
Skyfield in the unit tests.

## Stage 2 — Pico firmware (done)

`firmware/pico-tracker` takes pointing commands over USB serial, with
position limits and a heartbeat enforced on the Pico.

## Stage 3 — F´ integration (in progress)

- Done: `AntennaController` and `PicoLink` connect the propagator to the
  mount. A simulated pass test drives the mount through a real ISS pass
  seen from a virtual observer (see [COMMANDS.md](COMMANDS.md)).
- Done: tracking a real pass from the current TLE, the observer location
  and the real clock. First real pass tracked on 2026-10-12.
- Next: a browser view of the pass.

## Stage 4 — Demo (planned)

Track an ISS pass.

## Simplifications

- Azimuth is open-loop: steps are counted from a manual compass alignment,
  with no encoder.
- Azimuth travel is limited to ±90° by the servo cable, so the mount is
  turned by hand to face the middle of each pass.
- SGP4 accuracy depends on the age of the TLE.
- Elevation calibration is linear and set by eye.
