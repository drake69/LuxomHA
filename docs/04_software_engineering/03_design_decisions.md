# Design decisions & rationale

Date: 2026-06-28 · Project type: ESP32 firmware (ESPHome) · Deliverable: a YAML

This document captures the reasoning that shaped the project, so a future reader
understands *why* it is built this way and not otherwise.

## 1. Starting point

Two input files (delivered zipped, now under `../quarantine/`):

- `luxom_gateway_requisiti.md` — a thorough Luxom protocol knowledge base (DS65L IP
  interface, TCP:2300, ASCII `;`-framing, opcodes, password handshake, dimmer
  encoding, MQTT + HA discovery mapping). Kept as `02_luxom_protocol.md`.
- `luxom_mqtt_gateway.ino` — a working standalone Arduino/ESP32 sketch doing
  Luxom↔MQTT bridging with a *static* device table. Used as functional reference.

Goal: turn this into an ESP32 + Home Assistant project whose final artifact is a
**YAML**, organized like the sibling project **NoPowerWasted2500** (an ESPHome
project: uv-managed ESPHome, `secrets.yaml`, OTA flashing, one monolithic device
YAML with lambdas/globals).

## 2. Decisions

### 2.1 Platform — ESPHome (confirmed by the sibling)

"ESP32 on Home Assistant, the artifact is a YAML" ⇒ ESPHome, where the firmware
*is* a YAML compiled by ESPHome. Mirrors NoPowerWasted2500's structure and style.

### 2.2 Transport to HA — MQTT (not the native API)

This was the pivotal decision. Two options:

- **Native API** — simplest and most idiomatic for ESPHome (no broker), but entities
  must be **declared at compile time**.
- **MQTT** — requires a broker, but allows **publishing discovery configs at
  runtime**, i.e. creating entities dynamically.

The user does **not know the device list** and Luxom has **no bus-enumeration
command**, so the entity set cannot be fixed in the YAML up front — it must be built
at runtime from what is found on the bus. That requires runtime dynamic discovery,
which the native API cannot do. Therefore: **MQTT**. (The user also already runs and
knows MQTT, which removes its main downside.)

Rejected alternative "native API + static device list": discarded because the list
is unknown and, even with active probing, the bus only yields *anonymous* addresses
(no names, no physical mapping) — so a static list would be guesswork.

### 2.3 Framework — Arduino (not esp-idf)

The sibling uses `esp-idf`. We chose `framework: arduino` because the Luxom link is
a raw TCP client socket, trivially done with Arduino's `WiFiClient` (porting the
reference `.ino` almost 1:1). The framework is invisible to the user; ESPHome's
`mqtt:` works the same either way.

### 2.4 Device discovery — passive sniffing first

There is no enumeration on Luxom. Discovery is achieved by:

1. **Passive sniffing (always on)** — using the wall switches makes entities appear.
   This is the primary, most reliable path.
2. **Active `*P` probing (on-demand button)** — bulk sweep over a configurable
   address range. `*P` is a non-actuating state query, so probing is safe.

### 2.5 Shutters (tapparelle) — two-phase, static

Verified against the official openHAB binding: it supports only `switch` and
`dimmer`, **no rollershutter/cover**. Luxom shutters are two relays (up/down) with
no dedicated opcode, so they cannot be auto-classified by discovery (they look like
two switches).

A runtime "learn mode" (press UP then DOWN to pair the two relays) was prototyped,
then **replaced** — at the user's suggestion — by a cleaner two-phase split:

- **PHASE 1 — `luxom_cover_discovery.yaml`**: a diagnostic sniffer. Press UP/DOWN on
  each shutter, read the two addresses from the logs / a HA text entity.
- **PHASE 2 — `luxom_gateway.yaml`**: shutters declared statically in `secrets.yaml`
  (`luxom_covers: "Name|up|down ; ..."`), published as real HA `cover` entities.

This keeps the production firmware deterministic (no fragile runtime learning UI),
mirrors the sibling's `ble_discovery.yaml`-then-bake pattern, and the addresses
belonging to a cover are excluded from switch auto-discovery (no duplicates).

Shutter command model: OPEN/CLOSE/STOP replicate the wall buttons (`*S` on the
relevant relay, `*C` to stop), optimistic (no position feedback).

### 2.6 Configuration surface — `secrets.yaml` only

All site-specific values live in `secrets.yaml`: Wi-Fi, MQTT broker, DS65L host/port,
probe range, shutter list, OTA password. Values consumed inside C++ lambdas are
injected via `globals: initial_value: !secret …` (string secrets need inner quotes,
the proven NoPowerWasted2500 pattern).

### 2.7 Language

All official material is in **English** — code comments, log strings, entity names,
documentation and git commit messages — to match the ecosystem (ESPHome/openHAB).
The protocol knowledge base was originally delivered in Italian and has since been
translated to English for consistency. Conversation with the user stays in Italian.

