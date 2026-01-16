# SPDX-License-Identifier: MIT

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG, UNIT_METER, UNIT_CELSIUS
from . import Sen6xComponent, CONF_SEN6X_ID

CONF_ALTITUDE_COMPENSATION = "altitude_compensation"
CONF_AMBIENT_PRESSURE_COMPENSATION = "ambient_pressure_compensation"
CONF_TEMPERATURE_OFFSET = "temperature_offset"
CONF_OUTDOOR_CO2_REFERENCE = "outdoor_co2_reference"
CONF_NORMALIZED_OFFSET_SLOPE = "normalized_offset_slope"
CONF_TIME_CONSTANT = "time_constant"

sen6x_ns = cg.esphome_ns.namespace("sen6x")
Sen6xNumber = sen6x_ns.class_("Sen6xNumber", number.Number)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_SEN6X_ID): cv.use_id(Sen6xComponent),
    cv.Optional(CONF_ALTITUDE_COMPENSATION): number.number_schema(
        Sen6xNumber,
        icon="mdi:altimeter",
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_METER,
    ).extend({
        cv.Optional("mode", default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
        cv.Optional("restore_value", default=True): cv.boolean,
    }),
    cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION): number.number_schema(
        Sen6xNumber,
        icon="mdi:gauge",
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement="hPa",
    ).extend({
        cv.Optional("mode", default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
    }),
    cv.Optional(CONF_TEMPERATURE_OFFSET): number.number_schema(
        Sen6xNumber,
        icon="mdi:thermometer-lines",
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_CELSIUS,
    ).extend({
        cv.Optional("mode", default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
        # Advanced: temperature compensation parameters (slope + time_constant)
        cv.Optional(CONF_NORMALIZED_OFFSET_SLOPE, default=0.0): cv.float_range(min=-1.0, max=1.0),
        cv.Optional(CONF_TIME_CONSTANT, default=0): cv.int_range(min=0, max=65535),
    }),
    cv.Optional(CONF_OUTDOOR_CO2_REFERENCE): number.number_schema(
        Sen6xNumber,
        icon="mdi:molecule-co2",
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement="ppm",
    ).extend({
        cv.Optional("mode", default="BOX"): cv.enum(number.NUMBER_MODES, upper=True),
    }),
}

async def to_code(config):
    hub = await cg.get_variable(config[CONF_SEN6X_ID])

    if CONF_ALTITUDE_COMPENSATION in config:
        n = await number.new_number(config[CONF_ALTITUDE_COMPENSATION], min_value=0, max_value=3000, step=1)
        cg.add(hub.set_altitude_compensation_number(n))

    if CONF_AMBIENT_PRESSURE_COMPENSATION in config:
        n = await number.new_number(config[CONF_AMBIENT_PRESSURE_COMPENSATION], min_value=0, max_value=1100, step=1)
        cg.add(hub.set_ambient_pressure_compensation_number(n))

    if CONF_TEMPERATURE_OFFSET in config:
        temp_offset_config = config[CONF_TEMPERATURE_OFFSET]
        n = await number.new_number(temp_offset_config, min_value=-10, max_value=10, step=0.1)
        cg.add(hub.set_temperature_offset_number(n))
        # Pass advanced compensation parameters if configured
        slope = temp_offset_config.get(CONF_NORMALIZED_OFFSET_SLOPE, 0.0)
        time_const = temp_offset_config.get(CONF_TIME_CONSTANT, 0)
        if slope != 0.0 or time_const != 0:
            cg.add(hub.set_temperature_compensation(0.0, slope, time_const))

    if CONF_OUTDOOR_CO2_REFERENCE in config:
        n = await number.new_number(config[CONF_OUTDOOR_CO2_REFERENCE], min_value=350, max_value=500, step=1)
        cg.add(hub.set_outdoor_co2_reference_number(n))
