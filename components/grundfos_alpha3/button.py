import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import CONF_GRUNDFOS_ALPHA3_ID, GrundfosAlpha3, grundfos_alpha3_ns

GrundfosAlpha3PairButton = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3PairButton", button.Button, cg.Parented.template(GrundfosAlpha3)
)
GrundfosAlpha3UnpairButton = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3UnpairButton", button.Button, cg.Parented.template(GrundfosAlpha3)
)

CONF_PAIR_PUMP = "pair_pump"
CONF_UNPAIR_PUMP = "unpair_pump"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GRUNDFOS_ALPHA3_ID): cv.use_id(GrundfosAlpha3),
        cv.Optional(CONF_PAIR_PUMP): button.button_schema(
            GrundfosAlpha3PairButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:bluetooth-connect",
        ),
        cv.Optional(CONF_UNPAIR_PUMP): button.button_schema(
            GrundfosAlpha3UnpairButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:bluetooth-off",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRUNDFOS_ALPHA3_ID])
    for key in (CONF_PAIR_PUMP, CONF_UNPAIR_PUMP):
        if key in config:
            btn = await button.new_button(config[key])
            cg.add(btn.set_parent(parent))
