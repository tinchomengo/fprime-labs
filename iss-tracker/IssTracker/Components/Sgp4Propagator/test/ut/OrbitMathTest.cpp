// ======================================================================
// \title  OrbitMathTest.cpp
// \author tinchomengo
// \brief  Tests for OrbitMath against Skyfield reference values
// ======================================================================

#include <cmath>
#include <cstring>

#include <gtest/gtest.h>
#include <libsgp4/SGP4.h>
#include <libsgp4/Tle.h>

#include "IssTracker/Components/Sgp4Propagator/OrbitMath.hpp"
#include "OracleData.hpp"

namespace IssTracker {

namespace {

const libsgp4::SGP4& issPropagator() {
    static const libsgp4::SGP4 sgp4(libsgp4::Tle(Oracle::TLE_LINE1, Oracle::TLE_LINE2));
    return sgp4;
}

double parsed(const char* text) {
    double unixSeconds = 0.0;
    EXPECT_TRUE(OrbitMath::parseUtc(text, unixSeconds)) << text;
    return unixSeconds;
}

void expectMatchesOracle(const OrbitMath::Observer& observer, const Oracle::LookSample* samples, std::size_t count) {
    for (std::size_t i = 0; i < count; i++) {
        OrbitMath::LookAngle angle;
        ASSERT_TRUE(OrbitMath::lookAngle(issPropagator(), observer, parsed(samples[i].utc), angle));
        const double separation = Oracle::skySeparationDeg(angle.azimuthDeg, angle.elevationDeg,
                                                           samples[i].azimuthDeg, samples[i].elevationDeg);
        EXPECT_LT(separation, Oracle::SKY_TOLERANCE_DEG) << samples[i].utc;
        EXPECT_NEAR(angle.rangeKm, samples[i].rangeKm, Oracle::RANGE_TOLERANCE_KM) << samples[i].utc;
    }
}

void expectPassMatchesOracle(const OrbitMath::Pass& pass, const Oracle::PassReference& reference) {
    EXPECT_NEAR(pass.aosUnix, reference.aosUnix, Oracle::PASS_TIME_TOLERANCE_S);
    EXPECT_NEAR(pass.tcaUnix, reference.tcaUnix, Oracle::PASS_TIME_TOLERANCE_S);
    EXPECT_NEAR(pass.losUnix, reference.losUnix, Oracle::PASS_TIME_TOLERANCE_S);
    EXPECT_NEAR(pass.maxElevationDeg, reference.maxElevationDeg, Oracle::MAX_ELEVATION_TOLERANCE_DEG);
}

const OrbitMath::Observer VIRTUAL_70{Oracle::VIRTUAL_70_LAT, Oracle::VIRTUAL_70_LON, Oracle::VIRTUAL_70_ALT_M};
const OrbitMath::Observer BUENOS_AIRES{Oracle::BUENOS_AIRES_LAT, Oracle::BUENOS_AIRES_LON, Oracle::BUENOS_AIRES_ALT_M};

}  // namespace

// ----------------------------------------------------------------------
// Time conversion
// ----------------------------------------------------------------------

TEST(OrbitMathTime, UnixEpochAndKnownDates) {
    EXPECT_DOUBLE_EQ(OrbitMath::toUnixSeconds(libsgp4::DateTime(1970, 1, 1, 0, 0, 0)), 0.0);
    EXPECT_DOUBLE_EQ(OrbitMath::toUnixSeconds(libsgp4::DateTime(2000, 1, 1, 0, 0, 0)), 946684800.0);
    // Leap day, and a date after 27 leap seconds: Unix time ignores them, and so must we
    EXPECT_DOUBLE_EQ(OrbitMath::toUnixSeconds(libsgp4::DateTime(2024, 2, 29, 12, 0, 0)), 1709208000.0);
    EXPECT_DOUBLE_EQ(OrbitMath::toUnixSeconds(libsgp4::DateTime(2026, 10, 10, 17, 46, 50)), 1791654410.0);
}

TEST(OrbitMathTime, RoundTripKeepsMicroseconds) {
    const double unixSeconds = 1791654410.25;
    EXPECT_NEAR(OrbitMath::toUnixSeconds(OrbitMath::toDateTime(unixSeconds)), unixSeconds, 1.0e-6);
}

TEST(OrbitMathTime, TleEpoch) {
    const libsgp4::Tle tle(Oracle::TLE_LINE1, Oracle::TLE_LINE2);
    EXPECT_NEAR(OrbitMath::toUnixSeconds(tle.Epoch()), Oracle::TLE_EPOCH_UNIX, 1.0e-3);
}

TEST(OrbitMathTime, ParseValid) {
    double unixSeconds = 0.0;
    ASSERT_TRUE(OrbitMath::parseUtc("2026-10-10T17:35:00", unixSeconds));
    EXPECT_DOUBLE_EQ(unixSeconds, 1791653700.0);
    ASSERT_TRUE(OrbitMath::parseUtc("2026-10-11T22:14:27", unixSeconds));
    EXPECT_DOUBLE_EQ(unixSeconds, 1791756867.0);
    ASSERT_TRUE(OrbitMath::parseUtc("2024-02-29T12:00:00", unixSeconds));
    EXPECT_DOUBLE_EQ(unixSeconds, 1709208000.0);
}

TEST(OrbitMathTime, ParseRejectsMalformed) {
    double unixSeconds = 0.0;
    EXPECT_FALSE(OrbitMath::parseUtc("", unixSeconds));
    EXPECT_FALSE(OrbitMath::parseUtc("2026-10-10 17:35:00", unixSeconds));   // space instead of T
    EXPECT_FALSE(OrbitMath::parseUtc("2026-10-10T17:35:00Z", unixSeconds));  // trailing text
    EXPECT_FALSE(OrbitMath::parseUtc("2026-10-10T17:35", unixSeconds));      // missing seconds
    EXPECT_FALSE(OrbitMath::parseUtc("2026-13-01T00:00:00", unixSeconds));   // month 13
    EXPECT_FALSE(OrbitMath::parseUtc("2026-02-30T00:00:00", unixSeconds));   // 30 February
    EXPECT_FALSE(OrbitMath::parseUtc("2025-02-29T00:00:00", unixSeconds));   // not a leap year
    EXPECT_FALSE(OrbitMath::parseUtc("2026-10-10T24:00:00", unixSeconds));
    EXPECT_FALSE(OrbitMath::parseUtc("2026-10-10T17:60:00", unixSeconds));
}

TEST(OrbitMathTime, FormatRoundsToTheSecond) {
    char text[24];
    OrbitMath::formatUtc(1791654409.9, text, sizeof(text));
    EXPECT_STREQ(text, "2026-10-10T17:46:50");
    OrbitMath::formatUtc(1791654410.4, text, sizeof(text));
    EXPECT_STREQ(text, "2026-10-10T17:46:50");
    OrbitMath::formatUtc(Oracle::TLE_EPOCH_UNIX, text, sizeof(text));
    EXPECT_STREQ(text, Oracle::TLE_EPOCH_TEXT);
}

// ----------------------------------------------------------------------
// Look angles
// ----------------------------------------------------------------------

TEST(OrbitMathLookAngle, VirtualPassMatchesSkyfield) {
    expectMatchesOracle(VIRTUAL_70, Oracle::VIRTUAL_70_SAMPLES,
                        sizeof(Oracle::VIRTUAL_70_SAMPLES) / sizeof(Oracle::VIRTUAL_70_SAMPLES[0]));
}

TEST(OrbitMathLookAngle, BuenosAiresPassMatchesSkyfield) {
    expectMatchesOracle(BUENOS_AIRES, Oracle::BUENOS_AIRES_86_SAMPLES,
                        sizeof(Oracle::BUENOS_AIRES_86_SAMPLES) / sizeof(Oracle::BUENOS_AIRES_86_SAMPLES[0]));
}

TEST(OrbitMathLookAngle, VirtualPassStaysWithinNinetyDegreesOfNorth) {
    // The rig test relies on this: azimuth within +/-90 deg of zero for the whole pass
    const double aos = Oracle::VIRTUAL_70_PASS.aosUnix;
    const double los = Oracle::VIRTUAL_70_PASS.losUnix;
    for (double t = aos; t <= los; t += 1.0) {
        OrbitMath::LookAngle angle;
        ASSERT_TRUE(OrbitMath::lookAngle(issPropagator(), VIRTUAL_70, t, angle));
        const double signedAzimuth = angle.azimuthDeg > 180.0 ? angle.azimuthDeg - 360.0 : angle.azimuthDeg;
        EXPECT_LE(std::fabs(signedAzimuth), 90.0) << "at " << t;
    }
}

// ----------------------------------------------------------------------
// Pass prediction
// ----------------------------------------------------------------------

TEST(OrbitMathPass, VirtualPassMatchesSkyfield) {
    OrbitMath::Pass pass;
    ASSERT_TRUE(OrbitMath::findNextPass(issPropagator(), VIRTUAL_70, parsed("2026-10-10T17:35:00"), 24.0, pass));
    expectPassMatchesOracle(pass, Oracle::VIRTUAL_70_PASS);
}

TEST(OrbitMathPass, BuenosAiresPassMatchesSkyfield) {
    OrbitMath::Pass pass;
    ASSERT_TRUE(OrbitMath::findNextPass(issPropagator(), BUENOS_AIRES, parsed("2026-10-11T22:00:00"), 24.0, pass));
    expectPassMatchesOracle(pass, Oracle::BUENOS_AIRES_86_PASS);
}

TEST(OrbitMathPass, ReportsPassInProgress) {
    // Starting at the peak returns the same pass, with AOS before the start time
    OrbitMath::Pass pass;
    ASSERT_TRUE(
        OrbitMath::findNextPass(issPropagator(), VIRTUAL_70, Oracle::VIRTUAL_70_PASS.tcaUnix, 24.0, pass));
    expectPassMatchesOracle(pass, Oracle::VIRTUAL_70_PASS);
}

TEST(OrbitMathPass, NoPassNearTheSouthPole) {
    // The ISS never goes south of 51.6 deg, so it never rises for an observer at 89 deg south
    const OrbitMath::Observer southPole{-89.0, 0.0, 0.0};
    OrbitMath::Pass pass;
    EXPECT_FALSE(OrbitMath::findNextPass(issPropagator(), southPole, Oracle::TLE_EPOCH_UNIX, 24.0, pass));
}

}  // namespace IssTracker
