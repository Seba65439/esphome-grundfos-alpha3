#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/number/number.h"
#include "esphome/components/button/button.h"

#include <deque>
#include <string>
#include <vector>

namespace esphome::grundfos_alpha3 {

class GrundfosAlpha3;

class GrundfosAlpha3PowerSwitch : public switch_::Switch, public Parented<GrundfosAlpha3> {
 protected:
  void write_state(bool state) override;
};

class GrundfosAlpha3OperatingModeSelect : public select::Select, public Parented<GrundfosAlpha3> {
 protected:
  void control(const std::string &value) override;
};

class GrundfosAlpha3ControlModeSelect : public select::Select, public Parented<GrundfosAlpha3> {
 protected:
  void control(const std::string &value) override;
};

class GrundfosAlpha3SetpointNumber : public number::Number, public Parented<GrundfosAlpha3> {
 protected:
  void control(float value) override;
};

class GrundfosAlpha3PairButton : public button::Button, public Parented<GrundfosAlpha3> {
 protected:
  void press_action() override;
};

class GrundfosAlpha3UnpairButton : public button::Button, public Parented<GrundfosAlpha3> {
 protected:
  void press_action() override;
};

class GrundfosAlpha3 : public PollingComponent, public ble_client::BLEClientNode {
 public:
  GrundfosAlpha3() : PollingComponent(10000) {}

  void loop() override;
  void update() override;
  void dump_config() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

  // Sensor Setters
  void set_power_sensor(sensor::Sensor *s) { power_sensor_ = s; }
  void set_speed_sensor(sensor::Sensor *s) { speed_sensor_ = s; }
  void set_flow_sensor(sensor::Sensor *s) { flow_sensor_ = s; }
  void set_head_sensor(sensor::Sensor *s) { head_sensor_ = s; }
  void set_current_setpoint_sensor(sensor::Sensor *s) { current_setpoint_sensor_ = s; }
  void set_energy_sensor(sensor::Sensor *s) { energy_sensor_ = s; }
  void set_voltage_sensor(sensor::Sensor *s) { voltage_sensor_ = s; }
  void set_current_sensor(sensor::Sensor *s) { current_sensor_ = s; }
  void set_shaft_power_sensor(sensor::Sensor *s) { shaft_power_sensor_ = s; }
  void set_temp_electronics_sensor(sensor::Sensor *s) { temp_electronics_sensor_ = s; }
  void set_temp_motor_sensor(sensor::Sensor *s) { temp_motor_sensor_ = s; }
  void set_temp_liquid_sensor(sensor::Sensor *s) { temp_liquid_sensor_ = s; }
  void set_alarm_code_sensor(sensor::Sensor *s) { alarm_code_sensor_ = s; }

  void set_pump_running_sensor(binary_sensor::BinarySensor *s) { pump_running_sensor_ = s; }
  void set_pump_paired_sensor(binary_sensor::BinarySensor *s) { pump_paired_sensor_ = s; }

  void set_operating_mode_text_sensor(text_sensor::TextSensor *s) { operating_mode_text_sensor_ = s; }
  void set_control_mode_text_sensor(text_sensor::TextSensor *s) { control_mode_text_sensor_ = s; }
  void set_alarm_status_text_sensor(text_sensor::TextSensor *s) { alarm_status_text_sensor_ = s; }
  void set_pump_name_text_sensor(text_sensor::TextSensor *s) { pump_name_text_sensor_ = s; }

  void set_power_switch(GrundfosAlpha3PowerSwitch *sw) { power_switch_ = sw; }
  void set_operating_mode_select(GrundfosAlpha3OperatingModeSelect *sel) { operating_mode_select_ = sel; }
  void set_control_mode_select(GrundfosAlpha3ControlModeSelect *sel) { control_mode_select_ = sel; }
  void set_setpoint_number(GrundfosAlpha3SetpointNumber *num) { setpoint_number_ = num; }

  // Control Actions (nazwy trybów muszą odpowiadać opcjom z select.py)
  void write_power(bool state);
  void write_operating_mode(const std::string &mode_str);
  void write_control_mode(const std::string &mode_str);
  void write_setpoint(float setpoint_m);

  // Pairing Actions
  void pair_pump();
  void unpair_pump();
  /// Odczytuje z NVS, czy pompa ma zapisaną więź (bond) i aktualizuje bonded_.
  bool is_device_bonded();

