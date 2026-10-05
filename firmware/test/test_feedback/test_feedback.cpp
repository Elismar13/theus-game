#include <unity.h>

#include "Feedback.h"

namespace {

readout::State state(int hearts, readout::RunState run) {
  readout::State value;
  value.hearts = hearts;
  value.run = run;
  return value;
}

}  // namespace

void test_no_change_is_silent(void) {
  const readout::State before = state(3, readout::RunState::Run);
  const readout::State after = state(3, readout::RunState::Run);
  TEST_ASSERT_TRUE(feedback::forState(before, after) == feedback::Reaction::None);
}

void test_a_lost_heart_plays_the_heart_loss_cue(void) {
  const readout::State before = state(3, readout::RunState::Run);
  const readout::State after = state(2, readout::RunState::Invuln);
  TEST_ASSERT_TRUE(feedback::forState(before, after) == feedback::Reaction::HeartLoss);
}

void test_an_extra_heart_is_not_a_heart_loss(void) {
  // A fresh Run restores the Hearts; that is not a hit.
  const readout::State before = state(0, readout::RunState::Dead);
  const readout::State after = state(3, readout::RunState::Ready);
  TEST_ASSERT_TRUE(feedback::forState(before, after) == feedback::Reaction::None);
}

void test_reaching_dead_plays_the_game_over_cue(void) {
  const readout::State before = state(1, readout::RunState::Run);
  const readout::State after = state(0, readout::RunState::Dead);
  // The last blow ends the Run: game over supersedes the Heart it cost.
  TEST_ASSERT_TRUE(feedback::forState(before, after) == feedback::Reaction::GameOver);
}

void test_a_heart_lost_on_the_way_to_dead_is_still_heart_loss(void) {
  const readout::State before = state(2, readout::RunState::Run);
  const readout::State after = state(1, readout::RunState::Invuln);
  TEST_ASSERT_TRUE(feedback::forState(before, after) == feedback::Reaction::HeartLoss);
}

void test_staying_dead_does_not_replay_the_cue(void) {
  const readout::State before = state(0, readout::RunState::Dead);
  const readout::State after = state(0, readout::RunState::Dead);
  TEST_ASSERT_TRUE(feedback::forState(before, after) == feedback::Reaction::None);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_no_change_is_silent);
  RUN_TEST(test_a_lost_heart_plays_the_heart_loss_cue);
  RUN_TEST(test_an_extra_heart_is_not_a_heart_loss);
  RUN_TEST(test_reaching_dead_plays_the_game_over_cue);
  RUN_TEST(test_a_heart_lost_on_the_way_to_dead_is_still_heart_loss);
  RUN_TEST(test_staying_dead_does_not_replay_the_cue);
  return UNITY_END();
}
