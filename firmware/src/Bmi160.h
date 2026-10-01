#pragma once

#include <stdint.h>

#include "Signals.h"

// The BMI160 over I2C (docs/hardware.md: SDA 21, SCL 22, address 0x68).
//
// Wraps the BMI160-Arduino library and scales its raw counts into the units
// `signals::Sample` expects. The library calls Wire.begin() with the ESP32's
// default I2C pins, which are the ones the Device uses.
class Bmi160 {
 public:
  // Configures the sensor for 100 Hz acceleration and angular rate. Returns
  // false when the sensor does not answer on the bus; callers should not read
  // until it does.
  bool begin();

  // Reads one sample. The library's read does not report I2C errors, so a
  // missing sensor reads as zeros; `begin()` is the presence check.
  void read(signals::Sample& out);
};
