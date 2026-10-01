#include <unity.h>

#include "Signals.h"

namespace {

signals::Sample sample(float ax, float ay, float az) {
  signals::Sample s;
  s.ax = ax;
  s.ay = ay;
  s.az = az;
  s.gx = 0.0f;
  s.gy = 0.0f;
  s.gz = 0.0f;
  return s;
}

}  // namespace

void test_upright_reads_zero_pitch_and_one_g_vertical(void) {
  const signals::Derived derived = signals::derive(sample(0.0f, 1.0f, 0.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, derived.pitch);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, derived.vert);
}

void test_vertical_is_the_torso_up_axis(void) {
  const signals::Derived derived = signals::derive(sample(0.2f, 1.7f, -0.3f));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.7f, derived.vert);
}

void test_forward_pitch_is_positive(void) {
  // 45 degrees forward: ay = cos, az = sin.
  const float c = 0.70710678f;
  const signals::Derived derived = signals::derive(sample(0.0f, c, c));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.0f, derived.pitch);
}

void test_backward_pitch_is_negative(void) {
  const float c = 0.70710678f;
  const signals::Derived derived = signals::derive(sample(0.0f, c, -c));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -45.0f, derived.pitch);
}

void test_pitch_ignores_lateral_acceleration(void) {
  // Rolling about Z (ax) does not change the pitch about X.
  const float c = 0.70710678f;
  const signals::Derived derived = signals::derive(sample(0.5f, c, c));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 45.0f, derived.pitch);
}

void test_gyro_is_not_used_by_the_derivation(void) {
  signals::Sample still = sample(0.0f, 1.0f, 0.0f);
  signals::Sample spinning = still;
  spinning.gx = 300.0f;
  spinning.gy = -120.0f;
  spinning.gz = 45.0f;
  const signals::Derived a = signals::derive(still);
  const signals::Derived b = signals::derive(spinning);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, a.pitch, b.pitch);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, a.vert, b.vert);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_upright_reads_zero_pitch_and_one_g_vertical);
  RUN_TEST(test_vertical_is_the_torso_up_axis);
  RUN_TEST(test_forward_pitch_is_positive);
  RUN_TEST(test_backward_pitch_is_negative);
  RUN_TEST(test_pitch_ignores_lateral_acceleration);
  RUN_TEST(test_gyro_is_not_used_by_the_derivation);
  return UNITY_END();
}
