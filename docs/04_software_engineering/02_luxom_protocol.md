# Luxom bus → MQTT → Home Assistant: technical requirements and knowledge base

Technical reference for building an ESP32 gateway that translates the Luxom
protocol (DS65L IP interface) to MQTT with Home Assistant (HA) auto-discovery.
The protocol knowledge was extracted from the openHAB binding
`org.openhab.binding.luxom` (EPL-2.0), the official Luxom ASCII documentation, and
the ELAN g! integration note. The firmware implementation is original: opcodes,
framing and arithmetic are interface facts, not protected expression of the binding.

---

## 1. Architecture

```
Luxom L-bus ── DS65L (IP interface) ──TCP:2300/2301── ESP32 gateway ──MQTT── Broker ── Home Assistant
                                     ASCII ';'-framed         JSON discovery + state/command topics
```

- The physical Luxom bus is not reached directly: the IP interface exposes a
  transparent TCP socket that encapsulates the Luxom ASCII protocol.
- The ESP32 acts as a persistent **TCP client** to `DS65L:2300` (or `2301`, see §2)
  and as a bidirectional ASCII ↔ MQTT bridge.
- HA populates entities via MQTT discovery; no Python component on the HA side.

---

## 2. Luxom bus — model and transport

| Aspect | Value |
|---|---|
| Tested interface | DS65L IP interface |
| Transport | TCP, port **2300** (openHAB/KB) or **2301** (ELAN note for DS65L) |
| Encoding | ASCII |
| Frame terminator | `;` **only** (no CR/LF) |
| Response prefix | `@1` (inbound frames from the interface) |
| Keep-alive | socket `SO_KEEPALIVE`; application heartbeat `*U` |

### 2.1 Framing

- **Outbound**, the binding sends `message + ";"`, with no CR/LF.
- **Inbound**, characters are accumulated until `;`; the frame is the content
  without the terminator.
- Frames may arrive **concatenated** within the same TCP segment
  (e.g. `@1*A,0,2,02;@1*Z,057;`). Parsing must split strictly on `;`, not assume
  one frame per `read()`.

### 2.2 Addressing model

- Logical device address: `M,OO` (module, output). Examples: `1,01`, `2,02`, `A,02`.
- A constant filler field follows the opcode on the wire:
  - outbound: `*<op>,0,<M>,<OO>`
  - inbound: `@1*<op>,0,<M>,<OO>`
- The `0` field is hardcoded on transmit and **ignored** on receive
  (`address = tokens[2..]`).
- **There is no bus enumeration command.** The `(M,OO) → physical load` map lives
  in the Luxom PlusConfig project. So "scanning" means: (a) authoritative export
  from PlusConfig, (b) passive sniffing of `@1*…` frames while operating the wall
  buttons, or (c) active probing with `*P` over the known address space.

---

## 3. Opcode set

Derived from the `LuxomAction` enum (fields: command, `hasAddress`, `needsData`).

### 3.1 Outbound (commands from the gateway)

| Opcode | Meaning | Address | Notes |
|---|---|---|---|
| `*S` | Set / on | yes | `*S,0,<addr>` |
| `*C` | Clear / off | yes | `*C,0,<addr>` |
| `*P` | Ping / state query | yes | reply `@1*S`/`@1*C`/`@1*A` |
| `*T` | Toggle | yes | not used by the gateway |
| `*A` | Data (dimmer level preamble) | yes | followed by `*Z` |
| `*Z` | Data byte (level value) | no | `*Z,0<HEX>` |
| `*?` | Request for information | no | reply to the password handshake |
| `*!` | Module information (query) | no | |

### 3.2 Inbound (replies from the interface, `@1` prefix)

| Opcode | Meaning | Address |
|---|---|---|
| `@1*S` | State = ON | yes |
| `@1*C` | State = OFF | yes |
| `@1*A` | Dimmer level (preamble) | yes |
| `@1*Z` | Dimmer level (byte) | no |
| `@1*V` | Acknowledge | no |
| `@1*PW-` | Password request | no |
| `*!…` | Module information (data) | no |
| `*U` | Heartbeat | no |

---

## 4. Connection handshake

1. On connect the interface emits `@1*PW-` (password request).
2. The client replies `*?` (request for information).
3. The interface sends `*!<data>` (module information) → interpret as
   "online, start processing".
4. `*U` (heartbeat) and `@1*V` (acknowledge) are watchdog / no-op.

> If the installation uses an actual password, the frame may be `@1*PW-<code>`:
> in that case the handshake branch must be extended accordingly.

---

## 5. State semantics and queries

