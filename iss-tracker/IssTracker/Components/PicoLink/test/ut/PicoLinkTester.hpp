// ======================================================================
// \title  PicoLinkTester.hpp
// \author tinchomengo
// \brief  hpp file for PicoLink component test harness implementation class
// ======================================================================

#ifndef IssTracker_PicoLinkTester_HPP
#define IssTracker_PicoLinkTester_HPP

#include <string>
#include <vector>

#include "IssTracker/Components/PicoLink/PicoLink.hpp"
#include "IssTracker/Components/PicoLink/PicoLinkGTestBase.hpp"

namespace IssTracker {

class PicoLinkTester final : public PicoLinkGTestBase {
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

    //! Construct object PicoLinkTester
    PicoLinkTester();

    //! Destroy object PicoLinkTester
    ~PicoLinkTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Before the link opens nothing is sent and commands fail
    void testLinkNotOpen();

    //! After the link opens: HEARTBEAT first, then PING on otherwise quiet cycles
    void testHeartbeat();

    //! Mount commands from the AntennaController become protocol lines
    void testMountCommands();

    //! Ground commands become protocol lines
    void testGroundCommands();

    //! TLM, ERR and EVT lines become telemetry and events, even when split across reads
    void testReceive();

    //! The heartbeat timeout is set again after the Pico reboots
    void testRebootResetsHeartbeat();

    //! A failed send is reported and fails the command
    void testSendFailure();

    //! Over-long lines are dropped without losing the next one
    void testLongLineDropped();

  private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports
    // ----------------------------------------------------------------------

    //! Records the line the component sends to the driver
    Drv::ByteStreamStatus from_drvSendOut_handler(const FwIndexType portNum, Fw::Buffer& sendBuffer);

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Opens the link and runs the first cycle (which sends HEARTBEAT)
    void openLink();

    //! Delivers \p text as if the driver had read it from the Pico
    void receive(const std::string& text);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    PicoLink component;

    //! Lines sent to the driver, copied when sent (the buffer belongs to the sender)
    std::vector<std::string> m_sent;

    //! Status the fake driver returns for sends
    Drv::ByteStreamStatus m_sendStatus = Drv::ByteStreamStatus::OP_OK;
};

}  // namespace IssTracker

#endif
