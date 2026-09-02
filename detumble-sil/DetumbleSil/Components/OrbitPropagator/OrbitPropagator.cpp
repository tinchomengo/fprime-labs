// ======================================================================
// \title  OrbitPropagator.cpp
// \author tinchomengo
// \brief  cpp file for OrbitPropagator component implementation class
// ======================================================================

#include "DetumbleSil/Components/OrbitPropagator/OrbitPropagator.hpp"
#include <cmath>

namespace DetumbleSil {

namespace {

constexpr F64 PI = 3.14159265358979323846;
constexpr F64 DEG_TO_RAD = PI / 180.0;
constexpr F64 RAD_TO_DEG = 180.0 / PI;

// Earth's standard gravitational parameter (mu = G*M_earth), km^3/s^2 - a
// real, widely-tabulated constant, not a made-up value.
constexpr F64 MU_EARTH_KM3_S2 = 398600.4418;
constexpr F64 EARTH_RADIUS_KM = 6371.0;

// LEO altitude, chosen round for readability - not tied to any specific
// mission. Orbital radius and (via Kepler's third law, computed at
// construction below, not hardcoded) the orbital period both follow from
// this and MU_EARTH_KM3_S2.
constexpr F64 ALTITUDE_KM = 500.0;
constexpr F64 ORBIT_RADIUS_KM = EARTH_RADIUS_KM + ALTITUDE_KM;

// Orbital plane tilt relative to the equator - set to the ISS's real
// inclination (51.6 degrees) purely as an authentic reference value, not
// because this orbit is meant to represent the ISS. RAAN and argument of
// periapsis are both fixed at 0 (the ascending node sits on the reference
// X axis) - a real orbit has six independent elements; this demo only
// varies one (inclination) to keep a first pass legible. Circular only:
// eccentricity, and therefore a varying altitude/true-vs-mean-anomaly
// distinction, is stage 2 of the project's orbital-mechanics roadmap (see
// ../../../ROADMAP.md) - for a circular orbit, mean anomaly, true anomaly,
// and argument of latitude all coincide, so OrbitAngle below stands in for
// all three.
constexpr F64 INCLINATION_DEG = 51.6;

// The real orbital period at 500km altitude is ~94.5 minutes - far too
// slow to watch. This compresses simulated orbital time relative to wall
// time so a full orbit takes ~8 minutes instead (120x, then 24x, were both
// tried first and still moved too fast to comfortably watch/interact
// with); it is a visualization speed-up only, not part of the physics.
constexpr F64 TIME_ACCELERATION = 12.0;

// Caps how much elapsed wall time a single `run` call will ever advance
// simulated time by, so the first call (or a debugger pause/stall) can't
// produce a huge single jump in orbital position.
constexpr F64 MAX_CYCLE_DT_S = 1.0;

}  // namespace

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

OrbitPropagator ::OrbitPropagator(const char* const compName) : OrbitPropagatorComponentBase(compName) {
    // Kepler's third law: for a circular orbit the mean motion (angular
    // rate) is constant, n = sqrt(mu / r^3), and the period is 2*pi/n -
    // both derived here from ORBIT_RADIUS_KM/MU_EARTH_KM3_S2, not hardcoded.
    this->m_meanMotion_rad_s = sqrt(MU_EARTH_KM3_S2 / (ORBIT_RADIUS_KM * ORBIT_RADIUS_KM * ORBIT_RADIUS_KM));
    this->m_period_s = 2.0 * PI / this->m_meanMotion_rad_s;
    // Tangential speed for a circular orbit: v = omega * r (equivalently sqrt(mu/r) via vis-viva).
    this->m_velocity_km_s = this->m_meanMotion_rad_s * ORBIT_RADIUS_KM;
}

OrbitPropagator ::~OrbitPropagator() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void OrbitPropagator ::run_handler(FwIndexType portNum, U32 context) {
    Fw::Time now = this->getTime();
    if (!this->m_started) {
        this->m_lastTime = now;
        this->m_started = true;
    }

    F64 dt_s = static_cast<F64>(Fw::Time::sub(now, this->m_lastTime));
    this->m_lastTime = now;

    if (dt_s < 0.0) {
        dt_s = 0.0;
    } else if (dt_s > MAX_CYCLE_DT_S) {
        dt_s = MAX_CYCLE_DT_S;
    }

    this->m_simTime_s += dt_s * TIME_ACCELERATION;

    const F64 angle_rad = fmod(this->m_meanMotion_rad_s * this->m_simTime_s, 2.0 * PI);

    // Position in the orbital plane, then tilted about the line of nodes
    // (fixed along X here) by the inclination to place it in an
    // Earth-Centered Inertial (ECI) frame.
    const F64 xOrbital_km = ORBIT_RADIUS_KM * cos(angle_rad);
    const F64 yOrbital_km = ORBIT_RADIUS_KM * sin(angle_rad);
    const F64 inclination_rad = INCLINATION_DEG * DEG_TO_RAD;

    const F64 positionX_km = xOrbital_km;
    const F64 positionY_km = yOrbital_km * cos(inclination_rad);
    const F64 positionZ_km = yOrbital_km * sin(inclination_rad);

    this->tlmWrite_PositionX(positionX_km);
    this->tlmWrite_PositionY(positionY_km);
    this->tlmWrite_PositionZ(positionZ_km);
    this->tlmWrite_Altitude(ORBIT_RADIUS_KM - EARTH_RADIUS_KM);
    this->tlmWrite_OrbitAngle(angle_rad * RAD_TO_DEG);
    this->tlmWrite_OrbitalPeriodS(this->m_period_s);
    this->tlmWrite_OrbitalVelocity(this->m_velocity_km_s);

    const F64 orbitCount = this->m_simTime_s / this->m_period_s;
    this->tlmWrite_OrbitCount(orbitCount);

    const auto wholeOrbits = static_cast<U32>(floor(orbitCount));
    if (wholeOrbits > this->m_lastOrbitCount) {
        this->m_lastOrbitCount = wholeOrbits;
        this->log_ACTIVITY_LO_OrbitCompleted(wholeOrbits);
    }
}

}  // namespace DetumbleSil
