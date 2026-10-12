// ======================================================================
// \title  PicoProtocol.hpp
// \author tinchomengo
// \brief  Formats commands for, and parses lines from, the Pico firmware
//
// Plain C++ with no F´ dependencies, so it can be tested on its own. The
// protocol itself is defined in firmware/pico-tracker/tracker_core.hpp.
// ======================================================================

#ifndef IssTracker_PicoProtocol_HPP
#define IssTracker_PicoProtocol_HPP

#include <cstddef>

namespace IssTracker {
namespace PicoProtocol {

//! Heartbeat timeout requested from the Pico, seconds
constexpr int HEARTBEAT_TIMEOUT_S = 3;

//! Longest line either side sends, including the terminator
constexpr std::size_t MAX_LINE = 64;

enum class Command { ENABLE, DISABLE, STOW, ZERO_AZ, PING, HEARTBEAT, SET };

//! Writes the command line, newline included. Returns its length, or 0 if it didn't fit.
std::size_t format(Command command, double azimuthDeg, double elevationDeg, char* buffer, std::size_t size);

//! Mount state names used in TLM lines
enum class State { UNKNOWN, DISABLED, TRACKING, STOWING, STOWED };

//! One line received from the Pico
struct Line {
    enum class Kind { TLM, OK, ERR, EVT, OTHER } kind = Kind::OTHER;
    double azimuthDeg = 0.0;          //!< TLM only
    double elevationDeg = 0.0;        //!< TLM only
    State state = State::UNKNOWN;     //!< TLM only
    const char* detail = nullptr;     //!< Text after "OK ", "ERR " or "EVT "; points into the parsed line
};

//! Parses one line (without its newline)
Line parse(const char* line);

}  // namespace PicoProtocol
}  // namespace IssTracker

#endif
