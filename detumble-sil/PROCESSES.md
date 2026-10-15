# How this actually works

[CONCEPTS.md](CONCEPTS.md) explains the aerospace/CS *ideas* behind this
project. This doc is different: it traces the actual **data flow and call
chain**, hop by hop, for the specific things you see happen on screen -
what calls what, over what transport, in what order - grounded in the
real source, not a general description. Where a claim here depends on a
specific line of code, it's linked.

---

## 1. How the Re-kick feature works

Clicking **Re-kick** on the page does not simulate anything client-side.
It sends one real F´ command, uplinked exactly the way a ground station
would send it to a spacecraft, and everything that happens is a
consequence of F´ actually processing it:

```
Browser click
  -> visualizer.js: socket.send("KICK")                              [scripts/visualizer.js]
  -> bridge: handle_client() sees the literal text "KICK"            [bridge/telemetry_bridge.py]
  -> bridge: pipeline.send_command("TelemetryDeployment.detumbleSim.KICK", [])
  -> GDS encodes a real F' command packet, sends it over the TCP link
  -> ComCcsds uplink chain decodes/validates it (CRC-checked - see CONCEPTS.md)
  -> CdhCore.cmdDisp (command dispatcher) matches opcode 0x10015000
  -> routes to DetumbleSim's command port (registered as "port 12" -
     `cmdDisp` itself logs this exact registration on startup: "Opcode
     0x10015000 registered to port 12 slot 44", visible in the
     deployment's own console/event output)
  -> DetumbleSim::KICK_cmdHandler() runs:                             [DetumbleSim.cpp]
       - resetToTumble(): q <- identity, omega <- the fixed
         INITIAL_OMEGA_X/Y/Z_DEG_S values (always the same three
         numbers - that's deliberate, see CONCEPTS.md)
       - log_ACTIVITY_HI_ModeChanged(TUMBLING)  - always announced,
         even if mode was already TUMBLING (see the code comment on
         why - a real KICK marker on the timeline, not just an edge)
       - cmdResponse_out(OK)
```

The bridge's text-string check (`if message == "KICK"`) is the *only*
special-cased thing on the whole path - everything after that point is
exactly what would happen if a real ground operator typed the equivalent
command into `fprime-gds`'s own console instead of clicking a web button.

## 2. Stabilization after a kick

This part is pure physics, running entirely inside `DetumbleSim`, and it's
identical whether the tumble started from `KICK` or from the deployment's
own startup. Every rate-group cycle (10Hz - see §4), `run_handler`:

1. Computes `dt_s`, the real wall-clock time since the last cycle.
2. Sub-steps forward through that `dt_s` in fixed 5ms chunks
   (`PHYSICS_SUBSTEP_S`), because `omega` can change faster than 10Hz
   during a fast tumble and a single 100ms integration step would be too
   coarse. Each 5ms sub-step:
   - Computes `torque = -Kd * omega` (`Kd = 0.10`), clamped to
     `MAX_TORQUE_NM = 0.05`.
   - Advances `omega` via Euler's rotational equation, and `q` via
     quaternion kinematics (both explained in CONCEPTS.md).
3. After all sub-steps, `publishTelemetry()` converts `omega` to deg/s,
   computes `|omega|`, and compares it to `STABLE_OMEGA_THRESHOLD_DEG_S =
   2.0`. Crossing that threshold flips `Mode` and fires the
   `ModeChanged` event you see in the terminal.

With this project's tuned gain, that threshold crossing happens roughly
6-7 seconds after a kick — not just assumed: it was timed at ~6.44s during
development, and the exact log lines walked through in §5 show it again,
independently, at 6.47s (`15:01:50.275878` -> `15:01:56.749354`) -
consistent with each other, and with `gain=0.10` -> `settle~6.4s` from the
physics itself.

## 3. How the trajectory-following orbit works

This is two *independent* things, computed by two different components
that never talk to each other, composited onto the same 3D object only in
the browser:

**F´ side** ([OrbitPropagator.cpp](DetumbleSil/Components/OrbitPropagator/OrbitPropagator.cpp)) -
every 10Hz cycle, accumulates a time-accelerated simulated clock
(`m_simTime_s += dt_wallclock * 12`), computes the orbit angle from that
via the constant angular rate a circular orbit has (`n * simTime`), turns
that angle into an ECI position (in-plane, then tilted by the fixed 51.6°
inclination), and publishes `PositionX/Y/Z` in km - real telemetry,
exactly like everything else.

