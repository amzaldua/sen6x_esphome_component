# Home Assistant Presentation Guide

Recommendations for displaying SEN6x sensor data in Home Assistant.

## Sensor Categories

### Primary Sensors (Show by Default)

These sensors provide the most actionable information:

| Sensor | Why Show |
|--------|----------|
| `pm_2_5` | Primary particulate indicator |
| `co2` | Ventilation indicator |
| `temperature` | Comfort parameter |
| `humidity` | Comfort parameter |
| `voc_index` | Activity / air change detection (relative) |

### Secondary Sensors (Show on Demand)

Useful for monitoring but not essential for daily use:

| Sensor | Use Case |
|--------|----------|
| `pm_1_0`, `pm_4_0`, `pm_10_0` | Detailed PM analysis |
| `nox_index` | Combustion / traffic detection |
| `formaldehyde` | New construction/furniture (SEN68 / SEN69C only) |
| `tvoc_well`, `tvoc_reset` | Indicative estimations for building standard reference |

### Diagnostic Sensors (Hide by Default)

Technical sensors for troubleshooting:

| Sensor | Purpose |
|--------|---------|
| `nc_*` | Particle count analysis |
| `ambient_pressure` | Configuration feedback |
| `sensor_altitude` | Configuration feedback |
| Status binary sensors | Error detection |

---

## Dashboard Layout Recommendations

### Minimal Dashboard

Focus on the essential metrics:

```
┌─────────────────────────────────────┐
│     🌡️ Temperature    💧 Humidity   │
│        22.5°C            45%        │
├─────────────────────────────────────┤
│     🌫️ PM2.5          🫁 CO2        │
│      8.3 µg/m³         620 ppm      │
├─────────────────────────────────────┤
│          📈 VOC Index               │
│              105                    │
└─────────────────────────────────────┘
```

### Detailed Dashboard

For users who want full visibility:

```
┌─────────────────────────────────────┐
│          ENVIRONMENTAL              │
│  Temperature: 22.5°C                │
│  Humidity: 45%                      │
│  Dew Point: 10.2°C (calculated)     │
├─────────────────────────────────────┤
│        PARTICULATE MATTER           │
│  PM1.0:  3.2 µg/m³                  │
│  PM2.5:  8.3 µg/m³                  │
│  PM10:  12.1 µg/m³                  │
├─────────────────────────────────────┤
│           GAS SENSORS               │
│  (Indexes are relative indicators)  │
│  CO2:      620 ppm                  │
│  VOC Index: 105                     │
│  NOx Index:  42                     │
└─────────────────────────────────────┘
```

---

## Recommended Entity Categories

Configure these in ESPHome or customize in Home Assistant:

| Category | Sensors |
|----------|---------|
| Default | pm_2_5, temperature, humidity, co2, voc_index |
| Secondary | pm_1_0, pm_4_0, pm_10_0, nox_index, formaldehyde |
| Diagnostic | nc_*, ambient_pressure, sensor_altitude, status sensors |
| Config | altitude_compensation, temperature_offset, outdoor_co2_reference |

---

## Graph Recommendations

### PM2.5 History

- Time range: 24 hours
- Optional warning threshold at 25 µg/m³
- Optional limit at 50 µg/m³

> **Note**: These thresholds are based on commonly used guidelines (e.g., WHO), not defined by Sensirion. Adjust according to local standards.

### CO2 History

- Time range: 24 hours
- Show reference at 400 ppm (outdoor baseline)
- Show ventilation threshold at 1000 ppm

### VOC Index History

- Time range: 24 hours
- Baseline reference at 100
- Note: Values fluctuate normally with activity; short peaks are common and expected

---

## What NOT to Display

Avoid presenting these as primary metrics:

| Avoid | Reason |
|-------|--------|
| Raw status hex | Technical debugging only |
| Number concentration (NC) | Specialized use case |
| TVOC estimations as primary | Estimations, not direct measurements |

---

## Automation Considerations

### Safe Automation Triggers

- pm_2_5 > threshold → Alert or ventilation
- co2 > 1000 → Ventilation reminder
- fan_error = true → Maintenance alert

### Avoid

- Health-related automations based on VOC/NOx Index
- Medical alerts or recommendations
- "Air quality good/bad" assessments
- Composite or score-based automations (e.g., IAQ scores)

---

## References

This guide provides presentation recommendations only. Sensor interpretation should follow official Sensirion documentation and applicable local standards.
