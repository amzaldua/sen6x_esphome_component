# SPDX-License-Identifier: MIT

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import Sen6xComponent, CONF_SEN6X_ID

CONF_FAN_ERROR = "fan_error"
CONF_FAN_WARNING = "fan_warning"
CONF_GAS_ERROR = "gas_error"
CONF_RHT_ERROR = "rht_error"
CONF_PM_ERROR = "pm_error"
CONF_LASER_ERROR = "laser_error"
CONF_FAN_CLEANING_ACTIVE = "fan_cleaning_active"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_SEN6X_ID): cv.use_id(Sen6xComponent),
    cv.Optional(CONF_FAN_ERROR): binary_sensor.binary_sensor_schema(
        device_class="problem",
        entity_category="diagnostic",
        icon="mdi:fan-alert",
    ),
    cv.Optional(CONF_FAN_WARNING): binary_sensor.binary_sensor_schema(
        device_class="problem",
        entity_category="diagnostic",
        icon="mdi:fan-alert",
    ),
    cv.Optional(CONF_GAS_ERROR): binary_sensor.binary_sensor_schema(
        device_class="problem",
        entity_category="diagnostic",
        icon="mdi:gas-cylinder-alert",
    ),
    cv.Optional(CONF_RHT_ERROR): binary_sensor.binary_sensor_schema(
        device_class="problem",
        entity_category="diagnostic",
        icon="mdi:thermometer-alert",
    ),
    cv.Optional(CONF_PM_ERROR): binary_sensor.binary_sensor_schema(
        device_class="problem",
        entity_category="diagnostic",
        icon="mdi:air-filter",
    ),
    cv.Optional(CONF_LASER_ERROR): binary_sensor.binary_sensor_schema(
        device_class="problem",
        entity_category="diagnostic",
        icon="mdi:laser-pointer",
    ),
    cv.Optional(CONF_FAN_CLEANING_ACTIVE): binary_sensor.binary_sensor_schema(
        entity_category="diagnostic",
        icon="mdi:broom",
    ),
}

async def to_code(config):
    hub = await cg.get_variable(config[CONF_SEN6X_ID])

    if CONF_FAN_ERROR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_FAN_ERROR])
        cg.add(hub.set_fan_error_binary_sensor(sens))

    if CONF_FAN_WARNING in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_FAN_WARNING])
        cg.add(hub.set_fan_warning_binary_sensor(sens))

    if CONF_GAS_ERROR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_GAS_ERROR])
        cg.add(hub.set_gas_error_binary_sensor(sens))

    if CONF_RHT_ERROR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_RHT_ERROR])
        cg.add(hub.set_rht_error_binary_sensor(sens))
    
    if CONF_PM_ERROR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_PM_ERROR])
        cg.add(hub.set_pm_error_binary_sensor(sens))
    
    if CONF_LASER_ERROR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_LASER_ERROR])
        cg.add(hub.set_laser_error_binary_sensor(sens))

    if CONF_FAN_CLEANING_ACTIVE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_FAN_CLEANING_ACTIVE])
        cg.add(hub.set_fan_cleaning_active_binary_sensor(sens))

