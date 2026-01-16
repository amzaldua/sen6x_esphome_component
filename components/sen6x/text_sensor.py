# SPDX-License-Identifier: MIT

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from . import Sen6xComponent, CONF_SEN6X_ID

CONF_PRODUCT_NAME = "product_name"
CONF_SERIAL_NUMBER = "serial_number"
CONF_STATUS_HEX = "status_hex"
CONF_FIRMWARE_VERSION = "firmware_version"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_SEN6X_ID): cv.use_id(Sen6xComponent),
    cv.Optional(CONF_PRODUCT_NAME): text_sensor.text_sensor_schema(
        icon="mdi:chip",
        entity_category="diagnostic",
    ),
    cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(
        icon="mdi:barcode",
        entity_category="diagnostic",
    ),
    cv.Optional(CONF_STATUS_HEX): text_sensor.text_sensor_schema(
        icon="mdi:list-status",
        entity_category="diagnostic",
    ),
    cv.Optional(CONF_FIRMWARE_VERSION): text_sensor.text_sensor_schema(
        icon="mdi:memory",
        entity_category="diagnostic",
    ),
}

async def to_code(config):
    hub = await cg.get_variable(config[CONF_SEN6X_ID])

    if CONF_PRODUCT_NAME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_PRODUCT_NAME])
        cg.add(hub.set_product_name_text_sensor(sens))

    if CONF_SERIAL_NUMBER in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SERIAL_NUMBER])
        cg.add(hub.set_serial_number_text_sensor(sens))

    if CONF_STATUS_HEX in config:
        sens = await text_sensor.new_text_sensor(config[CONF_STATUS_HEX])
        cg.add(hub.set_status_text_sensor(sens))

    if CONF_FIRMWARE_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_FIRMWARE_VERSION])
        cg.add(hub.set_firmware_version_sensor(sens))

