# Status dashboard — LuxomHA

Snapshot of where the project stands. Legend: ✅ done · 🟡 partial / to verify · ⛔ blocked / open.

| Area | Status | Notes |
|---|---|---|
| Project scaffolding (ESPHome, uv, OTA, secrets) | ✅ | `sw_artifacts/` complete |
| Production gateway `luxom_gateway.yaml` | ✅ | written; **not yet flashed/tested on hardware** |
| Shutter discovery `luxom_cover_discovery.yaml` | ✅ | written; not yet run on hardware |
| Lights/outlets/dimmers discovery (sniffing + `*P` scan) | ✅ | implemented |
| Shutters (cover, two-phase) | ✅ | implemented (up/down + single-address) |
| Documentation (architecture, protocol, decisions, credits, refs, user guide) | ✅ | `documents/` per blueprint, English |
| Automated tests (host unit + esphome config + compile) | ✅ | all green; both YAMLs compile |
| CI (GitHub Actions) + Codecov | ✅ | runs on push; Codecov needs `CODECOV_TOKEN` secret |
| License | ✅ | Apache-2.0 (LICENSE + NOTICE + SPDX headers) |
| Repos pushed (private) | ✅ | drake69/LuxomHA (code), drake69/LuxomHA-docs |
| Reference PDFs (LUXOM_ASCII) | 🟡 | not downloadable here; URLs cited; ELAN note archived |
| DS65L port (2300 vs 2301) | 🟡 | configurable; verify on the unit |
| `@1*PW-` password code | 🟡 | basic handshake only; extend if a code is present |
| Hardware / field test (flash + on-bus) | ⛔ | **not yet tested on real hardware** — volunteers welcome |

## Immediate next steps

1. Fill `secrets.yaml`, flash `luxom_gateway.yaml` over USB, confirm TCP connect
   (try port 2300 then 2301).
2. Operate wall switches → verify entities appear in HA; run "scan bus".
3. For shutters: flash `luxom_cover_discovery.yaml`, capture up/down addresses,
   put them in `luxom_covers`, re-flash the gateway.

## Known limitations

See `../04_software_engineering/01_architecture.md` (Limitations) and
`02_luxom_protocol.md` (§10).
