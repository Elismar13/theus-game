#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

// The panel on the Device that shows the state of a Run (CONTEXT.md: Status
// Board). It is write-only and driven by TFT_eSPI; the pin map and the panel's
// quirks live in docs/hardware.md.
class StatusBoard {
 public:
  // Native geometry in portrait orientation, matching the 0.96" 80x160 panel.
  static constexpr int16_t kWidth = 80;
  static constexpr int16_t kHeight = 160;

  // The backlight is dimmable in eight steps above off: level 0 is off, and
  // level kBrightnessLevelMax is full.
  static constexpr uint8_t kBrightnessLevelMax = 8;

  // Powers up the panel and the backlight, then clears the window.
  void begin();

  // Level is clamped to 0..kBrightnessLevelMax and written through LEDC PWM.
  void setBrightness(uint8_t level);

  // The bring-up proof for this panel: a border on the physical edges, corner
  // and colour markers, text at a known position, and a sweep through every
  // backlight step. Success is visible by eye.
  void selfTest();

 private:
  TFT_eSPI panel_;
};