- A single device's state is obtained with `*P,0,<addr>`; the device replies
  `@1*S` / `@1*C` (relay) or `@1*A` + `@1*Z` (dimmer).
- With no enumeration, a state refresh means iterating `*P` over the known address
  list. This is also the mechanism used at boot and after reconnect (`*!`).
- Some devices (typically dimmers flagged `doesNotReply`) **do not** confirm state
  on the bus: they are handled optimistically (state published on command, never
  confirmed).

---

## 6. Dimmer encoding

- Level = hexadecimal byte; conversions:
  - encode: `byte = ceil(255 · pct / 100)`
  - decode: `pct = floor(100 · byte / 255)`
- The round-trip is stable but **not bijective**: a level set from HA may read back
  ±1% after the device echo.
- **Outbound** sequence (burst of 3 frames):
  ```
  *S,0,<addr>;        (or *C,0,<addr>; if pct == 0)
  *A,0,<addr>;
  *Z,0<HEX>;
  ```
- **Inbound** sequence: a `@1*A,0,<addr>;` pair followed by `@1*Z,0<HEX>;`. The
  `@1*A` address must be held while awaiting the following `@1*Z`, then paired (the
  binding uses a `previousCommand` with a `needsData` flag).
- The leading zero of the byte (`0<HEX>`) is harmless: `strtol(.,16)` ignores it.

---

## 7. openHAB implementation (structure and key behaviors)

Bundle `org.openhab.binding.luxom`, author K. Jespers, EPL-2.0.

### 7.1 Source map

| Class | Role |
|---|---|
| `protocol/LuxomAction` | opcode enum (command, `hasAddress`, `needsData`) |
| `protocol/LuxomCommand` | parse a frame into action + address/data |
| `protocol/LuxomCommunication` | TCP socket, char listener, `;` framing |
| `protocol/LuxomSystemInfo` | parse module information |
| `handler/LuxomBridgeHandler` | dispatch inbound frames, TCP flow control |
| `handler/LuxomThingHandler` | base: builds `*P` / `*S` / `*C` from address |
| `handler/LuxomSwitchHandler` | relay: ON/OFF + confirm ping |
| `handler/LuxomDimmerHandler` | dimmer: `*S/*A/*Z` burst, `onLevel`, `onToLast`, `stepPercentage` |
| `handler/util/PercentageConverter` | hex ↔ percentage conversion |

### 7.2 Relevant behaviors

- **Char-by-char listener**: `LuxomCommunication.runLuxomEvents()` reads one char at
  a time and closes the frame on `;`. Handles `stream end` (EOF) with optional
  *fast reconnect* (`useFastReconnect`) and, on failure, full reconnect with
  `forceRefreshThings()`.
- **Dispatch** (`handleIncomingLuxomMessage`):
  - `@1*PW-` → send `*?` (bypasses the queue / flow control).
  - `*!…` → `cmdSystemInfo()` and start processing (enables TCP flow control).
  - `@1*V` → log acknowledge.
  - `*A` / `@1*A` (needsData) → stored as `previousCommand`.
  - `*Z` / `@1*Z` → if the previous command needs data, the byte is attached to it.
  - otherwise → `findThingHandler(address)` and forward to the thing handler.
- **Switch**: after `set()` / `clear()` it always does a `ping()` to realign the
  real state.
- **Dimmer**: `doesNotReply=true` → thing kept ONLINE, no ping; `onToLast` → turns
  on to the last level; `onLevel` → default level; `IncreaseDecreaseType` → rounded
  to multiples of `stepPercentage`.
- **PercentageConverter**: `getPercentage` = `floor(100·dec/255)`;
  `getHexRepresentation` = `toHexString(ceil(255·pct/100))` in upper case.

### 7.3 Binding configuration (reference)

```
Bridge luxom:bridge:myhouse [ ipAddress="192.168.0.50", port="2300" ] {
    Thing switch switchBedroom1 "Switch 1" @ "Bedroom" [ address="1,01" ]
    Thing dimmer dimmerBedroom1 "Dimmer 1" @ "Bedroom" [ address="A,02" ]
    Thing dimmer dimmerKitchen1 "Dimmer 1" @ "Kitchen" [ address="A,04", doesNotReply=true ]
}
```

---

## 8. ESP32 gateway technical requirements

### 8.1 Hardware

- ESP32 (Wi-Fi). For a wired uplink: an ESP32 with RMII PHY (LAN8720) or an
  integrated board (WT32-ETH01, ESP32-POE); at the socket level it is identical,
  swapping `WiFiClient`/`WiFi` for `ETH`.

