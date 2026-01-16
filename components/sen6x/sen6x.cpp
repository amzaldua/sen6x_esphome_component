// SPDX-License-Identifier: MIT

#include "sen6x.h"
#include "environmental_physics.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <ctime>
#include <sys/stat.h>

// ESP-IDF NVS for pressure buffer persistence (fopen doesn't work without
// mounted filesystem)
#ifdef USE_ESP_IDF
#include <nvs.h>
#include <nvs_flash.h>
#endif

namespace esphome {
namespace sen6x {

using namespace esphome::sensor;
using namespace esphome::text_sensor;
using namespace esphome::binary_sensor;

static const char *const TAG = "sen6x";

uint8_t sen6x_crc(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if ((crc & 0x80) != 0)
        crc = (uint8_t)((crc << 1) ^ 0x31);
      else
        crc <<= 1;
    }
  }
  return crc;
}

void Sen6xComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SEN6x...");

  // Force Stop Measurement to ensure Idle Mode for configuration
  // This handles cases where ESP32 resets but Sensor is still running
  // Datasheet requires > 1400ms after stop command
  this->write_command_(SEN6X_CMD_STOP_MEASUREMENT);
  delay(1500); // Allow sensor to stop and enter Idle Mode

  // Read Serial Number FIRST to generate preference hash
  // This ensures unique preference storage per sensor (same as SEN5x official)
  uint8_t serial_buffer[32];
  if (this->read_bytes_(SEN6X_CMD_GET_SERIAL_NUMBER, serial_buffer, 32)) {
    // Use first 4 bytes for hash (creates unique ID per sensor)
    this->preference_hash_ = (serial_buffer[0] << 24) |
                             (serial_buffer[1] << 16) |
                             (serial_buffer[2] << 8) | serial_buffer[3];
    ESP_LOGI(TAG, "Serial-based preference hash: 0x%08X",
             (unsigned int)this->preference_hash_);
  } else {
    // Fallback to fixed hash if serial read fails (backward compatible)
    this->preference_hash_ = 0x6181DEAD;
    ESP_LOGW(TAG, "Failed to read serial, using fallback preference hash");
  }

  // Apply Volatile Configuration (if enabled)
  // Must be done in Idle Mode (before measurement starts)
  // 1. Initialize Preferences & Restore Configuration

  // VOC Tuning (now YAML-only, no switch, no persistence)
  // Apply VOC tuning parameters from YAML if configured
  if (this->voc_tuning_.has_value()) {
    GasTuning tuning = this->voc_tuning_.value();
    ESP_LOGI(TAG, "Applying VOC tuning from YAML: offset=%d, learning=%dh",
             tuning.index_offset, tuning.learning_time_gain_hours);
    this->write_voc_algorithm_tuning_(tuning);
  }

  // Apply NOx tuning if configured from YAML
  if (this->nox_tuning_.has_value()) {
    GasTuning tuning = this->nox_tuning_.value();
    ESP_LOGI(TAG, "Applying NOx tuning from YAML: offset=%d, learning=%dh",
             tuning.index_offset, tuning.learning_time_gain_hours);
    this->write_nox_algorithm_tuning_(tuning);
  }

  // Apply RHT Acceleration if configured from YAML
  // Volatile - must be applied on each boot before start_measurement
  if (this->rht_acceleration_.has_value()) {
    RhtAcceleration rht = this->rht_acceleration_.value();
    this->write_rht_acceleration_(rht);
  }

  // VOC Baseline Persistence (same as SEN5x official)
  // Restore saved VOC algorithm state for faster startup (skips 12h+ learning)
  if (this->store_baseline_) {
    this->voc_baseline_preference_ =
        global_preferences->make_preference<Sen6xBaselines>(
            this->preference_hash_ + 2, true);

    if (this->voc_baseline_preference_.load(&this->voc_baselines_storage_)) {
      ESP_LOGI(TAG, "Loaded VOC baseline state0: 0x%08X, state1: 0x%08X",
               (unsigned int)this->voc_baselines_storage_.state0,
               (unsigned int)this->voc_baselines_storage_.state1);

      if (this->voc_baselines_storage_.state0 != 0 &&
          this->voc_baselines_storage_.state1 != 0) {
        // Write VOC algorithm state to sensor (must be in Idle mode)
        // Command 0x6181: TX 8 bytes (4 x uint16 with CRC)
        uint16_t states[4];
        states[0] = (this->voc_baselines_storage_.state0 >> 16) & 0xFFFF;
        states[1] = this->voc_baselines_storage_.state0 & 0xFFFF;
        states[2] = (this->voc_baselines_storage_.state1 >> 16) & 0xFFFF;
        states[3] = this->voc_baselines_storage_.state1 & 0xFFFF;

        // Write command with data (4 words = 8 bytes + CRC)
        uint8_t cmd_buffer[2 + 12]; // 2 cmd + 4*(2 data + 1 crc)
        cmd_buffer[0] = SEN6X_CMD_VOC_ALGORITHM_STATE >> 8;
        cmd_buffer[1] = SEN6X_CMD_VOC_ALGORITHM_STATE & 0xFF;
        for (int i = 0; i < 4; i++) {
          cmd_buffer[2 + i * 3] = states[i] >> 8;
          cmd_buffer[2 + i * 3 + 1] = states[i] & 0xFF;
          cmd_buffer[2 + i * 3 + 2] = sen6x_crc(&cmd_buffer[2 + i * 3], 2);
        }

        if (this->write(cmd_buffer, sizeof(cmd_buffer)) == i2c::ERROR_OK) {
          ESP_LOGI(TAG, "Restored VOC algorithm state from NVS");
        } else {
          ESP_LOGW(TAG, "Failed to restore VOC algorithm state");
        }
        delay(20); // Per datasheet execution time
      }
    }
    this->seconds_since_last_baseline_store_ = 0;
  }

  // CO2 ASC
  this->co2_asc_preference_ =
      global_preferences->make_preference<bool>(this->preference_hash_ + 3);
  bool co2_asc_state = true; // Default ON (Datasheet)
  if (this->co2_asc_preference_.load(&co2_asc_state)) {
    ESP_LOGI(TAG, "Restored CO2 ASC: %s", co2_asc_state ? "ON" : "OFF");
  }
  this->write_co2_asc_(co2_asc_state);
  if (this->co2_asc_switch_ != nullptr) {
    this->co2_asc_switch_->publish_state(co2_asc_state);
  }

  // Auto Fan Cleaning (switch in HA, persisted, interval from YAML)
  this->auto_cleaning_preference_ =
      global_preferences->make_preference<bool>(this->preference_hash_ + 4);
  bool auto_clean_state = false; // Default OFF
  if (this->auto_cleaning_preference_.load(&auto_clean_state)) {
    ESP_LOGI(TAG, "Restored Auto Cleaning: %s",
             auto_clean_state ? "ON" : "OFF");
  }
  if (auto_clean_state) {
    this->configure_auto_cleaning_(true);
  }
  if (this->auto_cleaning_switch_ != nullptr) {
    this->auto_cleaning_switch_->publish_state(auto_clean_state);
  }

  // ========== IDLE-MODE CONFIGURATION (apply before Start Measurement)
  // ========== Commands that ONLY work in Idle Mode: Altitude, VOC Tuning,
  // CO2 ASC

  // Altitude - IMMEDIATE APPLICATION (Idle mode only per datasheet 4.8.38)
  this->altitude_preference_ =
      global_preferences->make_preference<float>(this->preference_hash_ + 5);
  float restored_altitude = NAN; // Initialize to NAN to detect if load fails
  bool has_altitude_pref = this->altitude_preference_.load(&restored_altitude);

  // Store for later diagnostic logging (setup logs happen before API
  // connects)
  this->pending_altitude_ = has_altitude_pref ? restored_altitude : NAN;

  if (has_altitude_pref && !std::isnan(restored_altitude)) {
    // Apply immediately in Idle mode (before Start Measurement)
    ESP_LOGI(TAG, "Applying Altitude from NVS: %.1f m", restored_altitude);
    if (this->write_altitude_compensation_(restored_altitude)) {
      ESP_LOGI(TAG, "Altitude write SUCCESS");
      if (this->altitude_compensation_number_ != nullptr) {
        this->altitude_compensation_number_->publish_state(restored_altitude);
      }
      // Verification read to confirm value was applied
      uint16_t verify_data;
      if (this->read_words_(SEN6X_CMD_GET_SENSOR_ALTITUDE, &verify_data, 1)) {
        float verified = (int16_t)verify_data;
        ESP_LOGI(TAG, "Altitude verification: sensor reports %.1f m", verified);
        this->pending_altitude_ = verified; // Update with verified value
      }
    } else {
      ESP_LOGW(TAG, "Altitude write FAILED");
    }
  } else {
    // No preference or NAN: read from device
    ESP_LOGI(TAG, "No valid Altitude preference (nan=%s), reading from device",
             std::isnan(restored_altitude) ? "true" : "false");
    uint16_t data;
    if (this->read_words_(SEN6X_CMD_GET_SENSOR_ALTITUDE, &data, 1)) {
      float value = (int16_t)data;
      ESP_LOGI(TAG, "Read Altitude from device: %.1f m", value);
      this->pending_altitude_ = value;
      if (this->altitude_compensation_number_ != nullptr) {
        this->altitude_compensation_number_->publish_state(value);
      }
    }
  }
  this->altitude_restored_ = true; // Mark as done (no deferred needed)

  // ========== START MEASUREMENT ==========
  if (this->start_measurement_()) {
    ESP_LOGI(TAG, "Measurement started.");
    this->read_device_identity_();
    this->read_device_configuration_();
  } else {
    ESP_LOGE(TAG, "Failed to start measurement!");
    this->mark_failed();
    return;
  }

  // ========== MEASUREMENT-MODE CONFIGURATION (apply after Start) ==========
  // Commands that work in BOTH Idle and Measurement: Temp Offset, Pressure
  // No stop/start needed - write directly!

  // Ambient Pressure (works in Measurement mode per datasheet 4.8.36)
  this->ambient_pressure_preference_ =
      global_preferences->make_preference<float>(this->preference_hash_ + 6);
  float restored_pressure = 0;
  if (this->ambient_pressure_preference_.load(&restored_pressure) &&
      !std::isnan(restored_pressure)) {
    ESP_LOGI(TAG, "Applying Pressure during Measurement: %.1f hPa",
             restored_pressure);
    if (this->write_ambient_pressure_compensation_(restored_pressure)) {
      if (this->ambient_pressure_compensation_number_ != nullptr) {
        this->ambient_pressure_compensation_number_->publish_state(
            restored_pressure);
      }
    }
  } else {
    uint16_t data;
    if (this->read_words_(SEN6X_CMD_GET_AMBIENT_PRESSURE, &data, 1)) {
      float value = (int16_t)data;
      ESP_LOGI(TAG, "Read Pressure from device: %.1f hPa", value);
      if (this->ambient_pressure_compensation_number_ != nullptr) {
        this->ambient_pressure_compensation_number_->publish_state(value);
      }
    }
  }

  // Temperature Offset (works in Measurement mode per datasheet 4.8.14)
  this->temperature_offset_preference_ =
      global_preferences->make_preference<float>(this->preference_hash_ + 7);
  float restored_offset = 0;
  if (this->temperature_offset_preference_.load(&restored_offset) &&
      !std::isnan(restored_offset)) {
    ESP_LOGI(TAG, "Applying Temp Offset during Measurement: %.2f C",
             restored_offset);
    if (this->write_temperature_offset_(restored_offset)) {
      if (this->temperature_offset_number_ != nullptr) {
        this->temperature_offset_number_->publish_state(restored_offset);
      }
    }
  } else {
    uint16_t data;
    if (this->read_words_(SEN6X_CMD_SET_TEMP_OFFSET, &data, 1)) {
      float value = (int16_t)data / 200.0f;
      ESP_LOGI(TAG, "Read Temp Offset from device: %.2f C", value);
      if (this->temperature_offset_number_ != nullptr) {
        this->temperature_offset_number_->publish_state(value);
      }
    }
  }
  this->temp_offset_restored_ = true; // Mark as done (no deferred needed)

  // Apply full temperature compensation if configured from YAML (slope +
  // time_constant)
  if (this->temperature_compensation_.has_value()) {
    this->write_temperature_compensation_(
        this->temperature_compensation_.value());
  }

  // Outdoor CO2 Reference (for rebreathed_air calculation AND FRC
  // calibration)
  this->outdoor_co2_reference_preference_ =
      global_preferences->make_preference<float>(this->preference_hash_ + 8);
  float restored_co2_ref = 400.0f;
  if (this->outdoor_co2_reference_preference_.load(&restored_co2_ref)) {
    ESP_LOGI(TAG, "Restored Outdoor CO2 Reference from NVS: %.0f ppm",
             restored_co2_ref);
    this->outdoor_co2_ppm_ = restored_co2_ref;
  }
  if (this->outdoor_co2_reference_number_ != nullptr) {
    this->outdoor_co2_reference_number_->publish_state(this->outdoor_co2_ppm_);
    this->outdoor_co2_reference_number_->set_control_callback(
        [this](float value) {
          ESP_LOGI(TAG, "Setting Outdoor CO2 Reference: %.0f ppm", value);
          this->outdoor_co2_ppm_ = value;
          this->outdoor_co2_reference_preference_.save(&value);
          this->outdoor_co2_reference_number_->publish_state(value);
        });
  }

  // Register Button Callbacks
  if (this->fan_cleaning_button_ != nullptr) {
    this->fan_cleaning_button_->set_press_callback(
        [this]() { this->start_fan_cleaning_(); });
  }
  if (this->device_reset_button_ != nullptr) {
    this->device_reset_button_->set_press_callback(
        [this]() { this->execute_device_reset_(); });
  }
  if (this->reset_preferences_button_ != nullptr) {
    this->reset_preferences_button_->set_press_callback(
        [this]() { this->execute_preferences_reset_(); });
  }

  // FRC button: Forced CO2 Recalibration (uses outdoor_co2_ppm as reference)
  if (this->force_co2_calibration_button_ != nullptr) {
    this->force_co2_calibration_button_->set_press_callback([this]() {
      // Check model supports CO2
      if (this->model_ != Sen6xModel::SEN63C &&
          this->model_ != Sen6xModel::SEN66 &&
          this->model_ != Sen6xModel::SEN69C) {
        ESP_LOGW(TAG, "FRC not supported on this model (no CO2 sensor)");
        return;
      }

      uint16_t ref_ppm = (uint16_t)this->outdoor_co2_ppm_;
      ESP_LOGI(TAG, "Starting Forced CO2 Recalibration with reference: %d ppm",
               ref_ppm);

      // Per datasheet: Stop measurement -> wait 1400ms -> send FRC -> wait
      // 500ms
      this->write_command_(SEN6X_CMD_STOP_MEASUREMENT);
      this->set_timeout(1500, [this, ref_ppm]() {
        if (this->perform_forced_co2_calibration_(ref_ppm)) {
          ESP_LOGI(TAG, "FRC completed successfully - calibration persisted to "
                        "sensor EEPROM");
        } else {
          ESP_LOGW(TAG, "FRC failed");
        }
        delay(100);
        this->start_measurement_();
      });
    });
  }

  // CO2 Factory Reset button: Resets FRC/ASC calibration to factory defaults
  if (this->co2_factory_reset_button_ != nullptr) {
    this->co2_factory_reset_button_->set_press_callback([this]() {
      // Check model supports CO2
      if (this->model_ != Sen6xModel::SEN63C &&
          this->model_ != Sen6xModel::SEN66 &&
          this->model_ != Sen6xModel::SEN69C) {
        ESP_LOGW(
            TAG,
            "CO2 Factory Reset not supported on this model (no CO2 sensor)");
        return;
      }

      ESP_LOGW(TAG, "Performing CO2 Sensor Factory Reset...");
      ESP_LOGW(TAG, "This will erase FRC and ASC calibration history!");

      // Per datasheet: Stop measurement -> wait -> send command -> wait 1400ms
      this->write_command_(SEN6X_CMD_STOP_MEASUREMENT);
      this->set_timeout(1500, [this]() {
        if (this->write_command_(SEN6X_CMD_CO2_FACTORY_RESET)) {
          ESP_LOGI(TAG, "CO2 Factory Reset sent successfully");
          // Wait 1400ms for command execution (per datasheet)
          this->set_timeout(1500, [this]() {
            ESP_LOGI(TAG, "CO2 calibration reset to factory defaults");
            this->start_measurement_();
          });
        } else {
          ESP_LOGE(TAG, "CO2 Factory Reset command failed!");
          this->start_measurement_();
        }
      });
    });
  }

  // SHT Heater button: Activate heater to remove condensation (0x6765)
  if (this->sht_heater_button_ != nullptr) {
    this->sht_heater_button_->set_press_callback([this]() {
      ESP_LOGI(TAG, "Activating SHT Heater for 1 second...");
      ESP_LOGI(TAG,
               "Wait at least 20s before measurement for accurate T values");

      // Per datasheet: Available in Idle mode only, 1300ms execution time
      this->write_command_(SEN6X_CMD_STOP_MEASUREMENT);
      this->set_timeout(1500, [this]() {
        if (this->write_command_(SEN6X_CMD_ACTIVATE_SHT_HEATER)) {
          ESP_LOGI(TAG, "SHT Heater activated (200mW for 1s)");
          // Wait 20s before restarting measurement
          this->set_timeout(20000, [this]() {
            ESP_LOGI(TAG,
                     "SHT Heater cooldown complete - restarting measurement");
            this->start_measurement_();
          });
        } else {
          ESP_LOGW(TAG, "Failed to activate SHT Heater");
          this->start_measurement_();
        }
      });
    });
  }

  // Clear Device Status button: Read and clear error flags (0xD210)
  if (this->clear_device_status_button_ != nullptr) {
    this->clear_device_status_button_->set_press_callback([this]() {
      ESP_LOGI(TAG, "Reading and clearing device status...");
      uint16_t status_words[2]; // Device status is 2 words (4 bytes)
      if (this->read_words_(SEN6X_CMD_READ_AND_CLEAR_STATUS, status_words, 2)) {
        uint32_t status = ((uint32_t)status_words[0] << 16) | status_words[1];
        ESP_LOGI(TAG, "Device status cleared. Previous status was: 0x%08X",
                 status);
      } else {
        ESP_LOGW(TAG, "Failed to read and clear device status");
      }
    });
  }

  // Register Number Callbacks
  if (this->altitude_compensation_number_ != nullptr) {
    this->altitude_compensation_number_->set_control_callback([this](
                                                                  float value) {
      ESP_LOGD(TAG, "Setting Altitude: %.1f m (Stopping measurement)", value);
      this->write_command_(SEN6X_CMD_STOP_MEASUREMENT);
      // Wait 1500ms for sensor to stop (Datasheet spec)
      this->set_timeout(1500, [this, value]() {
        this->write_altitude_compensation_(value);
        delay(50); // Ensure write processes
        this->start_measurement_();
      });
    });
  }
  if (this->ambient_pressure_compensation_number_ != nullptr) {
    this->ambient_pressure_compensation_number_->set_control_callback(
        [this](float value) {
          // Datasheet 4.8.36: Works in Idle AND Measurement mode - no stop
          // needed!
          ESP_LOGD(TAG, "Setting Pressure: %.1f hPa (direct write)", value);
          this->write_ambient_pressure_compensation_(value);
        });
  }
  if (this->temperature_offset_number_ != nullptr) {
    this->temperature_offset_number_->set_control_callback([this](float value) {
      // Datasheet 4.8.14: Works in Idle AND Measurement mode - no stop
      // needed!
      ESP_LOGD(TAG, "Setting Temp Offset: %.2f C (direct write)", value);
      this->write_temperature_offset_(value);
    });
  }

  // Register Switch Callbacks
  // Note: voc_tuning_switch_ removed - VOC tuning is now YAML-only

  if (this->co2_asc_switch_ != nullptr) {
    this->co2_asc_switch_->set_write_callback([this](bool state) {
      this->co2_asc_preference_.save(&state);
      ESP_LOGD(TAG, "Setting CO2 ASC to %s", state ? "ON" : "OFF");

      this->write_command_(SEN6X_CMD_STOP_MEASUREMENT);
      this->set_timeout(1500, [this, state]() {
        this->write_co2_asc_(state);
        delay(50);
        this->start_measurement_();
        this->co2_asc_switch_->publish_state(state);
      });
    });
  }

  // Auto Fan Cleaning switch callback
  if (this->auto_cleaning_switch_ != nullptr) {
    this->auto_cleaning_switch_->set_write_callback([this](bool state) {
      this->auto_cleaning_preference_.save(&state);
      this->configure_auto_cleaning_(state);
      this->auto_cleaning_switch_->publish_state(state);
    });
  }

  // Subscribe to external pressure source for automatic CO2 compensation
  if (this->pressure_source_ != nullptr) {
    ESP_LOGI(TAG, "External pressure source configured - subscribing for CO2 "
                  "compensation");
    this->pressure_source_->add_on_state_callback(
        [this](float pressure) { this->set_ambient_pressure(pressure); });
  }
}

