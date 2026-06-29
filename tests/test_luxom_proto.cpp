// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Luigi Corsaro
// Host unit tests for the pure logic in ../luxom_proto.h (the same code the ESPHome
// lambdas call). No framework: compile with a normal C++17 compiler and run.
//   c++ -std=c++17 tests/test_luxom_proto.cpp -o /tmp/luxom_tests && /tmp/luxom_tests
#include "../luxom_proto.h"
#include <iostream>
#include <string>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
  std::cerr << "FAIL: " #cond " (line " << __LINE__ << ")\n"; failures++; } } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); std::string _b = (b); if (!(std::string(_a) == _b)) { \
  std::cerr << "FAIL: " #a " == " #b " | got [" << _a << "] want [" << _b << "] (line " << __LINE__ << ")\n"; failures++; } } while (0)
#define CHECK_INT(a, b) do { long _a = (a); long _b = (b); if (_a != _b) { \
  std::cerr << "FAIL: " #a " == " #b " | got " << _a << " want " << _b << " (line " << __LINE__ << ")\n"; failures++; } } while (0)

static bool has(const std::string &hay, const std::string &needle) {
  return hay.find(needle) != std::string::npos;
}

int main() {
  using namespace luxom;

  // ---- protocol constants (lock the values) ----
  CHECK_EQ(frame::PW_REQUEST, "@1*PW-");
  CHECK_EQ(frame::MODULE_INFO, "*!");
  CHECK_EQ(frame::HEARTBEAT, "*U");
  CHECK_EQ(frame::ACK, "@1*V");
  CHECK_EQ(frame::IN_ON, "@1*S");
  CHECK_EQ(frame::IN_OFF, "@1*C");
  CHECK_EQ(frame::IN_DIM, "@1*A");
  CHECK_EQ(frame::IN_LEVEL, "@1*Z");
  CHECK_EQ(cmd::REQUEST_INFO, "*?");
  CHECK_EQ(cmd::SET, "*S,0,");
  CHECK_EQ(cmd::CLEAR, "*C,0,");
  CHECK_EQ(cmd::PING, "*P,0,");
  CHECK_EQ(cmd::DIM_PREAMBLE, "*A,0,");
  CHECK_EQ(cmd::LEVEL, "*Z,0");

  // ---- sanitize / desanitize ----
  CHECK_EQ(sanitize("2,03"), "2_03");
  CHECK_EQ(sanitize("A,02"), "A_02");
  CHECK_EQ(desanitize("2_03"), "2,03");
  CHECK_EQ(desanitize(sanitize("12,07")), "12,07");

  // ---- dimmer conversion ----
  CHECK_EQ(pct_to_hex(0), "00");
  CHECK_EQ(pct_to_hex(100), "FF");
  CHECK_EQ(pct_to_hex(50), "80");      // ceil(127.5) = 128
  CHECK_EQ(pct_to_hex(1), "03");       // ceil(2.55) = 3
  CHECK_EQ(pct_to_hex(150), "FF");     // clamped
  CHECK_EQ(pct_to_hex(-5), "00");      // clamped
  CHECK_INT(hex_to_pct("FF"), 100);
  CHECK_INT(hex_to_pct("00"), 0);
  CHECK_INT(hex_to_pct("80"), 50);
  CHECK_INT(hex_to_pct("080"), 50);    // leading zero harmless
  // round-trip within +/-1%
  for (int p = 0; p <= 100; p++) {
    int back = hex_to_pct(pct_to_hex(p));
    CHECK(back >= p - 1 && back <= p + 1);
  }

  // ---- parse_frame ----
  {
    Frame a = parse_frame("@1*S,0,2,01");
    CHECK_EQ(a.op, "@1*S"); CHECK(a.has_addr); CHECK_EQ(a.addr, "2,01");
    Frame b = parse_frame("@1*A,0,A,02");
    CHECK_EQ(b.op, "@1*A"); CHECK(b.has_addr); CHECK_EQ(b.addr, "A,02");
    Frame z = parse_frame("@1*Z,057");
    CHECK_EQ(z.op, "@1*Z"); CHECK(!z.has_addr);
    Frame u = parse_frame("*U");
    CHECK_EQ(u.op, "*U"); CHECK(!u.has_addr);
  }

  // ---- command topics ----
  CHECK_EQ(topic_oid("luxom/2_01/set"), "2_01");
  CHECK_EQ(topic_oid("luxom/A_02/bri/set"), "A_02");
  CHECK_EQ(topic_oid("garbage"), "");
  CHECK_EQ(cover_cid_from_topic("luxom/cover/2_03__2_04/set"), "2_03__2_04");
  CHECK_EQ(cover_cid_from_topic("luxom/cover/x/set"), "x");

  // ---- cover id <-> up/down ----
  CHECK_EQ(cover_cid("2,03", "2,04"), "2_03__2_04");
  {
    UpDown ud = cover_parse_cid("2_03__2_04");
    CHECK(ud.ok); CHECK_EQ(ud.up, "2,03"); CHECK_EQ(ud.down, "2,04");
    UpDown bad = cover_parse_cid("nope");
    CHECK(!bad.ok);
  }

  // ---- parse_covers ----
  {
    auto v = parse_covers("Kitchen|2,03|2,04 ; Bedroom|2,05|2,06");
    CHECK_INT(v.size(), 2);
    CHECK_EQ(v[0].name, "Kitchen"); CHECK_EQ(v[0].up, "2,03"); CHECK_EQ(v[0].down, "2,04");
    CHECK_EQ(v[1].name, "Bedroom"); CHECK_EQ(v[1].up, "2,05"); CHECK_EQ(v[1].down, "2,06");
    CHECK_INT(parse_covers("").size(), 0);
    CHECK_INT(parse_covers("Broken|1,01").size(), 0);   // missing third field
    CHECK_INT(parse_covers("  Hall | 3,01 | 3,02 ").size(), 1);  // spaces trimmed
  }

  // ---- discovery payloads ----
  {
    std::string sw = entity_config("2,01", false);
    CHECK(has(sw, "\"uniq_id\":\"luxom_2_01\""));
    CHECK(has(sw, "\"stat_t\":\"luxom/2_01/state\""));
    CHECK(has(sw, "\"cmd_t\":\"luxom/2_01/set\""));
    CHECK(!has(sw, "bri_cmd_t"));                       // switch has no brightness
    std::string dim = entity_config("A,02", true);
    CHECK(has(dim, "\"bri_cmd_t\":\"luxom/A_02/bri/set\""));
    CHECK(has(dim, "\"on_cmd_type\":\"brightness\""));
    CHECK_EQ(entity_config_topic("2,01", false), "homeassistant/switch/luxom_2_01/config");
    CHECK_EQ(entity_config_topic("A,02", true), "homeassistant/light/luxom_A_02/config");

    std::string cov = cover_config("Kitchen", "2,03", "2,04");
    CHECK(has(cov, "\"uniq_id\":\"luxom_cover_2_03__2_04\""));
    CHECK(has(cov, "\"pl_open\":\"OPEN\""));
    CHECK(has(cov, "\"optimistic\":true"));
    CHECK_EQ(cover_config_topic("2,03", "2,04"), "homeassistant/cover/luxom_2_03__2_04/config");
  }

  if (failures) { std::cerr << failures << " test(s) FAILED\n"; return 1; }
  std::cout << "all luxom_proto tests passed\n";
  return 0;
}
