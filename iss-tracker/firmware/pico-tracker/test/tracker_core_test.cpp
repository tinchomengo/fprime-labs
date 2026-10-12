// Host test for tracker_core: motion profile, elevation ramp, protocol and
// state machine, simulated at the firmware's step timer (10 kHz) and control
// loop (100 Hz) rates. No pico-sdk needed.
//
// Build and run from firmware/pico-tracker:
//   mkdir -p build && c++ -std=c++17 -I . test/tracker_core_test.cpp tracker_core.cpp -o build/tracker_core_test && ./build/tracker_core_test

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "tracker_core.hpp"

using namespace tracker;

namespace {

int g_failures = 0;

#define CHECK(condition)                                                    \
    do {                                                                    \
        if (!(condition)) {                                                 \
            std::printf("FAIL line %d: %s\n", __LINE__, #condition);        \
            g_failures++;                                                   \
        }                                                                   \
    } while (0)

constexpr float TICK_S = 1.0e-4f;  // step timer period in main.cpp
constexpr int TICKS_PER_CONTROL = 100;  // 10 ms control loop

int32_t steps(float deg) {
    return static_cast<int32_t>(std::lround(deg * STEPS_PER_DEG));
}

//! Runs a Controller the way main.cpp does
struct Rig {
    Controller controller{TICK_S};
    uint32_t nowMs = 0;
    int tickInControl = 0;
    std::string events;

    std::string send(const char* line) {
        char reply[64];
        controller.handleLine(line, nowMs, reply, sizeof(reply));
        return reply;
    }

    void run(float seconds) {
        const long ticks = std::lround(seconds / TICK_S);
        for (long i = 0; i < ticks; i++) {
            controller.stepTick();
            if (++tickInControl == TICKS_PER_CONTROL) {
                tickInControl = 0;
                nowMs += 10;
                char event[64];
                controller.update(nowMs, 0.01f, event, sizeof(event));
                if (event[0] != '\0') {
                    events += event;
                    events += ";";
                }
            }
        }
    }

    bool sawEvent(const char* name) const { return events.find(name) != std::string::npos; }
};

// ---------------- Motion profile ----------------

void testProfileReachesTargetWithinLimits() {
    AzimuthProfile profile(TICK_S);
    const int32_t target = steps(90.0f);
    profile.setTarget(target);

    long ticks = 0;
    long lastStepTick = -1;
    long minGap = 1L << 30;
    float maxSpeed = 0.0f;
    int32_t maxPosition = 0;
    while (profile.moving() && ticks < 100000) {
        const int step = profile.tick();
        ticks++;
        maxSpeed = std::fmax(maxSpeed, std::fabs(profile.velocity()));
        maxPosition = std::max(maxPosition, profile.position());
        if (step != 0) {
            if (lastStepTick >= 0) {
                minGap = std::min(minGap, ticks - lastStepTick);
            }
            lastStepTick = ticks;
        }
    }
    std::printf("0 to 90 deg: %.3f s, max speed %.1f deg/s, closest steps %ld ticks apart\n",
                static_cast<double>(ticks) * TICK_S, static_cast<double>(maxSpeed / STEPS_PER_DEG), minGap);

    CHECK(profile.position() == target);
    CHECK(maxPosition <= target);  // no overshoot
    CHECK(maxSpeed <= AZ_MAX_SPEED_DEG_S * STEPS_PER_DEG + 1e-3f);
    // main.cpp needs a free tick after each STEP pulse and one for a DIR change
    CHECK(minGap >= 3);
    // Accelerate 0.5 s, cruise 0.5 s, decelerate 0.5 s
    CHECK(std::fabs(static_cast<float>(ticks) * TICK_S - 1.5f) < 0.1f);
}

void testProfileReversesThroughZeroSpeed() {
    AzimuthProfile profile(TICK_S);
    profile.setTarget(400);
    for (int i = 0; i < 5000; i++) {
        profile.tick();  // 0.5 s in: at full speed
    }
    profile.setTarget(-400);

    const float oneTickOfAccel = AZ_ACCEL_DEG_S2 * STEPS_PER_DEG * TICK_S;
    float previous = profile.velocity();
    float largestJumpAcrossZero = 0.0f;
    int directionChanges = 0;
    int lastStep = 1;
    long ticks = 0;
    while (profile.moving() && ticks < 100000) {
        const int step = profile.tick();
        ticks++;
        if (step != 0 && step != lastStep) {
            directionChanges++;
            lastStep = step;
        }
        if ((previous > 0.0f) != (profile.velocity() > 0.0f)) {
            largestJumpAcrossZero = std::fmax(largestJumpAcrossZero, std::fabs(profile.velocity() - previous));
        }
        previous = profile.velocity();
    }

    CHECK(profile.position() == -400);
    CHECK(directionChanges == 1);
    // The velocity passes through zero instead of jumping across it
    CHECK(largestJumpAcrossZero <= AZ_MIN_SPEED_DEG_S * STEPS_PER_DEG + oneTickOfAccel + 1e-3f);
}

void testProfileSmallMovesAndTracking() {
    AzimuthProfile profile(TICK_S);
    profile.setTarget(2);
    for (int i = 0; i < 100000 && profile.moving(); i++) {
        profile.tick();
    }
    CHECK(profile.position() == 2);

    // Follow a target moving at 2.7 deg/s (the simulated pass's fastest
    // azimuth rate), updated once per second like the F´ deployment does
    float azimuth = -80.0f;
    profile.setTarget(steps(azimuth));
    for (int i = 0; i < 30000; i++) {
        profile.tick();
    }
    float maxLag = 0.0f;
    for (int second = 0; second < 60; second++) {
        azimuth += 2.7f;
        profile.setTarget(steps(azimuth));
        for (int i = 0; i < 10000; i++) {
            profile.tick();
        }
        maxLag = std::fmax(maxLag, std::fabs(azimuth - static_cast<float>(profile.position()) / STEPS_PER_DEG));
    }
    std::printf("tracking 2.7 deg/s with 1 Hz targets: lag before each update %.3f deg\n",
                static_cast<double>(maxLag));
    CHECK(maxLag < 0.3f);
}

// ---------------- Protocol ----------------

void testProtocolCommandsAndLimits() {
    Rig rig;
    CHECK(rig.send("PING") == "OK PING");
    CHECK(rig.send("SET 10 10") == "ERR STATE");  // disabled after boot
    CHECK(rig.send("STOW") == "ERR STATE");
    CHECK(rig.send("ZERO_AZ") == "OK ZERO_AZ");

    CHECK(rig.send("ENABLE") == "OK ENABLE");
    CHECK(rig.controller.state() == State::TRACKING);
    CHECK(rig.controller.driverEnabled());
    CHECK(rig.send("ZERO_AZ") == "ERR STATE");  // only while disabled

    CHECK(rig.send("SET 90.5 10") == "ERR LIMIT");
    CHECK(rig.send("SET -91 10") == "ERR LIMIT");
    CHECK(rig.send("SET 10 -0.1") == "ERR LIMIT");
    CHECK(rig.send("SET 10 90.1") == "ERR LIMIT");
    CHECK(rig.send("SET") == "ERR ARGS");
    CHECK(rig.send("SET 10") == "ERR ARGS");
    CHECK(rig.send("SET 10 20x") == "ERR ARGS");
    CHECK(rig.send("SET 10 20 30") == "ERR ARGS");
    CHECK(rig.send("SET nan 20") == "ERR ARGS");
    CHECK(rig.send("SET inf 20") == "ERR ARGS");
    CHECK(rig.send("set 10 20") == "ERR UNKNOWN");
    CHECK(rig.send("FOO") == "ERR UNKNOWN");
    CHECK(rig.send("SET  10 20  ") == "OK SET");  // extra spaces from hand-typing

    CHECK(rig.send("SET -90 0") == "OK SET");
    rig.run(3.0f);
    CHECK(std::fabs(rig.controller.azimuthDeg() + 90.0f) < 0.01f);
    CHECK(rig.controller.elevationDeg() == 0.0f);
}

void testStow() {
    Rig rig;
    rig.send("ENABLE");
    rig.send("SET -90 0");
    rig.run(3.0f);

    CHECK(rig.send("STOW") == "OK STOW");
    CHECK(rig.controller.state() == State::STOWING);
    CHECK(rig.send("SET 0 0") == "ERR STATE");  // no SET until stowed
    rig.run(3.0f);
    CHECK(rig.controller.state() == State::STOWED);
    CHECK(rig.sawEvent("EVT STOWED"));
    CHECK(std::fabs(rig.controller.azimuthDeg() - STOW_AZ_DEG) < 0.01f);
    CHECK(rig.controller.elevationDeg() == STOW_EL_DEG);

    CHECK(rig.send("SET 45 45") == "OK SET");  // leaves the stow
    CHECK(rig.controller.state() == State::TRACKING);
}

void testHeartbeatDefault() {
    Rig rig;
    rig.send("ENABLE");
    rig.send("SET 45 45");
    for (int i = 0; i < 20; i++) {
        rig.run(1.0f);
        rig.send("PING");  // a host that keeps talking
    }
    CHECK(rig.controller.state() == State::TRACKING);

    rig.run(59.9f);
    CHECK(rig.controller.state() == State::TRACKING);
    rig.run(0.2f);
    CHECK(rig.sawEvent("EVT HEARTBEAT_LOST"));
    rig.run(3.0f);
    CHECK(rig.sawEvent("EVT DISABLED"));
    CHECK(rig.controller.state() == State::DISABLED);
    CHECK(!rig.controller.driverEnabled());
    CHECK(std::fabs(rig.controller.azimuthDeg() - STOW_AZ_DEG) < 0.01f);
    CHECK(rig.controller.elevationDeg() == STOW_EL_DEG);

    // Disabled: no more heartbeat events
    rig.events.clear();
    rig.run(70.0f);
    CHECK(rig.events.empty());
}

void testHeartbeatCommand() {
    Rig rig;
    CHECK(rig.controller.heartbeatTimeoutMs() == DEFAULT_HEARTBEAT_TIMEOUT_MS);
    CHECK(rig.send("HEARTBEAT") == "ERR ARGS");
    CHECK(rig.send("HEARTBEAT 2.5") == "ERR ARGS");
    CHECK(rig.send("HEARTBEAT x") == "ERR ARGS");
    CHECK(rig.send("HEARTBEATS 3") == "ERR UNKNOWN");
    CHECK(rig.send("HEARTBEAT 0") == "ERR LIMIT");
    CHECK(rig.send("HEARTBEAT -3") == "ERR LIMIT");
    CHECK(rig.send("HEARTBEAT 601") == "ERR LIMIT");
    CHECK(rig.send("HEARTBEAT 3") == "OK HEARTBEAT");
    CHECK(rig.controller.heartbeatTimeoutMs() == 3000U);

    rig.send("ENABLE");
    rig.send("SET 30 30");
    rig.run(2.9f);
    CHECK(rig.controller.state() == State::TRACKING);
    rig.run(0.2f);
    CHECK(rig.sawEvent("EVT HEARTBEAT_LOST"));
    rig.run(3.0f);
    CHECK(rig.controller.state() == State::DISABLED);
}

void testDisableHoldsPosition() {
    Rig rig;
    rig.send("ENABLE");
    rig.send("SET 80 10");
    rig.run(0.3f);
    const float azimuthMidMove = rig.controller.azimuthDeg();
    CHECK(rig.send("DISABLE") == "OK DISABLE");
    rig.run(1.0f);
    CHECK(rig.controller.state() == State::DISABLED);
    CHECK(std::fabs(rig.controller.azimuthDeg() - azimuthMidMove) < 0.3f);
}

void testElevationRampRate() {
    Rig rig;
    rig.send("ENABLE");
    rig.send("SET 0 0");
    rig.run(1.0f);
    // 90 to 0 deg at 45 deg/s: halfway after 1 s
    CHECK(std::fabs(rig.controller.elevationDeg() - 45.0f) < 0.5f);
}

}  // namespace

int main() {
    testProfileReachesTargetWithinLimits();
    testProfileReversesThroughZeroSpeed();
    testProfileSmallMovesAndTracking();
    testProtocolCommandsAndLimits();
    testStow();
    testHeartbeatDefault();
    testHeartbeatCommand();
    testDisableHoldsPosition();
    testElevationRampRate();

    if (g_failures != 0) {
        std::printf("%d FAILED\n", g_failures);
        return 1;
    }
    std::printf("ALL PASSED\n");
    return 0;
}
