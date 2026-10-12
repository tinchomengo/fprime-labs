// ======================================================================
// \title  PicoLinkTester.cpp
// \author tinchomengo
// \brief  cpp file for PicoLink component test harness implementation class
// ======================================================================

#include "PicoLinkTester.hpp"

namespace IssTracker {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

PicoLinkTester ::PicoLinkTester()
    : PicoLinkGTestBase("PicoLinkTester", PicoLinkTester::MAX_HISTORY_SIZE), component("PicoLink") {
    this->initComponents();
    this->connectPorts();
}

PicoLinkTester ::~PicoLinkTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void PicoLinkTester ::testLinkNotOpen() {
    this->invoke_to_schedIn(0, 0);
    EXPECT_TRUE(this->m_sent.empty());

    this->sendCmd_MOUNT_ENABLE(0, 1);
    ASSERT_CMD_RESPONSE(0, PicoLinkComponentBase::OPCODE_MOUNT_ENABLE, 1, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_LinkNotOpen_SIZE(1);
    EXPECT_TRUE(this->m_sent.empty());
}

void PicoLinkTester ::testHeartbeat() {
    this->invoke_to_drvConnected(0);
    ASSERT_EVENTS_LinkOpened_SIZE(1);

    // First cycle: set the Pico's heartbeat timeout
    this->invoke_to_schedIn(0, 0);
    ASSERT_EQ(this->m_sent.size(), 1U);
    EXPECT_EQ(this->m_sent[0], "HEARTBEAT 3\n");

    // Quiet cycle: PING keeps it alive
    this->invoke_to_schedIn(0, 0);
    ASSERT_EQ(this->m_sent.size(), 2U);
    EXPECT_EQ(this->m_sent[1], "PING\n");

    // A cycle that already sent a SET needs no PING
    this->invoke_to_mountIn(0, MountAction::SET, 1.0, 2.0);
    this->invoke_to_schedIn(0, 0);
    ASSERT_EQ(this->m_sent.size(), 3U);
    EXPECT_EQ(this->m_sent[2], "SET 1.000 2.000\n");

    // The next quiet cycle pings again
    this->invoke_to_schedIn(0, 0);
    ASSERT_EQ(this->m_sent.size(), 4U);
    EXPECT_EQ(this->m_sent[3], "PING\n");
}

void PicoLinkTester ::testMountCommands() {
    this->openLink();
    this->invoke_to_mountIn(0, MountAction::ENABLE, 0.0, 0.0);
    this->invoke_to_mountIn(0, MountAction::SET, -88.3912, 0.0);
    this->invoke_to_mountIn(0, MountAction::STOW, 0.0, 0.0);
    this->invoke_to_mountIn(0, MountAction::DISABLE, 0.0, 0.0);

    const std::vector<std::string> expected = {"ENABLE\n", "SET -88.391 0.000\n", "STOW\n", "DISABLE\n"};
    EXPECT_EQ(this->m_sent, expected);
}

void PicoLinkTester ::testGroundCommands() {
    this->openLink();
    this->sendCmd_MOUNT_ENABLE(0, 1);
    this->sendCmd_MOUNT_SET(0, 2, 45.0, 30.0);
    this->sendCmd_MOUNT_STOW(0, 3);
    this->sendCmd_MOUNT_DISABLE(0, 4);
    this->sendCmd_MOUNT_ZERO_AZ(0, 5);

    ASSERT_CMD_RESPONSE_SIZE(5);
    ASSERT_CMD_RESPONSE(0, PicoLinkComponentBase::OPCODE_MOUNT_ENABLE, 1, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(1, PicoLinkComponentBase::OPCODE_MOUNT_SET, 2, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(2, PicoLinkComponentBase::OPCODE_MOUNT_STOW, 3, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(3, PicoLinkComponentBase::OPCODE_MOUNT_DISABLE, 4, Fw::CmdResponse::OK);
    ASSERT_CMD_RESPONSE(4, PicoLinkComponentBase::OPCODE_MOUNT_ZERO_AZ, 5, Fw::CmdResponse::OK);

    const std::vector<std::string> expected = {"ENABLE\n", "SET 45.000 30.000\n", "STOW\n", "DISABLE\n",
                                               "ZERO_AZ\n"};
    EXPECT_EQ(this->m_sent, expected);
}

void PicoLinkTester ::testReceive() {
    this->openLink();

    // Split mid-line, as USB reads arrive
    this->receive("TLM -45.00 0.0");
    ASSERT_TLM_Azimuth_SIZE(0);
    this->receive("0 TRACKING\r\nOK SET\nERR LI");
    this->receive("MIT\nEVT STOWED\n");

    ASSERT_TLM_Azimuth_SIZE(1);
    ASSERT_TLM_Azimuth(0, -45.0);
    ASSERT_TLM_Elevation(0, 0.0);
    ASSERT_TLM_State(0, MountState::TRACKING);

    ASSERT_EVENTS_MountError_SIZE(1);
    EXPECT_STREQ(this->eventHistory_MountError->at(0).reply.toChar(), "ERR LIMIT");
    ASSERT_TLM_ErrorCount(0, 1U);

    ASSERT_EVENTS_MountEvent_SIZE(1);
    EXPECT_STREQ(this->eventHistory_MountEvent->at(0).eventName.toChar(), "STOWED");

    // Every buffer loaned by the driver was returned
    ASSERT_from_drvReceiveReturnOut_SIZE(3);
}

void PicoLinkTester ::testRebootResetsHeartbeat() {
    this->openLink();
    this->m_sent.clear();

    this->receive("EVT BOOT\n");
    ASSERT_EVENTS_MountEvent_SIZE(1);
    this->invoke_to_schedIn(0, 0);
    ASSERT_EQ(this->m_sent.size(), 1U);
    EXPECT_EQ(this->m_sent[0], "HEARTBEAT 3\n");
}

void PicoLinkTester ::testSendFailure() {
    this->openLink();
    this->m_sendStatus = Drv::ByteStreamStatus::OTHER_ERROR;

    this->sendCmd_MOUNT_STOW(0, 6);
    ASSERT_CMD_RESPONSE(0, PicoLinkComponentBase::OPCODE_MOUNT_STOW, 6, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_SendFailed_SIZE(1);
}

void PicoLinkTester ::testLongLineDropped() {
    this->openLink();
    this->receive(std::string(200, 'x') + "\nTLM 12.50 30.00 STOWED\n");
    ASSERT_TLM_Azimuth_SIZE(1);
    ASSERT_TLM_Azimuth(0, 12.5);
    ASSERT_TLM_State(0, MountState::STOWED);
}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

Drv::ByteStreamStatus PicoLinkTester ::from_drvSendOut_handler(const FwIndexType portNum, Fw::Buffer& sendBuffer) {
    this->m_sent.emplace_back(reinterpret_cast<const char*>(sendBuffer.getData()),
                              static_cast<std::size_t>(sendBuffer.getSize()));
    return this->m_sendStatus;
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void PicoLinkTester ::openLink() {
    this->invoke_to_drvConnected(0);
    this->invoke_to_schedIn(0, 0);
    ASSERT_EQ(this->m_sent.size(), 1U);
    EXPECT_EQ(this->m_sent[0], "HEARTBEAT 3\n");
    this->m_sent.clear();
    this->clearHistory();
}

void PicoLinkTester ::receive(const std::string& text) {
    std::vector<U8> bytes(text.begin(), text.end());
    Fw::Buffer buffer(bytes.data(), static_cast<FwSizeType>(bytes.size()));
    this->invoke_to_drvReceiveIn(0, buffer, Drv::ByteStreamStatus::OP_OK);
}

}  // namespace IssTracker
