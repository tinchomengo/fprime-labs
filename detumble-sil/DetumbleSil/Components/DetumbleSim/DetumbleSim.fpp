module DetumbleSil {

    @ Detumble control mode: whether the controller currently sees the body
    @ as still tumbling (full-authority damping torque commanded) or settled
    @ (angular rate below DetumbleSim.cpp's STABLE_OMEGA_THRESHOLD_DEG_S).
    enum DetumbleMode {
        TUMBLING
        STABLE
    }

    @ Simulates a torque-free rigid body (Euler's rotational equation +
    @ quaternion kinematics) under a rate-damping control torque - a
    @ simplified stand-in for reaction wheels/thrusters, not a real actuator
    @ or sensor model. Integrates its own sub-steps every cycle (see
    @ DetumbleSim.cpp) so the physics stays accurate independent of how
    @ often `run` is actually called; only the resulting state is telemetered.
    passive component DetumbleSim {

        @ Re-tumbles the body: resets attitude to identity and angular
        @ velocity to a fixed, deliberately dramatic multi-axis rate, so the
        @ detumble sequence can be re-triggered on demand (e.g. to re-take a
        @ demo recording) without restarting the deployment.
        sync command KICK

        @ Attitude quaternion, body frame to inertial frame, scalar-first
        telemetry Qw: F64
        telemetry Qx: F64
        telemetry Qy: F64
        telemetry Qz: F64

        @ Body-frame angular velocity components, deg/s
        telemetry OmegaX: F64
        telemetry OmegaY: F64
        telemetry OmegaZ: F64

        @ |omega|, deg/s - the scalar the controller (and the HUD) tracks to decide TUMBLING vs STABLE
        telemetry OmegaMagnitude: F64

        @ Commanded control torque magnitude, N*m - saturates while tumbling, decays to ~0 once stable
        telemetry ControlTorqueMagnitude: F64

        @ Rotational kinetic energy, Joules (0.5 * omega . I . omega) - not conserved here, since
        @ the control torque actively removes it; watching it decay alongside OmegaMagnitude is
        @ the same detumble story told in energy terms instead of rate.
        telemetry RotationalKineticEnergy: F64

        @ Current control mode
        telemetry Mode: DetumbleMode

        @ Emitted whenever Mode changes
        event ModeChanged(mode: DetumbleMode) severity activity high id 0 format "Detumble mode changed to {}"

        @ Scheduled input port, called by the rate group to advance the simulation one telemetry cycle
        sync input port run: Svc.Sched

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channels handling
        import Fw.Channel

    }
}
