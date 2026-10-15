# IssTracker F´ project

F´ (F Prime) is a component-driven framework that enables rapid development and deployment of spaceflight and other embedded software applications.
**Please Visit the F´ Website:** https://fprime.jpl.nasa.gov.

## What this is

An az/el mount that points a stick at the International Space Station. A
stepper turns it in azimuth and a servo tilts it in elevation, driven by a
Raspberry Pi Pico 2 W.

The F´ deployment propagates the ISS orbit with SGP4 (`Sgp4Propagator`),
decides where the mount should point (`AntennaController`) and sends the
commands to the Pico over USB serial (`PicoLink`).

See [COMMANDS.md](COMMANDS.md) to run the simulated pass test,
[ROADMAP.md](ROADMAP.md) for the stages and [HARDWARE.md](HARDWARE.md) for
the power sequence, calibration and bring-up notes.

## Layout

- `IssTracker/Components/`: the F´ components, each with unit tests.
- `IssTracker/TrackerDeployment/`: the deployment. `sequences/` has the
  simulated pass test.
- `firmware/pico-tracker/`: Pico 2 W firmware that points the mount on
  commands received over USB serial. The protocol is described in
  `tracker_core.hpp`; `test/` has a host test for it.
- `firmware/pico-tracker-bringup/`: Pico 2 W bring-up firmware. It sweeps
  elevation through 90°, 45°, 0°, 45° and back to 90°, then turns azimuth
  +90° and back, printing each position over USB serial.
- `tools/`: scripts that generate the reference values for the unit tests.
- `lib/fprime`: F´ v4.2.2 (git submodule).
- `lib/sgp4`: [dnwrnr/sgp4](https://github.com/dnwrnr/sgp4) (git submodule).

## Building and flashing the firmware

Requires [pico-sdk](https://github.com/raspberrypi/pico-sdk) 2.3.0 with
`PICO_SDK_PATH` set, and the `arm-none-eabi` toolchain.

```bash
cd firmware/pico-tracker
cmake -B build
cmake --build build
```

1. With 12 V and 5 V off, hold BOOTSEL on the Pico and plug in USB. An
   `RP2350` drive appears.
2. Copy `build/pico_tracker.uf2` onto it. The Pico reboots into the new
   firmware.
3. Watch the output with `screen /dev/tty.usbmodem* 115200` (quit with
   `Ctrl-A`, `K`, `y`).
4. Power on 12 V, then 5 V.

The bring-up firmware builds and flashes the same way, from
`firmware/pico-tracker-bringup` (`build/tracker_bringup.uf2`).

## Building the F´ project

```bash
python3 -m venv fprime-venv
source fprime-venv/bin/activate
pip install -r requirements.txt
fprime-util generate
fprime-util build
```
