# SPDX-License-Identifier: MIT

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "button", "number", "text_sensor", "binary_sensor", "switch"]

sen6x_ns = cg.esphome_ns.namespace("sen6x")
Sen6xComponent = sen6x_ns.class_(
    "Sen6xComponent", cg.PollingComponent, i2c.I2CDevice
)
Sen6xModel = sen6x_ns.enum("Sen6xModel")
RhtAcceleration = sen6x_ns.struct("RhtAcceleration")

CONF_SEN6X_ID = "sen6x_id"
CONF_PRESSURE_SOURCE = "pressure_source"
CONF_RHT_ACCELERATION = "rht_acceleration"
CONF_RHT_K = "k"
CONF_RHT_P = "p"
CONF_RHT_T1 = "t1"
CONF_RHT_T2 = "t2"

# SEN6x Model Definitions (per Sensirion Datasheet v0.91+)
# Model capabilities: PM, PM4.0, RH/T, VOC, NOx, CO2, HCHO
MODEL_CAPABILITIES = {
    # SEN62: PM + RH/T (basic environmental)
    "SEN62": {"pm": True, "pm4": True, "rht": True, "voc": False, "nox": False, "co2": False, "hcho": False},
    # SEN63C: PM + RH/T + CO2 (CO2 variant)
    "SEN63C": {"pm": True, "pm4": False, "rht": True, "voc": False, "nox": False, "co2": True, "hcho": False},
    # SEN65: PM + RH/T + VOC + NOx (VOC/NOx variant)
    "SEN65": {"pm": True, "pm4": False, "rht": True, "voc": True, "nox": True, "co2": False, "hcho": False},
    # SEN66: PM + RH/T + VOC + NOx + CO2 (full-featured AIQ)
    "SEN66": {"pm": True, "pm4": True, "rht": True, "voc": True, "nox": True, "co2": True, "hcho": False},
    # SEN68: PM + RH/T + VOC + NOx + HCHO (formaldehyde variant)
    "SEN68": {"pm": True, "pm4": False, "rht": True, "voc": True, "nox": True, "co2": False, "hcho": True},
    # SEN69C: PM + RH/T + VOC + NOx + CO2 + HCHO (ultimate variant)
    "SEN69C": {"pm": True, "pm4": True, "rht": True, "voc": True, "nox": True, "co2": True, "hcho": True},
}

# Map model names to C++ enum values (Sen6xModel::VALUE)
MODEL_ENUM_MAP = {
    "SEN62": cg.RawExpression("esphome::sen6x::Sen6xModel::SEN62"),
    "SEN63C": cg.RawExpression("esphome::sen6x::Sen6xModel::SEN63C"),
    "SEN65": cg.RawExpression("esphome::sen6x::Sen6xModel::SEN65"),
    "SEN66": cg.RawExpression("esphome::sen6x::Sen6xModel::SEN66"),
    "SEN68": cg.RawExpression("esphome::sen6x::Sen6xModel::SEN68"),
    "SEN69C": cg.RawExpression("esphome::sen6x::Sen6xModel::SEN69C"),
}

MODELS = list(MODEL_CAPABILITIES.keys())

# RHT Acceleration schema (YAML-only, volatile - applied on each boot)
RHT_ACCELERATION_SCHEMA = cv.Schema({
    cv.Required(CONF_RHT_K): cv.int_range(min=-32768, max=32767),
    cv.Required(CONF_RHT_P): cv.int_range(min=-32768, max=32767),
    cv.Required(CONF_RHT_T1): cv.int_range(min=0, max=65535),
    cv.Required(CONF_RHT_T2): cv.int_range(min=0, max=65535),
})

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Sen6xComponent),
            # Optional external pressure sensor for CO2 compensation
            # Accepts the ID of any ESPHome sensor (BME280, BMP280, etc.)
            cv.Optional(CONF_PRESSURE_SOURCE): cv.use_id(sensor.Sensor),
            # RHT Acceleration parameters (YAML-only, volatile)
            cv.Optional(CONF_RHT_ACCELERATION): RHT_ACCELERATION_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("10s"))
    .extend(i2c.i2c_device_schema(0x6B))
)


def get_model_capabilities(model):
    """Get capabilities for a specific model."""
    return MODEL_CAPABILITIES.get(model, MODEL_CAPABILITIES["SEN66"])


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # Model is auto-detected from sensor at runtime - no set_model() call needed

    # Optional external pressure source for CO2 compensation
    if CONF_PRESSURE_SOURCE in config:
        pressure_sensor = await cg.get_variable(config[CONF_PRESSURE_SOURCE])
        cg.add(var.set_pressure_source(pressure_sensor))

    # RHT Acceleration (YAML-only, volatile - applied on each boot)
    if CONF_RHT_ACCELERATION in config:
        rht = config[CONF_RHT_ACCELERATION]
        rht_struct = cg.StructInitializer(
            RhtAcceleration,
            ("k", rht[CONF_RHT_K]),
            ("p", rht[CONF_RHT_P]),
            ("t1", rht[CONF_RHT_T1]),
            ("t2", rht[CONF_RHT_T2]),
        )
        cg.add(var.set_rht_acceleration(rht_struct))
