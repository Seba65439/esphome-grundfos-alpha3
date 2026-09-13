#include "grundfos_alpha3.h"
#include "esphome/core/log.h"
#include <esp_gap_ble_api.h>
#include <cstring>
#include <cmath>

namespace esphome {
namespace grundfos_alpha3 {

static const char *const TAG = "grundfos_alpha3";

static const esp32_ble_tracker::ESPBTUUID GRUNDFOS_SERVICE_UUID =
    esp32_ble_tracker::ESPBTUUID::from_uint16(0xFE5D);
static const esp32_ble_tracker::ESPBTUUID GRUNDFOS_CHAR_UUID =
    esp32_ble_tracker::ESPBTUUID::from_raw("859cffd1-036e-432a-aa28-1a0085b87ba9");

void GrundfosAlpha3PowerSwitch::write_state(bool state) {
  if (this->parent_ != nullptr) {
    this->parent_->write_power(state);
  }
}

void GrundfosAlpha3OperatingModeSelect::control(const std::string &value) {
  if (this->parent_ != nullptr) {
    this->parent_->write_operating_mode(value);
  }
}

void GrundfosAlpha3ControlModeSelect::control(const std::string &value) {
  if (this->parent_ != nullptr) {
    this->parent_->write_control_mode(value);
  }
}

void GrundfosAlpha3SetpointNumber::control(float value) {
  if (this->parent_ != nullptr) {
    this->parent_->write_setpoint(value);
  }
}

void GrundfosAlpha3PairButton::press_action() {
  if (this->parent_ != nullptr) {
    this->parent_->pair_pump();
  }
}

void GrundfosAlpha3UnpairButton::press_action() {
  if (this->parent_ != nullptr) {
    this->parent_->unpair_pump();
  }
}

GrundfosAlpha3::GrundfosAlpha3() : PollingComponent(10000) {}

void GrundfosAlpha3::setup() {
  ESP_LOGCONFIG(TAG, "Inicjalizacja komponentu Grundfos ALPHA3 BLE...");

  // Konfiguracja trwałego powiązania BLE SMP (BONDING) w pamięci NVS ESP-IDF:
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_BOND;
  esp_err_t ret = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_set_security_param(AUTHEN_REQ_MODE) failed: %d", ret);
  }

  uint8_t iocap = ESP_IO_CAP_NONE;
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));

  uint8_t key_size = 16;
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));

  uint8_t init_key = ESP_LE_KEY_PENC | ESP_LE_KEY_PID | ESP_LE_KEY_PCSRK | ESP_LE_KEY_PLK;
  uint8_t rsp_key = ESP_LE_KEY_PENC | ESP_LE_KEY_PID | ESP_LE_KEY_PCSRK | ESP_LE_KEY_PLK;
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

  if (this->setpoint_number_ != nullptr) {
    this->setpoint_number_->publish_state(1.5f);
  }
  if (this->current_setpoint_sensor_ != nullptr) {
    this->current_setpoint_sensor_->publish_state(1.5f);
  }
  if (this->operating_mode_select_ != nullptr) {
    this->operating_mode_select_->publish_state("Normalny");
  }
  if (this->operating_mode_text_sensor_ != nullptr) {
    this->operating_mode_text_sensor_->publish_state("Normalny");
  }
  if (this->control_mode_select_ != nullptr) {
    this->control_mode_select_->publish_state("Ciśnienie stałe");
  }
  if (this->control_mode_text_sensor_ != nullptr) {
    this->control_mode_text_sensor_->publish_state("Ciśnienie stałe");
  }
  if (this->power_switch_ != nullptr) {
    this->power_switch_->publish_state(true);
  }
}

void GrundfosAlpha3::dump_config() {
  ESP_LOGCONFIG(TAG, "Grundfos ALPHA3 BLE:");
  ESP_LOGCONFIG(TAG, "  Adres MAC: %s", this->parent()->address_str());
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

bool GrundfosAlpha3::is_device_bonded() {
  if (this->parent() == nullptr) return false;
  int dev_num = esp_ble_get_bond_device_num();
  if (dev_num <= 0) {
    this->is_bonded_ = false;
    return false;
  }
  std::vector<esp_ble_bond_dev_t> dev_list(dev_num);
  if (esp_ble_get_bond_device_list(&dev_num, dev_list.data()) == ESP_OK) {
    for (int i = 0; i < dev_num; i++) {
      if (memcmp(dev_list[i].bd_addr, this->parent()->get_remote_bda(), sizeof(esp_bd_addr_t)) == 0) {
        this->is_bonded_ = true;
        return true;
      }
    }
  }
  this->is_bonded_ = false;
  return false;
}

void GrundfosAlpha3::pair_pump() {
  uint32_t now = millis();
  this->last_pair_attempt_ = now;

  // Upewnij się, że parametry bezpieczeństwa SMP są zawsze ustawione
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_BOND;
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));

  uint8_t iocap = ESP_IO_CAP_NONE;
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));

  uint8_t key_size = 16;
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));

  uint8_t init_key = ESP_LE_KEY_PENC | ESP_LE_KEY_PID | ESP_LE_KEY_PCSRK | ESP_LE_KEY_PLK;
  uint8_t rsp_key = ESP_LE_KEY_PENC | ESP_LE_KEY_PID | ESP_LE_KEY_PCSRK | ESP_LE_KEY_PLK;
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

  bool bonded = this->is_device_bonded();
  if (bonded) {
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
  this->is_bonded_ = false;
  this->was_paired_ = false;
  if (this->pump_paired_sensor_ != nullptr) {
    this->pump_paired_sensor_->publish_state(false);
  }
}

