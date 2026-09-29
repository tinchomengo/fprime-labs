# Aerospace concepts in DetumbleSil

A reference for every real aerospace/spaceflight-software concept this
project actually touches — written for a software engineer picking up
aerospace concepts through this specific codebase, not a general textbook.
Each section says exactly which file implements it, and which numbers in
that file are real physics vs. this project's own arbitrary choices.

Organized by what's **built** (Stage 1 of [ROADMAP.md](ROADMAP.md)) first,
then the discipline these build toward as a whole, then what's **planned**
(later stages) so the concept exists here even before the code does.

---

## 1. Attitude dynamics & control

Everything in this section lives in
[DetumbleSim.cpp](DetumbleSil/Components/DetumbleSim/DetumbleSim.cpp).

### Rigid body, angular velocity, inertia tensor

A spacecraft's orientation changes according to how mass is distributed
around its own axes (its **inertia tensor**, `I`) and how fast it's
spinning about each axis (**angular velocity**, `omega` = `[omega_x,
omega_y, omega_z]`, one component per body axis). This project uses a
**diagonal** inertia tensor — no off-diagonal cross-coupling terms, which
is only true if the body axes are chosen to be the object's actual
principal axes of inertia (a simplification real spacecraft usually can't
assume exactly, but a very standard first approximation):

```
Ix = 0.050 kg*m^2
Iy = 0.065 kg*m^2
Iz = 0.090 kg*m^2
```

These are deliberately unequal (`Ix != Iy != Iz`) and roughly
3U-CubeSat-scale — not sourced from any real spacecraft.

### Euler's rotational equations of motion

The actual differential equation governing how `omega` changes over time:

```
I * omega_dot = torque - omega x (I * omega)
```

`torque` is whatever's actively being commanded (see below); the
`omega x (I*omega)` term is the **gyroscopic/Coriolis coupling** between
axes — it's why a tumbling asymmetric body's motion looks chaotic even
with zero external torque, not just "spinning down."

### Asymmetric inertia and chaotic tumbling

Because `Ix != Iy != Iz` here, this is a genuine instance of the
**intermediate-axis theorem** (a.k.a. the tennis-racket theorem, a.k.a. the
Dzhanibekov effect): rotation about the *highest* or *lowest* inertia axis
is stable, but about the *middle* one it isn't — small perturbations grow,
and the body appears to flip unpredictably. That's a deliberate choice
here (see the comment in `DetumbleSim.cpp`), not a bug: it makes the
free/uncontrolled tumble genuinely three-axis and non-repeating, instead of
a clean single-axis spin.

### Quaternion kinematics (attitude representation)

Orientation itself (not just its rate of change) is tracked as a unit
quaternion `q = [qw, qx, qy, qz]`, evolved by:

```
q_dot = 0.5 * q (x) [0, omega]        ((x) = quaternion multiplication)
```

Quaternions are used instead of Euler angles (roll/pitch/yaw) specifically
to avoid **gimbal lock** — the loss of a rotational degree of freedom that
happens when two Euler-angle axes align, which is a real failure mode
Euler angles have and quaternions don't. `q` is renormalized after every
integration step, since floating-point integration alone slowly drifts a
quaternion away from unit length.

### Rate-damping control (this project's "detumble" law)

```
torque = -Kd * omega        (Kd = CONTROL_GAIN_NM_PER_RAD_S = 0.10 N*m per rad/s)
```

saturated to `MAX_TORQUE_NM = 0.05 N*m`. This is a simplified stand-in for
an actuator (reaction wheels or thrusters, unspecified) — **not** the
classical B-dot law real spacecraft use (which drives torque from the rate
of change of the *measured local magnetic field* via magnetorquers, not
directly from a known `omega`). Both share the same goal (drive `omega` to
zero) and the same shape (pure rate feedback, no reference direction), but
B-dot doesn't need to know `omega` at all — it only needs a magnetometer.
Real B-dot is planned as a closer stage-3/4 exercise once there's a
simulated magnetic field and sensor to drive it from.

