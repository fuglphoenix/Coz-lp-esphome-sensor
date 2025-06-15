import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    DEVICE_CLASS_CARBON_DIOXIDE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

from . import MYCO2Component, myco2_ns

# Define the configuration schema for a sensor
CONFIG_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PARTS_PER_MILLION,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_CARBON_DIOXIDE,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(sensor.Sensor),
        cv.Required("myco2_hub_id"): cv.use_id(MYCO2Component),
    }
)

# The function that will be called to generate the C++ code for the sensor
async def to_code(config):
    # Get the hub that this sensor is connected to
    hub = await cg.get_variable(config["myco2_hub_id"])
    # Create a new sensor object
    var = cg.new_Pvariable(config[CONF_ID])
    # Set the parent hub for this sensor
    cg.add(hub.set_co2_sensor(var))
    # Register the sensor
    await sensor.register_sensor(var, config)