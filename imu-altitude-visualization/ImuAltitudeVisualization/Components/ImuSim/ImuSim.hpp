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
};

}  // namespace ImuAltitudeVisualization

#endif
