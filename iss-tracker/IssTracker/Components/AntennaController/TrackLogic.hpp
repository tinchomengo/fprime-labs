// ======================================================================
// \title  TrackLogic.hpp
// \author tinchomengo
// \brief  Decides what the mount does with each look angle
//
// Plain C++ with no F´ dependencies, so it can be tested on its own.
//
// Phases:
//   IDLE      Not tracking: nothing is sent.
//   WAITING   Satellite below the horizon: the mount waits on the horizon at
//             the satellite's azimuth, so it's already pointing where the
//             pass will rise.
//   TRACKING  Satellite above the horizon: the mount follows it. When it
//             sets, the mount stows and tracking ends.
//
// Any target outside the mount's azimuth range is skipped: the mount holds
// its last position. The same limits are enforced again on the Pico.
// ======================================================================

#ifndef IssTracker_TrackLogic_HPP
#define IssTracker_TrackLogic_HPP

namespace IssTracker {
namespace TrackLogic {

//! Mount azimuth travel either side of zero; matches the Pico firmware
constexpr double AZ_LIMIT_DEG = 90.0;
constexpr double EL_MAX_DEG = 90.0;

enum class Phase { IDLE, WAITING, TRACKING };

//! What the mount should do this cycle
struct Decision {
    enum class Action { NONE, POINT, STOW } action = Action::NONE;
    double azimuthDeg = 0.0;    //!< Mount azimuth for POINT, -180 to 180
    double elevationDeg = 0.0;  //!< Elevation for POINT, 0 to 90
    Phase nextPhase = Phase::IDLE;
    bool azimuthOutOfRange = false;  //!< Target skipped: outside +/-AZ_LIMIT_DEG
};

//! Compass azimuth (0 to 360) to mount azimuth (-180 to 180, 0 = mount zero)
double mountAzimuth(double compassAzimuthDeg);

//! Decides the mount action for one look angle
Decision decide(Phase phase, double compassAzimuthDeg, double elevationDeg);

}  // namespace TrackLogic
}  // namespace IssTracker

#endif
