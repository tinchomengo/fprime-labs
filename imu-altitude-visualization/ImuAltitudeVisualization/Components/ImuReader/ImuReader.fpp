module ImuAltitudeVisualization {
    @ Reads roll/pitch/yaw from a real MPU6050 sensor, via a Pico
    @ microcontroller streaming CSV lines ("roll,pitch,yaw\n") over USB
    @ serial. Receives raw bytes from a Drv.LinuxUartDriver, line-buffers
    @ them, and parses complete lines into telemetry.
    passive component ImuReader {

        @ Current roll angle, from the MPU6050
        telemetry Roll: F64

        @ Current pitch angle, from the MPU6050
        telemetry Pitch: F64

        @ Current yaw angle, from the MPU6050 (gyro-integrated only - no
        @ magnetometer, so this drifts over time and has no absolute heading)
        telemetry Yaw: F64

        # ----------------------------------------------------------------------
        # Ports to connect to a ByteStreamDriver (synchronous) - see
        # Drv.LinuxUartDriver, which this is meant to be wired to.
        # ----------------------------------------------------------------------
        import Drv.ByteStreamDriverClient

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables telemetry channels handling
        import Fw.Channel

    }
}