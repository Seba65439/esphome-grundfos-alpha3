import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from . import GrundfosAlpha3, grundfos_alpha3_ns, CONF_GRUNDFOS_ALPHA3_ID

GrundfosAlpha3SetpointNumber = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3SetpointNumber", number.Number, cg.Component
)

CONF_SETPOINT = "setpoint"

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
            min_value=0.5,
            max_value=5.0,
            step=0.1,
        )
        await cg.register_component(num, config[CONF_SETPOINT])
        cg.add(num.set_parent(parent))
        cg.add(parent.set_setpoint_number(num))
