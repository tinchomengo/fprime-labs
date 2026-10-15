// ======================================================================
// \title  OrbitMath.cpp
// \author tinchomengo
// \brief  Look angles and pass prediction on top of libsgp4
// ======================================================================

#include "OrbitMath.hpp"

#include <cmath>
#include <cstdio>
#include <exception>

#include <libsgp4/CoordGeodetic.h>
#include <libsgp4/CoordTopocentric.h>
#include <libsgp4/Eci.h>
#include <libsgp4/Observer.h>
#include <libsgp4/TimeSpan.h>
#include <libsgp4/Util.h>

namespace IssTracker {
namespace OrbitMath {

namespace {

constexpr double TICKS_PER_SECOND = 1.0e6;  // libsgp4 DateTime ticks are microseconds

// Coarse step when scanning for a horizon crossing. A pass shorter than this
// can be missed; ISS passes above 0 deg last several minutes.
constexpr double SEARCH_STEP_S = 20.0;

// Horizon crossings and the elevation peak are refined to this precision
constexpr double REFINE_TOLERANCE_S = 0.1;

// Longest an ISS pass can last; bounds the backward search for a pass in progress
constexpr double MAX_PASS_DURATION_S = 20.0 * 60.0;

// Sampling step for the azimuth range of a pass. Near the zenith the azimuth
// can swing ~14 deg/s, so the step must be short to follow it continuously.
constexpr double AZIMUTH_SAMPLE_S = 1.0;

//! Elevation in degrees, or false if SGP4 failed
bool elevationAt(const libsgp4::SGP4& sgp4, const Observer& observer, double unixSeconds, double& elevationDeg) {
    LookAngle angle;
    if (!lookAngle(sgp4, observer, unixSeconds, angle)) {
        return false;
    }
    elevationDeg = angle.elevationDeg;
    return true;
}

//! Bisects a horizon crossing known to lie in [below, above] (or [above, below])
bool refineCrossing(const libsgp4::SGP4& sgp4,
                    const Observer& observer,
                    double belowUnix,
                    double aboveUnix,
                    double& crossingUnix) {
    while (std::fabs(aboveUnix - belowUnix) > REFINE_TOLERANCE_S) {
        const double mid = 0.5 * (belowUnix + aboveUnix);
        double elevation = 0.0;
        if (!elevationAt(sgp4, observer, mid, elevation)) {
            return false;
        }
        if (elevation > 0.0) {
            aboveUnix = mid;
        } else {
            belowUnix = mid;
        }
    }
    crossingUnix = 0.5 * (belowUnix + aboveUnix);
    return true;
}

//! Golden-section search for the elevation peak between AOS and LOS.
//! Elevation rises then falls during a pass, so it has a single maximum.
bool refinePeak(const libsgp4::SGP4& sgp4,
                const Observer& observer,
                double startUnix,
                double endUnix,
                double& peakUnix,
                double& peakElevationDeg) {
    const double invPhi = (std::sqrt(5.0) - 1.0) / 2.0;
    double a = startUnix;
    double b = endUnix;
    double c = b - invPhi * (b - a);
    double d = a + invPhi * (b - a);
    double fc = 0.0;
    double fd = 0.0;
    if (!elevationAt(sgp4, observer, c, fc) || !elevationAt(sgp4, observer, d, fd)) {
        return false;
    }
    while ((b - a) > REFINE_TOLERANCE_S) {
        if (fc > fd) {
            b = d;
            d = c;
            fd = fc;
            c = b - invPhi * (b - a);
            if (!elevationAt(sgp4, observer, c, fc)) {
                return false;
            }
        } else {
            a = c;
            c = d;
            fc = fd;
            d = a + invPhi * (b - a);
            if (!elevationAt(sgp4, observer, d, fd)) {
                return false;
            }
        }
    }
    peakUnix = 0.5 * (a + b);
    return elevationAt(sgp4, observer, peakUnix, peakElevationDeg);
}

//! Azimuth range swept between AOS and LOS, following the azimuth
//! continuously (so a path through north isn't split at 0/360).
bool azimuthRange(const libsgp4::SGP4& sgp4,
                  const Observer& observer,
                  double aosUnix,
                  double losUnix,
                  double& centerDeg,
                  double& spanDeg) {
    LookAngle angle;
    if (!lookAngle(sgp4, observer, aosUnix, angle)) {
        return false;
    }
    double previous = angle.azimuthDeg;
    double unwrapped = previous;
    double lowest = unwrapped;
    double highest = unwrapped;

    for (double t = aosUnix + AZIMUTH_SAMPLE_S;; t += AZIMUTH_SAMPLE_S) {
        const double sampleTime = t < losUnix ? t : losUnix;
        if (!lookAngle(sgp4, observer, sampleTime, angle)) {
            return false;
        }
        double step = angle.azimuthDeg - previous;
        if (step > 180.0) {
            step -= 360.0;
        } else if (step < -180.0) {
            step += 360.0;
        }
        unwrapped += step;
        previous = angle.azimuthDeg;
        lowest = std::fmin(lowest, unwrapped);
        highest = std::fmax(highest, unwrapped);
        if (sampleTime >= losUnix) {
            break;
        }
    }

    centerDeg = std::fmod(0.5 * (lowest + highest), 360.0);
    if (centerDeg < 0.0) {
        centerDeg += 360.0;
    }
    spanDeg = highest - lowest;
    return true;
}

}  // namespace

libsgp4::DateTime toDateTime(double unixSeconds) {
    const double ticks = std::round(unixSeconds * TICKS_PER_SECOND);
    return libsgp4::DateTime(libsgp4::kUNIX_EPOCH + static_cast<int64_t>(ticks));
}

double toUnixSeconds(const libsgp4::DateTime& dateTime) {
    return static_cast<double>(dateTime.Ticks() - libsgp4::kUNIX_EPOCH) / TICKS_PER_SECOND;
}

bool parseUtc(const char* text, double& unixSeconds) {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    char trailing = '\0';
    // %c after the last field catches trailing garbage
    const int fields =
        std::sscanf(text, "%4d-%2d-%2dT%2d:%2d:%2d%c", &year, &month, &day, &hour, &minute, &second, &trailing);
    if (fields != 6) {
        return false;
    }
    if (!libsgp4::DateTime::IsValidYearMonthDay(year, month, day) || hour < 0 || hour > 23 || minute < 0 ||
        minute > 59 || second < 0 || second > 59) {
        return false;
    }
    unixSeconds = toUnixSeconds(libsgp4::DateTime(year, month, day, hour, minute, second));
    return true;
}

void formatUtc(double unixSeconds, char* buffer, std::size_t size) {
    const libsgp4::DateTime dateTime = toDateTime(std::round(unixSeconds));
    std::snprintf(buffer, size, "%04d-%02d-%02dT%02d:%02d:%02d", dateTime.Year(), dateTime.Month(), dateTime.Day(),
                  dateTime.Hour(), dateTime.Minute(), dateTime.Second());
}

bool lookAngle(const libsgp4::SGP4& sgp4, const Observer& observer, double unixSeconds, LookAngle& result) {
    // libsgp4 reports failures (decayed orbit, invalid elements) as exceptions;
    // they stop here so callers only see a return value.
    try {
        const libsgp4::Eci eci = sgp4.FindPosition(toDateTime(unixSeconds));
        libsgp4::Observer site(observer.latitudeDeg, observer.longitudeDeg, observer.altitudeM / 1000.0);
        const libsgp4::CoordTopocentric topo = site.GetLookAngle(eci);
        result.azimuthDeg = libsgp4::Util::RadiansToDegrees(topo.azimuth);
        result.elevationDeg = libsgp4::Util::RadiansToDegrees(topo.elevation);
        result.rangeKm = topo.range;
        result.rangeRateKmS = topo.rangeRate;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

namespace {

//! The first pass after \p startUnix whose AOS is before \p endUnix (or the
//! pass in progress at \p startUnix), whatever its peak elevation
bool findPass(const libsgp4::SGP4& sgp4, const Observer& observer, double startUnix, double endUnix, Pass& result) {
    double elevation = 0.0;
    if (!elevationAt(sgp4, observer, startUnix, elevation)) {
        return false;
    }

    // Already in a pass: step back to the last time it was below the horizon
    double t = startUnix;
    if (elevation > 0.0) {
        const double earliest = startUnix - MAX_PASS_DURATION_S;
        while (elevation > 0.0 && t > earliest) {
            t -= SEARCH_STEP_S;
            if (!elevationAt(sgp4, observer, t, elevation)) {
                return false;
            }
        }
        if (elevation > 0.0) {
            return false;
        }
    }

    // Scan forward for the rising crossing
    double previous = t;
    bool risen = false;
    while (t < endUnix) {
        previous = t;
        t += SEARCH_STEP_S;
        if (!elevationAt(sgp4, observer, t, elevation)) {
            return false;
        }
        if (elevation > 0.0) {
            risen = true;
            break;
        }
    }
    if (!risen) {
        return false;
    }
    if (!refineCrossing(sgp4, observer, previous, t, result.aosUnix)) {
        return false;
    }

    // Scan forward for the setting crossing
    const double passLimit = result.aosUnix + MAX_PASS_DURATION_S;
    while (elevation > 0.0) {
        previous = t;
        t += SEARCH_STEP_S;
        if (t > passLimit || !elevationAt(sgp4, observer, t, elevation)) {
            return false;
        }
    }
    if (!refineCrossing(sgp4, observer, t, previous, result.losUnix)) {
        return false;
    }

    if (!refinePeak(sgp4, observer, result.aosUnix, result.losUnix, result.tcaUnix, result.maxElevationDeg)) {
        return false;
    }

    LookAngle angle;
    if (!lookAngle(sgp4, observer, result.aosUnix, angle)) {
        return false;
    }
    result.aosAzimuthDeg = angle.azimuthDeg;
    if (!lookAngle(sgp4, observer, result.losUnix, angle)) {
        return false;
    }
    result.losAzimuthDeg = angle.azimuthDeg;

    return azimuthRange(sgp4, observer, result.aosUnix, result.losUnix, result.azimuthCenterDeg,
                        result.azimuthSpanDeg);
}

}  // namespace

bool findNextPass(const libsgp4::SGP4& sgp4,
                  const Observer& observer,
                  double startUnix,
                  double searchHours,
                  Pass& result,
                  double minElevationDeg) {
    const double endUnix = startUnix + searchHours * 3600.0;
    double searchFrom = startUnix;
    while (searchFrom < endUnix) {
        if (!findPass(sgp4, observer, searchFrom, endUnix, result)) {
            return false;
        }
        if (result.maxElevationDeg >= minElevationDeg) {
            return true;
        }
        // Too low: carry on from just after it sets
        searchFrom = result.losUnix + 1.0;
    }
    return false;
}

bool extractTleLines(const char* text, std::string& line1, std::string& line2) {
    bool found1 = false;
    bool found2 = false;
    const char* cursor = text;
    while (*cursor != '\0') {
        const char* end = cursor;
        while (*end != '\0' && *end != '\n') {
            end++;
        }
        std::string line(cursor, static_cast<std::size_t>(end - cursor));
        const std::string::size_type last = line.find_last_not_of(" \t\r");
        line.erase(last == std::string::npos ? 0 : last + 1);

        if (line.compare(0, 2, "1 ") == 0) {
            line1 = line;
            found1 = true;
        } else if (line.compare(0, 2, "2 ") == 0) {
            line2 = line;
            found2 = true;
        }
        cursor = (*end == '\n') ? end + 1 : end;
    }
    return found1 && found2;
}

}  // namespace OrbitMath
}  // namespace IssTracker
