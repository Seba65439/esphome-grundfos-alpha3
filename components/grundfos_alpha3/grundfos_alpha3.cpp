#include "grundfos_alpha3.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace esphome::grundfos_alpha3 {

static const char *const TAG = "grundfos_alpha3";

static const esp32_ble_tracker::ESPBTUUID GRUNDFOS_SERVICE_UUID = esp32_ble_tracker::ESPBTUUID::from_uint16(0xFE5D);
static const esp32_ble_tracker::ESPBTUUID GRUNDFOS_SERVICE_UUID_128 =
    esp32_ble_tracker::ESPBTUUID::from_raw("859cffd0-036e-432a-aa28-1a0085b87ba9");
static const esp32_ble_tracker::ESPBTUUID GRUNDFOS_CHAR_UUID =
    esp32_ble_tracker::ESPBTUUID::from_raw("859cffd1-036e-432a-aa28-1a0085b87ba9");

// ---------------------------------------------------------------------------
// Protokół GENI (GENIbus / GENIpro over BLE)
//
// Ramka: [SD] [LEN] [DA] [SA] [APDU...] [CRC16_HI] [CRC16_LO]
//   LEN   - liczba bajtów po polu LEN, bez 2 bajtów CRC
//   APDU  - [CLASS] [OS(2 bity) | LEN(6 bitów)] [DATA...]
//   CRC   - CRC-16-CCITT (0x1021, init 0xFFFF, xorout 0xFFFF) liczone od LEN do końca APDU
// ---------------------------------------------------------------------------
static const uint8_t GENI_SD_REQUEST = 0x27;
static const uint8_t GENI_SD_REPLY = 0x24;
static const uint8_t GENI_SD_REPLY_ERROR = 0x25;
static const uint8_t GENI_ADDR_PUMP = 0x20;
static const uint8_t GENI_ADDR_MASTER = 0xF8;
static const size_t GENI_FRAME_OVERHEAD = 4;  // SD + LEN + CRC16

static const uint8_t GENI_OP_GET = 0x00;
static const uint8_t GENI_OP_SET = 0x80;
static const uint8_t GENI_APDU_LEN_MASK = 0x3F;

static const uint8_t GENI_CLASS_ALARM = 0x05;
static const uint8_t GENI_CLASS_ASCII = 0x07;
static const uint8_t GENI_CLASS_GEP = 0x0A;  // Grundfos Extended Protocol (obiekty z typowanymi danymi)

// Klasa 5 / 7: identyfikatory rejestrów
static const uint8_t ALARM_ID_ACTUAL = 0x4B;
static const uint8_t ASCII_ID_PUMP_NAME = 0x11;

// Klasa 10: [SUB_ID] [OBJ_ID_HI] [OBJ_ID_LO]
static const uint8_t GEP_SUB_CONTROL = 0x56;
static const uint8_t GEP_SUB_ELECTRICAL = 0x57;
static const uint8_t GEP_SUB_HYDRAULIC = 0x5D;
static const uint16_t GEP_OBJ_OPERATION = 0x0006;     // stan pracy + tryb regulacji + nastawa [Pa]
static const uint16_t GEP_OBJ_CONTROL_MODE = 0x000A;  // tryb regulacji
static const uint16_t GEP_OBJ_SETPOINT = 0x0010;      // rejestr nastawy [Pa]
static const uint16_t GEP_OBJ_ELECTRICAL = 0x0045;    // napięcie, prąd, moc, RPM, temperatury
static const uint16_t GEP_OBJ_ENERGY = 0x0001;        // licznik energii [J] (typ 232)
static const uint16_t GEP_OBJ_HYDRAULIC = 0x0121;     // przepływ, wysokość podnoszenia, temp. cieczy

// Klasa 10: identyfikatory typów danych (w odpowiedzi i w ramkach zapisu)
static const uint16_t GEP_TYPE_ELECTRICAL = 0x0100;
static const uint16_t GEP_TYPE_OPERATION = 0x012F;  // wspólny dla Obj 6 i Obj 10
static const uint16_t GEP_TYPE_SETPOINT = 0x012E;
static const uint16_t GEP_TYPE_HYDRAULIC = 0x0130;
static const uint16_t GEP_TYPE_ENERGY = 0x00E8;
static const uint8_t GEP_TYPE_VERSION = 0x01;
static const uint8_t GEP_CONTROL_MODE_MARKER = 0x06;  // drugi bajt danych odpowiedzi Obj 10

// Odpowiedź klasy 10: SD LEN DA SA CLASS HDR | 00 TYPE_HI TYPE_LO VER 00 00 DATA_LEN | DATA... | CRC
static const size_t GEP_REPLY_TYPE_OFFSET = 7;
static const size_t GEP_REPLY_DATA_LEN_OFFSET = 12;
static const size_t GEP_REPLY_DATA_OFFSET = 13;

// Parametry transmisji
static const size_t BLE_CHUNK_SIZE = 20;  // oficjalna aplikacja zawsze dzieli zapisy na 20 B
static const uint32_t TX_FRAME_INTERVAL_MS = 150;
static const uint32_t TX_CHUNK_INTERVAL_MS = 15;
static const size_t TX_POLL_QUEUE_MAX = 16;
static const size_t TX_PRIORITY_QUEUE_MAX = 32;
static const uint32_t RX_TIMEOUT_MS = 500;
static const size_t RX_BUFFER_MAX = 512;
static const uint32_t PAIR_RETRY_INTERVAL_MS = 10000;
static const uint32_t ENERGY_POLL_EVERY = 3;
static const uint32_t NAME_POLL_EVERY = 10;

static const float PA_PER_METER = 9806.65f;
static const float SETPOINT_MIN_M = 0.5f;
static const float SETPOINT_MAX_M = 5.0f;

struct ModeName {
  uint8_t code;
  const char *name;
};

