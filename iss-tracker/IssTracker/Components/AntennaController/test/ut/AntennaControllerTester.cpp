// ======================================================================
// \title  AntennaControllerTester.cpp
// \author tinchomengo
// \brief  cpp file for AntennaController component test harness implementation class
// ======================================================================

#include "AntennaControllerTester.hpp"

namespace IssTracker {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

AntennaControllerTester ::AntennaControllerTester()
    : AntennaControllerGTestBase("AntennaControllerTester", AntennaControllerTester::MAX_HISTORY_SIZE),
      component("AntennaController") {
    this->initComponents();
    this->connectPorts();
}

AntennaControllerTester ::~AntennaControllerTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void AntennaControllerTester ::testIdleSendsNothing() {
    this->invoke_to_lookAngleIn(0, 10.0, 45.0);
    this->invoke_to_lookAngleIn(0, 10.0, -5.0);
    ASSERT_from_mountOut_SIZE(0);
    ASSERT_TLM_Phase(0, TrackPhase::IDLE);
}

void AntennaControllerTester ::testTrackStart() {
    this->sendCmd_TRACK_START(0, 1);
    ASSERT_CMD_RESPONSE(0, AntennaControllerComponentBase::OPCODE_TRACK_START, 1, Fw::CmdResponse::OK);
    ASSERT_EVENTS_TrackingStarted_SIZE(1);
    ASSERT_TLM_Phase(0, TrackPhase::WAITING);
    ASSERT_from_mountOut_SIZE(1);
    this->expectMountCommand(0, MountAction::ENABLE, 0.0, 0.0);

    // A second TRACK_START while tracking changes nothing
    this->clearHistory();
    this->sendCmd_TRACK_START(0, 2);
    ASSERT_CMD_RESPONSE(0, AntennaControllerComponentBase::OPCODE_TRACK_START, 2, Fw::CmdResponse::OK);
    ASSERT_EVENTS_TrackingStarted_SIZE(0);
    ASSERT_from_mountOut_SIZE(0);
}

void AntennaControllerTester ::testPassSequence() {
    this->startTracking();

    // Below the horizon at compass 271.6 deg: wait on the horizon at mount -88.4 deg
    this->invoke_to_lookAngleIn(0, 271.6, -3.4);
    ASSERT_from_mountOut_SIZE(1);
    this->expectMountCommand(0, MountAction::SET, 271.6 - 360.0, 0.0);
    ASSERT_TLM_Phase(0, TrackPhase::WAITING);
    ASSERT_TLM_TargetAzimuth(0, 271.6 - 360.0);
    ASSERT_TLM_TargetElevation(0, 0.0);

    // Rises: tracking starts
    this->clearHistory();
    this->invoke_to_lookAngleIn(0, 272.4, 0.2);
    ASSERT_EVENTS_PassStarted_SIZE(1);
    ASSERT_EVENTS_PassStarted(0, 272.4 - 360.0);
    ASSERT_TLM_Phase(0, TrackPhase::TRACKING);
    this->expectMountCommand(0, MountAction::SET, 272.4 - 360.0, 0.2);

    // Peak, due north
    this->clearHistory();
    this->invoke_to_lookAngleIn(0, 0.3, 70.0);
    this->expectMountCommand(0, MountAction::SET, 0.3, 70.0);

    // Sets: stow, pass over
    this->clearHistory();
    this->invoke_to_lookAngleIn(0, 87.9, -0.1);
    ASSERT_from_mountOut_SIZE(1);
    this->expectMountCommand(0, MountAction::STOW, 0.0, 0.0);
    ASSERT_EVENTS_PassComplete_SIZE(1);
    ASSERT_TLM_Phase(0, TrackPhase::IDLE);

    // Idle again: nothing more is sent
    this->clearHistory();
    this->invoke_to_lookAngleIn(0, 90.0, -5.0);
    ASSERT_from_mountOut_SIZE(0);
}

void AntennaControllerTester ::testTrackStop() {
    this->startTracking();
    this->invoke_to_lookAngleIn(0, 0.3, 70.0);

    this->clearHistory();
    this->sendCmd_TRACK_STOP(0, 3);
    ASSERT_CMD_RESPONSE(0, AntennaControllerComponentBase::OPCODE_TRACK_STOP, 3, Fw::CmdResponse::OK);
    ASSERT_EVENTS_TrackingStopped_SIZE(1);
    ASSERT_from_mountOut_SIZE(1);
    this->expectMountCommand(0, MountAction::STOW, 0.0, 0.0);
    ASSERT_TLM_Phase(0, TrackPhase::IDLE);

    this->clearHistory();
    this->invoke_to_lookAngleIn(0, 0.3, 70.0);
    ASSERT_from_mountOut_SIZE(0);

    // Stopping when already idle does nothing
    this->clearHistory();
    this->sendCmd_TRACK_STOP(0, 4);
    ASSERT_CMD_RESPONSE(0, AntennaControllerComponentBase::OPCODE_TRACK_STOP, 4, Fw::CmdResponse::OK);
    ASSERT_EVENTS_TrackingStopped_SIZE(0);
    ASSERT_from_mountOut_SIZE(0);
}

void AntennaControllerTester ::testAzimuthOutOfRange() {
    this->startTracking();

    // Due south is 180 deg from mount zero: outside +/-90 deg, so the mount holds
    this->invoke_to_lookAngleIn(0, 180.0, 40.0);
    ASSERT_from_mountOut_SIZE(0);
    ASSERT_EVENTS_AzimuthOutOfRange_SIZE(1);
    ASSERT_EVENTS_AzimuthOutOfRange(0, 180.0);
    // It rose while out of range: still counts as tracking, so setting stows
    ASSERT_TLM_Phase(0, TrackPhase::TRACKING);
}

void AntennaControllerTester ::testMountZero() {
    // Mount turned to face south-west, the centre of the 86 deg Buenos Aires pass
    this->paramSet_MOUNT_ZERO_AZ_DEG(222.9, Fw::ParamValid::VALID);
    this->component.loadParameters();
    ASSERT_EVENTS_MountZeroSet_SIZE(1);
    ASSERT_EVENTS_MountZeroSet(0, 222.9);

    this->startTracking();

    // Rising in the north-west, 0.1 deg past the mount's +90 deg: sent as +90
    this->invoke_to_lookAngleIn(0, 313.0, 0.5);
    ASSERT_from_mountOut_SIZE(1);
    this->expectMountCommand(0, MountAction::SET, 90.0, 0.5);
    ASSERT_EVENTS_AzimuthOutOfRange_SIZE(0);

    // Overhead-ish, toward the south-west: straight ahead of the mount
    this->clearHistory();
    this->invoke_to_lookAngleIn(0, 222.9, 85.0);
    this->expectMountCommand(0, MountAction::SET, 0.0, 85.0);

    // Behind the mount (north-east): far outside the range, skipped
    this->clearHistory();
    this->invoke_to_lookAngleIn(0, 45.0, 30.0);
    ASSERT_from_mountOut_SIZE(0);
    ASSERT_EVENTS_AzimuthOutOfRange_SIZE(1);

    // An invalid value is rejected and the previous one kept
    this->clearHistory();
    this->paramSet_MOUNT_ZERO_AZ_DEG(400.0, Fw::ParamValid::VALID);
    this->component.loadParameters();
    ASSERT_EVENTS_MountZeroInvalid_SIZE(1);
    this->invoke_to_lookAngleIn(0, 222.9, 85.0);
    this->expectMountCommand(0, MountAction::SET, 0.0, 85.0);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void AntennaControllerTester ::startTracking() {
    this->sendCmd_TRACK_START(0, 0);
    ASSERT_CMD_RESPONSE(0, AntennaControllerComponentBase::OPCODE_TRACK_START, 0, Fw::CmdResponse::OK);
    this->clearHistory();
}

void AntennaControllerTester ::expectMountCommand(FwSizeType index,
                                                  MountAction::T action,
                                                  F64 azimuthDeg,
                                                  F64 elevationDeg) {
    ASSERT_GT(this->fromPortHistory_mountOut->size(), index);
    const auto& entry = this->fromPortHistory_mountOut->at(index);
    EXPECT_EQ(entry.mountAction.e, action);
    if (action == MountAction::SET) {
        EXPECT_DOUBLE_EQ(entry.azimuthDeg, azimuthDeg);
        EXPECT_DOUBLE_EQ(entry.elevationDeg, elevationDeg);
    }
}

}  // namespace IssTracker
