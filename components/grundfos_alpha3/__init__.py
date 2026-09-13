import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import CONF_ID

DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor", "switch", "select", "number", "button"]
MULTI_CONF = True

grundfos_alpha3_ns = cg.esphome_ns.namespace("grundfos_alpha3")
GrundfosAlpha3 = grundfos_alpha3_ns.class_(
    "GrundfosAlpha3", cg.PollingComponent, ble_client.BLEClientNode
)

CONF_GRUNDFOS_ALPHA3_ID = "grundfos_alpha3_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GrundfosAlpha3),
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
