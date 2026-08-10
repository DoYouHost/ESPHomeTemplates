import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_RUNNING,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from . import CONF_A89301_ID, A89301

DEPENDENCIES = ["a89301"]

CONF_FAULT = "fault"
CONF_SPINNING = "spinning"
CONF_STARTING = "starting"
CONF_STANDBY = "standby"
CONF_BRAKING = "braking"
CONF_LOCK = "lock"
CONF_OVER_CURRENT = "over_current"
CONF_OVER_TEMPERATURE = "over_temperature"
CONF_SYSTEM_ERROR = "system_error"
CONF_OVER_VOLTAGE = "over_voltage"
CONF_UNDER_VOLTAGE = "under_voltage"

PROBLEM_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_PROBLEM,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)
RUNNING_SCHEMA = binary_sensor.binary_sensor_schema(device_class=DEVICE_CLASS_RUNNING)
STATUS_SCHEMA = binary_sensor.binary_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_A89301_ID): cv.use_id(A89301),
        cv.Optional(CONF_FAULT): PROBLEM_SCHEMA,
        cv.Optional(CONF_SPINNING): RUNNING_SCHEMA,
        cv.Optional(CONF_STARTING): STATUS_SCHEMA,
        cv.Optional(CONF_STANDBY): STATUS_SCHEMA,
        cv.Optional(CONF_BRAKING): STATUS_SCHEMA,
        cv.Optional(CONF_LOCK): PROBLEM_SCHEMA,
        cv.Optional(CONF_OVER_CURRENT): PROBLEM_SCHEMA,
        cv.Optional(CONF_OVER_TEMPERATURE): PROBLEM_SCHEMA,
        cv.Optional(CONF_SYSTEM_ERROR): PROBLEM_SCHEMA,
        cv.Optional(CONF_OVER_VOLTAGE): PROBLEM_SCHEMA,
        cv.Optional(CONF_UNDER_VOLTAGE): PROBLEM_SCHEMA,
    }
)

BINARY_SENSORS = {
    CONF_FAULT: "set_fault_binary_sensor",
    CONF_SPINNING: "set_spinning_binary_sensor",
    CONF_STARTING: "set_starting_binary_sensor",
    CONF_STANDBY: "set_standby_binary_sensor",
    CONF_BRAKING: "set_braking_binary_sensor",
    CONF_LOCK: "set_lock_binary_sensor",
    CONF_OVER_CURRENT: "set_over_current_binary_sensor",
    CONF_OVER_TEMPERATURE: "set_over_temperature_binary_sensor",
    CONF_SYSTEM_ERROR: "set_system_error_binary_sensor",
    CONF_OVER_VOLTAGE: "set_over_voltage_binary_sensor",
    CONF_UNDER_VOLTAGE: "set_under_voltage_binary_sensor",
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_A89301_ID])
    for key, setter in BINARY_SENSORS.items():
        if (conf := config.get(key)) is not None:
            cg.add(getattr(parent, setter)(await binary_sensor.new_binary_sensor(conf)))
