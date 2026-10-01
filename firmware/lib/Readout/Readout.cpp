#include "Readout.h"

#include <string.h>

#include "Protocol.h"

namespace readout {
namespace {

struct RunStateName {
  const char* name;
  RunState run;
};

// The wire names from `docs/protocol.md`, in the same order as the enum.
constexpr RunStateName kRunStateNames[] = {
    {"READY", RunState::Ready}, {"RUN", RunState::Run},     {"JUMP", RunState::Jump},
    {"CRAWL", RunState::Crawl}, {"INVULN", RunState::Invuln}, {"DEAD", RunState::Dead},
    {"PAUSED", RunState::Paused},
};

}  // namespace

uint32_t apply(State& state, const protocol::Object& object) {
  uint32_t changed = 0;
  long value = 0;

  // Counters are never negative; a negative value is malformed, not a value to
  // show, so it leaves the previous display intact like a missing field.
  if (protocol::getInt(object, "score", value) && value >= 0 && value != state.score) {
    state.score = static_cast<int>(value);
    changed |= kScore;
  }
  if (protocol::getInt(object, "hi", value) && value >= 0 && value != state.highScore) {
    state.highScore = static_cast<int>(value);
    changed |= kHighScore;
  }

  // hearts and max_hearts are read together: either one moving redraws the one
  // slot row. A malformed one keeps its previous value while the other still
  // updates, so the row never corrupts.
  int hearts = state.hearts;
  int maxHearts = state.maxHearts;
  bool heartsPresent = false;
  if (protocol::getInt(object, "hearts", value) && value >= 0) {
    hearts = static_cast<int>(value);
    heartsPresent = true;
  }
  if (protocol::getInt(object, "max_hearts", value) && value >= 0) {
    maxHearts = static_cast<int>(value);
    heartsPresent = true;
  }
  if (heartsPresent && (hearts != state.hearts || maxHearts != state.maxHearts)) {
    state.hearts = hearts;
    state.maxHearts = maxHearts;
    changed |= kHearts;
  }

  char run[16];
  if (protocol::getString(object, "run", run, sizeof(run))) {
    RunState parsed;
    if (parseRunState(run, parsed) && parsed != state.run) {
      state.run = parsed;
      changed |= kRun;
    }
  }

  return changed;
}

const char* runStateName(RunState run) {
  for (const RunStateName& entry : kRunStateNames) {
    if (entry.run == run) {
      return entry.name;
    }
  }
  return "";
}

bool parseRunState(const char* name, RunState& out) {
  for (const RunStateName& entry : kRunStateNames) {
    if (strcmp(entry.name, name) == 0) {
      out = entry.run;
      return true;
    }
  }
  return false;
}

}  // namespace readout
