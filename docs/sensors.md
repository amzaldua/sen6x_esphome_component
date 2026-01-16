# Sensor Reference

This document describes each sensor available in the SEN6x ESPHome component.

## Particulate Matter (PM)

### Overview

The SEN6x uses laser scattering to measure particulate matter in the air. Particles are classified by aerodynamic diameter.

| Sensor | Size Range | Unit | Precision (typical) |
|--------|------------|------|---------------------|
| `pm_1_0` | ≤1.0 µm | µg/m³ | ±(5 µg/m³ + 5% m.v.) @ 0–100 µg/m³ |
| `pm_2_5` | ≤2.5 µm | µg/m³ | ±(5 µg/m³ + 5% m.v.) @ 0–100 µg/m³ |
| `pm_4_0` | ≤4.0 µm | µg/m³ | ±25 µg/m³ @ 0–100 µg/m³ |
| `pm_10_0` | ≤10.0 µm | µg/m³ | ±25 µg/m³ @ 0–100 µg/m³ |

> **Note**: "m.v." = measured value. These are precision specifications under laboratory conditions as defined in the Sensirion datasheet. For 100–1000 µg/m³, precision is ±10% m.v. for PM1/PM2.5 and ±25% m.v. for PM4/PM10.

### Number Concentration (NC)

For applications requiring particle count instead of mass:

| Sensor | Size Range | Unit |
|--------|------------|------|
| `nc_0_5` | ≥0.5 µm | #/cm³ |
| `nc_1_0` | ≥1.0 µm | #/cm³ |
| `nc_2_5` | ≥2.5 µm | #/cm³ |
| `nc_4_0` | ≥4.0 µm | #/cm³ |
| `nc_10_0` | ≥10.0 µm | #/cm³ |

> **Note**: Number concentration values are derived from the mass concentration measurements using Sensirion's internal algorithms. Refer to the datasheet for precision specifications.

### Typical Values

| Environment | PM2.5 Range |
|-------------|-------------|
| Clean indoor air | 0–10 µg/m³ |
| Urban indoor | 10–25 µg/m³ |
| Cooking/smoking | 50–200+ µg/m³ |

---

## Temperature and Humidity

| Sensor | Unit | Range | Accuracy (typical) | Conditions |
|--------|------|-------|--------------------| -----------|
| `temperature` | °C | –10 to +50°C | ±0.45°C (typ.), ±0.7°C (max) | @ 15–30°C, 50%RH |
| `humidity` | %RH | 0–100% | ±4.5%RH (typ.), ±6%RH (max) | @ 25°C, 30–70%RH |

### Notes

- Temperature readings may be affected by self-heating of the sensor module
- Use `temperature_offset` to compensate for enclosure effects
- The SEN6x includes internal compensation algorithms (STAR Engine)
- Accuracy specifications are valid under conditions defined in the datasheet

---

## VOC Index

| Sensor | Unit | Range |
|--------|------|-------|
| `voc_index` | – | 1–500 |

### What it Measures

The VOC Index measures the aggregate level of volatile organic compounds relative to a learned baseline.

- **Index = 100**: Average/baseline conditions
- **Index > 100**: Above-average VOC levels
- **Index < 100**: Below-average VOC levels

### Important Notes

- This is a **relative index**, not an absolute concentration
- Requires an initial stabilization period after power-on, plus a longer learning phase (hours to days) for optimal baseline stability
- The baseline adapts over time (learning algorithm)
- Transient VOC Index peaks are commonly caused by human presence (breathing, skin emissions, clothing) and do not necessarily indicate harmful air quality
- See [voc_nox_indexes.md](voc_nox_indexes.md) for detailed interpretation guidance

---

## NOx Index

| Sensor | Unit | Range |
|--------|------|-------|
| `nox_index` | – | 1–500 |

### What it Measures

The NOx Index measures the aggregate level of nitrogen oxides relative to a learned baseline.

### Important Notes