float Sen6xComponent::get_setup_priority() const {
  return setup_priority::LATE;
}

void Sen6xComponent::read_device_identity_() {
  uint8_t buffer[32];

  // Read Product Name (0xD014)
  if (this->read_bytes_(SEN6X_CMD_GET_PRODUCT_NAME, buffer, 32)) {
    // Null-terminate or just construct string (buffer is raw bytes,
    // potentially padded nulls at end) Reference says 32 bytes.
    std::string product_name;
    for (int i = 0; i < 32; i++) {
      if (buffer[i] == 0)
        break; // Stop at null
      product_name += (char)buffer[i];
    }
    ESP_LOGI(TAG, "Product Name: %s", product_name.c_str());
    if (this->product_name_text_sensor_ != nullptr) {
      this->product_name_text_sensor_->publish_state(product_name);
    }

    // AUTO-DETECT MODEL from product name
    // This eliminates the need for user to specify model in YAML
    if (product_name.find("SEN62") != std::string::npos) {
      this->model_ = Sen6xModel::SEN62;
      ESP_LOGI(TAG, "Auto-detected model: SEN62 (PM + RH/T)");
    } else if (product_name.find("SEN63") != std::string::npos) {
      this->model_ = Sen6xModel::SEN63C;
      ESP_LOGI(TAG, "Auto-detected model: SEN63C (PM + RH/T + CO2)");
    } else if (product_name.find("SEN65") != std::string::npos) {
      this->model_ = Sen6xModel::SEN65;
      ESP_LOGI(TAG, "Auto-detected model: SEN65 (PM + RH/T + VOC + NOx)");
    } else if (product_name.find("SEN66") != std::string::npos) {
      this->model_ = Sen6xModel::SEN66;
      ESP_LOGI(TAG, "Auto-detected model: SEN66 (PM + RH/T + VOC + NOx + CO2)");
    } else if (product_name.find("SEN68") != std::string::npos) {
      this->model_ = Sen6xModel::SEN68;
      ESP_LOGI(TAG,
               "Auto-detected model: SEN68 (PM + RH/T + VOC + NOx + HCHO)");
    } else if (product_name.find("SEN69") != std::string::npos) {
      this->model_ = Sen6xModel::SEN69C;
      ESP_LOGI(
          TAG,
          "Auto-detected model: SEN69C (PM + RH/T + VOC + NOx + CO2 + HCHO)");
    } else {
      ESP_LOGW(TAG, "Unknown product '%s', defaulting to SEN66 behavior",
               product_name.c_str());
      this->model_ = Sen6xModel::SEN66;
    }

    // ========== AUTO-HIDE UNSUPPORTED SENSORS (Core alignment with SEN5x)
    // ========== Disable sensors that are not supported by the detected model
    // This prevents publishing invalid data and cleans up HA entity list

    // VOC: Only SEN65, SEN66, SEN68, SEN69C
    bool has_voc = (this->model_ == Sen6xModel::SEN65 ||
                    this->model_ == Sen6xModel::SEN66 ||
                    this->model_ == Sen6xModel::SEN68 ||
                    this->model_ == Sen6xModel::SEN69C);
    if (this->voc_index_sensor_ != nullptr && !has_voc) {
      ESP_LOGW(TAG, "VOC Index requires SEN65/66/68/69C - disabling sensor");
      this->voc_index_sensor_->set_internal(true);
      this->voc_index_sensor_ = nullptr;
    }

    // NOx: Only SEN65, SEN66, SEN68, SEN69C
    bool has_nox = has_voc; // Same models as VOC
    if (this->nox_sensor_ != nullptr && !has_nox) {
      ESP_LOGW(TAG, "NOx Index requires SEN65/66/68/69C - disabling sensor");
      this->nox_sensor_->set_internal(true);
      this->nox_sensor_ = nullptr;
    }

    // CO2: Only SEN63C, SEN66, SEN69C
    bool has_co2 = (this->model_ == Sen6xModel::SEN63C ||
                    this->model_ == Sen6xModel::SEN66 ||
                    this->model_ == Sen6xModel::SEN69C);
    if (this->co2_sensor_ != nullptr && !has_co2) {
      ESP_LOGW(TAG, "CO2 requires SEN63C/66/69C - disabling sensor");
      this->co2_sensor_->set_internal(true);
      this->co2_sensor_ = nullptr;
    }

    // HCHO: Only SEN68, SEN69C
    bool has_hcho = (this->model_ == Sen6xModel::SEN68 ||
                     this->model_ == Sen6xModel::SEN69C);
    if (this->formaldehyde_sensor_ != nullptr && !has_hcho) {
      ESP_LOGW(TAG, "Formaldehyde requires SEN68/69C - disabling sensor");
      this->formaldehyde_sensor_->set_disabled_by_default(true);
      this->formaldehyde_sensor_->set_internal(true); // Hide from HA completely
      this->formaldehyde_sensor_ = nullptr;
    }
  }

  // Read Serial Number (0xD033)
  if (this->read_bytes_(SEN6X_CMD_GET_SERIAL_NUMBER, buffer, 32)) {
    std::string serial;
    for (int i = 0; i < 32; i++) {
      if (buffer[i] == 0)
        break;
      serial += (char)buffer[i];
    }
    ESP_LOGI(TAG, "Serial Number: %s", serial.c_str());
    if (this->serial_number_text_sensor_ != nullptr) {
      this->serial_number_text_sensor_->publish_state(serial);
    }
  }

  // Read Firmware Version (0xD100) - Optional
  if (this->firmware_version_sensor_ != nullptr) {
    uint16_t version_data[1]; // 2 bytes (Major + Minor) + CRC
    if (this->read_words_(SEN6X_CMD_GET_VERSION, version_data, 1)) {
      uint8_t major = (version_data[0] >> 8) & 0xFF;
      uint8_t minor = version_data[0] & 0xFF;
      char version_str[16];
      snprintf(version_str, sizeof(version_str), "%d.%d", major, minor);
      ESP_LOGI(TAG, "Firmware Version: %s", version_str);
      this->firmware_version_sensor_->publish_state(version_str);
    }
  }
}

