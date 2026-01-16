# SEN6x ESPHome Component

🌐 **Languages**: English | [Español](README.es.md) | Other languages (on demand via Issues)

ESPHome component for the **Sensirion SEN6x** environmental sensor family.

## Quick Overview

- Direct ESPHome integration for Sensirion SEN6x sensors
- No custom IAQ scores or health interpretations
- VOC/NOx exposed strictly as Sensirion indexes (relative indicators)
- Optional TVOC estimations per official Sensirion AppNotes
- Tested on SEN66 hardware

## What is this?

This component provides direct integration of Sensirion SEN6x sensors with ESPHome and Home Assistant. It exposes sensor data as documented in the official Sensirion datasheet, with optional TVOC estimations based on the official Sensirion Application Note "Compliance of Sensirion's VOC Sensors with Building Standards" (included in [docs/reference](docs/reference/AQI_BuildingStandards.pdf)).

### Design Principles

- **Direct sensor data**: Core measurements come directly from the SEN6x hardware
- **Documented derivations only**: Additional values are derived strictly using official Sensirion AppNotes
- **No custom algorithms**: No proprietary IAQ scores or health interpretations
- **Transparent behavior**: What you configure is what you get

## Supported Models

| Model   | PM   | T/RH | VOC  | NOx  | CO2  | HCHO |
|---------|:----:|:----:|:----:|:----:|:----:|:----:|
| SEN62   | ✓    | ✓    | –    | –    | –    | –    |
| SEN63C  | ✓    | ✓    | –    | –    | ✓    | –    |
| SEN65   | ✓    | ✓    | ✓    | ✓    | –    | –    |
| SEN66   | ✓    | ✓    | ✓    | ✓    | ✓    | –    |
| SEN68   | ✓    | ✓    | ✓    | ✓    | –    | ✓    |
| SEN69C  | ✓    | ✓    | ✓    | ✓    | ✓    | ✓    |

The model is **auto-detected** from the sensor at runtime.

