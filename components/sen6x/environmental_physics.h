// SPDX-License-Identifier: MIT
// SEN6x ESPHome Component - Official Version
// Environmental Physics calculations based on Sensirion AQI_BuildingStandards
// AppNote

#pragma once

#include <cmath>

namespace esphome {
namespace sen6x {

class EnvironmentalPhysics {
public:
  // Calculate TVOC for WELL Building Standard (Molhave equivalent)
  // Based on Sensirion AQI_BuildingStandards AppNote
  static float calculate_well_tvoc(float voc_index) {
    if (voc_index >= 501)
      return 0.0f;
    if (voc_index > 500.9f)
      voc_index = 500.9f;
    return (std::log(501.0f - voc_index) - 6.24f) * -996.94f;
  }

  // Calculate TVOC for RESET Air (Isobutylene equivalent)
  // Based on Sensirion AQI_BuildingStandards AppNote
  static float calculate_reset_tvoc(float voc_index) {
    if (voc_index >= 501)
      return 0.0f;
    if (voc_index > 500.9f)
      voc_index = 500.9f;
    return (std::log(501.0f - voc_index) - 6.24f) * -878.53f;
  }

  // Calculate TVOC Ethanol (ppb)
  // Based on Sensirion AQI_BuildingStandards AppNote
  static float calculate_ethanol_tvoc(float voc_index) {
    if (voc_index >= 501)
      return 0.0f;
    if (voc_index > 500.9f)
      voc_index = 500.9f;
    return (std::log(501.0f - voc_index) - 6.24f) * -381.97f;
  }
};

} // namespace sen6x
} // namespace esphome
