#include <initializer_list>
#include <math.h>
#include <unity.h>
#include <vector>

#include "Classifier.h"
#include "Thresholds.h"

// The Edge Classifier's acceptance criteria (#11). The sequences below stand in
// for IMU traces recorded on hardware; swapping in a real recording is a change
// of data only.
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kRad = kPi / 180.0f;

// A sample that derives to the given torso pitch (deg) and torso-up
// acceleration (g). `Signals::derive` reads pitch as atan2(az, ay) and vert as
// ay, so az follows from the pitch.
signals::Sample sampleAt(float pitchDeg, float vert) {
  signals::Sample sample;
  sample.ax = 0.0f;
  sample.ay = vert;
  sample.az = vert * tanf(pitchDeg * kRad);
  sample.gx = 0.0f;
  sample.gy = 0.0f;
  sample.gz = 0.0f;
  return sample;
}

signals::Sample rest() {
  return sampleAt(0.0f, 1.0f);
}

struct Step {
  uint32_t nowMs;
  signals::Sample sample;
};

std::vector<Step> atInterval(uint32_t fromMs, uint32_t toMs, uint32_t stepMs, float pitchDeg,
                             float vert) {
  std::vector<Step> steps;
  for (uint32_t t = fromMs; t <= toMs; t += stepMs) {
    steps.push_back({t, sampleAt(pitchDeg, vert)});
  }
  return steps;
}

std::vector<classifier::Intent> run(const std::vector<Step>& steps,
                                    const classifier::Baseline& baseline = classifier::Baseline{},
                                    const thresholds::Values& values = thresholds::defaults()) {
  classifier::State state;
  std::vector<classifier::Intent> intents;
  for (const Step& step : steps) {
    const classifier::Intent intent =
        classifier::step(state, step.sample, baseline, values, step.nowMs);
    if (intent != classifier::Intent::None) {
      intents.push_back(intent);
    }
  }
  return intents;
}

void assertIntents(const std::vector<classifier::Intent>& got,
                   std::initializer_list<classifier::Intent> expected) {
  TEST_ASSERT_EQUAL_UINT32(expected.size(), got.size());
  size_t index = 0;
  for (classifier::Intent want : expected) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(want), static_cast<int>(got[index]));
    ++index;
  }
}

}  // namespace

void test_a_clean_jump_produces_jump(void) {
  const std::vector<Step> steps = {
      {0, rest()}, {10, rest()}, {20, sampleAt(0.0f, 1.9f)}, {30, sampleAt(0.0f, 1.2f)}, {40, rest()},
  };
  assertIntents(run(steps), {classifier::Intent::Jump});
}

void test_a_jump_with_an_arm_swing_still_produces_jump(void) {
  const std::vector<Step> steps = {
      {0, sampleAt(10.0f, 1.0f)},  {10, sampleAt(-8.0f, 1.1f)}, {20, sampleAt(2.0f, 1.8f)},
      {30, sampleAt(6.0f, 1.0f)},  {40, rest()},
  };
  assertIntents(run(steps), {classifier::Intent::Jump});
}

void test_a_held_crouch_commits_and_standing_up_releases(void) {
  std::vector<Step> steps = atInterval(0, 200, 10, 50.0f, cosf(50.0f * kRad));
  for (const Step& step : atInterval(210, 260, 10, 0.0f, 1.0f)) {
    steps.push_back(step);
  }
  assertIntents(run(steps), {classifier::Intent::Crawl, classifier::Intent::Up});
}

void test_a_crouch_shorter_than_the_hold_does_not_commit(void) {
  std::vector<Step> steps = atInterval(0, 100, 10, 50.0f, cosf(50.0f * kRad));
  for (const Step& step : atInterval(110, 200, 10, 0.0f, 1.0f)) {
    steps.push_back(step);
  }
  assertIntents(run(steps), {});
}

void test_walking_produces_no_intents(void) {
  std::vector<Step> steps;
  for (uint32_t t = 0; t <= 400; t += 10) {
    const bool up = (t / 10) % 2 == 0;
    // Walking peaks just under both thresholds: 1.55 g and 40 deg.
    steps.push_back({t, sampleAt(up ? 40.0f : -40.0f, up ? 1.55f : 0.9f)});
  }
  assertIntents(run(steps), {});
}

void test_swaying_produces_no_intents(void) {
  std::vector<Step> steps;
  for (uint32_t t = 0; t <= 400; t += 10) {
    steps.push_back({t, sampleAt(40.0f * sinf(static_cast<float>(t) * kRad), 1.0f)});
  }
  assertIntents(run(steps), {});
}

