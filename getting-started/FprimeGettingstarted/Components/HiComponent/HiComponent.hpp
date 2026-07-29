// ======================================================================
// \title  HiComponent.hpp
// \author tinchomengo
// \brief  hpp file for HiComponent component implementation class
// ======================================================================

#ifndef FprimeGettingstarted_HiComponent_HPP
#define FprimeGettingstarted_HiComponent_HPP

#include "FprimeGettingstarted/Components/HiComponent/HiComponentComponentAc.hpp"

namespace FprimeGettingstarted {

class HiComponent final : public HiComponentComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct HiComponent object
    HiComponent(const char* const compName  //!< The component name
    );

    //! Destroy HiComponent object
    ~HiComponent();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SAY_HI
    //!
    //! TODO
    //! Command to issue greeting with maximum length of 20 characters
    void SAY_HI_cmdHandler(FwOpcodeType opCode,              //!< The opcode
                           U32 cmdSeq,                       //!< The command sequence number
                           const Fw::CmdStringArg& greeting  //!< Greeting to repeat in the SayHiEvent event
                           ) override;
    private:
      U32 m_greetingCount = 0;
};

}  // namespace FprimeGettingstarted

#endif
