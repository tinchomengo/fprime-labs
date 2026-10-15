// Tracker logic for the Pico 2 W actuator node: line protocol, state machine,
// azimuth motion profile and elevation ramp. Plain C++ with no pico-sdk
// dependencies, so it can be compiled and tested on a computer.
//
// Protocol (ASCII lines over USB serial, terminated by \n or \r):
//
//   PING           -> OK PING
//   ENABLE         -> OK ENABLE    energizes the stepper and holds position
//   DISABLE        -> OK DISABLE   de-energizes the stepper (it can be turned by hand)
//   ZERO_AZ        -> OK ZERO_AZ   current azimuth becomes 0 (only while disabled)
//   SET <az> <el>  -> OK SET       absolute target in degrees: az within
//                                  +/-AZ_LIMIT_DEG of zero, el 0 to 90
//   STOW           -> OK STOW      moves to az 0, el 90
//   HEARTBEAT <s>  -> OK HEARTBEAT heartbeat timeout in whole seconds, 1 to 600
//
//   Errors: ERR UNKNOWN, ERR ARGS, ERR LIMIT, ERR STATE, ERR TOO_LONG
//   Events: EVT BOOT, EVT STOWED, EVT HEARTBEAT_LOST, EVT DISABLED
//   Telemetry (5 Hz): TLM <az> <el> <state>
//
// States: DISABLED (stepper off), TRACKING (follows SET), STOWING, STOWED.
// Any received line counts as a heartbeat. If none arrives within the
// heartbeat timeout while the stepper is enabled, the mount stows and then
// disables itself. The timeout starts at DEFAULT_HEARTBEAT_TIMEOUT_MS, long
// enough to type commands by hand; the F´ deployment shortens it with HEARTBEAT.

#ifndef TRACKER_CORE_HPP
#define TRACKER_CORE_HPP

#include <cstddef>
#include <cstdint>

namespace tracker {

// ---------------- Limits and rates ----------------

// Azimuth travel either side of zero. Limited by the servo cable; raise only
// after adding a slack loop.
constexpr float AZ_LIMIT_DEG = 90.0f;
constexpr float EL_MIN_DEG = 0.0f;
constexpr float EL_MAX_DEG = 90.0f;

constexpr float STOW_AZ_DEG = 0.0f;
constexpr float STOW_EL_DEG = 90.0f;

constexpr float STEPS_PER_DEG = 200.0f * 8.0f / 360.0f;  // 1.8 deg/step, 1/8 microstepping

// Azimuth speeds. 90 deg/s is the rate verified on the bring-up firmware; an
// ISS pass needs at most ~14 deg/s even near overhead.
constexpr float AZ_MAX_SPEED_DEG_S = 90.0f;
constexpr float AZ_ACCEL_DEG_S2 = 180.0f;
constexpr float AZ_MIN_SPEED_DEG_S = 2.0f;  // creep speed for the last steps to the target

constexpr float EL_MAX_SPEED_DEG_S = 45.0f;  // same as the bring-up servo ramp

// Heartbeat timeout after boot. The mount only moves on SET, so a long
// timeout can't cause motion; it bounds how long the stepper stays energized
// with nobody talking to it.
constexpr uint32_t DEFAULT_HEARTBEAT_TIMEOUT_MS = 60000;
constexpr long MIN_HEARTBEAT_S = 1;
constexpr long MAX_HEARTBEAT_S = 600;

constexpr std::size_t MAX_LINE_LENGTH = 64;

// ---------------- Azimuth motion profile ----------------

//! Trapezoidal velocity profile in microsteps, advanced at a fixed tick.
//! Accelerates toward the target, cruises at the maximum speed, and
//! decelerates so it stops on the target without overshooting.
class AzimuthProfile {
  public:
    explicit AzimuthProfile(float tickSeconds);

    //! Advances one tick. Returns +1 or -1 if a step must be taken now, else 0.
    int tick();

    void setTarget(int32_t steps) { m_target = steps; }
    int32_t target() const { return m_target; }
    int32_t position() const { return m_position; }
    float velocity() const { return m_velocity; }

    //! True while the target hasn't been reached or the motor is still moving
    bool moving() const { return m_position != m_target || m_velocity != 0.0f; }

    //! Stops immediately and holds the current position (used when the driver is disabled)
    void stop();

    //! Makes the current position zero (used by ZERO_AZ)
    void zero();

  private:
    float m_tick;
    float m_maxSpeed;  // microsteps/s
    float m_accel;     // microsteps/s^2
    float m_minSpeed;  // microsteps/s

    int32_t m_position = 0;
    int32_t m_target = 0;
    float m_velocity = 0.0f;  // microsteps/s, signed
    float m_phase = 0.0f;     // fraction of the next step accumulated
};

// ---------------- Elevation ramp ----------------

//! Moves the commanded elevation toward its target at a limited rate
class ElevationRamp {
  public:
    explicit ElevationRamp(float startDeg) : m_current(startDeg), m_target(startDeg) {}

    void update(float dtSeconds);
    void setTarget(float deg) { m_target = deg; }
    void hold() { m_target = m_current; }
    float current() const { return m_current; }
    bool moving() const { return m_current != m_target; }

  private:
    float m_current;
    float m_target;
};

// ---------------- Controller ----------------

enum class State { DISABLED, TRACKING, STOWING, STOWED };

const char* stateName(State state);

//! Protocol and state machine. Not thread-safe: the caller must keep the
//! step timer from running AzimuthProfile::tick() during these calls.
class Controller {
  public:
    explicit Controller(float tickSeconds);

    //! Handles one received line and writes the reply (without newline) into \p reply
    void handleLine(const char* line, uint32_t nowMs, char* reply, std::size_t replySize);

    //! Advances the elevation ramp and checks stow completion and the heartbeat.
    //! Writes an event (without newline) into \p event, or an empty string.
    void update(uint32_t nowMs, float dtSeconds, char* event, std::size_t eventSize);

    State state() const { return m_state; }
    uint32_t heartbeatTimeoutMs() const { return m_heartbeatTimeoutMs; }
    bool driverEnabled() const { return m_state != State::DISABLED; }
    float azimuthDeg() const { return static_cast<float>(m_azimuth.position()) / STEPS_PER_DEG; }
    float elevationDeg() const { return m_elevation.current(); }

    //! Called from the step timer
    int stepTick() { return m_azimuth.tick(); }

  private:
    void startStow(bool disableWhenStowed);
    void disable();

    AzimuthProfile m_azimuth;
    ElevationRamp m_elevation;
    State m_state = State::DISABLED;
    bool m_disableWhenStowed = false;
    uint32_t m_lastLineMs = 0;
    uint32_t m_heartbeatTimeoutMs = DEFAULT_HEARTBEAT_TIMEOUT_MS;
};

}  // namespace tracker

#endif
