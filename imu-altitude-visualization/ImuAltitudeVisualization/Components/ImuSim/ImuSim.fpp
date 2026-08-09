module ImuAltitudeVisualization {
    @ Simulated altitude telemetry generator. Roll/Pitch/Yaw used to live here
    @ too, until they moved to ImuReader (real MPU6050 sensor data); Altitude
    @ stays simulated since there's no barometer on the real hardware.
    passive component ImuSim {

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