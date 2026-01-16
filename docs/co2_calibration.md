# CO2 Calibration Guide

This document explains CO2 calibration options for SEN6x sensors.

## Applicable Models

CO2 is available on:
- SEN63C
- SEN66
- SEN69C

---

## Automatic Self-Calibration (ASC)

### What is ASC?

ASC automatically adjusts the CO2 baseline by assuming the sensor is periodically exposed to fresh outdoor air (~400 ppm).

### How it Works

1. Sensor tracks the lowest CO2 readings over time
2. Assumes minimum readings represent outdoor air
3. Gradually adjusts baseline to match 400 ppm

### Requirements

For ASC to function correctly:
- Sensor must be exposed to fresh air at least once per week
- Environment should have periods of low occupancy
- Not suitable for continuously occupied spaces

### Configuration

```yaml
switch:
  - platform: sen6x
    co2_automatic_self_calibration:
      name: "CO2 ASC"
```

ASC is **enabled by default** per Sensirion recommendation.

### When to Disable ASC

Disable ASC when:
- Sensor is in a space that never reaches outdoor CO2 levels
- Using forced recalibration instead
- Testing or troubleshooting

---

## Forced Recalibration (FRC)

### What is FRC?

FRC performs an immediate calibration using a known reference CO2 concentration.

### When to Use FRC

- Initial deployment in a new environment
- After extended periods with ASC disabled
- When readings appear consistently offset
- After sensor has been in storage

### Procedure

1. **Set the reference value**:
   ```yaml
   number:
     - platform: sen6x
       outdoor_co2_reference:
         name: "Outdoor CO2 Reference"
   ```
   
2. **Expose sensor to reference air**:
   - Place sensor outdoors or near open window
   - Wait 3+ minutes for stabilization
   - Typical outdoor CO2: 400–420 ppm

> **Note**: Actual outdoor CO2 concentration varies by location and regional context. When available, using measured data from nearby reference stations (environmental monitoring networks, meteorological stations with CO2 sensors) is preferable to assuming a generic value.

3. **Trigger calibration**:
   ```yaml
   button:
     - platform: sen6x
       force_co2_calibration:
         name: "Force CO2 Calibration"
   ```

4. **Verify results**:
   - CO2 readings should match reference value
   - Calibration is stored in sensor EEPROM

### FRC Requirements

Per Sensirion datasheet:
- Sensor must be in measurement mode
- Reference gas must be stable
- Allow 500ms after command for completion

---

## Pressure Compensation

CO2 readings are affected by ambient pressure. For improved accuracy, provide pressure data from any ESPHome-supported barometric sensor:

```yaml
sen6x:
  pressure_source: barometer_pressure

sensor:
  # Any pressure sensor works: bme280, bmp280, bme680, ms5611, etc.
  - platform: bme280_i2c
    pressure:
      id: barometer_pressure
```

This enables real-time pressure compensation for the CO2 photoacoustic sensor.

> **Note**: Atmospheric pressure varies with weather conditions. Using a local barometric sensor provides more accurate compensation than standard or estimated values.

---

## Factory Reset

To reset CO2 calibration to factory defaults:

```yaml
button:
  - platform: sen6x
    co2_factory_reset:
      name: "CO2 Factory Reset"
```

> **Warning**: This erases all calibration data stored in the sensor EEPROM.

---

## Troubleshooting

### Readings Too High

1. Check ASC is enabled
2. Ensure sensor gets fresh air exposure
3. Perform FRC with known reference

### Readings Too Low

1. Disable ASC temporarily
2. Check pressure compensation
3. Verify sensor placement (avoid direct sunlight)

### Readings Unstable

1. Check for drafts or HVAC interference
2. Increase update interval
3. Allow longer stabilization time

---

## References

- Sensirion SEN6x Datasheet, Section 1.5 "CO2 Specifications"
- Sensirion SEN6x Datasheet, Section 4.8 "CO2 Calibration Commands"
