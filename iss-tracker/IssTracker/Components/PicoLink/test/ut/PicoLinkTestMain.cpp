// ======================================================================
// \title  PicoLinkTestMain.cpp
// \author tinchomengo
// \brief  cpp file for PicoLink component test main function
//
// The PicoProtocol tests (PicoProtocolTest.cpp) are linked into the same
// executable.
// ======================================================================

#include "PicoLinkTester.hpp"

TEST(PicoLink, LinkNotOpen) {
    IssTracker::PicoLinkTester tester;
    tester.testLinkNotOpen();
}

TEST(PicoLink, Heartbeat) {
    IssTracker::PicoLinkTester tester;
    tester.testHeartbeat();
}

TEST(PicoLink, MountCommands) {
    IssTracker::PicoLinkTester tester;
    tester.testMountCommands();
}

TEST(PicoLink, GroundCommands) {
    IssTracker::PicoLinkTester tester;
    tester.testGroundCommands();
}

TEST(PicoLink, Receive) {
    IssTracker::PicoLinkTester tester;
    tester.testReceive();
}

TEST(PicoLink, RebootResetsHeartbeat) {
    IssTracker::PicoLinkTester tester;
    tester.testRebootResetsHeartbeat();
}

TEST(PicoLink, SendFailure) {
    IssTracker::PicoLinkTester tester;
    tester.testSendFailure();
}

TEST(PicoLink, LongLineDropped) {
    IssTracker::PicoLinkTester tester;
    tester.testLongLineDropped();
}

TEST(PicoLink, SilenceDetected) {
    IssTracker::PicoLinkTester tester;
    tester.testSilenceDetected();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
