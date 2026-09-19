import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv

from . import CONF_GRUNDFOS_ALPHA3_ID, GrundfosAlpha3, grundfos_alpha3_ns

GrundfosAlpha3OperatingModeSelect = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3OperatingModeSelect", select.Select, cg.Parented.template(GrundfosAlpha3)
)
GrundfosAlpha3ControlModeSelect = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3ControlModeSelect", select.Select, cg.Parented.template(GrundfosAlpha3)
)

CONF_OPERATING_MODE = "operating_mode"
CONF_CONTROL_MODE = "control_mode"

# Opcje muszą być identyczne z tablicami OPERATING_MODES / CONTROL_MODES w grundfos_alpha3.cpp
OPERATING_MODE_OPTIONS = ["Normalny", "Stop", "Min", "Maks"]
CONTROL_MODE_OPTIONS = [
    "Ciśnienie stałe",
    "Ciśnienie proporcjonalne",
    "Charakterystyka stała",
    "Tryb grzejnikowy",
    "Tryb ogrzewania podłogowego",
    "Grzejnikowe i podłogowe",
]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GRUNDFOS_ALPHA3_ID): cv.use_id(GrundfosAlpha3),
        cv.Optional(CONF_OPERATING_MODE): select.select_schema(
            GrundfosAlpha3OperatingModeSelect,
            icon="mdi:pump",
        ),
        cv.Optional(CONF_CONTROL_MODE): select.select_schema(
            GrundfosAlpha3ControlModeSelect,
            icon="mdi:tune-vertical",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRUNDFOS_ALPHA3_ID])

    if CONF_OPERATING_MODE in config:
        sel = await select.new_select(
            config[CONF_OPERATING_MODE], options=OPERATING_MODE_OPTIONS
        )
        cg.add(sel.set_parent(parent))
        cg.add(parent.set_operating_mode_select(sel))

    if CONF_CONTROL_MODE in config:
        sel = await select.new_select(
            config[CONF_CONTROL_MODE], options=CONTROL_MODE_OPTIONS
        )
        cg.add(sel.set_parent(parent))
        cg.add(parent.set_control_mode_select(sel))
