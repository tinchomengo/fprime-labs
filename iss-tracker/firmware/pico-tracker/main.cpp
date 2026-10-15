// Az/el tracker actuator node for the Raspberry Pi Pico 2 W.
//
// Receives pointing commands over USB serial and drives the azimuth stepper
// (TMC2209, STEP/DIR) and the elevation servo. The protocol, state machine and
// motion profile are in tracker_core.hpp; this file is the hardware side.
//
// The stepper starts disabled: nothing moves in azimuth until ENABLE.
// The servo is sent to the stow elevation (90 deg, stick vertical) at boot,
// so it snaps there when its 5 V supply comes on.
//
// Wiring:
//   GP2 (pin 4) -> servo signal (orange)   [servo powered from 5 V rail]
//   GP3 (pin 5) -> TMC2209 STEP
//   GP4 (pin 6) -> TMC2209 DIR
//   GP5 (pin 7) -> TMC2209 EN (active-low)
//   MS1 = MS2 = GND -> 1/8 microstepping (1600 microsteps/rev)

#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "tracker_core.hpp"

namespace {

// ---------------- Pins ----------------
constexpr uint SERVO_PIN = 2;
constexpr uint STEP_PIN = 3;
constexpr uint DIR_PIN = 4;
constexpr uint EN_PIN = 5;

// Measured: DIR high turns the motor counter-clockwise seen from above,
// so +azimuth (clockwise, compass convention) needs DIR low.
constexpr bool AZ_POSITIVE_DIR_LEVEL = false;

// ---------------- Servo calibration (see HARDWARE.md) ----------------
constexpr float EL_HORIZON_US = 2000.0f;  // 0 deg  -> 9 o'clock
constexpr float EL_ZENITH_US = 1000.0f;   // 90 deg -> 12 o'clock

// ---------------- Timing ----------------
// Step timer: 10 kHz. A STEP pulse lasts one tick (100 us; the TMC2209 needs
// more than 100 ns), and a DIR change gets one tick of setup before the next
// STEP. At the 400 microsteps/s maximum, steps are 25 ticks apart.
constexpr int64_t STEP_TICK_US = 100;
constexpr uint32_t CONTROL_PERIOD_MS = 10;    // elevation ramp, stow, heartbeat
constexpr uint32_t TELEMETRY_PERIOD_MS = 200; // 5 Hz

tracker::Controller g_controller(static_cast<float>(STEP_TICK_US) * 1.0e-6f);

// Step timer state (only touched in the timer callback)
bool g_stepHigh = false;
int g_pendingStep = 0;
int g_lastDirection = 0;

uint16_t elevationToPulseUs(float deg) {
    if (deg < tracker::EL_MIN_DEG) deg = tracker::EL_MIN_DEG;
    if (deg > tracker::EL_MAX_DEG) deg = tracker::EL_MAX_DEG;
    const float us = EL_HORIZON_US + (EL_ZENITH_US - EL_HORIZON_US) * (deg / 90.0f);
    return static_cast<uint16_t>(us + 0.5f);
}

void servoInit(float startDeg) {
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    const uint slice = pwm_gpio_to_slice_num(SERVO_PIN);
    pwm_config cfg = pwm_get_default_config();
    // 1 MHz PWM counter -> 1 count = 1 us; wrap at 20000 -> 50 Hz frame
    pwm_config_set_clkdiv(&cfg, static_cast<float>(clock_get_hz(clk_sys)) / 1'000'000.0f);
    pwm_config_set_wrap(&cfg, 19999);
    pwm_init(slice, &cfg, false);
    pwm_set_gpio_level(SERVO_PIN, elevationToPulseUs(startDeg));
    pwm_set_enabled(slice, true);
}

void stepperInit() {
    gpio_init(EN_PIN);
    gpio_set_dir(EN_PIN, GPIO_OUT);
    gpio_put(EN_PIN, 1);  // disabled

    gpio_init(STEP_PIN);
    gpio_set_dir(STEP_PIN, GPIO_OUT);
    gpio_put(STEP_PIN, 0);

    gpio_init(DIR_PIN);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_put(DIR_PIN, AZ_POSITIVE_DIR_LEVEL);
}

//! Runs every STEP_TICK_US. Each tick does at most one thing: end a STEP
//! pulse, start a pending STEP after a DIR change, or advance the profile.
bool stepTimerCallback(repeating_timer_t*) {
    if (g_stepHigh) {
        gpio_put(STEP_PIN, 0);
        g_stepHigh = false;
        return true;
    }
    if (g_pendingStep != 0) {
        gpio_put(STEP_PIN, 1);
        g_stepHigh = true;
        g_pendingStep = 0;
        return true;
    }

    const int step = g_controller.stepTick();
    if (step == 0) {
        return true;
    }
    if (step != g_lastDirection) {
        // Set DIR now and give it a tick to settle before the STEP edge
        gpio_put(DIR_PIN, step > 0 ? AZ_POSITIVE_DIR_LEVEL : !AZ_POSITIVE_DIR_LEVEL);
        g_lastDirection = step;
        g_pendingStep = step;
        return true;
    }
    gpio_put(STEP_PIN, 1);
    g_stepHigh = true;
    return true;
}

//! Reads available characters; returns true when a complete line is in \p line
bool readLine(char* line, std::size_t size, std::size_t& length, bool& overflow) {
    while (true) {
        const int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) {
            return false;
        }
        if (c == '\r' || c == '\n') {
            if (length == 0 && !overflow) {
                continue;  // ignore the \n of a \r\n pair, and blank lines
            }
            line[length] = '\0';
            return true;
        }
        if (c == 0x7F || c == '\b') {
            if (length > 0) {
                length--;  // Backspace when typing by hand
            }
            continue;
        }
        if (length + 1 < size) {
            line[length++] = static_cast<char>(c);
        } else {
            overflow = true;
        }
    }
}

}  // namespace

