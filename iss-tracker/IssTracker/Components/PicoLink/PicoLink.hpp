// ======================================================================
// \title  PicoLink.hpp
// \author tinchomengo
// \brief  hpp file for PicoLink component implementation class
// ======================================================================

#ifndef IssTracker_PicoLink_HPP
#define IssTracker_PicoLink_HPP

#include <Os/Mutex.hpp>

#include "IssTracker/Components/PicoLink/PicoLinkComponentAc.hpp"
#include "IssTracker/Components/PicoLink/PicoProtocol.hpp"

namespace IssTracker {

class PicoLink final : public PicoLinkComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PicoLink object
    PicoLink(const char* const compName  //!< The component name
    );

    //! Destroy PicoLink object
    ~PicoLink();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for drvConnected
    //!
    //! Ready signal when driver is connected
    void drvConnected_handler(FwIndexType portNum  //!< The port number
                              ) override;

    //! Handler implementation for drvReceiveIn
    //!
    //! Receive (read) data from driver.
    void drvReceiveIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& buffer,
                              const Drv::ByteStreamStatus& status) override;

    //! Handler implementation for mountIn
    //!
    //! Mount commands from the AntennaController
    void mountIn_handler(FwIndexType portNum,                   //!< The port number
                         const IssTracker::MountAction& mountAction,  //!< What to do
                         F64 azimuthDeg,                        //!< Mount azimuth, degrees
                         F64 elevationDeg                       //!< Degrees above the horizon
                         ) override;

    //! Handler implementation for schedIn
    //!
    //! Called by the rate group: keeps the Pico's heartbeat alive
    void schedIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command MOUNT_ENABLE
    void MOUNT_ENABLE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                 U32 cmdSeq            //!< The command sequence number
                                 ) override;

    //! Handler implementation for command MOUNT_DISABLE
    void MOUNT_DISABLE_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                  U32 cmdSeq            //!< The command sequence number
                                  ) override;

    //! Handler implementation for command MOUNT_STOW
    void MOUNT_STOW_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq            //!< The command sequence number
                               ) override;

    //! Handler implementation for command MOUNT_ZERO_AZ
    void MOUNT_ZERO_AZ_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                  U32 cmdSeq            //!< The command sequence number
                                  ) override;

    //! Handler implementation for command MOUNT_SET
    void MOUNT_SET_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                              U32 cmdSeq,           //!< The command sequence number
                              F64 azimuth,          //!< Degrees from mount zero
                              F64 elevation         //!< Degrees above the horizon
                              ) override;

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Sends a command line to the Pico. Returns false if it wasn't sent.
    bool send(PicoProtocol::Command command, F64 azimuthDeg = 0.0, F64 elevationDeg = 0.0);

    //! send(), for callers already holding m_sendLock
    bool sendLocked(PicoProtocol::Command command, F64 azimuthDeg, F64 elevationDeg);

    //! Sends a command on behalf of a ground command and responds to it
    void sendForCommand(FwOpcodeType opCode, U32 cmdSeq, PicoProtocol::Command command, F64 azimuthDeg, F64 elevationDeg);

    //! Appends one received byte; handles the line when it ends
    void processByte(U8 byte);

    //! Handles one complete line from the Pico
    void handleLine(const char* line);

    // ----------------------------------------------------------------------
    // State
    // ----------------------------------------------------------------------

    //! Sends come from the rate group, the command dispatcher and the
    //! receive thread's reaction to a reboot; this keeps them one at a time
    //! and protects the flags below.
    Os::Mutex m_sendLock;
    bool m_linkOpen = false;       //!< The driver reported the port open
    bool m_heartbeatSet = false;   //!< HEARTBEAT sent since the link opened or the Pico rebooted
    bool m_sentThisCycle = false;  //!< A line was sent since the last schedIn
    U32 m_cyclesWithoutData = 0;   //!< schedIn cycles since the last line from the Pico
    bool m_silenceReported = false;  //!< PicoSilent emitted for the current silence

  public:
    //! Rate group cycles (seconds) without data before PicoSilent
    static constexpr U32 SILENCE_LIMIT_CYCLES = 3;

  private:

    // Receive state, only used on the driver's receive thread
    char m_line[PicoProtocol::MAX_LINE];
    FwSizeType m_lineLength = 0;
    bool m_lineOverflow = false;
    U32 m_errorCount = 0;
};

}  // namespace IssTracker

#endif
