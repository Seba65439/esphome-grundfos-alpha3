# Komponent ESPHome dla pompy Grundfos ALPHA3 (GENI over BLE)

[English version (README.md)](./README.md)

Zewnętrzny komponent **ESPHome** dla mikrokontrolerów **ESP32** (ESP32, ESP32-C3, ESP32-S3), który odczytuje telemetrię i steruje pompą obiegową **Grundfos ALPHA3** przez Bluetooth Low Energy – bezpośrednio z **Home Assistant**, bez bramki Grundfos i bez aplikacji na telefonie.

Komponent obsługuje protokół **GENI (GENIbus / GENIpro over BLE)** oraz procedurę parowania i wiązania BLE SMP stosowaną przez oficjalną aplikację **Grundfos GO Remote**.

---

## 🌟 Możliwości

### Telemetria

| Klucz konfiguracji | Jednostka | Opis |
|---|---|---|
| `power` | W | Moc czynna pobierana z sieci (zgodna z wyświetlaczem pompy) |
| `speed` | RPM | Prędkość obrotowa wirnika |
| `flow` | m³/h | Szacowany przepływ |
| `head` | m | Rzeczywista wysokość podnoszenia (m sł. wody) |
| `current_setpoint` | m | Nastawa aktualnie używana przez pompę |
| `energy` | kWh | Licznik energii od pierwszego uruchomienia |
| `voltage` | V | Napięcie zasilania |
| `current` | A | Prąd silnika |
| `shaft_power` | W | Moc mechaniczna na wale |
| `temp_electronics` | °C | Temperatura elektroniki / falownika |
| `temp_motor` | °C | Temperatura uzwojeń silnika |
| `temp_liquid` | °C | Szacowana temperatura pompowanej cieczy |
| `alarm_code` | – | Surowy kod alarmu (`0` = brak alarmu) |

Sensory binarne: `pump_running` (silnik pracuje), `pump_paired` (więź BLE zapisana w NVS ESP32 lub aktywna zaszyfrowana sesja).

Sensory tekstowe: `operating_mode`, `control_mode`, `alarm_status` (opis alarmu), `pump_name` (nazwa zapisana w pompie, np. „DOM”).

### Sterowanie

- **Przełącznik `pump_power`** – `ON` → `Normalny`, `OFF` → `Stop`.
- **Select `operating_mode`**: `Normalny`, `Stop`, `Min` (krzywa minimalna), `Maks` (krzywa maksymalna – odpowietrzanie, szybkie dogrzanie).
- **Select `control_mode`**: `Ciśnienie stałe`, `Ciśnienie proporcjonalne`, `Charakterystyka stała`, `Tryb grzejnikowy`, `Tryb ogrzewania podłogowego`, `Grzejnikowe i podłogowe`.
- **Suwak `setpoint`** – nastawa wysokości podnoszenia **0,5 m – 5,0 m**, krok 0,1 m.
- **Przyciski `pair_pump` / `unpair_pump`** – rozpoczęcie parowania (szyfrowania) / usunięcie zapisanej więzi z NVS ESP32.

### Zachowanie komponentu

- Polecenia sterujące są wykonywane tylko przy zaszyfrowanym połączeniu **i** po co najmniej jednym odczycie bieżącego stanu pompy – w przeciwnym razie są pomijane z ostrzeżeniem w logu (komponent nigdy nie zapisuje do pompy wartości „w ciemno”).
- Po rozłączeniu BLE pomiary na żywo przechodzą w Home Assistant w stan *nieznany*; licznik energii zachowuje ostatnią wartość.
- Po pierwszym odczycie stanu w każdym połączeniu komponent odsyła bieżący stan pracy, co przełącza pompę w tryb sterowania zdalnego (z magistrali).

---

## 🔐 Parowanie z przyciskiem na pompie

Pompa ALPHA3 wymaga szyfrowanego połączenia BLE (SMP „Just Works” z wiązaniem), które trzeba potwierdzić **fizycznym przyciskiem połączenia** na pompie:

1. ESP32 łączy się z adresem MAC pompy i wysyła żądanie parowania.
2. W logu pojawia się komunikat `>>> NACIŚNIJ TERAZ PRZYCISK POŁĄCZENIA (RADIA) NA POMPIE! <<<` – naciśnij przycisk na pompie.
3. ESP32 i pompa wymieniają klucze; więź zapisywana jest trwale w NVS ESP32, a sensor `pump_paired` przechodzi w `ON`.
4. Przy kolejnych uruchomieniach ESP32 łączy się i szyfruje połączenie automatycznie – przycisk nie jest już potrzebny.

