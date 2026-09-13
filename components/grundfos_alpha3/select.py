import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from . import GrundfosAlpha3, grundfos_alpha3_ns, CONF_GRUNDFOS_ALPHA3_ID

GrundfosAlpha3OperatingModeSelect = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3OperatingModeSelect", select.Select, cg.Component
)
GrundfosAlpha3ControlModeSelect = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3ControlModeSelect", select.Select, cg.Component
)

CONF_OPERATING_MODE = "operating_mode"
CONF_CONTROL_MODE = "control_mode"

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
            config[CONF_OPERATING_MODE],
            options=["Normalny", "Stop", "Min", "Maks"],
        )
        await cg.register_component(sel, config[CONF_OPERATING_MODE])
        cg.add(sel.set_parent(parent))
        cg.add(parent.set_operating_mode_select(sel))
        
    if CONF_CONTROL_MODE in config:
        sel = await select.new_select(
            config[CONF_CONTROL_MODE],
            options=[
                "Ciśnienie stałe",
                "Ciśnienie proporcjonalne",
                "Charakterystyka stała",
                "Tryb grzejnikowy",
                "Tryb ogrzewania podłogowego",
                "Grzejnikowe i podłogowe",
            ],
        )
        await cg.register_component(sel, config[CONF_CONTROL_MODE])
        cg.add(sel.set_parent(parent))
        cg.add(parent.set_control_mode_select(sel))
