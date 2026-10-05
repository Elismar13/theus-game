#include "Feedback.h"

namespace feedback {

Reaction forState(const readout::State& before, const readout::State& after) {
  if (after.run == readout::RunState::Dead && before.run != readout::RunState::Dead) {
    return Reaction::GameOver;
  }
  if (after.hearts < before.hearts) {
    return Reaction::HeartLoss;
  }
  return Reaction::None;
}

}  // namespace feedback
