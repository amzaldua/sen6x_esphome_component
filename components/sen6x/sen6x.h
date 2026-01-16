// SPDX-License-Identifier: MIT

#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include <functional>

namespace esphome {
namespace sen6x {

static const uint16_t SEN6X_CMD_START_MEASUREMENT = 0x0021;
static const uint16_t SEN6X_CMD_STOP_MEASUREMENT = 0x0104;
static const uint16_t SEN6X_CMD_GET_DATA_READY = 0x0202;

// Read Measured Values - Each model has a specific command (Datasheet v0.92)
static const uint16_t SEN6X_CMD_READ_SEN62 = 0x04A3; // 18 bytes: PM + RH/T
static const uint16_t SEN6X_CMD_READ_SEN63C =
    0x0471; // 21 bytes: PM + RH/T + CO2
static const uint16_t SEN6X_CMD_READ_SEN65 =
    0x0446; // 24 bytes: PM + RH/T + VOC + NOx
static const uint16_t SEN6X_CMD_READ_SEN66 =
    0x0300; // 27 bytes: PM + RH/T + VOC + NOx + CO2
static const uint16_t SEN6X_CMD_READ_SEN68 =
    0x0467; // 27 bytes: PM + RH/T + VOC + NOx + HCHO
static const uint16_t SEN6X_CMD_READ_SEN69C =
    0x04B5; // 30 bytes: PM + RH/T + VOC + NOx + CO2 + HCHO
static const uint16_t SEN6X_CMD_GET_PRODUCT_NAME = 0xD014;
static const uint16_t SEN6X_CMD_GET_SERIAL_NUMBER = 0xD033;
static const uint16_t SEN6X_CMD_GET_STATUS = 0xD206;
static const uint16_t SEN6X_CMD_GET_AMBIENT_PRESSURE = 0x6720;
static const uint16_t SEN6X_CMD_GET_SENSOR_ALTITUDE = 0x6736;
// New Commands
static const uint16_t SEN6X_CMD_START_FAN_CLEANING = 0x5607;
static const uint16_t SEN6X_CMD_DEVICE_RESET = 0xD304;
static const uint16_t SEN6X_CMD_SET_TEMP_OFFSET = 0x60B2;
// CO2 Calibration Commands (SEN63C, SEN66, SEN69C only)
static const uint16_t SEN6X_CMD_FORCED_CO2_RECAL = 0x6707;
static const uint16_t SEN6X_CMD_CO2_FACTORY_RESET = 0x6754;
static const uint16_t SEN6X_CMD_GET_CO2_ASC = 0x6711;
static const uint16_t SEN6X_CMD_SET_CO2_ASC = 0x6711;
// VOC Algorithm State (for baseline persistence)
static const uint16_t SEN6X_CMD_VOC_ALGORITHM_STATE = 0x6181;

// Optional Commands (for full datasheet compliance)
static const uint16_t SEN6X_CMD_GET_VERSION = 0xD100;
static const uint16_t SEN6X_CMD_READ_AND_CLEAR_STATUS = 0xD210;
static const uint16_t SEN6X_CMD_NUMBER_CONCENTRATION = 0x0316;
static const uint16_t SEN6X_CMD_ACTIVATE_SHT_HEATER = 0x6765;
static const uint16_t SEN6X_CMD_GET_SHT_HEATER_MEASUREMENTS = 0x6790;

// Store baseline interval and threshold (same as SEN5x official)
static const uint32_t SHORTEST_BASELINE_STORE_INTERVAL = 10800; // 3 hours
static const uint32_t MAXIMUM_STORAGE_DIFF = 50;

// Structure for storing VOC baseline state
struct Sen6xBaselines {
  int32_t state0;
  int32_t state1;
} __attribute__((packed));

// Structure for temperature compensation parameters (same as SEN5x official)
struct TemperatureCompensation {
  int16_t offset;                  // Scaled x200 (°C)
  int16_t normalized_offset_slope; // Scaled x10000
  uint16_t time_constant;          // Seconds
};

// Structure for VOC/NOx algorithm tuning (6 parameters per Sensirion datasheet)
struct GasTuning {
  int16_t index_offset;
  int16_t
      learning_time_offset_hours; // 12 default, 720 for WELL Building Standard
  int16_t learning_time_gain_hours;
  int16_t gating_max_duration_minutes;
  int16_t std_initial;
  int16_t gain_factor;
};

// Structure for RHT Acceleration parameters (4 parameters per Sensirion
// datasheet) Command 0x6100 - Set Temperature Acceleration Parameters These are
// volatile (reset to defaults on power cycle)
// All values are scaled by 10: real value = stored value / 10
static const uint16_t SEN6X_CMD_SET_RHT_ACCELERATION = 0x6100;

struct RhtAcceleration {
  uint16_t k;  // Filter constant K (scaled x10, real K = k/10)
  uint16_t p;  // Filter constant P (scaled x10, real P = p/10)
  uint16_t t1; // Time constant T1 (scaled x10, T1[s] = t1/10)
  uint16_t t2; // Time constant T2 (scaled x10, T2[s] = t2/10)
};

// SEN6x Model Enum (per Sensirion Datasheet v0.91+)
enum class Sen6xModel : uint8_t {
  SEN62 = 0,  // PM + RH/T
  SEN63C = 1, // PM + RH/T + CO2
  SEN65 = 2,  // PM + RH/T + VOC + NOx
  SEN66 = 3,  // PM + RH/T + VOC + NOx + CO2 (default)
  SEN68 = 4,  // PM + RH/T + VOC + NOx + HCHO
  SEN69C = 5, // PM + RH/T + VOC + NOx + CO2 + HCHO
};

class Sen6xButton : public esphome::button::Button {
public:
  void set_press_callback(std::function<void()> &&callback) {
    callback_ = callback;
  }

protected:
  void press_action() override {
    if (this->callback_)
      this->callback_();
  }
  std::function<void()> callback_;
};

class Sen6xNumber : public esphome::number::Number {
public:
  void set_control_callback(std::function<void(float)> &&callback) {
    callback_ = callback;
  }

protected:
  void control(float value) override {
    this->publish_state(value);
    if (this->callback_)
      this->callback_(value);
  }
  std::function<void(float)> callback_;
};

class Sen6xSwitch : public switch_::Switch {
public:
  void set_write_callback(std::function<void(bool)> &&callback) {
    callback_ = callback;
  }

protected:
  void write_state(bool state) override {
    if (this->callback_) {
      this->callback_(state);
    }
    this->publish_state(state);
  }
  std::function<void(bool)> callback_;
};

class Sen6xComponent : public PollingComponent, public esphome::i2c::I2CDevice {
public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_model(Sen6xModel model) { model_ = model; }