void test_adjusting_the_strap_produces_no_intents(void) {
  // The strap is worked up to 44 deg — just short of crawl_deg — and held.
  std::vector<Step> steps;
  for (uint32_t t = 0; t <= 100; t += 10) {
    steps.push_back({t, sampleAt(static_cast<float>(t) * 0.44f, 1.0f)});
  }
  for (const Step& step : atInterval(110, 300, 10, 44.0f, 1.0f)) {
    steps.push_back(step);
  }
  assertIntents(run(steps), {});
}

void test_a_second_jump_inside_the_refractory_window_fires_once(void) {
  const std::vector<Step> steps = {
      {0, sampleAt(0.0f, 1.9f)}, {50, rest()}, {100, sampleAt(0.0f, 1.9f)}, {200, rest()},
  };
  assertIntents(run(steps), {classifier::Intent::Jump});
}

void test_a_jump_after_the_refractory_window_fires_again(void) {
  const std::vector<Step> steps = {
      {0, sampleAt(0.0f, 1.9f)},   {50, rest()}, {100, sampleAt(0.0f, 1.9f)},
      {200, rest()},         {300, sampleAt(0.0f, 1.9f)}, {400, rest()},
  };
  assertIntents(run(steps), {classifier::Intent::Jump, classifier::Intent::Jump});
}

void test_a_sustained_spike_fires_only_once(void) {
  // A hard landing or a long spike stays above jump_g; it is still one Jump.
  std::vector<Step> steps = atInterval(0, 500, 10, 0.0f, 1.9f);
  for (const Step& step : atInterval(510, 600, 10, 0.0f, 1.0f)) {
    steps.push_back(step);
  }
  assertIntents(run(steps), {classifier::Intent::Jump});
}

void test_sub_threshold_motion_produces_no_intents(void) {
  // 1.5 g is below jump_g and 44 deg below crawl_deg.
  std::vector<Step> steps = atInterval(0, 300, 10, 44.0f, 1.5f);
  assertIntents(run(steps), {});
}

void test_crawl_is_measured_relative_to_the_baseline(void) {
  const classifier::Baseline tilted{-20.0f};
  // 20 deg absolute is only 40 deg forward of a -20 deg neutral: no commit.
  std::vector<Step> below = atInterval(0, 300, 10, 20.0f, cosf(20.0f * kRad));
  assertIntents(run(below, tilted), {});

  // 30 deg absolute is 50 deg forward: commits.
  std::vector<Step> above = atInterval(0, 300, 10, 30.0f, cosf(30.0f * kRad));
  assertIntents(run(above, tilted), {classifier::Intent::Crawl});
}

void test_jump_takes_priority_over_crawl_on_the_same_sample(void) {
  const std::vector<Step> steps = {
      {0, sampleAt(50.0f, 1.9f)},
  };
  assertIntents(run(steps), {classifier::Intent::Jump});
}

void test_intent_names_match_the_wire(void) {
  TEST_ASSERT_EQUAL_STRING("JUMP", classifier::intentName(classifier::Intent::Jump));
  TEST_ASSERT_EQUAL_STRING("CRAWL", classifier::intentName(classifier::Intent::Crawl));
  TEST_ASSERT_EQUAL_STRING("UP", classifier::intentName(classifier::Intent::Up));
  TEST_ASSERT_EQUAL_STRING("", classifier::intentName(classifier::Intent::None));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_clean_jump_produces_jump);
  RUN_TEST(test_a_jump_with_an_arm_swing_still_produces_jump);
  RUN_TEST(test_a_held_crouch_commits_and_standing_up_releases);
  RUN_TEST(test_a_crouch_shorter_than_the_hold_does_not_commit);
  RUN_TEST(test_walking_produces_no_intents);
  RUN_TEST(test_swaying_produces_no_intents);
  RUN_TEST(test_adjusting_the_strap_produces_no_intents);
  RUN_TEST(test_a_second_jump_inside_the_refractory_window_fires_once);
  RUN_TEST(test_a_jump_after_the_refractory_window_fires_again);
  RUN_TEST(test_a_sustained_spike_fires_only_once);
  RUN_TEST(test_sub_threshold_motion_produces_no_intents);
  RUN_TEST(test_crawl_is_measured_relative_to_the_baseline);
  RUN_TEST(test_jump_takes_priority_over_crawl_on_the_same_sample);
  RUN_TEST(test_intent_names_match_the_wire);
  return UNITY_END();
}
