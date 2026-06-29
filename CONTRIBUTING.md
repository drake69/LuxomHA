# Contributing & design notes

This document orients developers: the **important design choices** (the *why*) and
how to work on the code. Full rationale lives in
`docs/04_software_engineering/03_design_decisions.md`; the architecture and
protocol mapping are in `docs/04_software_engineering/01_architecture.md` and
`docs/04_software_engineering/02_luxom_protocol.md`.

> The C++ / ESPHome code was written by Claude (Anthropic's AI). **C++ / ESPHome
> experts are very welcome** to review and improve it — open an issue or a PR. The
> goal is a correct, maintainable gateway; better implementations are appreciated.
> **Adding not-yet-integrated Luxom device types is especially welcome** (see
> "Adding a new device type" below).

## Project layout

| File | Role |
|---|---|
| `luxom_gateway.yaml` | Production gateway (ESPHome). The shipped artifact. |
| `luxom_cover_discovery.yaml` | One-off diagnostic to find shutter addresses. |
| `luxom_proto.h` | **Pure logic** (no ESPHome/Arduino): parsing, conversions, topic/JSON builders. Unit-tested. |
| `luxom_net.h` | lwip socket headers for the TCP link. |
| `secrets.yaml` | All site-specific values (git-ignored). |
| `tests/` | Host unit tests + CI fake secrets. |
| `test.sh`, `.github/workflows/ci.yml` | Local + CI checks. |

## Important design choices

1. **Transport to HA = MQTT, not the ESPHome native API.** The Luxom device list is
   unknown up front and the bus has *no enumeration command*, so entities must be
   created **at runtime** — only MQTT discovery allows that. The native API needs
   compile-time entities.

2. **Framework = Arduino core, but the socket uses lwip, not `WiFiClient`.** ESPHome
   on the arduino-esp32 core does **not** ship the Arduino WiFi library (it uses
   ESP-IDF `esp_wifi`), so `WiFiClient`/`<WiFi.h>` won't compile. The DS65L link is a
   non-blocking lwip BSD socket (`int` fd, `connect()`+`select()`, `recv`/`send`).
   The host must be an **IP** (no DNS). Global C calls are qualified (`::socket`) to
   avoid clashing with the `esphome::socket` namespace.

3. **Discovery.** Lights/outlets/dimmers are found by **passive sniffing** (always
   on) plus an on-demand **active `*P` scan** (`*P` is a non-actuating query).
   **Shutters** can't be auto-classified (two relays), so they are **two-phase**:
   `luxom_cover_discovery.yaml` reveals the up/down addresses, then they are declared
   statically in `secrets.yaml` (`luxom_covers`) and excluded from switch discovery.

4. **Pure logic lives in `luxom_proto.h`; the YAML lambdas are thin wrappers.** This
   is what makes the logic **unit-testable on the host** — the lambdas call the same
   functions the tests exercise, so tests cover production code, not a copy. (You
   cannot host-test a lambda embedded in a YAML string; that's why splitting the YAML
   into packages does *not* give test coverage — the logic must be real C++.)

5. **Persistence = the MQTT broker (retained), not ESP32 NVS.** Retained discovery +
   state survive reboots, and shutters are baked into the firmware from secrets, so a
   reboot needs no re-discovery. On-device NVS was deliberately not added.

6. **Single edit surface = `secrets.yaml`.** Everything site-specific is a `!secret`.
   Note the quoting: text secrets that feed a C++ `global` need inner quotes
   (`'"192.168.0.50"'`), and **numeric** secrets must be **quoted strings**
   (`luxom_port: "2301"`) because `initial_value` requires a string.

7. **English everywhere** — code comments, log strings, entity names, docs, commit
   messages — to match the ESPHome/openHAB ecosystem.

## Where to put new code

- **Reusable/parsing/formatting logic** → `luxom_proto.h`, and **add a test** in
  `tests/test_luxom_proto.cpp`. Keep it free of ESPHome/Arduino includes.
- **Socket/OS headers** → `luxom_net.h`.
- **ESPHome wiring** (entities, intervals, MQTT, scripts) → the YAML, calling
  `luxom::` helpers rather than re-implementing logic inline.

## Adding a new device type

The HA entity type and behaviour are defined entirely by the MQTT **discovery
payload** the gateway publishes (`homeassistant/<component>/luxom_<id>/config`). To
support a not-yet-integrated Luxom device type:

1. **Learn its frames** — what it sends/expects on the bus (see `docs/04_software_engineering/02_luxom_protocol.md`;
   capture real frames with `luxom_cover_discovery.yaml`).
2. **Add a discovery builder** in `luxom_proto.h` (like `entity_config` /
   `cover_config`): choose the HA `component` and build the JSON payload — and add
   unit tests for it.
3. **Wire it in the gateway YAML**: handle its inbound frames in `lux_on_frame`
   (state → HA) and add the MQTT command subscription(s) (HA → bus), calling the new
   helper.
4. Run `./test.sh --compile` and open a PR.

## Build & test

```bash
./test.sh             # host unit tests + esphome config validation (fast)
./test.sh --compile   # also full firmware compile (the decisive check)
./test.sh --coverage  # also coverage of the pure logic (luxom_proto.h)
```

Coverage is measured only for `luxom_proto.h` (the pure logic the lambdas delegate
to); the firmware/lambda glue and on-device behaviour are not measurable. CI builds
it with gcc + gcov and uploads to **Codecov** (needs a `CODECOV_TOKEN` repo secret).

What each layer covers:
- **host unit tests** (`tests/test_luxom_proto.cpp`) — behaviour of the pure logic.
- **`esphome config`** — YAML schema, secrets, substitutions.
- **`esphome compile`** — every lambda compiles (caught the `WiFiClient` and
  ambiguous-`socket()` bugs). For an ESPHome project the YAML *is* the firmware, so
  compilation is the real gate; config validation alone misses C++ errors.

CI (`.github/workflows/ci.yml`) runs all three on every push/PR: `unit` → `validate`
→ `compile` (the last with a PlatformIO cache). It runs on GitHub once the repo is
pushed to a GitHub remote.

## Branching & PRs

- **No direct commits to `main`.** Every change goes on a **feature branch** and is
  merged via a **pull request** (`gh pr create` → review → merge).
- Branch names: `feat/…`, `fix/…`, `docs/…`, `ci/…`, `refactor/…`.
- CI (`unit` → `validate` → `compile`) must be **green** before merge.

## Conventions for PRs

- Add/extend host unit tests for any change to `luxom_proto.h`.
- Keep the `secrets.yaml`-only edit surface; never commit a real `secrets.yaml`.
- English commit messages; CI must be green.
- No implicit AI co-authorship in commits unless explicitly agreed.
