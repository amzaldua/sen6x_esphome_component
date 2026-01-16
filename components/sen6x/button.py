# SPDX-License-Identifier: MIT

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG
from . import Sen6xComponent, CONF_SEN6X_ID

CONF_FAN_CLEANING = "fan_cleaning"
CONF_DEVICE_RESET = "device_reset"
CONF_RESET_PREFERENCES = "reset_preferences"
CONF_FORCE_CO2_CALIBRATION = "force_co2_calibration"
CONF_CO2_FACTORY_RESET = "co2_factory_reset"
CONF_SHT_HEATER = "sht_heater"
CONF_CLEAR_DEVICE_STATUS = "clear_device_status"

sen6x_ns = cg.esphome_ns.namespace("sen6x")
Sen6xButton = sen6x_ns.class_("Sen6xButton", button.Button)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_SEN6X_ID): cv.use_id(Sen6xComponent),
    cv.Optional(CONF_FAN_CLEANING): button.button_schema(
        Sen6xButton,
        icon="mdi:fan-remove",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
    cv.Optional(CONF_DEVICE_RESET): button.button_schema(
        Sen6xButton,
        icon="mdi:restart",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
    cv.Optional(CONF_RESET_PREFERENCES): button.button_schema(
        Sen6xButton,
        icon="mdi:restore-alert",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
    cv.Optional(CONF_FORCE_CO2_CALIBRATION): button.button_schema(
        Sen6xButton,
        icon="mdi:molecule-co2",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
    cv.Optional(CONF_CO2_FACTORY_RESET): button.button_schema(
        Sen6xButton,
        icon="mdi:refresh-circle",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
    cv.Optional(CONF_SHT_HEATER): button.button_schema(
        Sen6xButton,
        icon="mdi:radiator",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
    cv.Optional(CONF_CLEAR_DEVICE_STATUS): button.button_schema(
        Sen6xButton,
        icon="mdi:eraser",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
}

async def to_code(config):
    hub = await cg.get_variable(config[CONF_SEN6X_ID])

    if CONF_FAN_CLEANING in config:
        b = await button.new_button(config[CONF_FAN_CLEANING])
        cg.add(hub.set_fan_cleaning_button(b))

    if CONF_DEVICE_RESET in config:
        b = await button.new_button(config[CONF_DEVICE_RESET])
        cg.add(hub.set_device_reset_button(b))

    if CONF_RESET_PREFERENCES in config:
        b = await button.new_button(config[CONF_RESET_PREFERENCES])
        cg.add(hub.set_reset_preferences_button(b))

    if CONF_FORCE_CO2_CALIBRATION in config:
        b = await button.new_button(config[CONF_FORCE_CO2_CALIBRATION])
        cg.add(hub.set_force_co2_calibration_button(b))

    if CONF_CO2_FACTORY_RESET in config:
        b = await button.new_button(config[CONF_CO2_FACTORY_RESET])
        cg.add(hub.set_co2_factory_reset_button(b))

    if CONF_SHT_HEATER in config:
        b = await button.new_button(config[CONF_SHT_HEATER])
        cg.add(hub.set_sht_heater_button(b))

    if CONF_CLEAR_DEVICE_STATUS in config:
        b = await button.new_button(config[CONF_CLEAR_DEVICE_STATUS])
        cg.add(hub.set_clear_device_status_button(b))

