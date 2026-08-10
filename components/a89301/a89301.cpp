#include "a89301.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cmath>

namespace esphome {
namespace a89301 {

static const char *const TAG = "a89301";

/// Consecutive failed transfers before the driver is considered gone and the brake is engaged.
static const uint8_t MAX_ERRORS = 3;

const char *a89301_state_to_string(A89301State state) {
  switch (state) {
    case A89301State::IDLE:
      return "Idle";
    case A89301State::FIRST_CYCLE:
      return "First cycle (open loop)";
    case A89301State::IPD:
      return "Initial position detection";
    case A89301State::SPINNING:
      return "Spinning";
    case A89301State::LOCK:
      return "Lock detected";
    case A89301State::BRAKE:
      return "Braking";
    case A89301State::SLEEP:
      return "Sleep";
    case A89301State::WINDMILL:
      return "Windmill startup";
    case A89301State::CHANGE_DIRECTION:
      return "Changing direction";
    case A89301State::OCP:
      return "Overcurrent protection";
    case A89301State::OTP:
      return "Overtemperature protection";
    case A89301State::BAD_SYSTEM:
      return "System error (VREG/VCP)";
    case A89301State::BRAKE_PIN:
      return "Braking (BRAKE pin)";
    case A89301State::PRE_SLEEP:
      return "Soft off";
    default:
      return "Unknown";
  }
}

bool a89301_state_is_fault(A89301State state) {
  switch (state) {
    case A89301State::LOCK:
    case A89301State::OCP:
    case A89301State::OTP:
    case A89301State::BAD_SYSTEM:
      return true;
    default:
      return false;
  }
}

// The A89301 needs a STOP between the register address and the data phase, so the read cannot use
// I2CDevice::read_register(), which emits a repeated START.
bool A89301::read_reg_(uint8_t reg, uint16_t *value) {
  if (this->write(&reg, 1) != i2c::ERROR_OK)
    return false;
  uint8_t buffer[2];
  if (this->read(buffer, 2) != i2c::ERROR_OK)
    return false;
  *value = (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
  return true;
}

bool A89301::write_reg_(uint8_t reg, uint16_t value) {
  const uint8_t buffer[2] = {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
  return this->write_register(reg, buffer, 2) == i2c::ERROR_OK;
}

bool A89301::update_reg_(uint8_t reg, uint16_t mask, uint16_t value) {
  uint16_t current;
  if (!this->read_reg_(reg, &current))
    return false;
  const uint16_t updated = (current & ~mask) | (value & mask);
  if (updated == current)
    return true;
  return this->write_reg_(reg, updated);
}

void A89301::setup() {
  // Before anything else: the motor must not be able to spin while the driver is unconfigured.
  if (this->brake_pin_ != nullptr) {
    this->brake_pin_->setup();
    this->brake_pin_->digital_write(true);
  }
  this->brake_engaged_ = true;

  if (!this->initialize_driver_()) {
    ESP_LOGW(TAG, "Driver not responding, retrying on next update");
    this->status_set_warning();
  }
}

bool A89301::initialize_driver_() {
  uint16_t probe;
  if (!this->read_reg_(REG_OPERATION_STATE, &probe))
    return false;

  // Standby powers down the whole IC, including the I2C interface, so a zero demand would cut the
  // link and the driver could only be woken by a power cycle.
  if (this->disable_standby_ && !this->update_reg_(REG_STANDBY, 1 << 15, 1 << 15)) {
    ESP_LOGW(TAG, "Failed to disable standby mode");
    return false;
  }

  if (std::isnan(this->sense_resistor_override_)) {
    uint16_t raw;
    if (!this->read_reg_(REG_SENSE_RESISTOR, &raw))
      return false;
    this->sense_resistor_raw_ = raw >> 8;
  } else {
    this->sense_resistor_raw_ = static_cast<uint16_t>(lroundf(this->sense_resistor_override_ * 1000.0f * 3.7f));
  }
  if (this->sense_resistor_raw_ == 0)
    ESP_LOGW(TAG, "Sense resistor reads as 0, current sensors will report NaN");

  if (!this->update_reg_(REG_RATED_SPEED, 1 << 14, this->reverse_ ? 0 : (1 << 14)))
    return false;

  // Takes speed control away from the SPD pin (which doubles as SCL) and hands it to register 81.
  // The demand starts at zero even if the motor was enabled before a communication loss; the real
  // demand is re-applied by the ready callbacks below, once the brake has been released again.
  if (!this->write_reg_(REG_SPEED_DEMAND, 1 << 9))
    return false;

  this->initialized_ = true;
  this->error_count_ = 0;
  this->last_ok_ = millis();
  this->status_clear_warning();
  ESP_LOGI(TAG, "Driver initialized (sense resistor raw 0x%02X)", this->sense_resistor_raw_);

  for (auto &callback : this->ready_callbacks_)
    callback();
  return true;
}

bool A89301::push_demand_() {
  const float percent = this->enabled_ ? clamp(this->demand_percent_, 0.0f, 100.0f) : 0.0f;
  const uint16_t raw = static_cast<uint16_t>(lroundf(percent * DEMAND_MAX / 100.0f));
  return this->write_reg_(REG_SPEED_DEMAND, (1 << 9) | (raw & DEMAND_MAX));
}

void A89301::apply_brake_(bool engaged) {
  if (this->brake_engaged_ == engaged)
    return;
  this->brake_engaged_ = engaged;
  if (this->brake_pin_ != nullptr)
    this->brake_pin_->digital_write(engaged);
  ESP_LOGD(TAG, "Brake %s", engaged ? "engaged" : "released");
}

void A89301::communication_failed_() {
  if (++this->error_count_ < MAX_ERRORS)
    return;
  if (this->initialized_)
    ESP_LOGW(TAG, "Lost communication with the driver, engaging brake");
  this->initialized_ = false;
  this->cancel_timeout("brake");
  this->apply_brake_(true);
  this->status_set_warning();
}

void A89301::recover_driver_(const char *reason) {
  ESP_LOGW(TAG, "%s, re-initializing", reason);
  this->initialized_ = false;
  this->cancel_timeout("brake");
  this->apply_brake_(true);
  // Restores I2C speed mode with a zero demand; the ready callbacks then re-apply the commanded
  // state, which releases the brake again if the motor is supposed to be running.
  if (!this->initialize_driver_())
    this->communication_failed_();
}

// Polls register 81 on its own schedule so that the reaction time of the safety interlock does not
// depend on `update_interval`. The same read answers two questions: whether the driver still
// responds, and whether it is still under I2C speed control. A driver that browned out and reloaded
// its EEPROM reverts to SPD-pin control - and SPD is the SCL line, which idles high, i.e. full
// speed demand - while still acknowledging I2C perfectly well.
void A89301::loop() {
  if (this->watchdog_interval_ == 0 || !this->initialized_)
    return;
  if (millis() - this->last_ok_ < this->watchdog_interval_)
    return;

  uint16_t demand_reg;
  if (!this->read_reg_(REG_SPEED_DEMAND, &demand_reg)) {
    this->communication_failed_();
    return;
  }

  this->last_ok_ = millis();
  this->error_count_ = 0;
  if ((demand_reg & (1 << 9)) == 0)
    this->recover_driver_("Driver left I2C speed mode (power cycle?)");
}

void A89301::set_enabled(bool enabled) {
  this->enabled_ = enabled;

  if (!this->initialized_) {
    // Without a working link the motor cannot be commanded, so the brake stays on regardless of
    // what was requested. The state is re-applied from the ready callback once the link is back.
    this->cancel_timeout("brake");
    this->apply_brake_(true);
    return;
  }

  if (enabled) {
    this->cancel_timeout("brake");
    // The brake overrides the speed demand, so it has to be released before the demand is sent.
    this->apply_brake_(false);
  }

  if (!this->push_demand_()) {
    this->communication_failed_();
    return;
  }

  if (!enabled && this->brake_when_off_) {
    // Braking a spinning motor stresses the MOSFETs, so let the driver coast down first.
    this->set_timeout("brake", this->brake_delay_, [this]() { this->apply_brake_(true); });
  }
}

void A89301::set_demand(float percent) {
  this->demand_percent_ = clamp(percent, 0.0f, 100.0f);
  if (this->initialized_ && this->enabled_ && !this->push_demand_())
    this->communication_failed_();
}

void A89301::set_reverse(bool reverse) {
  this->reverse_ = reverse;
  if (this->initialized_ && !this->update_reg_(REG_RATED_SPEED, 1 << 14, reverse ? 0 : (1 << 14)))
    this->communication_failed_();
}

void A89301::update() {
  if (!this->initialized_) {
    if (!this->initialize_driver_())
      this->communication_failed_();
    return;
  }

  uint16_t raw[8];
  for (uint8_t i = 0; i < 8; i++) {
    if (!this->read_reg_(REG_MOTOR_SPEED + i, &raw[i])) {
      this->communication_failed_();
      return;
    }
  }

  this->error_count_ = 0;
  this->last_ok_ = millis();
  this->status_clear_warning();
  this->publish_status_(raw);
}

void A89301::publish_status_(uint16_t raw[8]) {
  ESP_LOGV(TAG, "Readback 120..127: %04X %04X %04X %04X %04X %04X %04X %04X", raw[0], raw[1], raw[2], raw[3], raw[4],
           raw[5], raw[6], raw[7]);

  const float speed_hz = raw[0] * 0.530f;
  const float supply_voltage = raw[3] / 5.0f;
  const float current_scale =
      this->sense_resistor_raw_ == 0 ? NAN : 125.0f / (static_cast<float>(this->sense_resistor_raw_) * 1000.0f);
  const auto state = static_cast<A89301State>((raw[7] >> 12) & 0x0F);

#ifdef USE_SENSOR
  if (this->speed_sensor_ != nullptr)
    this->speed_sensor_->publish_state(speed_hz);
  if (this->rpm_sensor_ != nullptr)
    this->rpm_sensor_->publish_state(speed_hz * 60.0f / this->pole_pairs_);
  if (this->bus_current_sensor_ != nullptr)
    this->bus_current_sensor_->publish_state(raw[1] * current_scale);
  if (this->q_axis_current_sensor_ != nullptr)
    this->q_axis_current_sensor_->publish_state(raw[2] * current_scale);
  if (this->supply_voltage_sensor_ != nullptr)
    this->supply_voltage_sensor_->publish_state(supply_voltage);
  if (this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(static_cast<float>(raw[4]) - 53.0f);
  if (this->speed_demand_sensor_ != nullptr)
    this->speed_demand_sensor_->publish_state((raw[5] & DEMAND_MAX) * 100.0f / DEMAND_MAX);
  if (this->duty_sensor_ != nullptr)
    this->duty_sensor_->publish_state((raw[6] & DEMAND_MAX) * 100.0f / DEMAND_MAX);
#endif

#ifdef USE_BINARY_SENSOR
  if (this->fault_binary_sensor_ != nullptr)
    this->fault_binary_sensor_->publish_state(a89301_state_is_fault(state));
  if (this->spinning_binary_sensor_ != nullptr)
    this->spinning_binary_sensor_->publish_state(state == A89301State::SPINNING);
  if (this->starting_binary_sensor_ != nullptr) {
    this->starting_binary_sensor_->publish_state(state == A89301State::FIRST_CYCLE || state == A89301State::IPD ||
                                                 state == A89301State::WINDMILL);
  }
  if (this->standby_binary_sensor_ != nullptr) {
    this->standby_binary_sensor_->publish_state(state == A89301State::IDLE || state == A89301State::SLEEP ||
                                                state == A89301State::PRE_SLEEP);
  }
  if (this->braking_binary_sensor_ != nullptr) {
    this->braking_binary_sensor_->publish_state(state == A89301State::BRAKE || state == A89301State::BRAKE_PIN);
  }
  if (this->lock_binary_sensor_ != nullptr)
    this->lock_binary_sensor_->publish_state(state == A89301State::LOCK);
  if (this->over_current_binary_sensor_ != nullptr)
    this->over_current_binary_sensor_->publish_state(state == A89301State::OCP);
  if (this->over_temperature_binary_sensor_ != nullptr)
    this->over_temperature_binary_sensor_->publish_state(state == A89301State::OTP);
  if (this->system_error_binary_sensor_ != nullptr)
    this->system_error_binary_sensor_->publish_state(state == A89301State::BAD_SYSTEM);
  // The A89301 exposes no OVP/UVP status bit, so both are derived from the VBB readback.
  if (this->over_voltage_binary_sensor_ != nullptr)
    this->over_voltage_binary_sensor_->publish_state(supply_voltage >= this->over_voltage_threshold_);
  if (this->under_voltage_binary_sensor_ != nullptr)
    this->under_voltage_binary_sensor_->publish_state(supply_voltage <= this->under_voltage_threshold_);
#endif

#ifdef USE_TEXT_SENSOR
  if (this->operation_state_text_sensor_ != nullptr)
    this->operation_state_text_sensor_->publish_state(a89301_state_to_string(state));
#endif
}

void A89301::dump_config() {
  ESP_LOGCONFIG(TAG, "A89301:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Pole pairs: %u", this->pole_pairs_);
  ESP_LOGCONFIG(TAG, "  Disable standby: %s", YESNO(this->disable_standby_));
  if (this->watchdog_interval_ == 0) {
    ESP_LOGCONFIG(TAG, "  Watchdog: disabled");
  } else {
    ESP_LOGCONFIG(TAG, "  Watchdog interval: %" PRIu32 " ms", this->watchdog_interval_);
  }
  if (this->brake_pin_ != nullptr) {
    LOG_PIN("  Brake pin: ", this->brake_pin_);
    ESP_LOGCONFIG(TAG, "  Brake when off: %s", YESNO(this->brake_when_off_));
    ESP_LOGCONFIG(TAG, "  Brake delay: %" PRIu32 " ms", this->brake_delay_);
  }
  ESP_LOGCONFIG(TAG, "  Over voltage threshold: %.1f V", this->over_voltage_threshold_);
  ESP_LOGCONFIG(TAG, "  Under voltage threshold: %.1f V", this->under_voltage_threshold_);
  if (this->is_failed() || !this->initialized_)
    ESP_LOGE(TAG, "  Communication with A89301 failed!");

#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Speed", this->speed_sensor_);
  LOG_SENSOR("  ", "RPM", this->rpm_sensor_);
  LOG_SENSOR("  ", "Bus current", this->bus_current_sensor_);
  LOG_SENSOR("  ", "Q-axis current", this->q_axis_current_sensor_);
  LOG_SENSOR("  ", "Supply voltage", this->supply_voltage_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Speed demand", this->speed_demand_sensor_);
  LOG_SENSOR("  ", "Duty", this->duty_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Fault", this->fault_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Spinning", this->spinning_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Starting", this->starting_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Standby", this->standby_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Braking", this->braking_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Lock", this->lock_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Over current", this->over_current_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Over temperature", this->over_temperature_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "System error", this->system_error_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Over voltage", this->over_voltage_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Under voltage", this->under_voltage_binary_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Operation state", this->operation_state_text_sensor_);
#endif
}

}  // namespace a89301
}  // namespace esphome
