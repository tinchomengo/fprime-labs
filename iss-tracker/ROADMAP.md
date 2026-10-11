# IssTracker roadmap

An az/el mount that points at the International Space Station in real
time, with the pointing computed by an F´ deployment. Each stage is
finished before the next one starts.

## Stage 0 — Hardware bring-up and project setup (done)

- Azimuth (stepper) and elevation (servo) axes working on a Raspberry Pi
  Pico 2 W. See [HARDWARE.md](HARDWARE.md).
- `firmware/pico-tracker-bringup`: a fixed test loop on both axes.
- F´ project scaffolded.

## Stage 1 — Orbit propagation (planned)

Compute the ISS azimuth and elevation for an observer from a TLE using
SGP4, and predict upcoming passes. Results are checked against an
independent reference implementation.

## Stage 2 — Pico firmware (planned)

Replace the test loop with a serial interface so the host can command the
mount, with position limits enforced on the Pico.

## Stage 3 — F´ integration (planned)

F´ components for tracking and fault handling, and a browser view of the
pass.

## Stage 4 — Demo (planned)

Track a visible ISS pass.

## Simplifications

- Azimuth is open-loop: steps are counted from a manual north alignment,
  with no encoder.
- SGP4 accuracy depends on the age of the TLE.
- Elevation calibration is linear and set by eye.