**Browser side** ([visualizer.js](visualization/scripts/visualizer.js)) -
receives those three numbers over the WebSocket, scales them into scene
units, and every animation frame (independent of how often new telemetry
actually arrives) eases the craft's *displayed* position toward the
latest received value, so 10Hz samples read as continuous motion at 60fps
rather than visible jumps. Three things ride on that same eased position
each frame:

- `craft.position` - the CubeSat model itself.
- The camera + orbit-controls target - both translated by the same
  per-frame delta as the craft, so the camera rigidly follows it (see §7
  for why this exists).
- The velocity-direction arrow's origin. Its *direction*, though, is
  **not** telemetry - `OrbitPropagator` only publishes speed
  (`OrbitalVelocity`), not a velocity vector - so the arrow's direction is
  computed client-side from the same circular-orbit geometry (`orbital
  plane normal × position`, normalized). It's a display convenience
  derived from the same physics, not a second source of truth.

**The static orbit ring** you see the craft travel along is drawn once,
at page load, directly from `ORBIT_RADIUS_KM`/inclination constants
hardcoded in `visualizer.js` - not fetched from telemetry, since for a
*circular* orbit the whole path is a fixed shape computable in advance.
This is a real, human-maintained consistency risk worth naming plainly:
those constants only match `OrbitPropagator.cpp`'s actual altitude/
inclination because both files say so in a comment pointing at each other
— nothing enforces it automatically. Changing one without the other would
silently make the drawn ring wrong.

## 4. F´'s role in the project

Concretely, not just conceptually: **every number this page shows is
computed inside an F´ component, in C++, and nowhere else.** The chain a
piece of data takes to get from "computed" to "on your screen":

```
F' component (C++, DetumbleSim/OrbitPropagator)
  -> tlmWrite_*() - hands the value to F's own telemetry system
  -> downlinked over the TCP link to fprime-gds
  -> GDS decodes the raw bytes into a named, typed value (using the
     dictionary generated from this project's own .fpp files)
  -> bridge/telemetry_bridge.py: a DataHandler registered with GDS's
     Python pipeline API receives it, translates the F' channel name to
     a short JS-friendly key, re-encodes as JSON
  -> WebSocket -> browser
  -> visualizer.js: eases/smooths for display, never recomputes physics
```

The browser does exactly three kinds of thing with this data: **ease it**
(so 10Hz samples look smooth at 60fps), **derive pure display geometry**
from it (the velocity arrow's direction, the camera-follow delta), and
**render it**. It never runs the actual attitude or orbital dynamics -
those exist in exactly one place, `DetumbleSim.cpp`/`OrbitPropagator.cpp`,
running as real F´ components under F's own real-time scheduler. The
bridge script is not part of F´ itself - it's a small Python translator
this project wrote, sitting on top of GDS's own public pipeline API,
whose only job is turning F' channel/event names into the JSON shape the
browser expects.

## 5. Reading the terminal log

Every line is one F´ event, forwarded verbatim (not reformatted) by the
bridge's `EventBridge`. The format is F's own:

```
<ISO-8601 timestamp>: <deployment>.<component>.<eventName> EventSeverity.<severity> : <message>
```

The timestamp is the deployment's own system clock at the instant the
event was logged (via the `chronoTime` component every component's `time`
port is wired to) - not a network-receipt time. `EventSeverity` is F's own
event category, not a generic log level: `ACTIVITY_HI` marks normal,
notable operational activity; `COMMAND` specifically marks command
dispatch/response bookkeeping; other severities (`WARNING_HI`, `FATAL`,
...) exist for actual problems and would look visibly different if one
ever fired.

Walking through the exact five lines you pasted, as one real sequence:

```
14:59:54.559  ModeChanged -> STABLE      a previous tumble finished settling
15:01:50.275  ModeChanged -> TUMBLING    a KICK just ran (§1/§2)
15:01:50.276  OpCodeDispatched            the dispatcher logging the route it just took
15:01:50.276  OpCodeCompleted             the dispatcher logging DetumbleSim's OK response
15:01:56.749  ModeChanged -> STABLE      ~6.47s later: |omega| crossed back below threshold
```

The middle three lines are one KICK command's entire round trip, and their
*order* is a real, checkable consequence of F's own command-dispatch
mechanics, not arbitrary: `CommandDispatcherImpl` calls
`compCmdSend_out(...)` (which - because `DetumbleSim`'s command port is
declared `sync`, the only kind a passive component can have - synchronously
runs `KICK_cmdHandler` right there, which is where `ModeChanged` gets
logged) and only *after* that call returns does it log
`OpCodeDispatched`. `KICK_cmdHandler`'s own `cmdResponse_out(OK)` call,
by contrast, targets `compCmdStat`, a port the dispatcher declares
`async` — so instead of running immediately, it's queued, and
`OpCodeCompleted` is logged slightly later, as the next message the
dispatcher's own task picks up (the ~48µs gap between `OpCodeDispatched`
and `OpCodeCompleted`, versus the ~880µs gap before it, is consistent with
exactly that: one is "still unwinding the same call stack," the other is
"picking up the next queued message").