void Sen6xComponent::update() {
  // One-time diagnostic log (visible in API log, unlike setup logs)
  static bool first_update = true;
  if (first_update) {
    first_update = false;
    ESP_LOGI(TAG,
             "Boot diagnostics - Altitude loaded from prefs: %.1f m (NaN means "
             "none)",
             this->pending_altitude_);
  }

  if (this->fan_cleaning_active_state_) {
    ESP_LOGD(TAG, "Skipping measurement update during fan cleaning.");
    return;
  }

  if (millis() - this->last_fan_cleaning_end_time_ < 10000) {
    ESP_LOGD(TAG, "Skipping measurement update (settling after cleaning).");
    return;
  }

  // ========== VOC BASELINE PERSISTENCE (same as SEN5x official) ==========
  // Save VOC algorithm state every 3 hours if baseline has changed
  // significantly
  if (this->store_baseline_) {
    this->seconds_since_last_baseline_store_ += 10; // update_interval is ~10s

    if (this->seconds_since_last_baseline_store_ >
        SHORTEST_BASELINE_STORE_INTERVAL) {
      // Read current VOC algorithm state from sensor
      uint8_t cmd[2] = {(uint8_t)(SEN6X_CMD_VOC_ALGORITHM_STATE >> 8),
                        (uint8_t)(SEN6X_CMD_VOC_ALGORITHM_STATE & 0xFF)};

      if (this->write(cmd, 2) == i2c::ERROR_OK) {
        delay(20); // Per datasheet execution time

        uint8_t raw_data[12]; // 4 words * (2 bytes + 1 CRC)
        if (this->read(raw_data, 12) == i2c::ERROR_OK) {
          // Validate CRCs and extract data
          bool crc_ok = true;
          uint16_t states[4];
          for (int i = 0; i < 4 && crc_ok; i++) {
            uint8_t expected_crc = sen6x_crc(&raw_data[i * 3], 2);
            if (raw_data[i * 3 + 2] != expected_crc) {
              crc_ok = false;
            } else {
              states[i] = (raw_data[i * 3] << 8) | raw_data[i * 3 + 1];
            }
          }

          if (crc_ok) {
            int32_t state0 = (states[0] << 16) | states[1];
            int32_t state1 = (states[2] << 16) | states[3];

            // Check if state changed significantly
            int32_t diff0 =
                std::abs(this->voc_baselines_storage_.state0 - state0);
            int32_t diff1 =
                std::abs(this->voc_baselines_storage_.state1 - state1);

            if ((uint32_t)diff0 > MAXIMUM_STORAGE_DIFF ||
                (uint32_t)diff1 > MAXIMUM_STORAGE_DIFF) {
              this->seconds_since_last_baseline_store_ = 0;
              this->voc_baselines_storage_.state0 = state0;
              this->voc_baselines_storage_.state1 = state1;

              if (this->voc_baseline_preference_.save(
                      &this->voc_baselines_storage_)) {
                ESP_LOGI(TAG,
                         "Stored VOC baseline state0: 0x%08X, state1: 0x%08X",
                         (unsigned int)state0, (unsigned int)state1);
              } else {
                ESP_LOGW(TAG, "Could not store VOC baselines");
              }
            }
          }
        }
      }
    }
  }

  this->read_device_status_();

  // Prevent reading data during fan cleaning to avoid PM spikes and I2C
  // errors (NACKs) - REDUNDANT BUT SAFETY DOUBLE CHECK
  if (this->fan_cleaning_active_state_) {
    return;
  }

  // === DATA READY CHECK (Datasheet 4.8.3) ===
  // Check if new measurement data is available before reading
  uint16_t ready_data;
  if (!this->read_words_(SEN6X_CMD_GET_DATA_READY, &ready_data, 1)) {
    ESP_LOGW(TAG, "Failed to check data ready status");
    return;
  }
  // Byte 1 contains the ready flag (0x01 = ready, 0x00 = not ready)
  bool data_ready = (ready_data & 0x00FF) != 0;
  if (!data_ready) {
    ESP_LOGD(TAG, "Data not ready yet, skipping measurement");
    return;
  }

  uint16_t data[10]; // Max 10 words for SEN69C
  uint8_t word_count = this->get_measurement_word_count_();

  if (!this->read_measurement_data_(data, word_count)) {
    ESP_LOGW(TAG, "Failed to read data");
    return;
  }

  // === INVALID DATA DETECTION (Datasheet 4.8.4-4.8.9) ===
  // When sensor hasn't stabilized, it returns 0xFFFF (uint16) or 0x7FFF (int16)
  // Common data layout (first 6 words): PM1.0[0], PM2.5[1], PM4.0[2],
  // PM10.0[3], RH[4], T[5] Remaining words depend on model:
  //   SEN62: (no more)
  //   SEN63C: CO2[6]
  //   SEN65: VOC[6], NOx[7]
  //   SEN66: VOC[6], NOx[7], CO2[8]
  //   SEN68: VOC[6], NOx[7], HCHO[8]
  //   SEN69C: VOC[6], NOx[7], HCHO[8], CO2[9]
  bool has_invalid_data = false;

  // Check PM sensors (uint16 - 0xFFFF indicates invalid)
  if (data[0] == 0xFFFF || data[1] == 0xFFFF || data[2] == 0xFFFF ||
      data[3] == 0xFFFF) {
    ESP_LOGD(TAG, "PM data invalid (0xFFFF), waiting for stabilization");
    has_invalid_data = true;
  }

  // Check RH/T/VOC/NOx (int16 - 0x7FFF indicates invalid)
  if (data[4] == 0x7FFF || data[5] == 0x7FFF || data[6] == 0x7FFF ||
      data[7] == 0x7FFF) {
    ESP_LOGD(TAG,
             "RH/T/VOC/NOx data invalid (0x7FFF), waiting for stabilization");
    has_invalid_data = true;
  }

  // Check CO2 (uint16 - 0xFFFF indicates invalid)
  if (data[8] == 0xFFFF) {
    ESP_LOGD(TAG, "CO2 data invalid (0xFFFF), waiting for stabilization");
    has_invalid_data = true;
  }

  if (has_invalid_data) {
    return;
  }

  // Parse and publish
  // Scaling factors based on Sensirion datasheet/driver logic
  // Mass Concentration: / 10.0
  if (this->pm_1_0_sensor_ != nullptr)
    this->pm_1_0_sensor_->publish_state(data[0] / 10.0f);
  if (this->pm_2_5_sensor_ != nullptr)
    this->pm_2_5_sensor_->publish_state(data[1] / 10.0f);
  if (this->pm_4_0_sensor_ != nullptr)
    this->pm_4_0_sensor_->publish_state(data[2] / 10.0f);
  if (this->pm_10_0_sensor_ != nullptr)
    this->pm_10_0_sensor_->publish_state(data[3] / 10.0f);

  // RH: / 100.0 (int16)
  float humidity = (int16_t)data[4] / 100.0f;
  if (this->humidity_sensor_ != nullptr)
    this->humidity_sensor_->publish_state(humidity);

  // T: / 200.0 (Sensirion Standard for SEN5x/6x)
  float temperature = (int16_t)data[5] / 200.0f;
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature);

  // VOC/NOx: / 10.0
  float voc_index = 0.0f;
  if (this->voc_index_sensor_ != nullptr) {
    voc_index = (int16_t)data[6] / 10.0f;
    this->voc_index_sensor_->publish_state(voc_index);
  }

  // Calculated Metrics (WELL/RESET) based on VOC Index
  if (voc_index > 0.0f) {
    if (this->well_tvoc_sensor_ != nullptr) {
      float well_tvoc = EnvironmentalPhysics::calculate_well_tvoc(voc_index);
      this->well_tvoc_sensor_->publish_state(well_tvoc);
    }
    if (this->reset_tvoc_sensor_ != nullptr) {
      float reset_tvoc = EnvironmentalPhysics::calculate_reset_tvoc(voc_index);
      this->reset_tvoc_sensor_->publish_state(reset_tvoc);
    }
  }
  // TVOC Ethanol - Only publish if VOC Index is valid
  if (this->tvoc_ethanol_sensor_ != nullptr && voc_index > 0.0f) {
    float ethanol = EnvironmentalPhysics::calculate_ethanol_tvoc(voc_index);
    this->tvoc_ethanol_sensor_->publish_state(ethanol);
  }

  if (this->nox_sensor_ != nullptr)
    this->nox_sensor_->publish_state((int16_t)data[7] / 10.0f);

  // CO2: Position varies by model (Datasheet v0.92)
  //   SEN63C: data[6]
  //   SEN66: data[8]
  //   SEN69C: data[9] (after HCHO at data[8])
  float co2 = 0.0f;
  if (this->co2_sensor_ != nullptr) {
    switch (this->model_) {
    case Sen6xModel::SEN63C:
      co2 = (float)data[6];
      break;
    case Sen6xModel::SEN66:
      co2 = (float)data[8];
      break;
    case Sen6xModel::SEN69C:
      co2 = (float)data[9];
      break;
    default:
      co2 = 0.0f; // Model doesn't have CO2
      break;
    }
    if (co2 > 0.0f) {
      this->co2_sensor_->publish_state(co2);
    }
  }

  // HCHO (Formaldehyde): data[8] for SEN68/SEN69C (Datasheet v0.92)
  // Scaled by /10 for ppb
  float hcho = 0.0f;
  if (this->formaldehyde_sensor_ != nullptr) {
    if (this->model_ == Sen6xModel::SEN68 ||
        this->model_ == Sen6xModel::SEN69C) {
      hcho = data[8] / 10.0f; // HCHO [ppb] = value / 10
      if (hcho > 0.0f) {
        this->formaldehyde_sensor_->publish_state(hcho);
      }
    }
  }

  // ========== NUMBER CONCENTRATION (particles/cm³) ==========
  // Optional: Read 0x0316 only if at least one NC sensor is configured
  if (this->nc_0_5_sensor_ != nullptr || this->nc_1_0_sensor_ != nullptr ||
      this->nc_2_5_sensor_ != nullptr || this->nc_4_0_sensor_ != nullptr ||
      this->nc_10_0_sensor_ != nullptr) {
    uint16_t nc_data[5]; // 5 uint16 values + CRCs
    if (this->read_words_(SEN6X_CMD_NUMBER_CONCENTRATION, nc_data, 5)) {
      // All values scaled x10 per datasheet
      if (this->nc_0_5_sensor_ != nullptr && nc_data[0] != 0xFFFF) {
        this->nc_0_5_sensor_->publish_state((float)nc_data[0] / 10.0f);
      }
      if (this->nc_1_0_sensor_ != nullptr && nc_data[1] != 0xFFFF) {
        this->nc_1_0_sensor_->publish_state((float)nc_data[1] / 10.0f);
      }
      if (this->nc_2_5_sensor_ != nullptr && nc_data[2] != 0xFFFF) {
        this->nc_2_5_sensor_->publish_state((float)nc_data[2] / 10.0f);
      }
      if (this->nc_4_0_sensor_ != nullptr && nc_data[3] != 0xFFFF) {
        this->nc_4_0_sensor_->publish_state((float)nc_data[3] / 10.0f);
      }
      if (this->nc_10_0_sensor_ != nullptr && nc_data[4] != 0xFFFF) {
        this->nc_10_0_sensor_->publish_state((float)nc_data[4] / 10.0f);
      }
    }
  }
}