  void set_pm_1_0_sensor(esphome::sensor::Sensor *pm_1_0) {
    pm_1_0_sensor_ = pm_1_0;
  }
  void set_pm_2_5_sensor(esphome::sensor::Sensor *pm_2_5) {
    pm_2_5_sensor_ = pm_2_5;
  }
  void set_pm_4_0_sensor(esphome::sensor::Sensor *pm_4_0) {
    pm_4_0_sensor_ = pm_4_0;
  }
  void set_pm_10_0_sensor(esphome::sensor::Sensor *pm_10_0) {
    pm_10_0_sensor_ = pm_10_0;
  }
  void set_humidity_sensor(esphome::sensor::Sensor *humidity) {
    humidity_sensor_ = humidity;
  }
  void set_temperature_sensor(esphome::sensor::Sensor *temperature) {
    temperature_sensor_ = temperature;
  }
  void set_voc_index_sensor(esphome::sensor::Sensor *voc_index) {
    voc_index_sensor_ = voc_index;
  }
  void set_nox_index_sensor(esphome::sensor::Sensor *nox_index) {
    nox_sensor_ = nox_index;
  }
  void set_co2_sensor(esphome::sensor::Sensor *co2) { co2_sensor_ = co2; }
  void set_formaldehyde_sensor(esphome::sensor::Sensor *hcho) {
    formaldehyde_sensor_ = hcho;
  }

  void set_ambient_pressure_sensor(esphome::sensor::Sensor *sens) {
    ambient_pressure_sensor_ = sens;
  }
  void set_sensor_altitude_sensor(esphome::sensor::Sensor *sens) {
    sensor_altitude_sensor_ = sens;
  }

  void set_tvoc_well_sensor(sensor::Sensor *sens) { well_tvoc_sensor_ = sens; }
  void set_tvoc_reset_sensor(sensor::Sensor *sens) {
    reset_tvoc_sensor_ = sens;
  }
  void set_tvoc_ethanol_sensor(sensor::Sensor *sens) {
    tvoc_ethanol_sensor_ = sens;
  }

