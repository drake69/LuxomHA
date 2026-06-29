# Project memory — LuxomHA

Living project memory: context, history, and future. Human-facing.

## Context

**Goal.** An ESP32 gateway that bridges a **Luxom** home-automation bus (via the
**DS65L IP interface**) to **Home Assistant** over **MQTT**, with automatic entity
discovery. The deliverable is an **ESPHome YAML** (the firmware *is* the YAML).

**Project type.** PACCHETTO software (ESP32 firmware), built with ESPHome. Structure
and conventions follow the sibling project **NoPowerWasted2500**; documentation
follows `PROJECT_BLUEPRINT` / `PRODUCT_BLUEPRINT`.

**Owner profile.** Sysadmin: comfortable running MQTT and ESPHome as a user, not a
YAML/firmware author. Therefore the design exposes a single edit surface
(`secrets.yaml`) and everything else is automatic.

## Capabilities (what the project does)

- TCP client to the DS65L; ASCII framing (`;`), password handshake, reconnect.
- **Lights / outlets / dimmers**: discovered at runtime (passive sniffing always
  on; optional active `*P` scan button) and exposed as HA `switch` / `light` via
  MQTT discovery.
- **Shutters**: a two-phase flow — a discovery tool finds the up/down addresses,
  then they are declared in `secrets.yaml` and exposed as HA `cover` entities.
- **Automated tests**: `esphome config` + `esphome compile` via `test.sh` and CI.

## History — decisions and turning points

- **DA-1 → TP-1: Transport = MQTT (not ESPHome native API).** The device list is
  unknown and Luxom has no bus enumeration, so entities must be created at runtime;
  only MQTT allows dynamic discovery. The owner also already runs MQTT.
- **DA-2 → TP-2: Framework = Arduino (not esp-idf).** A raw `WiFiClient` TCP socket
  is trivial on Arduino and lets the reference `.ino` logic port almost 1:1.
- **DA-3 → TP-3: Discovery = passive sniffing first.** Using the wall switches makes
  entities appear; an on-demand `*P` scan covers the rest. No blind auto-probing at
  boot.
- **DA-4 → TP-4: Shutters = two-phase, static.** A runtime "learn mode" was
  prototyped, then replaced by a cleaner discovery-tool → static-config split
  (deterministic production firmware).
- **TP-5: ELAN integration note found.** Independently confirms no auto-discovery,
  the shutter device types (2-Button = up/down), and DS65L port **2301** (vs 2300).
- **DA-5 → TP-6: bus link = lwip sockets, not Arduino `WiFiClient`.** The automated
  compile test revealed ESPHome's arduino-esp32 core doesn't ship the Arduino WiFi
  library; the TCP layer was rewritten on lwip POSIX sockets (non-blocking `int`
  fd). The host must be an IP.
- **TP-7: automated tests added.** `esphome config` + `esphome compile`, wrapped in
  `test.sh` and a GitHub Actions workflow. The compile gate caught real C++ bugs.
- **DA-6: persistence = MQTT broker, not ESP32 NVS** (decided 2026-06-28). The
  broker's retained discovery/state is the single source of truth and survives
  reboots, so on-device NVS persistence was deliberately not added. Revisit only if
  the broker is reset frequently. See design decisions §2.10.

## Future / open points

- Confirm the actual DS65L port on the unit (2300 vs 2301) and whether the
  `@1*PW-` handshake carries a password code.
- Optional: time-based shutter position (percentage) and live cover state.
- Optional: persistence of discovered devices (today it relies on broker-retained
  discovery).
- Optional: wired Ethernet uplink variant.
- Obtain the official `LUXOM_ASCII.pdf` / `_extended.pdf` (or a PlusConfig export)
  to widen/verify the opcode set and the address map.
