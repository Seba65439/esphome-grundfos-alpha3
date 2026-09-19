# ESPHome Component for Grundfos ALPHA3 (GENI over BLE)

[![ESPHome](https://img.shields.io/badge/ESPHome-Component-blue?logo=esphome)](https://esphome.io/)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-Ready-41BDF5?logo=home-assistant)](https://www.home-assistant.io/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

[Wersja polska (README.pl.md)](./README.pl.md)

An **ESPHome** external component for **ESP32** microcontrollers (ESP32, ESP32-C3, ESP32-S3) that reads live telemetry from and controls a **Grundfos ALPHA3** circulator pump over Bluetooth Low Energy, directly from **Home Assistant** – no Grundfos gateway and no phone app required.

It speaks the pump's **GENI (GENIbus / GENIpro over BLE)** protocol and handles the BLE SMP pairing/bonding flow used by the official **Grundfos GO Remote** app.

> [!NOTE]
> Entity option values and log messages are in **Polish** (e.g. `Normalny`, `Ciśnienie stałe`). The tables below list the English meaning of every value.

---

## ✨ Features

### Telemetry

| Config key | Unit | Description |
|---|---|---|
| `power` | W | Active electrical power (matches the pump display) |
| `speed` | RPM | Rotor speed |
| `flow` | m³/h | Estimated flow rate |
| `head` | m | Actual head (differential pressure, metres of water column) |
| `current_setpoint` | m | Setpoint currently used by the pump |
| `energy` | kWh | Lifetime energy counter |
| `voltage` | V | Mains supply voltage |
| `current` | A | Motor current |
| `shaft_power` | W | Mechanical power on the shaft |
| `temp_electronics` | °C | Control electronics / inverter temperature |
| `temp_motor` | °C | Motor winding temperature |
| `temp_liquid` | °C | Estimated pumped liquid temperature |
| `alarm_code` | – | Raw alarm code (`0` = no alarm) |

Binary sensors: `pump_running` (motor running), `pump_paired` (BLE bond stored in ESP32 NVS or encrypted session active).

Text sensors: `operating_mode`, `control_mode`, `alarm_status` (human-readable alarm), `pump_name` (name stored in the pump).

### Control

- **`pump_power` switch** – `ON` → `Normalny` (normal operation), `OFF` → `Stop`.
- **`operating_mode` select**

  | Option | Meaning |
  |---|---|
  | `Normalny` | Normal operation according to the control mode |
  | `Stop` | Pump stopped |
  | `Min` | Minimum curve |
  | `Maks` | Maximum curve (venting, fast warm-up) |

- **`control_mode` select**

  | Option | Meaning |
  |---|---|
  | `Ciśnienie stałe` | Constant pressure |
  | `Ciśnienie proporcjonalne` | Proportional pressure |
  | `Charakterystyka stała` | Constant curve |
  | `Tryb grzejnikowy` | Radiator mode |
  | `Tryb ogrzewania podłogowego` | Underfloor heating mode |
  | `Grzejnikowe i podłogowe` | Combined radiator + underfloor mode |

- **`setpoint` number** – head setpoint slider, **0.5 m – 5.0 m**, step 0.1 m.
- **`pair_pump` / `unpair_pump` buttons** – start pairing (encryption) / remove the stored bond from ESP32 NVS.

### Behaviour notes

- Control commands are accepted only while the BLE session is encrypted **and** the current pump state has been read at least once – otherwise they are ignored with a warning in the log (so the component never writes guessed values to the pump).
- After a BLE disconnect, live measurements become *unknown* in Home Assistant; the energy counter keeps its last value.
- On the first successful read after each connection the component re-sends the current operating state, which switches the pump into remote (bus) control mode.

---

## 🔐 Pairing & Security

ALPHA3 pumps require an encrypted BLE link (SMP "Just Works" with bonding) that must be confirmed with the **physical connect button** on the pump:

```
[ ESP32 ]                                      [ Grundfos ALPHA3 ]
    |---- 1. BLE connect (pump MAC address) -------->|
    |---- 2. SMP pairing request (Just Works) ------>|
    |              >>> PRESS THE CONNECT BUTTON ON THE PUMP <<<
    |<--- 3. Key exchange, link encrypted ----------|
    |     Keys stored in ESP32 NVS (bond)            |
    |<=== 4. GENI telemetry / control ==============>|
```

Once bonded, the ESP32 reconnects and re-encrypts automatically after every reboot – the button is needed only once. If the pump is reset or paired with another device and encryption starts failing, press **Unpair** and pair again.

---

## 🛠️ Hardware

Any ESP32 with BLE: classic ESP32 (ESP-WROOM-32, DevKit), **ESP32-C3** (SuperMini, XIAO – compact, recommended), ESP32-S3. The example configuration targets ESP32-C3 with the Arduino framework (the tested setup):

```yaml
# Classic ESP32
esp32:
  board: esp32dev
  framework:
    type: arduino

# ESP32-C3
esp32:
  board: esp32-c3-devkitm-1
  variant: esp32c3
  framework:
    type: arduino
```

---

## 🚀 Quick Start

1. **Find the pump's BLE MAC address**, e.g. with a BLE scanner app such as *nRF Connect* near the pump, or from the phone's Bluetooth device list after connecting with Grundfos GO Remote.
2. **Create `secrets.yaml`** next to the configuration, based on [`secrets.yaml.example`](./secrets.yaml.example) (Wi-Fi, fallback AP, OTA password and API encryption key).
3. **Set the MAC address** in [`grundfos_alpha3_esp32.yaml`](./grundfos_alpha3_esp32.yaml):
   ```yaml
   substitutions:
     pump_mac_address: "XX:XX:XX:XX:XX:XX"  # your pump's MAC address
   ```
4. **Flash** (`esphome run grundfos_alpha3_esp32.yaml` or *Install* in the ESPHome Dashboard) and watch the logs. When you see
   ```text
   [W][grundfos_alpha3]: >>> NACIŚNIJ TERAZ PRZYCISK POŁĄCZENIA (RADIA) NA POMPIE! <<<
   ```
   ("press the connect button on the pump now") – **press the connect button on the ALPHA3**.
5. Pairing succeeded when the log shows
   ```text
   [I][grundfos_alpha3]: [XX:XX:XX:XX:XX:XX] Połączenie z pompą zaszyfrowane (sparowano).
   ```
   All entities populate in Home Assistant within a few seconds.

---

## 📦 Using the Component in Your Own Configuration

```yaml
external_components:
  - source: github://Seba65439/esphome-grundfos-alpha3
    components: [ grundfos_alpha3 ]

esp32_ble:
  io_capability: none

esp32_ble_tracker:
  scan_parameters:
    interval: 1100ms
    window: 300ms
    active: false

ble_client:
  - mac_address: "XX:XX:XX:XX:XX:XX"
    id: alpha3_ble
    auto_connect: true
    on_numeric_comparison_request:
      - then:
          - ble_client.numeric_comparison_reply:
              id: alpha3_ble
              accept: true
    on_passkey_request:
      - then:
          - ble_client.passkey_reply:
              id: alpha3_ble
              passkey: 0

grundfos_alpha3:
  id: pump_alpha3
  ble_client_id: alpha3_ble
  update_interval: 10s

sensor:
  - platform: grundfos_alpha3
    grundfos_alpha3_id: pump_alpha3
    power:
      name: "Pump power"
    flow:
      name: "Pump flow"
    head:
      name: "Pump head"
```

All platforms (`sensor`, `binary_sensor`, `text_sensor`, `switch`, `select`, `number`, `button`) and their keys are shown in the full example [`grundfos_alpha3_esp32.yaml`](./grundfos_alpha3_esp32.yaml). Every key is optional – configure only the entities you need.

---

## 📊 Home Assistant Dashboard

[`home_assistant_dashboard.yaml`](./home_assistant_dashboard.yaml) contains a ready-made Lovelace view (gauges for head, flow and power, control panel, diagnostics, alarm banner, history graph). The entity IDs assume the example configuration (`friendly_name: "Pompa Grundfos ALPHA3"`); adjust them if you renamed anything.

Import: *Edit dashboard → + (add view) → ⋮ → Edit in YAML*, paste the file content and save.

---

## 🔬 Protocol Notes

- **GATT**: service `0xFE5D` (Grundfos A/S; 128-bit form `859cffd0-036e-432a-aa28-1a0085b87ba9`), a single characteristic `859cffd1-036e-432a-aa28-1a0085b87ba9` used for *Write Without Response* (requests) and *Notify* (replies). Writes are split into 20-byte chunks, exactly as the official app does.
- **Frame**: `[SD] [LEN] [DA] [SA] [APDU…] [CRC16]` – SD `0x27` = request, `0x24` = reply; LEN counts bytes after itself excluding the CRC; DA/SA `0x20` (pump) / `0xF8` (master).
- **APDU**: `[CLASS] [OS(2 bits) | LEN(6 bits)] [DATA…]`.
- **CRC**: CRC-16-CCITT (poly `0x1021`, init `0xFFFF`, xorout `0xFFFF`), computed from LEN to the end of the APDU (the SD byte is excluded).
- **Objects used**:

  | Class | Request | Content |
  |---|---|---|
  | 5 | `4B` | Actual alarm code |
  | 7 | `11` | Pump name (ASCII) |
  | 10 | `57 00 45` | Voltage, current, power, shaft power, speed, temperatures, running flag |
  | 10 | `5D 01 21` | Flow, head, liquid temperature |
  | 10 | `56 00 06` | Operating state, control mode, setpoint [Pa] (read / write) |
  | 10 | `56 00 0A` | Control mode (read / write) |
  | 10 | `56 00 10` | Setpoint register [Pa] (write) |
  | 10 | `57 00 01` | Lifetime energy [J] (double) |

---

## ⚖️ Disclaimer

This is an independent project created for home-automation interoperability. It is **not** an official Grundfos product and is not affiliated with, endorsed by or supported by Grundfos Holding A/S. All trademarks belong to their respective owners.

Use at your own risk and verify heating-system parameters before changing the pump's operation.

---

## 📄 License

[MIT](LICENSE)