void GrundfosAlpha3::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                         esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      esp32_ble_client::BLECharacteristic *chr = nullptr;

      // 1. Sprawdź oficjalny serwis Grundfos GENI (0xFE5D)
      chr = this->parent()->get_characteristic(GRUNDFOS_SERVICE_UUID, GRUNDFOS_CHAR_UUID);

      // 2. Jeśli nie odnaleziono, sprawdź alternatywny 128-bitowy UUID serwisu
      if (chr == nullptr) {
        static const auto ALT_SERVICE_UUID =
            esp32_ble_tracker::ESPBTUUID::from_raw("859cffd0-036e-432a-aa28-1a0085b87ba9");
        chr = this->parent()->get_characteristic(ALT_SERVICE_UUID, GRUNDFOS_CHAR_UUID);
      }

      if (chr == nullptr) {
        ESP_LOGE(TAG, "[%s] Nie znaleziono charakterystyki Grundfos BLE (859cffd1-036e-432a-aa28-1a0085b87ba9)",
                 this->parent()->address_str());
        break;
      }
      this->char_handle_ = chr->handle;
      ESP_LOGI(TAG, "[%s] Znaleziono charakterystykę Grundfos (uchwyt 0x%04X). Rejestracja powiadomień...",
               this->parent()->address_str(), this->char_handle_);

      // Sprawdź czy urządzenie jest już w pamięci NVS (BONDED)
      this->is_device_bonded();

      // Jeśli sesja nie jest uwierzytelniona, wywołaj pair_pump() (wznowi lub rozpocznie wiązanie)
      if (!this->parent()->is_paired()) {
        this->pair_pump();
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

      this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
      this->connected_ = true;
      this->notify_registered_ = true;
      ESP_LOGI(TAG, "[%s] Powiadomienia włączone dla pompy Grundfos ALPHA3!", this->parent()->address_str());

      // Włącz CCCD (0x2902)
      auto *descr = this->parent()->get_config_descriptor(this->char_handle_);
      if (descr != nullptr) {
        this->cccd_handle_ = descr->handle;
        uint16_t notify_en = 1;
        esp_ble_gattc_write_char_descr(
            this->parent()->get_gattc_if(),
            this->parent()->get_conn_id(),
            this->cccd_handle_,
            sizeof(notify_en),
            reinterpret_cast<uint8_t *>(&notify_en),
            ESP_GATT_WRITE_TYPE_RSP,
            ESP_GATT_AUTH_REQ_NONE);
      }

      // Jeśli sesja jest uwierzytelniona LUB urządzenie jest trwale powiązane (BONDED w NVS), rozpocznij polling
      if (this->parent()->is_paired() || this->is_bonded_) {
        this->poll_step_ = 0;
        this->poll_cycles_ = 0;
        this->update();
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle != this->char_handle_)
        break;
      this->handle_rx_bytes(param->notify.value, param->notify.value_len);
      break;
    }

    case ESP_GATTC_WRITE_CHAR_EVT: {
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "[%s] Błąd zapisu BLE (status: 0x%02X)",
                 this->parent()->address_str(), param->write.status);
      }
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "[%s] Rozłączono z pompą Grundfos ALPHA3.", this->parent()->address_str());
      this->connected_ = false;
      this->notify_registered_ = false;
      this->remote_control_initialized_ = false;
      this->char_handle_ = 0;
      this->cccd_handle_ = 0;
      this->rx_buffer_.clear();
      this->tx_queue_.clear();
      if (this->pump_running_sensor_ != nullptr) {
        this->pump_running_sensor_->publish_state(false);
      }
      break;
    }

    default:
      break;
  }
}