### 8.2 Software / libraries

- `PubSubClient` for MQTT. `mqtt.setBufferSize(512)` is **mandatory**: the default
  256 bytes silently drops the discovery payloads. (In ESPHome this is handled by
  the `mqtt:` component.)
- For high load or non-blocking I/O: `AsyncMqttClient` + `AsyncTCP`.

### 8.3 Implementation constraints

- **Framing**: `;` terminator without CR/LF; strict split on `;` handling
  concatenated frames.
- **Single session**: many IP interfaces accept **only one** TCP client at a time.
  While the gateway holds the socket, do not run a second client (openHAB/Homey) on
  the same DS65L. To be verified on the unit.
- **Password handshake**: reply to `@1*PW-` with `*?`; extend if a code is present.
- **Reconnect**: backoff on socket loss; on restore, re-publish discovery and re-run
  the `*P` refresh.
- **Dimmer pairing**: hold the `@1*A` address until the following `@1*Z`.
- **doesNotReply**: optimistic state, no ping.

---

## 9. MQTT + Home Assistant discovery mapping

- Static device table in the firmware: `{addr, kind, name, doesNotReply}`, populated
  from the scan / PlusConfig export. (In this gateway switches/dimmers are instead
  discovered at runtime; shutters are static — see the architecture doc.)
- Sanitization: `addr` (`M,OO`) → `M_OO` for `unique_id`/`object_id` and topics.
- Topics:
  - state: `luxom/<M_OO>/state` (`ON`/`OFF`, retained)
  - brightness: `luxom/<M_OO>/bri` (0–100, retained)
  - command: `luxom/<M_OO>/set`
  - brightness command: `luxom/<M_OO>/bri/set`
- Discovery (retained): `homeassistant/{light|switch}/luxom_<M_OO>/config`.
  - `switch` for relays, `light` with `bri_scl=100` and `on_cmd_type=brightness`
    for dimmers.

---

## 10. Limitations and trade-offs

- **No enumeration**: the address list is the only non-automatable input.
- **Non-bijective dimmer conversion**: ±1% tolerance on the round-trip.
- **Optimistic state** for `doesNotReply`: possible divergence if physical actuation
  happens outside the gateway.
- **Single TCP session**: the gateway must be the exclusive consumer.
- **Possible authentication**: handle `@1*PW-<code>` if present.

---

## 11. References

- Luxom ASCII protocol: `LUXOM_ASCII.pdf`, `LUXOM_ASCII_extended.pdf`
  (host `old.luxom.io`; automated access blocked). See `references/00_references.md`.
- openHAB binding: `openhab/openhab-addons`, bundle `org.openhab.binding.luxom`
  (EPL-2.0). Docs: `https://www.openhab.org/addons/bindings/luxom/`.
- ELAN g! integration note: `references/ELAN_Luxom_integration_note_6.7.12.pdf`.

> License note: the binding is EPL-2.0 (file-level copyleft). A literal translation
> of the Java source would produce a derivative work bound by EPL-2.0.
> Re-implementing the protocol from these interface facts avoids that constraint.

---

## 12. Addendum — ELAN integration note + PlusConfig (2026-06-28)

Additions from the ELAN g! integration note (`references/`) and web sources. See
`references/00_references.md` for URLs.

- **DS65L TCP port = 2301.** The ELAN note explicitly documents port **2301** for
  the DS65L Ethernet interface; the KB/openHAB use **2300**. Likely a
  firmware/setup difference → `luxom_port` stays configurable in `secrets.yaml`; if
  the connection fails, try the other value.
- **Device types (ELAN driver), including shutters** — confirm the gateway model:
  - `Luxom On/Off Load`, `Luxom Dimmer Load` → relay / dimmer (already handled).
  - `Luxom 2-Button Shutter` → **two addresses** (`Open Gr/Ad` + `Close Gr/Ad`):
    the up/down model of our `cover` entities.
  - `Luxom 1-Button Shutter` → **single address** (pulse): our fallback (up == down).
  - `Luxom Curtain/Blind` → dedicated type with feedback; our covers are optimistic
    (no live position).
  - `Luxom Action` → scenes/actions (not handled by the gateway).
- **No bus auto-discovery** (reaffirmed by the ELAN note): devices must be added
  manually; the `(group/module, address/output) → load` map lives in **PlusConfig**
  (installer software). Hence the gateway discovers via sniffing/probing and
  shutters are configured by hand (PHASE 1).
- **Addressing**: `Group` (module) + `Address` (output) = our `M,OO` pair.
  Example command: `*S,0,2,01;`.
