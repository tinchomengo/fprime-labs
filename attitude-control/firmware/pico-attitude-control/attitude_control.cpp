// Closes an attitude-stabilization loop entirely on the Pico: reads roll/
// pitch/yaw from an MPU6050 over I2C (i2c0), runs a proportional controller
// against one axis, and drives a servo (PWM on GP2) to counteract deviation
// from level - all locally, independent of F', for a tight control loop.
//
// Also streams status over USB serial ("roll,pitch,yaw,servo_us\n") purely
// for supervisory monitoring (AttitudeInterface, on the F' side) - nothing
// on the F' side is in this loop's hot path.
//
// I2C: SDA to GPIO0, SCL to GPIO1
// Servo signal: GPIO2 (physical pin 4)

#include <math.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

namespace {

// ----------------------------------------------------------------------
// MPU6050 / I2C - unchanged from the sibling project's firmware
// ----------------------------------------------------------------------

constexpr uint8_t MPU6050_ADDR = 0x68;
constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t REG_WHO_AM_I = 0x75;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t WHO_AM_I_EXPECTED = 0x68;

constexpr uint I2C_SDA_PIN = 0;
constexpr uint I2C_SCL_PIN = 1;
constexpr uint I2C_BAUDRATE_HZ = 400000;  // 400kHz ("fast mode")

constexpr uint32_t I2C_TIMEOUT_US = 50000;  // 50ms

constexpr float ACCEL_SENSITIVITY_LSB_PER_G = 16384.0f;
constexpr float GYRO_SENSITIVITY_LSB_PER_DPS = 131.0f;

constexpr float COMPLEMENTARY_FILTER_ALPHA = 0.98f;

// TEMPORARY, and NOT YET VERIFIED for this project's physical mounting:
// these are copied from the sibling project (imu-altitude-visualization),
// where they were empirically found for THAT project's mounting angle and
// axis orientation. This board has been soldered fresh, in a new physical
// position (near the servo mechanism) - both the offsets AND the roll/
// pitch axis-swap below need to be re-verified with the same isolated-
// axis-testing methodology once assembled,
// not assumed to carry over.
constexpr float ROLL_MOUNTING_OFFSET_DEG = 0.0f;
constexpr float PITCH_MOUNTING_OFFSET_DEG = 0.0f;

constexpr uint32_t LOOP_PERIOD_MS = 50;  // 20Hz sensor sampling / control loop rate
constexpr float LOOP_PERIOD_S = LOOP_PERIOD_MS / 1000.0f;

constexpr uint32_t HEARTBEAT_PERIOD_MS = 500;  // approx 1Hz LED blink
constexpr int LOOPS_PER_HEARTBEAT_TOGGLE = HEARTBEAT_PERIOD_MS / LOOP_PERIOD_MS;

struct RawReading {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
};

void i2c_bus_scan() {
    printf("Scanning I2C bus...\n");
    bool found_any = false;
    for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
        uint8_t dummy = 0x00;
        int result = i2c_write_timeout_us(i2c0, addr, &dummy, 1, false, I2C_TIMEOUT_US);
        if (result == 1) {
            printf("  Found device at address 0x%02X\n", addr);
            found_any = true;
        }
    }
    if (!found_any) {
        printf("  No devices found on the bus at all.\n");
    }
}

bool mpu6050_probe() {
    uint8_t reg = REG_WHO_AM_I;
    int wrote = i2c_write_timeout_us(i2c0, MPU6050_ADDR, &reg, 1, true, I2C_TIMEOUT_US);
    if (wrote != 1) {
        return false;
    }
    uint8_t who_am_i = 0;
    int read = i2c_read_timeout_us(i2c0, MPU6050_ADDR, &who_am_i, 1, false, I2C_TIMEOUT_US);
    return read == 1 && who_am_i == WHO_AM_I_EXPECTED;
}

bool mpu6050_init() {
    uint8_t buf[2] = {REG_PWR_MGMT_1, 0x00};  // clear sleep bit, wake the sensor
    int wrote = i2c_write_timeout_us(i2c0, MPU6050_ADDR, buf, 2, false, I2C_TIMEOUT_US);
    return wrote == 2;
}