void Sen6xComponent::read_device_status_() {
  uint16_t status_words[2]; // Device status is 2 words (4 bytes)
  if (!this->read_words_(SEN6X_CMD_GET_STATUS, status_words, 2)) {
    ESP_LOGW(TAG, "Failed to read device status");
    return;
  }

  uint32_t device_status = ((uint32_t)status_words[0] << 16) | status_words[1];
  ESP_LOGD(TAG, "Device Status: 0x%08X", device_status);

  // Publish Status Hex
  if (this->status_text_sensor_ != nullptr) {
    char hex_value[11];
    sprintf(hex_value, "0x%08X", device_status);
    this->status_text_sensor_->publish_state(hex_value);
  }

  // Publish Binary Sensors
  // Bit definitions based on Sensirion SEN6x datasheet/driver
  if (this->fan_error_binary_sensor_ != nullptr)
    this->fan_error_binary_sensor_->publish_state((device_status & (1 << 21)) !=
                                                  0);

  if (this->rht_error_binary_sensor_ != nullptr)
    this->rht_error_binary_sensor_->publish_state((device_status & (1 << 20)) !=
                                                  0);

  if (this->gas_error_binary_sensor_ != nullptr)
    this->gas_error_binary_sensor_->publish_state((device_status & (1 << 19)) !=
                                                  0);

  if (this->pm_error_binary_sensor_ != nullptr)
    this->pm_error_binary_sensor_->publish_state((device_status & (1 << 18)) !=
                                                 0);

  // LDIR/Laser Error (Bit 17 or 12?)
  // Common driver maps Bit 17 to Laser Error
  if (this->laser_error_binary_sensor_ != nullptr)
    this->laser_error_binary_sensor_->publish_state(
        (device_status & (1 << 17)) != 0);

  // Fan Warning (Bit 21 is technically Fan Speed Warning)
  if (this->fan_warning_binary_sensor_ != nullptr)
    this->fan_warning_binary_sensor_->publish_state(
        (device_status & (1 << 21)) != 0);

  // Fan Cleaning Active
  // Datasheet does not define a dedicated status bit for this.
  // We use our internal software state.
  if (this->fan_cleaning_active_binary_sensor_ != nullptr)
    this->fan_cleaning_active_binary_sensor_->publish_state(
        this->fan_cleaning_active_state_);

  // Log warnings for debugging (preserved)
  if (device_status & (1 << 21))
    ESP_LOGW(TAG, "Status: Fan Speed Warning");
  if (device_status & (1 << 19))
    ESP_LOGW(TAG, "Status: Gas Error");
  if (device_status & (1 << 18))
    ESP_LOGW(TAG, "Status: PM Error");
}