// Nazwy muszą być identyczne z opcjami zdefiniowanymi w select.py
static const ModeName OPERATING_MODES[] = {
    {0x00, "Normalny"},
    {0x01, "Stop"},
    {0x02, "Min"},
    {0x03, "Maks"},
};
static const ModeName CONTROL_MODES[] = {
    {0x00, "Ciśnienie stałe"},
    {0x01, "Ciśnienie proporcjonalne"},
    {0x02, "Charakterystyka stała"},
    {0x0D, "Tryb grzejnikowy"},
    {0x0E, "Tryb ogrzewania podłogowego"},
    {0x0F, "Grzejnikowe i podłogowe"},
};
static const uint8_t OP_MODE_NORMAL = 0x00;
static const uint8_t OP_MODE_STOP = 0x01;
static const uint8_t OP_MODE_MAX_CODE = 0x03;

template<size_t N> static const char *find_mode_name(const ModeName (&table)[N], uint8_t code) {
  for (const auto &entry : table) {
    if (entry.code == code)
      return entry.name;
  }
  return nullptr;
}

template<size_t N> static bool find_mode_code(const ModeName (&table)[N], const std::string &name, uint8_t *code) {
  for (const auto &entry : table) {
    if (name == entry.name) {
      *code = entry.code;
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Encje sterujące
// ---------------------------------------------------------------------------
void GrundfosAlpha3PowerSwitch::write_state(bool state) { this->parent_->write_power(state); }

void GrundfosAlpha3OperatingModeSelect::control(const std::string &value) {
  this->parent_->write_operating_mode(value);
}

void GrundfosAlpha3ControlModeSelect::control(const std::string &value) { this->parent_->write_control_mode(value); }

void GrundfosAlpha3SetpointNumber::control(float value) { this->parent_->write_setpoint(value); }

void GrundfosAlpha3PairButton::press_action() { this->parent_->pair_pump(); }

void GrundfosAlpha3UnpairButton::press_action() { this->parent_->unpair_pump(); }

// ---------------------------------------------------------------------------
// Cykl życia komponentu
// ---------------------------------------------------------------------------
void GrundfosAlpha3::dump_config() {
  ESP_LOGCONFIG(TAG, "Grundfos ALPHA3 BLE:");
  ESP_LOGCONFIG(TAG, "  Adres MAC: %s", this->parent()->address_str());
  LOG_UPDATE_INTERVAL(this);
  LOG_BINARY_SENSOR("  ", "Stan sparowania BLE (Paired):", this->pump_paired_sensor_);
  LOG_BINARY_SENSOR("  ", "Pompa włączona (Running):", this->pump_running_sensor_);
  LOG_SENSOR("  ", "Moc (Power):", this->power_sensor_);
  LOG_SENSOR("  ", "Prędkość (Speed):", this->speed_sensor_);
  LOG_SENSOR("  ", "Przepływ (Flow):", this->flow_sensor_);
  LOG_SENSOR("  ", "Wysokość podnoszenia (Head):", this->head_sensor_);
  LOG_SENSOR("  ", "Wartość zadana (Setpoint):", this->current_setpoint_sensor_);
  LOG_SENSOR("  ", "Energia (Energy):", this->energy_sensor_);
  LOG_SENSOR("  ", "Napięcie (Voltage):", this->voltage_sensor_);
  LOG_SENSOR("  ", "Prąd (Current):", this->current_sensor_);
  LOG_SENSOR("  ", "Moc na wale (Shaft Power):", this->shaft_power_sensor_);
  LOG_SENSOR("  ", "Temp. elektroniki:", this->temp_electronics_sensor_);
  LOG_SENSOR("  ", "Temp. silnika:", this->temp_motor_sensor_);
  LOG_SENSOR("  ", "Temp. cieczy:", this->temp_liquid_sensor_);
  LOG_SENSOR("  ", "Kod alarmu:", this->alarm_code_sensor_);
  LOG_TEXT_SENSOR("  ", "Stan pracy (Operating Mode):", this->operating_mode_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Tryb regulacji (Control Mode):", this->control_mode_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Status alarmu:", this->alarm_status_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Nazwa pompy:", this->pump_name_text_sensor_);
}

void GrundfosAlpha3::loop() {
  // Stos Bluedroid startuje w ESP32BLE::loop(), już po setup() tego komponentu.
  // Stan więzi z NVS odczytujemy więc jednorazowo, gdy stos jest gotowy.
  if (!this->bond_state_loaded_) {
    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED)
      return;
    this->bond_state_loaded_ = true;
    this->is_device_bonded();
    this->was_paired_ = this->bonded_;
    if (this->pump_paired_sensor_ != nullptr)
      this->pump_paired_sensor_->publish_state(this->bonded_);
  }

  const uint32_t now = millis();
  const bool encrypted = this->parent()->is_paired();

  if (encrypted != this->was_encrypted_) {
    this->was_encrypted_ = encrypted;
    if (encrypted) {
      ESP_LOGI(TAG, "[%s] Połączenie z pompą zaszyfrowane (sparowano).", this->parent()->address_str());
      // Jeśli powiadomienia są już aktywne, od razu pobierz pełny stan pompy.
      if (this->notify_registered_) {
        this->poll_cycles_ = 0;
        this->update();
      }
    }
  }

  const bool paired = encrypted || this->bonded_;
  if (paired != this->was_paired_) {
    this->was_paired_ = paired;
    if (this->pump_paired_sensor_ != nullptr)
      this->pump_paired_sensor_->publish_state(paired);
  }

  // Połączono, ale sesja nie jest zaszyfrowana: ponawiaj parowanie / wznowienie szyfrowania.
  if (this->connected_ && !encrypted && now - this->last_pair_attempt_ >= PAIR_RETRY_INTERVAL_MS) {
    this->pair_pump();
  }

  // Porzuć niekompletną ramkę, jeśli reszta fragmentów nie dotarła w rozsądnym czasie.
  if (!this->rx_buffer_.empty() && now - this->last_rx_time_ > RX_TIMEOUT_MS) {
    ESP_LOGD(TAG, "Porzucono niekompletną ramkę RX (%u B) po przekroczeniu czasu oczekiwania.",
             (unsigned) this->rx_buffer_.size());
    this->rx_buffer_.clear();
  }

  this->process_tx_(now);
}

void GrundfosAlpha3::update() {
  if (!this->connected_ || !this->notify_registered_ || this->char_handle_ == 0) {
    return;
  }

  // Jeśli pompa nie jest sparowana, nie wysyłaj zapytań telemetrii
  if (!this->parent()->is_paired()) {
    ESP_LOGD(TAG, "Oczekiwanie na sparowanie (kliknięcie przycisku na pompie)...");
    return;
  }

  // Telemetria elektryczna: napięcie, prąd, moc, RPM, temperatury (57 00 45)
  this->queue_command_(build_gep_read_(GEP_SUB_ELECTRICAL, GEP_OBJ_ELECTRICAL));
  // Telemetria hydrauliczna: przepływ, wysokość podnoszenia, temp. cieczy (5D 01 21)
  this->queue_command_(build_gep_read_(GEP_SUB_HYDRAULIC, GEP_OBJ_HYDRAULIC));
  // Stan pracy i nastawa (56 00 06)
  this->queue_command_(build_gep_read_(GEP_SUB_CONTROL, GEP_OBJ_OPERATION));
  // Tryb regulacji (56 00 0A)
  this->queue_command_(build_gep_read_(GEP_SUB_CONTROL, GEP_OBJ_CONTROL_MODE));
  // Alarmy (klasa 5, 4B)
  this->queue_command_(build_read_(GENI_CLASS_ALARM, ALARM_ID_ACTUAL));

  // Licznik energii (57 00 01) - rzadziej
  if (this->poll_cycles_ % ENERGY_POLL_EVERY == 0) {
    this->queue_command_(build_gep_read_(GEP_SUB_ELECTRICAL, GEP_OBJ_ENERGY));
  }
  // Nazwa pompy ASCII (klasa 7, 11) - najrzadziej
  if (this->poll_cycles_ % NAME_POLL_EVERY == 0) {
    this->queue_command_(build_read_(GENI_CLASS_ASCII, ASCII_ID_PUMP_NAME));
  }

  this->poll_cycles_++;
}

// ---------------------------------------------------------------------------
// Parowanie (BLE SMP) i więź (bond) w NVS
// ---------------------------------------------------------------------------
void GrundfosAlpha3::configure_security_() {
  if (this->security_configured_)
    return;

  // Just Works + Secure Connections + trwała więź (BONDING) w NVS ESP-IDF.
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_BOND;
  uint8_t iocap = ESP_IO_CAP_NONE;
  uint8_t key_size = 16;
  uint8_t init_key = ESP_LE_KEY_PENC | ESP_LE_KEY_PID | ESP_LE_KEY_PCSRK | ESP_LE_KEY_PLK;
  uint8_t rsp_key = ESP_LE_KEY_PENC | ESP_LE_KEY_PID | ESP_LE_KEY_PCSRK | ESP_LE_KEY_PLK;

  esp_err_t err = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
  if (err == ESP_OK)
    err = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
  if (err == ESP_OK)
    err = esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
  if (err == ESP_OK)
    err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
  if (err == ESP_OK)
    err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_set_security_param failed: %d", err);
    return;
  }
  this->security_configured_ = true;
}

bool GrundfosAlpha3::is_device_bonded() {
  int dev_num = esp_ble_get_bond_device_num();
  if (dev_num <= 0) {
    this->bonded_ = false;
    return false;
  }
  std::vector<esp_ble_bond_dev_t> dev_list(dev_num);
  bool found = false;
  if (esp_ble_get_bond_device_list(&dev_num, dev_list.data()) == ESP_OK) {
    for (int i = 0; i < dev_num; i++) {
      if (memcmp(dev_list[i].bd_addr, this->parent()->get_remote_bda(), sizeof(esp_bd_addr_t)) == 0) {
        found = true;
        break;
      }
    }
  }
  this->bonded_ = found;
  return found;
}

void GrundfosAlpha3::pair_pump() {
  this->last_pair_attempt_ = millis();
  this->configure_security_();

  if (this->is_device_bonded()) {
    ESP_LOGI(TAG, "[%s] Pompa jest już trwale powiązana (BONDED w NVS ESP32). Wznawiam szyfrowanie sesji...",
             this->parent()->address_str());
  } else {
    ESP_LOGW(TAG, "============================================================");
    ESP_LOGW(TAG, "[%s] PROCEDURA PAROWANIA Z POMPĄ GRUNDFOS ALPHA3!", this->parent()->address_str());
    ESP_LOGW(TAG, ">>> NACIŚNIJ TERAZ PRZYCISK POŁĄCZENIA (RADIA) NA POMPIE! <<<");
    ESP_LOGW(TAG, "Wysyłam żądanie wiązania BLE SMP (BOND)...");
    ESP_LOGW(TAG, "============================================================");
  }

  esp_err_t err = this->parent()->pair();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "[%s] Błąd wywołania pair(): %d", this->parent()->address_str(), err);
  }
}