int main() {
    stdio_init_all();
    stepperInit();
    servoInit(tracker::STOW_EL_DEG);

    repeating_timer_t stepTimer;
    // Negative period: measured from the start of one callback to the next
    add_repeating_timer_us(-STEP_TICK_US, stepTimerCallback, nullptr, &stepTimer);

    printf("EVT BOOT\n");

    char line[tracker::MAX_LINE_LENGTH + 1];
    std::size_t length = 0;
    bool overflow = false;
    char reply[48];
    char event[48];

    absolute_time_t nextControl = make_timeout_time_ms(CONTROL_PERIOD_MS);
    absolute_time_t nextTelemetry = make_timeout_time_ms(TELEMETRY_PERIOD_MS);

    while (true) {
        const uint32_t nowMs = to_ms_since_boot(get_absolute_time());

        if (readLine(line, sizeof(line), length, overflow)) {
            if (overflow) {
                printf("ERR TOO_LONG\n");
            } else {
                // The step timer must not run the profile while the controller changes it
                const uint32_t irq = save_and_disable_interrupts();
                g_controller.handleLine(line, nowMs, reply, sizeof(reply));
                restore_interrupts(irq);
                printf("%s\n", reply);
            }
            length = 0;
            overflow = false;
        }

        if (time_reached(nextControl)) {
            nextControl = delayed_by_ms(nextControl, CONTROL_PERIOD_MS);

            const uint32_t irq = save_and_disable_interrupts();
            g_controller.update(nowMs, static_cast<float>(CONTROL_PERIOD_MS) * 1.0e-3f, event, sizeof(event));
            const bool driverEnabled = g_controller.driverEnabled();
            const float elevation = g_controller.elevationDeg();
            restore_interrupts(irq);

            gpio_put(EN_PIN, driverEnabled ? 0 : 1);  // EN is active-low
            pwm_set_gpio_level(SERVO_PIN, elevationToPulseUs(elevation));
            if (event[0] != '\0') {
                printf("%s\n", event);
            }
        }

        if (time_reached(nextTelemetry)) {
            nextTelemetry = delayed_by_ms(nextTelemetry, TELEMETRY_PERIOD_MS);

            const uint32_t irq = save_and_disable_interrupts();
            const float azimuth = g_controller.azimuthDeg();
            const float elevation = g_controller.elevationDeg();
            const tracker::State state = g_controller.state();
            restore_interrupts(irq);

            printf("TLM %.2f %.2f %s\n", azimuth, elevation, tracker::stateName(state));
        }
    }
}
