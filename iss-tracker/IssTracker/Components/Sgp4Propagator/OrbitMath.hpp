// ======================================================================
// \title  OrbitMath.hpp
// \author tinchomengo
// \brief  Look angles and pass prediction on top of libsgp4
//
// Plain C++ with no F´ dependencies, so it can be tested on its own and
// compared directly against an independent implementation (Skyfield).
//
// Time is passed around as Unix seconds (UTC). Unix time and libsgp4's
// DateTime both count every day as exactly 86400 s, so converting between
// them is a fixed offset. Going through a leap-second-aware calendar
// instead would shift the ISS by ~27 s along its orbit (~200 km).
//
// Elevations are geometric: no atmospheric refraction is applied.
// ======================================================================

#ifndef IssTracker_OrbitMath_HPP
#define IssTracker_OrbitMath_HPP

#include <cstddef>

#include <libsgp4/DateTime.h>
#include <libsgp4/SGP4.h>

namespace IssTracker {
namespace OrbitMath {

//! Observer position on the WGS-72 ellipsoid used by libsgp4
struct Observer {
    double latitudeDeg;   //!< Geodetic latitude, degrees, north positive
    double longitudeDeg;  //!< Longitude, degrees, east positive
    double altitudeM;     //!< Height above the ellipsoid, metres
};

//! Where the satellite is in the observer's sky
struct LookAngle {
    double azimuthDeg;     //!< 0-360, clockwise from north
    double elevationDeg;   //!< Above the horizon, -90 to 90
    double rangeKm;        //!< Slant range
    double rangeRateKmS;   //!< Positive when moving away
};

//! One pass above the horizon (elevation > 0)
struct Pass {
    double aosUnix;           //!< Acquisition of signal: rises above the horizon
    double tcaUnix;           //!< Time of closest approach: highest elevation
    double losUnix;           //!< Loss of signal: sets below the horizon
    double maxElevationDeg;   //!< Elevation at TCA
    double aosAzimuthDeg;     //!< Azimuth at AOS
    double losAzimuthDeg;     //!< Azimuth at LOS
};

//! Unix seconds (UTC) to a libsgp4 DateTime
libsgp4::DateTime toDateTime(double unixSeconds);

//! libsgp4 DateTime to Unix seconds (UTC)
double toUnixSeconds(const libsgp4::DateTime& dateTime);

//! Parses "YYYY-MM-DDTHH:MM:SS" (UTC) into Unix seconds
//! \return false if the text is malformed or not a valid date/time
bool parseUtc(const char* text, double& unixSeconds);

//! Formats Unix seconds as "YYYY-MM-DDTHH:MM:SS" (UTC), rounded to the second.
//! \p size must be at least 20.
void formatUtc(double unixSeconds, char* buffer, std::size_t size);

//! Look angle from \p observer to the satellite at \p unixSeconds
//! \return false if SGP4 cannot propagate to that time (e.g. decayed orbit)
bool lookAngle(const libsgp4::SGP4& sgp4, const Observer& observer, double unixSeconds, LookAngle& result);

//! Finds the next pass after \p startUnix, searching up to \p searchHours ahead.
//! If the satellite is already above the horizon at \p startUnix, returns the
//! pass in progress (its AOS is then before \p startUnix).
//! \return false if no pass was found or SGP4 failed during the search
bool findNextPass(const libsgp4::SGP4& sgp4,
                  const Observer& observer,
                  double startUnix,
                  double searchHours,
                  Pass& result);

}  // namespace OrbitMath
}  // namespace IssTracker

#endif
