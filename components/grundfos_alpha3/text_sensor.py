import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from . import GrundfosAlpha3, CONF_GRUNDFOS_ALPHA3_ID

CONF_OPERATING_MODE = "operating_mode"
CONF_CONTROL_MODE = "control_mode"
CONF_ALARM_STATUS = "alarm_status"
CONF_PUMP_NAME = "pump_name"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GRUNDFOS_ALPHA3_ID): cv.use_id(GrundfosAlpha3),
        cv.Optional(CONF_OPERATING_MODE): text_sensor.text_sensor_schema(
            icon="mdi:state-machine",
        ),
        cv.Optional(CONF_CONTROL_MODE): text_sensor.text_sensor_schema(
            icon="mdi:tune-vertical",
        ),
        cv.Optional(CONF_ALARM_STATUS): text_sensor.text_sensor_schema(
            icon="mdi:alert",
        ),
        cv.Optional(CONF_PUMP_NAME): text_sensor.text_sensor_schema(
            icon="mdi:tag-outline",
        ),
    }
)

async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRUNDFOS_ALPHA3_ID])
    
    text_sensors_map = [
        (CONF_OPERATING_MODE, "set_operating_mode_text_sensor"),
        (CONF_CONTROL_MODE, "set_control_mode_text_sensor"),
        (CONF_ALARM_STATUS, "set_alarm_status_text_sensor"),
        (CONF_PUMP_NAME, "set_pump_name_text_sensor"),
    ]
    
    for conf_key, setter_func in text_sensors_map:
        if conf_key in config:
            tsens = await text_sensor.new_text_sensor(config[conf_key])
            cg.add(getattr(parent, setter_func)(tsens))
