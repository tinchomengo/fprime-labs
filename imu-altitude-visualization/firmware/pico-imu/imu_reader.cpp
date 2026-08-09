// Reads roll/pitch/yaw from MPU6050 over I2C (i2c0),
// and streams them as CSV text lines over USB
// (serial: "roll,pitch,yaw\n", once per loop iteration)
// SDA to GPIO0
// SCL to GPIO1

#include <math.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

namespace {

constexpr uint8_t MPU6050_ADDR = 0x68;
constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t REG_WHO_AM_I = 0x75;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t WHO_AM_I_EXPECTED = 0x68;

constexpr uint I2C_SDA_PIN = 0;
constexpr uint I2C_SCL_PIN = 1;
constexpr uint I2C_BAUDRATE_HZ = 400000;  // 400kHz ("fast mode")

// A loose physical connection stuck the bus mid-transaction
// i2c_*_blocking calls have no timeout and can hang forever so every I2C call here uses the
// timeout variants instead, and callers check the return from that

constexpr uint32_t I2C_TIMEOUT_US = 50000;  // 50ms

// Sensitivity for the sensor's default full-scale ranges (+/-2g,
// +/-250 deg/s), which is what it powers up with

constexpr float ACCEL_SENSITIVITY_LSB_PER_G = 16384.0f;
constexpr float GYRO_SENSITIVITY_LSB_PER_DPS = 131.0f;

// Complementary filter blend factor for roll/pitch: how much to trust the
// gyro's smooth-but-drifting rate vs. the accelerometer's noisy-but-absolute gravity reference.

// Closer to 1.0 = smoother, but slower to correct drift.
constexpr float COMPLEMENTARY_FILTER_ALPHA = 0.98f;

// TEMPORARY! For hardware limitations, the MPU6050 currently sits mounted
// at a fixed tilt on the protoboard rather than flat (soldering pending)

// These are the roll/pitch readings observed at that resting angle
// (subtracting them makes the current physical resting position read as roll=0, pitch=0

constexpr float ROLL_MOUNTING_OFFSET_DEG = -49.0f;
constexpr float PITCH_MOUNTING_OFFSET_DEG = -11.0f;

constexpr uint32_t LOOP_PERIOD_MS = 50;  // 20Hz sensor sampling/outputrate
constexpr float LOOP_PERIOD_S = LOOP_PERIOD_MS / 1000.0f;

constexpr uint32_t HEARTBEAT_PERIOD_MS = 500;  //  aprox 1Hz LED blink
constexpr int LOOPS_PER_HEARTBEAT_TOGGLE = HEARTBEAT_PERIOD_MS / LOOP_PERIOD_MS;

struct RawReading {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
};

// Writes 1 'dummy' byte to every valid 7-bit I2C address and
// reports which ones ACK. 0-length writes aren't allowed by this SDK (asserts on len == 0)
// so a single throwaway byte stands in for the
// "does anything answer here" check a typical bus scan does.

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

// Returns true if sensor answered on the bus at all at startup
// (independent of whether init/read later succeed)
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

    // Each value is a big-endian 16-bit signed reading. Bytes 6-7 (between
    // accel and gyro) are temperature, intentionally unused here
    out->accel_x = static_cast<int16_t>(data[0] << 8 | data[1]);
    out->accel_y = static_cast<int16_t>(data[2] << 8 | data[3]);
    out->accel_z = static_cast<int16_t>(data[4] << 8 | data[5]);
    out->gyro_x = static_cast<int16_t>(data[8] << 8 | data[9]);
    out->gyro_y = static_cast<int16_t>(data[10] << 8 | data[11]);
    out->gyro_z = static_cast<int16_t>(data[12] << 8 | data[13]);
    return true;
}

}  // namespace

int main() {
    stdio_init_all();
    cyw43_arch_init();  // needed to drive the Pico Led

    i2c_init(i2c0, I2C_BAUDRATE_HZ);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);

    sleep_ms(100);  // lets the sensor power up before talking to it

    // report what's on the bus at all (which address, if any)
    // before assuming sensor lives at the standard 0x68
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

            // Angle from gravity's direction. absolute, but noisy and only valid when not accelerating
            // roll/pitch formulas (and which gyro axis drives each) are
            // swapped from the textbook versions on purpose: empirically,
            // this MPU6050's physical mounting has its X/Y axes rotated
            // 90 deg relative to this board's roll/pitch axes (tested from schematic)
            float accel_roll_deg =
                atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / static_cast<float>(M_PI);
            float accel_pitch_deg = atan2f(ay, az) * 180.0f / static_cast<float>(M_PI);

            roll = COMPLEMENTARY_FILTER_ALPHA * (roll + gy * LOOP_PERIOD_S) +
                   (1.0f - COMPLEMENTARY_FILTER_ALPHA) * accel_roll_deg;
            pitch = COMPLEMENTARY_FILTER_ALPHA * (pitch + gx * LOOP_PERIOD_S) +
                    (1.0f - COMPLEMENTARY_FILTER_ALPHA) * accel_pitch_deg;

            // No magnetometer, so yaw has no absolute reference to correct against
            // this is a pure gyro integration and will eventually drift unlike roll/pitch above
            yaw += gz * LOOP_PERIOD_S;
            if (yaw > 180.0f) {
                yaw -= 360.0f;
            } else if (yaw < -180.0f) {
                yaw += 360.0f;
            }

            printf("%.2f,%.2f,%.2f\n", roll - ROLL_MOUNTING_OFFSET_DEG, pitch - PITCH_MOUNTING_OFFSET_DEG, yaw);
        } else {
            // Read failed (definitely loose connection)
            // Keep streaming and skip the angle update
            // and try re-waking the sensor in case it dropped and came back in sleeping state
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
