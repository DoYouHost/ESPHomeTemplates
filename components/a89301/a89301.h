#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <vector>

namespace esphome {
namespace a89301 {

/// Shadow register of an EEPROM word: register address = EEPROM address + 64.
static const uint8_t REG_RATED_SPEED = 72;   // [10:0] RATED_SPEED, [11] SPEED_CLOSE_LOOP, [14] DIRECTION
static const uint8_t REG_SPEED_DEMAND = 81;  // [8:0] SPEED_DEMAND, [9] I2C_SPEED_MODE
static const uint8_t REG_SENSE_RESISTOR = 84;  // [7:0] RATED_VOLTAGE, [15:8] SENSE_RESISTOR
static const uint8_t REG_STANDBY = 85;         // [15] STANDBY_DIS

/// Read-only status registers.
static const uint8_t REG_MOTOR_SPEED = 120;
static const uint8_t REG_BUS_CURRENT = 121;
static const uint8_t REG_Q_AXIS_CURRENT = 122;
static const uint8_t REG_VBB = 123;
static const uint8_t REG_TEMPERATURE = 124;
static const uint8_t REG_CONTROL_DEMAND = 125;
static const uint8_t REG_CONTROL_COMMAND = 126;
static const uint8_t REG_OPERATION_STATE = 127;

static const uint16_t DEMAND_MAX = 511;

/// state_top_level[3:0], read from register 127[15:12]. Values 4 and 8 are undocumented.
enum class A89301State : uint8_t {
  IDLE = 0,
  FIRST_CYCLE = 1,
  IPD = 2,
  SPINNING = 3,
  UNKNOWN_4 = 4,
  LOCK = 5,
  BRAKE = 6,
  SLEEP = 7,
  UNKNOWN_8 = 8,
  WINDMILL = 9,
  CHANGE_DIRECTION = 10,
  OCP = 11,
  OTP = 12,
  BAD_SYSTEM = 13,
  BRAKE_PIN = 14,
  PRE_SLEEP = 15,
};

const char *a89301_state_to_string(A89301State state);
bool a89301_state_is_fault(A89301State state);

class A89301 : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  // Runs after the I2C bus (BUS = 1000) so that setup() can already talk to the driver, but before
  // anything that could command the motor, so the brake interlock is asserted as early as possible.
  float get_setup_priority() const override { return setup_priority::IO; }

  void set_brake_pin(GPIOPin *pin) { this->brake_pin_ = pin; }
  void set_brake_when_off(bool brake_when_off) { this->brake_when_off_ = brake_when_off; }
  void set_brake_delay(uint32_t delay_ms) { this->brake_delay_ = delay_ms; }
  void set_pole_pairs(uint8_t pole_pairs) { this->pole_pairs_ = pole_pairs; }
  void set_watchdog_interval(uint32_t interval_ms) { this->watchdog_interval_ = interval_ms; }
  void set_disable_standby(bool disable) { this->disable_standby_ = disable; }
  void set_sense_resistor(float ohms) { this->sense_resistor_override_ = ohms; }
  void set_over_voltage_threshold(float volts) { this->over_voltage_threshold_ = volts; }
  void set_under_voltage_threshold(float volts) { this->under_voltage_threshold_ = volts; }

  /// Called once the driver has been configured for I2C speed control. Fires again after a
  /// communication loss has been recovered, so restored states can be re-applied.
  void add_on_ready_callback(std::function<void()> &&callback) {
    this->ready_callbacks_.push_back(std::move(callback));
  }
  bool is_ready_() const { return this->initialized_; }

  /// Enable or disable the motor. Disabling ramps the demand down to zero and engages the brake
  /// after `brake_delay`; enabling releases the brake before applying the demand.
  void set_enabled(bool enabled);
  /// Set the speed demand as a percentage of full scale (0-100).
  void set_demand(float percent);
  /// true selects the A->C->B phase order (DIRECTION = 0).
  void set_reverse(bool reverse);

  bool enabled() const { return this->enabled_; }
  float demand() const { return this->demand_percent_; }
  bool reverse() const { return this->reverse_; }

#ifdef USE_SENSOR
  SUB_SENSOR(speed)
  SUB_SENSOR(rpm)
  SUB_SENSOR(bus_current)
  SUB_SENSOR(q_axis_current)
  SUB_SENSOR(supply_voltage)
  SUB_SENSOR(temperature)
  SUB_SENSOR(speed_demand)
  SUB_SENSOR(duty)
#endif
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(fault)
  SUB_BINARY_SENSOR(spinning)
  SUB_BINARY_SENSOR(starting)
  SUB_BINARY_SENSOR(standby)
  SUB_BINARY_SENSOR(braking)
  SUB_BINARY_SENSOR(lock)
  SUB_BINARY_SENSOR(over_current)
  SUB_BINARY_SENSOR(over_temperature)
  SUB_BINARY_SENSOR(system_error)
  SUB_BINARY_SENSOR(over_voltage)
  SUB_BINARY_SENSOR(under_voltage)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(operation_state)
#endif

 protected:
  bool read_reg_(uint8_t reg, uint16_t *value);
  bool write_reg_(uint8_t reg, uint16_t value);
  bool update_reg_(uint8_t reg, uint16_t mask, uint16_t value);

  /// Puts the driver under I2C speed control with a zero demand and keeps it out of standby.
  bool initialize_driver_();
  /// Re-runs initialization after the driver was found in an unexpected state, braking meanwhile.
  void recover_driver_(const char *reason);
  bool push_demand_();
  void apply_brake_(bool engaged);
  void communication_failed_();
  void publish_status_(uint16_t raw[8]);

  GPIOPin *brake_pin_{nullptr};
  bool brake_when_off_{true};
  uint32_t brake_delay_{2000};
  uint8_t pole_pairs_{1};
  bool disable_standby_{true};
  float sense_resistor_override_{NAN};
  float over_voltage_threshold_{47.0f};
  float under_voltage_threshold_{5.5f};
  uint32_t watchdog_interval_{1000};  // 0 disables the watchdog

  bool initialized_{false};
  bool brake_engaged_{true};
  bool enabled_{false};
  float demand_percent_{0.0f};
  bool reverse_{false};
  uint8_t error_count_{0};
  uint32_t last_ok_{0};  // millis() of the last successful transfer, drives the watchdog

  /// Register 84[15:8]; sense resistor (mOhm) = value / 3.7. Cached because every current reading
  /// is scaled by it.
  uint16_t sense_resistor_raw_{0};
  std::vector<std::function<void()>> ready_callbacks_;
};

}  // namespace a89301
}  // namespace esphome
