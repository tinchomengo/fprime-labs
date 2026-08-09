// ======================================================================
// \title  ImuReader.hpp
// \author tinchomengo
// \brief  hpp file for ImuReader component implementation class
// ======================================================================

#ifndef ImuAltitudeVisualization_ImuReader_HPP
#define ImuAltitudeVisualization_ImuReader_HPP

#include "ImuAltitudeVisualization/Components/ImuReader/ImuReaderComponentAc.hpp"

namespace ImuAltitudeVisualization {

class ImuReader final : public ImuReaderComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ImuReader object
    ImuReader(const char* const compName  //!< The component name
    );

    //! Destroy ImuReader object
    ~ImuReader();

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

    // ----------------------------------------------------------------------
    // Line buffering + parsing
    // ----------------------------------------------------------------------

    //! Appends one incoming byte to the line buffer, or - on '\n' - parses
    //! the accumulated line and resets the buffer for the next one.
    void processByte(U8 byte);

    //! Parses a complete, NUL-terminated line as "roll,pitch,yaw" and
    //! writes telemetry on success. Anything that doesn't parse as three
    //! numbers (e.g. the firmware's "ERR" or startup diagnostic lines) is
    //! silently ignored.
    void parseLine(const char* line);

    static constexpr FwSizeType LINE_BUFFER_SIZE = 128;
    char m_lineBuffer[LINE_BUFFER_SIZE];
    FwSizeType m_lineLength = 0;
};

}  // namespace ImuAltitudeVisualization

#endif
