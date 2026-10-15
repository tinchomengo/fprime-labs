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

TEST(TrackLogicAzimuth, MountZeroNotAtNorth) {
    // Mount zero facing south-west (222.9 deg)
    EXPECT_NEAR(TrackLogic::mountAzimuth(222.9, 222.9), 0.0, 1e-9);
    EXPECT_NEAR(TrackLogic::mountAzimuth(313.0, 222.9), 90.1, 1e-9);
    EXPECT_NEAR(TrackLogic::mountAzimuth(133.0, 222.9), -89.9, 1e-9);
    EXPECT_NEAR(TrackLogic::mountAzimuth(0.0, 222.9), 137.1, 1e-9);
    // Mount zero just west of north: a target just east of north is a small positive angle
    EXPECT_NEAR(TrackLogic::mountAzimuth(5.0, 355.0), 10.0, 1e-9);
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
    EXPECT_FALSE(decision.azimuthClamped);
}

TEST(TrackLogicDecide, SlightlyPastTheLimitIsClamped) {
    // 0.1 deg past +90 with the mount zero at 222.9 deg: sent as +90
    Decision decision = TrackLogic::decide(Phase::TRACKING, 313.0, 1.0, 222.9);
    EXPECT_EQ(decision.action, Decision::Action::POINT);
    EXPECT_DOUBLE_EQ(decision.azimuthDeg, 90.0);
    EXPECT_TRUE(decision.azimuthClamped);
    EXPECT_FALSE(decision.azimuthOutOfRange);

    // Same on the negative side, and while waiting below the horizon
    decision = TrackLogic::decide(Phase::WAITING, 132.8, -2.0, 222.9);
    EXPECT_EQ(decision.action, Decision::Action::POINT);
    EXPECT_DOUBLE_EQ(decision.azimuthDeg, -90.0);
    EXPECT_DOUBLE_EQ(decision.elevationDeg, 0.0);

    // Past the clamp margin: skipped
    decision = TrackLogic::decide(Phase::TRACKING, 95.1, 10.0);
    EXPECT_EQ(decision.action, Decision::Action::NONE);
    EXPECT_TRUE(decision.azimuthOutOfRange);
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

// ----------------------------------------------------------------------
// A real pass: the 86 deg pass over Buenos Aires on 2026-10-11
// ----------------------------------------------------------------------

namespace {

struct ReplayResult {
    int secondsOutOfRangeAboveHorizon = 0;
    double stowTime = -1.0;
};

//! Replays the Buenos Aires pass once per second for a mount whose zero faces \p mountZeroDeg
ReplayResult replayBuenosAiresPass(double mountZeroDeg) {
    const libsgp4::SGP4 sgp4(libsgp4::Tle(Oracle::TLE_LINE1, Oracle::TLE_LINE2));
    const OrbitMath::Observer observer{Oracle::BUENOS_AIRES_LAT, Oracle::BUENOS_AIRES_LON,
                                       Oracle::BUENOS_AIRES_ALT_M};
    const Oracle::PassReference& pass = Oracle::BUENOS_AIRES_86_PASS;

    ReplayResult result;
    Phase phase = Phase::WAITING;
    for (double t = pass.aosUnix - 60.0; t <= pass.losUnix + 60.0; t += 1.0) {
        OrbitMath::LookAngle angle;
        EXPECT_TRUE(OrbitMath::lookAngle(sgp4, observer, t, angle));
        const Decision decision = TrackLogic::decide(phase, angle.azimuthDeg, angle.elevationDeg, mountZeroDeg);
        if (decision.azimuthOutOfRange && angle.elevationDeg >= 0.0) {
            result.secondsOutOfRangeAboveHorizon++;
        }
        if (decision.action == Decision::Action::POINT) {
            EXPECT_LE(std::fabs(decision.azimuthDeg), TrackLogic::AZ_LIMIT_DEG);
        }
        if (decision.action == Decision::Action::STOW) {
            result.stowTime = t;
        }
        phase = decision.nextPhase;
    }
    return result;
}

}  // namespace

TEST(TrackLogicPass, RealPassNeedsTheMountTurnedToItsCentre) {
    // With mount zero at north, most of this pass is out of the +/-90 deg range
    const ReplayResult north = replayBuenosAiresPass(0.0);
    EXPECT_GT(north.secondsOutOfRangeAboveHorizon, 200);

    // Turned to the pass centre that PREDICT_PASS reports, it fits. The pass
    // sweeps 180.2 deg, so near each horizon it is 0.1 deg past +/-90 deg;
    // those targets are clamped to the limit, not skipped.
    const libsgp4::SGP4 sgp4(libsgp4::Tle(Oracle::TLE_LINE1, Oracle::TLE_LINE2));
    const OrbitMath::Observer observer{Oracle::BUENOS_AIRES_LAT, Oracle::BUENOS_AIRES_LON,
                                       Oracle::BUENOS_AIRES_ALT_M};
    OrbitMath::Pass predicted;
    ASSERT_TRUE(OrbitMath::findNextPass(sgp4, observer, Oracle::BUENOS_AIRES_86_PASS.aosUnix - 600.0, 24.0, predicted));

    const ReplayResult centred = replayBuenosAiresPass(predicted.azimuthCenterDeg);
    EXPECT_EQ(centred.secondsOutOfRangeAboveHorizon, 0);
    EXPECT_NEAR(centred.stowTime, Oracle::BUENOS_AIRES_86_PASS.losUnix, 1.0);
}

}  // namespace IssTracker
