from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@MorganMLGman"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

a89301_ns = cg.esphome_ns.namespace("a89301")
A89301 = a89301_ns.class_("A89301", cg.PollingComponent, i2c.I2CDevice)

CONF_A89301_ID = "a89301_id"
CONF_POLE_PAIRS = "pole_pairs"
CONF_BRAKE_PIN = "brake_pin"
CONF_BRAKE_WHEN_OFF = "brake_when_off"
CONF_BRAKE_DELAY = "brake_delay"
CONF_DISABLE_STANDBY = "disable_standby"
CONF_WATCHDOG_INTERVAL = "watchdog_interval"
CONF_SENSE_RESISTOR = "sense_resistor"
CONF_OVER_VOLTAGE_THRESHOLD = "over_voltage_threshold"
CONF_UNDER_VOLTAGE_THRESHOLD = "under_voltage_threshold"


def _validate(config):
    if CONF_BRAKE_PIN not in config and config[CONF_BRAKE_WHEN_OFF]:
        raise cv.Invalid(
            f"'{CONF_BRAKE_WHEN_OFF}' requires '{CONF_BRAKE_PIN}' to be set. "
            f"Set '{CONF_BRAKE_WHEN_OFF}: false' if the motor should coast when off."
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(A89301),
            cv.Required(CONF_POLE_PAIRS): cv.int_range(min=1, max=255),
            cv.Optional(CONF_BRAKE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BRAKE_WHEN_OFF, default=True): cv.boolean,
            cv.Optional(
                CONF_BRAKE_DELAY, default="2s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DISABLE_STANDBY, default=True): cv.boolean,
            cv.Optional(
                CONF_WATCHDOG_INTERVAL, default="1s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SENSE_RESISTOR): cv.resistance,
            cv.Optional(CONF_OVER_VOLTAGE_THRESHOLD, default="47V"): cv.voltage,
            cv.Optional(CONF_UNDER_VOLTAGE_THRESHOLD, default="5.5V"): cv.voltage,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(i2c.i2c_device_schema(0x55)),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_pole_pairs(config[CONF_POLE_PAIRS]))
    cg.add(var.set_brake_when_off(config[CONF_BRAKE_WHEN_OFF]))
    cg.add(var.set_brake_delay(config[CONF_BRAKE_DELAY]))
    cg.add(var.set_disable_standby(config[CONF_DISABLE_STANDBY]))
    cg.add(var.set_watchdog_interval(config[CONF_WATCHDOG_INTERVAL]))
    cg.add(var.set_over_voltage_threshold(config[CONF_OVER_VOLTAGE_THRESHOLD]))
    cg.add(var.set_under_voltage_threshold(config[CONF_UNDER_VOLTAGE_THRESHOLD]))

    if (brake_pin := config.get(CONF_BRAKE_PIN)) is not None:
        cg.add(var.set_brake_pin(await cg.gpio_pin_expression(brake_pin)))
    if (sense_resistor := config.get(CONF_SENSE_RESISTOR)) is not None:
        cg.add(var.set_sense_resistor(sense_resistor))
