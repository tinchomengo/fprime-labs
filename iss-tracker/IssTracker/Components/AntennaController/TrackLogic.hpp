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
// A target up to AZ_CLAMP_MARGIN_DEG past the mount's azimuth range is sent
// as the limit itself: near the horizon a pass can sit a fraction of a degree
// outside the range for tens of seconds. A target further out is skipped (the
// mount holds its last position) and reported. The same limits are enforced
// again on the Pico.
//
// The mount's zero doesn't have to face north. Its travel is limited to
// +/-AZ_LIMIT_DEG by the servo cable, so for a real pass the whole mount is
// turned to face the middle of that pass's azimuth path, and the compass
// direction it faces is given as mountZeroDeg.
// ======================================================================

#ifndef IssTracker_TrackLogic_HPP
#define IssTracker_TrackLogic_HPP

namespace IssTracker {
namespace TrackLogic {

//! Mount azimuth travel either side of zero; matches the Pico firmware
constexpr double AZ_LIMIT_DEG = 90.0;
constexpr double EL_MAX_DEG = 90.0;

//! How far past AZ_LIMIT_DEG a target is clamped to the limit instead of skipped
constexpr double AZ_CLAMP_MARGIN_DEG = 5.0;

enum class Phase { IDLE, WAITING, TRACKING };

//! What the mount should do this cycle
struct Decision {
    enum class Action { NONE, POINT, STOW } action = Action::NONE;
    double azimuthDeg = 0.0;    //!< Mount azimuth for POINT, -180 to 180
    double elevationDeg = 0.0;  //!< Elevation for POINT, 0 to 90
    Phase nextPhase = Phase::IDLE;
    bool azimuthClamped = false;     //!< Target slightly past the limit, sent as the limit
    bool azimuthOutOfRange = false;  //!< Target skipped: more than the clamp margin past the limit
};

//! Compass azimuth (0 to 360) to mount azimuth (-180 to 180), for a mount
//! whose zero faces compass azimuth \p mountZeroDeg
double mountAzimuth(double compassAzimuthDeg, double mountZeroDeg = 0.0);

//! Decides the mount action for one look angle, for a mount whose zero faces
//! compass azimuth \p mountZeroDeg
Decision decide(Phase phase, double compassAzimuthDeg, double elevationDeg, double mountZeroDeg = 0.0);

}  // namespace TrackLogic
}  // namespace IssTracker

#endif
