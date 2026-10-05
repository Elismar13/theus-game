#pragma once

#include <stdint.h>

#include "Readout.h"

// The Device's audible reactions to a Run (ADR-0005: the buzzer is load-bearing).
// The decision is kept separate from the buzzer so it runs on the host, while
// playing the pattern is the Device's job.
namespace feedback {

enum class Reaction : uint8_t {
  None,
  HeartLoss,
  GameOver,
};

// Which cue one `state` message warrants, given the mirror before and after it.
// The end of a Run supersedes the Heart it ended on: the last blow gets the
// game-over sound alone, not both cues at once.
Reaction forState(const readout::State& before, const readout::State& after);

}  // namespace feedback
