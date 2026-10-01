#pragma once

#include <stdint.h>

namespace protocol {
struct Object;
}

// The Status Board's read-out of a Run (CONTEXT.md: Run State, Score, High
// Score, Heart). Pure: no Arduino, no TFT, so the change detection that keeps
// the panel from flickering is host-testable.
namespace readout {

// The Run States that travel on the wire (`docs/protocol.md`): the Game Core's
// set plus PAUSED, which the Link introduces before the core has a pause (#15).
enum class RunState {
  Ready,
  Run,
  Jump,
  Crawl,
  Invuln,
  Dead,
  Paused,
};

// Everything the panel shows about a Run. It is the Device's mirror of the
// page's `state` message, not a second source of truth.
struct State {
  int score = 0;
  int highScore = 0;
  int hearts = 0;
  int maxHearts = 0;
  RunState run = RunState::Ready;
};

// A field of the read-out, as a bit in the changed mask returned by `apply`.
enum Field : uint32_t {
  kScore = 1u << 0,
  kHighScore = 1u << 1,
  // hearts and max_hearts share a bit: both drive the one slot row.
  kHearts = 1u << 2,
  kRun = 1u << 3,
  kAll = kScore | kHighScore | kHearts | kRun,
};

// Applies one parsed `state` message. Each field is validated on its own: a
// missing or mistyped field keeps its previous value, while the others still
// update. Returns the fields whose displayed value actually changed, so the
// caller redraws only those regions.
uint32_t apply(State& state, const protocol::Object& object);

// The wire name for a Run State, e.g. "CRAWL".
const char* runStateName(RunState run);

// Parses a wire name; false when it is not one of the Run States.
bool parseRunState(const char* name, RunState& out);

}  // namespace readout
