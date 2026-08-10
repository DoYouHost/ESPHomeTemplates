#pragma once

#include "esphome/components/fan/fan.h"
#include "esphome/core/component.h"

#include "a89301.h"

namespace esphome {
namespace a89301 {

class A89301Fan : public Component, public fan::Fan {
 public:
  A89301Fan(A89301 *parent, int speed_count, float min_demand)
      : parent_(parent), speed_count_(speed_count), min_demand_(min_demand) {}

  void setup() override;
  void dump_config() override;
  fan::FanTraits get_traits() override;

 protected:
  void control(const fan::FanCall &call) override;
  /// Maps speed level 1..speed_count onto the min_demand..100% range, so the lowest level still
  /// clears the driver's speed-input OFF threshold.
  float speed_to_percent_(int speed) const;

  A89301 *parent_;
  int speed_count_;
  float min_demand_;
};

}  // namespace a89301
}  // namespace esphome
