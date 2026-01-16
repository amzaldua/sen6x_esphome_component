# Changelog: sen6x_esphome_component

Registro de cambios y mejoras realizadas en el componente oficial.
**Mantener sincronizado con proyecto_sen6x cuando aplique.**

---

## [Divergencia] - 2026-01-16

### Estado de Sincronización

> **IMPORTANTE**: Los proyectos han divergido. Este documento registra las diferencias.

| Archivo | sen6x_esphome_component | proyecto_sen6x | Diferencia |
|---------|-------------------------|----------------|------------|
| `sen6x.cpp` | 1659 líneas | 2500 líneas | +840 líneas (extensiones Phase 4/5) |
| `sen6x.h` | ~700 líneas | ~900 líneas | +200 líneas (members extensiones) |
| `sensor.py` | ~250 líneas | 623 líneas | +373 líneas (sensores derivados) |
| `binary_sensor.py` | 85 líneas | 139 líneas | +54 líneas (Phase 4/5 flags) |

### Funcionalidades Divergentes

#### Solo en proyecto_sen6x (NO en componente base):

**Sensores derivados Phase 4/5:**
- `iaq_score`, `mold_risk`, `thermal_comfort`
- `aqi_us`, `aqi_eu`, `rebreathed_air`
- `co2_trend`, `pressure_trend`, `weather_code`
- `migraine_risk`, `joint_pain_risk`, `storm_risk`
- `warmup_time`, `air_density`, `reading_confidence`

**Binary sensors extendidos:**
- `voc_baseline_drift`, `data_anomaly`
- `pressure_drop_event`, `humidity_condensation_risk`, `sustained_voc_event`

**Funcionalidades avanzadas:**
- Buffer de presión 24h con persistencia NVS
- Weather prediction (Zambretti)
- Detección de anomalías de datos

#### Formaldehyde unit discrepancia:
| Proyecto | Unidad | Nota |
|----------|--------|------|
| sen6x_esphome_component | ppb | ✅ Alineado con datasheet |
| proyecto_sen6x | µg/m³ | ⚠️ Convertido, verificar código |

---

## [Documentación] - 2026-01-15

### Cambios de documentación aplicados a sen6x_esphome_component

- `README.md`: Actualizado con Testing Status, Derived Estimations, Referenced Version
- `docs/sensors.md`: Specs alineadas con datasheet (T/RH accuracy, PM precision, CO2 por modelo)
- `docs/voc_nox_indexes.md`: Warm-up vs Learning, patrones observacionales, disclaimers
- `docs/co2_calibration.md`: Notas sobre variabilidad regional CO2 y presión atmosférica
- `docs/home_assistant_presentation.md`: Wording neutral, disclaimers regulatorios

> **NOTA**: Estos cambios de documentación NO existen en proyecto_sen6x (no tiene esta estructura docs/).

---

## [Inicial] - 2026-01-08

### Origen
- Bifurcado desde `proyecto_sen6x` (versión extendida)
- Objetivo: Componente candidato para ESPHome oficial

### Simplificaciones vs proyecto_sen6x

#### Sensores Eliminados
| Sensor | Razón |
|--------|-------|
| `dew_point` | Derivado psicométrico |
| `absolute_humidity` | Derivado psicométrico |
| `heat_index` | Derivado psicométrico |
| `aqi_us`, `aqi_eu` | Derivados EPA/EU |
| `iaq_score` | Custom |
| `mold_risk` | Custom |
| `thermal_comfort` | Custom |
| `rebreathed_air` | Custom |
| `co2_trend` | Custom |
| `pressure_trend` | Barometría avanzada |
| `weather_code` | Barometría avanzada |
| `pressure_sea_level` | Barometría avanzada |
| `air_density` | Barometría avanzada |
| `weather_data_confidence` | Barometría avanzada |
| `migraine_risk` | Barometría avanzada |
| `joint_pain_risk` | Barometría avanzada |
| `storm_risk` | Barometría avanzada |
| `warmup_time` | Custom UX |
| `reading_confidence` | Custom UX |

#### Funcionalidades Eliminadas
- Buffer de presión 24h con persistencia NVS
- Cálculos de EnvironmentalPhysics (excepto TVOC)
- Detección de anomalías de datos
- Weather prediction (Zambretti)

#### Funcionalidades Mantenidas
- Sensores físicos: PM, T, RH, VOC, NOx, CO2, HCHO, NC
- TVOC WELL/RESET/Ethanol (Sensirion AppNote)
- Persistencia VOC/NOx baselines
- Persistencia preferencias (altitude, switches)
- Corrección presión CO2 (pressure_source)
- Buttons, Switches, Numbers

---

## Mejoras a Sincronizar

> Cualquier mejora hecha aquí que deba portarse a proyecto_sen6x

### Pendientes de sincronizar (componente → proyecto)
- [ ] Documentación (estructura /docs) - crear en proyecto_sen6x si se desea
- [ ] Formaldehyde unit verificar (ppb vs µg/m³)
- [ ] Core functions si hay bugfixes

### Ya sincronizadas
- (ninguna por ahora)

---

## Notas de Desarrollo

### Para re-sincronizar core en el futuro:

1. **Identificar funciones core** (I2C commands, read/write, calibration)
2. **Copiar de sen6x_esphome_component → proyecto_sen6x**
3. **Preservar extensiones Phase 4/5** en proyecto_sen6x
4. **Verificar compilación** en ambos proyectos

### Funciones consideradas "core":
- `sen6x_crc`, `read_bytes_`, `read_words_`, `write_command_*`
- `read_device_identity_`, `read_device_status_`, `read_device_configuration_`
- `write_altitude_compensation_`, `set_ambient_pressure`, `write_temperature_offset_`
- `write_voc_algorithm_tuning_`, `write_nox_algorithm_tuning_`
- `perform_forced_co2_calibration_`, `configure_auto_cleaning_`
- `read_measurement_data_` (lectura I2C según modelo)

