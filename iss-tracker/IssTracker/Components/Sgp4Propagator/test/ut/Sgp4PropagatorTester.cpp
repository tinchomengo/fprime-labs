// ======================================================================
// \title  Sgp4PropagatorTester.cpp
// \author tinchomengo
// \brief  cpp file for Sgp4Propagator component test harness implementation class
// ======================================================================

#include "Sgp4PropagatorTester.hpp"

#include <cmath>
#include <string>

#include "IssTracker/Components/Sgp4Propagator/OrbitMath.hpp"

namespace IssTracker {

namespace {

F64 unixOf(const char* utc) {
    double unixSeconds = 0.0;
    EXPECT_TRUE(OrbitMath::parseUtc(utc, unixSeconds)) << utc;
    return unixSeconds;
}

}  // namespace

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

Sgp4PropagatorTester ::Sgp4PropagatorTester()
    : Sgp4PropagatorGTestBase("Sgp4PropagatorTester", Sgp4PropagatorTester::MAX_HISTORY_SIZE),
      component("Sgp4Propagator") {
    this->initComponents();
    this->connectPorts();
    this->setWallClock(WALL_CLOCK_UNIX);
}

Sgp4PropagatorTester ::~Sgp4PropagatorTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void Sgp4PropagatorTester ::testLoadTle() {
    this->sendCmd_LOAD_TLE(0, 1, Fw::CmdStringArg(Oracle::TLE_LINE1), Fw::CmdStringArg(Oracle::TLE_LINE2));

    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Sgp4PropagatorComponentBase::OPCODE_LOAD_TLE, 1, Fw::CmdResponse::OK);
    ASSERT_EVENTS_TleLoaded_SIZE(1);
    EXPECT_EQ(this->eventHistory_TleLoaded->at(0).satNumber, 25544U);
    EXPECT_STREQ(this->eventHistory_TleLoaded->at(0).epoch.toChar(), Oracle::TLE_EPOCH_TEXT);
    EXPECT_NEAR(this->eventHistory_TleLoaded->at(0).ageHours, (WALL_CLOCK_UNIX - Oracle::TLE_EPOCH_UNIX) / 3600.0,
                1.0e-3);
}

void Sgp4PropagatorTester ::testLoadTleRejectsGarbage() {
    this->sendCmd_LOAD_TLE(0, 2, Fw::CmdStringArg("not a"), Fw::CmdStringArg("tle"));
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Sgp4PropagatorComponentBase::OPCODE_LOAD_TLE, 2, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_TleRejected_SIZE(1);
    ASSERT_EVENTS_TleLoaded_SIZE(0);

    // Still nothing loaded
    this->invoke_to_schedIn(0, 0);
    ASSERT_EVENTS_NoTleLoaded_SIZE(1);
    ASSERT_TLM_Azimuth_SIZE(0);

    // A rejected TLE doesn't replace one that is already loaded
    this->loadIssTle();
    this->sendCmd_LOAD_TLE(0, 3, Fw::CmdStringArg("not a"), Fw::CmdStringArg("tle"));
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_Azimuth_SIZE(1);
}

void Sgp4PropagatorTester ::testLoadTleIgnoresTrailingWhitespace() {
    const std::string line1 = std::string(Oracle::TLE_LINE1) + "   \r\n";
    const std::string line2 = std::string(Oracle::TLE_LINE2) + "\t \n";
    this->sendCmd_LOAD_TLE(0, 4, Fw::CmdStringArg(line1.c_str()), Fw::CmdStringArg(line2.c_str()));
    ASSERT_CMD_RESPONSE(0, Sgp4PropagatorComponentBase::OPCODE_LOAD_TLE, 4, Fw::CmdResponse::OK);
    ASSERT_EVENTS_TleLoaded_SIZE(1);
}