void GrundfosAlpha3::unpair_pump() {
  ESP_LOGW(TAG, "[%s] Usuwanie zapamiętanego sparowania (unpair / remove bond)...", this->parent()->address_str());
  esp_err_t err = esp_ble_remove_bond_device(this->parent()->get_remote_bda());
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "[%s] Sparowanie usunięte z pamięci NVS ESP32.", this->parent()->address_str());
  } else {
    ESP_LOGE(TAG, "[%s] Błąd usuwania sparowania: %d", this->parent()->address_str(), err);
  }
  this->bonded_ = false;
  this->was_paired_ = false;
  if (this->pump_paired_sensor_ != nullptr) {
    this->pump_paired_sensor_->publish_state(false);
  }
}

void GrundfosAlpha3::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event != ESP_GAP_BLE_AUTH_CMPL_EVT)
    return;
  // BLEClient przekazuje zdarzenia GAP wszystkich urządzeń - filtrujemy po adresie pompy.
  const auto &auth = param->ble_security.auth_cmpl;
  if (memcmp(auth.bd_addr, this->parent()->get_remote_bda(), sizeof(esp_bd_addr_t)) != 0)
    return;

  if (auth.success) {
    // Klucze trafiają do NVS asynchronicznie - w razie potrzeby sprawdź ponownie po chwili.
    if (!this->is_device_bonded()) {
      this->set_timeout("bond_check", 1000, [this]() { this->is_device_bonded(); });
    }
  } else {
    ESP_LOGW(TAG, "[%s] Uwierzytelnienie BLE nieudane (powód: 0x%02X).", this->parent()->address_str(),
             auth.fail_reason);
    if (this->bonded_) {
      ESP_LOGW(TAG, "Jeśli pompa była resetowana lub parowana z innym urządzeniem, użyj przycisku "
                    "'Usuń sparowanie', a następnie sparuj ponownie.");
    }
  }
}

