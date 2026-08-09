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

constexpr F64 ALTITUDE_BASE_M = 100.0;
constexpr F64 ALTITUDE_AMPLITUDE_M = 20.0;
constexpr F64 ALTITUDE_PERIOD_S = 8.0;

// Set to false while using ImuReader's real-sensor rotation mapping
// remember to set totrue to resume the sine-wave simulation.
constexpr bool ALTITUDE_ANIMATION_ENABLED = false;
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

    const F64 altitude = ALTITUDE_ANIMATION_ENABLED
                              ? ALTITUDE_BASE_M + ALTITUDE_AMPLITUDE_M * sin(2.0 * PI * t / ALTITUDE_PERIOD_S)
                              : ALTITUDE_BASE_M;

    this->tlmWrite_Altitude(altitude);
}

}  // namespace ImuAltitudeVisualization
