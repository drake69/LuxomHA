# Luxom → MQTT → Home Assistant gateway (ESP32 / ESPHome)

[![ESPHome CI](https://github.com/drake69/LuxomHA/actions/workflows/ci.yml/badge.svg)](https://github.com/drake69/LuxomHA/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/drake69/LuxomHA/branch/main/graph/badge.svg)](https://codecov.io/gh/drake69/LuxomHA)

<!-- Private repo: append ?token=<BADGE_TOKEN> to the codecov badge URL above
     (Codecov → repo → Settings → Badge) for it to render. -->

> ⚠️ **Proof of concept / early draft — not yet tested on real hardware.** The
> firmware compiles and passes the automated checks (host unit tests, ESPHome
> `config` + `compile`), but it has **not yet been run against an actual Luxom DS65L
> and Home Assistant installation**. Field testing is pending. **Volunteers with a
> Luxom setup are very welcome** to try it and report results — open an issue or a PR.

An ESP32 firmware (built with **ESPHome**) that bridges a **Luxom** home-automation
bus to **Home Assistant** over **MQTT**, with automatic entity discovery.

The ESP32 acts as a TCP client to the Luxom **DS65L IP interface** (ASCII protocol,
`;`-terminated frames) and translates the bus traffic into MQTT, publishing Home
Assistant MQTT-discovery configs so entities appear in HA on their own.

```
Luxom L-bus ── DS65L (IP) ──TCP:2300── ESP32 (ESPHome) ──MQTT── Broker ── Home Assistant
```

## What you get in Home Assistant

| Luxom device | HA entity | How it is discovered |
|---|---|---|
| Relay / outlet | `switch` | automatic (passive sniffing or *scan bus*) |
| Dimmer | `light` (brightness 0–100) | automatic |
| Shutter | `cover` (open/close/stop) | static, from `secrets.yaml` (see PHASE 1) |

You only ever edit **`secrets.yaml`**. Everything else is handled by the firmware.

## Requirements

- An ESP32 board (e.g. `esp32dev`). For a wired uplink, an Ethernet ESP32 works too.
- A reachable Luxom **DS65L** IP interface (TCP — port **2300** or **2301**, see
  `secrets.yaml`; the ELAN note documents 2301 for the DS65L, openHAB uses 2300).
- An **MQTT broker** that Home Assistant uses (MQTT integration enabled, discovery on).
- [`uv`](https://docs.astral.sh/uv/) to run ESPHome.

> The DS65L typically accepts **a single TCP client**. While this gateway is
> connected, do not run another client (openHAB/Homey) against the same interface.

## Quick start

```bash
cp secrets.yaml.example secrets.yaml      # then fill it in (see comments inside)

# First flash over USB:
./flash_ota.sh gateway /dev/ttyUSB0
# Later, over the air:
./flash_ota.sh gateway 192.168.0.60
```

Then in Home Assistant: turn your Luxom lights/outlets on/off from the wall — they
appear automatically. Or press the **"Luxom: scan bus"** button (exposed by the
gateway) to actively probe the address range from `secrets.yaml`.

## Shutters (two phases)

Luxom has no bus enumeration and shutters have no dedicated opcode (they are two
relays, up/down). So shutters are set up in two phases:

1. **Discovery** — flash `luxom_cover_discovery.yaml`, press **UP** then **DOWN**
   on each shutter, read the two addresses from the logs (or the *recent addresses*
   entity in HA).
2. **Production** — put them in `secrets.yaml` (`luxom_covers`) and flash
   `luxom_gateway.yaml`. They show up as real HA `cover` entities.

```yaml
# secrets.yaml
luxom_covers: '"Kitchen|2,03|2,04 ; Bedroom|2,05|2,06"'
```

See **`docs/04_software_engineering/01_architecture.md`** for architecture,
the protocol mapping, and limitations.

## Files

| File | Purpose |
|---|---|
| `luxom_gateway.yaml` | **The artifact** — production ESPHome gateway |
| `luxom_cover_discovery.yaml` | PHASE 1 diagnostic tool to find shutter addresses |
| `luxom_proto.h` | Pure protocol/helper logic (host-unit-tested) |
| `luxom_net.h` | lwip socket headers for the TCP link |
| `secrets.yaml.example` | Template for your local `secrets.yaml` |
| `flash_ota.sh` | Build/flash helper (USB or OTA) |
| `test.sh` / `tests/` | Automated checks (unit tests + config/compile) |
| `CONTRIBUTING.md` | Design choices & how to develop |
| `docs/06_knowledge_base/01_user_guide.md` | **User guide** (Home Assistant, no dev skills) |
| `docs/04_software_engineering/01_architecture.md` | Architecture, protocol mapping, limitations |
| `docs/04_software_engineering/02_luxom_protocol.md` | Luxom protocol knowledge base |
| `docs/04_software_engineering/03_design_decisions.md` | Design decisions & rationale |
| `docs/04_software_engineering/04_credits.md` | Attributions |
| `docs/04_software_engineering/references/` | Reference docs (ELAN note, ASCII-doc URLs, PlusConfig) |

## Authorship & disclaimer

The **C++ / ESPHome implementation** in this repository was written **entirely by
Claude** (Anthropic's AI assistant).

The **software-engineering requirements** — together with the **functional**,
**operational** and **design** requirements — belong to and were defined by the
**project owner**. In short: the owner specified *what* to build and the constraints
to respect; the AI produced the *implementation*.

The project owner is responsible for reviewing, testing, and operating this firmware.
The software is provided "as is", without warranty of any kind.

**Contributions welcome.** If you are a C++ / ESPHome expert and would like to review
or improve the code, you are very welcome — please open an issue or a pull request.
See `CONTRIBUTING.md` for the design rationale and how to build and test.

**New / not-yet-integrated device types are especially welcome.** Today the gateway
maps Luxom relays (HA `switch`), dimmers (HA `light`) and shutters (HA `cover`).
Other Luxom device types (e.g. scenes/actions, sensors, other modules) are not
integrated yet — adding them is a great contribution. The entity type and behaviour
in Home Assistant are defined **entirely by the MQTT discovery payload** we publish
(`homeassistant/<component>/luxom_<id>/config`), so supporting a new type means:
(1) handle its frames on the bus, and (2) publish the right discovery config. See the
`entity_config` / `cover_config` helpers in `luxom_proto.h` and `CONTRIBUTING.md`.

## License

Apache License 2.0 — see [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).
