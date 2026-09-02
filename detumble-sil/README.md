# DetumbleSil F´ project

F´ (F Prime) is a component-driven framework that enables rapid development and deployment of spaceflight and other embedded software applications.
**Please Visit the F´ Website:** https://fprime.jpl.nasa.gov.

## What this is

Demo entirely in F´ - no hardware. Two
independent simulated subsystems, each its own F´ component (deliberately
kept separate - orbital motion and attitude dynamics are separate
subsystems on a real spacecraft):

- [DetumbleSim](DetumbleSil/Components/DetumbleSim) simulates a tumbling
  rigid body (Euler's rotational equation + quaternion kinematics) under a
  rate-damping control torque, closing the loop every simulation cycle.
- [OrbitPropagator](DetumbleSil/Components/OrbitPropagator) propagates a
  circular two-body (Kepler) orbit - radius and period derived at runtime
  from Kepler's third law, not hardcoded - time-accelerated for
  visualization.

Both are deliberately simplified stand-ins, not a flight-grade ADCS - see
the header comments in each component's `.fpp`/`.cpp` for exactly what's
real physics vs. illustrative/untuned, and [ROADMAP.md](ROADMAP.md) for
what's staged next (elliptical orbits, simulated sensors, attitude
determination, mode management).

Telemetry streams out through F´/GDS like any other deployment;
`bridge/telemetry_bridge.py` forwards it to a WebSocket, and
`visualization/` renders it live in the browser - see
[COMMANDS.md](COMMANDS.md) to run the full stack. That page shows:

- A real CubeSat STL model (see `visualization/models/NOTICE` for
  attribution), attitude- and position-driven by live telemetry, orbiting
  a textured, to-scale Earth (real NASA imagery, specular/normal maps, an
  independently-drifting cloud layer, and a Fresnel-glow atmosphere shell -
  see `visualization/models/earth/NOTICE`) along its actual trajectory path.
- A plain data panel, not a branded UI - rolling charts of `|omega|` and
  orbit angle, plus numeric tiles for torque, rotational kinetic energy,
  orbit angle/count, and orbital velocity - all read straight off F´
  telemetry channels, not computed client-side.
- A live terminal panel streaming the _entire_ F´ event log verbatim - mode
  changes, orbit completions, command dispatch/completion, rate group
  health, everything, from every component, not filtered to one subsystem.
  Real F´ framework activity, not simulated physics, and all of it from
  the _onboard_ side (F´ is flight software; the `fprime-gds` window is
  the separate ground-station tool watching it, standing in for a real RF
  link). This is what shows command activity (e.g. `KICK` from the
  Re-kick button): real `OpCodeDispatched`/`OpCodeCompleted` event lines.
  (Two framework telemetry tiles - rate-group max cycle time and cycle
  slips - were tried first and dropped: on this light a workload they sat
  at/near 0 too often to be worth a tile, versus the event log's always-live
  activity. Wiring up command-dispatch telemetry along the way surfaced a
  real gap in the bootstrap-generated topology: `cmdDisp`'s scheduled `run`
  port was never connected to a rate group, so its telemetry channels could
  never have published at all - independent of the event log, and still
  fixed, in `topology.fpp`.)

A `KICK` command re-tumbles the body on demand (also reachable from the
page's "Re-kick" button, relayed back through the bridge), so the detumble
sequence can be re-triggered without restarting the deployment - handy for
repeat demo takes.