void Sgp4PropagatorTester ::testNoTle() {
    this->invoke_to_schedIn(0, 0);
    this->invoke_to_schedIn(0, 0);

    // Warned once, not every cycle; no look angle published
    ASSERT_EVENTS_NoTleLoaded_SIZE(1);
    ASSERT_TLM_Azimuth_SIZE(0);
    ASSERT_TLM_Elevation_SIZE(0);
    ASSERT_from_lookAngleOut_SIZE(0);
    ASSERT_TLM_SimClockActive_SIZE(2);

    this->sendCmd_PREDICT_PASS(0, 5);
    ASSERT_CMD_RESPONSE(0, Sgp4PropagatorComponentBase::OPCODE_PREDICT_PASS, 5, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_NoTleLoaded_SIZE(2);
}

void Sgp4PropagatorTester ::testRealClockMatchesSkyfield() {
    // Peak of the 86 deg pass over Buenos Aires, on the real clock
    const Oracle::LookSample& peak = Oracle::BUENOS_AIRES_86_SAMPLES[2];
    this->setObserver(Oracle::BUENOS_AIRES_LAT, Oracle::BUENOS_AIRES_LON, Oracle::BUENOS_AIRES_ALT_M);
    this->setWallClock(unixOf(peak.utc));
    this->loadIssTle();

    this->expectLookAngle(peak);
    ASSERT_TLM_SimClockActive(0, false);
    EXPECT_NEAR(this->tlmHistory_TleAgeHours->at(0).arg, (unixOf(peak.utc) - Oracle::TLE_EPOCH_UNIX) / 3600.0,
                1.0e-3);
}

void Sgp4PropagatorTester ::testSimClockReplaysVirtualPass() {
    this->setObserver(Oracle::VIRTUAL_70_LAT, Oracle::VIRTUAL_70_LON, Oracle::VIRTUAL_70_ALT_M);
    this->loadIssTle();

    const Oracle::LookSample* samples = Oracle::VIRTUAL_70_SAMPLES;
    const F64 simStart = unixOf(samples[0].utc);
    this->startSimClock(samples[0].utc, 1.0);

    // At 1x, simulated time advances exactly as much as the wall clock
    for (const Oracle::LookSample& sample : Oracle::VIRTUAL_70_SAMPLES) {
        this->setWallClock(WALL_CLOCK_UNIX + (unixOf(sample.utc) - simStart));
        this->expectLookAngle(sample);
        ASSERT_TLM_SimClockActive(0, true);
    }
}

void Sgp4PropagatorTester ::testSimClockRate() {
    this->setObserver(Oracle::VIRTUAL_70_LAT, Oracle::VIRTUAL_70_LON, Oracle::VIRTUAL_70_ALT_M);
    this->loadIssTle();
    this->startSimClock(Oracle::VIRTUAL_70_SAMPLES[0].utc, 10.0);

    // 32 s of wall time at 10x is 320 s of simulated time: 17:41:30 + 320 s = 17:46:50
    this->setWallClock(WALL_CLOCK_UNIX + 32.0);
    this->expectLookAngle(Oracle::VIRTUAL_70_SAMPLES[2]);
}

void Sgp4PropagatorTester ::testSimClockRejectsInvalidInput() {
    this->sendCmd_SIM_CLOCK_START(0, 6, Fw::CmdStringArg("2026-10-10 17:41:30"), 1.0);
    ASSERT_CMD_RESPONSE(0, Sgp4PropagatorComponentBase::OPCODE_SIM_CLOCK_START, 6,
                        Fw::CmdResponse::VALIDATION_ERROR);
    ASSERT_EVENTS_InvalidSimStart_SIZE(1);

    const F64 badRates[] = {0.0, -1.0, 100.5, NAN};
    for (const F64 rate : badRates) {
        this->clearHistory();
        this->sendCmd_SIM_CLOCK_START(0, 7, Fw::CmdStringArg("2026-10-10T17:41:30"), rate);
        ASSERT_CMD_RESPONSE(0, Sgp4PropagatorComponentBase::OPCODE_SIM_CLOCK_START, 7,
                            Fw::CmdResponse::VALIDATION_ERROR);
        ASSERT_EVENTS_InvalidSimRate_SIZE(1);
    }

    // Nothing was started
    ASSERT_EVENTS_SimClockStarted_SIZE(0);
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_SimClockActive(0, false);
}

void Sgp4PropagatorTester ::testSimClockStop() {
    this->loadIssTle();
    this->startSimClock("2026-10-10T17:41:30", 1.0);

    this->sendCmd_SIM_CLOCK_STOP(0, 8);
    ASSERT_CMD_RESPONSE(0, Sgp4PropagatorComponentBase::OPCODE_SIM_CLOCK_STOP, 8, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SimClockStopped_SIZE(1);

    // Back on the wall clock: 2026-10-11T23:00:00
    this->invoke_to_schedIn(0, 0);
    ASSERT_TLM_SimClockActive(0, false);
    EXPECT_STREQ(this->tlmHistory_PropagationTime->at(0).arg.toChar(), "2026-10-11T23:00:00");
}

void Sgp4PropagatorTester ::testPredictPass() {
    this->setObserver(Oracle::VIRTUAL_70_LAT, Oracle::VIRTUAL_70_LON, Oracle::VIRTUAL_70_ALT_M);
    this->loadIssTle();
    this->startSimClock("2026-10-10T17:35:00", 1.0);

    this->sendCmd_PREDICT_PASS(0, 9);
    ASSERT_CMD_RESPONSE(0, Sgp4PropagatorComponentBase::OPCODE_PREDICT_PASS, 9, Fw::CmdResponse::OK);
    ASSERT_EVENTS_PassPredicted_SIZE(1);

    // Skyfield: AOS 17:41:18.92, TCA 17:46:49.91, LOS 17:52:20.29 (reported rounded to the second)
    const auto& pass = this->eventHistory_PassPredicted->at(0);
    EXPECT_STREQ(pass.aos.toChar(), "2026-10-10T17:41:19");
    EXPECT_STREQ(pass.tca.toChar(), "2026-10-10T17:46:50");
    EXPECT_STREQ(pass.los.toChar(), "2026-10-10T17:52:20");
    EXPECT_NEAR(pass.maxElevation, Oracle::VIRTUAL_70_PASS.maxElevationDeg, Oracle::MAX_ELEVATION_TOLERANCE_DEG);
    // Rises in the west-northwest and sets in the east-northeast, both within 90 deg of north
    EXPECT_GT(pass.aosAzimuth, 270.0);
    EXPECT_LT(pass.losAzimuth, 90.0);

    // The ISS never rises for an observer near the South Pole
    this->clearHistory();
    this->setObserver(-89.0, 0.0, 0.0);
    this->sendCmd_PREDICT_PASS(0, 10);
    ASSERT_CMD_RESPONSE(0, Sgp4PropagatorComponentBase::OPCODE_PREDICT_PASS, 10, Fw::CmdResponse::OK);
    ASSERT_EVENTS_NoPassFound_SIZE(1);
    ASSERT_EVENTS_NoPassFound(0, Sgp4Propagator::PASS_SEARCH_HOURS);
}

void Sgp4PropagatorTester ::testObserverRejectsOutOfRange() {
    this->setObserver(Oracle::VIRTUAL_70_LAT, Oracle::VIRTUAL_70_LON, Oracle::VIRTUAL_70_ALT_M);

    this->paramSet_OBSERVER_LAT_DEG(95.0, Fw::ParamValid::VALID);
    this->component.loadParameters();
    ASSERT_EVENTS_ObserverInvalid_SIZE(1);
    ASSERT_EVENTS_ObserverSet_SIZE(0);

    // Still the virtual observer
    this->loadIssTle();
    this->startSimClock(Oracle::VIRTUAL_70_SAMPLES[2].utc, 1.0);
    this->expectLookAngle(Oracle::VIRTUAL_70_SAMPLES[2]);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void Sgp4PropagatorTester ::setObserver(F64 latitude, F64 longitude, F64 altitude) {
    this->clearHistory();
    this->paramSet_OBSERVER_LAT_DEG(latitude, Fw::ParamValid::VALID);
    this->paramSet_OBSERVER_LON_DEG(longitude, Fw::ParamValid::VALID);
    this->paramSet_OBSERVER_ALT_M(altitude, Fw::ParamValid::VALID);
    this->component.loadParameters();
    ASSERT_EVENTS_ObserverSet_SIZE(1);
    this->clearHistory();
}

void Sgp4PropagatorTester ::setWallClock(F64 unixSeconds) {
    const F64 wholeSeconds = std::floor(unixSeconds);
    const U32 microseconds = static_cast<U32>(std::round((unixSeconds - wholeSeconds) * 1.0e6));
    this->setTestTime(Fw::Time(TimeBase::TB_WORKSTATION_TIME, static_cast<U32>(wholeSeconds), microseconds));
}

void Sgp4PropagatorTester ::loadIssTle() {
    this->clearHistory();
    this->sendCmd_LOAD_TLE(0, 0, Fw::CmdStringArg(Oracle::TLE_LINE1), Fw::CmdStringArg(Oracle::TLE_LINE2));
    ASSERT_CMD_RESPONSE(0, Sgp4PropagatorComponentBase::OPCODE_LOAD_TLE, 0, Fw::CmdResponse::OK);
    this->clearHistory();
}

void Sgp4PropagatorTester ::startSimClock(const char* startUtc, F64 rate) {
    this->clearHistory();
    this->sendCmd_SIM_CLOCK_START(0, 0, Fw::CmdStringArg(startUtc), rate);
    ASSERT_CMD_RESPONSE(0, Sgp4PropagatorComponentBase::OPCODE_SIM_CLOCK_START, 0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_SimClockStarted_SIZE(1);
    EXPECT_STREQ(this->eventHistory_SimClockStarted->at(0).startUtc.toChar(), startUtc);
    this->clearHistory();
}

void Sgp4PropagatorTester ::expectLookAngle(const Oracle::LookSample& sample) {
    this->clearHistory();
    this->invoke_to_schedIn(0, 0);

    ASSERT_TLM_Azimuth_SIZE(1);
    ASSERT_TLM_Elevation_SIZE(1);
    ASSERT_TLM_RangeKm_SIZE(1);
    const F64 azimuth = this->tlmHistory_Azimuth->at(0).arg;
    const F64 elevation = this->tlmHistory_Elevation->at(0).arg;
    EXPECT_LT(Oracle::skySeparationDeg(azimuth, elevation, sample.azimuthDeg, sample.elevationDeg),
              Oracle::SKY_TOLERANCE_DEG)
        << sample.utc;
    EXPECT_NEAR(this->tlmHistory_RangeKm->at(0).arg, sample.rangeKm, Oracle::RANGE_TOLERANCE_KM) << sample.utc;
    EXPECT_STREQ(this->tlmHistory_PropagationTime->at(0).arg.toChar(), sample.utc);

    // The same look angle goes out to the AntennaController
    ASSERT_from_lookAngleOut_SIZE(1);
    ASSERT_from_lookAngleOut(0, azimuth, elevation);
}

}  // namespace IssTracker
