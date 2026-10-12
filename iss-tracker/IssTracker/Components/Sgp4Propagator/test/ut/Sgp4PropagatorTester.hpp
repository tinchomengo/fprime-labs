// ======================================================================
// \title  Sgp4PropagatorTester.hpp
// \author tinchomengo
// \brief  hpp file for Sgp4Propagator component test harness implementation class
// ======================================================================

#ifndef IssTracker_Sgp4PropagatorTester_HPP
#define IssTracker_Sgp4PropagatorTester_HPP

#include "IssTracker/Components/Sgp4Propagator/Sgp4Propagator.hpp"
#include "IssTracker/Components/Sgp4Propagator/Sgp4PropagatorGTestBase.hpp"
#include "OracleData.hpp"

namespace IssTracker {

class Sgp4PropagatorTester final : public Sgp4PropagatorGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 100;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Wall clock used by most tests: 2026-10-11T23:00:00 UTC
    static constexpr F64 WALL_CLOCK_UNIX = 1791759600.0;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object Sgp4PropagatorTester
    Sgp4PropagatorTester();

    //! Destroy object Sgp4PropagatorTester
    ~Sgp4PropagatorTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! LOAD_TLE accepts a valid TLE and reports its epoch and age
    void testLoadTle();

    //! LOAD_TLE rejects garbage and keeps the TLE already loaded
    void testLoadTleRejectsGarbage();

    //! LOAD_TLE ignores trailing whitespace from copy-paste
    void testLoadTleIgnoresTrailingWhitespace();

    //! With no TLE, the rate group warns once and PREDICT_PASS fails
    void testNoTle();

    //! On the real clock, the look angle matches Skyfield
    void testRealClockMatchesSkyfield();

    //! The simulated clock replays the virtual 70 deg pass at 1x
    void testSimClockReplaysVirtualPass();

    //! The simulated clock rate multiplies elapsed wall time
    void testSimClockRate();

    //! SIM_CLOCK_START rejects a malformed start time and out-of-range rates
    void testSimClockRejectsInvalidInput();

    //! SIM_CLOCK_STOP returns to the real clock
    void testSimClockStop();

    //! PREDICT_PASS finds the virtual pass, and reports when there is none
    void testPredictPass();

    //! Out-of-range observer parameters are rejected and the old location is kept
    void testObserverRejectsOutOfRange();

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Sets the observer parameters and loads them into the component
    void setObserver(F64 latitude, F64 longitude, F64 altitude);

    //! Sets the time the component reads from its time port
    void setWallClock(F64 unixSeconds);

    //! Loads the reference ISS TLE and clears the histories
    void loadIssTle();

    //! Starts the simulated clock and clears the histories
    void startSimClock(const char* startUtc, F64 rate);

    //! Runs one rate group cycle and checks the look angle against \p sample
    void expectLookAngle(const Oracle::LookSample& sample);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    Sgp4Propagator component;
};

}  // namespace IssTracker

#endif