// ---------------------------------------------------------------------------
// Zdarzenia GATT
// ---------------------------------------------------------------------------
void GrundfosAlpha3::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_CONNECT_EVT: {
      // Parametry SMP muszą być ustawione, zanim pompa lub ESP32 rozpocznie parowanie.
      this->configure_security_();
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      // Oficjalny 16-bitowy serwis Grundfos (0xFE5D) lub jego 128-bitowy odpowiednik.
      auto *chr = this->parent()->get_characteristic(GRUNDFOS_SERVICE_UUID, GRUNDFOS_CHAR_UUID);
      if (chr == nullptr) {
        chr = this->parent()->get_characteristic(GRUNDFOS_SERVICE_UUID_128, GRUNDFOS_CHAR_UUID);
      }
      if (chr == nullptr) {
        ESP_LOGE(TAG, "[%s] Nie znaleziono charakterystyki Grundfos BLE (859cffd1-036e-432a-aa28-1a0085b87ba9)",
                 this->parent()->address_str());
        break;
      }
      this->char_handle_ = chr->handle;
      ESP_LOGI(TAG, "[%s] Znaleziono charakterystykę Grundfos (uchwyt 0x%04X). Rejestracja powiadomień...",
               this->parent()->address_str(), this->char_handle_);

      // Jeśli sesja nie jest uwierzytelniona, wywołaj pair_pump() (wznowi lub rozpocznie wiązanie)
      if (!this->parent()->is_paired()) {
        this->pair_pump();
      } else {
        this->is_device_bonded();
      }

      auto status = esp_ble_gattc_register_for_notify(gattc_if, this->parent()->get_remote_bda(), this->char_handle_);
      if (status != ESP_OK) {
        ESP_LOGE(TAG, "[%s] Błąd rejestracji powiadomień: %d", this->parent()->address_str(), status);
      }
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      if (param->reg_for_notify.handle != this->char_handle_)
        break;
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "[%s] Rejestracja powiadomień odrzucona (status: 0x%02X)", this->parent()->address_str(),
                 param->reg_for_notify.status);
        break;
      }

      // Włącz powiadomienia w deskryptorze CCCD (0x2902)
      auto *descr = this->parent()->get_config_descriptor(this->char_handle_);
      if (descr != nullptr) {
        uint16_t notify_en = 1;
        auto status = esp_ble_gattc_write_char_descr(
            this->parent()->get_gattc_if(), this->parent()->get_conn_id(), descr->handle, sizeof(notify_en),
            reinterpret_cast<uint8_t *>(&notify_en), ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
        if (status != ESP_OK) {
          ESP_LOGW(TAG, "[%s] Błąd zapisu CCCD: %d", this->parent()->address_str(), status);
        }
      } else {
        ESP_LOGW(TAG, "[%s] Brak deskryptora CCCD - powiadomienia mogą nie działać.", this->parent()->address_str());
      }

      // Usługi/deskryptory nie są już potrzebne - BLEClient może zwolnić ich pamięć.
      this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
      this->connected_ = true;
      this->notify_registered_ = true;
      ESP_LOGI(TAG, "[%s] Powiadomienia włączone dla pompy Grundfos ALPHA3!", this->parent()->address_str());

      // Gdy sesja jest już zaszyfrowana, pobierz stan od razu (w przeciwnym razie zrobi to loop() po sparowaniu).
      this->poll_cycles_ = 0;
      this->update();
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_handle_)
        break;
      this->handle_rx_bytes_(param->notify.value, param->notify.value_len);
      break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] Błąd zapisu BLE (status: 0x%02X)", this->parent()->address_str(), param->write.status);
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "[%s] Rozłączono z pompą Grundfos ALPHA3.", this->parent()->address_str());
      this->reset_connection_state_();
      break;
    }

    default:
      break;
  }
}

void GrundfosAlpha3::reset_connection_state_() {
  const bool was_connected = this->connected_;
  this->connected_ = false;
  this->notify_registered_ = false;
  this->remote_control_initialized_ = false;
  this->operation_state_valid_ = false;
  this->char_handle_ = 0;
  this->rx_buffer_.clear();
  this->tx_priority_queue_.clear();
  this->tx_poll_queue_.clear();
  this->tx_frame_.clear();
  this->tx_offset_ = 0;

  // Nieudane próby połączenia też kończą się DISCONNECT_EVT - nie publikuj wtedy ponownie tych samych stanów.
  if (!was_connected)
    return;

  // Pomiary na żywo bez połączenia byłyby nieaktualne - w HA przechodzą w stan "nieznany".
  // Licznik energii zachowuje ostatnią wartość (encja total_increasing).
  for (auto *s : {this->power_sensor_, this->speed_sensor_, this->flow_sensor_, this->head_sensor_,
                  this->voltage_sensor_, this->current_sensor_, this->shaft_power_sensor_,
                  this->temp_electronics_sensor_, this->temp_motor_sensor_, this->temp_liquid_sensor_}) {
    if (s != nullptr)
      s->publish_state(NAN);
  }
  if (this->pump_running_sensor_ != nullptr) {
    this->pump_running_sensor_->publish_state(false);
  }
}

// ---------------------------------------------------------------------------
// Nadawanie: kolejka + nieblokujące dzielenie ramek na fragmenty BLE
// ---------------------------------------------------------------------------
void GrundfosAlpha3::queue_command_(std::vector<uint8_t> frame, bool high_priority) {
  if (high_priority) {
    if (this->tx_priority_queue_.size() >= TX_PRIORITY_QUEUE_MAX) {
      ESP_LOGW(TAG, "Kolejka poleceń sterujących pełna - pomijam polecenie.");
      return;
    }
    this->tx_priority_queue_.push_back(std::move(frame));
  } else {
    // Zapobiegaj przepełnieniu kolejki pollingu
    if (this->tx_poll_queue_.size() >= TX_POLL_QUEUE_MAX)
      return;
    this->tx_poll_queue_.push_back(std::move(frame));
  }
}

