// ======================================================================
// \title  DetumbleSim.hpp
// \author tinchomengo
// \brief  hpp file for DetumbleSim component implementation class
// ======================================================================

#ifndef DetumbleSil_DetumbleSim_HPP
#define DetumbleSil_DetumbleSim_HPP

#include "DetumbleSil/Components/DetumbleSim/DetumbleSimComponentAc.hpp"

namespace DetumbleSil {

class DetumbleSim final : public DetumbleSimComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct DetumbleSim object
    DetumbleSim(const char* const compName  //!< The component name
    );

    //! Destroy DetumbleSim object
    ~DetumbleSim();

  private:
    // ----------------------------------------------------------------------
    // Small local math types - just enough vector/quaternion algebra for
    // the rigid-body integration below, not a general-purpose math library.
    // ----------------------------------------------------------------------

    struct Vec3 {
        F64 x;
        F64 y;
        F64 z;
    };

    struct Quat {
        F64 w;
        F64 x;
        F64 y;
        F64 z;
    };

    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Scheduled input port, called by the rate group to advance the simulation one telemetry cycle
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    // ----------------------------------------------------------------------
    // Command handler implementations
    // ----------------------------------------------------------------------

    //! Handler implementation for command KICK
    void KICK_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                         U32 cmdSeq            //!< The command sequence number
                         ) override;

    // ----------------------------------------------------------------------
    // Simulation internals
    // ----------------------------------------------------------------------

    //! Resets attitude/rate to the fixed initial tumble state. Shared by
    //! construction-time state init and the KICK command.
    void resetToTumble();

    //! Advances (q, omega) forward by one fixed-size physics sub-step,
    //! recomputing the rate-damping control torque at that sub-step's rate.
    //! Returns the control torque used, so the caller can telemeter its
    //! magnitude from the most recent sub-step.
    Vec3 integrateSubstep(F64 dt_s);

    //! Publishes the current (q, omega, mode) state as telemetry, and
    //! raises ModeChanged if mode has just changed.
    void publishTelemetry(F64 lastTorqueMagnitude_Nm);

    // Small vector/quaternion helpers, kept as private statics purely so
    // integrateSubstep() reads as the physics, not the algebra.
    static Vec3 cross(const Vec3& a, const Vec3& b);
    static Quat quatMultiply(const Quat& a, const Quat& b);
    static Quat quatNormalize(const Quat& q);

    Fw::Time m_lastTime;     //!< Time of the previous run_handler call, for computing dt
    bool m_started = false;  //!< Whether m_lastTime has been set yet

    Quat m_q{};                                    //!< Attitude quaternion, body frame to inertial frame
    Vec3 m_omega{};                                //!< Body-frame angular velocity, rad/s
    DetumbleMode m_mode = DetumbleMode::TUMBLING;  //!< Current control mode
};

}  // namespace DetumbleSil

#endif
