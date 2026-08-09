// ======================================================================
// \title  ImuReader.cpp
// \author tinchomengo
// \brief  cpp file for ImuReader component implementation class
// ======================================================================

#include "ImuAltitudeVisualization/Components/ImuReader/ImuReader.hpp"
#include <cstdio>

namespace ImuAltitudeVisualization {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

ImuReader ::ImuReader(const char* const compName) : ImuReaderComponentBase(compName) {}

ImuReader ::~ImuReader() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void ImuReader ::drvConnected_handler(FwIndexType portNum) {
    // No action needed - the driver is ready to receive as soon as it's
    // opened; this just signals that fact.
}

void ImuReader ::drvReceiveIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {
    if (status == Drv::ByteStreamStatus::OP_OK) {
        const U8* data = buffer.getData();
        const FwSizeType size = buffer.getSize();
        for (FwSizeType i = 0; i < size; i++) {
            this->processByte(data[i]);
        }
    }
    // The driver loaned us this buffer - always give it back, regardless
    // of whether the data in it was usable.
    this->drvReceiveReturnOut_out(0, buffer);
}

// ----------------------------------------------------------------------
// Line buffering + parsing
// ----------------------------------------------------------------------

void ImuReader ::processByte(U8 byte) {
    if (byte == '\n') {
        this->m_lineBuffer[this->m_lineLength] = '\0';
        this->parseLine(this->m_lineBuffer);
        this->m_lineLength = 0;
        return;
    }
    if (byte == '\r') {
        return;  // ignore, in case the sender uses CRLF line endings
    }
    if (this->m_lineLength >= LINE_BUFFER_SIZE - 1) {
        // Line too long (e.g. noise with no newline) - drop it and start
        // fresh rather than parsing garbage.
        this->m_lineLength = 0;
        return;
    }
    this->m_lineBuffer[this->m_lineLength] = static_cast<char>(byte);
    this->m_lineLength++;
}

void ImuReader ::parseLine(const char* line) {
    F64 roll = 0.0;
    F64 pitch = 0.0;
    F64 yaw = 0.0;
    // Matches "roll,pitch,yaw" (e.g. from the Pico firmware). Anything
    // else - "ERR", startup diagnostics, a partial/corrupted line - simply
    // doesn't match all three conversions and is ignored.
    const int parsed = sscanf(line, "%lf,%lf,%lf", &roll, &pitch, &yaw);
    if (parsed == 3) {
        this->tlmWrite_Roll(roll);
        this->tlmWrite_Pitch(pitch);
        this->tlmWrite_Yaw(yaw);
    }
}

}  // namespace ImuAltitudeVisualization
