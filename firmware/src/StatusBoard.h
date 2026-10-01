#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "Readout.h"

// The panel on the Device that shows the state of a Run (CONTEXT.md: Status
// Board). It is write-only and driven by TFT_eSPI; the pin map and the panel's
// quirks live in docs/hardware.md.
//
// It is a Status Board, not a HUD (ADR-0005): Score, High Score, Hearts and Run
// State are read between Runs and by onlookers. Each value owns a fixed region,
// and `render` repaints only the regions the read-out says changed, so a Score
// tick does not flash the whole panel.
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

  // The Link state, large enough to read between Runs: `LINK OK` or
  // `LINK LOST`. Repaints only when it actually changes.
  void showLink(bool up);

  // Paints the regions named by `changed` (readout::kAll for a full paint) from
  // the mirrored Run. Only those regions are cleared and redrawn.
  void render(const readout::State& state, uint32_t changed);

  // Repaints the whole window from scratch, Link band included, ignoring what
  // was drawn before. Used after the panel self-test, which paints over
  // everything and would otherwise leave its proof behind.
  void repaint(const readout::State& state, bool linkUp);

 private:
  void drawScore(int score);
  void drawHighScore(int highScore);
  void drawHearts(int hearts, int maxHearts);
  void drawRun(readout::RunState run);
  void drawLinkRegion(bool up);

  TFT_eSPI panel_;
  bool linkDrawn_ = false;
  bool linkUp_ = false;
};
