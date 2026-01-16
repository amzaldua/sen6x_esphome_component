# SPDX-License-Identifier: MIT
# SEN6x ESPHome Component - Official Version
# Based on Sensirion Datasheet + AQI_BuildingStandards AppNote

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_CO2,
    CONF_HUMIDITY,
    CONF_PM_1_0,
    CONF_PM_2_5,
    CONF_PM_10_0,
    CONF_TEMPERATURE,
    CONF_TVOC,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)

from . import Sen6xComponent, CONF_SEN6X_ID

CONF_PM_4_0 = "pm_4_0"
CONF_VOC_INDEX = "voc_index"
CONF_NOX_INDEX = "nox_index"
CONF_FORMALDEHYDE = "formaldehyde"
CONF_TVOC_WELL = "tvoc_well"
CONF_TVOC_RESET = "tvoc_reset"
CONF_TVOC_ETHANOL = "tvoc_ethanol"
CONF_NC_0_5 = "nc_0_5"
CONF_NC_1_0 = "nc_1_0"
CONF_NC_2_5 = "nc_2_5"
CONF_NC_4_0 = "nc_4_0"
CONF_NC_10_0 = "nc_10_0"
CONF_AMBIENT_PRESSURE = "ambient_pressure"
CONF_SENSOR_ALTITUDE = "sensor_altitude"

# VOC/NOx Algorithm Tuning parameters (6 parameters per Sensirion datasheet)
CONF_ALGORITHM_TUNING = "algorithm_tuning"
CONF_INDEX_OFFSET = "index_offset"
CONF_LEARNING_TIME_OFFSET_HOURS = "learning_time_offset_hours"
CONF_LEARNING_TIME_GAIN_HOURS = "learning_time_gain_hours"
CONF_GATING_MAX_DURATION_MINUTES = "gating_max_duration_minutes"
CONF_STD_INITIAL = "std_initial"
CONF_GAIN_FACTOR = "gain_factor"

# VOC tuning defaults (per Sensirion datasheet)
VOC_DEFAULTS = {
    "index_offset": 100,
    "learning_time_offset_hours": 12,  # 720 for WELL Building Standard
    "learning_time_gain_hours": 12,
    "gating_max_duration_minutes": 180,
    "std_initial": 50,
    "gain_factor": 230,
}

# NOx tuning defaults (gating is longer)
NOX_DEFAULTS = {
    "index_offset": 1,
    "learning_time_offset_hours": 12,
    "learning_time_gain_hours": 12,
    "gating_max_duration_minutes": 720,
    "std_initial": 50,
    "gain_factor": 230,
}

