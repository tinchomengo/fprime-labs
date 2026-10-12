module IssTracker {

    @ Propagates a satellite (the ISS) from a TLE with SGP4 and computes its
    @ azimuth, elevation and range as seen by the observer, once per rate
    @ group cycle. The orbit math lives in OrbitMath.cpp (on top of the
    @ dnwrnr/sgp4 library) and is unit tested against Skyfield.
    @
    @ Propagation time is the real clock by default. SIM_CLOCK_START replaces
    @ it with a simulated clock that starts at a chosen UTC time and runs at a
    @ chosen rate, so a past or future pass can be replayed through the same
    @ code path as a live one.
    passive component Sgp4Propagator {

        # ----------------------------------------------------------------------
        # Ports
        # ----------------------------------------------------------------------

        @ Called by the rate group to propagate and publish the current look angle
        sync input port schedIn: Svc.Sched

        @ The look angle computed each cycle, for the AntennaController
        output port lookAngleOut: LookAngle

        # ----------------------------------------------------------------------
        # Commands
        # ----------------------------------------------------------------------

        @ Loads a TLE, replacing any TLE already loaded. Trailing whitespace on
        @ each line is ignored.
        sync command LOAD_TLE(
            line1: string size 80 @< TLE line 1 (69 characters)
            line2: string size 80 @< TLE line 2 (69 characters)
        )

        @ Switches to a simulated clock: propagation time becomes the given UTC
        @ start time plus the elapsed wall-clock time multiplied by the rate
        sync command SIM_CLOCK_START(
            startUtc: string size 24 @< Start time, YYYY-MM-DDTHH:MM:SS (UTC)
            rate: F64 @< Simulated seconds per wall-clock second, 1 = real time (max 100)
        )

        @ Switches back to the real clock
        sync command SIM_CLOCK_STOP

        @ Searches the next 24 h, from the current propagation time, for the
        @ next pass above the horizon. Reports the pass in progress if the
        @ satellite is already above the horizon.
        sync command PREDICT_PASS

        # ----------------------------------------------------------------------
        # Parameters
        # ----------------------------------------------------------------------

        @ Observer geodetic latitude, degrees, north positive (-90 to 90)
        param OBSERVER_LAT_DEG: F64 default -34.60

        @ Observer longitude, degrees, east positive (-180 to 180)
        param OBSERVER_LON_DEG: F64 default -58.38

        @ Observer height above the ellipsoid, metres (-500 to 10000)
        param OBSERVER_ALT_M: F64 default 25.0

        # ----------------------------------------------------------------------
        # Telemetry
        # ----------------------------------------------------------------------

        @ Azimuth, degrees clockwise from north (0-360)
        telemetry Azimuth: F64

        @ Elevation above the horizon, degrees (negative when below it)
        telemetry Elevation: F64

        @ Slant range from the observer, km
        telemetry RangeKm: F64

        @ Range rate, km/s, positive when moving away
        telemetry RangeRateKmS: F64

        @ UTC time the look angle was computed for (real or simulated clock)
        telemetry PropagationTime: string size 20

        @ Propagation time minus the TLE epoch, hours
        telemetry TleAgeHours: F64

        @ True while the simulated clock is in use
        telemetry SimClockActive: bool

        # ----------------------------------------------------------------------
        # Events
        # ----------------------------------------------------------------------

        @ A TLE was parsed and loaded
        event TleLoaded(
            satNumber: U32 @< NORAD catalogue number
            epoch: string size 20 @< TLE epoch, UTC
            ageHours: F64 @< Propagation time minus the epoch
        ) \
            severity activity high \
            format "Loaded TLE for satellite {}, epoch {} UTC, {.1f} h old"

        @ A TLE could not be parsed
        event TleRejected(
            reason: string size 80 @< Parser error
        ) \
            severity warning low \
            format "TLE rejected: {}"

        @ Propagation or pass prediction was requested with no TLE loaded
        event NoTleLoaded() \
            severity warning low \
            format "No TLE loaded; send LOAD_TLE first"

        @ SGP4 could not propagate to the requested time
        event PropagationFailed(
            propagationTime: string size 20 @< Propagation time, UTC
        ) \
            severity warning high \
            format "SGP4 could not propagate to {} UTC" \
            throttle 5

        @ The simulated clock was started
        event SimClockStarted(
            startUtc: string size 20 @< Simulated start time
            rate: F64 @< Simulated seconds per wall-clock second
        ) \
            severity activity high \
            format "Simulated clock started at {} UTC, {.1f}x"

        @ The real clock is in use again
        event SimClockStopped() \
            severity activity high \
            format "Simulated clock stopped; using the real clock"

        @ SIM_CLOCK_START received a start time it could not parse
        event InvalidSimStart(
            startUtc: string size 24 @< Text received
        ) \
            severity warning low \
            format "Invalid simulated clock start '{}': expected YYYY-MM-DDTHH:MM:SS"

        @ SIM_CLOCK_START received a rate outside (0, 100]
        event InvalidSimRate(
            rate: F64 @< Rate received
        ) \
            severity warning low \
            format "Invalid simulated clock rate {}: must be greater than 0 and at most 100"

        @ The next pass was found
        event PassPredicted(
            aos: string size 20 @< Rises above the horizon, UTC
            aosAzimuth: F64 @< Azimuth at AOS, degrees
            tca: string size 20 @< Highest elevation, UTC
            maxElevation: F64 @< Elevation at TCA, degrees
            los: string size 20 @< Sets below the horizon, UTC
            losAzimuth: F64 @< Azimuth at LOS, degrees
        ) \
            severity activity high \
            format "Next pass: AOS {} UTC at az {.1f}, TCA {} UTC at el {.1f}, LOS {} UTC at az {.1f}"

        @ No pass was found in the search window
        event NoPassFound(
            searchHours: U32 @< Length of the search window
        ) \
            severity activity high \
            format "No pass found in the next {} h"

        @ The observer location changed
        event ObserverSet(
            latitude: F64 @< Degrees
            longitude: F64 @< Degrees
            altitude: F64 @< Metres
        ) \
            severity activity high \
            format "Observer at {.4f}, {.4f}, {.0f} m"

        @ The observer parameters are out of range; the previous location is kept
        event ObserverInvalid(
            latitude: F64 @< Degrees
            longitude: F64 @< Degrees
            altitude: F64 @< Metres
        ) \
            severity warning low \
            format "Observer {.4f}, {.4f}, {.0f} m is out of range; keeping the previous location"

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

        @Port to set the value of a parameter
        param set port prmSetOut

    }
}
