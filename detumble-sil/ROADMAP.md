# DetumbleSil roadmap

A staged, learning-oriented plan for growing this from a single detumble
demo into something closer to a small ADCS (Attitude Determination and
Control System) flight-software stack — built incrementally, one aerospace
concept at a time, each stage checked in on before the next begins.

This project takes inspiration from the general shape of other open-source
SIL (Software-in-the-Loop) ADCS projects — sensor data flowing into a
flight-software process that determines attitude/orbit state and closes a
control loop — but every design choice and every line here is our own:
different architecture (F´ components instead of a custom TCP protocol),
different math where it overlaps, and a different set of simplifications
chosen for this project's own pace and goals.

## Stage 1 — Orbital trajectory (done)

- `OrbitPropagator`: a circular two-body (Kepler) orbit, propagated
  time-accelerated for visualization. Orbital radius/period follow from
  Kepler's third law computed at runtime, not hardcoded. Decoupled from
  `DetumbleSim` on purpose — orbital motion and attitude dynamics are
  separate subsystems on a real spacecraft.
- Real CubeSat STL model in the 3D view, orbiting a textured (not flat-color)
  Earth, at real relative scale to Earth and the orbit path (only the craft
  mesh's own size is exaggerated for visibility — see `constants.js`).
- Live analytics, plain data panel: rolling charts of `|omega|` and orbit
  angle, plus numeric telemetry tiles (torque, rotational kinetic energy,
  orbit angle/count, orbital velocity), fed straight from F´ channels.

Simplifications explicit in this stage (see `OrbitPropagator.cpp`):
circular only (no eccentricity), fixed RAAN/argument of periapsis, no J2
oblateness or drag, no attitude/orbit coupling (e.g. no orbit-dependent
disturbance torques yet).

**Why the settled attitude isn't nadir-pointing**: `DetumbleSim`'s control
law is `torque = -Kd * omega` — pure body-rate feedback, with no attitude
reference term at all. Its only job is driving `omega` to zero; it has no
notion of "toward Earth" or any other target direction, so whichever
orientation the body happens to be in the instant `omega` settles is where
it stays. (Runs also aren't bit-identical - wall-clock timing jitter
between `run` calls, integrated through several seconds of a genuinely
chaotic asymmetric tumble, is enough to nudge the final attitude a little
each time - but that jitter is a small perturbation on *top of* the "no
attitude target" behavior, not the cause of it.) This mirrors a real ADCS:
detumble-only control is deliberately reference-free; pointing at
something specific is a *different* control law entirely, layered on once
attitude is known - which is exactly stage 5's "Pointing" mode, gated on
stage 4's attitude determination existing first.

## Stage 2 — Elliptical orbits (planned)

Generalize `OrbitPropagator` to real eccentric orbits: solve Kepler's
equation (mean anomaly → eccentric anomaly, via Newton-Raphson) instead of
assuming mean anomaly = true anomaly. Altitude becomes a genuinely live
telemetry value (periapsis/apoapsis), rather than the constant it is for a
circular orbit today.

## Stage 3 — Simulated sensors (planned)

A `SensorSim`-style component that reports what a real IMU/star
tracker/sun sensor would actually see — with realistic noise and gyro
bias — replacing `DetumbleSim`'s current perfect-knowledge `omega`. This
is the point where "ground truth" and "what the flight software actually
knows" first diverge, which is the crux of the next stage.

## Stage 4 — Attitude determination (planned)

Estimate attitude/rate from the noisy sensor data of stage 3 (e.g. a
complementary or Kalman-style filter), and close the detumble control loop
on the *estimate*, not the simulator's ground truth — mirroring how a real
ADCS actually operates.

## Stage 5 — Mode management (planned)

A small autonomous mode manager (its own states and transition logic —
not a copy of any other project's) that selects between something like
Standby / Detumble / Pointing based on estimated rate, reference validity,
and simple health signals, and issues the corresponding command.

## Stretch goals (unscheduled)

Basic FDIR (fault detection/response) and fault injection for testing;
simple EPS (power) modeling; nadir/sun pointing once stage 4 exists.
