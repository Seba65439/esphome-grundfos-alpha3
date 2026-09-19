import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH

from . import CONF_GRUNDFOS_ALPHA3_ID, GrundfosAlpha3, grundfos_alpha3_ns

GrundfosAlpha3PowerSwitch = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3PowerSwitch", switch.Switch, cg.Parented.template(GrundfosAlpha3)
)

CONF_PUMP_POWER = "pump_power"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GRUNDFOS_ALPHA3_ID): cv.use_id(GrundfosAlpha3),
        # Stan przełącznika zawsze pochodzi z pompy - przywracanie stanu po restarcie nie ma zastosowania.
        cv.Optional(CONF_PUMP_POWER): switch.switch_schema(
            GrundfosAlpha3PowerSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            icon="mdi:power",
            default_restore_mode="DISABLED",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRUNDFOS_ALPHA3_ID])
    if CONF_PUMP_POWER in config:
        sw = await switch.new_switch(config[CONF_PUMP_POWER])
        cg.add(sw.set_parent(parent))
        cg.add(parent.set_power_switch(sw))