void GrundfosAlpha3::process_tx_(uint32_t now) {
  // Trwa wysyłanie ramki dłuższej niż jeden fragment BLE
  if (!this->tx_frame_.empty()) {
    if (now - this->last_tx_chunk_time_ >= TX_CHUNK_INTERVAL_MS) {
      this->send_chunk_(now);
    }
    return;
  }

  if (now - this->last_tx_frame_time_ < TX_FRAME_INTERVAL_MS)
    return;

  // Polecenia sterujące mają pierwszeństwo przed odpytywaniem (kolejność FIFO w obrębie kolejki)
  std::deque<std::vector<uint8_t>> *queue = nullptr;
  if (!this->tx_priority_queue_.empty()) {
    queue = &this->tx_priority_queue_;
  } else if (!this->tx_poll_queue_.empty()) {
    queue = &this->tx_poll_queue_;
  } else {
    return;
  }

  this->tx_frame_ = std::move(queue->front());
  queue->pop_front();
  this->tx_offset_ = 0;
  this->last_tx_frame_time_ = now;
  this->send_chunk_(now);
}

bool GrundfosAlpha3::send_chunk_(uint32_t now) {
  if (!this->connected_ || this->char_handle_ == 0 || this->tx_offset_ >= this->tx_frame_.size()) {
    this->tx_frame_.clear();
    this->tx_offset_ = 0;
    return false;
  }

  const size_t chunk_len = std::min(BLE_CHUNK_SIZE, this->tx_frame_.size() - this->tx_offset_);
  esp_err_t err = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                           this->char_handle_, chunk_len, this->tx_frame_.data() + this->tx_offset_,
                                           ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "[%s] Błąd wysyłania fragmentu BLE (offset %u, len %u): %d", this->parent()->address_str(),
             (unsigned) this->tx_offset_, (unsigned) chunk_len, err);
    this->tx_frame_.clear();
    this->tx_offset_ = 0;
    return false;
  }

  this->tx_offset_ += chunk_len;
  this->last_tx_chunk_time_ = now;
  if (this->tx_offset_ >= this->tx_frame_.size()) {
    this->tx_frame_.clear();
    this->tx_offset_ = 0;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Odbiór: reasemblacja fragmentów BLE i weryfikacja ramek
// ---------------------------------------------------------------------------
void GrundfosAlpha3::handle_rx_bytes_(const uint8_t *data, size_t length) {
  if (data == nullptr || length == 0)
    return;

  if (this->rx_buffer_.size() + length > RX_BUFFER_MAX) {
    ESP_LOGW(TAG, "Przepełnienie bufora RX - czyszczenie.");
    this->rx_buffer_.clear();
  }
  this->rx_buffer_.insert(this->rx_buffer_.end(), data, data + length);
  this->last_rx_time_ = millis();

  while (!this->rx_buffer_.empty()) {
    // Pomiń bajty poprzedzające początek ramki (0x24 = odpowiedź, 0x25 = błąd)
    auto start = std::find_if(this->rx_buffer_.begin(), this->rx_buffer_.end(),
                              [](uint8_t b) { return b == GENI_SD_REPLY || b == GENI_SD_REPLY_ERROR; });
    if (start != this->rx_buffer_.begin()) {
      this->rx_buffer_.erase(this->rx_buffer_.begin(), start);
      continue;
    }

    if (this->rx_buffer_.size() < 2)
      break;  // Potrzeba więcej bajtów na pole LEN

    const uint8_t declared_len = this->rx_buffer_[1];
    if (declared_len < 2) {
      // Ramka musi zawierać co najmniej DA i SA - to nie jest początek ramki
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    const size_t full_frame_len = declared_len + GENI_FRAME_OVERHEAD;
    if (this->rx_buffer_.size() < full_frame_len)
      break;  // Czekamy na kolejne fragmenty BLE (limit czasu pilnuje loop())

    // CRC-16 liczone od LEN do końca danych (bez bajtu SD i bez 2 bajtów CRC)
    const uint16_t computed_crc = calculate_crc16_(this->rx_buffer_.data() + 1, full_frame_len - 3);
    const uint16_t received_crc = (static_cast<uint16_t>(this->rx_buffer_[full_frame_len - 2]) << 8) |
                                  this->rx_buffer_[full_frame_len - 1];
    if (computed_crc != received_crc) {
      ESP_LOGW(TAG, "Odrzucono ramkę z błędną sumą CRC: obliczona 0x%04X != odebrana 0x%04X", computed_crc,
               received_crc);
      // Odrzuć tylko bajt startu - kolejna poprawna ramka może zaczynać się wewnątrz odrzuconych danych
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    std::vector<uint8_t> frame(this->rx_buffer_.begin(), this->rx_buffer_.begin() + full_frame_len);
    this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + full_frame_len);
    this->process_geni_frame_(frame);
  }
}

void GrundfosAlpha3::process_geni_frame_(const std::vector<uint8_t> &frame) {
  if (frame[0] == GENI_SD_REPLY_ERROR) {
    ESP_LOGD(TAG, "Pompa zgłosiła błąd przetwarzania zapytania (ramka 0x25).");
    return;
  }
  if (frame.size() < 8)
    return;

  const uint8_t apdu_class = frame[4];
  const size_t apdu_len = frame[5] & GENI_APDU_LEN_MASK;
  const size_t data_end = frame.size() - 2;  // bez CRC

  // Klasa 5: Alarmy i ostrzeżenia
  if (apdu_class == GENI_CLASS_ALARM) {
    const uint8_t alarm_code = frame[6];
    if (this->alarm_code_sensor_ != nullptr) {
      this->alarm_code_sensor_->publish_state(alarm_code);
    }
    if (this->alarm_status_text_sensor_ != nullptr) {
      this->alarm_status_text_sensor_->publish_state(decode_alarm_code_(alarm_code));
    }
    return;
  }

  // Klasa 7: Nazwa urządzenia (ASCII zakończone zerem)
  if (apdu_class == GENI_CLASS_ASCII) {
    std::string name_str;
    for (size_t i = 6; i < 6 + apdu_len && i < data_end; i++) {
      if (frame[i] == 0)
        break;
      name_str += static_cast<char>(frame[i]);
    }
    if (!name_str.empty() && this->pump_name_text_sensor_ != nullptr) {
      this->pump_name_text_sensor_->publish_state(name_str);
    }
    return;
  }

  // Klasa 10: Parametry rozszerzone Grundfos
  if (apdu_class == GENI_CLASS_GEP) {
    this->process_gep_reply_(frame);
  }
}

void GrundfosAlpha3::process_gep_reply_(const std::vector<uint8_t> &frame) {
  if (frame.size() < GEP_REPLY_DATA_OFFSET + 2)
    return;

  const uint16_t type_id =
      (static_cast<uint16_t>(frame[GEP_REPLY_TYPE_OFFSET]) << 8) | frame[GEP_REPLY_TYPE_OFFSET + 1];
  const size_t data_len = frame[GEP_REPLY_DATA_LEN_OFFSET];
  if (GEP_REPLY_DATA_OFFSET + data_len > frame.size() - 2) {
    ESP_LOGW(TAG, "Niespójna długość danych w odpowiedzi klasy 10 (typ 0x%04X)", type_id);
    return;
  }
  const uint8_t *data = &frame[GEP_REPLY_DATA_OFFSET];

  switch (type_id) {
    // Odpowiedź na 57 00 45 (Telemetria elektryczna)
    case GEP_TYPE_ELECTRICAL: {
      if (data_len < 37)
        break;
      const float mains_voltage = parse_float_be_(&data[0]);
      const float motor_current = parse_float_be_(&data[8]);
      const float active_power = parse_float_be_(&data[12]);
      const float shaft_power = parse_float_be_(&data[16]);
      const float speed_rpm = parse_float_be_(&data[20]);
      const float temp_electronics = parse_float_be_(&data[28]);
      const float temp_motor = parse_float_be_(&data[32]);
      const bool running = data[36] == 0x01;

      const std::pair<sensor::Sensor *, float> values[] = {
          {this->voltage_sensor_, mains_voltage},        {this->current_sensor_, motor_current},
          {this->power_sensor_, active_power},           {this->shaft_power_sensor_, shaft_power},
          {this->speed_sensor_, speed_rpm},              {this->temp_electronics_sensor_, temp_electronics},
          {this->temp_motor_sensor_, temp_motor},
      };
      for (const auto &v : values) {
        if (v.first != nullptr && !std::isnan(v.second))
          v.first->publish_state(v.second);
      }
      if (this->pump_running_sensor_ != nullptr) {
        this->pump_running_sensor_->publish_state(running);
      }
      break;
    }

    // Odpowiedź na 5D 01 21 (Telemetria hydrauliczna)
    case GEP_TYPE_HYDRAULIC: {
      if (data_len < 24)
        break;
      const float flow_m3s = parse_float_be_(&data[0]);
      const float head_pa = parse_float_be_(&data[4]);
      const float temp_liquid = parse_float_be_(&data[16]);

      if (!std::isnan(flow_m3s) && this->flow_sensor_ != nullptr) {
        this->flow_sensor_->publish_state(flow_m3s * 3600.0f);  // m³/s -> m³/h
      }
      if (!std::isnan(head_pa) && this->head_sensor_ != nullptr) {
        this->head_sensor_->publish_state(head_pa / PA_PER_METER);  // Pa -> m sł. wody
      }
      if (!std::isnan(temp_liquid) && this->temp_liquid_sensor_ != nullptr) {
        this->temp_liquid_sensor_->publish_state(temp_liquid);
      }
      break;
    }

    // Odpowiedzi na 56 00 06 i 56 00 0A (ten sam typ danych, rozróżniane drugim bajtem)
    //   Obj 6:  00 [OP_MODE] [CTRL_MODE] [FLOAT_SETPOINT_PA]
    //   Obj 10: 00 06 [CTRL_MODE] 7F FF FF FF
    case GEP_TYPE_OPERATION: {
      if (data_len < 7 || data[0] != 0x00)
        break;

      if (data[1] == GEP_CONTROL_MODE_MARKER) {
        this->current_ctrl_mode_ = data[2];
        this->publish_control_mode_(data[2]);
        break;
      }
      if (data[1] > OP_MODE_MAX_CODE)
        break;

      this->current_op_mode_ = data[1];
      this->current_ctrl_mode_ = data[2];
      this->publish_operating_mode_(data[1]);
      this->publish_control_mode_(data[2]);

      const float setpoint_pa = parse_float_be_(&data[3]);
      if (!std::isnan(setpoint_pa) && setpoint_pa > 0.0f) {
        this->current_setpoint_m_ = setpoint_pa / PA_PER_METER;
        this->publish_setpoint_(this->current_setpoint_m_);
      }
      this->operation_state_valid_ = true;

      // Automatyczna aktywacja sterowania magistralą (Remote Control Mode):
      // po połączeniu pompa pracuje autonomicznie; zapis Obj 6 z bieżącym stanem przejmuje sterowanie zdalne.
      if (!this->remote_control_initialized_) {
        this->remote_control_initialized_ = true;
        ESP_LOGI(TAG, "Inicjalizacja autoryzacji sterowania magistralą GENI pompy ALPHA3 (Object 6)...");
        this->send_operating_mode_(this->current_op_mode_);
      }
      break;
    }

    // Odpowiedź na 57 00 01 (Energia całkowita, double [J])
    case GEP_TYPE_ENERGY: {
      if (data_len < 8)
        break;
      const double energy_joules = parse_double_be_(&data[0]);
      if (!std::isnan(energy_joules) && energy_joules > 0.0 && this->energy_sensor_ != nullptr) {
        this->energy_sensor_->publish_state(static_cast<float>(energy_joules / 3600000.0));  // J -> kWh
      }
      break;
    }

    default:
      ESP_LOGV(TAG, "Nieobsługiwany typ odpowiedzi klasy 10: 0x%04X", type_id);
      break;
  }
}

// ---------------------------------------------------------------------------
// Sterowanie
// ---------------------------------------------------------------------------
bool GrundfosAlpha3::can_write_(const char *action) {
  if (!this->connected_ || !this->notify_registered_ || this->char_handle_ == 0 || !this->parent()->is_paired()) {
    ESP_LOGW(TAG, "%s: brak zaszyfrowanego połączenia z pompą - polecenie pominięte.", action);
    return false;
  }
  if (!this->operation_state_valid_) {
    ESP_LOGW(TAG, "%s: bieżący stan pompy nie został jeszcze odczytany - polecenie pominięte.", action);
    return false;
  }
  return true;
}

void GrundfosAlpha3::write_power(bool state) {
  ESP_LOGI(TAG, "Zmieniono przełącznik zasilania pompy: %s", state ? "ON (Normal)" : "OFF (Stop)");
  if (!this->can_write_("Przełącznik zasilania"))
    return;
  this->send_operating_mode_(state ? OP_MODE_NORMAL : OP_MODE_STOP);
}

void GrundfosAlpha3::write_operating_mode(const std::string &mode_str) {
  uint8_t op_mode;
  if (!find_mode_code(OPERATING_MODES, mode_str, &op_mode)) {
    ESP_LOGW(TAG, "Nieznany stan pracy: '%s'", mode_str.c_str());
    return;
  }
  if (!this->can_write_("Stan pracy"))
    return;
  this->send_operating_mode_(op_mode);
}

void GrundfosAlpha3::write_control_mode(const std::string &mode_str) {
  uint8_t ctrl_mode;
  if (!find_mode_code(CONTROL_MODES, mode_str, &ctrl_mode)) {
    ESP_LOGW(TAG, "Nieznany tryb regulacji: '%s'", mode_str.c_str());
    return;
  }
  if (!this->can_write_("Tryb regulacji"))
    return;
  this->send_control_mode_(ctrl_mode);
}

void GrundfosAlpha3::write_setpoint(float setpoint_m) {
  if (std::isnan(setpoint_m))
    return;
  if (!this->can_write_("Nastawa"))
    return;

  setpoint_m = std::clamp(setpoint_m, SETPOINT_MIN_M, SETPOINT_MAX_M);
  this->current_setpoint_m_ = setpoint_m;
  const float setpoint_pa = setpoint_m * PA_PER_METER;
  ESP_LOGI(TAG, "Wysyłanie nowej wartości zadanej: %.1f m (%.0f Pa)", setpoint_m, setpoint_pa);
  this->publish_setpoint_(setpoint_m);

  // Kolejność na magistrali (sprawdzona na pompie): najpierw Obj 6 z nową nastawą - to ta ramka
  // bezpośrednio zmienia punkt pracy ALPHA3 w bieżącym trybie - a potem rejestr nastawy Obj 16.
  this->queue_command_(this->build_operation_frame_(setpoint_pa), true);

  // Obj 16 (18 B): 00 04 [FLOAT_SETPOINT_PA] 3F000000 41000000 00000000
  std::vector<uint8_t> payload = {0x00, 0x04};
  append_float_be_(payload, setpoint_pa);
  payload.insert(payload.end(), {0x3F, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  this->queue_command_(build_gep_write_(GEP_SUB_CONTROL, GEP_OBJ_SETPOINT, GEP_TYPE_SETPOINT, payload), true);
}

void GrundfosAlpha3::send_operating_mode_(uint8_t op_mode) {
  this->current_op_mode_ = op_mode;
  ESP_LOGI(TAG, "Wysyłanie nowego stanu pracy: %s (kod 0x%02X)", decode_op_mode_(op_mode).c_str(), op_mode);
  this->publish_operating_mode_(op_mode);
  this->queue_command_(this->build_operation_frame_(this->current_setpoint_m_ * PA_PER_METER), true);
}

void GrundfosAlpha3::send_control_mode_(uint8_t ctrl_mode) {
  this->current_ctrl_mode_ = ctrl_mode;
  ESP_LOGI(TAG, "Wysyłanie nowego trybu regulacji: %s (kod 0x%02X)", decode_ctrl_mode_(ctrl_mode).c_str(), ctrl_mode);
  this->publish_control_mode_(ctrl_mode);

  // Kolejność na magistrali (sprawdzona na pompie): Obj 6 (niesie nowy kod trybu), potem Obj 10.
  this->send_operating_mode_(this->current_op_mode_);
  const std::vector<uint8_t> payload = {0x00, GEP_CONTROL_MODE_MARKER, ctrl_mode, 0x7F, 0xFF, 0xFF, 0xFF};
  this->queue_command_(build_gep_write_(GEP_SUB_CONTROL, GEP_OBJ_CONTROL_MODE, GEP_TYPE_OPERATION, payload), true);

  // Odczyt weryfikujący tryb regulacji
  this->queue_command_(build_gep_read_(GEP_SUB_CONTROL, GEP_OBJ_CONTROL_MODE));
}

std::vector<uint8_t> GrundfosAlpha3::build_operation_frame_(float setpoint_pa) const {
  // Obj 6: 00 [OP_MODE] [CTRL_MODE] [FLOAT_SETPOINT_PA]
  std::vector<uint8_t> payload = {0x00, this->current_op_mode_, this->current_ctrl_mode_};
  append_float_be_(payload, setpoint_pa);
  return build_gep_write_(GEP_SUB_CONTROL, GEP_OBJ_OPERATION, GEP_TYPE_OPERATION, payload);
}

void GrundfosAlpha3::publish_operating_mode_(uint8_t op_mode) {
  const std::string mode_str = decode_op_mode_(op_mode);
  if (this->operating_mode_text_sensor_ != nullptr) {
    this->operating_mode_text_sensor_->publish_state(mode_str);
  }
  if (this->operating_mode_select_ != nullptr && find_mode_name(OPERATING_MODES, op_mode) != nullptr) {
    this->operating_mode_select_->publish_state(mode_str);
  }
  if (this->power_switch_ != nullptr) {
    this->power_switch_->publish_state(op_mode == OP_MODE_NORMAL);
  }
}

void GrundfosAlpha3::publish_control_mode_(uint8_t ctrl_mode) {
  const std::string ctrl_str = decode_ctrl_mode_(ctrl_mode);
  if (this->control_mode_text_sensor_ != nullptr) {
    this->control_mode_text_sensor_->publish_state(ctrl_str);
  }
  // Select przyjmuje tylko zdefiniowane opcje - nieznany kod trafia wyłącznie do sensora tekstowego
  if (this->control_mode_select_ != nullptr && find_mode_name(CONTROL_MODES, ctrl_mode) != nullptr) {
    this->control_mode_select_->publish_state(ctrl_str);
  }
}

void GrundfosAlpha3::publish_setpoint_(float setpoint_m) {
  if (this->current_setpoint_sensor_ != nullptr) {
    this->current_setpoint_sensor_->publish_state(setpoint_m);
  }
  if (this->setpoint_number_ != nullptr) {
    this->setpoint_number_->publish_state(roundf(setpoint_m * 10.0f) / 10.0f);
  }
}

// ---------------------------------------------------------------------------
// Budowanie ramek GENI
// ---------------------------------------------------------------------------
std::vector<uint8_t> GrundfosAlpha3::build_frame_(uint8_t apdu_class, uint8_t op, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> frame;
  frame.reserve(data.size() + 8);
  frame.push_back(GENI_SD_REQUEST);
  frame.push_back(static_cast<uint8_t>(data.size() + 4));  // DA + SA + CLASS + OS/LEN + dane
  frame.push_back(GENI_ADDR_PUMP);
  frame.push_back(GENI_ADDR_MASTER);
  frame.push_back(apdu_class);
  frame.push_back(op | static_cast<uint8_t>(data.size() & GENI_APDU_LEN_MASK));
  frame.insert(frame.end(), data.begin(), data.end());

  const uint16_t crc = calculate_crc16_(frame.data() + 1, frame.size() - 1);
  frame.push_back(static_cast<uint8_t>(crc >> 8));
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  return frame;
}

std::vector<uint8_t> GrundfosAlpha3::build_read_(uint8_t apdu_class, uint8_t id) {
  return build_frame_(apdu_class, GENI_OP_GET, {id});
}

std::vector<uint8_t> GrundfosAlpha3::build_gep_read_(uint8_t sub_id, uint16_t obj_id) {
  return build_frame_(GENI_CLASS_GEP, GENI_OP_GET,
                      {sub_id, static_cast<uint8_t>(obj_id >> 8), static_cast<uint8_t>(obj_id & 0xFF)});
}

std::vector<uint8_t> GrundfosAlpha3::build_gep_write_(uint8_t sub_id, uint16_t obj_id, uint16_t type_id,
                                                      const std::vector<uint8_t> &payload) {
  // [SUB] [OBJ_HI] [OBJ_LO] [TYPE_HI] [TYPE_LO] [VER] 00 00 [LEN] [PAYLOAD...]
  std::vector<uint8_t> data = {sub_id,
                               static_cast<uint8_t>(obj_id >> 8),
                               static_cast<uint8_t>(obj_id & 0xFF),
                               static_cast<uint8_t>(type_id >> 8),
                               static_cast<uint8_t>(type_id & 0xFF),
                               GEP_TYPE_VERSION,
                               0x00,
                               0x00,
                               static_cast<uint8_t>(payload.size())};
  data.insert(data.end(), payload.begin(), payload.end());
  return build_frame_(GENI_CLASS_GEP, GENI_OP_SET, data);
}

void GrundfosAlpha3::append_float_be_(std::vector<uint8_t> &out, float value) {
  uint32_t raw;
  memcpy(&raw, &value, sizeof(raw));
  out.push_back(static_cast<uint8_t>(raw >> 24));
  out.push_back(static_cast<uint8_t>(raw >> 16));
  out.push_back(static_cast<uint8_t>(raw >> 8));
  out.push_back(static_cast<uint8_t>(raw));
}

// ---------------------------------------------------------------------------
// Narzędzia
// ---------------------------------------------------------------------------
uint16_t GrundfosAlpha3::calculate_crc16_(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= (static_cast<uint16_t>(data[i]) << 8);
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc ^ 0xFFFF;
}

float GrundfosAlpha3::parse_float_be_(const uint8_t *data) {
  const uint32_t val = (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
                       (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
  // 0x7FFFFFFF = wartość niedostępna, 0xFFC00000 = NaN
  if (val == 0x7FFFFFFF || val == 0xFFC00000) {
    return NAN;
  }
  float f;
  memcpy(&f, &val, sizeof(f));
  return f;
}

double GrundfosAlpha3::parse_double_be_(const uint8_t *data) {
  uint64_t val = 0;
  for (int i = 0; i < 8; i++) {
    val = (val << 8) | data[i];
  }
  double d;
  memcpy(&d, &val, sizeof(d));
  return d;
}

std::string GrundfosAlpha3::decode_ctrl_mode_(uint8_t code) {
  const char *name = find_mode_name(CONTROL_MODES, code);
  if (name != nullptr)
    return name;
  char buf[16];
  snprintf(buf, sizeof(buf), "Tryb 0x%02X", code);
  return buf;
}

std::string GrundfosAlpha3::decode_op_mode_(uint8_t code) {
  const char *name = find_mode_name(OPERATING_MODES, code);
  if (name != nullptr)
    return name;
  char buf[16];
  snprintf(buf, sizeof(buf), "Stan 0x%02X", code);
  return buf;
}

std::string GrundfosAlpha3::decode_alarm_code_(uint8_t code) {
  switch (code) {
    case 0x00:
      return "Brak błędu (Normalna praca)";
    case 0x0A:
      return "F01 - Suchobieg (Dry running)";
    case 0x1E:
      return "F02 - Zablokowany wirnik (Rotor blocked)";
    case 0x28:
      return "F03 - Przeciążenie silnika (Motor overload)";
    case 0x38:
      return "F04 - Przegrzanie modułu/silnika (Overheat)";
    case 0x48:
      return "F05 - Błąd czujnika wewnętrznego (Sensor fault)";
    case 0x58:
      return "F06 - Błąd komunikacji (Communication fault)";
    case 0x60:
      return "F07 - Niskie napięcie zasilania (Undervoltage)";
    case 0x64:
      return "F08 - Przepięcie w sieci (Overvoltage)";
    default: {
      char buf[32];
      snprintf(buf, sizeof(buf), "Błąd 0x%02X (%d)", code, code);
      return buf;
    }
  }
}

}  // namespace esphome::grundfos_alpha3

#endif  // USE_ESP32
