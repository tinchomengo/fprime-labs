#include "tracker_core.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace tracker {

namespace {

//! Parses a float that must be followed by a space or the end of the line
bool parseFloat(const char*& cursor, float& value) {
    char* end = nullptr;
    value = std::strtof(cursor, &end);
    if (end == cursor || (*end != ' ' && *end != '\0') || !std::isfinite(value)) {
        return false;
    }
    cursor = end;
    return true;
}

int32_t degreesToSteps(float deg) {
    return static_cast<int32_t>(std::lround(deg * STEPS_PER_DEG));
}

}  // namespace

// ---------------- AzimuthProfile ----------------

AzimuthProfile::AzimuthProfile(float tickSeconds)
    : m_tick(tickSeconds),
      m_maxSpeed(AZ_MAX_SPEED_DEG_S * STEPS_PER_DEG),
      m_accel(AZ_ACCEL_DEG_S2 * STEPS_PER_DEG),
      m_minSpeed(AZ_MIN_SPEED_DEG_S * STEPS_PER_DEG) {}

int AzimuthProfile::tick() {
    const int32_t remaining = m_target - m_position;

    // On target: stop. Deceleration starts early enough that the speed here is
    // at most sqrt(2 * accel * 1 step), ~9 deg/s, which a stepper stops from instantly.
    if (remaining == 0) {
        m_velocity = 0.0f;
        m_phase = 0.0f;
        return 0;
    }

    // Distance needed to stop from the current speed: v^2 / (2a)
    const float stoppingSteps = m_velocity * m_velocity / (2.0f * m_accel);
    const bool headingToTarget = (remaining > 0 && m_velocity >= 0.0f) || (remaining < 0 && m_velocity <= 0.0f);
    const float direction = remaining > 0 ? 1.0f : -1.0f;
    const float dv = m_accel * m_tick;

    if (headingToTarget && static_cast<float>(std::abs(remaining)) > stoppingSteps) {
        // Accelerate toward the target, up to the maximum speed
        m_velocity += direction * dv;
        if (std::fabs(m_velocity) > m_maxSpeed) {
            m_velocity = direction * m_maxSpeed;
        }
    } else {
        // Decelerate: either about to arrive, or moving away from the target
        if (m_velocity > 0.0f) {
            m_velocity = std::fmax(0.0f, m_velocity - dv);
        } else if (m_velocity < 0.0f) {
            m_velocity = std::fmin(0.0f, m_velocity + dv);
        }
        if (m_velocity == 0.0f) {
            m_phase = 0.0f;  // a partial step in the old direction doesn't carry into the new one
        }
    }

    // Never stall short of the target: creep the last steps at the minimum speed
    if (remaining != 0 && headingToTarget && std::fabs(m_velocity) < m_minSpeed) {
        m_velocity = direction * m_minSpeed;
    }

    m_phase += std::fabs(m_velocity) * m_tick;
    if (m_phase < 1.0f) {
        return 0;
    }
    m_phase -= 1.0f;

    const int step = m_velocity > 0.0f ? 1 : -1;
    m_position += step;
    return step;
}

void AzimuthProfile::stop() {
    m_target = m_position;
    m_velocity = 0.0f;
    m_phase = 0.0f;
}

void AzimuthProfile::zero() {
    m_position = 0;
    stop();
}

// ---------------- ElevationRamp ----------------

void ElevationRamp::update(float dtSeconds) {
    const float maxChange = EL_MAX_SPEED_DEG_S * dtSeconds;
    const float difference = m_target - m_current;
    if (std::fabs(difference) <= maxChange) {
        m_current = m_target;
    } else {
        m_current += difference > 0.0f ? maxChange : -maxChange;
    }
}

// ---------------- Controller ----------------

const char* stateName(State state) {
    switch (state) {
        case State::DISABLED:
            return "DISABLED";
        case State::TRACKING:
            return "TRACKING";
        case State::STOWING:
            return "STOWING";
        case State::STOWED:
            return "STOWED";
    }
    return "UNKNOWN";
}

Controller::Controller(float tickSeconds) : m_azimuth(tickSeconds), m_elevation(STOW_EL_DEG) {}