void Sen6xComponent::read_device_configuration_() {
  uint16_t data;

  // Read Ambient Pressure (0x6720)
  if (this->read_words_(SEN6X_CMD_GET_AMBIENT_PRESSURE, &data, 1)) {
    // Scale checks needed. Assuming int16 raw hPa for now based on common
    // drivers.
    int16_t pressure = (int16_t)data;
    ESP_LOGD(TAG, "Ambient Pressure: %d", pressure);
    if (this->ambient_pressure_sensor_ != nullptr)
      this->ambient_pressure_sensor_->publish_state(pressure); // Scaling 1.0

    // Sync Number Component
    if (this->ambient_pressure_compensation_number_ != nullptr) {
      this->ambient_pressure_compensation_number_->publish_state(pressure);
    }
  }

  // Read Sensor Altitude (0x6736)
  if (this->read_words_(SEN6X_CMD_GET_SENSOR_ALTITUDE, &data, 1)) {
    int16_t altitude = (int16_t)data;
    ESP_LOGD(TAG, "Sensor Altitude read from device: %d", altitude);

    // SEN6x doesn't persist altitude internally - it's volatile.
    // After boot, the sensor reports 0 even though we wrote a value in
    // setup(). Use the NVS value for both sensor and number if we restored
    // it.
    if (this->altitude_restored_ && !std::isnan(this->pending_altitude_)) {
      // We restored from NVS - use that value (more accurate than sensor's 0)
      float nvs_altitude = this->pending_altitude_;
      if (this->sensor_altitude_sensor_ != nullptr)
        this->sensor_altitude_sensor_->publish_state(nvs_altitude);
      // Number already published in setup(), skip here
    } else {
      // No NVS value - use what sensor reports
      if (this->sensor_altitude_sensor_ != nullptr)
        this->sensor_altitude_sensor_->publish_state(altitude);
      if (this->altitude_compensation_number_ != nullptr)
        this->altitude_compensation_number_->publish_state(altitude);
    }
  }
}

bool Sen6xComponent::write_command_(uint16_t command) {
  uint8_t data[2];
  data[0] = (command >> 8) & 0xFF;
  data[1] = command & 0xFF;
  return this->write(data, 2) == i2c::ERROR_OK;
}

bool Sen6xComponent::write_command_with_data_(uint16_t command, uint16_t data) {
  uint8_t buffer[5];
  buffer[0] = (command >> 8) & 0xFF;
  buffer[1] = command & 0xFF;
  buffer[2] = (data >> 8) & 0xFF;
  buffer[3] = data & 0xFF;

  // Generate CRC for the data word
  uint8_t crc_data[2] = {buffer[2], buffer[3]};
  buffer[4] = sen6x_crc(crc_data, 2);

  return this->write(buffer, 5) == i2c::ERROR_OK;
}

bool Sen6xComponent::start_measurement_() {
  return this->write_command_(SEN6X_CMD_START_MEASUREMENT);
}

void Sen6xComponent::start_fan_cleaning_() {
  ESP_LOGD(TAG, "Stopping measurement to start fan cleaning...");
  if (!this->write_command_(SEN6X_CMD_STOP_MEASUREMENT)) {
    ESP_LOGW(TAG, "Failed to stop measurement!");
    return;
  }

  // Wait 100ms before starting cleaning, non-blocking
  this->set_timeout("start_cleaning_delay", 100,
                    [this]() { this->continue_fan_cleaning_(); });
}

