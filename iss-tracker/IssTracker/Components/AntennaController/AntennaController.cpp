// ======================================================================
// \title  AntennaController.cpp
// \author tinchomengo
// \brief  cpp file for AntennaController component implementation class
// ======================================================================

#include "IssTracker/Components/AntennaController/AntennaController.hpp"

namespace IssTracker {

namespace {

TrackPhase toTelemetry(TrackLogic::Phase phase) {
    switch (phase) {
        case TrackLogic::Phase::WAITING:
            return TrackPhase::WAITING;
        case TrackLogic::Phase::TRACKING:
            return TrackPhase::TRACKING;
        case TrackLogic::Phase::IDLE:
        default:
            return TrackPhase::IDLE;
    }
}

}  // namespace

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

AntennaController ::AntennaController(const char* const compName) : AntennaControllerComponentBase(compName) {}

AntennaController ::~AntennaController() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void AntennaController ::lookAngleIn_handler(FwIndexType portNum, F64 azimuthDeg, F64 elevationDeg) {
    const TrackLogic::Decision decision = TrackLogic::decide(this->m_phase, azimuthDeg, elevationDeg);

    if (this->m_phase == TrackLogic::Phase::WAITING && decision.nextPhase == TrackLogic::Phase::TRACKING) {
        this->log_ACTIVITY_HI_PassStarted(TrackLogic::mountAzimuth(azimuthDeg));
    }

    switch (decision.action) {
        case TrackLogic::Decision::Action::POINT:
            this->sendToMount(MountAction::SET, decision.azimuthDeg, decision.elevationDeg);
            this->tlmWrite_TargetAzimuth(decision.azimuthDeg);
            this->tlmWrite_TargetElevation(decision.elevationDeg);
            break;
        case TrackLogic::Decision::Action::STOW:
            this->sendToMount(MountAction::STOW, 0.0, 0.0);
            this->log_ACTIVITY_HI_PassComplete();
            break;
        case TrackLogic::Decision::Action::NONE:
        default:
            break;
    }

    if (decision.azimuthOutOfRange) {
        this->log_WARNING_LO_AzimuthOutOfRange(TrackLogic::mountAzimuth(azimuthDeg));
    }

    this->setPhase(decision.nextPhase);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void AntennaController ::TRACK_START_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (this->m_phase == TrackLogic::Phase::IDLE) {
        this->sendToMount(MountAction::ENABLE, 0.0, 0.0);
        this->setPhase(TrackLogic::Phase::WAITING);
        this->log_ACTIVITY_HI_TrackingStarted();
    }
    // Already tracking: nothing to change
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void AntennaController ::TRACK_STOP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->sendToMount(MountAction::STOW, 0.0, 0.0);
    this->setPhase(TrackLogic::Phase::IDLE);
    this->log_ACTIVITY_HI_TrackingStopped();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void AntennaController ::sendToMount(MountAction::T action, F64 azimuthDeg, F64 elevationDeg) {
    if (this->isConnected_mountOut_OutputPort(0)) {
        this->mountOut_out(0, MountAction(action), azimuthDeg, elevationDeg);
    }
}

void AntennaController ::setPhase(TrackLogic::Phase phase) {
    this->m_phase = phase;
    this->tlmWrite_Phase(toTelemetry(phase));
}

}  // namespace IssTracker