> **Note**: Feature availability is model-dependent. See [Testing Status](#testing-status) for verification details.

## Hardware Connection

### SEN6x Pinout

| SEN6x Pin | Function | ESP32 Connection |
|-----------|----------|------------------|
| 1 (VDD) | Power supply | 3.3V or 5V |
| 2 (GND) | Ground | GND |
| 3 (SDA) | I2C Data | GPIO21 (or any SDA) |
| 4 (SCL) | I2C Clock | GPIO22 (or any SCL) |
| 5 (SEL) | Interface select | GND (for I2C mode) |
| 6 (NC) | Not connected | – |

> **Note**: The SEN6x operates at 3.3V logic but accepts 5V power supply. I2C lines must not exceed 3.3V logic levels. Pin 5 (SEL) must be connected to GND for I2C mode.

### Recommended Pull-up Resistors

The I2C bus requires pull-up resistors on SDA and SCL lines. Many ESP32 development boards include internal pull-ups, but for reliable operation:
- Use external 4.7kΩ pull-up resistors to 3.3V if experiencing communication issues

> **Important**: Verify the I2C pin assignments for your specific ESP32 board. GPIO21/GPIO22 are typical defaults but may vary. For complete SEN6x electrical specifications, refer to the [official datasheet](docs/reference/datasheet.pdf).

## Installation

Add this to your ESPHome YAML:

```yaml
external_components:
  - source: github://amzaldua/sen6x_esphome_component
    components: [sen6x]

i2c:
  sda: GPIO21
  scl: GPIO22

sen6x:
  id: sen6x_sensor
  address: 0x6B
  update_interval: 10s
```

## Basic Configuration

### Minimal Example

```yaml
sensor:
  - platform: sen6x
    pm_2_5:
      name: "PM 2.5"
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
    voc_index:
      name: "VOC Index"
    co2:
      name: "CO2"
```

### Full Example with All Sensors

```yaml
sen6x:
  id: sen6x_sensor
  address: 0x6B
  update_interval: 10s
  # Optional: External pressure source for CO2 compensation
  pressure_source: bme280_pressure

sensor:
  - platform: sen6x
    # Particulate Matter
    pm_1_0:
      name: "PM 1.0"
    pm_2_5:
      name: "PM 2.5"
    pm_4_0:
      name: "PM 4.0"
    pm_10_0:
      name: "PM 10.0"
    
    # Environmental
    temperature:
      name: "Temperature"
    humidity:
      name: "Humidity"
    
    # Gas Indexes
    voc_index:
      name: "VOC Index"
    nox_index:
      name: "NOx Index"
    
    # CO2 (models with C suffix)
    co2:
      name: "CO2"
    
    # Formaldehyde (SEN68, SEN69C only)
    formaldehyde:
      name: "Formaldehyde"
    
    # TVOC Estimations (based on Sensirion AppNote)
    tvoc_well:
      name: "TVOC WELL"
    tvoc_reset:
      name: "TVOC RESET"
```

## Available Sensors

### Direct Sensor Data

| Sensor | Unit | Description |
|--------|------|-------------|
| `pm_1_0` | µg/m³ | Particulate matter ≤1.0 µm |
| `pm_2_5` | µg/m³ | Particulate matter ≤2.5 µm |
| `pm_4_0` | µg/m³ | Particulate matter ≤4.0 µm |
| `pm_10_0` | µg/m³ | Particulate matter ≤10.0 µm |
| `temperature` | °C | Ambient temperature |
| `humidity` | % | Relative humidity |
| `voc_index` | – | VOC Index (1–500) |
| `nox_index` | – | NOx Index (1–500) |
| `co2` | ppm | CO2 concentration |
| `formaldehyde` | ppb | Formaldehyde concentration (SEN68 / SEN69C only) |
| `nc_0_5` to `nc_10_0` | #/cm³ | Number concentration |

### Derived Estimations (Calculated, not measured)

| Sensor | Unit | Source |
|--------|------|--------|
| `tvoc_well` | µg/m³ | WELL Building Standard (Mølhave equivalent) |
| `tvoc_reset` | µg/m³ | RESET Air Standard (Isobutylene equivalent) |
| `tvoc_ethanol` | ppb | Ethanol equivalent |

> **Note**: These values are not direct sensor measurements. They are calculated using the official formulas from Sensirion Application Note "AQI_BuildingStandards" (March 2023).

## Configuration Options

### CO2 Pressure Compensation

For improved CO2 accuracy, connect an external barometer:

```yaml
sen6x:
  pressure_source: barometer_pressure

# Use any ESPHome-supported pressure sensor:
sensor:
  - platform: bme280_i2c  # or bmp280, bme680, ms5611, etc.
    pressure:
      id: barometer_pressure
```

If not provided, the sensor operates using its internal assumptions without raising errors, but with reduced absolute accuracy.

### Altitude Compensation

```yaml
number:
  - platform: sen6x
    altitude_compensation:
      name: "Altitude"
```

### Temperature Offset

```yaml
number:
  - platform: sen6x
    temperature_offset:
      name: "Temperature Offset"
```

### VOC Algorithm Tuning

For WELL/RESET compliance, use the 720h learning time:

```yaml
sensor:
  - platform: sen6x
    voc_index:
      name: "VOC Index"
      algorithm_tuning:
        learning_time_offset_hours: 720
```

### CO2 Automatic Self-Calibration (ASC)

```yaml
switch:
  - platform: sen6x
    co2_automatic_self_calibration:
      name: "CO2 ASC"
```

### Forced CO2 Recalibration (FRC)

```yaml
button:
  - platform: sen6x
    force_co2_calibration:
      name: "Force CO2 Calibration"

number:
  - platform: sen6x
    outdoor_co2_reference:
      name: "Outdoor CO2 Reference"
```

## Binary Sensors (Device Status)

```yaml
binary_sensor:
  - platform: sen6x
    fan_error:
      name: "Fan Error"
    fan_warning:
      name: "Fan Warning"
    gas_error:
      name: "Gas Error"
    rht_error:
      name: "RHT Error"
    pm_error:
      name: "PM Error"
    laser_error:
      name: "Laser Error"
    fan_cleaning_active:
      name: "Fan Cleaning Active"
```

## Buttons

```yaml
button:
  - platform: sen6x
    fan_cleaning:
      name: "Start Fan Cleaning"
    device_reset:
      name: "Device Reset"
```

## Limitations

### VOC and NOx are Indexes, Not Concentrations

The VOC Index and NOx Index are **relative indicators** (1–500 scale), not absolute gas concentrations. Per Sensirion documentation:

- Index = 100 represents the average baseline
- Index > 100 indicates above-average VOC/NOx levels
- Index < 100 indicates below-average levels

The indexes require an initial warm-up period after power-on and a longer learning phase for optimal baseline stability, as described in the Sensirion documentation.

### TVOC Estimations

The `tvoc_well`, `tvoc_reset`, and `tvoc_ethanol` sensors provide **estimated** TVOC concentrations based on laboratory calibration with ethanol. Per Sensirion AppNote:

> "This approach is only a simplification since real indoor gas compositions may vary significantly over time and from environment to environment."

### No Health Assessments

This component does **not** provide:
- Health risk evaluations
- Air quality scores or ratings
- Medical recommendations

Sensor data should be interpreted by qualified professionals when used for health-related decisions.

## Testing Status

> **Important**: This component has been developed and tested on **SEN66** hardware only.

| Model | Status |
|-------|--------|
| SEN62 | ⚠️ Untested – commands implemented per datasheet, not validated on hardware |
| SEN63C | ⚠️ Untested – commands implemented per datasheet, not validated on hardware |
| SEN65 | ⚠️ Untested – commands implemented per datasheet, not validated on hardware |
| **SEN66** | ✅ **Tested and verified** |
| SEN68 | ⚠️ Untested – not yet available in market |
| SEN69C | ⚠️ Untested – not yet available in market |

### Notes

- All I2C commands have been audited against the official Sensirion datasheet
- Models with formaldehyde (SEN68, SEN69C) are implemented strictly per datasheet specifications
- At the time of writing, SEN68 and SEN69C have limited market availability
- Community testing and feedback for other models is welcome

## Documentation

See the `/docs` folder for detailed information:

- [sensors.md](docs/sensors.md) – Detailed sensor specifications
- [voc_nox_indexes.md](docs/voc_nox_indexes.md) – Understanding VOC/NOx indexes
- [co2_calibration.md](docs/co2_calibration.md) – CO2 calibration guide
- [home_assistant_presentation.md](docs/home_assistant_presentation.md) – Dashboard recommendations

## References

This component was developed using the following official Sensirion documentation (obtained from the SEN69C product page):

| Document | Referenced Version | Included |
|----------|-------------------|----------|
| SEN6x Datasheet | v0.92 (December 2025) | [docs/reference/datasheet.pdf](docs/reference/datasheet.pdf) |
| AQI Building Standards AppNote | v1.1 (September 2023) | [docs/reference/AQI_BuildingStandards.pdf](docs/reference/AQI_BuildingStandards.pdf) |

Online product pages:
- [SEN6x product family](https://sensirion.com/search/products?q=SEN6x)
- [SEN66 (tested model)](https://sensirion.com/products/catalog/SEN66)
- [SEN69C (datasheet source)](https://sensirion.com/products/catalog/SEN69C)

## License

MIT License

## Status

**Stable** – Tested and verified on SEN66 hardware. Other models require community validation.