void GrundfosAlpha3::loop() {
  uint32_t now = millis();

  // Sprawdzaj stan powiązania NVS co 3 sekundy (zamiast w każdym obrocie pętli loop, co dusiło procesor i WiFi)
  if (now - this->last_bond_check_ >= 3000) {
    this->last_bond_check_ = now;
    this->is_device_bonded();
  }

  bool is_paired = this->parent()->is_paired() || this->is_bonded_;
  if (is_paired != this->was_paired_) {
    this->was_paired_ = is_paired;
    if (this->pump_paired_sensor_ != nullptr) {
      this->pump_paired_sensor_->publish_state(is_paired);
    }
    if (is_paired) {
      ESP_LOGI(TAG, "============================================================");
      ESP_LOGI(TAG, "[%s] SUKCES! Pompa Grundfos ALPHA3 powiązana i połączenie zaszyfrowane!",
               this->parent()->address_str());
      ESP_LOGI(TAG, "============================================================");
      // Pobierz początkowy stan parametrów
      this->poll_step_ = 0;
      this->poll_cycles_ = 0;
      this->update();
    }
  }

  // Jeśli połączono z BLE, ale urządzenie nie jest jeszcze powiązane w NVS:
  if (this->connected_ && !this->parent()->is_paired() && !this->is_bonded_) {
    if (now - this->last_pair_attempt_ >= 10000) {
      this->pair_pump();
    }
  }

  // Przesyłaj kolejne polecenia z kolejki w odstępach min 150 ms
  if (!this->tx_queue_.empty() && (now - this->last_tx_time_ >= 150)) {
    this->send_next_queued_command();
    this->last_tx_time_ = now;
  }
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

  // Polling sekwencyjny w standardowym cyklu update()
  // 1. Telemetria elektryczna: Moc, RPM, Napięcie, Prąd, Temp (57 00 45)
  // TX: 27 07 20 F8 0A 03 57 00 45 E5 9C
  static const uint8_t POLL_POWER[] = {0x27, 0x07, 0x20, 0xF8, 0x0A, 0x03, 0x57, 0x00, 0x45, 0xE5, 0x9C};
  this->queue_command(std::vector<uint8_t>(POLL_POWER, POLL_POWER + sizeof(POLL_POWER)));

  // 2. Telemetria hydrauliczna: Przepływ, Wysokość podnoszenia, Temp medium (5D 01 21)
  // TX: 27 07 20 F8 0A 03 5D 01 21 3D 4E
  static const uint8_t POLL_HYDRAULIC[] = {0x27, 0x07, 0x20, 0xF8, 0x0A, 0x03, 0x5D, 0x01, 0x21, 0x3D, 0x4E};
  this->queue_command(std::vector<uint8_t>(POLL_HYDRAULIC, POLL_HYDRAULIC + sizeof(POLL_HYDRAULIC)));

  // 3. Stan pracy i nastawa zadana (56 00 06)
  // TX: 27 07 20 F8 0A 03 56 00 06 AA 0B
  static const uint8_t POLL_OP_MODE[] = {0x27, 0x07, 0x20, 0xF8, 0x0A, 0x03, 0x56, 0x00, 0x06, 0xAA, 0x0B};
  this->queue_command(std::vector<uint8_t>(POLL_OP_MODE, POLL_OP_MODE + sizeof(POLL_OP_MODE)));

  // 4. Tryb regulacji pompy (56 00 0A)
  // TX: 27 07 20 F8 0A 03 56 00 0A 6B 87
  static const uint8_t POLL_CTRL_MODE[] = {0x27, 0x07, 0x20, 0xF8, 0x0A, 0x03, 0x56, 0x00, 0x0A, 0x6B, 0x87};
  this->queue_command(std::vector<uint8_t>(POLL_CTRL_MODE, POLL_CTRL_MODE + sizeof(POLL_CTRL_MODE)));

  // 5. Alarmy i ostrzeżenia (05 01 4B)
  // TX: 27 05 20 F8 05 01 4B 81 BA
  static const uint8_t POLL_ALARMS[] = {0x27, 0x05, 0x20, 0xF8, 0x05, 0x01, 0x4B, 0x81, 0xBA};
  this->queue_command(std::vector<uint8_t>(POLL_ALARMS, POLL_ALARMS + sizeof(POLL_ALARMS)));

  // 6. Licznik całkowitej energii kWh (57 00 01) - co 3 cykle
  if (this->poll_cycles_ % 3 == 0) {
    // TX: 27 07 20 F8 0A 03 57 00 01 ED DC
    static const uint8_t POLL_ENERGY[] = {0x27, 0x07, 0x20, 0xF8, 0x0A, 0x03, 0x57, 0x00, 0x01, 0xED, 0xDC};
    this->queue_command(std::vector<uint8_t>(POLL_ENERGY, POLL_ENERGY + sizeof(POLL_ENERGY)));
  }

  // 7. Nazwa urządzenia ASCII (07 01 11) - co 10 cykli
  if (this->poll_cycles_ % 10 == 0) {
    // TX: 27 05 20 F8 07 01 11 14 65
    static const uint8_t POLL_NAME[] = {0x27, 0x05, 0x20, 0xF8, 0x07, 0x01, 0x11, 0x14, 0x65};
    this->queue_command(std::vector<uint8_t>(POLL_NAME, POLL_NAME + sizeof(POLL_NAME)));
  }

  this->poll_cycles_++;
}

void GrundfosAlpha3::queue_command(const std::vector<uint8_t> &frame, bool high_priority) {
  if (high_priority) {
    this->tx_queue_.push_front(frame);
  } else {
    // Zapobiegaj przepełnieniu kolejki pollingu
    if (this->tx_queue_.size() < 16) {
      this->tx_queue_.push_back(frame);
    }
  }
}

void GrundfosAlpha3::send_next_queued_command() {
  if (this->tx_queue_.empty()) return;
  auto frame = this->tx_queue_.front();
  this->tx_queue_.pop_front();
  this->send_frame_ble(frame);
}

bool GrundfosAlpha3::send_frame_ble(const std::vector<uint8_t> &frame) {
  if (!this->connected_ || this->char_handle_ == 0) {
    return false;
  }

  size_t total_len = frame.size();
  size_t offset = 0;
  while (offset < total_len) {
    size_t chunk_len = std::min((size_t) 20, total_len - offset);
    esp_err_t err = esp_ble_gattc_write_char(
        this->parent()->get_gattc_if(),
        this->parent()->get_conn_id(),
        this->char_handle_,
        chunk_len,
        const_cast<uint8_t *>(frame.data() + offset),
        ESP_GATT_WRITE_TYPE_NO_RSP,
        ESP_GATT_AUTH_REQ_NONE);

    if (err != ESP_OK) {
      ESP_LOGW(TAG, "[%s] Błąd wysyłania fragmentu BLE (offset %u, len %u): %d",
               this->parent()->address_str(), (unsigned) offset, (unsigned) chunk_len, err);
      return false;
    }
    offset += chunk_len;
    if (offset < total_len) {
      delay(15);
    }
  }
  return true;
}

