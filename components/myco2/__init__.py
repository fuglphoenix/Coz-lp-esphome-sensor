import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

# Namespace and class declaration remain the same
myco2_ns = cg.esphome_ns.namespace("myco2")
MYCO2Component = myco2_ns.class_("MYCO2", cg.PollingComponent, uart.UARTDevice)

# Define keys for our configuration options
CONF_AUTO_ZERO = "auto_zero"
CONF_INITIAL_INTERVAL = "initial_interval"
CONF_REGULAR_INTERVAL = "regular_interval"

# The schema should ONLY define the component's YAML configuration.
# NO service schemas should be here.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MYCO2Component),
        cv.Optional(CONF_AUTO_ZERO): cv.Schema(
            {
                cv.Required(CONF_INITIAL_INTERVAL): cv.positive_time_period,
                cv.Required(CONF_REGULAR_INTERVAL): cv.positive_time_period,
            }
        ),
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    # This code is now much simpler and correct.
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[uart.CONF_UART_ID])
    cg.add(var.set_uart_parent(parent))

    if auto_zero_config := config.get(CONF_AUTO_ZERO):
        initial_days = auto_zero_config[CONF_INITIAL_INTERVAL].total_seconds / 86400.0
        regular_days = auto_zero_config[CONF_REGULAR_INTERVAL].total_seconds / 86400.0
        cg.add(var.set_auto_zero_config(True, initial_days, regular_days))