# Integracja ESPHome dla pompy Grundfos ALPHA3 (GENI over BLE)

[English version (README.md)](./README.md)

Kompletny komponent **ESPHome** dla mikrokontrolera **ESP32** (oraz **ESP32-C3**), umożliwiający bezpośrednie monitorowanie parametrów oraz bezprzewodowe sterowanie pompą obiegową **Grundfos ALPHA3** (oraz kompatybilnymi pompami Grundfos z łącznością Bluetooth) z poziomu **Home Assistant**.

Komponent został opracowany na drodze inżynierii wstecznej oficjalnego protokołu **GENI (GENIbus / GENIpro over BLE)** oraz procedury bezpieczeństwa i parowania BLE SMP z wykorzystaniem aplikacji **Grundfos GO Remote**.

---

## 🌟 Możliwości i funkcje komponentu

### 1. Odczyt parametrów pomiarowych (Telemetria na żywo):
- **Pobór mocy ($W$)**: Aktualna moc czynna pobierana przez silnik (zgodna 1:1 ze wskazaniami na ekranie aplikacji i wyświetlaczu pompy).
- **Prędkość obrotowa ($RPM$)**: Rzeczywista prędkość obrotowa wirnika silnika.
- **Szacowany przepływ ($m^3/h$)**: Wydajność hydrauliczna pompy w czasie rzeczywistym.
- **Wysokość podnoszenia ($m$)**: Rzeczywista różnica ciśnień wytwarzana przez pompę (metry słupa wody).
- **Aktualna wartość zadana ($m$)**: Nastawa zadana przez użytkownika lub wyliczona przez AutoAdapt.
- **Całkowity licznik energii ($kWh$)**: Narastające zużycie energii elektrycznej od momentu pierwszego uruchomienia pompy.
- **Napięcie zasilania ($V$)**: Aktualne napięcie w sieci zasilającej 230V.
- **Prąd silnika ($A$)**: Pobór prądu przez uzwojenia silnika.
- **Moc mechaniczna ($W$)**: Moc oddawana na wale silnika.
- **Temperatury ($^\circ C$)**:
  - Temperatura modułu sterującego / falownika
  - Temperatura uzwojeń silnika
  - Szacowana temperatura pompowanej cieczy / wody w instalacji
- **Stan pracy silnika (Binary Sensor)**: Flaga informująca, czy pompa aktualnie tłoczy wodę.
- **Status sparowania BLE (Binary Sensor)**: Informuje, czy nawiązano bezpieczne, zaszyfrowane połączenie (BONDED) z pompą.

### 2. Sterowanie pompą (Kontrola z Home Assistant):
- **Przełącznik zasilania (ON/OFF)**:
  - `ON` $\rightarrow$ tryb normalnej pracy (`Normalny`)
  - `OFF` $\rightarrow$ natychmiastowe zatrzymanie pompy (`Stop`)
- **Wybór stanu pracy (Operating Mode Select)**:
  - `Normalny`: standardowa praca według wybranego trybu
  - `Stop`: zatrzymanie pompy
  - `Min`: wymuszenie pracy na krzywej minimalnej
  - `Maks`: wymuszenie maksymalnej wydajności (odpowietrzanie, szybkie dogrzanie)
- **Wybór trybu regulacji (Control Mode Select)**:
  - `AutoAdapt`: inteligentne dopasowanie charakterystyki przez pompę
  - `Ciśnienie proporcjonalne`: instalacje dwururowe z zaworami termostatycznymi
  - `Ciśnienie stałe`: instalacje ogrzewania podłogowego
  - `Charakterystyka stała`: praca ze stałą prędkością obrotową (I, II, III bieg)
  - `Tryb grzejnikowy` (Radiator mode)
  - `Tryb ogrzewania podłogowego` (Underfloor mode)
  - `Grzejnikowe i podłogowe` (Combined mode)
- **Regulacja wydajności / Nastawa wysokości podnoszenia (Number Slider)**:
  - Płynny suwak w Home Assistant w zakresie od **1.0 m** do **6.0 m** (krok 0.1 m).
- **Przyciski funkcyjne (Buttons)**:
  - `Paruj pompę`: ręczne wymuszenie procedury szyfrowania BLE SMP i parowania z przycisku pompy.
  - `Usuń sparowanie`: reset zapamiętanej więzi (bond) w ESP32.

### 3. Diagnostyka i alarmy:
- **Kod alarmu (Sensor numeryczny)**: Surowy kod błędu z rejestru pompy (0 = OK).
- **Opis błędu (Text Sensor)**: Czytelny opis w języku polskim (np. *Suchobieg*, *Zablokowany wirnik* itp.).
- **Nazwa pompy (Text Sensor)**: Nazwa odczytana z pompy (np. "DOM").

---

## 🔐 Jak działa parowanie z przyciskiem na pompie ALPHA3

Pompy **Grundfos ALPHA3** wymagają zabezpieczenia **BLE SMP (Security Manager Protocol) z potwierdzeniem fizycznym na urządzeniu** (ochrona przed łączeniem się przez niepowołane osoby):

