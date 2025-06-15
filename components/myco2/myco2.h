#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

#include "esphome/components/api/custom_api_device.h"
#include <cstdarg>

namespace esphome {
namespace myco2 {

static const char *const TAG = "myco2";


class MYCO2 : public PollingComponent, public uart::UARTDevice, public api::CustomAPIDevice {
 public:
  MYCO2() : PollingComponent(30000) {}

  // ... (struct AutoZeroConfig, set_co2_sensor, set_auto_zero_config are unchanged) ...
  struct AutoZeroConfig {
    bool enabled = false;
    float initial_interval_days = 0.0;
    float regular_interval_days = 0.0;
  };
  void set_co2_sensor(sensor::Sensor *s) { this->co2_sensor_ = s; }
  void set_auto_zero_config(bool enabled, float initial_days, float regular_days);


  float get_setup_priority() const override { return setup_priority::DATA; }

  void setup() override {
    ESP_LOGD(TAG, "Setting up MYCO2...");
    packet_buffer_.reserve(32);
    send_command("K 2\r\n");
    send_command("M 4\r\n");

    if (this->auto_zero_config_.enabled) {
      ESP_LOGI(TAG, "Applying auto-zero config (Initial: %.1f days, Regular: %.1f days)",
               this->auto_zero_config_.initial_interval_days, this->auto_zero_config_.regular_interval_days);
      this->enable_auto_zero(this->auto_zero_config_.initial_interval_days,
                             this->auto_zero_config_.regular_interval_days);
    }


    ESP_LOGI(TAG, "Registering services...");
    register_service(&MYCO2::zero_fresh_air, "zero_fresh_air");
    register_service(&MYCO2::zero_nitrogen, "zero_nitrogen");
    register_service(&MYCO2::zero_known_gas, "zero_known_gas", {"concentration"});
    register_service(&MYCO2::enable_auto_zero, "enable_auto_zero", {"initial_interval", "regular_interval"});
    register_service(&MYCO2::disable_auto_zero, "disable_auto_zero");
  }

  // --- Service Methods (must be public) ---
  void zero_fresh_air() {
    ESP_LOGI(TAG, "Service called: Zeroing in fresh air...");
    send_command("G\r\n");
  }
  void zero_nitrogen() {
    ESP_LOGI(TAG, "Service called: Zeroing in nitrogen...");
    send_command("U\r\n");
  }
  void zero_known_gas(int concentration) {
    ESP_LOGI(TAG, "Service called: Zeroing with known gas concentration: %d ppm", concentration);
    send_formatted_command("X %d\r\n", concentration);
  }
  void enable_auto_zero(float initial_interval, float regular_interval) {
    ESP_LOGI(TAG, "Service called: Enabling auto-zero (Initial: %.1f, Regular: %.1f)", initial_interval, regular_interval);
    send_formatted_command("@ %.1f %.1f\r\n", initial_interval, regular_interval);
  }
  void disable_auto_zero() {
    ESP_LOGI(TAG, "Service called: Disabling auto-zero");
    send_command("@ 0\r\n");
  }

  // ... (loop, update, and protected members/methods are unchanged) ...
  void loop() override;
  void update() override;

 protected:
  sensor::Sensor *co2_sensor_{nullptr};
  std::string packet_buffer_;
  AutoZeroConfig auto_zero_config_;
  bool parse_packet(const std::string &payload, int *p_co2);
  void send_command(const char *command);
  void send_formatted_command(const char *format, ...);
};


// --- Implementations (no changes needed here) ---
void MYCO2::set_auto_zero_config(bool enabled, float initial_days, float regular_days) {
  ESP_LOGD(TAG, "Received auto-zero config - Enabled: %s, Initial: %.1fd, Regular: %.1fd",
           enabled ? "true" : "false", initial_days, regular_days);
  this->auto_zero_config_.enabled = enabled;
  this->auto_zero_config_.initial_interval_days = initial_days;
  this->auto_zero_config_.regular_interval_days = regular_days;
}

void MYCO2::loop() {
  while (available()) {
    char in_char = read();
    packet_buffer_ += in_char;
    if (in_char == '\n') {
      ESP_LOGV(TAG, "Received packet: [%s]", packet_buffer_.c_str());
      int co2_val;
      if (parse_packet(packet_buffer_, &co2_val)) {
        if (this->co2_sensor_ != nullptr) {
          this->co2_sensor_->publish_state(co2_val);
        }
      }
      packet_buffer_.clear();
    }
  }
}

void MYCO2::update() {
    send_command("Q\r\n");
}

bool MYCO2::parse_packet(const std::string &payload, int *p_co2) {
    const char *start_of_z = strstr(payload.c_str(), "Z");
    if (start_of_z == nullptr) return false;
    int value = 0;
    if (sscanf(start_of_z, "Z %d", &value) == 1) {
        *p_co2 = value;
        return true;
    }
    return false;
}

void MYCO2::send_command(const char *command) {
    write_str(command);
    ESP_LOGV(TAG, "Sent command: %s", command);
}

void MYCO2::send_formatted_command(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[32];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    send_command(buffer);
}

}  // namespace myco2
}  // namespace esphome