void Sen6xComponent::continue_fan_cleaning_() {

  ESP_LOGD(TAG, "Starting fan cleaning...");
  if (this->write_command_(SEN6X_CMD_START_FAN_CLEANING)) {
    ESP_LOGI(TAG, "Fan cleaning started. Measurement will resume in 12s.");

    // Set software state and update binary sensor immediately
    this->fan_cleaning_active_state_ = true;
    if (this->fan_cleaning_active_binary_sensor_ != nullptr)
      this->fan_cleaning_active_binary_sensor_->publish_state(true);

    this->set_timeout("resume_measurement", 12000, [this]() {
      ESP_LOGI(TAG, "Resuming measurement after fan cleaning...");
      this->start_measurement_();

      // Clear software state
      this->fan_cleaning_active_state_ = false;
      this->last_fan_cleaning_end_time_ = millis();
      if (this->fan_cleaning_active_binary_sensor_ != nullptr)
        this->fan_cleaning_active_binary_sensor_->publish_state(false);
    });
  } else {
    ESP_LOGW(TAG, "Failed to start fan cleaning");
    this->start_measurement_();
  }
}

void Sen6xComponent::execute_preferences_reset_() {
  ESP_LOGW(TAG, "Resetting all preferences to defaults/factory...");

  // 1. Numbers -> Set to NAN (Invalidate to force read from device on next
  // boot)
  float nan_val = NAN;
  if (this->altitude_preference_.save(&nan_val))
    ESP_LOGD(TAG, "Invalidated Altitude preference");
  if (this->ambient_pressure_preference_.save(&nan_val))
    ESP_LOGD(TAG, "Invalidated Pressure preference");
  if (this->temperature_offset_preference_.save(&nan_val))
    ESP_LOGD(TAG, "Invalidated Temp Offset preference");

  // 2. Switches -> Set to Safety Defaults
  // Note: voc_tuning_preference_ removed - VOC tuning is now YAML-only

  bool co2_asc_default = true; // Enabled
  if (this->co2_asc_preference_.save(&co2_asc_default))
    ESP_LOGD(TAG, "Reset CO2 ASC preference to Enabled");

  bool auto_clean_default = false; // Disabled
  if (this->auto_cleaning_preference_.save(&auto_clean_default))
    ESP_LOGD(TAG, "Reset Auto Cleaning preference to Disabled");

  ESP_LOGI(TAG, "Preferences reset complete. Restarting is recommended.");
}
void Sen6xComponent::execute_device_reset_() {
  ESP_LOGD(TAG, "Resetting device...");
  if (this->write_command_(SEN6X_CMD_DEVICE_RESET)) {
    ESP_LOGI(TAG, "Device reset command sent");
    // Re-run setup after a short delay? Or just let polling resume?
    // A reset usually requires re-initialization (start measurement).
    // Let's mark component as failed or try to re-init in next loop?
    // Ideally, we should restart measurement after reset.
    this->status_set_warning();
    // Non-blocking delay for reset recovery
    this->set_timeout("reset_recovery", 100, [this]() {
      this->start_measurement_();
      this->status_clear_warning();
    });
  } else {
    ESP_LOGW(TAG, "Failed to reset device");
  }
}

// Sen6xNumber::setup removed.

bool Sen6xComponent::write_altitude_compensation_(float altitude) {
  uint16_t alt_int = (uint16_t)altitude;
  ESP_LOGD(TAG, "Writing Altitude Compensation: %d m", alt_int);
  if (this->write_command_with_data_(SEN6X_CMD_GET_SENSOR_ALTITUDE, alt_int)) {
    ESP_LOGI(TAG, "Altitude Compensation written");
    // Manual persistence save
    if (this->altitude_preference_.save(&altitude)) {
      ESP_LOGD(TAG, "Altitude persisted to flash");
    }
    if (this->sensor_altitude_sensor_ != nullptr) {
      this->sensor_altitude_sensor_->publish_state(alt_int);
    }
    return true;
  } else {
    ESP_LOGW(TAG, "Failed to write Altitude Compensation");
    return false;
  }
}

// Public method for external barometric sensor integration (e.g., BME280)
// This allows feeding real-time pressure data for CO2 compensation
bool Sen6xComponent::set_ambient_pressure(float pressure_hpa) {
  // Validate range per datasheet: 700-1200 hPa
  if (pressure_hpa < 700.0f || pressure_hpa > 1200.0f) {
    ESP_LOGW(TAG, "Ambient pressure %.1f hPa out of range (700-1200), ignoring",
             pressure_hpa);
    return false;
  }

  // Only applicable to models with CO2: SEN63C, SEN66, SEN69C
  if (this->model_ != Sen6xModel::SEN63C && this->model_ != Sen6xModel::SEN66 &&
      this->model_ != Sen6xModel::SEN69C) {
    // Silently ignore for models without CO2 (no point in logging every time)
    return true;
  }

  // Only write if pressure changed significantly (±1 hPa) to reduce I2C writes
  // Datasheet: volatile setting, but doesn't need constant updates
  static float last_written_pressure = 0.0f;
  if (fabsf(pressure_hpa - last_written_pressure) < 1.0f &&
      last_written_pressure != 0.0f) {
    // Pressure hasn't changed enough, skip write
    return true;
  }

  ESP_LOGD(TAG, "External pressure sensor: %.1f hPa (changed from %.1f)",
           pressure_hpa, last_written_pressure);

  if (this->write_ambient_pressure_compensation_(pressure_hpa)) {
    last_written_pressure = pressure_hpa;
    return true;
  }
  return false;
}

bool Sen6xComponent::write_ambient_pressure_compensation_(float pressure) {
  uint16_t press_int = (uint16_t)lroundf(pressure);
  ESP_LOGD(TAG, "Writing Ambient Pressure Compensation: %d hPa", press_int);
  if (this->write_command_with_data_(SEN6X_CMD_GET_AMBIENT_PRESSURE,
                                     press_int)) {
    ESP_LOGI(TAG, "Ambient Pressure Compensation written");
    if (this->ambient_pressure_preference_.save(&pressure)) {
      ESP_LOGD(TAG, "Ambient Pressure persisted to flash");
    }
    if (this->ambient_pressure_sensor_ != nullptr) {
      this->ambient_pressure_sensor_->publish_state(press_int);
    }
    return true;
  } else {
    ESP_LOGW(TAG, "Failed to write Ambient Pressure Compensation");
    return false;
  }
}

bool Sen6xComponent::write_temperature_offset_(float offset) {
  // Datasheet 4.8.14: Requires 4 words (12 bytes of data)
  // Word 0: Offset (int16, scaled * 200)
  // Word 1: Slope (int16, scaled * 10000) -> 0 for constant offset
  // Word 2: Time Constant (uint16, seconds) -> 0 for immediate application
  // Word 3: Slot (uint16, MUST be 0-4) -> Use slot 0 (base self-heating)

  int16_t offset_ticks = (int16_t)(offset * 200.0f);
  int16_t slope = 0;          // No temperature-dependent scaling
  uint16_t time_constant = 0; // Apply immediately
  uint16_t slot = 0;          // CRITICAL: Must specify valid slot!

  ESP_LOGD(TAG, "Writing Temp Offset: %.2f C (%d ticks), Slot: %d", offset,
           offset_ticks, slot);

  uint16_t payload[4] = {(uint16_t)offset_ticks, (uint16_t)slope, time_constant,
                         slot};

  uint8_t buffer[2 + (4 * 3)]; // Cmd (2) + 4 words * (2 data + 1 CRC)
  buffer[0] = 0x60;
  buffer[1] = 0xB2;

  int idx = 2;
  for (int i = 0; i < 4; i++) {
    buffer[idx++] = (payload[i] >> 8) & 0xFF;
    buffer[idx++] = payload[i] & 0xFF;
    uint8_t crc_data[2] = {buffer[idx - 2], buffer[idx - 1]};
    buffer[idx++] = sen6x_crc(crc_data, 2);
  }

  if (this->write(buffer, sizeof(buffer)) == i2c::ERROR_OK) {
    ESP_LOGI(TAG, "Temperature Offset written to slot %d", slot);
    if (this->temperature_offset_preference_.save(&offset)) {
      ESP_LOGD(TAG, "Persisted to flash");
    }
    return true;
  }
  ESP_LOGW(TAG, "Failed to write Temperature Offset");
  return false;
}

