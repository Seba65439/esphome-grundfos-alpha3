import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_POWER,
    CONF_SPEED,
    CONF_ENERGY,
    CONF_VOLTAGE,
    CONF_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_WATT,
    UNIT_KILOWATT_HOURS,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_REVOLUTIONS_PER_MINUTE,
)
from . import GrundfosAlpha3, CONF_GRUNDFOS_ALPHA3_ID

CONF_FLOW = "flow"
CONF_HEAD = "head"
CONF_CURRENT_SETPOINT = "current_setpoint"
CONF_SHAFT_POWER = "shaft_power"
CONF_TEMP_ELECTRONICS = "temp_electronics"
CONF_TEMP_MOTOR = "temp_motor"
CONF_TEMP_LIQUID = "temp_liquid"
CONF_ALARM_CODE = "alarm_code"

UNIT_CUBIC_METERS_PER_HOUR = "m³/h"
UNIT_METER = "m"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GRUNDFOS_ALPHA3_ID): cv.use_id(GrundfosAlpha3),
        cv.Optional(CONF_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SPEED): sensor.sensor_schema(
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:fan",
        ),
        cv.Optional(CONF_FLOW): sensor.sensor_schema(
            unit_of_measurement=UNIT_CUBIC_METERS_PER_HOUR,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:water-pump",
        ),
        cv.Optional(CONF_HEAD): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:arrow-up-down",
        ),
        cv.Optional(CONF_CURRENT_SETPOINT): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:target",
        ),
        cv.Optional(CONF_ENERGY): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CURRENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_AMPERE,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_CURRENT,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SHAFT_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMP_ELECTRONICS): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMP_MOTOR): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMP_LIQUID): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ALARM_CODE): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:alert-circle-outline",
        ),
    }
)

async def to_code(config):
    parent = await cg.get_variable(config[CONF_GRUNDFOS_ALPHA3_ID])
    
    sensors_map = [
        (CONF_POWER, "set_power_sensor"),
        (CONF_SPEED, "set_speed_sensor"),
        (CONF_FLOW, "set_flow_sensor"),
        (CONF_HEAD, "set_head_sensor"),
        (CONF_CURRENT_SETPOINT, "set_current_setpoint_sensor"),
        (CONF_ENERGY, "set_energy_sensor"),
        (CONF_VOLTAGE, "set_voltage_sensor"),
        (CONF_CURRENT, "set_current_sensor"),
        (CONF_SHAFT_POWER, "set_shaft_power_sensor"),
        (CONF_TEMP_ELECTRONICS, "set_temp_electronics_sensor"),
        (CONF_TEMP_MOTOR, "set_temp_motor_sensor"),
        (CONF_TEMP_LIQUID, "set_temp_liquid_sensor"),
        (CONF_ALARM_CODE, "set_alarm_code_sensor"),
    ]
    
    for conf_key, setter_func in sensors_map:
        if conf_key in config:
            sens = await sensor.new_sensor(config[conf_key])
            cg.add(getattr(parent, setter_func)(sens))