  // Number Concentration sensors (particles/cm³)
  void set_nc_0_5_sensor(sensor::Sensor *sens) { nc_0_5_sensor_ = sens; }
  void set_nc_1_0_sensor(sensor::Sensor *sens) { nc_1_0_sensor_ = sens; }
  void set_nc_2_5_sensor(sensor::Sensor *sens) { nc_2_5_sensor_ = sens; }
  void set_nc_4_0_sensor(sensor::Sensor *sens) { nc_4_0_sensor_ = sens; }
  void set_nc_10_0_sensor(sensor::Sensor *sens) { nc_10_0_sensor_ = sens; }

  // Firmware version text sensor
  void set_firmware_version_sensor(text_sensor::TextSensor *sens) {
    firmware_version_sensor_ = sens;
  }

  void set_outdoor_co2_ppm(float ppm) { outdoor_co2_ppm_ = ppm; }

  void set_voc_algorithm_tuning_720h(bool enable) {
    voc_algorithm_tuning_720h_ = enable;
  }

  // Temperature compensation parameters (slope + time_constant from YAML)
  void set_temperature_compensation(float offset, float normalized_offset_slope,
                                    uint16_t time_constant) {
    TemperatureCompensation temp_comp;
    temp_comp.offset = static_cast<int16_t>(offset * 200);
    temp_comp.normalized_offset_slope =
        static_cast<int16_t>(normalized_offset_slope * 10000);
    temp_comp.time_constant = time_constant;
    temperature_compensation_ = temp_comp;
  }

  // VOC algorithm tuning parameters (from YAML, 6 parameters)
  void set_voc_algorithm_tuning(int16_t index_offset,
                                int16_t learning_time_offset_hours,
                                int16_t learning_time_gain_hours,
                                int16_t gating_max_duration_minutes,
                                int16_t std_initial, int16_t gain_factor) {
    GasTuning tuning;
    tuning.index_offset = index_offset;
    tuning.learning_time_offset_hours = learning_time_offset_hours;
    tuning.learning_time_gain_hours = learning_time_gain_hours;
    tuning.gating_max_duration_minutes = gating_max_duration_minutes;
    tuning.std_initial = std_initial;
    tuning.gain_factor = gain_factor;
    voc_tuning_ = tuning;
    // Update 720h flag for logging based on learning_time_offset_hours
    voc_algorithm_tuning_720h_ = (learning_time_offset_hours >= 720);
  }

  // NOx algorithm tuning parameters (from YAML, 6 parameters)
  void set_nox_algorithm_tuning(int16_t index_offset,
                                int16_t learning_time_offset_hours,
                                int16_t learning_time_gain_hours,
                                int16_t gating_max_duration_minutes,
                                int16_t std_initial, int16_t gain_factor) {
    GasTuning tuning;
    tuning.index_offset = index_offset;
    tuning.learning_time_offset_hours = learning_time_offset_hours;
    tuning.learning_time_gain_hours = learning_time_gain_hours;
    tuning.gating_max_duration_minutes = gating_max_duration_minutes;
    tuning.std_initial = std_initial;
    tuning.gain_factor = gain_factor;
    nox_tuning_ = tuning;
  }

  void set_product_name_text_sensor(esphome::text_sensor::TextSensor *sens) {
    product_name_text_sensor_ = sens;
  }
  void set_serial_number_text_sensor(esphome::text_sensor::TextSensor *sens) {
    serial_number_text_sensor_ = sens;
  }
  void set_status_text_sensor(esphome::text_sensor::TextSensor *sens) {
    status_text_sensor_ = sens;
  }

  void set_fan_error_binary_sensor(esphome::binary_sensor::BinarySensor *sens) {
    fan_error_binary_sensor_ = sens;
  }
  void
  set_fan_warning_binary_sensor(esphome::binary_sensor::BinarySensor *sens) {
    fan_warning_binary_sensor_ = sens;
  }
  void set_gas_error_binary_sensor(esphome::binary_sensor::BinarySensor *sens) {
    gas_error_binary_sensor_ = sens;
  }
  void set_rht_error_binary_sensor(esphome::binary_sensor::BinarySensor *sens) {
    rht_error_binary_sensor_ = sens;
  }
  void set_pm_error_binary_sensor(esphome::binary_sensor::BinarySensor *sens) {
    pm_error_binary_sensor_ = sens;
  }
  void
  set_laser_error_binary_sensor(esphome::binary_sensor::BinarySensor *sens) {
    laser_error_binary_sensor_ = sens;
  }
  void set_fan_cleaning_active_binary_sensor(
      esphome::binary_sensor::BinarySensor *sens) {
    fan_cleaning_active_binary_sensor_ = sens;
  }