bool Sen6xComponent::write_temperature_compensation_(
    const TemperatureCompensation &compensation) {
  // Datasheet 4.8.14: Requires 4 words (12 bytes of data)
  // Word 0: Offset (int16, scaled * 200)
  // Word 1: Slope (int16, scaled * 10000)
  // Word 2: Time Constant (uint16, seconds)
  // Word 3: Slot (uint16, MUST be 0-4) -> Use slot 0 (base self-heating)

  ESP_LOGD(TAG, "Writing Temp Compensation: offset=%d, slope=%d, time=%d",
           compensation.offset, compensation.normalized_offset_slope,
           compensation.time_constant);

  uint16_t payload[4] = {
      (uint16_t)compensation.offset,
      (uint16_t)compensation.normalized_offset_slope,
      compensation.time_constant,
      0 // Slot 0
  };

  uint8_t buffer[2 + (4 * 3)]; // Cmd (2) + 4 words * (2 data + 1 CRC)
  buffer[0] = 0x60;
  buffer[1] = 0xB2;

  int idx = 2;
  for (int i = 0; i < 4; i++) {
    buffer[idx++] = (payload[i] >> 8) & 0xFF;
    buffer[idx++] = payload[i] & 0xFF;
    uint8_t crc_data[2] = {buffer[idx - 2], buffer[idx - 1]};
    buffer[idx++] = sen6x_crc(crc_data, 2);
  }

  if (this->write(buffer, sizeof(buffer)) == i2c::ERROR_OK) {
    ESP_LOGI(TAG,
             "Temperature Compensation written (offset=%.2f°C, slope=%.4f, "
             "time=%ds)",
             compensation.offset / 200.0f,
             compensation.normalized_offset_slope / 10000.0f,
             compensation.time_constant);
    return true;
  }
  ESP_LOGW(TAG, "Failed to write Temperature Compensation");
  return false;
}

bool Sen6xComponent::write_voc_algorithm_tuning_(const GasTuning &tuning) {
  // Command 0x60D0: Set VOC Algorithm Tuning Parameters
  // Payload: 6 words (12 bytes)
  uint16_t payload[6];
  payload[0] = tuning.index_offset;
  payload[1] = tuning.learning_time_offset_hours; // 12 default, 720 for WELL
  payload[2] = tuning.learning_time_gain_hours;
  payload[3] = tuning.gating_max_duration_minutes;
  payload[4] = tuning.std_initial;
  payload[5] = tuning.gain_factor;

  ESP_LOGD(
      TAG,
      "VOC Tuning: idx=%d, offset=%dh, gain=%dh, gating=%dm, std=%d, factor=%d",
      payload[0], payload[1], payload[2], payload[3], payload[4], payload[5]);

  uint8_t buffer[2 + (6 * 3)]; // Cmd (2) + 6 words * (2 data + 1 CRC)
  buffer[0] = 0x60;
  buffer[1] = 0xD0;

  int idx = 2;
  for (int i = 0; i < 6; i++) {
    buffer[idx++] = (payload[i] >> 8) & 0xFF;
    buffer[idx++] = payload[i] & 0xFF;
    uint8_t crc_data[2] = {buffer[idx - 2], buffer[idx - 1]};
    buffer[idx++] = sen6x_crc(crc_data, 2);
  }

  return this->write(buffer, sizeof(buffer)) == i2c::ERROR_OK;
}

bool Sen6xComponent::write_nox_algorithm_tuning_(const GasTuning &tuning) {
  // Command 0x60E1: Set NOx Algorithm Tuning Parameters
  // Payload: 5 words (NOx has no std_initial parameter)
  uint16_t payload[5];
  payload[0] = tuning.index_offset;
  payload[1] = tuning.learning_time_offset_hours; // 12 default, 720 for WELL
  payload[2] = tuning.learning_time_gain_hours;
  payload[3] = tuning.gating_max_duration_minutes;
  payload[4] = tuning.gain_factor;

  ESP_LOGD(TAG,
           "NOx Tuning: idx=%d, offset=%dh, gain=%dh, gating=%dm, factor=%d",
           payload[0], payload[1], payload[2], payload[3], payload[4]);

  uint8_t buffer[2 + (5 * 3)];
  buffer[0] = 0x60;
  buffer[1] = 0xE1;

  int idx = 2;
  for (int i = 0; i < 5; i++) {
    buffer[idx++] = (payload[i] >> 8) & 0xFF;
    buffer[idx++] = payload[i] & 0xFF;
    uint8_t crc_data[2] = {buffer[idx - 2], buffer[idx - 1]};
    buffer[idx++] = sen6x_crc(crc_data, 2);
  }

  return this->write(buffer, sizeof(buffer)) == i2c::ERROR_OK;
}

bool Sen6xComponent::write_rht_acceleration_(const RhtAcceleration &rht) {
  // Command 0x6100: Set Temperature Acceleration Parameters
  // Payload: 4 words (K, P, T1, T2)
  // These are volatile (reset to defaults on power cycle)
  // Must be called in Idle Mode (before start_measurement)
  uint16_t payload[4];
  payload[0] = rht.k;
  payload[1] = rht.p;
  payload[2] = rht.t1;
  payload[3] = rht.t2;

  ESP_LOGI(TAG, "RHT Acceleration: K=%d, P=%d, T1=%ds, T2=%ds", payload[0],
           payload[1], payload[2], payload[3]);

  uint8_t buffer[2 + (4 * 3)]; // Command (2) + 4 words * (2 bytes + 1 CRC)
  buffer[0] = (SEN6X_CMD_SET_RHT_ACCELERATION >> 8) & 0xFF;
  buffer[1] = SEN6X_CMD_SET_RHT_ACCELERATION & 0xFF;

  int idx = 2;
  for (int i = 0; i < 4; i++) {
    buffer[idx++] = (payload[i] >> 8) & 0xFF;
    buffer[idx++] = payload[i] & 0xFF;
    uint8_t crc_data[2] = {buffer[idx - 2], buffer[idx - 1]};
    buffer[idx++] = sen6x_crc(crc_data, 2);
  }

  return this->write(buffer, sizeof(buffer)) == i2c::ERROR_OK;
}

bool Sen6xComponent::write_co2_asc_(bool enabled) {
  // Command 0x6711, 2 bytes (Padding + Status) + CRC
  uint16_t data = enabled ? 0x0001 : 0x0000;
  return this->write_command_with_data_(0x6711, data);
}

bool Sen6xComponent::perform_forced_co2_calibration_(uint16_t reference_ppm) {
  // Datasheet 4.8.31: Forced CO2 Recalibration
  // Must be called in Idle Mode (measurement stopped)
  // Takes ~500ms, reads back correction value
  // Calibration is PERSISTENT (stored in sensor EEPROM)

  ESP_LOGI(TAG, "Executing Forced CO2 Recalibration with reference: %d ppm",
           reference_ppm);

  // Send command with reference value
  if (!this->write_command_with_data_(SEN6X_CMD_FORCED_CO2_RECAL,
                                      reference_ppm)) {
    ESP_LOGW(TAG, "Failed to send FRC command");
    return false;
  }

  // Wait 500ms for FRC to complete (per datasheet)
  delay(550);

  // Read correction value (3 bytes: 2 data + 1 CRC)
  uint16_t correction;
  if (this->read_words_(SEN6X_CMD_FORCED_CO2_RECAL, &correction, 1)) {
    if (correction == 0xFFFF) {
      ESP_LOGW(TAG, "FRC failed - sensor returned error (0xFFFF)");
      return false;
    }

    // Correction value is (returned - 0x8000)
    int16_t offset = (int16_t)(correction - 0x8000);
    ESP_LOGI(TAG, "FRC successful! Correction offset: %d ppm (raw: 0x%04X)",
             offset, correction);
    return true;
  }

  ESP_LOGW(TAG, "Failed to read FRC result");
  return false;
}

void Sen6xComponent::configure_auto_cleaning_(bool enabled) {
  // First cancel any existing interval/timeout
  this->cancel_interval("auto_clean");
  this->cancel_timeout("auto_clean_first");

  if (enabled) {
    uint32_t interval_hours = this->auto_cleaning_interval_ms_ / 3600000;
    ESP_LOGI(TAG, "Enabling Auto Fan Cleaning (every %u hours)",
             interval_hours);
    ESP_LOGI(TAG, "First auto-clean scheduled in %u hours", interval_hours);

    // Use timeout for first cleaning, then set_interval for subsequent ones
    // This prevents immediate execution when the switch is toggled
    this->set_timeout(
        "auto_clean_first", this->auto_cleaning_interval_ms_, [this]() {
          ESP_LOGI(TAG, "Triggering First Scheduled Auto Fan Cleaning");
          this->start_fan_cleaning_();

          // Now set up recurring interval for subsequent cleanings
          this->set_interval(
              "auto_clean", this->auto_cleaning_interval_ms_, [this]() {
                ESP_LOGI(TAG, "Triggering Scheduled Auto Fan Cleaning");
                this->start_fan_cleaning_();
              });
        });
  } else {
    ESP_LOGI(TAG, "Disabling Auto Fan Cleaning");
  }
}

