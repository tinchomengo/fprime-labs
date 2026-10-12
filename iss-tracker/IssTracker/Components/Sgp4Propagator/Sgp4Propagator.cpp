// ======================================================================
// \title  Sgp4Propagator.cpp
// \author tinchomengo
// \brief  cpp file for Sgp4Propagator component implementation class
// ======================================================================

#include "IssTracker/Components/Sgp4Propagator/Sgp4Propagator.hpp"

#include <exception>
#include <string>
#include <utility>

#include <libsgp4/Tle.h>

namespace IssTracker {

namespace {

constexpr F64 MAX_SIM_RATE = 100.0;

constexpr F64 MIN_ALTITUDE_M = -500.0;
constexpr F64 MAX_ALTITUDE_M = 10000.0;

constexpr F64 SECONDS_PER_HOUR = 3600.0;

constexpr FwSizeType UTC_TEXT_SIZE = 24;  // "YYYY-MM-DDTHH:MM:SS" plus terminator, with margin

//! Command string without trailing spaces, tabs or line endings (left by copy-paste)
std::string trimTrailing(const Fw::CmdStringArg& arg) {
    std::string text(arg.toChar());
    const std::string::size_type end = text.find_last_not_of(" \t\r\n");
    text.erase(end == std::string::npos ? 0 : end + 1);
    return text;
}

}  // namespace

// Out-of-class definition, required in C++14 when the constant is bound to a reference
constexpr U32 Sgp4Propagator::PASS_SEARCH_HOURS;

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Sgp4Propagator ::Sgp4Propagator(const char* const compName) : Sgp4PropagatorComponentBase(compName) {
    // Placeholder until the parameters are loaded; matches the FPP defaults
    this->m_observer = OrbitMath::Observer{-34.60, -58.38, 25.0};
}

Sgp4Propagator ::~Sgp4Propagator() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Sgp4Propagator ::schedIn_handler(FwIndexType portNum, U32 context) {
    this->tlmWrite_SimClockActive(this->m_simActive);

    if (!this->m_sgp4) {
        // Report once, not every cycle
        if (!this->m_noTleReported) {
            this->log_WARNING_LO_NoTleLoaded();
            this->m_noTleReported = true;
        }
        return;
    }

    const F64 now = this->propagationSeconds();
    char timeText[UTC_TEXT_SIZE];
    OrbitMath::formatUtc(now, timeText, sizeof(timeText));

    OrbitMath::LookAngle angle;
    if (!OrbitMath::lookAngle(*this->m_sgp4, this->m_observer, now, angle)) {
        this->log_WARNING_HI_PropagationFailed(Fw::LogStringArg(timeText));
        return;
    }

    this->tlmWrite_Azimuth(angle.azimuthDeg);
    this->tlmWrite_Elevation(angle.elevationDeg);
    this->tlmWrite_RangeKm(angle.rangeKm);
    this->tlmWrite_RangeRateKmS(angle.rangeRateKmS);
    this->tlmWrite_PropagationTime(Fw::TlmString(timeText));
    this->tlmWrite_TleAgeHours((now - this->m_tleEpochSeconds) / SECONDS_PER_HOUR);

    if (this->isConnected_lookAngleOut_OutputPort(0)) {
        this->lookAngleOut_out(0, angle.azimuthDeg, angle.elevationDeg);
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Sgp4Propagator ::LOAD_TLE_cmdHandler(FwOpcodeType opCode,
                                          U32 cmdSeq,
                                          const Fw::CmdStringArg& line1,
                                          const Fw::CmdStringArg& line2) {
    // libsgp4 reports invalid TLEs and unusable elements as exceptions;
    // the current TLE is only replaced once both steps succeed.
    try {
        const libsgp4::Tle tle(trimTrailing(line1), trimTrailing(line2));
        std::unique_ptr<libsgp4::SGP4> sgp4(new libsgp4::SGP4(tle));

        this->m_sgp4 = std::move(sgp4);
        this->m_tleEpochSeconds = OrbitMath::toUnixSeconds(tle.Epoch());
        this->m_noTleReported = false;

        char epochText[UTC_TEXT_SIZE];
        OrbitMath::formatUtc(this->m_tleEpochSeconds, epochText, sizeof(epochText));
        const F64 ageHours = (this->propagationSeconds() - this->m_tleEpochSeconds) / SECONDS_PER_HOUR;
        this->log_ACTIVITY_HI_TleLoaded(static_cast<U32>(tle.NoradNumber()), Fw::LogStringArg(epochText),
                                        ageHours);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    } catch (const std::exception& error) {
        this->log_WARNING_LO_TleRejected(Fw::LogStringArg(error.what()));
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

void Sgp4Propagator ::SIM_CLOCK_START_cmdHandler(FwOpcodeType opCode,
                                                 U32 cmdSeq,
                                                 const Fw::CmdStringArg& startUtc,
                                                 F64 rate) {
    F64 startSeconds = 0.0;
    if (!OrbitMath::parseUtc(startUtc.toChar(), startSeconds)) {
        this->log_WARNING_LO_InvalidSimStart(startUtc);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    // Written as a negated range check so NaN is rejected too
    if (!(rate > 0.0 && rate <= MAX_SIM_RATE)) {
        this->log_WARNING_LO_InvalidSimRate(rate);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }

    this->m_simStartSeconds = startSeconds;
    this->m_simWallStartSeconds = this->wallClockSeconds();
    this->m_simRate = rate;
    this->m_simActive = true;

    char startText[UTC_TEXT_SIZE];
    OrbitMath::formatUtc(startSeconds, startText, sizeof(startText));
    this->log_ACTIVITY_HI_SimClockStarted(Fw::LogStringArg(startText), rate);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Sgp4Propagator ::SIM_CLOCK_STOP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->m_simActive = false;
    this->log_ACTIVITY_HI_SimClockStopped();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Sgp4Propagator ::PREDICT_PASS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (!this->m_sgp4) {
        this->log_WARNING_LO_NoTleLoaded();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }

    OrbitMath::Pass pass;
    if (!OrbitMath::findNextPass(*this->m_sgp4, this->m_observer, this->propagationSeconds(),
                                 static_cast<double>(PASS_SEARCH_HOURS), pass)) {
        // Not finding a pass is a valid answer, not a command failure
        this->log_ACTIVITY_HI_NoPassFound(PASS_SEARCH_HOURS);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }

    char aosText[UTC_TEXT_SIZE];
    char tcaText[UTC_TEXT_SIZE];
    char losText[UTC_TEXT_SIZE];
    OrbitMath::formatUtc(pass.aosUnix, aosText, sizeof(aosText));
    OrbitMath::formatUtc(pass.tcaUnix, tcaText, sizeof(tcaText));
    OrbitMath::formatUtc(pass.losUnix, losText, sizeof(losText));
    this->log_ACTIVITY_HI_PassPredicted(Fw::LogStringArg(aosText), pass.aosAzimuthDeg, Fw::LogStringArg(tcaText),
                                        pass.maxElevationDeg, Fw::LogStringArg(losText), pass.losAzimuthDeg);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Parameter hooks
// ----------------------------------------------------------------------

void Sgp4Propagator ::parametersLoaded() {
    this->updateObserver();
}

void Sgp4Propagator ::parameterUpdated(FwPrmIdType id) {
    this->updateObserver();
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

F64 Sgp4Propagator ::wallClockSeconds() {
    const Fw::Time now = this->getTime();
    return static_cast<F64>(now.getSeconds()) + static_cast<F64>(now.getUSeconds()) * 1.0e-6;
}

F64 Sgp4Propagator ::propagationSeconds() {
    const F64 wall = this->wallClockSeconds();
    if (!this->m_simActive) {
        return wall;
    }
    return this->m_simStartSeconds + (wall - this->m_simWallStartSeconds) * this->m_simRate;
}

void Sgp4Propagator ::updateObserver() {
    Fw::ParamValid valid;
    const F64 latitude = this->paramGet_OBSERVER_LAT_DEG(valid);
    const F64 longitude = this->paramGet_OBSERVER_LON_DEG(valid);
    const F64 altitude = this->paramGet_OBSERVER_ALT_M(valid);

    // Written as negated range checks so NaN is rejected too
    if (!(latitude >= -90.0 && latitude <= 90.0) || !(longitude >= -180.0 && longitude <= 180.0) ||
        !(altitude >= MIN_ALTITUDE_M && altitude <= MAX_ALTITUDE_M)) {
        this->log_WARNING_LO_ObserverInvalid(latitude, longitude, altitude);
        return;
    }

    this->m_observer = OrbitMath::Observer{latitude, longitude, altitude};
    this->log_ACTIVITY_HI_ObserverSet(latitude, longitude, altitude);
}

}  // namespace IssTracker