void GrundfosAlpha3::handle_rx_bytes(const uint8_t *data, size_t length) {
  if (data == nullptr || length == 0) return;

  if (this->rx_buffer_.size() > 512) {
    this->rx_buffer_.clear();
  }

  // Dołącz nowe bajty do bufora reasemblacji
  this->rx_buffer_.insert(this->rx_buffer_.end(), data, data + length);

  // Szukaj początku ramki (0x24 = Slave Response, 0x25 = Slave Error)
  while (!this->rx_buffer_.empty()) {
    if (this->rx_buffer_[0] != 0x24 && this->rx_buffer_[0] != 0x25) {
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }

    if (this->rx_buffer_.size() < 2) {
      // Potrzeba więcej bajtów na pole LEN
      break;
    }

    uint8_t declared_len = this->rx_buffer_[1];
    size_t full_frame_len = declared_len + 4; // 1B SD + 1B LEN + LEN B danych + 2B CRC-16

    if (this->rx_buffer_.size() < full_frame_len) {
      // Czekamy na resztę fragmentów BLE (reasemblacja MTU)
      break;
    }

    // Wyodrębnij kompletną ramkę
    std::vector<uint8_t> frame(this->rx_buffer_.begin(), this->rx_buffer_.begin() + full_frame_len);
    this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + full_frame_len);

    // Weryfikacja sumy kontrolnej CRC-16 (od LEN [indeks 1] do końca danych, bez 2 bajtów CRC - SD jest pomijany!)
    uint16_t computed_crc = calculate_crc16(frame.data() + 1, full_frame_len - 3);
    uint16_t received_crc = (static_cast<uint16_t>(frame[full_frame_len - 2]) << 8) | frame[full_frame_len - 1];

    if (computed_crc == received_crc) {
      this->process_geni_frame(frame);
    } else {
      ESP_LOGW(TAG, "Odrzucono ramkę z błędną sumą CRC: obliczona 0x%04X != odebrana 0x%04X",
               computed_crc, received_crc);
    }
  }
}