  // Setters for Buttons
  void set_fan_cleaning_button(Sen6xButton *btn) { fan_cleaning_button_ = btn; }
  void set_device_reset_button(Sen6xButton *btn) { device_reset_button_ = btn; }
  void set_reset_preferences_button(Sen6xButton *btn) {
    reset_preferences_button_ = btn;
  }
  void set_force_co2_calibration_button(Sen6xButton *btn) {
    force_co2_calibration_button_ = btn;
  }
  void set_co2_factory_reset_button(Sen6xButton *btn) {
    co2_factory_reset_button_ = btn;
  }
  void set_sht_heater_button(Sen6xButton *btn) { sht_heater_button_ = btn; }
  void set_clear_device_status_button(Sen6xButton *btn) {
    clear_device_status_button_ = btn;
  }

  // Setters for Numbers
  void set_altitude_compensation_number(Sen6xNumber *n) {
    altitude_compensation_number_ = n;
  }
  void set_ambient_pressure_compensation_number(Sen6xNumber *n) {
    ambient_pressure_compensation_number_ = n;
  }
  void set_temperature_offset_number(Sen6xNumber *n) {
    temperature_offset_number_ = n;
  }
  void set_outdoor_co2_reference_number(Sen6xNumber *n) {
    outdoor_co2_reference_number_ = n;
  }

  // Note: set_voc_tuning_switch removed - VOC tuning is now YAML-only
  void set_co2_asc_switch(Sen6xSwitch *sw) { co2_asc_switch_ = sw; }
  void set_auto_cleaning_switch(Sen6xSwitch *sw) { auto_cleaning_switch_ = sw; }
  void set_auto_cleaning_interval(uint32_t interval_ms) {
    auto_cleaning_interval_ms_ = interval_ms;
  }

  // Public method for external barometric sensor integration
  // Allows feeding pressure from BME280/BMP280/etc for CO2 compensation
  bool set_ambient_pressure(float pressure_hpa);

  // Set external pressure source sensor (configured from YAML)
  // The component will auto-subscribe to this sensor's state changes
  void set_pressure_source(sensor::Sensor *source) {
    pressure_source_ = source;
  }

  // Store baseline configuration (same as SEN5x official)
  void set_store_baseline(bool store_baseline) {
    store_baseline_ = store_baseline;
  }

  // RHT Acceleration configuration (YAML-only, volatile - applied on each boot)
  void set_rht_acceleration(RhtAcceleration rht) { rht_acceleration_ = rht; }

protected:
  void read_device_identity_();
  void read_device_status_();
  void read_device_configuration_();

  // Action Helpers
  void start_fan_cleaning_();
  void continue_fan_cleaning_();
  void execute_device_reset_();
  bool write_altitude_compensation_(float altitude);
  bool write_ambient_pressure_compensation_(float pressure);
  bool write_temperature_offset_(float offset);
  bool
  write_temperature_compensation_(const TemperatureCompensation &compensation);
  bool write_voc_algorithm_tuning_(const GasTuning &tuning);
  bool write_nox_algorithm_tuning_(const GasTuning &tuning);
  bool write_rht_acceleration_(const RhtAcceleration &rht);
  bool write_co2_asc_(bool enabled);
  bool perform_forced_co2_calibration_(uint16_t reference_ppm);
  void configure_auto_cleaning_(bool enabled);
  void execute_preferences_reset_();

  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_4_0_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *voc_index_sensor_{nullptr};
  sensor::Sensor *nox_sensor_{nullptr};
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *formaldehyde_sensor_{nullptr};

  sensor::Sensor *ambient_pressure_sensor_{nullptr};
  sensor::Sensor *sensor_altitude_sensor_{nullptr};

  sensor::Sensor *well_tvoc_sensor_{nullptr};
  sensor::Sensor *reset_tvoc_sensor_{nullptr};
  sensor::Sensor *tvoc_ethanol_sensor_{nullptr};