### Why the settled attitude doesn't point anywhere

This control law has **no attitude-reference term** — it only ever drives
`omega -> 0`, never `attitude -> some target`. So whichever orientation the
body happens to be in the instant it stops tumbling is where it stays;
nothing here ever points a face at Earth, the sun, or anything else. This
mirrors real ADCS design: detumbling is deliberately reference-free, and
*pointing* at something is a separate control law, layered on once you
have both attitude **determination** (see §4) and a target direction —
which is exactly what stage 5's "Pointing" mode would add.

**Nadir pointing** specifically means holding one chosen body axis aimed
at the *local nadir* direction - straight down at Earth's center, which in
this project's own terms is simply `-position` (the negative of
`OrbitPropagator`'s own position vector, normalized). It's one of the most
common real pointing modes (needed for Earth-observation instruments,
many communication antennas), and it's a moving target - as the craft
orbits, nadir constantly changes direction in inertial space, so holding
it isn't a one-time maneuver but a continuously-running control law. This
project doesn't implement it (there's no pointing control law at all yet),
but the ingredient it needs that isn't yet available is attitude
*determination*, not the nadir-direction math itself, which is this one
line.

### Rotational kinetic energy

```
KE = 0.5 * (Ix*omega_x^2 + Iy*omega_y^2 + Iz*omega_z^2)
```