 protected:
  // --- Budowanie ramek GENI ---
  static std::vector<uint8_t> build_frame_(uint8_t apdu_class, uint8_t op, const std::vector<uint8_t> &data);
  static std::vector<uint8_t> build_read_(uint8_t apdu_class, uint8_t id);
  static std::vector<uint8_t> build_gep_read_(uint8_t sub_id, uint16_t obj_id);
  static std::vector<uint8_t> build_gep_write_(uint8_t sub_id, uint16_t obj_id, uint16_t type_id,
                                               const std::vector<uint8_t> &payload);
  static void append_float_be_(std::vector<uint8_t> &out, float value);

  // --- Sterowanie ---
  bool can_write_(const char *action);
  void send_operating_mode_(uint8_t op_mode);
  void send_control_mode_(uint8_t ctrl_mode);
  std::vector<uint8_t> build_operation_frame_(float setpoint_pa) const;
  void publish_operating_mode_(uint8_t op_mode);
  void publish_control_mode_(uint8_t ctrl_mode);
  void publish_setpoint_(float setpoint_m);

  // --- Nadawanie (nieblokujące) ---
  void queue_command_(std::vector<uint8_t> frame, bool high_priority = false);
  void process_tx_(uint32_t now);
  bool send_chunk_(uint32_t now);

  // --- Odbiór ---
  void handle_rx_bytes_(const uint8_t *data, size_t length);
  void process_geni_frame_(const std::vector<uint8_t> &frame);
  void process_gep_reply_(const std::vector<uint8_t> &frame);

  void configure_security_();
  void reset_connection_state_();

  static uint16_t calculate_crc16_(const uint8_t *data, size_t length);
  static float parse_float_be_(const uint8_t *data);
  static double parse_double_be_(const uint8_t *data);
  static std::string decode_alarm_code_(uint8_t code);
  static std::string decode_ctrl_mode_(uint8_t code);
  static std::string decode_op_mode_(uint8_t code);

  uint16_t char_handle_{0};
  bool connected_{false};
  bool notify_registered_{false};
  bool security_configured_{false};
  bool bond_state_loaded_{false};
  bool was_paired_{false};
  bool was_encrypted_{false};
  bool bonded_{false};
  bool remote_control_initialized_{false};
  /// true po odebraniu stanu pracy (Obj 6) w bieżącym połączeniu - dopiero wtedy wolno wysyłać zapisy.
  bool operation_state_valid_{false};
  uint32_t last_pair_attempt_{0};

  // RX
  std::vector<uint8_t> rx_buffer_;
  uint32_t last_rx_time_{0};

  // TX: kolejka priorytetowa (zapisy, FIFO) jest opróżniana przed kolejką odpytywania.
  std::deque<std::vector<uint8_t>> tx_priority_queue_;
  std::deque<std::vector<uint8_t>> tx_poll_queue_;
  std::vector<uint8_t> tx_frame_;
  size_t tx_offset_{0};
  uint32_t last_tx_frame_time_{0};
  uint32_t last_tx_chunk_time_{0};
  uint32_t poll_cycles_{0};

  float current_setpoint_m_{1.5f};
  uint8_t current_op_mode_{0};
  uint8_t current_ctrl_mode_{0};

  // Sensor Pointers
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *speed_sensor_{nullptr};
  sensor::Sensor *flow_sensor_{nullptr};
  sensor::Sensor *head_sensor_{nullptr};
  sensor::Sensor *current_setpoint_sensor_{nullptr};
  sensor::Sensor *energy_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *shaft_power_sensor_{nullptr};
  sensor::Sensor *temp_electronics_sensor_{nullptr};
  sensor::Sensor *temp_motor_sensor_{nullptr};
  sensor::Sensor *temp_liquid_sensor_{nullptr};
  sensor::Sensor *alarm_code_sensor_{nullptr};

  binary_sensor::BinarySensor *pump_running_sensor_{nullptr};
  binary_sensor::BinarySensor *pump_paired_sensor_{nullptr};

  text_sensor::TextSensor *operating_mode_text_sensor_{nullptr};
  text_sensor::TextSensor *control_mode_text_sensor_{nullptr};
  text_sensor::TextSensor *alarm_status_text_sensor_{nullptr};
  text_sensor::TextSensor *pump_name_text_sensor_{nullptr};

  GrundfosAlpha3PowerSwitch *power_switch_{nullptr};
  GrundfosAlpha3OperatingModeSelect *operating_mode_select_{nullptr};
  GrundfosAlpha3ControlModeSelect *control_mode_select_{nullptr};
  GrundfosAlpha3SetpointNumber *setpoint_number_{nullptr};
};

}  // namespace esphome::grundfos_alpha3

#endif  // USE_ESP32