  // Number Concentration sensors (particles/cm³)
  sensor::Sensor *nc_0_5_sensor_{nullptr};
  sensor::Sensor *nc_1_0_sensor_{nullptr};
  sensor::Sensor *nc_2_5_sensor_{nullptr};
  sensor::Sensor *nc_4_0_sensor_{nullptr};
  sensor::Sensor *nc_10_0_sensor_{nullptr};

  // Firmware version text sensor
  text_sensor::TextSensor *firmware_version_sensor_{nullptr};

  float outdoor_co2_ppm_{400.0f};
  Sen6xModel model_{Sen6xModel::SEN66}; // Default to SEN66
  sensor::Sensor *pressure_source_{
      nullptr}; // External pressure sensor for CO2 compensation

  bool voc_algorithm_tuning_720h_{false};
  optional<TemperatureCompensation>
      temperature_compensation_;   // From YAML (slope + time_constant)
  optional<GasTuning> voc_tuning_; // From YAML (advanced VOC tuning)
  optional<GasTuning> nox_tuning_; // From YAML (advanced NOx tuning)
  bool fan_cleaning_active_state_{false};
  uint32_t last_fan_cleaning_end_time_{0};

  text_sensor::TextSensor *product_name_text_sensor_{nullptr};
  text_sensor::TextSensor *serial_number_text_sensor_{nullptr};
  text_sensor::TextSensor *status_text_sensor_{nullptr};

  binary_sensor::BinarySensor *fan_error_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *fan_warning_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *gas_error_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *rht_error_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *pm_error_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *laser_error_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *fan_cleaning_active_binary_sensor_{nullptr};

  Sen6xButton *fan_cleaning_button_{nullptr};
  Sen6xButton *device_reset_button_{nullptr};
  Sen6xButton *reset_preferences_button_{nullptr};
  Sen6xButton *force_co2_calibration_button_{nullptr};
  Sen6xButton *co2_factory_reset_button_{nullptr};
  Sen6xButton *sht_heater_button_{nullptr};
  Sen6xButton *clear_device_status_button_{nullptr};

  Sen6xNumber *altitude_compensation_number_{nullptr};
  Sen6xNumber *ambient_pressure_compensation_number_{nullptr};
  Sen6xNumber *temperature_offset_number_{nullptr};
  Sen6xNumber *outdoor_co2_reference_number_{nullptr};

  // voc_tuning_switch_ removed - now YAML-only under
  // sensor/voc_index/algorithm_tuning
  Sen6xSwitch *co2_asc_switch_{nullptr};
  Sen6xSwitch *auto_cleaning_switch_{nullptr};
  uint32_t auto_cleaning_interval_ms_{604800000}; // Default 7 days in ms

  ESPPreferenceObject altitude_preference_;
  ESPPreferenceObject ambient_pressure_preference_;
  ESPPreferenceObject temperature_offset_preference_;
  // voc_tuning_preference_ removed - no persistence needed for YAML-only config
  ESPPreferenceObject co2_asc_preference_;
  ESPPreferenceObject auto_cleaning_preference_;
  ESPPreferenceObject outdoor_co2_reference_preference_;

  // VOC baseline persistence (same as SEN5x official)
  bool store_baseline_{true}; // Default: true
  Sen6xBaselines voc_baselines_storage_{0, 0};
  ESPPreferenceObject voc_baseline_preference_;
  uint32_t seconds_since_last_baseline_store_{0};

  // Preference hash (based on serial number, same as SEN5x official)
  // Ensures unique storage per sensor, avoiding conflicts with multiple sensors
  uint32_t preference_hash_{0};

  // RHT Acceleration configuration (YAML-only, volatile)
  optional<RhtAcceleration> rht_acceleration_;

  // Deferred restore state (apply after first successful measurement)
  float pending_altitude_{NAN};
  float pending_temp_offset_{NAN};
  bool altitude_restored_{false};
  bool temp_offset_restored_{false};

  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    CRC_CHECK_FAILED,
  } error_code_{NONE};
  bool start_measurement_();
  bool write_command_(uint16_t command);
  bool write_command_with_data_(uint16_t command, uint16_t data); // New helper

  // Helpers
  bool read_bytes_(uint16_t command, uint8_t *buffer, uint8_t len);
  bool read_words_(uint16_t command, uint16_t *data, uint8_t words);
  bool read_measurement_data_(uint16_t *data, uint8_t len);
  uint8_t get_measurement_word_count_(); // Returns word count based on model
};

} // namespace sen6x
} // namespace esphome
