#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/number/number.h"
#include "esphome/components/button/button.h"

#include <vector>
#include <string>
#include <deque>

namespace esphome {
namespace grundfos_alpha3 {

class GrundfosAlpha3;

class GrundfosAlpha3PowerSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(GrundfosAlpha3 *parent) { parent_ = parent; }
 protected:
  void write_state(bool state) override;
  GrundfosAlpha3 *parent_{nullptr};
};

class GrundfosAlpha3OperatingModeSelect : public select::Select, public Component {
 public:
  void set_parent(GrundfosAlpha3 *parent) { parent_ = parent; }
 protected:
  void control(const std::string &value) override;
  GrundfosAlpha3 *parent_{nullptr};
};

class GrundfosAlpha3ControlModeSelect : public select::Select, public Component {
 public:
  void set_parent(GrundfosAlpha3 *parent) { parent_ = parent; }
 protected:
  void control(const std::string &value) override;
  GrundfosAlpha3 *parent_{nullptr};
};

class GrundfosAlpha3SetpointNumber : public number::Number, public Component {
 public:
  void set_parent(GrundfosAlpha3 *parent) { parent_ = parent; }
 protected:
  void control(float value) override;
  GrundfosAlpha3 *parent_{nullptr};
};

class GrundfosAlpha3PairButton : public button::Button, public Component {
 public:
  void set_parent(GrundfosAlpha3 *parent) { parent_ = parent; }
 protected:
  void press_action() override;
  GrundfosAlpha3 *parent_{nullptr};
};

class GrundfosAlpha3UnpairButton : public button::Button, public Component {
 public:
  void set_parent(GrundfosAlpha3 *parent) { parent_ = parent; }
 protected:
  void press_action() override;
  GrundfosAlpha3 *parent_{nullptr};
};

class GrundfosAlpha3 : public PollingComponent, public ble_client::BLEClientNode {
 public:
  GrundfosAlpha3();

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

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

  // Control Actions
  void write_power(bool state);
  void write_operating_mode(const std::string &mode_str);
  void write_control_mode(const std::string &mode_str);
  void write_setpoint(float setpoint_m);

  // Pairing Actions
  void pair_pump();
  void unpair_pump();
  bool is_device_bonded();

 protected:
  void queue_command(const std::vector<uint8_t> &frame, bool high_priority = false);
  void send_next_queued_command();
  bool send_frame_ble(const std::vector<uint8_t> &frame);

  void handle_rx_bytes(const uint8_t *data, size_t length);
  void process_geni_frame(const std::vector<uint8_t> &frame);

  static uint16_t calculate_crc16(const uint8_t *data, size_t length);
  static float parse_float_be(const uint8_t *data);
  static double parse_double_be(const uint8_t *data);
  static std::string decode_alarm_code(uint8_t code);
  static std::string decode_ctrl_mode(uint8_t code);
  static uint8_t encode_ctrl_mode(const std::string &str);
  static std::string decode_op_mode(uint8_t code);
  static uint8_t encode_op_mode(const std::string &str);

  uint16_t char_handle_{0};
  uint16_t cccd_handle_{0};
  bool connected_{false};
  bool notify_registered_{false};
  bool was_paired_{false};
  bool is_bonded_{false};
  uint32_t last_pair_attempt_{0};
  uint32_t last_bond_check_{0};
  bool remote_control_initialized_{false};

  std::vector<uint8_t> rx_buffer_;
  std::deque<std::vector<uint8_t>> tx_queue_;
  uint32_t last_tx_time_{0};
  uint32_t last_poll_step_time_{0};
  uint8_t poll_step_{0};
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

}  // namespace grundfos_alpha3
}  // namespace esphome