void GrundfosAlpha3::process_geni_frame(const std::vector<uint8_t> &frame) {
  if (frame.size() < 6) return;

  uint8_t apdu_class = frame[4];
  uint8_t apdu_len = frame[5];

  // 1. Klasa 5: Alarmy i ostrzeżenia
  if (apdu_class == 0x05) {
    if (frame.size() >= 7) {
      uint8_t alarm_code = frame[6];
      if (this->alarm_code_sensor_ != nullptr) {
        this->alarm_code_sensor_->publish_state(alarm_code);
      }
      if (this->alarm_status_text_sensor_ != nullptr) {
        this->alarm_status_text_sensor_->publish_state(decode_alarm_code(alarm_code));
      }
    }
    return;
  }

  // 2. Klasa 7: Nazwa urządzenia (ASCII)
  if (apdu_class == 0x07) {
    if (frame.size() > 8) {
      size_t name_len = frame[1] - 4; // LEN - (DA+SA+Class+ApduLen)
      std::string name_str;
      for (size_t i = 6; i < 6 + name_len && i < frame.size() - 2; i++) {
        if (frame[i] == 0) break;
        name_str += static_cast<char>(frame[i]);
      }
      if (!name_str.empty() && this->pump_name_text_sensor_ != nullptr) {
        this->pump_name_text_sensor_->publish_state(name_str);
      }
    }
    return;
  }

  // 3. Klasa 10 (GEP): Parametry rozszerzone Grundfos
  if (apdu_class == 0x0A) {
    if (frame.size() < 14) return;

    const uint8_t *payload = &frame[13];
    size_t payload_len = frame.size() - 15; // odejmij nagłówki i 2 bajty CRC

    // Odpowiedź na 57 00 45 (Telemetria elektryczna: moc, RPM, napięcie, prąd, temp)
    if (payload_len >= 37) {
      float mains_voltage = parse_float_be(&payload[0]);
      float motor_current = parse_float_be(&payload[8]);
      float active_power = parse_float_be(&payload[12]);
      float shaft_power = parse_float_be(&payload[16]);
      float speed_rpm = parse_float_be(&payload[20]);
      float temp_electronics = parse_float_be(&payload[28]);
      float temp_motor = parse_float_be(&payload[32]);
      uint8_t running_flag = payload[36];

      if (!std::isnan(mains_voltage) && this->voltage_sensor_ != nullptr)
        this->voltage_sensor_->publish_state(mains_voltage);
      if (!std::isnan(motor_current) && this->current_sensor_ != nullptr)
        this->current_sensor_->publish_state(motor_current);
      if (!std::isnan(active_power) && this->power_sensor_ != nullptr)
        this->power_sensor_->publish_state(active_power);
      if (!std::isnan(shaft_power) && this->shaft_power_sensor_ != nullptr)
        this->shaft_power_sensor_->publish_state(shaft_power);
      if (!std::isnan(speed_rpm) && this->speed_sensor_ != nullptr)
        this->speed_sensor_->publish_state(speed_rpm);
      if (!std::isnan(temp_electronics) && this->temp_electronics_sensor_ != nullptr)
        this->temp_electronics_sensor_->publish_state(temp_electronics);
      if (!std::isnan(temp_motor) && this->temp_motor_sensor_ != nullptr)
        this->temp_motor_sensor_->publish_state(temp_motor);

      if (this->pump_running_sensor_ != nullptr) {
        this->pump_running_sensor_->publish_state(running_flag == 0x01);
      }
      return;
    }

    // Odpowiedź na 5D 01 21 (Telemetria hydrauliczna: przepływ, wysokość, temp wody)
    if (payload_len == 24) {
      float flow_m3s = parse_float_be(&payload[0]);
      float head_pa = parse_float_be(&payload[4]);
      float temp_liquid = parse_float_be(&payload[16]);

      if (!std::isnan(flow_m3s) && this->flow_sensor_ != nullptr) {
        // Przeliczenie m³/s -> m³/h
        float flow_m3h = flow_m3s * 3600.0f;
        this->flow_sensor_->publish_state(flow_m3h);
      }
      if (!std::isnan(head_pa) && this->head_sensor_ != nullptr) {
        // Przeliczenie Pa -> m słupa wody (1 m = 9806.65 Pa)
        float head_m = head_pa / 9806.65f;
        this->head_sensor_->publish_state(head_m);
      }
      if (!std::isnan(temp_liquid) && this->temp_liquid_sensor_ != nullptr) {
        this->temp_liquid_sensor_->publish_state(temp_liquid);
      }
      return;
    }

    // Odpowiedź na 56 00 06 (Stan pracy, tryb regulacji i nastawa)
    // Format: 24 12 01 F8 0A 0E 00 01 2F 01 00 00 07 00 [OP_MODE] [CTRL_MODE] [FLOAT_SETPOINT_PA] [CRC16]
    if (payload_len >= 7 && payload[0] == 0x00 && payload[1] <= 0x03) {
      uint8_t op_mode = payload[1];
      uint8_t ctrl_code = payload[2];
      float setpoint_pa = parse_float_be(&payload[3]);

      this->current_op_mode_ = op_mode;
      this->current_ctrl_mode_ = ctrl_code;

      std::string mode_str = decode_op_mode(op_mode);
      if (this->operating_mode_text_sensor_ != nullptr) {
        this->operating_mode_text_sensor_->publish_state(mode_str);
      }
      if (this->operating_mode_select_ != nullptr) {
        this->operating_mode_select_->publish_state(mode_str);
      }
      if (this->power_switch_ != nullptr) {
        this->power_switch_->publish_state(op_mode == 0);
      }

      std::string ctrl_str = decode_ctrl_mode(ctrl_code);
      if (this->control_mode_text_sensor_ != nullptr) {
        this->control_mode_text_sensor_->publish_state(ctrl_str);
      }
      if (this->control_mode_select_ != nullptr) {
        this->control_mode_select_->publish_state(ctrl_str);
      }

      if (!std::isnan(setpoint_pa) && setpoint_pa > 0.0f) {
        float setpoint_m = setpoint_pa / 9806.65f;
        this->current_setpoint_m_ = setpoint_m;
        if (this->current_setpoint_sensor_ != nullptr) {
          this->current_setpoint_sensor_->publish_state(setpoint_m);
        }
        if (this->setpoint_number_ != nullptr) {
          this->setpoint_number_->publish_state(roundf(setpoint_m * 10.0f) / 10.0f);
        }
      }

      // Automatyczna aktywacja autoryzacji magistrali (Remote Control Mode):
      // Przy pierwszym połączeniu pompa znajduje się w trybie lokalnym / autonomicznym.
      // Wysłanie ramki Object 6 natychmiast przejmuje kontrolę zdalną bez konieczności ręcznego przełączania na Maks!
      if (!this->remote_control_initialized_) {
        this->remote_control_initialized_ = true;
        ESP_LOGI(TAG, "Inicjalizacja autoryzacji sterowania magistralą GENI pompy ALPHA3 (Object 6)...");
        this->write_operating_mode(decode_op_mode(this->current_op_mode_));
      }

      return;
    }

    // Odpowiedź na 56 00 0A (Tryb regulacji)
    // Format: 24 12 01 F8 0A 0E 00 01 2F 01 00 00 07 00 06 [CTRL_CODE] 7F FF FF FF [CRC16]
    if (payload_len >= 3 && payload[0] == 0x00 && payload[1] == 0x06) {
      uint8_t ctrl_code = payload[2];
      this->current_ctrl_mode_ = ctrl_code;

      std::string ctrl_str = decode_ctrl_mode(ctrl_code);
      if (this->control_mode_text_sensor_ != nullptr) {
        this->control_mode_text_sensor_->publish_state(ctrl_str);
      }
      if (this->control_mode_select_ != nullptr) {
        this->control_mode_select_->publish_state(ctrl_str);
      }
      return;
    }

    // Odpowiedź na 57 00 01 (Energia całkowita kWh - Obiekt 232 / 0x00E8)
    if (frame.size() >= 23 && frame[8] == 0xE8) {
      double energy_joules = parse_double_be(&frame[13]);
      if (!std::isnan(energy_joules) && energy_joules > 0.0) {
        // 1 kWh = 3 600 000 J
        float energy_kwh = static_cast<float>(energy_joules / 3600000.0);
        if (this->energy_sensor_ != nullptr) {
          this->energy_sensor_->publish_state(energy_kwh);
        }
      }
      return;
    }
  }
}

