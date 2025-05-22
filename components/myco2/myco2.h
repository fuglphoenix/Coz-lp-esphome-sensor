#pragma once // Use #pragma once or traditional include guards

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"
#include "esphome/core/automation.h" // Needed for register_service

#include <vector> // Needed for String/vector usage if esphome.h didn't pull it in implicitly

namespace esphome {
namespace myco2 {

static const char *const TAG = "myco2"; // Define TAG within namespace

class MYCO2 : public PollingComponent, public uart::UARTDevice, public Component {
public:
    // Constructor: Initialize PollingComponent with a default (will be overridden by YAML)
    MYCO2(uart::UARTComponent *parent) : PollingComponent(30000), uart::UARTDevice(parent) {}

    // Data structure to hold the CO2 measurement
    struct MYCO2Packet {
        int co2;
    };
    // Structure to hold auto-zero configuration
    struct AutoZeroConfig {
        bool enabled = false; // Default: disabled
        float initial_interval = 0;
        float regular_interval = 0;
    };

    // Function to set the auto-zero configuration (called from sensor.py)
    void set_auto_zero_config(AutoZeroConfig config) {
        auto_zero_config_ = config;
    }

    // Setter for the sensor object created in sensor.py
    void set_co2_sensor(sensor::Sensor *sensor) { this->co2_sensor_ = sensor; }

    // === Setup and Configuration ===

    float get_setup_priority() const override {
        return setup_priority::DATA;
    }

    void setup() override {
        ESP_LOGCONFIG(TAG, "Setting up MyCO2 Sensor...");
        packet_buffer_.reserve(32);

        // Initialize the sensor (adjust commands as per your sensor's datasheet)
        send_command("K 2\r\n");  // Set the sensor to polling mode
        send_command("M 4\r\n");

        // Auto-zero configuration (initialized from YAML via set_auto_zero_config)
        if (auto_zero_config_.enabled) {
            ESP_LOGCONFIG(TAG, "Auto-zero configured: Initial %.1f days, Regular %.1f days",
                        auto_zero_config_.initial_interval, auto_zero_config_.regular_interval);
            enable_auto_zero(auto_zero_config_.initial_interval, auto_zero_config_.regular_interval);
        } else {
            ESP_LOGCONFIG(TAG, "Auto-zero disabled by configuration.");
            disable_auto_zero(); // Optionally send disable command explicitly if needed
        }

        // Register services for Home Assistant
        register_service(&MYCO2::zero_fresh_air, "zero_fresh_air");
        register_service(&MYCO2::zero_nitrogen, "zero_nitrogen");
        register_service(&MYCO2::zero_known_gas, "zero_known_gas", {"concentration"});
        register_service(&MYCO2::enable_auto_zero, "enable_auto_zero",
                        {"initial_interval_decimal_days", "regular_interval_decimal_days"});
        register_service(&MYCO2::disable_auto_zero, "disable_auto_zero");
    }

    void dump_config() override {
        ESP_LOGCONFIG(TAG, "MyCO2 Sensor:");
        LOG_SENSOR("  ", "CO2 Sensor", this->co2_sensor_);
        LOG_UPDATE_INTERVAL(this);
        LOG_UART_DEVICE();
        ESP_LOGCONFIG(TAG, "  Auto-zero Enabled: %s", ONOFF(this->auto_zero_config_.enabled));
         if (this->auto_zero_config_.enabled) {
            ESP_LOGCONFIG(TAG, "    Initial Interval: %.1f days", this->auto_zero_config_.initial_interval);
            ESP_LOGCONFIG(TAG, "    Regular Interval: %.1f days", this->auto_zero_config_.regular_interval);
        }
    }

    // === Main Loop (Handles Incoming Data) ===

    void loop() override {
        while (available()) {
            char in_char = read();
            // Handle potential buffer overflow - though unlikely with short packets
            if (packet_buffer_.length() >= 31) {
                 ESP_LOGW(TAG, "Packet buffer near overflow, clearing.");
                 packet_buffer_.clear();
            }

            packet_buffer_ += in_char;

            if (in_char == '\n') {
                ESP_LOGV(TAG, "Received packet: [%s]", packet_buffer_.c_str()); // Changed to VERBOSE
                if (parse_packet(packet_buffer_, &packet_)) {
                    if (this->co2_sensor_ != nullptr) {
                        this->co2_sensor_->publish_state(packet_.co2);
                    } else {
                        ESP_LOGW(TAG, "CO2 sensor object not set!");
                    }
                }
                packet_buffer_.clear();  // Clear the buffer for the next packet
            }
        }
    }

