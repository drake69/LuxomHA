# Luxom Gateway — User guide (Home Assistant)

A step-by-step guide to connect your **Luxom** home-automation system to **Home
Assistant**, using a small ESP32 device. **No programming knowledge required** —
everything is done from Home Assistant's graphical interface.

By the end your Luxom **lights, outlets, dimmers and shutters** will appear in Home
Assistant and you can control them like any other smart device.

---

## 1. What you need

- A **Home Assistant** installation (the add-on store available — i.e. Home
  Assistant OS or Supervised).
- The **MQTT** integration already working in Home Assistant (the *Mosquitto broker*
  add-on is the usual choice). If you don't have it yet, install the **Mosquitto
  broker** add-on and add the **MQTT** integration first.
- An **ESP32** board (a small Wi-Fi microcontroller, ~10€).
- Your Luxom **DS65L** network interface, connected to your network. You need:
  - its **IP address** (e.g. `192.168.0.50`),
  - its **port** (try **2301**; some installations use **2300**).
- A **USB cable** to connect the ESP32 to your computer for the very first setup.

> Important: the DS65L usually accepts **only one** connection at a time. If you
> also use openHAB/Homey/ELAN on the same DS65L, turn that off while using this
> gateway.

---

## 2. Install the ESPHome add-on

1. In Home Assistant, go to **Settings → Add-ons → Add-on Store**.
2. Search for **ESPHome Device Builder** and click **Install**.
3. When installed, click **Start**, enable **Show in sidebar**, and open it.

You now have a graphical ESPHome dashboard inside Home Assistant.

---

## 3. Add the gateway configuration

1. In the ESPHome dashboard click **+ New device → Continue**, give it a name like
   `luxom-gateway`, choose **ESP32**, and finish. (We will replace its content.)
2. Find the new device card, click the **⋮ (three dots) → Edit**.
3. **Delete everything** in the editor and **paste** the full content of the file
   `luxom_gateway.yaml` (provided with this project).
4. Open (or create) the **`secrets.yaml`** file in the same editor (top-right
   **Secrets** button) and fill in your values — see the next section.
5. Click **Save**.

---

## 4. Fill in your details (`secrets.yaml`)

This is the **only** thing you ever edit. Copy the block below and replace the
values. **Keep the quotes exactly as shown** — some lines have quotes inside quotes,
that is on purpose.

```yaml
# Wi-Fi
wifi_ssid: "YourWiFiName"
wifi_password: "YourWiFiPassword"

# Home Assistant MQTT broker
mqtt_host: "192.168.0.10"          # IP of your Home Assistant / Mosquitto
mqtt_user: "mqtt"
mqtt_pass: "your-mqtt-password"

# Luxom DS65L interface
luxom_host: '"192.168.0.50"'       # keep the inner quotes!
luxom_port: "2301"                 # quoted; try "2301" first, if it fails use "2300"

# Address range for the "scan bus" button (leave as-is unless told otherwise)
luxom_probe_modules: '"1,2,3,4,5,6,7,8,9,A,B,C,D,E,F,G,H"'   # keep inner quotes!
luxom_probe_out_max: "16"          # quoted

# Shutters — leave empty for now, we set these up in section 8
luxom_covers: '""'

# OTA (over-the-air update) password — choose any password
ota_password: "choose-any-password"
```

Click **Save**.

---

## 5. First installation (via USB)

1. Plug the ESP32 into your computer with the USB cable.
2. In the ESPHome dashboard, on the gateway card click **⋮ → Install**.
3. Choose **Plug into this computer** and follow the browser prompts (pick the USB
   serial port). Wait for the build and upload to finish (a few minutes the first
   time).
4. When it says it's done, you can **unplug** the ESP32 and power it anywhere near
   the DS65L with any USB charger. From now on updates happen **wirelessly** (just
   click Install → *Wirelessly*).

> **Wireless updates (OTA).** They are **password-protected** (`ota_password` in
> `secrets.yaml`). The ESPHome dashboard finds the gateway automatically by name
> (`luxom-gateway.local`), so you normally don't need its IP. If wireless updates
> can't find it, give it a **stable address** in one of two ways:
> - a **DHCP reservation** on your router (simplest), or
> - a **fixed IP**: uncomment the `manual_ip` block in `luxom_gateway.yaml` and set
>   `esp_static_ip` / `esp_gateway` / `esp_subnet` in `secrets.yaml`.

---

## 6. Check it is working

