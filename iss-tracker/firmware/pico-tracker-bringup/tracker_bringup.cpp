// Two-axis bring-up test for the az/el tracker on a Raspberry Pi Pico 2 W.
//
// Loop:
//   1. Elevation (servo) sweeps slowly 90 -> 45 -> 0 -> 45 -> 90 deg
//      (viewed from the front: 12 -> 10:30 -> 9 o'clock and back),
//      holding 1 s at each stop and printing angle + pulse width.
//   2. Azimuth (stepper) turns 90 deg clockwise from where it started,
//      pauses, and turns back.
// Azimuth stays between its start position and +90 deg, so the servo cable
// never wraps.
//
// Elevation is commanded in degrees and clamped to [0, 90], so the stick
// can never be driven below the horizon.
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
#include "pico/stdlib.h"

namespace {

// ---------------- Servo (elevation) ----------------
constexpr uint SERVO_PIN = 2;

// Calibration: pulse width that puts the stick at the horizon (0 deg,
// 9 o'clock seen from the front) and at the zenith (90 deg, 12 o'clock).
// Measured: +1000 us = 3 clock-hours (90 deg) counter-clockwise, i.e.
// ~11.1 us/deg. To correct a position observed at clock-hour h_obs when it
// should be at h_target:  new_us = old_us + (h_obs - h_target) * 333.
constexpr float EL_HORIZON_US = 2000.0f;  // 0 deg  -> 9 o'clock
constexpr float EL_ZENITH_US = 1000.0f;   // 90 deg -> 12 o'clock

constexpr float EL_MIN_DEG = 0.0f;
constexpr float EL_MAX_DEG = 90.0f;
constexpr float EL_START_DEG = 45.0f;

constexpr uint16_t SERVO_RAMP_STEP_US = 10;
constexpr uint32_t SERVO_RAMP_PERIOD_MS = 20;  // 10 us / 20 ms = slow sweep

uint16_t el_deg_to_us(float deg) {
    if (deg < EL_MIN_DEG) deg = EL_MIN_DEG;
    if (deg > EL_MAX_DEG) deg = EL_MAX_DEG;
    const float us = EL_HORIZON_US + (EL_ZENITH_US - EL_HORIZON_US) * (deg / 90.0f);
    return static_cast<uint16_t>(us + 0.5f);
}

uint16_t g_servo_us = el_deg_to_us(EL_START_DEG);

void servo_init() {
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    const uint slice = pwm_gpio_to_slice_num(SERVO_PIN);
    pwm_config cfg = pwm_get_default_config();
    // 1 MHz PWM counter -> 1 count = 1 us; wrap at 20000 -> 50 Hz frame.
    pwm_config_set_clkdiv(&cfg, static_cast<float>(clock_get_hz(clk_sys)) / 1'000'000.0f);
    pwm_config_set_wrap(&cfg, 19999);
    pwm_init(slice, &cfg, false);
    pwm_set_gpio_level(SERVO_PIN, g_servo_us);
    pwm_set_enabled(slice, true);
}

void servo_write_us(uint16_t us) {
    pwm_set_gpio_level(SERVO_PIN, us);
    g_servo_us = us;
}

// Ramps through intermediate pulse widths; both endpoints come from
// el_deg_to_us(), so every intermediate value is inside [0, 90] deg too.
void servo_ramp_to(uint16_t target_us) {
    while (g_servo_us != target_us) {
        if (g_servo_us < target_us) {
            const uint16_t next = g_servo_us + SERVO_RAMP_STEP_US;
            servo_write_us(next > target_us ? target_us : next);
        } else {
            const uint16_t next = g_servo_us - SERVO_RAMP_STEP_US;
            servo_write_us(next < target_us ? target_us : next);
        }
        sleep_ms(SERVO_RAMP_PERIOD_MS);
    }
}

void el_move_to(float deg) {
    servo_ramp_to(el_deg_to_us(deg));
    printf("  EL %.0f deg (%u us)\n", deg, g_servo_us);
}

// ---------------- Stepper (azimuth) ----------------
constexpr uint STEP_PIN = 3;
constexpr uint DIR_PIN = 4;
constexpr uint EN_PIN = 5;

constexpr int STEPS_PER_REV = 200 * 8;  // 1.8 deg/step, 1/8 microstep
constexpr float STEPS_PER_DEG = STEPS_PER_REV / 360.0f;
constexpr uint32_t STEP_PULSE_US = 5;
constexpr uint32_t STEP_PERIOD_US = 2500;  // 400 microsteps/s -> 90 deg in 1 s

// Azimuth convention: compass azimuth, positive = clockwise seen from above
// (N 0 -> E 90 -> S 180 -> W 270).
// Measured: DIR high turns the motor counter-clockwise seen from above,
// so +azimuth needs DIR low.
constexpr bool AZ_POSITIVE_DIR_LEVEL = false;

void stepper_init() {
    gpio_init(EN_PIN);
    gpio_set_dir(EN_PIN, GPIO_OUT);
    gpio_put(EN_PIN, 1);  // disabled until setup is done

    gpio_init(STEP_PIN);
    gpio_set_dir(STEP_PIN, GPIO_OUT);
    gpio_put(STEP_PIN, 0);

    gpio_init(DIR_PIN);
    gpio_set_dir(DIR_PIN, GPIO_OUT);
    gpio_put(DIR_PIN, 0);
}

// Relative azimuth move; positive = clockwise seen from above.
void az_move_deg(float delta_deg) {
    const bool positive = delta_deg >= 0.0f;
    const float magnitude = positive ? delta_deg : -delta_deg;
    const int steps = static_cast<int>(magnitude * STEPS_PER_DEG + 0.5f);

    gpio_put(DIR_PIN, positive ? AZ_POSITIVE_DIR_LEVEL : !AZ_POSITIVE_DIR_LEVEL);
    sleep_us(10);  // DIR setup time before the first STEP edge
    for (int i = 0; i < steps; ++i) {
        gpio_put(STEP_PIN, 1);
        sleep_us(STEP_PULSE_US);
        gpio_put(STEP_PIN, 0);
        sleep_us(STEP_PERIOD_US - STEP_PULSE_US);
    }
}

}  // namespace

int main() {
    stdio_init_all();
    stepper_init();
    servo_init();

    sleep_ms(2000);  // time to open the serial monitor
    printf("Tracker bring-up: EL horizon %.0f us, zenith %.0f us; AZ 0 to +90 deg\n",
           EL_HORIZON_US, EL_ZENITH_US);

    gpio_put(EN_PIN, 0);  // energize stepper
    sleep_ms(100);

    while (true) {
        printf("EL: sweep 90 -> 45 -> 0 -> 45 -> 90\n");
        el_move_to(90.0f);  // 12 o'clock
        sleep_ms(1000);
        el_move_to(45.0f);  // 10:30
        sleep_ms(1000);
        el_move_to(0.0f);   // 9 o'clock
        sleep_ms(1000);
        el_move_to(45.0f);
        sleep_ms(1000);
        el_move_to(90.0f);
        sleep_ms(500);

        printf("AZ: +90 deg (clockwise from above)\n");
        az_move_deg(90.0f);
        sleep_ms(1000);
        printf("AZ: -90 deg (counter-clockwise from above)\n");
        az_move_deg(-90.0f);
        sleep_ms(1000);
    }
}