Telemetered as `RotationalKineticEnergy`. Not conserved here — the control
torque actively removes it (that's the whole point of the controller) —
which is a useful contrast to the *free* (uncontrolled) case, where `KE`
and angular momentum magnitude really would stay constant.

### Numerical integration

The real-world control loop runs at 10Hz (see §3), but `omega` can be
changing far faster than that during a fast tumble, so a single 100ms
integration step would be wildly inaccurate. Each `run` call instead
sub-steps forward in fixed `PHYSICS_SUBSTEP_S = 5ms` chunks internally
(simple explicit/semi-implicit Euler integration, not RK4), so the
telemetered result stays accurate independent of the outer publish rate.
This is a real numerical-methods concern any real-time embedded control
loop has to handle, not unique to this project.

---

## 2. Orbital mechanics

Everything in this section lives in
[OrbitPropagator.cpp](DetumbleSil/Components/OrbitPropagator/OrbitPropagator.cpp).

### The two-body problem

The orbit here is a **two-body** solution — Earth and the spacecraft only,
ignoring the Moon, the Sun, other satellites, and (see below) even Earth's
own non-spherical shape. This is the classical starting point of orbital
mechanics: closed-form, exactly solvable, and a very good approximation
for a spacecraft that isn't especially close to another large body.

### Kepler's third law, mean motion, orbital period

For a circular orbit of radius `r` around a body with standard
gravitational parameter `mu`:

```
n = sqrt(mu / r^3)          (mean motion - constant angular rate, rad/s)
T = 2*pi / n                 (orbital period)
```

`mu = G*M_earth = 398600.4418 km^3/s^2` is a real, tabulated constant (not
derived from `G` and Earth's mass separately here, which is the normal
convention since `mu` is known far more precisely than `G` and `M_earth`
individually). `r = 6371 + 500 = 6871 km` (Earth's real mean radius plus a
chosen 500km LEO altitude). Both `n` and `T` are computed from these at
construction, not hardcoded — the code is deriving orbital period from
first principles, not memorizing "low orbits take about 90 minutes."

### Mean anomaly, true anomaly, eccentric anomaly

These three angles describe *where* a body is along its orbit at time `t`,
and they only coincide for a **circular** orbit, which is what lets this
project get away with a much simpler formula (`angle = n * t`) than a real
orbit propagator needs. For an eccentric orbit they diverge, and relating
them requires solving **Kepler's equation** (`M = E - e*sin(E)`) — a
transcendental equation with no closed-form solution, normally solved
numerically (Newton-Raphson). That generalization is stage 2 of the
roadmap; until then, `OrbitAngle` telemetry stands in for all three at
once, which the code comments call out explicitly.

### Orbital elements (why only two are varied here)

A general orbit needs six numbers (its **classical orbital elements**) to
fully specify: semi-major axis, eccentricity, inclination, RAAN
(right ascension of ascending node), argument of periapsis, and one
anomaly/epoch. This project only varies **altitude** (-> semi-major axis,
circular so eccentricity = 0) and **inclination** (51.6 degrees — the ISS's
real inclination, used here purely as an authentic reference value, *not*
because this orbit represents the ISS). RAAN and argument of periapsis are
both fixed at zero — the ascending node sits on a fixed reference axis.

### Earth-Centered Inertial (ECI) frame

Spacecraft position (`PositionX/Y/Z` telemetry) is expressed in a
non-rotating frame centered on Earth — the standard frame orbital
mechanics is normally done in, as opposed to an Earth-*fixed* frame (which
would rotate once per day under the orbit and is what you'd need to derive
ground-track latitude/longitude, not implemented here).

### Orbital velocity

```
v = n * r     (equivalently sqrt(mu / r) - the vis-viva equation for e = 0)
```

~7.6 km/s at this orbit's altitude — a real, checkable number: that's
genuinely how fast something in a 500km circular orbit moves.

### What's *not* modeled (stated explicitly, not left implicit)

No J2 (Earth's oblateness) or higher gravity harmonics, no atmospheric
drag, no third-body (Moon/Sun) perturbation, no solar radiation pressure.
A real orbit propagator used for anything operational accounts for at
least the first of these; this one is a clean two-body Kepler solution on
purpose, as a first stage.

### Orbital position: propagation vs. determination

What `OrbitPropagator` does - compute where the craft *should* be from
known initial conditions plus the equations of motion, with no outside
measurement involved - is **propagation**, one half of orbital navigation.
The other half is **determination**: independently measuring or
cross-checking where the craft *actually* is (from GPS, ground radar
tracking, or a star fix), since real orbits drift away from a pure
two-body Kepler prediction over time (exactly because of the unmodeled
effects listed above - J2, drag, etc.). The gap between a propagated
position and an independently-determined one is a **position residual** -
a standard real accuracy metric in orbital navigation. This project has no
independent truth source to compare against (the propagation *is* the only
position that exists here), so there's no residual to compute yet; it
becomes a meaningful, checkable number once real perturbations enter the
model (stage 2+) and there's something for the pure-Kepler prediction to
be checked against.

---

## 3. Flight software & systems engineering

This is the part that's specific to building this *in F´* rather than a
generic simulation script - real flight-software architecture and
real-time systems concerns, not just orbital/attitude physics.

### Onboard vs. ground segment

F´ is **flight software** — the software that runs on a spacecraft's own
flight computer. `fprime-gds` (the separate window you run) is the
**ground segment**: mission control, watching telemetry and sending
commands over a comm link (a plain TCP socket here, standing in for a real
RF link to a real spacecraft). The deployment binary
(`DetumbleSil_TelemetryDeployment`) *is* the "spacecraft" in this analogy.

### Component-based software architecture

F´ structures flight software as independent **components**
(`DetumbleSim`, `OrbitPropagator`, plus framework ones like the command
dispatcher) connected by typed ports, each with its own `.fpp` interface
definition and generated boilerplate. Keeping attitude and orbital
simulation as two separate components (rather than one big one) mirrors
how a real spacecraft keeps those as separate subsystems.

### Rate groups & real-time scheduling

`DetumbleSim`/`OrbitPropagator` are driven by a **rate group**
(`Svc.ActiveRateGroup`) — a real-time scheduler that calls every attached
component's `run` port at a fixed rate (10Hz here, see
[Main.cpp](DetumbleSil/TelemetryDeployment/Main.cpp)). Real flight software
runs multiple rate groups at different frequencies for different
criticality levels (fast attitude control, slower housekeeping). A rate
group that fails to finish its work before the next tick is due is a
**cycle slip** — a real overrun/deadline-miss concept for any real-time
system, not unique to spacecraft.

### Commands, telemetry, and events (three distinct downlink concepts)

F´ separates these cleanly, and this project exercises all three:

- **Commands** — ground uplinks an instruction (`KICK`), the onboard
  **command dispatcher** (`Svc.CmdDispatcher`) routes it to the right
  component by opcode.
- **Telemetry channels** — periodic (or on-change) numeric/state values a
  component reports (`OmegaMagnitude`, `PositionX`, ...).
- **Events** — discrete, timestamped, severity-tagged log lines a
  component raises when something happens (`ModeChanged`,
  `OpCodeDispatched`, ...) — what the page's terminal panel streams.

### "Update on change" vs. periodic telemetry

Some channels (`RgMaxTime` while investigating, `CommandsDispatched`) only
transmit when their value actually changes, to save downlink bandwidth —
a real, deliberate tradeoff real missions make (bandwidth to/from a
spacecraft is genuinely scarce), which is also *why* those channels can
appear to show nothing to a client that connects after the value has
already stabilized: there's no "give me the current value" semantics,
only "tell me when it changes." This project hit that directly — see the
git history around the framework-telemetry tiles that got removed for it.

### Data integrity: CRC

A **CRC** (cyclic redundancy check) is a small checksum appended to a
block of data specifically to detect transmission corruption — the
receiver recomputes it from the data it actually got and compares against
the transmitted value; a mismatch means the data was corrupted in transit
and shouldn't be trusted. This matters more for a spacecraft than almost
any other kind of link: there's no cable to reseat, and a corrupted
command executed as-is could do real damage.

This project doesn't implement CRC itself — it's already there, underneath,
as part of F´'s standard uplink pipeline: `Svc.Ccsds.TcDeframer` (the
component that unpacks an incoming command frame before it reaches the
command dispatcher) validates a CRC on every frame and raises an
`InvalidCrc` event if it fails. Every `KICK` this project's page sends has
already passed a real integrity check before `DetumbleSim` ever sees it —
inherited from the framework, not something built here, but real and
active in this project's own uplink path.

