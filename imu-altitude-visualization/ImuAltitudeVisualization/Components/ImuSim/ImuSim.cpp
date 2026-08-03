// ======================================================================
// \title  ImuSim.cpp
// \author tinchomengo
// \brief  cpp file for ImuSim component implementation class
// ======================================================================

#include "ImuAltitudeVisualization/Components/ImuSim/ImuSim.hpp"
#include <cmath>

namespace ImuAltitudeVisualization {

namespace {
constexpr F64 PI = 3.14159265358979323846;

constexpr F64 ROLL_AMPLITUDE_DEG = 15.0;
constexpr F64 ROLL_PERIOD_S = 6.0;

constexpr F64 PITCH_AMPLITUDE_DEG = 10.0;
constexpr F64 PITCH_PERIOD_S = 4.5;

constexpr F64 YAW_RATE_DEG_PER_S = 10.0;

constexpr F64 ALTITUDE_BASE_M = 100.0;
constexpr F64 ALTITUDE_AMPLITUDE_M = 20.0;
constexpr F64 ALTITUDE_PERIOD_S = 8.0;
}  // namespace

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ImuSim ::ImuSim(const char* const compName) : ImuSimComponentBase(compName) {}

ImuSim ::~ImuSim() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ImuSim ::run_handler(FwIndexType portNum, U32 context) {
    Fw::Time now = this->getTime();
    if (!this->m_started) {
        this->m_startTime = now;
        this->m_started = true;
    }
    const F64 t = static_cast<F64>(Fw::Time::sub(now, this->m_startTime));

    const F64 roll = ROLL_AMPLITUDE_DEG * sin(2.0 * PI * t / ROLL_PERIOD_S);
    const F64 pitch = PITCH_AMPLITUDE_DEG * sin(2.0 * PI * t / PITCH_PERIOD_S);
    F64 yaw = fmod(YAW_RATE_DEG_PER_S * t, 360.0);
    if (yaw < 0.0) {
        yaw += 360.0;
    }
    const F64 altitude = ALTITUDE_BASE_M + ALTITUDE_AMPLITUDE_M * sin(2.0 * PI * t / ALTITUDE_PERIOD_S);

    this->tlmWrite_Roll(roll);
    this->tlmWrite_Pitch(pitch);
    this->tlmWrite_Yaw(yaw);
    this->tlmWrite_Altitude(altitude);
}

}  // namespace ImuAltitudeVisualization
