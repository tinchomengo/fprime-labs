// ======================================================================
// \title  AntennaController.hpp
// \author tinchomengo
// \brief  hpp file for AntennaController component implementation class
// ======================================================================

#ifndef IssTracker_AntennaController_HPP
#define IssTracker_AntennaController_HPP

#include "IssTracker/Components/AntennaController/AntennaControllerComponentAc.hpp"
#include "IssTracker/Components/AntennaController/TrackLogic.hpp"

namespace IssTracker {

class AntennaController final : public AntennaControllerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct AntennaController object
    AntennaController(const char* const compName  //!< The component name
    );

    //! Destroy AntennaController object
    ~AntennaController();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for lookAngleIn
    //!
    //! Look angle from the Sgp4Propagator, once per rate group cycle
    void lookAngleIn_handler(FwIndexType portNum,  //!< The port number
                             F64 azimuthDeg,       //!< Degrees clockwise from north, 0 to 360
                             F64 elevationDeg      //!< Degrees above the horizon, negative below it
                             ) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command TRACK_START
    void TRACK_START_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                U32 cmdSeq            //!< The command sequence number
                                ) override;

    //! Handler implementation for command TRACK_STOP
    void TRACK_STOP_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq            //!< The command sequence number
                               ) override;

    // ----------------------------------------------------------------------
    // Parameter hooks
    // ----------------------------------------------------------------------

    //! Called once the parameters have been loaded at startup
    void parametersLoaded() override;

    //! Called whenever a parameter is set by command
    void parameterUpdated(FwPrmIdType id) override;

    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Reads MOUNT_ZERO_AZ_DEG and applies it if it's in range
    void updateMountZero();

    //! Sends a command to the mount, if the port is connected
    void sendToMount(MountAction::T action, F64 azimuthDeg, F64 elevationDeg);

    //! Updates the phase and its telemetry
    void setPhase(TrackLogic::Phase phase);

    TrackLogic::Phase m_phase = TrackLogic::Phase::IDLE;
    F64 m_mountZeroDeg = 0.0;  //!< Compass direction the mount's zero faces
};

}  // namespace IssTracker

#endif