    // === Update (Called Periodically by PollingComponent) ===

    void update() override {
        // Request a new CO2 measurement
        ESP_LOGD(TAG, "Requesting CO2 measurement");
        send_command("Q\r\n");
    }

    // === Service Implementations ===

    void zero_fresh_air() {
        ESP_LOGD(TAG, "Service: Zeroing in fresh air...");
        send_command("G\r\n");
    }

    void zero_nitrogen() {
        ESP_LOGD(TAG, "Service: Zeroing in nitrogen...");
        send_command("U\r\n");
    }

    void zero_known_gas(int concentration) {
        ESP_LOGD(TAG, "Service: Zeroing with known gas concentration: %d ppm", concentration);
        send_formatted_command("X %d\r\n", concentration);
    }

    void enable_auto_zero(float initial_interval, float regular_interval) {
        ESP_LOGD(TAG, "Service: Enabling auto-zero with initial interval %.1f days and regular interval %.1f days",
                initial_interval, regular_interval);
        send_formatted_command("@ %.1f %.1f\r\n", initial_interval, regular_interval);
        // Update internal config state if called via service
        this->auto_zero_config_.enabled = true;
        this->auto_zero_config_.initial_interval = initial_interval;
        this->auto_zero_config_.regular_interval = regular_interval;
    }

    void disable_auto_zero() {
        ESP_LOGD(TAG, "Service: Disabling auto-zero");
        send_command("@ 0\r\n");
         // Update internal config state if called via service
        this->auto_zero_config_.enabled = false;
    }

private:
    MYCO2Packet packet_;
    std::string packet_buffer_; // Use std::string instead of Arduino String
    AutoZeroConfig auto_zero_config_;
    sensor::Sensor *co2_sensor_{nullptr}; // Pointer to the sensor object

    // Helper function to parse the received packet
    bool parse_packet(const std::string &payload, MYCO2Packet *p) {
        size_t co2_start_index = payload.find('Z'); // Use std::string::find

        // Validate the start of the CO2 value is found
        if (co2_start_index == std::string::npos) {
            ESP_LOGW(TAG, "Invalid packet format: 'Z' not found in [%s]", payload.c_str());
            return false;
        }

        size_t end_index = payload.find(']', co2_start_index);
        if (end_index == std::string::npos) {
             ESP_LOGW(TAG, "Invalid packet format: ']' not found after 'Z' in [%s]", payload.c_str());
             return false;
        }

        // Check if there are enough characters after 'Z' for a valid reading
        // Assuming format is like "Z xxxxx]" -> index Z+2 to end_index-1
        size_t value_start_index = co2_start_index + 2;
         if (value_start_index >= end_index) {
            ESP_LOGW(TAG, "Invalid packet format: No digits found between 'Z' and ']' in [%s]", payload.c_str());
            return false;
        }

        try {
            std::string co2_str = payload.substr(value_start_index, end_index - value_start_index);
            // Trim leading/trailing whitespace just in case
            co2_str.erase(0, co2_str.find_first_not_of(' '));
            co2_str.erase(co2_str.find_last_not_of(' ') + 1);

            p->co2 = std::stoi(co2_str); // Use std::stoi for conversion
            ESP_LOGD(TAG, "Parsed CO2 value: %d", p->co2);
            return true;
        } catch (const std::invalid_argument &ia) {
            ESP_LOGW(TAG, "Invalid number format for CO2 in packet: [%s]", payload.c_str());
            return false;
        } catch (const std::out_of_range &oor) {
             ESP_LOGW(TAG, "CO2 value out of range in packet: [%s]", payload.c_str());
             return false;
        }
    }

    // Helper function to send commands
    void send_command(const char *command) {
        write_str(command);
        ESP_LOGD(TAG, "Sent command: %s", command); // Changed to DEBUG
    }

    // Helper function to send formatted commands
    void send_formatted_command(const char *format, ...) {
        va_list args;
        va_start(args, format);
        char buffer[64]; // Increased buffer size slightly
        int ret = vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        if (ret < 0 || ret >= sizeof(buffer)) {
             ESP_LOGW(TAG, "Error formatting command or buffer too small");
             return;
        }
        send_command(buffer);
    }
};

} // namespace myco2
} // namespace esphome