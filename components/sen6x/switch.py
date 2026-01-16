# SPDX-License-Identifier: MIT

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG
from . import Sen6xComponent, CONF_SEN6X_ID

CONF_CO2_AUTOMATIC_SELF_CALIBRATION = "co2_automatic_self_calibration"
CONF_AUTO_FAN_CLEANING = "auto_fan_cleaning"
CONF_INTERVAL = "interval"

sen6x_ns = cg.esphome_ns.namespace("sen6x")
Sen6xSwitch = sen6x_ns.class_("Sen6xSwitch", switch.Switch)

# Default interval: 7 days = 604800 seconds
DEFAULT_CLEANING_INTERVAL = 604800

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_SEN6X_ID): cv.use_id(Sen6xComponent),
    # voc_algorithm_tuning removed - now in sensor/voc_index/algorithm_tuning
    cv.Optional(CONF_CO2_AUTOMATIC_SELF_CALIBRATION): switch.switch_schema(
        Sen6xSwitch,
        icon="mdi:autorenew",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
    cv.Optional(CONF_AUTO_FAN_CLEANING): switch.switch_schema(
        Sen6xSwitch,
        icon="mdi:fan-auto",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ).extend({
        # Interval for auto-cleaning (default 7 days)
        cv.Optional(CONF_INTERVAL, default="7d"): cv.positive_time_period_seconds,
    }),
}

async def to_code(config):
    hub = await cg.get_variable(config[CONF_SEN6X_ID])

    if CONF_CO2_AUTOMATIC_SELF_CALIBRATION in config:
        s = await switch.new_switch(config[CONF_CO2_AUTOMATIC_SELF_CALIBRATION])
        cg.add(hub.set_co2_asc_switch(s))

    if CONF_AUTO_FAN_CLEANING in config:
        auto_clean_config = config[CONF_AUTO_FAN_CLEANING]
        s = await switch.new_switch(auto_clean_config)
        cg.add(hub.set_auto_cleaning_switch(s))
        # Pass interval in milliseconds
        interval_seconds = auto_clean_config[CONF_INTERVAL].total_seconds
        cg.add(hub.set_auto_cleaning_interval(int(interval_seconds * 1000)))