### 2.8 Bus link implementation — lwip BSD sockets (not Arduino `WiFiClient`)

The first implementation used Arduino's `WiFiClient` for the TCP socket. It **failed
to compile**: ESPHome on the arduino-esp32 3.x core does **not** ship the Arduino
`WiFi` library — it drives Wi-Fi through ESP-IDF's `esp_wifi` directly — so
`WiFiClient` / `<WiFi.h>` are not in scope. (Caught by the automated compile test,
see §5.)

The socket layer was rewritten on the **lwip POSIX socket API** (always available on
ESP-IDF): a non-blocking `int` file descriptor in a global, `connect()` checked via
`select()`, `recv()` in the read pump, `send()` for outbound frames. The headers are
pulled in by `luxom_net.h` (`esphome: includes:`). The DS65L host must be an **IP
address** (no DNS lookup). This is framework-agnostic and drops the Arduino
dependency entirely.

### 2.9 Numeric secrets are quoted strings

`globals: initial_value: !secret …` requires a **string** value, so numeric secrets
(`luxom_port`, `luxom_probe_out_max`) must be quoted in `secrets.yaml`
(`luxom_port: "2301"`). A bare number fails validation (`Must be string, got EInt`).
Caught by the automated config test (§5).

### 2.10 Persistence — MQTT broker, not on-device NVS

Discovered lights/switches/dimmers are persisted by the **MQTT broker** (retained
discovery + retained state), not on the ESP32. Shutters persist because they are
compiled into the firmware from `secrets.yaml`. The ESP32's NVS flash is therefore
**not** used to store the discovered-device list.

Rationale: the broker is already the source of truth and survives ESP32 reboots, so
on-device persistence would be redundant — a reboot does **not** require re-running
discovery. The only failure mode is losing the broker's retained messages *and*
rebooting the ESP; then lights reappear when next used / scanned (shutters are
unaffected, being in firmware). On-device NVS persistence (serializing `lux_known`
into a restored `std::string`) was considered and **deliberately not added**.
Decision (user, 2026-06-28): keep the broker as the single source of truth; revisit
NVS only if the broker is reset frequently.

## 3. Resulting architecture

```
Luxom L-bus ── DS65L (IP) ──TCP:2300── ESP32 (ESPHome, Arduino)
                                            │  lambdas: framing, handshake,
                                            │  dimmer pairing, discovery
                                            ▼
                                     MQTT (broker)
                                            │  retained discovery + state/cmd
                                            ▼
                                     Home Assistant
```

- `luxom_gateway.yaml` — the production artifact.
- `luxom_cover_discovery.yaml` — PHASE 1 tool.
- Topic map, protocol mapping and limitations: see `01_architecture.md`.

## 3a. Corroboration from the ELAN integration note

Late in the session an ELAN g! integration note for Luxom surfaced
(`references/ELAN_Luxom_integration_note_6.7.12.pdf`). It independently confirms
the key design assumptions and refines two facts:

- **No auto-discovery** — devices must be added manually via PlusConfig. Validates
  the sniffing/probing approach and the manual shutter setup.
- **Shutter device types exist**: 2-Button Shutter = Open address + Close address
  (our up/down cover), 1-Button Shutter = single address (our fallback),
  Curtain/Blind = dedicated type. Validates the cover model.
- **DS65L Ethernet port = 2301** (vs 2300 in the openHAB KB) → kept configurable.

See `references/00_references.md` for PlusConfig and the ASCII-doc sources.

## 4. Open points / future work

- Optional time-based shutter position (percentage) and live cover state.
- On-device NVS persistence of discovered devices — **decided against** (see §2.10);
  revisit only if the broker is reset frequently.
- Confirm whether the specific DS65L unit enforces single-client TCP and whether a
  password code is present in the `@1*PW-` handshake.
- Optional wired (Ethernet) uplink variant (swap `wifi` for `ethernet`).

## 5. Testing & CI

The configs are verified automatically, with no hardware:

- **`esphome config`** — schema / secrets / substitution validation (fast). Caught
  the numeric-secret quoting issue (§2.9).
- **`esphome compile`** — full C++ build of the lambdas. Caught the `WiFiClient`
  incompatibility (§2.8) and the ambiguous `socket()` call. This is the decisive
  test.
- **`test.sh`** — runs both (`./test.sh` validate, `./test.sh --compile` build). It
  uses `tests/ci_secrets.yaml` when no real `secrets.yaml` is present, so it never
  touches real credentials.
- **`.github/workflows/ci.yml`** — GitHub Actions: a `validate` job and a `compile`
  job (with PlatformIO build cache).

Strategic point: for an ESPHome project the YAML *is* the firmware, so the real test
is **compilation** — config validation alone misses C++ lambda errors. Both are kept,
and the compile gate is what makes lambda-heavy ESPHome safe to change.
