// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Luigi Corsaro
// Pure Luxom protocol/helper logic — NO ESPHome/Arduino dependencies.
// Shared by the ESPHome lambdas (via `esphome: includes:`) AND by the host unit
// tests (tests/test_luxom_proto.cpp), so the tests cover the real production code.
// Plain C++17 + std only.
#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace luxom {

// ---- Luxom protocol strings (named constants) -----------------------------
namespace frame {   // inbound frames / opcodes (interface -> gateway)
  inline const std::string PW_REQUEST = "@1*PW-";   // password request (reply with cmd::REQUEST_INFO)
  inline const std::string MODULE_INFO = "*!";       // module-info prefix (match with rfind(.,0))
  inline const std::string HEARTBEAT = "*U";         // application heartbeat (no-op)
  inline const std::string ACK = "@1*V";             // acknowledge (no-op)
  inline const std::string IN_ON = "@1*S";           // state ON
  inline const std::string IN_OFF = "@1*C";          // state OFF
  inline const std::string IN_DIM = "@1*A";          // dimmer preamble (address; @1*Z follows)
  inline const std::string IN_LEVEL = "@1*Z";        // dimmer level byte
}
namespace cmd {     // outbound commands (gateway -> interface); append address/level
  inline const std::string REQUEST_INFO = "*?";      // reply to frame::PW_REQUEST
  inline const std::string SET = "*S,0,";            // set / on   -> append "<M,OO>"
  inline const std::string CLEAR = "*C,0,";          // clear / off -> append "<M,OO>"
  inline const std::string PING = "*P,0,";           // state query -> append "<M,OO>"
  inline const std::string DIM_PREAMBLE = "*A,0,";   // dimmer data preamble -> append "<M,OO>"
  inline const std::string LEVEL = "*Z,0";           // dimmer level -> append "<HEX>"
}

// ---- address <-> MQTT object id -------------------------------------------
// "M,OO" -> "M_OO"  (a comma cannot appear in an MQTT topic)
inline std::string sanitize(const std::string &addr) {
  std::string s = addr;
  for (auto &c : s) if (c == ',') c = '_';
  return s;
}
// "M_OO" -> "M,OO"
inline std::string desanitize(const std::string &oid) {
  std::string s = oid;
  for (auto &c : s) if (c == '_') c = ',';
  return s;
}

// ---- dimmer level conversion ----------------------------------------------
// percentage -> two-digit upper-case hex byte, byte = ceil(255*pct/100)
inline std::string pct_to_hex(int pct) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  long b = (255L * pct + 99) / 100;
  char buf[5];
  snprintf(buf, sizeof(buf), "%02lX", b);
  return std::string(buf);
}
// hex byte (leading "0" harmless) -> percentage, pct = floor(100*byte/255)
inline int hex_to_pct(const std::string &hex) {
  long v = strtol(hex.c_str(), nullptr, 16);
  return (int) (100L * v / 255);
}

// ---- inbound frame parsing -------------------------------------------------
// A frame is "<op>" or "<op>,0,<M>,<OO>". Returns opcode and, when present,
// the logical address "M,OO" (the leading "0" filler is skipped).
struct Frame {
  std::string op;
  std::string addr;
  bool has_addr;
};
inline Frame parse_frame(const std::string &f) {
  Frame r;
  r.has_addr = false;
  size_t c1 = f.find(',');
  r.op = (c1 == std::string::npos) ? f : f.substr(0, c1);
  if (c1 == std::string::npos) return r;
  size_t c2 = f.find(',', c1 + 1);   // skip the "0" filler
  if (c2 == std::string::npos) return r;
  r.addr = f.substr(c2 + 1);
  r.has_addr = true;
  return r;
}

// ---- command topics --------------------------------------------------------
// "luxom/<oid>/set" or "luxom/<oid>/bri/set" -> "<oid>" ("" if malformed)
inline std::string topic_oid(const std::string &topic) {
  size_t a = topic.find('/');
  if (a == std::string::npos) return "";
  size_t b = topic.find('/', a + 1);
  if (b == std::string::npos) return "";
  return topic.substr(a + 1, b - a - 1);
}
// "luxom/cover/<cid>/set" -> "<cid>" ("" if malformed)
inline std::string cover_cid_from_topic(const std::string &topic) {
  size_t a = topic.find('/');
  if (a == std::string::npos) return "";
  size_t b = topic.find('/', a + 1);
  if (b == std::string::npos) return "";
  size_t c = topic.find('/', b + 1);
  if (c == std::string::npos) return "";
  return topic.substr(b + 1, c - b - 1);
}

