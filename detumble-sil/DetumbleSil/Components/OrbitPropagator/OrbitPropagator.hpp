// ======================================================================
// \title  OrbitPropagator.hpp
// \author tinchomengo
// \brief  hpp file for OrbitPropagator component implementation class
// ======================================================================

#ifndef DetumbleSil_OrbitPropagator_HPP
#define DetumbleSil_OrbitPropagator_HPP

#include "DetumbleSil/Components/OrbitPropagator/OrbitPropagatorComponentAc.hpp"

namespace DetumbleSil {

class OrbitPropagator final : public OrbitPropagatorComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct OrbitPropagator object
    OrbitPropagator(const char* const compName  //!< The component name
    );

    //! Destroy OrbitPropagator object
    ~OrbitPropagator();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Scheduled input port, called by the rate group to advance the orbit one cycle
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    Fw::Time m_lastTime;     //!< Time of the previous run_handler call, for computing dt
    bool m_started = false;  //!< Whether m_lastTime has been set yet

    F64 m_simTime_s = 0.0;     //!< Accumulated simulated (time-accelerated) elapsed time
    U32 m_lastOrbitCount = 0;  //!< Whole orbits already announced via OrbitCompleted

    // Kepler's third law derivatives of ORBIT_RADIUS_KM/MU_EARTH_KM3_S2 -
    // constant for this circular orbit, so computed once rather than every cycle.
    F64 m_meanMotion_rad_s;  //!< Constant angular rate, rad/s
    F64 m_period_s;          //!< Orbital period, s
    F64 m_velocity_km_s;     //!< Constant orbital speed, km/s
};

}  // namespace DetumbleSil

#endif
