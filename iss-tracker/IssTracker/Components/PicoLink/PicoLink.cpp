// ======================================================================
// \title  PicoLink.cpp
// \author tinchomengo
// \brief  cpp file for PicoLink component implementation class
// ======================================================================

#include "IssTracker/Components/PicoLink/PicoLink.hpp"

#include <cstring>

namespace IssTracker {

namespace {

MountState toTelemetry(PicoProtocol::State state) {
    switch (state) {
        case PicoProtocol::State::DISABLED:
            return MountState::DISABLED;
        case PicoProtocol::State::TRACKING:
            return MountState::TRACKING;
        case PicoProtocol::State::STOWING:
            return MountState::STOWING;
        case PicoProtocol::State::STOWED:
            return MountState::STOWED;
        case PicoProtocol::State::UNKNOWN:
        default:
            return MountState::UNKNOWN;
    }
}

}  // namespace

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PicoLink ::PicoLink(const char* const compName) : PicoLinkComponentBase(compName) {}

PicoLink ::~PicoLink() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PicoLink ::drvConnected_handler(FwIndexType portNum) {
    {
        Os::ScopeLock lock(this->m_sendLock);
        this->m_linkOpen = true;
        this->m_heartbeatSet = false;
    }
    this->log_ACTIVITY_HI_LinkOpened();
}

void PicoLink ::drvReceiveIn_handler(FwIndexType portNum, Fw::Buffer& buffer, const Drv::ByteStreamStatus& status) {
    if (status == Drv::ByteStreamStatus::OP_OK) {
        const U8* data = buffer.getData();
        for (FwSizeType i = 0; i < buffer.getSize(); i++) {
            this->processByte(data[i]);
        }
    }
    // The driver loaned us this buffer: always give it back
    this->drvReceiveReturnOut_out(0, buffer);
}

void PicoLink ::mountIn_handler(FwIndexType portNum,
                                const IssTracker::MountAction& mountAction,
                                F64 azimuthDeg,
                                F64 elevationDeg) {
    switch (mountAction.e) {
        case MountAction::ENABLE:
            this->send(PicoProtocol::Command::ENABLE);
            break;
        case MountAction::DISABLE:
            this->send(PicoProtocol::Command::DISABLE);
            break;
        case MountAction::SET:
            this->send(PicoProtocol::Command::SET, azimuthDeg, elevationDeg);
            break;
        case MountAction::STOW:
            this->send(PicoProtocol::Command::STOW);
            break;
        default:
            break;
    }
}

void PicoLink ::schedIn_handler(FwIndexType portNum, U32 context) {
    Os::ScopeLock lock(this->m_sendLock);
    if (!this->m_linkOpen) {
        return;
    }
    if (!this->m_heartbeatSet) {
        // First cycle after the link opened or the Pico rebooted
        this->m_heartbeatSet = this->sendLocked(PicoProtocol::Command::HEARTBEAT, 0.0, 0.0);
    } else if (!this->m_sentThisCycle) {
        // Nothing else went out this cycle: keep the heartbeat alive
        (void)this->sendLocked(PicoProtocol::Command::PING, 0.0, 0.0);
    }
    this->m_sentThisCycle = false;
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PicoLink ::MOUNT_ENABLE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->sendForCommand(opCode, cmdSeq, PicoProtocol::Command::ENABLE, 0.0, 0.0);
}

void PicoLink ::MOUNT_DISABLE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->sendForCommand(opCode, cmdSeq, PicoProtocol::Command::DISABLE, 0.0, 0.0);
}

void PicoLink ::MOUNT_STOW_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->sendForCommand(opCode, cmdSeq, PicoProtocol::Command::STOW, 0.0, 0.0);
}

void PicoLink ::MOUNT_ZERO_AZ_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->sendForCommand(opCode, cmdSeq, PicoProtocol::Command::ZERO_AZ, 0.0, 0.0);
}

void PicoLink ::MOUNT_SET_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, F64 azimuth, F64 elevation) {
    // Limits are checked by the Pico, which answers ERR LIMIT (reported as MountError)
    this->sendForCommand(opCode, cmdSeq, PicoProtocol::Command::SET, azimuth, elevation);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

bool PicoLink ::send(PicoProtocol::Command command, F64 azimuthDeg, F64 elevationDeg) {
    Os::ScopeLock lock(this->m_sendLock);
    return this->sendLocked(command, azimuthDeg, elevationDeg);
}

bool PicoLink ::sendLocked(PicoProtocol::Command command, F64 azimuthDeg, F64 elevationDeg) {
    if (!this->m_linkOpen || !this->isConnected_drvSendOut_OutputPort(0)) {
        this->log_WARNING_LO_LinkNotOpen();
        return false;
    }
    char text[PicoProtocol::MAX_LINE];
    const std::size_t length = PicoProtocol::format(command, azimuthDeg, elevationDeg, text, sizeof(text));
    if (length == 0) {
        return false;
    }
    // The driver writes the bytes before returning; the buffer stays ours
    Fw::Buffer buffer(reinterpret_cast<U8*>(text), static_cast<FwSizeType>(length));
    if (this->drvSendOut_out(0, buffer) != Drv::ByteStreamStatus::OP_OK) {
        this->log_WARNING_HI_SendFailed();
        return false;
    }
    this->m_sentThisCycle = true;
    return true;
}

void PicoLink ::sendForCommand(FwOpcodeType opCode,
                               U32 cmdSeq,
                               PicoProtocol::Command command,
                               F64 azimuthDeg,
                               F64 elevationDeg) {
    const bool sent = this->send(command, azimuthDeg, elevationDeg);
    this->cmdResponse_out(opCode, cmdSeq, sent ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void PicoLink ::processByte(U8 byte) {
    if (byte == '\n' || byte == '\r') {
        if (this->m_lineLength > 0 && !this->m_lineOverflow) {
            this->m_line[this->m_lineLength] = '\0';
            this->handleLine(this->m_line);
        }
        this->m_lineLength = 0;
        this->m_lineOverflow = false;
        return;
    }
    if (this->m_lineLength + 1 < sizeof(this->m_line)) {
        this->m_line[this->m_lineLength++] = static_cast<char>(byte);
    } else {
        // Too long to be a protocol line: drop it
        this->m_lineOverflow = true;
    }
}

void PicoLink ::handleLine(const char* line) {
    const PicoProtocol::Line parsed = PicoProtocol::parse(line);
    switch (parsed.kind) {
        case PicoProtocol::Line::Kind::TLM:
            this->tlmWrite_Azimuth(parsed.azimuthDeg);
            this->tlmWrite_Elevation(parsed.elevationDeg);
            this->tlmWrite_State(toTelemetry(parsed.state));
            break;
        case PicoProtocol::Line::Kind::ERR:
            this->m_errorCount++;
            this->tlmWrite_ErrorCount(this->m_errorCount);
            this->log_WARNING_LO_MountError(Fw::LogStringArg(line));
            break;
        case PicoProtocol::Line::Kind::EVT:
            this->log_ACTIVITY_HI_MountEvent(Fw::LogStringArg(parsed.detail));
            if (std::strcmp(parsed.detail, "BOOT") == 0) {
                // The Pico restarted with its default heartbeat: set it again
                Os::ScopeLock lock(this->m_sendLock);
                this->m_heartbeatSet = false;
            }
            break;
        case PicoProtocol::Line::Kind::OK:
        case PicoProtocol::Line::Kind::OTHER:
        default:
            // OK replies arrive for every SET; nothing to report
            break;
    }
}

}  // namespace IssTracker
