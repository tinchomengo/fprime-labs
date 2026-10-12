// ======================================================================
// \title  PicoProtocol.cpp
// \author tinchomengo
// \brief  Formats commands for, and parses lines from, the Pico firmware
// ======================================================================

#include "PicoProtocol.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace IssTracker {
namespace PicoProtocol {

namespace {

State parseState(const char* name) {
    if (std::strcmp(name, "DISABLED") == 0) {
        return State::DISABLED;
    }
    if (std::strcmp(name, "TRACKING") == 0) {
        return State::TRACKING;
    }
    if (std::strcmp(name, "STOWING") == 0) {
        return State::STOWING;
    }
    if (std::strcmp(name, "STOWED") == 0) {
        return State::STOWED;
    }
    return State::UNKNOWN;
}

//! If \p line starts with \p prefix, returns the text after it, else nullptr
const char* after(const char* line, const char* prefix) {
    const std::size_t length = std::strlen(prefix);
    return std::strncmp(line, prefix, length) == 0 ? line + length : nullptr;
}

}  // namespace

std::size_t format(Command command, double azimuthDeg, double elevationDeg, char* buffer, std::size_t size) {
    int written = 0;
    switch (command) {
        case Command::ENABLE:
            written = std::snprintf(buffer, size, "ENABLE\n");
            break;
        case Command::DISABLE:
            written = std::snprintf(buffer, size, "DISABLE\n");
            break;
        case Command::STOW:
            written = std::snprintf(buffer, size, "STOW\n");
            break;
        case Command::ZERO_AZ:
            written = std::snprintf(buffer, size, "ZERO_AZ\n");
            break;
        case Command::PING:
            written = std::snprintf(buffer, size, "PING\n");
            break;
        case Command::HEARTBEAT:
            written = std::snprintf(buffer, size, "HEARTBEAT %d\n", HEARTBEAT_TIMEOUT_S);
            break;
        case Command::SET:
            // The Pico rejects NaN and infinity; don't send them at all
            if (!std::isfinite(azimuthDeg) || !std::isfinite(elevationDeg)) {
                return 0;
            }
            written = std::snprintf(buffer, size, "SET %.3f %.3f\n", azimuthDeg, elevationDeg);
            break;
    }
    if (written <= 0 || static_cast<std::size_t>(written) >= size) {
        return 0;
    }
    return static_cast<std::size_t>(written);
}

Line parse(const char* line) {
    Line result;
    const char* rest = nullptr;

    if ((rest = after(line, "TLM ")) != nullptr) {
        double azimuth = 0.0;
        double elevation = 0.0;
        char state[16] = {};
        if (std::sscanf(rest, "%lf %lf %15s", &azimuth, &elevation, state) == 3) {
            result.kind = Line::Kind::TLM;
            result.azimuthDeg = azimuth;
            result.elevationDeg = elevation;
            result.state = parseState(state);
        }
    } else if ((rest = after(line, "OK ")) != nullptr) {
        result.kind = Line::Kind::OK;
        result.detail = rest;
    } else if ((rest = after(line, "ERR ")) != nullptr) {
        result.kind = Line::Kind::ERR;
        result.detail = rest;
    } else if ((rest = after(line, "EVT ")) != nullptr) {
        result.kind = Line::Kind::EVT;
        result.detail = rest;
    }
    return result;
}

}  // namespace PicoProtocol
}  // namespace IssTracker