### Language & build standard

F´ targets **C++14** by default (`CMAKE_CXX_STANDARD 14`, set in the
framework's own `cmake/settings.cmake`; this project doesn't override it,
so that's the effective standard `DetumbleSim.cpp`/`OrbitPropagator.cpp`
are written and compiled against). That's a real, deliberate flight-software
convention worth naming rather than assuming "newest is best": embedded
and flight targets tend to lag well behind the latest language standard,
because compiler support on qualified/flight-certified toolchains for
older, real hardware lags behind desktop compilers, and a more conservative
standard means a smaller, better-understood set of language features to
verify and trust. This project's own code (the physics/control logic in
the two components) is straightforward enough that it doesn't lean on any
particularly modern C++ feature either way — it would compile basically
unchanged under a newer standard.

---

## 4. ADCS as a discipline

**ADCS** = Attitude Determination and Control System. It's a subfield of
the broader discipline **GNC** — Guidance, Navigation, and Control:

- **Guidance** — deciding *where you want to go/point*.
- **Navigation** — figuring out where you actually are/how you're
  oriented right now (attitude *determination* is the attitude-specific
  half of this).
- **Control** — computing the actuator commands that close the gap
  between the two.

GNC as a whole also covers *translational* motion — a rocket's ascent
guidance, a lander's descent, an orbital rendezvous — none of which
applies to this project. ADCS is specifically the attitude-only slice of
GNC that's relevant to a satellite that isn't actively maneuvering its
orbit, which is exactly this project's scope. ADCS itself splits in two,
and that split matters more than it looks:

