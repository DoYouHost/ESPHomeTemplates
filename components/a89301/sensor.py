import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_SPEED,
    CONF_TEMPERATURE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_FAN,
    ICON_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_PERCENT,
    UNIT_REVOLUTIONS_PER_MINUTE,
    UNIT_VOLT,
)

from . import CONF_A89301_ID, A89301

DEPENDENCIES = ["a89301"]

CONF_RPM = "rpm"
CONF_BUS_CURRENT = "bus_current"
CONF_Q_AXIS_CURRENT = "q_axis_current"
CONF_SUPPLY_VOLTAGE = "supply_voltage"
CONF_SPEED_DEMAND = "speed_demand"
CONF_DUTY = "duty"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_A89301_ID): cv.use_id(A89301),
        cv.Optional(CONF_SPEED): sensor.sensor_schema(
            unit_of_measurement=UNIT_HERTZ,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_FREQUENCY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RPM): sensor.sensor_schema(
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
            accuracy_decimals=0,
            icon=ICON_FAN,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BUS_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_Q_AXIS_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SUPPLY_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_SPEED_DEMAND): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            icon=ICON_PERCENT,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_DUTY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            icon=ICON_PERCENT,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)

SENSORS = {
    CONF_SPEED: "set_speed_sensor",
    CONF_RPM: "set_rpm_sensor",
    CONF_BUS_CURRENT: "set_bus_current_sensor",
    CONF_Q_AXIS_CURRENT: "set_q_axis_current_sensor",
    CONF_SUPPLY_VOLTAGE: "set_supply_voltage_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
    CONF_SPEED_DEMAND: "set_speed_demand_sensor",
    CONF_DUTY: "set_duty_sensor",
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_A89301_ID])
    for key, setter in SENSORS.items():
        if (conf := config.get(key)) is not None:
            cg.add(getattr(parent, setter)(await sensor.new_sensor(conf)))
