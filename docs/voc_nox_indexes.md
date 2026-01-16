# Understanding VOC and NOx Indexes

This document explains the VOC Index and NOx Index as implemented in the SEN6x sensor family.

## What is an Index?

The VOC Index and NOx Index are **relative indicators**, not absolute gas concentrations.

| Value | Meaning |
|-------|---------|
| 100 | Average/baseline conditions |
| > 100 | Above-average levels (higher than typical) |
| < 100 | Below-average levels (lower than typical) |
| 1 | Minimum value |
| 500 | Maximum value |

### Why Indexes Instead of Concentrations?

Per Sensirion documentation:

> "Metal-oxide (MOX) gas sensors [...] are limited to non-selective measurements of concentration ratios. [...] MOX sensors cannot distinguish individual VOCs present in the air."

The index approach provides:
- Relative change detection
- Event identification
- Trend monitoring

Without requiring identification of specific gas compounds.

---

## Warm-up vs. Learning Phase

After power-on, the VOC and NOx sensors go through two distinct phases:

| Phase | Duration | Description |
|-------|----------|-------------|
| **Initial stabilization** | Seconds to minutes | Sensor hardware stabilization. Readings may be erratic. |
| **Baseline learning** | 12 hours (default) or 720 hours (WELL/RESET) | Algorithm learns the "normal" environment. |

> **Important**: Warm-up and learning are not the same. A sensor that has completed warm-up may still produce suboptimal readings until the baseline has stabilized.

During warm-up:
- Index values may be unstable
- Readings should not be used for decision-making

---

## Baseline Learning

The SEN6x uses an adaptive algorithm that learns the "normal" baseline of the environment.

### Understanding Baseline Adaptation

> **Critical concept**: The baseline represents the sensor's learned "normal" for the current environment — not necessarily clean air.

This means:
- An environment with chronic VOC sources may have a "normalized" baseline
- **VOC Index = 100 does NOT always mean clean air**
- **VOC Index = 100 means typical conditions for that environment**

### Default Parameters

| Parameter | Default | WELL/RESET |
|-----------|---------|------------|
| Learning time offset | 12 hours | 720 hours |
| Learning time gain | 12 hours | 12 hours |
| Gating duration | 180 min | 180 min |
| Index offset | 100 | 100 |

### WELL Building Standard Configuration

For compliance with WELL Building Standard or RESET Air, use the 720-hour learning time:

```yaml
sensor:
  - platform: sen6x
    voc_index:
      name: "VOC Index"
      algorithm_tuning:
        learning_time_offset_hours: 720
```

This setting:
- Enables monitoring of trends over weeks
- Required for building standard compliance
- Does not affect electrical warm-up time

---

## VOC Index: Typical Triggers

The VOC sensor responds to a wide variety of organic compounds. Common triggers include:

| Source | Typical Response |
|--------|------------------|
| Human presence (breathing, skin emissions) | Moderate increase |
| Cooking | Significant increase |
| Cleaning products | Significant increase |
| New furniture / paint | Sustained elevated levels |
| Cosmetics, deodorants | Transient spikes |

> **Note**: Transient VOC events often correlate with human activity and do not necessarily indicate toxic exposure. The sensor cannot distinguish between harmless and harmful compounds.

---

## NOx Index: Typical Triggers

Based on the sensor's design for detecting oxidizing gases, the NOx sensor typically responds to:

| Source | Typical Response |
|--------|------------------|
| Gas combustion (stoves, heaters) | Significant increase |
| Vehicle exhaust (open windows, garages) | Moderate to high increase |
| Tobacco smoke | Moderate increase |
| Human presence alone | Minimal response |

> **Note**: These are typical observed responses. Unlike VOC, the NOx Index is generally less sensitive to human presence and more indicative of combustion-related events. Refer to Sensirion documentation for official specifications.

---

## Interpreting Combined Patterns

The following are **observational patterns** based on typical sensor behavior, not official Sensirion specifications:

| Pattern | Possible Interpretation |
|---------|-------------------------|
| VOC high + NOx low | Typical human activity, cleaning, or organic sources |
| VOC high + NOx high | Combustion source (cooking, heating, exhaust) |
| NOx sustained high | Poor ventilation or external pollution source |
| VOC spike, quick return | Transient event (spray, perfume, cooking) |

> **Caution**: These are observational patterns derived from typical use cases, not diagnostic rules. Actual conditions may vary significantly. Do not use for safety-critical decisions.

---

## Baseline Persistence

This component automatically persists the VOC algorithm state to flash memory.

### How it Works

1. Algorithm state is saved every 3 hours
2. State is restored on reboot
3. This reduces learning time after power cycles

### Configuration

Baseline persistence is enabled by default. The sensor stores:
- Current baseline values
- Learning algorithm state

---

## Limitations

### Non-Selective Measurement

From Sensirion AppNote:

> "A single MOX sensor generates a single, aggregate output for a VOC mixture. As a result, such MOX sensors cannot distinguish individual VOCs present in the air."

The index cannot determine:
- Which specific VOCs are present
- Whether detected VOCs are harmful or harmless

### Context Dependency

The same index value can represent different situations:

| Scenario | VOC Source | Index |
|----------|------------|-------|
| Cooking | Food vapors (mostly harmless) | ~300 |
| Fresh paint | Solvents (potentially harmful) | ~300 |

Both produce similar index values despite different implications.

### Regulatory Disclaimer

> **Important**: These indexes are provided for informational purposes only. They do not constitute air quality certification, do not replace calibrated measurement equipment, and should not be used as the sole basis for health-related decisions.

---

## Converting to TVOC (Optional)

For applications requiring absolute TVOC values, see the [sensors.md](sensors.md) documentation on `tvoc_well`, `tvoc_reset`, and `tvoc_ethanol` sensors.

These conversions use official formulas from Sensirion AppNote "AQI_BuildingStandards" but should be understood as **estimations** valid under laboratory conditions.

---

## References

- Sensirion SEN6x Datasheet, Section 1.4 "VOC and NOx Specifications"
- Sensirion AppNote: "Compliance of Sensirion's VOC Sensors with Building Standards" (March 2023)
