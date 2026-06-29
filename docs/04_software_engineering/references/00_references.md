# Luxom — external references & knowledge sources

This folder collects the authoritative external sources behind the protocol
knowledge base (`../02_luxom_protocol.md`). Where a document could not be archived
locally, the URL is given.

## Local files

| File | What it is |
|---|---|
| `ELAN_Luxom_integration_note_6.7.12.pdf` | ELAN g! integration note for Luxom (rev. 2014-09-23). Confirms: no auto-discovery (devices added manually via PlusConfig); DS65L Ethernet **port 2301**; device types incl. **1-Button Shutter**, **2-Button Shutter** (Open Gr/Ad + Close Gr/Ad), **Curtain/Blind**, Dimmer Load, On/Off Load, Action. |

## Official Luxom ASCII protocol (not archivable here)

The canonical command reference lives in these PDFs. They were hosted on
`luxom.be` (now **offline**); copies remain on `old.luxom.io`, but that host
**blocks automated download** (TLS SNI / robots), so they must be fetched manually
from a browser:

- `LUXOM_ASCII.pdf` — https://old.luxom.io/uploads/ppfiles/27/LUXOM_ASCII.pdf
- `LUXOM_ASCII_extended.pdf` — https://old.luxom.io/uploads/ppfiles/28/LUXOM_ASCII_extended.pdf
- DS65L datasheet (`LTF_E_DS65L.pdf`) — https://old.luxom.io/uploads/ppfiles/32/LTF_E_DS65L.pdf

> If you download them, drop them in this folder and add them to the table above.

## PlusConfig (device address map)

**PlusConfig** (a.k.a. *PlusConfig 2000*) is the Luxom installer configuration
software. It programs the modules and holds the authoritative map
`(group/module , address/output) → physical load`. There is **no bus enumeration
command**, so PlusConfig (or passive sniffing of the bus) is the only way to obtain
the device list and to know which `M,OO` address drives which load. This is why the
gateway discovers devices by sniffing/probing and why shutters are configured by
hand after `luxom_cover_discovery.yaml`.

- PlusConfig tutorial (VDAB campus, Dutch): https://sites.google.com/vdabcampus.be/domotica-immotica-van-ontwerp-/homepage/5-workshop-luxom/5-c-plusconfig/5-c-1-voorbereiding-installatie

## Other useful links

- openHAB Luxom binding (interface reference, EPL-2.0): https://www.openhab.org/addons/bindings/luxom/
- Homey TCP/IP ASCII interface thread: https://community.homey.app/t/luxom-domotics-tcp-ip-ascii-interface/9772
- Home Assistant community thread: https://community.home-assistant.io/t/luxom-ip-bases-home-automation/153652
- Luxom current site / successor: https://luxom.io/ — https://homecenter.be/luxom/
- "LUXOM BASIS" (basics, Dutch): https://docplayer.nl/12468996-Luxom-basis-2008-versie-1-07.html

## Provenance note

The protocol opcodes used by this gateway (`*S`, `*C`, `*A`, `*Z`, `*P`, framing on
`;`, handshake `@1*PW-` → `*?`, dimmer hex level) are corroborated by the openHAB
binding and the ELAN note. The exact, complete ASCII reference is in the PDFs above,
which were not freely downloadable from this environment.
