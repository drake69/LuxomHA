# Developer Guide — Luxom → MQTT → Home Assistant gateway

## Overview

ESP32 firmware (ESPHome, Arduino framework) that bridges a Luxom bus to Home
Assistant. The ESP32 is a persistent **TCP client** to the Luxom **DS65L IP
interface** and a bidirectional **ASCII ↔ MQTT** bridge. Home Assistant entities
are created via **MQTT discovery** (no custom HA component, no native API).

```
Luxom L-bus ── DS65L (IP) ──TCP── ESP32 (ESPHome) ──MQTT── Broker ── Home Assistant
                          ASCII ';'-framed     retained discovery + state/command topics
```

> **TCP port.** The ELAN integration note documents port **2301** for the DS65L
> Ethernet interface; the openHAB binding / protocol KB use **2300**. They likely
> differ by firmware/setup, so the port is configurable in `secrets.yaml`
> (`luxom_port`). If a connection fails, try the other value.

### Technology stack

| Component | Choice | Why |
|---|---|---|
| MCU | ESP32 (`esp32dev`) | cheap, Wi-Fi/Ethernet |
| Build | ESPHome (`framework: arduino`) | the deliverable is a YAML; Arduino gives a trivial `WiFiClient` TCP socket |
| Transport to HA | **MQTT** | needed for *runtime* dynamic entity creation (the device list is unknown up front) |
| Bus link | raw TCP via `WiFiClient` in lambdas | the Luxom protocol is custom ASCII over TCP |

> Why MQTT and not the ESPHome native API? The native API only exposes entities
> declared at compile time. Here the Luxom device list is **not known in advance**
> and the bus has **no enumeration command**, so entities must be created at
> runtime — which means publishing MQTT discovery dynamically. See
> `03_design_decisions.md` for the full decision log.

## Files

| File | Role |
|---|---|
| `luxom_gateway.yaml` | Production gateway (PHASE 2) |
| `luxom_cover_discovery.yaml` | Shutter address sniffer (PHASE 1) |
| `secrets.yaml` | All site-specific values (not committed) |
| `02_luxom_protocol.md` | Protocol knowledge base (reference) |

## Discovery model

There is **no bus enumeration** on Luxom. The gateway discovers devices three ways:

1. **Passive sniffing (always on).** Any inbound `@1*…` frame from an unknown
   address auto-creates the entity: `@1*S`/`@1*C` → `switch`, `@1*A`+`@1*Z` →
   `light` (dimmer). So just using the wall switches populates Home Assistant.
2. **Active probing (`Luxom: scan bus` button).** Iterates `*P,0,<M,OO>` over the
   range in `secrets.yaml` (`luxom_probe_modules` × `luxom_probe_out_max`). `*P` is
   a **state query** — it does not actuate anything. Responders are classified and
   published.
3. **Shutters — static (PHASE 1 → PHASE 2).** Shutters are two relays with no
   dedicated opcode and cannot be auto-classified, so they are configured
   explicitly in `secrets.yaml` after discovering their addresses with
   `luxom_cover_discovery.yaml`.

Discovery configs are published **retained**, so Home Assistant keeps the entities
across gateway reboots (the broker retains them).

## MQTT topic map

| Topic | Direction | Payload |
|---|---|---|
| `homeassistant/{switch,light,cover}/luxom_<id>/config` | gw → broker | discovery JSON (retained) |
| `luxom/<id>/state` | gw → HA | `ON` / `OFF` (retained) |
| `luxom/<id>/bri` | gw → HA | `0..100` (retained, dimmer only) |
| `luxom/<id>/set` | HA → gw | `ON` / `OFF` |
| `luxom/<id>/bri/set` | HA → gw | `0..100` (dimmer only) |
| `luxom/cover/<up>__<down>/set` | HA → gw | `OPEN` / `CLOSE` / `STOP` |
| `luxom/gateway/status` | gw LWT | `online` / `offline` (availability) |

`<id>` is the Luxom address `M,OO` sanitized as `M_OO` (the comma cannot appear in
a topic). For covers the topic id encodes both relay addresses, so commands work
even after a reboot without any persisted state.

## Protocol mapping (Luxom ASCII ↔ gateway)

Derived from `02_luxom_protocol.md`. Outbound frames are `message + ";"`.

| Action | Frame(s) sent |
|---|---|
| Switch ON | `*S,0,<addr>` then `*P,0,<addr>` (confirm) |
| Switch OFF | `*C,0,<addr>` then `*P,0,<addr>` |
| Dimmer to N% | `*S,0,<addr>` (or `*C` if 0), `*A,0,<addr>`, `*Z,0<HEX>` |
| State query | `*P,0,<addr>` |
| Cover OPEN | `*C,0,<down>`, `*S,0,<up>` |
| Cover CLOSE | `*C,0,<up>`, `*S,0,<down>` |
| Cover STOP | `*C,0,<up>`, `*C,0,<down>` |

Inbound handling (`lux_on_frame`):
- `@1*PW-` → reply `*?` (password handshake).
- `*!…` → bus ready; re-publish known discovery.
- `@1*S`/`@1*C` → state ON/OFF (and auto-create switch if new).
- `@1*A` → remember address; the following `@1*Z` carries the dimmer byte.
- `@1*Z` → decode level (`pct = 100·byte/255`), publish brightness+state.
- `*U`, `@1*V` → heartbeat / ack (ignored).

Dimmer level encoding: `byte = ceil(255·pct/100)` (HEX, upper-case); decoding:
`pct = floor(100·byte/255)`. The round-trip is stable but not bijective (±1%).

## Code structure (single YAML, marstek-style)

- `globals` — socket, RX buffer, dimmer-pairing address, known-devices vector,
  loaded covers, scan cursor, connection flags, and the `!secret` parameters.
- `script` — `lux_send`, `lux_load_covers`, `lux_emit_config`, `lux_emit_cover`,
  `lux_republish`, `lux_discover_start`, `lux_on_frame`.
- `interval` — 1s connection management + handshake fallback; 100ms socket read
  pump (splits on `;`, handles concatenated frames); 250ms active-scan stepper
  (4 probes/tick to avoid flooding); 300s periodic `*P` refresh; 5s diagnostics.
- MQTT command subscriptions registered once in `on_boot` (ESPHome re-subscribes
  on reconnect); `mqtt.on_connect` re-publishes discovery.

## Limitations / trade-offs

- **No bus enumeration**: the probe range and the shutter list are the only
  non-automatable inputs.
- **Single TCP session**: the DS65L usually accepts one client. The gateway must
  be the exclusive consumer.
- **Dimmer conversion** is not bijective (±1% on round-trip).
- **Shutters are optimistic** (no position feedback): OPEN/CLOSE/STOP replicate the
  wall buttons; HA shows no live position. Time-based position could be added.
- **No persistence of discovered devices**: relies on broker-retained discovery.
  If the broker loses retained messages, re-run a scan / use the switches again.
- **Switch vs dimmer classification** assumes relays answer `@1*S/@1*C` and dimmers
  answer `@1*A/@1*Z` (per the binding). A dimmer that only ever replies `@1*C`
  while off could be misclassified until it reports a level.

## Notes

- The Arduino `WiFiClient` type is brought into scope via
  `esphome: includes: [luxom_net.h]` (a one-line header that `#include <WiFi.h>`).
  This is required on the Arduino framework — without it `main.cpp` fails to
  compile (`'WiFiClient' was not declared`). Don't remove that include.
- First flash must be over USB; subsequent flashes can be OTA (`flash_ota.sh`).