bool mpu6050_read_raw(RawReading* out) {
    uint8_t reg = REG_ACCEL_XOUT_H;
    int wrote = i2c_write_timeout_us(i2c0, MPU6050_ADDR, &reg, 1, true, I2C_TIMEOUT_US);
    if (wrote != 1) {
        return false;
    }

    uint8_t data[14];
    int read = i2c_read_timeout_us(i2c0, MPU6050_ADDR, data, sizeof(data), false, I2C_TIMEOUT_US);
    if (read != static_cast<int>(sizeof(data))) {
        return false;
    }

    out->accel_x = static_cast<int16_t>(data[0] << 8 | data[1]);
    out->accel_y = static_cast<int16_t>(data[2] << 8 | data[3]);
    out->accel_z = static_cast<int16_t>(data[4] << 8 | data[5]);
    out->gyro_x = static_cast<int16_t>(data[8] << 8 | data[9]);
    out->gyro_y = static_cast<int16_t>(data[10] << 8 | data[11]);
    out->gyro_z = static_cast<int16_t>(data[12] << 8 | data[13]);
    return true;
}

// ----------------------------------------------------------------------
// Servo (PWM) - new for this project
// ----------------------------------------------------------------------

constexpr uint SERVO_PIN = 2;  // GP2, physical pin 4
constexpr uint16_t SERVO_PULSE_MIN_US = 1000;
constexpr uint16_t SERVO_PULSE_MAX_US = 2000;
constexpr uint16_t SERVO_PULSE_CENTER_US = 1500;
constexpr uint32_t SERVO_PWM_PERIOD_US = 20000;  // 20ms = standard 50Hz hobby-servo signaling

void pwm_setup_servo() {
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(SERVO_PIN);

    pwm_config config = pwm_get_default_config();
    // Assumes the default 125MHz system clock. Dividing by 125 makes each
    // PWM counter tick exactly 1us, so a pulse width in microseconds can be
    // written directly as the PWM level - no extra unit conversion needed.
    pwm_config_set_clkdiv(&config, 125.0f);
    pwm_config_set_wrap(&config, static_cast<uint16_t>(SERVO_PWM_PERIOD_US - 1));
    pwm_init(slice, &config, true);

    pwm_set_gpio_level(SERVO_PIN, SERVO_PULSE_CENTER_US);  // start centered/neutral
}

// ----------------------------------------------------------------------
// Control loop constants - new for this project
// ----------------------------------------------------------------------

// TEMPORARY: which computed angle the control loop treats as "the"
// controlled axis, since the physical mounting orientation (and therefore
// which formula is actually "roll" vs "pitch" for this servo's rotation
// axis) hasn't been verified yet. Flip and retest if the servo reacts to the wrong physical motion
constexpr bool CONTROLLED_AXIS_IS_ROLL = true;

constexpr float SETPOINT_DEG = 0.0f;  // hold level

// Proportional gain: microseconds of servo pulse-width correction per
// degree of angle error. UNTUNED placeholder, deliberately conservative so
// the first test doesn't slam the servo. Increase gradually while watching
// for oscillation around the setpoint; back off if it overshoots.
constexpr float KP_US_PER_DEG = 5.0f;

// Slew-rate limit: maximum change in commanded pulse width per loop tick
// (LOOP_PERIOD_MS = 50ms). Softens current spikes - mitigating the missing
// decoupling capacitor - by preventing the servo from being commanded to
// move abruptly, at the cost of a slightly slower response to large errors.
constexpr uint16_t MAX_SERVO_STEP_US = 60;

uint16_t slew_limit(uint16_t current_us, uint16_t target_us) {
    if (target_us > current_us && target_us - current_us > MAX_SERVO_STEP_US) {
        return current_us + MAX_SERVO_STEP_US;
    }
    if (target_us < current_us && current_us - target_us > MAX_SERVO_STEP_US) {
        return current_us - MAX_SERVO_STEP_US;
    }
    return target_us;
}

}  // namespace

