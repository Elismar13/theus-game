#include <stddef.h>
#include <unity.h>

#include "Sounds.h"

namespace {

using sounds::Sound;

constexpr Sound kAllSounds[] = {
    Sound::Jump,        Sound::HeartLoss, Sound::GameOver,
    Sound::Recalibration, Sound::LowBattery,
};

bool samePattern(sounds::Pattern a, sounds::Pattern b) {
  if (a.count != b.count) {
    return false;
  }
  for (uint8_t i = 0; i < a.count; ++i) {
    if (a.tones[i].frequencyHz != b.tones[i].frequencyHz ||
        a.tones[i].durationMs != b.tones[i].durationMs) {
      return false;
    }
  }
  return true;
}

}  // namespace

void test_every_sound_has_a_non_empty_pattern(void) {
  for (Sound sound : kAllSounds) {
    sounds::Pattern pattern = sounds::pattern(sound);
    TEST_ASSERT_NOT_NULL(pattern.tones);
    TEST_ASSERT_TRUE(pattern.count > 0);
  }
}

void test_patterns_are_pairwise_distinct(void) {
  constexpr size_t count = sizeof(kAllSounds) / sizeof(kAllSounds[0]);
  for (size_t i = 0; i < count; ++i) {
    for (size_t j = i + 1; j < count; ++j) {
      TEST_ASSERT_FALSE(samePattern(sounds::pattern(kAllSounds[i]),
                                    sounds::pattern(kAllSounds[j])));
    }
  }
}

void test_every_tone_has_a_duration_and_a_sane_frequency(void) {
  for (Sound sound : kAllSounds) {
    sounds::Pattern pattern = sounds::pattern(sound);
    for (uint8_t i = 0; i < pattern.count; ++i) {
      TEST_ASSERT_TRUE(pattern.tones[i].durationMs > 0);
      const uint16_t frequency = pattern.tones[i].frequencyHz;
      TEST_ASSERT_TRUE(frequency == 0 || (frequency >= 100 && frequency <= 5000));
    }
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_every_sound_has_a_non_empty_pattern);
  RUN_TEST(test_patterns_are_pairwise_distinct);
  RUN_TEST(test_every_tone_has_a_duration_and_a_sane_frequency);
  return UNITY_END();
}