void Controller::handleLine(const char* line, uint32_t nowMs, char* reply, std::size_t replySize) {
    // Any line, valid or not, shows the host is alive
    m_lastLineMs = nowMs;

    if (std::strcmp(line, "PING") == 0) {
        std::snprintf(reply, replySize, "OK PING");
    } else if (std::strcmp(line, "ENABLE") == 0) {
        if (m_state == State::DISABLED) {
            m_azimuth.stop();  // hold wherever it is now
            m_elevation.hold();
            m_state = State::TRACKING;
        }
        std::snprintf(reply, replySize, "OK ENABLE");
    } else if (std::strcmp(line, "DISABLE") == 0) {
        disable();
        std::snprintf(reply, replySize, "OK DISABLE");
    } else if (std::strcmp(line, "ZERO_AZ") == 0) {
        if (m_state != State::DISABLED) {
            std::snprintf(reply, replySize, "ERR STATE");
            return;
        }
        m_azimuth.zero();
        std::snprintf(reply, replySize, "OK ZERO_AZ");
    } else if (std::strcmp(line, "STOW") == 0) {
        if (m_state == State::DISABLED) {
            std::snprintf(reply, replySize, "ERR STATE");
            return;
        }
        startStow(false);
        std::snprintf(reply, replySize, "OK STOW");
    } else if (std::strcmp(line, "HEARTBEAT") == 0 || std::strncmp(line, "HEARTBEAT ", 10) == 0) {
        const char* text = line + 9;
        char* end = nullptr;
        const long seconds = std::strtol(text, &end, 10);
        while (end != nullptr && *end == ' ') {
            end++;
        }
        if (end == text || *end != '\0') {
            std::snprintf(reply, replySize, "ERR ARGS");
            return;
        }
        if (seconds < MIN_HEARTBEAT_S || seconds > MAX_HEARTBEAT_S) {
            std::snprintf(reply, replySize, "ERR LIMIT");
            return;
        }
        m_heartbeatTimeoutMs = static_cast<uint32_t>(seconds) * 1000U;
        std::snprintf(reply, replySize, "OK HEARTBEAT");
    } else if (std::strcmp(line, "SET") == 0 || std::strncmp(line, "SET ", 4) == 0) {
        const char* cursor = line + 3;
        float az = 0.0f;
        float el = 0.0f;
        bool valid = parseFloat(cursor, az) && parseFloat(cursor, el);
        while (*cursor == ' ') {
            cursor++;  // tolerate trailing spaces when typing by hand
        }
        if (!valid || *cursor != '\0') {
            std::snprintf(reply, replySize, "ERR ARGS");
            return;
        }
        if (std::fabs(az) > AZ_LIMIT_DEG || el < EL_MIN_DEG || el > EL_MAX_DEG) {
            std::snprintf(reply, replySize, "ERR LIMIT");
            return;
        }
        if (m_state != State::TRACKING && m_state != State::STOWED) {
            std::snprintf(reply, replySize, "ERR STATE");
            return;
        }
        m_azimuth.setTarget(degreesToSteps(az));
        m_elevation.setTarget(el);
        m_state = State::TRACKING;
        std::snprintf(reply, replySize, "OK SET");
    } else {
        std::snprintf(reply, replySize, "ERR UNKNOWN");
    }
}

void Controller::update(uint32_t nowMs, float dtSeconds, char* event, std::size_t eventSize) {
    event[0] = '\0';
    m_elevation.update(dtSeconds);

    if (m_state == State::STOWING && !m_azimuth.moving() && !m_elevation.moving()) {
        if (m_disableWhenStowed) {
            disable();
            std::snprintf(event, eventSize, "EVT DISABLED");
        } else {
            m_state = State::STOWED;
            std::snprintf(event, eventSize, "EVT STOWED");
        }
        return;
    }

    // Unsigned subtraction stays correct when the millisecond counter wraps
    const bool heartbeatLost = (nowMs - m_lastLineMs) > m_heartbeatTimeoutMs;
    if (heartbeatLost && (m_state == State::TRACKING || m_state == State::STOWED ||
                          (m_state == State::STOWING && !m_disableWhenStowed))) {
        startStow(true);
        std::snprintf(event, eventSize, "EVT HEARTBEAT_LOST");
    }
}

void Controller::startStow(bool disableWhenStowed) {
    m_azimuth.setTarget(degreesToSteps(STOW_AZ_DEG));
    m_elevation.setTarget(STOW_EL_DEG);
    m_state = State::STOWING;
    m_disableWhenStowed = disableWhenStowed;
}

void Controller::disable() {
    m_azimuth.stop();
    m_elevation.hold();
    m_state = State::DISABLED;
    m_disableWhenStowed = false;
}

}  // namespace tracker
