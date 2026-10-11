# IssTracker hardware

## Mechanical layout

- The stepper stands on its rear face. A flat plastic cap pressed onto its
  shaft is the mounting face for the elevation axis.
- The servo is glued on its side on that cap, so the horn sweeps a vertical
  plane and the servo rotates with azimuth.
- Viewed from the front, the stick moves like a clock hand: 9 o'clock is
  the horizon (0° elevation) and 12 o'clock is the zenith (90°). The
  9-o'clock end of the stick is the pointing direction.

## Power sequence

- **On:** USB, then 12 V, then 5 V. The servo moves to its start position
  when 5 V arrives.
- **Off:** 5 V, then 12 V, then USB.
- Never connect or disconnect motor wires with 12 V on.
- Before powering new wiring, with everything off, check continuity:
  - 12 V+ to every logic pin: open
  - 5 V+ to 12 V+: open
  - + to GND on each rail: no steady beep
  - each stepper coil across its two driver pins: about 6.8 Ω

## Calibration

| Parameter | Value |
|---|---|
| TMC2209 Vref | 0.688 V (about 0.49 A RMS with 0.11 Ω sense resistors), measured with 12 V on and the motor disconnected |
| Azimuth resolution | 1600 microsteps/rev (4.444 microsteps/°) |
| Azimuth sign | +az is clockwise seen from above, which is DIR low (`AZ_POSITIVE_DIR_LEVEL = false`) |
| Elevation 0° (horizon, 9 o'clock) | 2000 µs |
| Elevation 90° (zenith, 12 o'clock) | 1000 µs |
| Elevation scale | about 11.1 µs/°, linear, checked by eye at 0°, 45° and 90° |

- **Elevation correction:** if the stick sits at clock-hour `h_obs` when it
  should be at `h_target`, use `new_us = old_us + (h_obs - h_target) × 333`.
  More µs moves the horn counter-clockwise seen from the front. Count hours
  past 12 as 13, 14, and so on.
- **Azimuth sign:** swapping the two wires of one coil reverses the
  direction, so re-check the sign after any motor rewiring.
- **Servo range:** only 1000 to 2000 µs has been tested.

## Bring-up issues

1. **Coil resistance read about 7.5 Ω per half instead of 3.4 Ω.** The
   motor's original cable and connector added resistance. Measuring on the
   motor pins gave the correct value.
2. **The original cable had no wire on motor pin 6**, which is a coil end.
   Replaced with jumpers directly on the motor pins.
3. **Coil A read open across its driver pins** although each jumper had
   continuity. The jumper sockets weren't gripping motor pins 1 and 3.
   Fixed by reseating them.
4. **The stepper vibrated in place with periodic pauses** after the servo
   cable snagged. Coil A had lost contact, so only one phase was driven.
   Fixed by reseating and adding strain relief.
5. **The servo arc pointed below the horizon** (9 o'clock to 6 o'clock).
   The zenith was outside the servo's range, so software couldn't correct
   it. Fixed by remounting the horn on the spline at a different angle.
6. **Vref read about 0 V with only USB power.** On this module Vref only
   reads correctly with 12 V applied, so it has to be set with 12 V on and
   the motor disconnected.
