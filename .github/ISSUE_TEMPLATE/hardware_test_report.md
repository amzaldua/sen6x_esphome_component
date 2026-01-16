---
name: Hardware Test Report
about: Report testing results for a SEN6x model
title: '[TEST] SEN6x Model: '
labels: hardware-validation
assignees: ''
---

## Hardware Information
- **SEN6x Model**: (e.g., SEN62, SEN63C, SEN65, SEN66, SEN68, SEN69C)
- **Serial Number** (optional):
- **Firmware Version** (from text_sensor):

## Test Environment
- **ESPHome Version**:
- **ESP32 Board**:
- **I2C Configuration**: (pins, pull-up resistors, cable length)

## Test Results

### Sensor Readings
| Sensor | Working | Notes |
|--------|---------|-------|
| PM 1.0 / 2.5 / 4.0 / 10.0 | ✅ / ❌ | |
| Temperature | ✅ / ❌ | |
| Humidity | ✅ / ❌ | |
| VOC Index | ✅ / ❌ | |
| NOx Index | ✅ / ❌ | |
| CO2 | ✅ / ❌ / N/A | |
| Formaldehyde | ✅ / ❌ / N/A | |

### Features Tested
- [ ] Basic sensor readings
- [ ] TVOC estimations (WELL/RESET)
- [ ] Altitude compensation
- [ ] Pressure compensation
- [ ] CO2 calibration (ASC/FRC)
- [ ] Fan cleaning
- [ ] Temperature offset

## I2C Communication
```
# Paste I2C-related logs here (device detection, NACK errors, etc.)
```

## Configuration Used
```yaml
# Paste your YAML configuration here
```

## Additional Notes
Any observations, issues, or suggestions.
