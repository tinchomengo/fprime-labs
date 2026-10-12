module IssTracker {

    @ Serial link to the Pico actuator node: sends pointing commands, receives
    @ position telemetry. Talks the line protocol defined in
    @ firmware/pico-tracker/tracker_core.hpp over a Drv.LinuxUartDriver.
    @
    @ On the first rate group cycle after the link opens (and again whenever
    @ the Pico reboots) it sets the Pico's heartbeat timeout to
    @ HEARTBEAT_TIMEOUT_S, then sends a line at least once per cycle (PING if
    @ nothing else was sent), so the mount stows and disables itself if this
    @ deployment stops.
    passive component PicoLink {

        # ----------------------------------------------------------------------
        # Ports
        # ----------------------------------------------------------------------

        @ Connection to the serial driver
        import Drv.ByteStreamDriverClient

        @ Mount commands from the AntennaController
        sync input port mountIn: MountCommand

        @ Called by the rate group: keeps the Pico's heartbeat alive
        sync input port schedIn: Svc.Sched

        # ----------------------------------------------------------------------
        # Commands, for operating the mount by hand
        # ----------------------------------------------------------------------

        @ Energizes the stepper
        sync command MOUNT_ENABLE

        @ De-energizes the stepper; it can then be turned by hand
        sync command MOUNT_DISABLE

        @ Moves to the stow position (az 0, el 90)
        sync command MOUNT_STOW

        @ Makes the current azimuth zero; the Pico only accepts it while disabled
        sync command MOUNT_ZERO_AZ

        @ Points the mount
        sync command MOUNT_SET(
            azimuth: F64 @< Degrees from mount zero, within the Pico's limit
            elevation: F64 @< Degrees above the horizon, 0 to 90
        )

        # ----------------------------------------------------------------------
        # Telemetry, from the Pico's TLM lines
        # ----------------------------------------------------------------------

        @ Mount azimuth reported by the Pico, degrees from mount zero
        telemetry Azimuth: F64

        @ Mount elevation reported by the Pico, degrees
        telemetry Elevation: F64

        @ Mount state reported by the Pico
        telemetry State: MountState

        @ ERR replies received from the Pico
        telemetry ErrorCount: U32

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ The serial link to the Pico is open
        event LinkOpened() \
            severity activity high \
            format "Serial link to the Pico is open"

        @ A command was dropped because the serial link isn't open
        event LinkNotOpen() \
            severity warning low \
            format "No serial link to the Pico; command not sent" \
            throttle 1

        @ The serial driver failed to send a line
        event SendFailed() \
            severity warning high \
            format "Could not send to the Pico" \
            throttle 5

        @ The Pico rejected a command
        event MountError(
            reply: string size 40 @< Reply from the Pico
        ) \
            severity warning low \
            format "Pico replied: {}" \
            throttle 10

        @ The Pico reported an event (boot, stowed, heartbeat lost, disabled)
        event MountEvent(
            eventName: string size 40 @< Event name from the Pico
        ) \
            severity activity high \
            format "Pico: {}"

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