void GrundfosAlpha3::write_power(bool state) {
  ESP_LOGI(TAG, "Zmieniono przełącznik zasilania pompy: %s", state ? "ON (Normal)" : "OFF (Stop)");
  if (this->power_switch_ != nullptr) {
    this->power_switch_->publish_state(state);
  }
  this->write_operating_mode(state ? "Normalny" : "Stop");
}

void GrundfosAlpha3::write_operating_mode(const std::string &mode_str) {
  uint8_t op_mode = encode_op_mode(mode_str);
  this->current_op_mode_ = op_mode;

  ESP_LOGI(TAG, "Wysyłanie nowego stanu pracy: %s (kod 0x%02X)", mode_str.c_str(), op_mode);

  if (this->operating_mode_select_ != nullptr) {
    this->operating_mode_select_->publish_state(mode_str);
  }
  if (this->operating_mode_text_sensor_ != nullptr) {
    this->operating_mode_text_sensor_->publish_state(mode_str);
  }
  if (this->power_switch_ != nullptr) {
    this->power_switch_->publish_state(op_mode == 0);
  }

  // Ramka zapisu stanu pracy (Obj 6):
  // 27 14 20 F8 0A 90 56 00 06 01 2F 01 00 00 07 00 [MODE] [CTRL] [FLOAT_SETPOINT_PA] [CRC16]
  uint8_t frame[22];
  frame[0] = 0x27;
  frame[1] = 0x14;
  frame[2] = 0x20;
  frame[3] = 0xF8;
  frame[4] = 0x0A;
  frame[5] = 0x90;
  frame[6] = 0x56;
  frame[7] = 0x00;
  frame[8] = 0x06;
  frame[9] = 0x01;
  frame[10] = 0x2F;
  frame[11] = 0x01;
  frame[12] = 0x00;
  frame[13] = 0x00;
  frame[14] = 0x07;
  frame[15] = 0x00;
  frame[16] = op_mode;
  frame[17] = this->current_ctrl_mode_;

  float sp_pa = this->current_setpoint_m_ * 9806.65f;
  uint32_t raw_sp;
  memcpy(&raw_sp, &sp_pa, sizeof(raw_sp));
  frame[18] = (raw_sp >> 24) & 0xFF;
  frame[19] = (raw_sp >> 16) & 0xFF;
  frame[20] = (raw_sp >> 8) & 0xFF;
  frame[21] = raw_sp & 0xFF;

  uint16_t crc = calculate_crc16(frame + 1, 21);
  std::vector<uint8_t> pkt(frame, frame + 22);
  pkt.push_back((crc >> 8) & 0xFF);
  pkt.push_back(crc & 0xFF);

  this->queue_command(pkt, true);
}

void GrundfosAlpha3::write_control_mode(const std::string &mode_str) {
  uint8_t ctrl_code = encode_ctrl_mode(mode_str);
  this->current_ctrl_mode_ = ctrl_code;

  std::string clean_name = decode_ctrl_mode(ctrl_code);
  ESP_LOGI(TAG, "Wysyłanie nowego trybu regulacji: '%s' -> '%s' (kod 0x%02X)",
           mode_str.c_str(), clean_name.c_str(), ctrl_code);

  if (this->control_mode_select_ != nullptr) {
    this->control_mode_select_->publish_state(clean_name);
  }
  if (this->control_mode_text_sensor_ != nullptr) {
    this->control_mode_text_sensor_->publish_state(clean_name);
  }

  // Ramka zapisu trybu regulacji (Obj 10):
  // 27 14 20 F8 0A 90 56 00 0A 01 2F 01 00 00 07 00 06 [CTRL] 7F FF FF FF [CRC16]
  uint8_t frame10[22];
  frame10[0] = 0x27;
  frame10[1] = 0x14;
  frame10[2] = 0x20;
  frame10[3] = 0xF8;
  frame10[4] = 0x0A;
  frame10[5] = 0x90;
  frame10[6] = 0x56;
  frame10[7] = 0x00;
  frame10[8] = 0x0A;
  frame10[9] = 0x01;
  frame10[10] = 0x2F;
  frame10[11] = 0x01;
  frame10[12] = 0x00;
  frame10[13] = 0x00;
  frame10[14] = 0x07;
  frame10[15] = 0x00;
  frame10[16] = 0x06;
  frame10[17] = ctrl_code;
  frame10[18] = 0x7F;
  frame10[19] = 0xFF;
  frame10[20] = 0xFF;
  frame10[21] = 0xFF;

  uint16_t crc10 = calculate_crc16(frame10 + 1, 21);
  std::vector<uint8_t> pkt10(frame10, frame10 + 22);
  pkt10.push_back((crc10 >> 8) & 0xFF);
  pkt10.push_back(crc10 & 0xFF);

  this->queue_command(pkt10, true);

  // Wyślij również aktualizację do Object 6, aby zsynchronizować tryb pracy pompy
  this->write_operating_mode(decode_op_mode(this->current_op_mode_));

  // Kolejkuj odpytanie weryfikujące (56 00 0A)
  static const uint8_t POLL_CTRL_MODE[] = {0x27, 0x07, 0x20, 0xF8, 0x0A, 0x03, 0x56, 0x00, 0x0A, 0x6B, 0x87};
  this->queue_command(std::vector<uint8_t>(POLL_CTRL_MODE, POLL_CTRL_MODE + sizeof(POLL_CTRL_MODE)));
}

