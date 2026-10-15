// ======================================================================
// \title  Sgp4Propagator.hpp
// \author tinchomengo
// \brief  hpp file for Sgp4Propagator component implementation class
// ======================================================================

#ifndef IssTracker_Sgp4Propagator_HPP
#define IssTracker_Sgp4Propagator_HPP

#include <memory>
#include <string>

#include <libsgp4/SGP4.h>

#include "IssTracker/Components/Sgp4Propagator/OrbitMath.hpp"
#include "IssTracker/Components/Sgp4Propagator/Sgp4PropagatorComponentAc.hpp"

namespace IssTracker {

class Sgp4Propagator final : public Sgp4PropagatorComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Sgp4Propagator object
    Sgp4Propagator(const char* const compName  //!< The component name
    );

    //! Destroy Sgp4Propagator object
    ~Sgp4Propagator();

    //! Length of the PREDICT_PASS search window
    static constexpr U32 PASS_SEARCH_HOURS = 24;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for schedIn
    //!
    //! Called by the rate group to propagate and publish the current look angle
    void schedIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command LOAD_TLE
    void LOAD_TLE_cmdHandler(FwOpcodeType opCode,              //!< The opcode
                             U32 cmdSeq,                       //!< The command sequence number
                             const Fw::CmdStringArg& line1,    //!< TLE line 1
                             const Fw::CmdStringArg& line2     //!< TLE line 2
                             ) override;

    //! Handler implementation for command LOAD_TLE_FILE
    void LOAD_TLE_FILE_cmdHandler(FwOpcodeType opCode,           //!< The opcode
                                  U32 cmdSeq,                    //!< The command sequence number
                                  const Fw::CmdStringArg& path   //!< TLE file path
                                  ) override;

    //! Handler implementation for command SIM_CLOCK_START
    void SIM_CLOCK_START_cmdHandler(FwOpcodeType opCode,               //!< The opcode
                                    U32 cmdSeq,                        //!< The command sequence number
                                    const Fw::CmdStringArg& startUtc,  //!< Start time, YYYY-MM-DDTHH:MM:SS
                                    F64 rate                           //!< Simulated seconds per wall second
                                    ) override;

    //! Handler implementation for command SIM_OBSERVER
    void SIM_OBSERVER_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                 U32 cmdSeq,           //!< The command sequence number
                                 F64 latitude,         //!< Degrees
                                 F64 longitude,        //!< Degrees
                                 F64 altitude          //!< Metres
                                 ) override;

    //! Handler implementation for command SIM_CLOCK_STOP
    void SIM_CLOCK_STOP_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                   U32 cmdSeq            //!< The command sequence number
                                   ) override;

    //! Handler implementation for command PREDICT_PASS
    void PREDICT_PASS_cmdHandler(FwOpcodeType opCode,  //!< The opcode
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

    //! Validates and loads a TLE, reporting the result as events.
    //! The current TLE is kept if the new one is rejected.
    bool loadTle(const std::string& line1, const std::string& line2);

    //! Current wall-clock time as Unix seconds
    F64 wallClockSeconds();

    //! Time to propagate to: the wall clock, or the simulated clock if active
    F64 propagationSeconds();

    //! Reads the observer parameters and applies them if they're in range
    void updateObserver();

    //! Reads PASS_MIN_ELEVATION_DEG and applies it if it's in range
    void updateMinPassElevation();

    //! The simulated observer while one is set, else the observer parameters
    const OrbitMath::Observer& activeObserver() const;

    //! Whether a location is a valid observer
    static bool validObserver(F64 latitude, F64 longitude, F64 altitude);

    // ----------------------------------------------------------------------
    // State
    // ----------------------------------------------------------------------

    //! Propagator for the loaded TLE; empty until LOAD_TLE succeeds.
    //! Allocated only when a TLE is loaded, never in the rate group path.
    std::unique_ptr<libsgp4::SGP4> m_sgp4;
    F64 m_tleEpochSeconds = 0.0;  //!< TLE epoch, Unix seconds

    OrbitMath::Observer m_observer;     //!< Last valid observer location from the parameters
    OrbitMath::Observer m_simObserver;  //!< Simulated observer, used while m_simObserverActive
    bool m_simObserverActive = false;

    F64 m_minPassElevationDeg = 10.0;  //!< PASS_MIN_ELEVATION_DEG; matches its FPP default

    bool m_simActive = false;         //!< Simulated clock in use
    F64 m_simStartSeconds = 0.0;      //!< Simulated time at the moment it started
    F64 m_simWallStartSeconds = 0.0;  //!< Wall-clock time at the moment it started
    F64 m_simRate = 1.0;              //!< Simulated seconds per wall-clock second

    bool m_noTleReported = false;  //!< NoTleLoaded already emitted by the rate group
};

}  // namespace IssTracker

#endif