## 6. How the `|omega|` chart's data is measured and represented

- **Measured**: `DetumbleSim` computes `omega` (rad/s) via the sub-stepped
  Euler integration in §2, converts to deg/s, and takes the vector
  magnitude - once per 10Hz cycle. (The 5ms sub-steps are accurate
  internally, but only the *last* sub-step of each 100ms cycle is what
  actually gets telemetered - the channel is a 10Hz snapshot, not a
  5ms-resolution stream.)
- **Sent**: `OmegaMagnitude` is telemetered unconditionally every cycle
  (it has no "update on change" qualifier in `DetumbleSim.fpp`), so a new
  value reaches the bridge, and the browser, 10 times a second regardless
  of whether it actually changed.
- **Charted vs. displayed - a real distinction**: the numeric `|omega|`
  tile shows `displayScalars.omegaMagnitude`, an *eased* value smoothed
  frame-by-frame toward the latest sample (`SCALAR_SMOOTHING_RATE`). The
  **chart**, by contrast, is pushed `targetScalars.omegaMagnitude` -
  the raw, unsmoothed value exactly as F´ last reported it, sampled once
  per animation frame at whatever `targetScalars` currently holds. So the
  chart is the closer-to-raw-telemetry view of the two, and the tile
  number is the easier-to-read one; they'll match once a value has been
  steady for a moment, but during a fast change they can visibly differ
  for an instant.
- **Rendered**: `chart.js`'s `TimeSeriesChart` keeps a rolling
  `CHART_WINDOW_S`-second buffer of `(time, value)` samples and redraws
  the whole visible window every animation frame - it's a plain
  `<canvas>` line, not a library.

## 7. How the two 3D models connect to the system

**Earth** is not driven by telemetry at all. It's a fixed-radius textured
sphere sitting at the scene origin; the only thing that moves on it is the
cloud layer, which self-rotates at a small constant rate purely for visual
effect, unrelated to any data. (Earth's *real* rotation - once per ~24h -
isn't modeled either way; see CONCEPTS.md's note on the ECI frame, which
treats Earth as non-rotating background by convention, not by omission.)

**The CubeSat** is the one object actually driven by two independent live
telemetry streams at once:

```
DetumbleSim.{Qw,Qx,Qy,Qz}      -> craft.quaternion   (attitude/rotation)
OrbitPropagator.{PositionX,Y,Z} -> craft.position      (orbital location)
```

Both are applied to the *same* `craft` object, but they come from two F´
components that have no port connecting them and never exchange data -
the compositing only happens visually, in the browser. That's a real
simplification worth naming (see CONCEPTS.md: no orbit/attitude coupling
is modeled - a real spacecraft's orbit and attitude aren't perfectly
independent, e.g. gravity-gradient torque). The body-frame axis
arrows are children of the `craft` object in the three.js scene graph, so
they inherit its rotation automatically - no extra per-frame code needed
to keep them pointing along the body axes. The velocity arrow and the
camera/controls target, by contrast, are *not* children of `craft` (a
world-frame direction and a camera don't belong in the craft's local
coordinate space) and are instead explicitly repositioned every frame
from the same `displayPosition` value, in code (§3).
