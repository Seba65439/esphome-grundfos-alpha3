import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_SWITCH,
)
from . import GrundfosAlpha3, grundfos_alpha3_ns, CONF_GRUNDFOS_ALPHA3_ID

GrundfosAlpha3PowerSwitch = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3PowerSwitch", switch.Switch, cg.Component
)

CONF_PUMP_POWER = "pump_power"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GRUNDFOS_ALPHA3_ID): cv.use_id(GrundfosAlpha3),
        cv.Optional(CONF_PUMP_POWER): switch.switch_schema(
            GrundfosAlpha3PowerSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            icon="mdi:power",
        ),
    }
)

async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRUNDFOS_ALPHA3_ID])
    if CONF_PUMP_POWER in config:
        sw = await switch.new_switch(config[CONF_PUMP_POWER])
        await cg.register_component(sw, config[CONF_PUMP_POWER])
        cg.add(sw.set_parent(parent))
        cg.add(parent.set_power_switch(sw))