1. **Wykrycie pompy**: ESP32 skanuje eter w poszukiwaniu pompy.
2. **Nawiązanie połączenia**: ESP32 łączy się z adresem MAC pompy i wysyła żądanie autoryzacji szyfrowania (`pair()`).
3. **Potwierdzenie przyciskiem**:
   - Dioda łączności / radia na przednim panelu pompy zaczyna migać.
   - W konsoli ESPHome pojawia się komunikat:
     `>>> NACIŚNIJ TERAZ PRZYCISK POŁĄCZENIA (RADIA) NA POMPIE! <<<`
   - **Naciśnij fizyczny przycisk na obudowie pompy ALPHA3** (przycisk łączności ze strzałkami / symbolem radia).
4. **Zaszyfrowanie i zapamiętanie (Bonding)**:
   - Pompa zatwierdza połączenie z ESP32.
   - ESP32 i pompa wymieniają klucze kryptograficzne i zapisują je trwale w pamięci NVS.
   - Sensor `Status sparowania BLE` zmienia stan na `ON`.
   - ESPHome natychmiast odczytuje wszystkie parametry pracy pompy.
   - **Przy kolejnych uruchomieniach klikanie przycisku nie jest już potrzebne** – ESP32 łączy się automatycznie z zapamiętanymi kluczami!

---

## 🛠️ Wybór płytki (ESP32 vs ESP32-C3)

W pliku `grundfos_alpha3_esp32.yaml` możesz wybrać swój model ESP32:

### Dla standardowego ESP32 (ESP-WROOM-32, NodeMCU, DevKit):
```yaml
esp32:
  board: esp32dev
  framework:
    type: arduino
```

### Dla płytek ESP32-C3 (np. SuperMini C3, Xiao C3):
```yaml
esp32:
  board: esp32-c3-devkitm-1
  variant: esp32c3
  framework:
    type: arduino
```

---

## 🚀 Instrukcja pierwszego uruchomienia

### Krok 1: Znalezienie adresu MAC pompy ALPHA3

W pliku `grundfos_alpha3_esp32.yaml` wbudowany jest automatyczny skaner Grundfos:
1. Wgraj oprogramowanie na ESP32 (nawet z domyślnym adresem MAC).
2. Otwórz podgląd logów (**Logs**) w ESPHome Dashboard lub monitorze portu szeregowego.
3. Podejdź do pompy ALPHA3 i naciśnij na niej przycisk połączenia.
4. W logach pojawi się wyróżniony komunikat:
   ```text
   [I][alpha3_scanner]: ============================================================
   [I][alpha3_scanner]: >>> ZNALEZIONO POMPĘ GRUNDFOS W ZASIĘGU BLE! <<<
   [I][alpha3_scanner]:   Nazwa rozgłaszana: 'Grundfos ALPHA3'
   [I][alpha3_scanner]:   Adres MAC pompy:   B4:E6:2D:XX:XX:XX
   [I][alpha3_scanner]:   Siła sygnału RSSI: -58 dBm
   [I][alpha3_scanner]: Wpisz powyższy adres MAC do konfiguracji YAML jako pump_mac_address!
   [I][alpha3_scanner]: ============================================================
   ```
5. Skopiuj ten adres MAC.

### Krok 2: Wpisanie adresu MAC i sieci Wi-Fi

W pliku `grundfos_alpha3_esp32.yaml` uzupełnij:
```yaml
substitutions:
  name: "grundfos-alpha3-controller"
  friendly_name: "Pompa Grundfos ALPHA3"
  pump_mac_address: "B4:E6:2D:XX:XX:XX"  # Twój skopiowany MAC pompy

wifi:
  ssid: "TWOJA_NAZWA_WIFI"
  password: "TWOJE_HASLO_WIFI"
```

### Krok 3: Wgranie i parowanie

1. Kliknij **Install** w ESPHome.
2. Gdy ESP32 uruchomi się i połączy z pompą, w logach pojawi się:
   ```text
   [W][grundfos_alpha3]: >>> NACIŚNIJ TERAZ PRZYCISK POŁĄCZENIA (RADIA) NA POMPIE! <<<
   ```
3. Naciśnij fizyczny przycisk na pompie ALPHA3.
4. W logach pojawi się:
   ```text
   [I][grundfos_alpha3]: SUKCES! Pompa Grundfos ALPHA3 sparowana i połączenie zaszyfrowane!
   ```
5. Od tej chwili w Home Assistant pojawią się wszystkie encje: moc, przepływ, podnoszenie, temperatura, przełącznik i suwaki!

---

## 📊 Gotowy panel Home Assistant (Lovelace)

W repozytorium przygotowano plik z gotowym, estetycznym panelem kontrolnym:
👉 [`home_assistant_dashboard.yaml`](./home_assistant_dashboard.yaml)

Zawiera:
- **Wskaźniki zegarowe (Gauges)**: Pobór mocy, przepływ $m^3/h$, wysokość podnoszenia $m$ ze strefami kolorystycznymi.
- **Sterowanie**: Włącznik pompy ON/OFF, wybór trybu regulacji (AutoAdapt, stałe ciśnienie itp.) oraz płynny suwak nastawy.
- **Kafelki diagnostyczne**: Temperatury (woda, silnik, elektronika), obroty RPM, licznik zużytej energii kWh, parametry zasilania.
- **Karty warunkowe**: Automatycznie wyświetlany czerwony baner w razie błędu/alarmu oraz przycisk parowania BLE, jeśli urządzenie wymaga ponownego powiązania.
- **Wykres telemetryczny na żywo**: Historia mocy i przepływu w czasie.