// Reads 'len' bytes from the sensor after sending 'command'
// 'buffer' must be of size 'len'
bool Sen6xComponent::read_bytes_(uint16_t command, uint8_t *buffer,
                                 uint8_t len) {
  if (!this->write_command_(command))
    return false;

  delay(20); // 20ms wait for processing (standard for most SEN66 commands)

  // Calculate total bytes to read: len is data bytes.
  // Wire format: For every 2 data bytes, 1 CRC byte.
  // So if len=32 (Product Name), that's 16 words. 16 * 3 = 48 bytes on wire.

  size_t wire_len = len + (len / 2); // 1 CRC per 2 bytes
  uint8_t raw_buffer[wire_len];

  if (this->read(raw_buffer, wire_len) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C read failed for command 0x%04X", command);
    return false;
  }

  // Unpack and CRC check
  int idx = 0;
  for (int i = 0; i < len; i += 2) {
    uint8_t b1 = raw_buffer[idx++];
    uint8_t b2 = raw_buffer[idx++];
    uint8_t crc = raw_buffer[idx++];

    uint8_t data_to_check[2] = {b1, b2};
    if (sen6x_crc(data_to_check, 2) != crc) {
      ESP_LOGW(TAG, "CRC Error reading command 0x%04X, word %d", command,
               i / 2);
      return false;
    }
    buffer[i] = b1;
    buffer[i + 1] = b2;
  }
  return true;
}

// Reads 'words' 16-bit words from the sensor after sending 'command'
// 'data' array must be of size 'words'
bool Sen6xComponent::read_words_(uint16_t command, uint16_t *data,
                                 uint8_t words) {
  if (!this->write_command_(command))
    return false;

  delay(20); // 20ms wait for processing

  // Each word is 2 bytes data + 1 byte CRC = 3 bytes on wire
  size_t wire_len = words * 3;
  uint8_t raw_buffer[wire_len];

  if (this->read(raw_buffer, wire_len) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C read failed for command 0x%04X", command);
    return false;
  }

  // Unpack and CRC check
  int idx = 0;
  for (int i = 0; i < words; i++) {
    uint8_t msb = raw_buffer[idx++];
    uint8_t lsb = raw_buffer[idx++];
    uint8_t crc = raw_buffer[idx++];

    uint8_t data_to_check[2] = {msb, lsb};
    if (sen6x_crc(data_to_check, 2) != crc) {
      ESP_LOGW(TAG, "CRC Error reading command 0x%04X, word %d", command, i);
      return false;
    }
    data[i] = (msb << 8) | lsb;
  }
  return true;
}

// This method reads measurement values using the model-specific command
// Each SEN6x model has its own I2C command (Datasheet v0.92 Table 26)
bool Sen6xComponent::read_measurement_data_(uint16_t *data, uint8_t len) {
  uint16_t cmd;
  switch (this->model_) {
  case Sen6xModel::SEN62:
    cmd = SEN6X_CMD_READ_SEN62;
    break;
  case Sen6xModel::SEN63C:
    cmd = SEN6X_CMD_READ_SEN63C;
    break;
  case Sen6xModel::SEN65:
    cmd = SEN6X_CMD_READ_SEN65;
    break;
  case Sen6xModel::SEN66:
    cmd = SEN6X_CMD_READ_SEN66;
    break;
  case Sen6xModel::SEN68:
    cmd = SEN6X_CMD_READ_SEN68;
    break;
  case Sen6xModel::SEN69C:
    cmd = SEN6X_CMD_READ_SEN69C;
    break;
  default:
    cmd = SEN6X_CMD_READ_SEN66; // Fallback to SEN66
    break;
  }
  return this->read_words_(cmd, data, len);
}

// Returns the number of data words based on sensor model (Datasheet v0.92)
uint8_t Sen6xComponent::get_measurement_word_count_() {
  switch (this->model_) {
  case Sen6xModel::SEN62:
    return 6; // PM1,PM2.5,PM4,PM10,RH,T
  case Sen6xModel::SEN63C:
    return 7; // PM1,PM2.5,PM4,PM10,RH,T,CO2
  case Sen6xModel::SEN65:
    return 8; // PM1,PM2.5,PM4,PM10,RH,T,VOC,NOx
  case Sen6xModel::SEN66:
    return 9; // PM1,PM2.5,PM4,PM10,RH,T,VOC,NOx,CO2
  case Sen6xModel::SEN68:
    return 9; // PM1,PM2.5,PM4,PM10,RH,T,VOC,NOx,HCHO (no CO2)
  case Sen6xModel::SEN69C:
    return 10; // PM1,PM2.5,PM4,PM10,RH,T,VOC,NOx,HCHO,CO2
  default:
    return 9;
  }
}

void Sen6xComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "SEN6x:");
  LOG_I2C_DEVICE(this);
  LOG_SENSOR("  ", "PM1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM4.0", this->pm_4_0_sensor_);
  LOG_SENSOR("  ", "PM10.0", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "VOC Index", this->voc_index_sensor_);
  LOG_SENSOR("  ", "NOx Index", this->nox_sensor_);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_sensor_);
  LOG_SENSOR("  ", "TVOC WELL", this->well_tvoc_sensor_);
  LOG_SENSOR("  ", "TVOC RESET", this->reset_tvoc_sensor_);
  LOG_SENSOR("  ", "TVOC Ethanol", this->tvoc_ethanol_sensor_);

  if (this->voc_algorithm_tuning_720h_) {
    ESP_LOGCONFIG(TAG, "  VOC Algorithm Tuning: 720h (Building Standards)");
  } else {
    ESP_LOGCONFIG(TAG, "  VOC Algorithm Tuning: 12h (Default)");
  }

  // Altitude persistence diagnostic (shows boot-loaded value)
  ESP_LOGCONFIG(TAG, "  Altitude (loaded from NVS): %.1f m%s",
                this->pending_altitude_,
                std::isnan(this->pending_altitude_) ? " [NOT FOUND]" : "");

  LOG_TEXT_SENSOR("  ", "Product Name", this->product_name_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Serial Number", this->serial_number_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Status Hex", this->status_text_sensor_);

  LOG_BINARY_SENSOR("  ", "Fan Error", this->fan_error_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Fan Warning", this->fan_warning_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Gas Error", this->gas_error_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "RHT Error", this->rht_error_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "PM Error", this->pm_error_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Laser Error", this->laser_error_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Cleaning Active",
                    this->fan_cleaning_active_binary_sensor_);

  // Model validation warnings
  const char *model_name;
  switch (this->model_) {
  case Sen6xModel::SEN62:
    model_name = "SEN62";
    break;
  case Sen6xModel::SEN63C:
    model_name = "SEN63C";
    break;
  case Sen6xModel::SEN65:
    model_name = "SEN65";
    break;
  case Sen6xModel::SEN66:
    model_name = "SEN66";
    break;
  case Sen6xModel::SEN68:
    model_name = "SEN68";
    break;
  case Sen6xModel::SEN69C:
    model_name = "SEN69C";
    break;
  default:
    model_name = "Unknown";
    break;
  }
  ESP_LOGCONFIG(TAG, "  Model: %s", model_name);

  // VOC/NOx validation (only SEN65, SEN66, SEN68, SEN69C have VOC/NOx)
  bool has_voc =
      (this->model_ == Sen6xModel::SEN65 || this->model_ == Sen6xModel::SEN66 ||
       this->model_ == Sen6xModel::SEN68 || this->model_ == Sen6xModel::SEN69C);
  if (!has_voc) {
    if (this->voc_index_sensor_ != nullptr) {
      ESP_LOGW(TAG,
               "  WARNING: VOC sensor configured but %s does not have VOC!",
               model_name);
    }
    if (this->nox_sensor_ != nullptr) {
      ESP_LOGW(TAG,
               "  WARNING: NOx sensor configured but %s does not have NOx!",
               model_name);
    }
  }

  // CO2 validation (only SEN63C, SEN66, SEN69C have CO2)
  bool has_co2 =
      (this->model_ == Sen6xModel::SEN63C ||
       this->model_ == Sen6xModel::SEN66 || this->model_ == Sen6xModel::SEN69C);
  if (!has_co2 && this->co2_sensor_ != nullptr) {
    ESP_LOGW(TAG, "  WARNING: CO2 sensor configured but %s does not have CO2!",
             model_name);
  }

  // HCHO validation (only SEN68, SEN69C have HCHO)
  bool has_hcho =
      (this->model_ == Sen6xModel::SEN68 || this->model_ == Sen6xModel::SEN69C);
  if (!has_hcho && this->formaldehyde_sensor_ != nullptr) {
    ESP_LOGW(TAG,
             "  WARNING: Formaldehyde sensor configured but %s does not have "
             "HCHO!",
             model_name);
  }

  // PM4.0 validation (SEN63C, SEN65, SEN68 don't have PM4.0)
  bool has_pm4 =
      (this->model_ == Sen6xModel::SEN62 || this->model_ == Sen6xModel::SEN66 ||
       this->model_ == Sen6xModel::SEN69C);
  if (!has_pm4 && this->pm_4_0_sensor_ != nullptr) {
    ESP_LOGW(TAG,
             "  WARNING: PM4.0 sensor configured but %s does not have PM4.0!",
             model_name);
  }
}

} // namespace sen6x
} // namespace esphome
