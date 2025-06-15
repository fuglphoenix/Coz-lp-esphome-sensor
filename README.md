# CozIR®-LP CO2 Sensor Component for ESPHome

This is an external component for [ESPHome](https://esphome.io/) that integrates UART-based CO2 sensors. It provides a clean way to read sensor values and expose advanced calibration and configuration functions as services in Home Assistant.


## Features

-   **CO2 Sensor Integration**: Reads CO2 values in parts-per-million (PPM) from a UART-connected sensor.
-   **Home Assistant Services**: Exposes device-specific services for calibration and configuration (e.g., `zero_fresh_air`, `enable_auto_zero`).
-   **Configurable Auto-Zero**: Allows for enabling and configuring the sensor's automatic background calibration directly from the ESPHome YAML.
-   **Easy Installation**: Deploys directly from GitHub, no manual file copying required.

## Installation & Configuration

This component is installed by adding it as an `external_component` pointing to this GitHub repository.  

Simply add the following configuration to your device's `.yaml` file:

```yaml
# 1. Point ESPHome to this GitHub repository to install the component
external_components:
  - source:
      type: git
      url: "https://github.com/fuglphoenix/Coz-lp-esphome-sensor"
      ref: "main" # You can pin this to a specific version/tag in the future
    # refresh: 2s # Uncomment for local development to force a refresh

# 2. Configure the UART bus for communication with the sensor
uart:
  id: uart_bus
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

# 3. Configure the myco2 component hub
myco2:
  id: my_co2_hub
  uart_id: uart_bus
  # Optional: Configure the sensor's automatic background zeroing feature.
  # If this block is omitted, auto-zero will be disabled by default.
  auto_zero:
    initial_interval: 14d # Time before the first auto-zero. Use d, h, min, s.
    regular_interval: 1d  # Interval for subsequent auto-zero operations.

# 4. Create the sensor entity that will appear in Home Assistant
sensor:
  - platform: myco2
    myco2_hub_id: my_co2_hub
    name: "Living Room CO2"
```

## Home Assistant Services

Once the device is connected to Home Assistant, this component will automatically create several services to interact with the sensor. You can call these from automations, scripts, or the Developer Tools UI.

The service name will always follow the format `esphome.<your_device_name>_<service_name>`. For example, if your device is named `co2_sensor` in ESPHome, the `zero_fresh_air` service will be `esphome.co2_sensor_zero_fresh_air`.

| Service                 | Description                                                                     | Parameters                               |
| ----------------------- | ------------------------------------------------------------------------------- | ---------------------------------------- |
| `zero_fresh_air`        | Calibrates the sensor, assuming it is currently in fresh air (~415 ppm).        | None                                     |
| `zero_nitrogen`         | Calibrates the sensor's zero point, assuming it is in 100% nitrogen.            | None                                     |
| `zero_known_gas`        | Calibrates the sensor to a specific known gas concentration.                    | `concentration` (integer, required, in ppm) |
| `enable_auto_zero`      | Manually enables the automatic background calibration feature on the sensor.    | `initial_interval` (number, required, in decimal days)<br>`regular_interval` (number, required, in decimal days) |
| `disable_auto_zero`     | Manually disables the automatic background calibration feature.                 | None                                     |