- Same relative interpretation as VOC Index (100 = baseline)
- Separate learning algorithm from VOC
- Compared to VOC Index, NOx Index is less sensitive to human presence and more sensitive to combustion-related sources such as gas appliances, traffic emissions, or outdoor pollution ingress
- See [voc_nox_indexes.md](voc_nox_indexes.md) for detailed interpretation guidance

---

## CO2

| Sensor | Unit | Range |
|--------|------|-------|
| `co2` | ppm | 400–5000 ppm (SEN66), 380–32000 ppm (SEN63C/SEN69C) |

### Accuracy

CO2 accuracy varies by model and concentration range:

**SEN66:**
| Range | Accuracy |
|-------|----------|
| 400–1000 ppm | ±(50 ppm + 2.5% m.v.) |
| 1001–2000 ppm | ±(50 ppm + 3% m.v.) |
| 2001–5000 ppm | ±(40 ppm + 5% m.v.) |

**SEN63C / SEN69C:**
| Range | Accuracy |
|-------|----------|
| 400–5000 ppm | ±(100 ppm + 10% m.v.) |

> **Note**: Refer to the official Sensirion datasheet for complete specifications, conditions, and accuracy guarantees.

### Available Models

CO2 is only available on models with "C" suffix:
- SEN63C
- SEN66
- SEN69C

### Calibration

- **ASC (Automatic Self-Calibration)**: Enabled by default. Requires exposure to fresh air (~400 ppm) at least once per week.
- **FRC (Forced Recalibration)**: Manual calibration with known reference concentration.

See [co2_calibration.md](co2_calibration.md) for details.

### Pressure Compensation

CO2 readings can be improved by providing ambient pressure:

```yaml
sen6x:
  pressure_source: barometer_sensor
```

---

## Formaldehyde (HCHO)

| Sensor | Unit | Range | Accuracy (typical) |
|--------|------|-------|---------------------|
| `formaldehyde` | ppb | 0–2000 ppb | ±20 ppb or ±20% m.v. (the larger) @ 0–200 ppb |

### Available Models

Formaldehyde is only available on:
- SEN68
- SEN69C

### Notes

- Electrochemical measurement, primarily sensitive to formaldehyde
- Resolution: 0.1 ppb
- Typical start-up time: 10 minutes until output is within specifications
- No baseline learning required

### Cross-Sensitivity

Per Sensirion datasheet:
- Cross-sensitivity to ethanol: 0.3% (15 ppb equivalent) when tested at 5 ppm ethanol

> **Note**: In environments with significant ethanol presence, readings may be affected.

---

## TVOC Estimations

These sensors provide **estimated** TVOC concentrations based on the VOC Index, using formulas from Sensirion AppNote "AQI_BuildingStandards".

| Sensor | Unit | Standard |
|--------|------|----------|
| `tvoc_well` | µg/m³ | WELL Building Standard (Mølhave equivalent) |
| `tvoc_reset` | µg/m³ | RESET Air (Isobutylene equivalent) |
| `tvoc_ethanol` | ppb | Ethanol equivalent |

### Formulas (from Sensirion AppNote)

```
TVOC_ethanol [ppb] = (ln(501 - VOC_Index) - 6.24) × (-381.97)
TVOC_well [µg/m³] = (ln(501 - VOC_Index) - 6.24) × (-996.94)
TVOC_reset [µg/m³] = (ln(501 - VOC_Index) - 6.24) × (-878.53)
```

### Limitations

These values are **derived estimations**, not direct measurements.

> **Important**: These estimations are derived exclusively from the VOC Index and do not represent a direct measurement of total VOC concentration. They should not be compared to PID-based or laboratory-grade (GC-MS) VOC measurements.

From Sensirion AppNote:

> "This approach is only a simplification since real indoor gas compositions may vary significantly over time and from environment to environment."

### Regulatory Disclaimer

These values **do not certify compliance** with WELL Building Standard or RESET Air. They are provided for indicative and comparative purposes only. Actual certification requires accredited measurement equipment and procedures.
