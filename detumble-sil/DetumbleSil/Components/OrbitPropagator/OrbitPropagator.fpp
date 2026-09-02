module DetumbleSil {

    @ Simulates a circular two-body (Kepler) orbit and propagates the
    @ craft's position along it, time-accelerated for visualization.
    @ Deliberately decoupled from DetumbleSim - orbital motion and attitude
    @ dynamics are separate subsystems in a real spacecraft, and keeping
    @ them as separate F' components mirrors that. See OrbitPropagator.cpp
    @ for exactly which simplifications apply (circular only, no J2/drag,
    @ fixed RAAN/argument of periapsis, time-accelerated) - this is stage 1
    @ of the project's orbital-mechanics roadmap (see ../../../ROADMAP.md).
    passive component OrbitPropagator {

        @ Craft position, Earth-Centered Inertial (ECI) frame, km
        telemetry PositionX: F64
        telemetry PositionY: F64
        telemetry PositionZ: F64

        @ Altitude above a spherical Earth, km - constant for a circular orbit
        telemetry Altitude: F64

        @ Angle traveled since periapsis crossing, degrees (0-360) - for a
        @ circular orbit this is also the true anomaly and the argument of
        @ latitude, since periapsis is undefined
        telemetry OrbitAngle: F64

        @ Orbital period, seconds - derived once at startup from Kepler's
        @ third law (T = 2*pi*sqrt(r^3/mu)), not a fixed constant
        telemetry OrbitalPeriodS: F64

        @ Orbital speed, km/s - constant for a circular orbit (v = mean_motion * radius);
        @ ~7.6 km/s at this orbit's 500km altitude, the real speed a LEO craft moves at
        telemetry OrbitalVelocity: F64

        @ Fractional orbits completed since startup (simulated time / period)
        telemetry OrbitCount: F64

        @ Emitted each time OrbitCount crosses a whole number
        event OrbitCompleted(count: U32) severity activity low id 0 format "Completed orbit #{}"

        @ Scheduled input port, called by the rate group to advance the orbit one cycle
        sync input port run: Svc.Sched

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channels handling
        import Fw.Channel

    }
}