void GrundfosAlpha3::write_setpoint(float setpoint_m) {
  if (setpoint_m < 0.5f) setpoint_m = 0.5f;
  if (setpoint_m > 5.0f) setpoint_m = 5.0f;
  this->current_setpoint_m_ = setpoint_m;

  float setpoint_pa = setpoint_m * 9806.65f;
  ESP_LOGI(TAG, "Wysyłanie nowej wartości zadanej: %.1f m (%.0f Pa)", setpoint_m, setpoint_pa);

  if (this->setpoint_number_ != nullptr) {
    this->setpoint_number_->publish_state(setpoint_m);
  }
  if (this->current_setpoint_sensor_ != nullptr) {
    this->current_setpoint_sensor_->publish_state(setpoint_m);
  }

  // 1. Wyślij ramkę zapisu rejestru wartości zadanej (Obj 16):
  // 27 1F 20 F8 0A 9B 56 00 10 01 2E 01 00 00 12 00 04 [FLOAT_BE] 3F 00 00 00 41 00 00 00 00 00 00 00 [CRC16]
  uint8_t frame[33];
  frame[0] = 0x27;
  frame[1] = 0x1F;
  frame[2] = 0x20;
  frame[3] = 0xF8;
  frame[4] = 0x0A;
  frame[5] = 0x9B;
  frame[6] = 0x56;
  frame[7] = 0x00;
  frame[8] = 0x10;
  frame[9] = 0x01;
  frame[10] = 0x2E;
  frame[11] = 0x01;
  frame[12] = 0x00;
  frame[13] = 0x00;
  frame[14] = 0x12;
  frame[15] = 0x00;
  frame[16] = 0x04;

  uint32_t raw_sp;
  memcpy(&raw_sp, &setpoint_pa, sizeof(raw_sp));
  frame[17] = (raw_sp >> 24) & 0xFF;
  frame[18] = (raw_sp >> 16) & 0xFF;
  frame[19] = (raw_sp >> 8) & 0xFF;
  frame[20] = raw_sp & 0xFF;

  frame[21] = 0x3F;
  frame[22] = 0x00;
  frame[23] = 0x00;
  frame[24] = 0x00;
  frame[25] = 0x41;
  frame[26] = 0x00;
  frame[27] = 0x00;
  frame[28] = 0x00;
  frame[29] = 0x00;
  frame[30] = 0x00;
  frame[31] = 0x00;
  frame[32] = 0x00;

  uint16_t crc = calculate_crc16(frame + 1, 32);
  std::vector<uint8_t> pkt(frame, frame + 33);
  pkt.push_back((crc >> 8) & 0xFF);
  pkt.push_back(crc & 0xFF);

  this->queue_command(pkt, true);

  // 2. KLUCZOWE DLA GRUNDFOS ALPHA3: Wyślij również ramkę punktu pracy (Obj 6) z nową nastawą!
  // To właśnie ta ramka bezpośrednio steruje nastawą punktu pracy pompy ALPHA3 w bieżącym trybie!
  uint8_t frame6[22];
  frame6[0] = 0x27;
  frame6[1] = 0x14;
  frame6[2] = 0x20;
  frame6[3] = 0xF8;
  frame6[4] = 0x0A;
  frame6[5] = 0x90;
  frame6[6] = 0x56;
  frame6[7] = 0x00;
  frame6[8] = 0x06;
  frame6[9] = 0x01;
  frame6[10] = 0x2F;
  frame6[11] = 0x01;
  frame6[12] = 0x00;
  frame6[13] = 0x00;
  frame6[14] = 0x07;
  frame6[15] = 0x00;
  frame6[16] = this->current_op_mode_;
  frame6[17] = this->current_ctrl_mode_;
  frame6[18] = (raw_sp >> 24) & 0xFF;
  frame6[19] = (raw_sp >> 16) & 0xFF;
  frame6[20] = (raw_sp >> 8) & 0xFF;
  frame6[21] = raw_sp & 0xFF;

  uint16_t crc6 = calculate_crc16(frame6 + 1, 21);
  std::vector<uint8_t> pkt6(frame6, frame6 + 22);
  pkt6.push_back((crc6 >> 8) & 0xFF);
  pkt6.push_back(crc6 & 0xFF);

  this->queue_command(pkt6, true);
}

