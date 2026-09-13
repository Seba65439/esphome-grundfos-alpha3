import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID
from . import GrundfosAlpha3, CONF_GRUNDFOS_ALPHA3_ID, grundfos_alpha3_ns

GrundfosAlpha3PairButton = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3PairButton", button.Button, cg.Component
)
GrundfosAlpha3UnpairButton = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3UnpairButton", button.Button, cg.Component
)

CONF_PAIR_PUMP = "pair_pump"
CONF_UNPAIR_PUMP = "unpair_pump"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GRUNDFOS_ALPHA3_ID): cv.use_id(GrundfosAlpha3),
        cv.Optional(CONF_PAIR_PUMP): button.button_schema(
            GrundfosAlpha3PairButton,
            icon="mdi:bluetooth-connect",
        ),
        cv.Optional(CONF_UNPAIR_PUMP): button.button_schema(
            GrundfosAlpha3UnpairButton,
            icon="mdi:bluetooth-off",
        ),
    }
)

async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRUNDFOS_ALPHA3_ID])
    if CONF_PAIR_PUMP in config:
        btn = await button.new_button(config[CONF_PAIR_PUMP])
        cg.add(btn.set_parent(parent))
    if CONF_UNPAIR_PUMP in config:
        btn = await button.new_button(config[CONF_UNPAIR_PUMP])
        cg.add(btn.set_parent(parent))
