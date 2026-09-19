import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_RUNNING,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from . import CONF_GRUNDFOS_ALPHA3_ID, GrundfosAlpha3

CONF_PUMP_RUNNING = "pump_running"
CONF_PUMP_PAIRED = "pump_paired"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GRUNDFOS_ALPHA3_ID): cv.use_id(GrundfosAlpha3),
        cv.Optional(CONF_PUMP_RUNNING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_RUNNING,
            icon="mdi:pump",
        ),
        cv.Optional(CONF_PUMP_PAIRED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:bluetooth-connect",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRUNDFOS_ALPHA3_ID])
    if CONF_PUMP_RUNNING in config:
        bsens = await binary_sensor.new_binary_sensor(config[CONF_PUMP_RUNNING])
        cg.add(parent.set_pump_running_sensor(bsens))
    if CONF_PUMP_PAIRED in config:
        bsens = await binary_sensor.new_binary_sensor(config[CONF_PUMP_PAIRED])
        cg.add(parent.set_pump_paired_sensor(bsens))
