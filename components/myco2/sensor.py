import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_ID,
    CONF_CO2,
    CONF_UPDATE_INTERVAL,
    CONF_NAME,
    UNIT_PARTS_PER_MILLION,
    ICON_MOLECULE_CO2,
    DEVICE_CLASS_CARBON_DIOXIDE,
    STATE_CLASS_MEASUREMENT,
)

# Declare dependencies
DEPENDENCIES = ['uart']
# Declare the namespace used in the C++ code
myco2_ns = cg.esphome_ns.namespace('esphome').namespace('myco2')
# Declare the C++ class name
MYCO2Component = myco2_ns.class_('MYCO2', cg.PollingComponent, uart.UARTDevice, cg.Component)
# Declare the C++ AutoZeroConfig struct name
AutoZeroConfigStruct = myco2_ns.struct('AutoZeroConfig')

# Define configuration keys for auto-zero
CONF_AUTO_ZERO = 'auto_zero'
CONF_ENABLED = 'enabled'
CONF_INITIAL_INTERVAL = 'initial_interval_days'
CONF_REGULAR_INTERVAL = 'regular_interval_days'

# Define the configuration schema for the 'auto_zero' block
AUTO_ZERO_SCHEMA = cv.Schema({
    cv.Required(CONF_ENABLED): cv.boolean,
    cv.Optional(CONF_INITIAL_INTERVAL, default=14.0): cv.positive_float, # Default from old lambda
    cv.Optional(CONF_REGULAR_INTERVAL, default=1.0): cv.positive_float,  # Default from old lambda
})

# Define the main configuration schema for the platform
CONFIG_SCHEMA = sensor.sensor_schema(
        unit_of_measurement=UNIT_PARTS_PER_MILLION,
        icon=ICON_MOLECULE_CO2,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_CARBON_DIOXIDE,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend({
        # Generate an ID for the C++ component object
        cv.GenerateID(): cv.declare_id(MYCO2Component),
        # Require the ID of the UART bus to use
        cv.Required(uart.CONF_UART_ID): cv.use_id(uart.UARTComponent),
        # Optional auto-zero configuration block
        cv.Optional(CONF_AUTO_ZERO): AUTO_ZERO_SCHEMA,
        # Allow overriding the default update interval (30s from C++ constructor)
        cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
    }).extend(cv.COMPONENT_SCHEMA) # Include standard component settings like update_interval


async def to_code(config):
    # Find the UART bus component using its ID
    uart_component = await cg.get_variable(config[uart.CONF_UART_ID])
    # Create a new instance of the MYCO2 C++ class (Pvariable for pointer)
    var = cg.new_Pvariable(config[CONF_ID], uart_component)

    # Register the C++ object as a component
    await cg.register_component(var, config)
    # Register the C++ object as a UART device
    await uart.register_uart_device(var, config)

    # Create the sensor entity based on the schema configuration
    sens = await sensor.new_sensor(config)
    # Link the sensor entity to the C++ object's sensor pointer
    cg.add(var.set_co2_sensor(sens))

    # Handle auto-zero configuration if present in YAML
    if auto_zero_config := config.get(CONF_AUTO_ZERO):
         # Create a C++ AutoZeroConfig struct instance in the generated code
        auto_zero_struct = cg.Struct(AutoZeroConfigStruct)() # Create instance
        # Set the struct members from the YAML config
        cg.add(auto_zero_struct.set_enabled(auto_zero_config[CONF_ENABLED]))
        cg.add(auto_zero_struct.set_initial_interval(auto_zero_config[CONF_INITIAL_INTERVAL]))
        cg.add(auto_zero_struct.set_regular_interval(auto_zero_config[CONF_REGULAR_INTERVAL]))
         # Call the C++ set_auto_zero_config method with the struct
        cg.add(var.set_auto_zero_config(auto_zero_struct))