import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, ICON_FAN

from . import CONF_A89301_ID, A89301

DEPENDENCIES = ["a89301"]

CONF_OPERATION_STATE = "operation_state"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_A89301_ID): cv.use_id(A89301),
        cv.Optional(CONF_OPERATION_STATE): text_sensor.text_sensor_schema(
            icon=ICON_FAN,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_A89301_ID])
    if (conf := config.get(CONF_OPERATION_STATE)) is not None:
        cg.add(
            parent.set_operation_state_text_sensor(
                await text_sensor.new_text_sensor(conf)
            )
        )
