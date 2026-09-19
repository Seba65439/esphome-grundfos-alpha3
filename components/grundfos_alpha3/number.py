import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv

from . import CONF_GRUNDFOS_ALPHA3_ID, GrundfosAlpha3, grundfos_alpha3_ns

GrundfosAlpha3SetpointNumber = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3SetpointNumber", number.Number, cg.Parented.template(GrundfosAlpha3)
)

CONF_SETPOINT = "setpoint"

# Zakres musi odpowiadać SETPOINT_MIN_M / SETPOINT_MAX_M w grundfos_alpha3.cpp
SETPOINT_MIN_M = 0.5
SETPOINT_MAX_M = 5.0
SETPOINT_STEP_M = 0.1

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GRUNDFOS_ALPHA3_ID): cv.use_id(GrundfosAlpha3),
        cv.Optional(CONF_SETPOINT): number.number_schema(
            GrundfosAlpha3SetpointNumber,
            unit_of_measurement="m",
            icon="mdi:arrow-up-down",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRUNDFOS_ALPHA3_ID])
    if CONF_SETPOINT in config:
        num = await number.new_number(
            config[CONF_SETPOINT],
            min_value=SETPOINT_MIN_M,
            max_value=SETPOINT_MAX_M,
            step=SETPOINT_STEP_M,
        )
        cg.add(num.set_parent(parent))
        cg.add(parent.set_setpoint_number(num))