Jeśli pompa została zresetowana lub sparowana z innym urządzeniem i szyfrowanie przestaje działać, użyj przycisku **Usuń sparowanie**, a następnie sparuj ponownie.

---

## 🛠️ Sprzęt

Dowolne ESP32 z BLE: klasyczne ESP32 (ESP-WROOM-32, DevKit), **ESP32-C3** (SuperMini, XIAO – kompaktowe, polecane), ESP32-S3. Przykładowa konfiguracja jest przygotowana dla ESP32-C3 z frameworkiem Arduino (przetestowany zestaw):

```yaml
# Klasyczne ESP32
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

## 🚀 Pierwsze uruchomienie

1. **Ustal adres MAC BLE pompy**, np. aplikacją *nRF Connect* w pobliżu pompy albo z listy urządzeń Bluetooth telefonu po połączeniu z Grundfos GO Remote.
2. **Utwórz `secrets.yaml`** obok konfiguracji na podstawie [`secrets.yaml.example`](./secrets.yaml.example) (Wi-Fi, hasło AP awaryjnego, hasło OTA, klucz szyfrowania API).
3. **Wpisz adres MAC** w [`grundfos_alpha3_esp32.yaml`](./grundfos_alpha3_esp32.yaml):
   ```yaml
   substitutions:
     pump_mac_address: "XX:XX:XX:XX:XX:XX"  # adres MAC Twojej pompy
   ```
4. **Wgraj firmware** (`esphome run grundfos_alpha3_esp32.yaml` lub *Install* w ESPHome Dashboard) i obserwuj logi. Gdy pojawi się
   ```text
   [W][grundfos_alpha3]: >>> NACIŚNIJ TERAZ PRZYCISK POŁĄCZENIA (RADIA) NA POMPIE! <<<
   ```
   **naciśnij przycisk połączenia na pompie ALPHA3**.
5. Parowanie się udało, gdy w logu pojawi się
   ```text
   [I][grundfos_alpha3]: [XX:XX:XX:XX:XX:XX] Połączenie z pompą zaszyfrowane (sparowano).
   ```
   Po kilku sekundach wszystkie encje pojawią się w Home Assistant.

---

## 📦 Użycie we własnej konfiguracji

```yaml
external_components:
  - source: github://Seba65439/esphome-grundfos-alpha3
    components: [ grundfos_alpha3 ]
```

Wymagane są też sekcje `esp32_ble`, `esp32_ble_tracker` i `ble_client` – pełny przykład ze wszystkimi platformami i kluczami encji znajduje się w [`grundfos_alpha3_esp32.yaml`](./grundfos_alpha3_esp32.yaml) oraz w [README.md](./README.md#-using-the-component-in-your-own-configuration). Każdy klucz encji jest opcjonalny.

---

## 📊 Panel Home Assistant (Lovelace)

[`home_assistant_dashboard.yaml`](./home_assistant_dashboard.yaml) zawiera gotowy widok: zegary podnoszenia, przepływu i mocy, panel sterowania, diagnostykę, baner alarmowy i wykres historii. Identyfikatory encji odpowiadają przykładowej konfiguracji (`friendly_name: "Pompa Grundfos ALPHA3"`) – dostosuj je, jeśli zmieniłeś nazwy.

Import: *Edytuj pulpit → + (dodaj widok) → ⋮ → Edytuj w YAML*, wklej zawartość pliku i zapisz.

---

## 🔬 Uwagi o protokole

Szczegóły warstwy GATT, budowy ramek GENI, sumy CRC-16 i używanych obiektów opisano w sekcji [Protocol Notes](./README.md#-protocol-notes) angielskiego README.

---

## ⚖️ Zastrzeżenie

Projekt jest niezależną inicjatywą na potrzeby automatyki domowej. **Nie jest** oficjalnym produktem Grundfos i nie jest powiązany z Grundfos Holding A/S ani przez nią wspierany. Wszystkie znaki towarowe należą do ich właścicieli.

Używasz na własną odpowiedzialność – przed zmianą pracy pompy sprawdź parametry instalacji grzewczej.

---

## 📄 Licencja

[MIT](LICENSE)