int main() {
    stdio_init_all();
    cyw43_arch_init();  // needed to drive the Pico Led

    i2c_init(i2c0, I2C_BAUDRATE_HZ);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);

    pwm_setup_servo();

    sleep_ms(100);  // lets the sensor power up before talking to it

    i2c_bus_scan();

    if (mpu6050_probe()) {
        printf("MPU6050 detected, starting.\n");
        mpu6050_init();
    } else {
        printf("MPU6050 NOT detected - check wiring. Retrying in the background.\n");
    }

    float roll = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
    uint16_t servo_pulse_us = SERVO_PULSE_CENTER_US;

    int loop_count = 0;
    bool led_on = false;
    bool sensor_ok = false;

    while (true) {
        RawReading raw;
        if (mpu6050_read_raw(&raw)) {
            if (!sensor_ok) {
                printf("MPU6050 connection (re)established.\n");
                sensor_ok = true;
            }

            float ax = raw.accel_x / ACCEL_SENSITIVITY_LSB_PER_G;
            float ay = raw.accel_y / ACCEL_SENSITIVITY_LSB_PER_G;
            float az = raw.accel_z / ACCEL_SENSITIVITY_LSB_PER_G;
            float gx = raw.gyro_x / GYRO_SENSITIVITY_LSB_PER_DPS;
            float gy = raw.gyro_y / GYRO_SENSITIVITY_LSB_PER_DPS;
            float gz = raw.gyro_z / GYRO_SENSITIVITY_LSB_PER_DPS;

            float accel_roll_deg =
                atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / static_cast<float>(M_PI);
            float accel_pitch_deg = atan2f(ay, az) * 180.0f / static_cast<float>(M_PI);

            roll = COMPLEMENTARY_FILTER_ALPHA * (roll + gy * LOOP_PERIOD_S) +
                   (1.0f - COMPLEMENTARY_FILTER_ALPHA) * accel_roll_deg;
            pitch = COMPLEMENTARY_FILTER_ALPHA * (pitch + gx * LOOP_PERIOD_S) +
                    (1.0f - COMPLEMENTARY_FILTER_ALPHA) * accel_pitch_deg;

            yaw += gz * LOOP_PERIOD_S;
            if (yaw > 180.0f) {
                yaw -= 360.0f;
            } else if (yaw < -180.0f) {
                yaw += 360.0f;
            }

            const float roll_corrected = roll - ROLL_MOUNTING_OFFSET_DEG;
            const float pitch_corrected = pitch - PITCH_MOUNTING_OFFSET_DEG;
            const float controlled_angle_deg = CONTROLLED_AXIS_IS_ROLL ? roll_corrected : pitch_corrected;

            // P-controller: command the servo proportionally to how far off
            // the setpoint the controlled angle currently is.
            const float error_deg = SETPOINT_DEG - controlled_angle_deg;
            float target_pulse_us_f = SERVO_PULSE_CENTER_US + KP_US_PER_DEG * error_deg;
            if (target_pulse_us_f < SERVO_PULSE_MIN_US) {
                target_pulse_us_f = SERVO_PULSE_MIN_US;
            } else if (target_pulse_us_f > SERVO_PULSE_MAX_US) {
                target_pulse_us_f = SERVO_PULSE_MAX_US;
            }
            const uint16_t target_pulse_us = static_cast<uint16_t>(target_pulse_us_f);

            servo_pulse_us = slew_limit(servo_pulse_us, target_pulse_us);
            pwm_set_gpio_level(SERVO_PIN, servo_pulse_us);

            printf("%.2f,%.2f,%.2f,%u\n", roll_corrected, pitch_corrected, yaw, servo_pulse_us);
        } else {
            // Read failed - hold the servo at its last commanded position
            // (no new control update) rather than snapping it anywhere,
            // and keep streaming so the link never looks dead.
            sensor_ok = false;
            printf("ERR\n");
            mpu6050_init();
        }

        if (++loop_count >= LOOPS_PER_HEARTBEAT_TOGGLE) {
            loop_count = 0;
            led_on = !led_on;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
        }

        sleep_ms(LOOP_PERIOD_MS);
    }
}
