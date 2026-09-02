// ======================================================================
// \title  DetumbleSim.cpp
// \author tinchomengo
// \brief  cpp file for DetumbleSim component implementation class
// ======================================================================

#include "DetumbleSil/Components/DetumbleSim/DetumbleSim.hpp"
#include <cmath>

namespace DetumbleSil {

namespace {

constexpr F64 PI = 3.14159265358979323846;
constexpr F64 DEG_TO_RAD = PI / 180.0;
constexpr F64 RAD_TO_DEG = 180.0 / PI;

// Diagonal body-frame inertia tensor, kg*m^2 - roughly 3U-cubesat-scale,
// deliberately asymmetric (Ix != Iy != Iz) so the free (uncontrolled) tumble
// is genuine multi-axis rigid-body motion rather than a clean single-axis
// spin. Not sourced from any real spacecraft design.
constexpr F64 INERTIA_X_KGM2 = 0.050;
constexpr F64 INERTIA_Y_KGM2 = 0.065;
constexpr F64 INERTIA_Z_KGM2 = 0.090;

// Rate-damping control law: torque = -CONTROL_GAIN * omega, saturated to
// MAX_TORQUE_NM. This is a simplified stand-in for reaction wheels or
// thrusters, not a magnetorquer/B-dot law - there's no magnetic-field model
// here. Gain and saturation are UNTUNED/illustrative, chosen only to
// visibly bring INITIAL_OMEGA_*_DEG_S to rest over roughly a demo-length
// window without overshoot or instability.
constexpr F64 CONTROL_GAIN_NM_PER_RAD_S = 0.10;
constexpr F64 MAX_TORQUE_NM = 0.05;

// Fixed, deliberately dramatic initial multi-axis tumble - not randomized,
// so a KICK reproduces the exact same demo take every time.
constexpr F64 INITIAL_OMEGA_X_DEG_S = 130.0;
constexpr F64 INITIAL_OMEGA_Y_DEG_S = -95.0;
constexpr F64 INITIAL_OMEGA_Z_DEG_S = 160.0;

// Below this |omega|, the body is considered detumbled - an arbitrary demo
// threshold, not a mission requirement.
constexpr F64 STABLE_OMEGA_THRESHOLD_DEG_S = 2.0;

// Fixed-size physics sub-step. `run` fires once per telemetry cycle (see
// the deployment's rate group wiring), which is far coarser than these
// dynamics can tolerate in a single Euler step - so every `run` call
// sub-steps forward in PHYSICS_SUBSTEP_S chunks, recomputing the control
// torque at each one, and only the final state is telemetered.
constexpr F64 PHYSICS_SUBSTEP_S = 0.005;

// Caps how much elapsed wall time a single `run` call will ever integrate,
// so the first call (or a debugger pause/stall) can't demand an unbounded
// number of sub-steps.
constexpr F64 MAX_CYCLE_DT_S = 1.0;

F64 clampTorque(F64 value_Nm) {
    if (value_Nm > MAX_TORQUE_NM) {
        return MAX_TORQUE_NM;
    }
    if (value_Nm < -MAX_TORQUE_NM) {
        return -MAX_TORQUE_NM;
    }
    return value_Nm;
}

}  // namespace

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

DetumbleSim ::DetumbleSim(const char* const compName) : DetumbleSimComponentBase(compName) {
    this->resetToTumble();
}

DetumbleSim ::~DetumbleSim() {}

// ----------------------------------------------------------------------
// Vector / quaternion helpers
// ----------------------------------------------------------------------

DetumbleSim::Vec3 DetumbleSim ::cross(const Vec3& a, const Vec3& b) {
    return Vec3{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

// Hamilton product a*b
DetumbleSim::Quat DetumbleSim ::quatMultiply(const Quat& a, const Quat& b) {
    return Quat{
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    };
}

DetumbleSim::Quat DetumbleSim ::quatNormalize(const Quat& q) {
    const F64 norm = sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (norm < 1.0e-12) {
        // Degenerate (should never happen from a unit-start integration) -
        // fall back to identity rather than dividing by ~0.
        return Quat{1.0, 0.0, 0.0, 0.0};
    }
    return Quat{q.w / norm, q.x / norm, q.y / norm, q.z / norm};
}

// ----------------------------------------------------------------------
// Simulation internals
// ----------------------------------------------------------------------

void DetumbleSim ::resetToTumble() {
    this->m_q = Quat{1.0, 0.0, 0.0, 0.0};
    this->m_omega = Vec3{
        INITIAL_OMEGA_X_DEG_S * DEG_TO_RAD,
        INITIAL_OMEGA_Y_DEG_S * DEG_TO_RAD,
        INITIAL_OMEGA_Z_DEG_S * DEG_TO_RAD,
    };
    this->m_mode = DetumbleMode::TUMBLING;
}

DetumbleSim::Vec3 DetumbleSim ::integrateSubstep(F64 dt_s) {
    const Vec3 torque{
        clampTorque(-CONTROL_GAIN_NM_PER_RAD_S * this->m_omega.x),
        clampTorque(-CONTROL_GAIN_NM_PER_RAD_S * this->m_omega.y),
        clampTorque(-CONTROL_GAIN_NM_PER_RAD_S * this->m_omega.z),
    };

    // Euler's rotational equation for a torque-free-except-control rigid
    // body: I*omega_dot = torque - omega x (I*omega)
    const Vec3 Iomega{
        INERTIA_X_KGM2 * this->m_omega.x,
        INERTIA_Y_KGM2 * this->m_omega.y,
        INERTIA_Z_KGM2 * this->m_omega.z,
    };
    const Vec3 gyroscopic = cross(this->m_omega, Iomega);

    this->m_omega.x += dt_s * (torque.x - gyroscopic.x) / INERTIA_X_KGM2;
    this->m_omega.y += dt_s * (torque.y - gyroscopic.y) / INERTIA_Y_KGM2;
    this->m_omega.z += dt_s * (torque.z - gyroscopic.z) / INERTIA_Z_KGM2;

    // Quaternion kinematics: qdot = 0.5 * q (x) [0, omega] (body-frame omega)
    const Quat omegaQuat{0.0, this->m_omega.x, this->m_omega.y, this->m_omega.z};
    const Quat qdot = quatMultiply(this->m_q, omegaQuat);

    this->m_q.w += dt_s * 0.5 * qdot.w;
    this->m_q.x += dt_s * 0.5 * qdot.x;
    this->m_q.y += dt_s * 0.5 * qdot.y;
    this->m_q.z += dt_s * 0.5 * qdot.z;
    this->m_q = quatNormalize(this->m_q);

    return torque;
}

void DetumbleSim ::publishTelemetry(F64 lastTorqueMagnitude_Nm) {
    this->tlmWrite_Qw(this->m_q.w);
    this->tlmWrite_Qx(this->m_q.x);
    this->tlmWrite_Qy(this->m_q.y);
    this->tlmWrite_Qz(this->m_q.z);

    const F64 omegaX_deg = this->m_omega.x * RAD_TO_DEG;
    const F64 omegaY_deg = this->m_omega.y * RAD_TO_DEG;
    const F64 omegaZ_deg = this->m_omega.z * RAD_TO_DEG;
    const F64 omegaMag_deg = sqrt(omegaX_deg * omegaX_deg + omegaY_deg * omegaY_deg + omegaZ_deg * omegaZ_deg);

    this->tlmWrite_OmegaX(omegaX_deg);
    this->tlmWrite_OmegaY(omegaY_deg);
    this->tlmWrite_OmegaZ(omegaZ_deg);
    this->tlmWrite_OmegaMagnitude(omegaMag_deg);
    this->tlmWrite_ControlTorqueMagnitude(lastTorqueMagnitude_Nm);

    const F64 kineticEnergy_J =
        0.5 * (INERTIA_X_KGM2 * this->m_omega.x * this->m_omega.x + INERTIA_Y_KGM2 * this->m_omega.y * this->m_omega.y +
               INERTIA_Z_KGM2 * this->m_omega.z * this->m_omega.z);
    this->tlmWrite_RotationalKineticEnergy(kineticEnergy_J);

    const DetumbleMode::T newMode =
        (omegaMag_deg <= STABLE_OMEGA_THRESHOLD_DEG_S) ? DetumbleMode::STABLE : DetumbleMode::TUMBLING;
    if (newMode != this->m_mode) {
        this->m_mode = newMode;
        this->log_ACTIVITY_HI_ModeChanged(this->m_mode);
    }
    this->tlmWrite_Mode(this->m_mode);
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void DetumbleSim ::run_handler(FwIndexType portNum, U32 context) {
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

    const auto numSubsteps = static_cast<int>(dt_s > 0.0 ? ceil(dt_s / PHYSICS_SUBSTEP_S) : 0);

    Vec3 lastTorque{0.0, 0.0, 0.0};
    if (numSubsteps > 0) {
        const F64 substep_dt = dt_s / static_cast<F64>(numSubsteps);
        for (int i = 0; i < numSubsteps; ++i) {
            lastTorque = this->integrateSubstep(substep_dt);
        }
    }

    const F64 lastTorqueMagnitude_Nm =
        sqrt(lastTorque.x * lastTorque.x + lastTorque.y * lastTorque.y + lastTorque.z * lastTorque.z);
    this->publishTelemetry(lastTorqueMagnitude_Nm);
}

// ----------------------------------------------------------------------
// Command handler implementations
// ----------------------------------------------------------------------

void DetumbleSim ::KICK_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->resetToTumble();
    // Always announce, even if the mode label (TUMBLING) happens to be
    // unchanged from before the kick - this is the operator-visible marker
    // of "a new tumble was just commanded," not just a mode-label edge.
    this->log_ACTIVITY_HI_ModeChanged(this->m_mode);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace DetumbleSil