1. On the gateway card click **Logs** (this is the live log viewer).
2. You should see lines like:
   - `TCP connected to 192.168.0.50:2301`
   - `module info -> bus ready`
   - `subscribed to MQTT command topics`
3. If instead you see `TCP connect ... failed` repeatedly, the port is probably
   wrong: change `luxom_port` to the other value (2300/2301) in `secrets.yaml`,
   Save, and Install again. Also re-check the DS65L IP address.

The log lines mean:
- `BUS>` / `BUS<` — a message sent / received on the Luxom bus,
- `MQTT<` — a command coming from Home Assistant,
- `MQTT>` — a status sent to Home Assistant.

---

## 7. Your lights and outlets appear automatically

There is **no list to fill in**. The gateway learns your devices as they are used:

- **Just operate your Luxom wall switches.** Each light/outlet/dimmer you press
  shows up in Home Assistant automatically (under **Settings → Devices & Services →
  MQTT → Luxom Gateway**).
- Prefer to find them all at once? In Home Assistant open the **Luxom Gateway**
  device and press the **"Luxom: scan bus"** button. The gateway gently queries the
  whole address range and adds whatever answers.

The new entities get generic names like *“Luxom 2,01”*. Just **rename** them in Home
Assistant (click the entity → settings ✏️ → change the name to “Kitchen light”,
etc.). Lights with dimming show a brightness slider automatically.

---

## 8. Set up your shutters (one-time, two steps)

Shutters need a quick learning step, because on the Luxom bus a shutter is two
relays (up and down) and they can't be guessed automatically.

**Step A — find the addresses**

1. In the ESPHome dashboard add a **second** configuration the same way as in
   section 3, but paste the file **`luxom_cover_discovery.yaml`** instead. Use the
   same `secrets.yaml`.
2. Install it on the ESP32 (this temporarily replaces the gateway — that's fine).
3. Open its **Logs**.
4. Walk to a shutter and **press UP**. In the log you will see a line like:
   `>>> ACTIVATION  address = 2,03`. That is the **UP** address — write it down.
5. **Press DOWN** on the same shutter → another line, e.g. `address = 2,04`. That is
   the **DOWN** address.
6. Repeat for every shutter.

**Step B — tell the gateway**

1. Open `secrets.yaml` and set the `luxom_covers` line, listing each shutter as
   `Name|UP|DOWN`, separated by `;`. Example for two shutters:

   ```yaml
   luxom_covers: '"Kitchen|2,03|2,04 ; Bedroom|2,05|2,06"'
   ```
   Keep the outer `'"..."'` quotes exactly.
2. Switch back to the **`luxom-gateway`** configuration and **Install** it again.
3. Your shutters now appear in Home Assistant as **covers** with open / close / stop
   buttons.

> You only do this once. After that the gateway runs on its own.

---

## 9. Everyday use

- Control everything from Home Assistant dashboards, automations, voice assistants —
  exactly like any other device.
- If the gateway or Home Assistant restarts, your devices stay (they are remembered
  by the MQTT broker).
- To update the firmware later: ESPHome dashboard → gateway card → **Install →
  Wirelessly**.

---

## 10. Troubleshooting

| Symptom | What to do |
|---|---|
| Log shows `TCP connect ... failed` | Wrong port or IP. Switch `luxom_port` between 2301/2300; check the DS65L IP; make sure no other system (openHAB/Homey/ELAN) holds the DS65L. |
| Nothing appears in Home Assistant | Confirm the **MQTT** integration is set up; check `mqtt_host`/user/password; press a wall switch or the **scan bus** button; open Logs and look for `MQTT>` lines. |
| A device doesn't show up | Operate it physically once (passive learning), or press **scan bus**. |
| A shutter shows as two switches | You haven't added it to `luxom_covers` yet — do section 8. |
| Wrong up/down on a shutter | Swap the two addresses in `luxom_covers` and re-install. |
| Entities have ugly names | Rename them in Home Assistant (this does not affect the gateway). |
| I changed `secrets.yaml` but nothing changed | You must **Install** again after saving. |

---

## 11. Glossary

- **ESP32** — the small Wi-Fi board that runs the gateway.
- **DS65L** — the Luxom network interface the gateway talks to.
- **MQTT** — the messaging system Home Assistant uses to receive the devices.
- **OTA** — wireless firmware update (no USB needed after the first time).
- **Address `M,OO`** — how Luxom identifies a device (module, output), e.g. `2,03`.

For technical details see the documents under `04_software_engineering/`.
