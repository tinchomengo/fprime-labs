module IssTracker {

    @ Turns the propagated az/el into mount commands within the mount's limits.
    @ The decisions are in TrackLogic.cpp: wait on the horizon at the
    @ satellite's azimuth until it rises, follow it while it's up, and stow
    @ when it sets. Targets slightly past the mount's azimuth range are sent
    @ as the limit; targets further out are skipped.
    passive component AntennaController {

        # ----------------------------------------------------------------------
        # Ports
        # ----------------------------------------------------------------------

        @ Look angle from the Sgp4Propagator, once per rate group cycle
        guarded input port lookAngleIn: LookAngle

        @ Commands for the mount, to the PicoLink
        output port mountOut: MountCommand

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------

        @ Enables the mount and starts tracking the next (or current) pass
        guarded command TRACK_START

        @ Stops tracking and stows the mount. Does nothing if not tracking.
        guarded command TRACK_STOP

        # ----------------------------------------------------------------------
        # Parameters
        # ----------------------------------------------------------------------

        @ Compass direction (degrees from true north, 0 to 360) that the mount's
        @ azimuth zero faces. For a real pass, turn the mount to face the centre
        @ that PREDICT_PASS reports and set it here. 0 for the simulated pass test.
        param MOUNT_ZERO_AZ_DEG: F64 default 0.0

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Tracking phase
        telemetry Phase: TrackPhase

        @ Azimuth last sent to the mount, degrees from mount zero (-180 to 180)
        telemetry TargetAzimuth: F64

        @ Elevation last sent to the mount, degrees
        telemetry TargetElevation: F64

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ Tracking started
        event TrackingStarted() \
            severity activity high \
            format "Tracking started; waiting for the satellite to rise"

        @ Tracking stopped by command
        event TrackingStopped() \
            severity activity high \
            format "Tracking stopped; stowing"

        @ The satellite rose above the horizon
        event PassStarted(
            azimuth: F64 @< Mount azimuth at the start, degrees
        ) \
            severity activity high \
            format "Pass started at mount azimuth {.1f} deg"

        @ The satellite set; the mount stows
        event PassComplete() \
            severity activity high \
            format "Pass complete; stowing"

        @ The mount's zero direction changed
        event MountZeroSet(
            azimuth: F64 @< Compass direction of the mount's zero, degrees
        ) \
            severity activity high \
            format "Mount zero faces {.1f} deg (true north)"

        @ MOUNT_ZERO_AZ_DEG is outside 0 to 360; the previous value is kept
        event MountZeroInvalid(
            azimuth: F64 @< Value received
        ) \
            severity warning low \
            format "Mount zero {} deg is outside 0 to 360; keeping the previous value"

        @ A target was outside the mount's azimuth range and was skipped
        event AzimuthOutOfRange(
            azimuth: F64 @< Mount azimuth requested, degrees
        ) \
            severity warning low \
            format "Mount azimuth {.1f} deg is outside the mount's range; holding position" \
            throttle 5

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

        @ Port to return the value of a parameter
        param get port prmGetOut

        @ Port to set the value of a parameter
        param set port prmSetOut

    }
}
