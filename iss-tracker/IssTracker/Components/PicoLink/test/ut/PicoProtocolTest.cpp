// ======================================================================
// \title  PicoProtocolTest.cpp
// \author tinchomengo
// \brief  Tests for PicoProtocol formatting and parsing
// ======================================================================

#include <cmath>
#include <string>

#include <gtest/gtest.h>

#include "IssTracker/Components/PicoLink/PicoProtocol.hpp"

namespace IssTracker {

using PicoProtocol::Command;
using PicoProtocol::Line;
using PicoProtocol::State;

namespace {

std::string formatted(Command command, double azimuth = 0.0, double elevation = 0.0) {
    char buffer[PicoProtocol::MAX_LINE];
    const std::size_t length = PicoProtocol::format(command, azimuth, elevation, buffer, sizeof(buffer));
    return std::string(buffer, length);
}

}  // namespace

TEST(PicoProtocolFormat, Commands) {
    EXPECT_EQ(formatted(Command::ENABLE), "ENABLE\n");
    EXPECT_EQ(formatted(Command::DISABLE), "DISABLE\n");
    EXPECT_EQ(formatted(Command::STOW), "STOW\n");
    EXPECT_EQ(formatted(Command::ZERO_AZ), "ZERO_AZ\n");
    EXPECT_EQ(formatted(Command::PING), "PING\n");
    EXPECT_EQ(formatted(Command::HEARTBEAT), "HEARTBEAT 3\n");
    EXPECT_EQ(formatted(Command::SET, -88.3912, 0.0), "SET -88.391 0.000\n");
    EXPECT_EQ(formatted(Command::SET, 0.2969, 69.9993), "SET 0.297 69.999\n");
}

TEST(PicoProtocolFormat, RejectsNonFiniteAndShortBuffers) {
    EXPECT_EQ(formatted(Command::SET, NAN, 10.0), "");
    EXPECT_EQ(formatted(Command::SET, 10.0, INFINITY), "");
    char tiny[4];
    EXPECT_EQ(PicoProtocol::format(Command::ENABLE, 0.0, 0.0, tiny, sizeof(tiny)), 0U);
}

TEST(PicoProtocolParse, Telemetry) {
    Line line = PicoProtocol::parse("TLM -45.00 0.00 TRACKING");
    EXPECT_EQ(line.kind, Line::Kind::TLM);
    EXPECT_DOUBLE_EQ(line.azimuthDeg, -45.0);
    EXPECT_DOUBLE_EQ(line.elevationDeg, 0.0);
    EXPECT_EQ(line.state, State::TRACKING);

    EXPECT_EQ(PicoProtocol::parse("TLM 0.00 90.00 DISABLED").state, State::DISABLED);
    EXPECT_EQ(PicoProtocol::parse("TLM 0.00 90.00 STOWING").state, State::STOWING);
    EXPECT_EQ(PicoProtocol::parse("TLM 0.00 90.00 STOWED").state, State::STOWED);
    EXPECT_EQ(PicoProtocol::parse("TLM 0.00 90.00 SOMETHING").state, State::UNKNOWN);

    // Truncated telemetry isn't trusted
    EXPECT_EQ(PicoProtocol::parse("TLM 12.5").kind, Line::Kind::OTHER);
    EXPECT_EQ(PicoProtocol::parse("TLM").kind, Line::Kind::OTHER);
}

TEST(PicoProtocolParse, RepliesAndEvents) {
    Line line = PicoProtocol::parse("OK SET");
    EXPECT_EQ(line.kind, Line::Kind::OK);
    EXPECT_STREQ(line.detail, "SET");

    line = PicoProtocol::parse("ERR LIMIT");
    EXPECT_EQ(line.kind, Line::Kind::ERR);
    EXPECT_STREQ(line.detail, "LIMIT");

    line = PicoProtocol::parse("EVT HEARTBEAT_LOST");
    EXPECT_EQ(line.kind, Line::Kind::EVT);
    EXPECT_STREQ(line.detail, "HEARTBEAT_LOST");

    EXPECT_EQ(PicoProtocol::parse("").kind, Line::Kind::OTHER);
    EXPECT_EQ(PicoProtocol::parse("garbage").kind, Line::Kind::OTHER);
}

}  // namespace IssTracker
