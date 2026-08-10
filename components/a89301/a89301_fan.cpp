#include "a89301_fan.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace a89301 {

static const char *const TAG = "a89301.fan";

void A89301Fan::setup() {
  // Deferred until the driver is under I2C speed control; it also runs again after a recovered
  // communication loss, which re-applies the last commanded state to the freshly reset driver.
  this->parent_->add_on_ready_callback([this]() {
    auto restored = this->restore_state_();
    if (restored.has_value())
      restored->to_call(*this).perform();
  });
}

fan::FanTraits A89301Fan::get_traits() { return {false, true, true, this->speed_count_}; }

float A89301Fan::speed_to_percent_(int speed) const {
  if (this->speed_count_ <= 1)
    return 100.0f;
  const float step = (100.0f - this->min_demand_) / (this->speed_count_ - 1);
  return this->min_demand_ + (clamp(speed, 1, this->speed_count_) - 1) * step;
}

void A89301Fan::control(const fan::FanCall &call) {
  if (auto direction = call.get_direction()) {
    this->direction = *direction;
    this->parent_->set_reverse(this->direction == fan::FanDirection::REVERSE);
  }
  if (auto speed = call.get_speed()) {
    this->speed = *speed;
    this->parent_->set_demand(this->speed_to_percent_(this->speed));
  }
  if (auto state = call.get_state()) {
    this->state = *state;
    this->parent_->set_enabled(this->state);
  }
  this->publish_state();
}

void A89301Fan::dump_config() {
  LOG_FAN("", "A89301 Fan", this);
  ESP_LOGCONFIG(TAG, "  Speed count: %d", this->speed_count_);
  ESP_LOGCONFIG(TAG, "  Minimum demand: %.1f %%", this->min_demand_);
}

}  // namespace a89301
}  // namespace esphome
