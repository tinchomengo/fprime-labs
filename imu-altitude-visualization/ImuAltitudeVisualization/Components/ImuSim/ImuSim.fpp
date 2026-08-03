module ImuAltitudeVisualization {
    @ Simulated IMU/altitude telemetry generator producing smooth synthetic orientation and altitude data
    passive component ImuSim {

        @ Current roll angle, simulated
        telemetry Roll: F64

        @ Current pitch angle, simulated
        telemetry Pitch: F64

        @ Current yaw angle, simulated
        telemetry Yaw: F64

        @ Current altitude, simulated
        telemetry Altitude: F64

        @ Scheduled input port, called by the rate group to trigger a new simulated reading
        sync input port run: Svc.Sched

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables telemetry channels handling
        import Fw.Channel

    }
}