- **Determination** — figuring out what your *current* attitude actually
  is, from sensors (gyroscopes, star trackers, sun sensors, magnetometers),
  which are always noisy and never give you ground truth directly.
- **Control** — deciding what torque to command, given where you are (from
  determination) and where you want to be.

This project currently only has **half** of that: `DetumbleSim` controls
directly off a perfectly-known `omega` (simulator ground truth), with no
sensor model and no estimator in between. That's stages 3-4 of the
roadmap, and it's the single biggest simplification in the whole project -
worth naming plainly rather than leaving implicit.

### Planned, not yet built (stages 3-5 of ROADMAP.md)

- **Sensor modeling** - what a real gyro/sun-sensor/star-tracker actually
  reports: noise, bias (a slowly-drifting sensor error, distinct from
  noise), and finite sample rate.
- **State estimation** - actually solving for attitude from sensor
  readings. Two genuinely different families of method apply here:
  - **TRIAD** (TRIaxial Attitude Determination) - the classical
    *deterministic* method: take exactly two independent vector
    measurements (e.g. a sun-sensor direction and a magnetometer
    direction), known in both the body frame (as measured) and a
    reference frame (as expected, e.g. from an ephemeris/field model).
    Build an orthonormal triad of axes from each pair the same way, and
    the rotation matrix taking one triad to the other *is* the attitude
    solution - no iteration, no filtering, a single closed-form answer
    from a single instant's measurements.
  - **Complementary/Kalman filtering** - *recursive* methods that fuse
    measurements over time instead of solving from a single instant,
    trading TRIAD's simplicity for noise rejection and the ability to also
    estimate things you can't directly measure, like gyro bias.

  A real ADCS often uses both: TRIAD (or similar) for a fast initial/coarse
  solution, feeding a filter that refines and tracks it continuously.
- **Mode management** - a spacecraft doesn't run one control law forever;
  it moves through explicit **operating modes**, each with different
  sensors trusted and different control laws active:
  - **Standby** - minimal activity; waiting, not actively controlling.
  - **Detumble** - what `DetumbleSim` already implements: rate-damping
    only, no attitude reference (see §1).
  - **Pointing** - actively holding a target attitude (e.g. nadir - see
    §1) using determination + a reference direction + a tracking control
    law, none of which exist here yet.
  - **Safe** - a low-risk fallback entered autonomously on a detected
    fault (see FDIR, next), trading normal operation for whatever
    minimizes risk until a person can intervene.
- **FDIR** (Fault Detection, Isolation, and Recovery) - autonomously
  noticing something's wrong (a sensor reading out of range, a component
  gone silent) and reacting, rather than assuming everything onboard
  always works.

---

## Glossary

| Term | Meaning |
|---|---|
| GNC | Guidance, Navigation, and Control - the broader discipline ADCS belongs to |
| ADCS | Attitude Determination and Control System |
| ECI | Earth-Centered Inertial (reference frame) |
| LEO | Low Earth Orbit |
| RAAN | Right Ascension of the Ascending Node (an orbital element) |
| `mu` | Standard gravitational parameter (`G * mass`) |
| Mean/true/eccentric anomaly | Three ways of expressing where a body is along its orbit |
| Position residual | The gap between a propagated position and an independently-determined one |
| Nadir | The direction straight down from the craft to Earth's center |
| Quaternion | A 4-number attitude representation avoiding gimbal lock |
| Gimbal lock | Loss of a rotational degree of freedom in Euler-angle representations |
| B-dot control | A real detumble law driven by the rate of change of a measured magnetic field |
| TRIAD | A deterministic attitude-determination method from exactly two vector measurements |
| FDIR | Fault Detection, Isolation, and Recovery |
| CRC | Cyclic redundancy check - a checksum that detects data corrupted in transit |
| GDS | Ground Data System (F´'s ground-side tool, `fprime-gds`) |
| Rate group | A real-time scheduler driving components at a fixed frequency |
| Cycle slip | A rate group failing to finish before its next tick is due |
