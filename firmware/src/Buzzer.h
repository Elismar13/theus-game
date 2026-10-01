#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "Sounds.h"

// The Device's passive piezo (docs/hardware.md: GPIO26, LEDC). It plays a
// pattern without blocking: play() selects it and update() advances it one
// tone at a time, so the main loop keeps sampling the button and the Sensor
// Module while a sound rings out.
//
// Mute is persisted in NVS and restored on begin(), so it survives a reboot.
class Buzzer {
 public:
  void begin();

  // Select a pattern and start it at the next update(). Ignored while muted.
  void play(sounds::Sound sound);

  // Advance the running pattern. Call often; pass a monotonic millisecond
  // clock.
  void update(uint32_t nowMs);

  // Persist the mute state. Muting silences anything already playing.
  void setMuted(bool muted);
  void toggleMute();

  bool muted() const { return muted_; }

 private:
  void stop();

  Preferences preferences_;
  sounds::Pattern current_{nullptr, 0};
  uint8_t index_ = 0;
  bool running_ = false;
  bool muted_ = false;
  uint32_t stepEndsMs_ = 0;
};
