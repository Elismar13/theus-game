#pragma once

#include <stdint.h>

// The Device's audible vocabulary. The patterns are plain data so they can be
// inspected on a host and played through LEDC on the Device.
namespace sounds {

enum class Sound : uint8_t {
  Jump,
  HeartLoss,
  GameOver,
  Recalibration,
  LowBattery,
};

struct Tone {
  uint16_t frequencyHz;  // 0 is a rest: silence for the duration
  uint16_t durationMs;
};

struct Pattern {
  const Tone* tones;
  uint8_t count;
};

// The pattern for a sound. Always non-null with count > 0.
Pattern pattern(Sound sound);

}  // namespace sounds
