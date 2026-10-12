// ======================================================================
// \title  AntennaControllerTester.hpp
// \author tinchomengo
// \brief  hpp file for AntennaController component test harness implementation class
// ======================================================================

#ifndef IssTracker_AntennaControllerTester_HPP
#define IssTracker_AntennaControllerTester_HPP

#include "IssTracker/Components/AntennaController/AntennaController.hpp"
#include "IssTracker/Components/AntennaController/AntennaControllerGTestBase.hpp"

namespace IssTracker {

class AntennaControllerTester final : public AntennaControllerGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 100;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object AntennaControllerTester
    AntennaControllerTester();

    //! Destroy object AntennaControllerTester
    ~AntennaControllerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! While idle, look angles don't reach the mount
    void testIdleSendsNothing();

    //! TRACK_START enables the mount once and waits on the horizon
    void testTrackStart();

    //! A full pass: wait, rise, track, set, stow
    void testPassSequence();

    //! TRACK_STOP stows and returns to idle; when already idle it does nothing
    void testTrackStop();

    //! Targets outside the azimuth range are skipped and reported
    void testAzimuthOutOfRange();

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Sends TRACK_START and clears the histories
    void startTracking();

    //! Checks mount command \p index
    void expectMountCommand(FwSizeType index, MountAction::T action, F64 azimuthDeg, F64 elevationDeg);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    AntennaController component;
};

}  // namespace IssTracker

#endif
