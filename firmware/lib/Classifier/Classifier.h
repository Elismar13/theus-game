#pragma once

#include <stdint.h>

#include "Signals.h"
#include "Thresholds.h"

// The Edge Classifier (CONTEXT.md): the Device's own reading of the player's
// body, turning a Sensor Module sample into an Intent. Pure and deterministic:
// no I/O, no Arduino, so the same code runs in the host tests and on the Device
// (ADR-0002).
namespace classifier {

// What the player expressed with this sample, or None. `Up` is the release of a
// Crawl (CONTEXT.md: Intent).
enum class Intent {
  None,
  Jump,
  Crawl,
  Up,
};

// The neutral posture a Recalibration captured (#12): the torso pitch the
// Sensor Module reads while the player stands still. Crawl is measured relative
// to it. It is never persisted.
struct Baseline {
  float pitch = 0.0f;  // deg, positive forward
};

// The Classifier's hysteresis, hold and refractory timers. Kept explicit rather
// than hidden in the module so the reducer stays a function of its inputs.
struct State {
  enum class Crouch {
    Idle,     // no crouch in progress
    Pending,  // tilted, waiting out the hold time
    Held,     // committed; an Up is owed
  };

  Crouch crouch = Crouch::Idle;
  uint32_t crouchStartedMs = 0;
  uint32_t lastJumpMs = 0;
  bool hasJumped = false;
  // True while vert is at or above `jump_g`, so one excursion above the
  // threshold yields at most one Jump however long it lasts.
  bool overJumpThreshold = false;
};

// How far below `crawl_deg` the pitch must fall to release a committed Crawl.
// Hysteresis keeps a held crouch from chattering around the engage threshold.
constexpr float kCrawlReleaseDeg = 10.0f;

// One sample in, one Intent out. Advances `state`; `nowMs` is the Device's
// monotonic clock. A Jump takes priority over a Crawl that would fire on the
// same sample.
Intent step(State& state, const signals::Sample& sample, const Baseline& baseline,
            const thresholds::Values& thresholds, uint32_t nowMs);

// The wire name for an Intent, e.g. "JUMP" (`docs/protocol.md`).
const char* intentName(Intent intent);

// Captures the neutral Baseline across a window of stillness, rejecting a
// capture the player moved during as `Failed`. Pure: samples and a monotonic
// clock in, a phase and a Baseline out, so the noisy-capture case runs on the
// host like the rest of the Edge Classifier.
class Calibration {
 public:
  enum class Phase : uint8_t {
    Idle,       // no capture in progress
    Capturing,  // collecting samples
    Done,       // a Baseline was captured
    Failed,     // too noisy or too few samples to trust
  };

  struct Config {
    // Stillness required before the capture is accepted.
    uint32_t windowMs = 1500;
    // Peak-to-peak pitch tolerated across the window, in degrees.
    float maxSpreadDeg = 8.0f;
    // Below this many samples the capture is not trustworthy.
    uint16_t minSamples = 10;
  };

  // Starts a capture, discarding any in progress.
  void start(uint32_t nowMs);

  // Feeds one sample; returns true when the capture just finished. A sample
  // outside a capture is ignored.
  bool addSample(const signals::Sample& sample, uint32_t nowMs);

  // Finishes a capture once the window has elapsed, even if samples have
  // stopped arriving, so a dead sensor cannot leave it stuck Capturing.
  bool tick(uint32_t nowMs);

  Phase phase() const { return phase_; }
  bool active() const { return phase_ == Phase::Capturing; }
  // Valid when phase() is Done. The previous value is kept on Failed.
  const Baseline& baseline() const { return baseline_; }

 private:
  bool finish();

  Config config_{};
  Phase phase_ = Phase::Idle;
  Baseline baseline_{};
  uint32_t startMs_ = 0;
  uint16_t samples_ = 0;
  float pitchSum_ = 0.0f;
  float pitchMin_ = 0.0f;
  float pitchMax_ = 0.0f;
};

}  // namespace classifier
