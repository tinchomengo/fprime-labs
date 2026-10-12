module IssTracker {

    @ Where the satellite is in the observer's sky
    port LookAngle(
        azimuthDeg: F64 @< Degrees clockwise from north, 0 to 360
        elevationDeg: F64 @< Degrees above the horizon, negative below it
    )

    @ What the mount is asked to do
    enum MountAction {
        ENABLE @< Energize the stepper and hold position
        DISABLE @< De-energize the stepper
        SET @< Point at the given azimuth and elevation
        STOW @< Go to the stow position (az 0, el 90)
    }

    @ A command for the mount. Azimuth and elevation are only used by SET.
    port MountCommand(
        mountAction: MountAction @< What to do
        azimuthDeg: F64 @< Mount azimuth, degrees from zero, -180 to 180
        elevationDeg: F64 @< Degrees above the horizon
    )

    @ Tracking phase of the AntennaController
    enum TrackPhase {
        IDLE @< Not tracking
        WAITING @< Tracking started; satellite below the horizon
        TRACKING @< Satellite above the horizon and being followed
    }

    @ Mount state reported by the Pico
    enum MountState {
        UNKNOWN @< No telemetry received yet
        DISABLED @< Stepper de-energized
        TRACKING @< Following SET commands
        STOWING @< Moving to the stow position
        STOWED @< Holding the stow position
    }

}