# Algorithm tuning sub-schema (6 parameters per Sensirion datasheet)
ALGORITHM_TUNING_SCHEMA = lambda defaults: cv.Schema({
    cv.Optional(CONF_INDEX_OFFSET, default=defaults["index_offset"]): cv.int_range(min=1, max=250),
    cv.Optional(CONF_LEARNING_TIME_OFFSET_HOURS, default=defaults["learning_time_offset_hours"]): cv.int_range(min=1, max=1000),
    cv.Optional(CONF_LEARNING_TIME_GAIN_HOURS, default=defaults["learning_time_gain_hours"]): cv.int_range(min=1, max=1000),
    cv.Optional(CONF_GATING_MAX_DURATION_MINUTES, default=defaults["gating_max_duration_minutes"]): cv.int_range(min=0, max=3000),
    cv.Optional(CONF_STD_INITIAL, default=defaults["std_initial"]): cv.int_range(min=10, max=5000),
    cv.Optional(CONF_GAIN_FACTOR, default=defaults["gain_factor"]): cv.int_range(min=1, max=1000),
})

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SEN6X_ID): cv.use_id(Sen6xComponent),
        cv.Optional(CONF_PM_1_0): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon="mdi:blur",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_PM1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PM_2_5): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon="mdi:blur",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_PM25,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PM_4_0): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon="mdi:blur",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_PM_10_0): sensor.sensor_schema(
            unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
            icon="mdi:blur",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_PM10,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:water-percent",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon="mdi:thermometer",
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_VOC_INDEX): sensor.sensor_schema(
            icon="mdi:air-filter",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend({
            # Advanced: VOC algorithm tuning (6 parameters per datasheet)
            cv.Optional(CONF_ALGORITHM_TUNING): ALGORITHM_TUNING_SCHEMA(VOC_DEFAULTS),
        }),
        cv.Optional(CONF_NOX_INDEX): sensor.sensor_schema(
            icon="mdi:air-filter",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ).extend({
            # Advanced: NOx algorithm tuning (6 parameters per datasheet)
            cv.Optional(CONF_ALGORITHM_TUNING): ALGORITHM_TUNING_SCHEMA(NOX_DEFAULTS),
        }),
        cv.Optional(CONF_CO2): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            icon="mdi:molecule-co2",
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CARBON_DIOXIDE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_FORMALDEHYDE): sensor.sensor_schema(
            unit_of_measurement="ppb",
            icon="mdi:molecule",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TVOC_WELL): sensor.sensor_schema(
            unit_of_measurement="µg/m³",
            icon="mdi:air-filter",
            accuracy_decimals=0,
            device_class="volatile_organic_compounds",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TVOC_RESET): sensor.sensor_schema(
            unit_of_measurement="µg/m³",
            icon="mdi:air-filter",
            accuracy_decimals=0,
            device_class="volatile_organic_compounds",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TVOC_ETHANOL): sensor.sensor_schema(
            unit_of_measurement="ppb",
            icon="mdi:chemical-weapon",
            accuracy_decimals=0,
            device_class="volatile_organic_compounds",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_NC_0_5): sensor.sensor_schema(
            unit_of_measurement="#/cm³",
            icon="mdi:blur",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_NC_1_0): sensor.sensor_schema(
            unit_of_measurement="#/cm³",
            icon="mdi:blur",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_NC_2_5): sensor.sensor_schema(
            unit_of_measurement="#/cm³",
            icon="mdi:blur",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_NC_4_0): sensor.sensor_schema(
            unit_of_measurement="#/cm³",
            icon="mdi:blur",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_NC_10_0): sensor.sensor_schema(
            unit_of_measurement="#/cm³",
            icon="mdi:blur",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_AMBIENT_PRESSURE): sensor.sensor_schema(
            unit_of_measurement="hPa",
            icon="mdi:gauge",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category="diagnostic",
        ),
        cv.Optional(CONF_SENSOR_ALTITUDE): sensor.sensor_schema(
            unit_of_measurement="m",
            icon="mdi:elevation-rise",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category="diagnostic",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SEN6X_ID])

    if CONF_PM_1_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_1_0])
        cg.add(hub.set_pm_1_0_sensor(sens))
    if CONF_PM_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_PM_2_5])
        cg.add(hub.set_pm_2_5_sensor(sens))
    if CONF_PM_4_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_4_0])
        cg.add(hub.set_pm_4_0_sensor(sens))
    if CONF_PM_10_0 in config:
        sens = await sensor.new_sensor(config[CONF_PM_10_0])
        cg.add(hub.set_pm_10_0_sensor(sens))
    if CONF_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(hub.set_humidity_sensor(sens))
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(hub.set_temperature_sensor(sens))

    if CONF_VOC_INDEX in config:
        voc_config = config[CONF_VOC_INDEX]
        sens = await sensor.new_sensor(voc_config)
        cg.add(hub.set_voc_index_sensor(sens))
        # VOC algorithm tuning (advanced, 6 parameters per datasheet)
        if CONF_ALGORITHM_TUNING in voc_config:
            tuning = voc_config[CONF_ALGORITHM_TUNING]
            cg.add(hub.set_voc_algorithm_tuning(
                tuning[CONF_INDEX_OFFSET],
                tuning[CONF_LEARNING_TIME_OFFSET_HOURS],
                tuning[CONF_LEARNING_TIME_GAIN_HOURS],
                tuning[CONF_GATING_MAX_DURATION_MINUTES],
                tuning[CONF_STD_INITIAL],
                tuning[CONF_GAIN_FACTOR],
            ))

    if CONF_NOX_INDEX in config:
        nox_config = config[CONF_NOX_INDEX]
        sens = await sensor.new_sensor(nox_config)
        cg.add(hub.set_nox_index_sensor(sens))
        # NOx algorithm tuning (advanced, 6 parameters per datasheet)
        if CONF_ALGORITHM_TUNING in nox_config:
            tuning = nox_config[CONF_ALGORITHM_TUNING]
            cg.add(hub.set_nox_algorithm_tuning(
                tuning[CONF_INDEX_OFFSET],
                tuning[CONF_LEARNING_TIME_OFFSET_HOURS],
                tuning[CONF_LEARNING_TIME_GAIN_HOURS],
                tuning[CONF_GATING_MAX_DURATION_MINUTES],
                tuning[CONF_STD_INITIAL],
                tuning[CONF_GAIN_FACTOR],
            ))

    if CONF_CO2 in config:
        sens = await sensor.new_sensor(config[CONF_CO2])
        cg.add(hub.set_co2_sensor(sens))
    if CONF_FORMALDEHYDE in config:
        sens = await sensor.new_sensor(config[CONF_FORMALDEHYDE])
        cg.add(hub.set_formaldehyde_sensor(sens))
    if CONF_TVOC_WELL in config:
        sens = await sensor.new_sensor(config[CONF_TVOC_WELL])
        cg.add(hub.set_tvoc_well_sensor(sens))
    if CONF_TVOC_RESET in config:
        sens = await sensor.new_sensor(config[CONF_TVOC_RESET])
        cg.add(hub.set_tvoc_reset_sensor(sens))
    if CONF_TVOC_ETHANOL in config:
        sens = await sensor.new_sensor(config[CONF_TVOC_ETHANOL])
        cg.add(hub.set_tvoc_ethanol_sensor(sens))
    if CONF_NC_0_5 in config:
        sens = await sensor.new_sensor(config[CONF_NC_0_5])
        cg.add(hub.set_nc_0_5_sensor(sens))
    if CONF_NC_1_0 in config:
        sens = await sensor.new_sensor(config[CONF_NC_1_0])
        cg.add(hub.set_nc_1_0_sensor(sens))
    if CONF_NC_2_5 in config:
        sens = await sensor.new_sensor(config[CONF_NC_2_5])
        cg.add(hub.set_nc_2_5_sensor(sens))
    if CONF_NC_4_0 in config:
        sens = await sensor.new_sensor(config[CONF_NC_4_0])
        cg.add(hub.set_nc_4_0_sensor(sens))
    if CONF_NC_10_0 in config:
        sens = await sensor.new_sensor(config[CONF_NC_10_0])
        cg.add(hub.set_nc_10_0_sensor(sens))
    if CONF_AMBIENT_PRESSURE in config:
        sens = await sensor.new_sensor(config[CONF_AMBIENT_PRESSURE])
        cg.add(hub.set_ambient_pressure_sensor(sens))
    if CONF_SENSOR_ALTITUDE in config:
        sens = await sensor.new_sensor(config[CONF_SENSOR_ALTITUDE])
        cg.add(hub.set_sensor_altitude_sensor(sens))