// ---- cover id <-> up/down addresses ---------------------------------------
inline std::string cover_cid(const std::string &up, const std::string &down) {
  return sanitize(up) + "__" + sanitize(down);
}
struct UpDown {
  std::string up;
  std::string down;
  bool ok;
};
inline UpDown cover_parse_cid(const std::string &cid) {
  size_t sep = cid.find("__");
  if (sep == std::string::npos) return {"", "", false};
  return {desanitize(cid.substr(0, sep)), desanitize(cid.substr(sep + 2)), true};
}

// ---- shutters config: "Name|up|down ; Name|up|down" -----------------------
struct Cover {
  std::string name;
  std::string up;
  std::string down;
};
inline std::vector<Cover> parse_covers(const std::string &cfg) {
  std::vector<Cover> out;
  auto trim = [](std::string &v) {
    while (!v.empty() && v.front() == ' ') v.erase(v.begin());
    while (!v.empty() && v.back() == ' ') v.pop_back();
  };
  size_t start = 0;
  while (start <= cfg.size()) {
    size_t semi = cfg.find(';', start);
    std::string entry = (semi == std::string::npos) ? cfg.substr(start)
                                                    : cfg.substr(start, semi - start);
    trim(entry);
    if (!entry.empty()) {
      size_t p1 = entry.find('|');
      size_t p2 = (p1 == std::string::npos) ? std::string::npos : entry.find('|', p1 + 1);
      if (p1 != std::string::npos && p2 != std::string::npos) {
        Cover c;
        c.name = entry.substr(0, p1);
        c.up = entry.substr(p1 + 1, p2 - p1 - 1);
        c.down = entry.substr(p2 + 1);
        trim(c.name); trim(c.up); trim(c.down);
        if (!c.name.empty() && !c.up.empty() && !c.down.empty()) out.push_back(c);
      }
    }
    if (semi == std::string::npos) break;
    start = semi + 1;
  }
  return out;
}

// ---- Home Assistant MQTT discovery payloads -------------------------------
inline std::string device_json() {
  return "\"dev\":{\"ids\":[\"luxom_gw\"],\"name\":\"Luxom Gateway\","
         "\"mf\":\"Luxom\",\"mdl\":\"DS65L bridge\"}";
}
// switch ('s') or dimmer/light ('d') config payload for address "M,OO"
inline std::string entity_config(const std::string &addr, bool dimmer) {
  std::string oid = sanitize(addr);
  std::string base = "luxom/" + oid;
  std::string p = std::string("{\"name\":\"Luxom ") + addr +
    "\",\"uniq_id\":\"luxom_" + oid +
    "\",\"stat_t\":\"" + base + "/state\"" +
    ",\"cmd_t\":\"" + base + "/set\"" +
    ",\"pl_on\":\"ON\",\"pl_off\":\"OFF\"" +
    ",\"avty_t\":\"luxom/gateway/status\"";
  if (dimmer) {
    p += std::string(",\"bri_stat_t\":\"") + base + "/bri\"" +
         ",\"bri_cmd_t\":\"" + base + "/bri/set\"" +
         ",\"bri_scl\":100,\"on_cmd_type\":\"brightness\"";
  }
  p += "," + device_json() + "}";
  return p;
}
inline std::string entity_config_topic(const std::string &addr, bool dimmer) {
  return std::string("homeassistant/") + (dimmer ? "light" : "switch") +
         "/luxom_" + sanitize(addr) + "/config";
}
inline std::string cover_config(const std::string &name, const std::string &up,
                                const std::string &down) {
  std::string cid = cover_cid(up, down);
  std::string base = "luxom/cover/" + cid;
  return std::string("{\"name\":\"") + name + "\",\"uniq_id\":\"luxom_cover_" + cid + "\"" +
         ",\"cmd_t\":\"" + base + "/set\"" +
         ",\"pl_open\":\"OPEN\",\"pl_cls\":\"CLOSE\",\"pl_stop\":\"STOP\"" +
         ",\"optimistic\":true,\"avty_t\":\"luxom/gateway/status\"," + device_json() + "}";
}
inline std::string cover_config_topic(const std::string &up, const std::string &down) {
  return "homeassistant/cover/luxom_" + cover_cid(up, down) + "/config";
}

}  // namespace luxom
