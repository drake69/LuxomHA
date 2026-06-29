# Credits & attributions

## Luxom protocol knowledge

The Luxom ASCII protocol facts (opcodes, framing, handshake, dimmer arithmetic)
were derived from:

- The **openHAB binding** `org.openhab.binding.luxom` by K. Jespers, licensed
  **EPL-2.0**, used as an *interface reference only* (no source code copied).
- The **ELAN g! integration note** for Luxom (see `references/`), which confirms
  the addressing model, the lack of auto-discovery, the DS65L Ethernet port, and
  the shutter device types.
- The official Luxom ASCII documentation — `LUXOM_ASCII.pdf` and
  `LUXOM_ASCII_extended.pdf`. These are **not bundled** here: they were hosted on
  `luxom.be` (now offline) and remain on `old.luxom.io`, which blocks automated
  download. See `references/00_references.md` for the URLs and how to fetch them.

> License note: the openHAB binding is EPL-2.0 (file-level copyleft). A literal
> translation of its Java source would be a derivative work bound by EPL-2.0. This
> firmware is an **original re-implementation** of the interface facts (opcodes,
> framing, arithmetic), which are not protected expression — so it is not a
> derivative of the binding.

The openHAB binding supports only `switch` and `dimmer` thing types (no covers).
Shutter handling here is original, informed by the ELAN note's 1-/2-Button Shutter
model.

## Device address map — PlusConfig

The `(group/module , address/output)` map is defined by the installer in Luxom
**PlusConfig**. There is no bus enumeration, so PlusConfig (or passive sniffing) is
the authoritative source of the device list. See `references/00_references.md`.

## Reference implementation

An earlier standalone Arduino sketch (`luxom_mqtt_gateway.ino`) was the project
owner's own **initial draft** of the idea — the seed this ESPHome firmware grew
from. It served as the functional reference for the protocol handling and is now
superseded by the ESPHome configuration.

## Project layout / conventions

Project structure and ESPHome packaging conventions (uv-managed ESPHome,
`secrets.yaml`, OTA flashing) follow the sibling project **NoPowerWasted2500**;
documentation hierarchy follows `PROJECT_BLUEPRINT` / `PRODUCT_BLUEPRINT`.

## Authorship

The C++ / ESPHome implementation was written entirely by **Claude** (Anthropic's AI
assistant). The software-engineering, functional, operational and design requirements
belong to and were defined by the **project owner**, who is responsible for review,
testing and operation. See the README "Authorship & disclaimer" section.

## License

Licensed under the **Apache License 2.0**. Copyright 2026 Luigi Corsaro. See the
`LICENSE` and `NOTICE` files in the code repository (`sw_artifacts/`).
