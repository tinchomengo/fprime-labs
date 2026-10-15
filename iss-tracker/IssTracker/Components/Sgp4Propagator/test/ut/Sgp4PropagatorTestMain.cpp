// ======================================================================
// \title  Sgp4PropagatorTestMain.cpp
// \author tinchomengo
// \brief  cpp file for Sgp4Propagator component test main function
//
// The OrbitMath tests (OrbitMathTest.cpp) are linked into the same
// executable and run alongside these.
// ======================================================================

#include "Sgp4PropagatorTester.hpp"

TEST(Sgp4PropagatorTle, Load) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testLoadTle();
}

TEST(Sgp4PropagatorTle, RejectsGarbage) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testLoadTleRejectsGarbage();
}

TEST(Sgp4PropagatorTle, IgnoresTrailingWhitespace) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testLoadTleIgnoresTrailingWhitespace();
}

TEST(Sgp4PropagatorTle, LoadFromFile) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testLoadTleFile();
}

TEST(Sgp4PropagatorTle, Old) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testTleOld();
}

TEST(Sgp4PropagatorTle, NoTle) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testNoTle();
}

TEST(Sgp4PropagatorLookAngle, RealClockMatchesSkyfield) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testRealClockMatchesSkyfield();
}

TEST(Sgp4PropagatorSimClock, ReplaysVirtualPass) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testSimClockReplaysVirtualPass();
}

TEST(Sgp4PropagatorSimClock, Rate) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testSimClockRate();
}

TEST(Sgp4PropagatorSimClock, RejectsInvalidInput) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testSimClockRejectsInvalidInput();
}

TEST(Sgp4PropagatorSimClock, Stop) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testSimClockStop();
}

TEST(Sgp4PropagatorSimClock, SimObserver) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testSimObserver();
}

TEST(Sgp4PropagatorPass, Predict) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testPredictPass();
}

TEST(Sgp4PropagatorPass, MinElevation) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testPassMinElevation();
}

TEST(Sgp4PropagatorObserver, RejectsOutOfRange) {
    IssTracker::Sgp4PropagatorTester tester;
    tester.testObserverRejectsOutOfRange();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
