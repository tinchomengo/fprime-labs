// ======================================================================
// \title  TrackLogic.cpp
// \author tinchomengo
// \brief  Decides what the mount does with each look angle
// ======================================================================

#include "TrackLogic.hpp"

#include <cmath>

namespace IssTracker {
namespace TrackLogic {

double mountAzimuth(double compassAzimuthDeg, double mountZeroDeg) {
    double azimuth = std::fmod(compassAzimuthDeg - mountZeroDeg, 360.0);
    if (azimuth < 0.0) {
        azimuth += 360.0;
    }
    return azimuth > 180.0 ? azimuth - 360.0 : azimuth;
}

Decision decide(Phase phase, double compassAzimuthDeg, double elevationDeg, double mountZeroDeg) {
    Decision decision;
    decision.nextPhase = phase;

    if (phase == Phase::IDLE) {
        return decision;
    }

    const bool aboveHorizon = elevationDeg >= 0.0;

    if (phase == Phase::TRACKING && !aboveHorizon) {
        // The pass is over
        decision.action = Decision::Action::STOW;
        decision.nextPhase = Phase::IDLE;
        return decision;
    }
    if (phase == Phase::WAITING && aboveHorizon) {
        decision.nextPhase = Phase::TRACKING;
    }

    double azimuth = mountAzimuth(compassAzimuthDeg, mountZeroDeg);
    if (std::fabs(azimuth) > AZ_LIMIT_DEG + AZ_CLAMP_MARGIN_DEG) {
        decision.azimuthOutOfRange = true;
        return decision;
    }
    if (std::fabs(azimuth) > AZ_LIMIT_DEG) {
        azimuth = azimuth > 0.0 ? AZ_LIMIT_DEG : -AZ_LIMIT_DEG;
        decision.azimuthClamped = true;
    }

    decision.action = Decision::Action::POINT;
    decision.azimuthDeg = azimuth;
    // Below the horizon the mount waits on it; elevation can't exceed 90
    decision.elevationDeg = aboveHorizon ? std::fmin(elevationDeg, EL_MAX_DEG) : 0.0;
    return decision;
}

}  // namespace TrackLogic
}  // namespace IssTracker
