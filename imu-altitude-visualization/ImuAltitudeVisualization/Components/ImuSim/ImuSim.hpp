// ======================================================================
// \title  ImuSim.hpp
// \author tinchomengo
// \brief  hpp file for ImuSim component implementation class
// ======================================================================

#ifndef ImuAltitudeVisualization_ImuSim_HPP
#define ImuAltitudeVisualization_ImuSim_HPP

#include "ImuAltitudeVisualization/Components/ImuSim/ImuSimComponentAc.hpp"

namespace ImuAltitudeVisualization {

class ImuSim final : public ImuSimComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ImuSim object
    ImuSim(const char* const compName  //!< The component name
    );

    //! Destroy ImuSim object
    ~ImuSim();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Scheduled input port, called by the rate group to trigger a new simulated reading
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

    Fw::Time m_startTime;  //!< Time of the first tick, used to compute elapsed simulation time
    bool m_started = false;  //!< Whether m_startTime has been set yet
};

}  // namespace ImuAltitudeVisualization

#endif
