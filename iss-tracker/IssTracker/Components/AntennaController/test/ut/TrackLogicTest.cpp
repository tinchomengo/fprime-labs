// ======================================================================
// \title  TrackLogicTest.cpp
// \author tinchomengo
// \brief  Tests for TrackLogic, including a replay of the simulated pass
// ======================================================================

#include <cmath>

#include <gtest/gtest.h>
#include <libsgp4/SGP4.h>
#include <libsgp4/Tle.h>

#include "IssTracker/Components/AntennaController/TrackLogic.hpp"
#include "IssTracker/Components/Sgp4Propagator/OrbitMath.hpp"
#include "IssTracker/Components/Sgp4Propagator/test/ut/OracleData.hpp"

namespace IssTracker {

using TrackLogic::Decision;
using TrackLogic::Phase;

// ----------------------------------------------------------------------
// Azimuth conversion
// ----------------------------------------------------------------------

TEST(TrackLogicAzimuth, CompassToMount) {
    EXPECT_DOUBLE_EQ(TrackLogic::mountAzimuth(0.0), 0.0);
    EXPECT_DOUBLE_EQ(TrackLogic::mountAzimuth(87.5), 87.5);
    EXPECT_DOUBLE_EQ(TrackLogic::mountAzimuth(180.0), 180.0);
    EXPECT_DOUBLE_EQ(TrackLogic::mountAzimuth(272.4), 272.4 - 360.0);
    EXPECT_DOUBLE_EQ(TrackLogic::mountAzimuth(360.0), 0.0);
    EXPECT_DOUBLE_EQ(TrackLogic::mountAzimuth(-10.0), -10.0);
}

// ----------------------------------------------------------------------
// Decisions
// ----------------------------------------------------------------------

TEST(TrackLogicDecide, IdleDoesNothing) {
    const Decision decision = TrackLogic::decide(Phase::IDLE, 10.0, 45.0);
    EXPECT_EQ(decision.action, Decision::Action::NONE);
    EXPECT_EQ(decision.nextPhase, Phase::IDLE);
}

TEST(TrackLogicDecide, WaitingBelowHorizonPointsAtTheHorizon) {
    const Decision decision = TrackLogic::decide(Phase::WAITING, 271.6, -3.4);
    EXPECT_EQ(decision.action, Decision::Action::POINT);
    EXPECT_NEAR(decision.azimuthDeg, -88.4, 1e-9);
    EXPECT_DOUBLE_EQ(decision.elevationDeg, 0.0);
    EXPECT_EQ(decision.nextPhase, Phase::WAITING);
}

TEST(TrackLogicDecide, RisingStartsTracking) {
    const Decision decision = TrackLogic::decide(Phase::WAITING, 272.4, 0.5);
    EXPECT_EQ(decision.action, Decision::Action::POINT);
    EXPECT_DOUBLE_EQ(decision.elevationDeg, 0.5);
    EXPECT_EQ(decision.nextPhase, Phase::TRACKING);
}

TEST(TrackLogicDecide, SettingStowsAndEnds) {
    const Decision decision = TrackLogic::decide(Phase::TRACKING, 87.9, -0.1);
    EXPECT_EQ(decision.action, Decision::Action::STOW);
    EXPECT_EQ(decision.nextPhase, Phase::IDLE);
}

TEST(TrackLogicDecide, OutOfRangeAzimuthIsSkipped) {
    // Due south is 180 deg from mount zero: beyond the +/-90 deg range
    Decision decision = TrackLogic::decide(Phase::TRACKING, 180.0, 40.0);
    EXPECT_EQ(decision.action, Decision::Action::NONE);
    EXPECT_TRUE(decision.azimuthOutOfRange);
    EXPECT_EQ(decision.nextPhase, Phase::TRACKING);

    decision = TrackLogic::decide(Phase::WAITING, 200.0, -20.0);
    EXPECT_EQ(decision.action, Decision::Action::NONE);
    EXPECT_TRUE(decision.azimuthOutOfRange);
    EXPECT_EQ(decision.nextPhase, Phase::WAITING);

    // Exactly at the limit is allowed
    decision = TrackLogic::decide(Phase::TRACKING, 270.0, 10.0);
    EXPECT_EQ(decision.action, Decision::Action::POINT);
    EXPECT_DOUBLE_EQ(decision.azimuthDeg, -90.0);
}

// ----------------------------------------------------------------------
// The simulated pass, replayed once per second as the deployment runs it
// ----------------------------------------------------------------------

TEST(TrackLogicPass, SimulatedPassDrivesTheMountWithinItsLimits) {
    const libsgp4::SGP4 sgp4(libsgp4::Tle(Oracle::TLE_LINE1, Oracle::TLE_LINE2));
    const OrbitMath::Observer observer{Oracle::VIRTUAL_70_LAT, Oracle::VIRTUAL_70_LON, Oracle::VIRTUAL_70_ALT_M};

    double start = 0.0;
    ASSERT_TRUE(OrbitMath::parseUtc("2026-10-10T17:40:19", start));  // SIM_CLOCK_START time

    Phase phase = Phase::WAITING;  // after TRACK_START
    double trackingStart = -1.0;
    double stowTime = -1.0;
    double highestElevation = -1.0;
    double previousAzimuth = 0.0;
    double largestAzimuthStep = 0.0;
    bool havePrevious = false;
    int pointsWhileWaiting = 0;

    for (double t = start; t <= start + 15.0 * 60.0; t += 1.0) {
        OrbitMath::LookAngle angle;
        ASSERT_TRUE(OrbitMath::lookAngle(sgp4, observer, t, angle));
        const Decision decision = TrackLogic::decide(phase, angle.azimuthDeg, angle.elevationDeg);

        EXPECT_FALSE(decision.azimuthOutOfRange) << "at " << t;
        if (decision.action == Decision::Action::POINT) {
            EXPECT_LE(std::fabs(decision.azimuthDeg), TrackLogic::AZ_LIMIT_DEG);
            EXPECT_GE(decision.elevationDeg, 0.0);
            EXPECT_LE(decision.elevationDeg, TrackLogic::EL_MAX_DEG);
            if (phase == Phase::WAITING && decision.nextPhase == Phase::WAITING) {
                pointsWhileWaiting++;
                EXPECT_DOUBLE_EQ(decision.elevationDeg, 0.0);
            }
            if (havePrevious) {
                largestAzimuthStep = std::fmax(largestAzimuthStep, std::fabs(decision.azimuthDeg - previousAzimuth));
            }
            previousAzimuth = decision.azimuthDeg;
            havePrevious = true;
            highestElevation = std::fmax(highestElevation, decision.elevationDeg);
        }
        if (phase == Phase::WAITING && decision.nextPhase == Phase::TRACKING) {
            trackingStart = t;
        }
        if (decision.action == Decision::Action::STOW) {
            EXPECT_LT(stowTime, 0.0) << "stowed twice";
            stowTime = t;
        }
        phase = decision.nextPhase;
    }

    // Waits on the horizon for the minute before AOS, starts at AOS, stows at LOS
    EXPECT_EQ(pointsWhileWaiting, 60);
    EXPECT_NEAR(trackingStart, Oracle::VIRTUAL_70_PASS.aosUnix, 1.0);
    EXPECT_NEAR(stowTime, Oracle::VIRTUAL_70_PASS.losUnix, 1.0);
    EXPECT_NEAR(highestElevation, Oracle::VIRTUAL_70_PASS.maxElevationDeg, 0.1);
    EXPECT_EQ(phase, Phase::IDLE);
    // Largest azimuth change between one-second commands: well within the
    // mount's 90 deg/s
    EXPECT_LT(largestAzimuthStep, 3.0);
}

}  // namespace IssTracker
