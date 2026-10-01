#include "Sounds.h"

#include <stddef.h>

namespace {

using sounds::Sound;
using sounds::Tone;

template <size_t N>
constexpr sounds::Pattern makePattern(const Tone (&tones)[N]) {
  return {tones, static_cast<uint8_t>(N)};
}

// A chirp that rises, to sit apart from every other pattern.
constexpr Tone kJump[] = {
    {660, 35},
    {990, 70},
};

// One low, flat buzz — the audible "that hurt".
constexpr Tone kHeartLoss[] = {
    {220, 250},
};

// A long fall from mid to low.
constexpr Tone kGameOver[] = {
    {660, 120},
    {440, 120},
    {220, 300},
};

// Two rising beeps: "stand still".
constexpr Tone kRecalibration[] = {
    {440, 100},
    {0, 60},
    {660, 100},
};

// Two low beeps with a gap, deliberately below the Recalibration tones.
constexpr Tone kLowBattery[] = {
    {200, 150},
    {0, 80},
    {200, 150},
};

}  // namespace

namespace sounds {

Pattern pattern(Sound sound) {
  switch (sound) {
    case Sound::Jump:
      return makePattern(kJump);
    case Sound::HeartLoss:
      return makePattern(kHeartLoss);
    case Sound::GameOver:
      return makePattern(kGameOver);
    case Sound::Recalibration:
      return makePattern(kRecalibration);
    case Sound::LowBattery:
      return makePattern(kLowBattery);
  }
  return {nullptr, 0};
}

}  // namespace sounds
