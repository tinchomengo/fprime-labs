// ======================================================================
// \title  AntennaControllerTestMain.cpp
// \author tinchomengo
// \brief  cpp file for AntennaController component test main function
//
// The TrackLogic tests (TrackLogicTest.cpp), including the replay of the
// simulated pass, are linked into the same executable.
// ======================================================================

#include "AntennaControllerTester.hpp"

TEST(AntennaController, IdleSendsNothing) {
    IssTracker::AntennaControllerTester tester;
    tester.testIdleSendsNothing();
}

TEST(AntennaController, TrackStart) {
    IssTracker::AntennaControllerTester tester;
    tester.testTrackStart();
}

TEST(AntennaController, PassSequence) {
    IssTracker::AntennaControllerTester tester;
    tester.testPassSequence();
}

TEST(AntennaController, TrackStop) {
    IssTracker::AntennaControllerTester tester;
    tester.testTrackStop();
}

TEST(AntennaController, AzimuthOutOfRange) {
    IssTracker::AntennaControllerTester tester;
    tester.testAzimuthOutOfRange();
}

TEST(AntennaController, MountZero) {
    IssTracker::AntennaControllerTester tester;
    tester.testMountZero();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
