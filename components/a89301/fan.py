import esphome.codegen as cg
from esphome.components import fan
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SPEED_COUNT

from . import CONF_A89301_ID, A89301, a89301_ns

DEPENDENCIES = ["a89301"]

CONF_MIN_DEMAND = "min_demand"

A89301Fan = a89301_ns.class_("A89301Fan", cg.Component, fan.Fan)

CONFIG_SCHEMA = (
    fan.fan_schema(A89301Fan)
    .extend(
        {
            cv.GenerateID(CONF_A89301_ID): cv.use_id(A89301),
            cv.Optional(CONF_SPEED_COUNT, default=100): cv.int_range(min=1, max=511),
            cv.Optional(CONF_MIN_DEMAND, default="10%"): cv.percentage,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_A89301_ID])

    var = cg.new_Pvariable(
        config[CONF_ID],
        parent,
        config[CONF_SPEED_COUNT],
        config[CONF_MIN_DEMAND] * 100.0,
    )
    await cg.register_component(var, config)
    await fan.register_fan(var, config)