uint16_t GrundfosAlpha3::calculate_crc16(const uint8_t *data, size_t length) {
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

float GrundfosAlpha3::parse_float_be(const uint8_t *data) {
  uint32_t val = (static_cast<uint32_t>(data[0]) << 24) |
                 (static_cast<uint32_t>(data[1]) << 16) |
                 (static_cast<uint32_t>(data[2]) << 8) |
                 static_cast<uint32_t>(data[3]);
  if (val == 0x7FFFFFFF || val == 0xFFC00000) {
    return NAN;
  }
  float f;
  memcpy(&f, &val, sizeof(f));
  return f;
}

double GrundfosAlpha3::parse_double_be(const uint8_t *data) {
  uint64_t val = (static_cast<uint64_t>(data[0]) << 56) |
                 (static_cast<uint64_t>(data[1]) << 48) |
                 (static_cast<uint64_t>(data[2]) << 40) |
                 (static_cast<uint64_t>(data[3]) << 32) |
                 (static_cast<uint64_t>(data[4]) << 24) |
                 (static_cast<uint64_t>(data[5]) << 16) |
                 (static_cast<uint64_t>(data[6]) << 8) |
                 static_cast<uint64_t>(data[7]);
  double d;
  memcpy(&d, &val, sizeof(d));
  return d;
}

std::string GrundfosAlpha3::decode_ctrl_mode(uint8_t code) {
  switch (code) {
    case 0x00: return "Ciśnienie stałe";
    case 0x01: return "Ciśnienie proporcjonalne";
    case 0x02: return "Charakterystyka stała";
    case 0x0D: return "Tryb grzejnikowy";
    case 0x0E: return "Tryb ogrzewania podłogowego";
    case 0x0F: return "Grzejnikowe i podłogowe";
    default:
      char buf[32];
      snprintf(buf, sizeof(buf), "Tryb 0x%02X", code);
      return std::string(buf);
  }
}

uint8_t GrundfosAlpha3::encode_ctrl_mode(const std::string &str) {
  // 1. Grzejnikowe i podłogowe (0x0F)
  if ((str.find("grzejnik") != std::string::npos || str.find("Grzejnik") != std::string::npos || str.find("radiator") != std::string::npos) &&
      (str.find("podłog") != std::string::npos || str.find("podlog") != std::string::npos || str.find("underfloor") != std::string::npos)) {
    return 0x0F;
  }
  // 2. Tryb grzejnikowy (0x0D)
  if (str.find("grzejnik") != std::string::npos || str.find("Grzejnik") != std::string::npos || str.find("radiator") != std::string::npos) {
    return 0x0D;
  }
  // 3. Tryb ogrzewania podłogowego (0x0E)
  if (str.find("podłog") != std::string::npos || str.find("podlog") != std::string::npos || str.find("underfloor") != std::string::npos) {
    return 0x0E;
  }
  // 4. Ciśnienie proporcjonalne (0x01)
  if (str.find("proporc") != std::string::npos || str.find("Proporc") != std::string::npos || str.find("proportional") != std::string::npos) {
    return 0x01;
  }
  // 5. Charakterystyka stała (0x02)
  if (str.find("charakteryst") != std::string::npos || str.find("Charakteryst") != std::string::npos ||
      str.find("curve") != std::string::npos || str.find("Curve") != std::string::npos) {
    return 0x02;
  }
  // 6. Stałe ciśnienie / Ciśnienie stałe (0x00)
  if (str.find("stałe") != std::string::npos || str.find("stale") != std::string::npos ||
      str.find("constant") != std::string::npos || str.find("Constant") != std::string::npos ||
      str.find("ciśn") != std::string::npos || str.find("cisn") != std::string::npos) {
    return 0x00;
  }

  // Wartości numeryczne lub heksadecymalne jako fallback
  if (str == "13" || str == "0x0D" || str == "0x0d") return 0x0D;
  if (str == "14" || str == "0x0E" || str == "0x0e") return 0x0E;
  if (str == "15" || str == "0x0F" || str == "0x0f") return 0x0F;
  if (str == "1" || str == "0x01") return 0x01;
  if (str == "2" || str == "0x02") return 0x02;
  if (str == "0" || str == "0x00") return 0x00;

  return 0x00;
}

std::string GrundfosAlpha3::decode_op_mode(uint8_t code) {
  switch (code) {
    case 0x01: return "Stop";
    case 0x02: return "Min";
    case 0x03: return "Maks";
    case 0x00:
    default: return "Normalny";
  }
}

uint8_t GrundfosAlpha3::encode_op_mode(const std::string &str) {
  if (str.find("Stop") != std::string::npos || str.find("stop") != std::string::npos || str == "1") return 0x01;
  if (str.find("Min") != std::string::npos || str.find("min") != std::string::npos || str == "2") return 0x02;
  if (str.find("Maks") != std::string::npos || str.find("maks") != std::string::npos ||
      str.find("Max") != std::string::npos || str.find("max") != std::string::npos || str == "3") return 0x03;
  return 0x00;
}

std::string GrundfosAlpha3::decode_alarm_code(uint8_t code) {
  switch (code) {
    case 0x00: return "Brak błędu (Normalna praca)";
    case 0x0A: return "F01 - Suchobieg (Dry running)";
    case 0x1E: return "F02 - Zablokowany wirnik (Rotor blocked)";
    case 0x28: return "F03 - Przeciążenie silnika (Motor overload)";
    case 0x38: return "F04 - Przegrzanie modułu/silnika (Overheat)";
    case 0x48: return "F05 - Błąd czujnika wewnętrznego (Sensor fault)";
    case 0x58: return "F06 - Błąd komunikacji (Communication fault)";
    case 0x60: return "F07 - Niskie napięcie zasilania (Undervoltage)";
    case 0x64: return "F08 - Przepięcie w sieci (Overvoltage)";
    default:
      char buf[32];
      snprintf(buf, sizeof(buf), "Błąd 0x%02X (%d)", code, code);
      return std::string(buf);
  }
}

}  // namespace grundfos_alpha3
}  // namespace esphome
