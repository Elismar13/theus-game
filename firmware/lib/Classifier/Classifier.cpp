#include "Classifier.h"

namespace classifier {
namespace {

Intent classifyJump(State& state, const signals::Derived& derived,
                    const thresholds::Values& thresholds, uint32_t nowMs) {
  if (derived.vert < thresholds.jump_g) {
    // Falling back below the threshold re-arms the next Jump.
    state.overJumpThreshold = false;
    return Intent::None;
  }
  if (state.overJumpThreshold) {
    // Still the same excursion: a sustained spike is one Jump, not many.
    return Intent::None;
  }
  state.overJumpThreshold = true;
  if (state.hasJumped &&
      nowMs - state.lastJumpMs < static_cast<uint32_t>(thresholds.jump_refractory_ms)) {
    return Intent::None;
  }
  state.hasJumped = true;
  state.lastJumpMs = nowMs;
  return Intent::Jump;
}

Intent classifyCrawl(State& state, const signals::Derived& derived, const Baseline& baseline,
                     const thresholds::Values& thresholds, uint32_t nowMs) {
  const float relative = derived.pitch - baseline.pitch;
  const float release = thresholds.crawl_deg - kCrawlReleaseDeg;

  switch (state.crouch) {
    case State::Crouch::Idle:
      if (relative >= thresholds.crawl_deg) {
        state.crouch = State::Crouch::Pending;
        state.crouchStartedMs = nowMs;
      }
      return Intent::None;

    case State::Crouch::Pending:
      // The hold must be continuous: dropping back below the engage threshold
      // before it elapses discards the attempt.
      if (relative < thresholds.crawl_deg) {
        state.crouch = State::Crouch::Idle;
        return Intent::None;
      }
      if (nowMs - state.crouchStartedMs >= static_cast<uint32_t>(thresholds.crawl_hold_ms)) {
        state.crouch = State::Crouch::Held;
        return Intent::Crawl;
      }
      return Intent::None;

    case State::Crouch::Held:
      if (relative < release) {
        state.crouch = State::Crouch::Idle;
        return Intent::Up;
      }
      return Intent::None;
  }
  return Intent::None;
}

}  // namespace

Intent step(State& state, const signals::Sample& sample, const Baseline& baseline,
            const thresholds::Values& thresholds, uint32_t nowMs) {
  const signals::Derived derived = signals::derive(sample);
  // A Jump is the sharper event: if both would fire, report the Jump and let the
  // Crawl state carry on from the next sample.
  const Intent jump = classifyJump(state, derived, thresholds, nowMs);
  if (jump != Intent::None) {
    return jump;
  }
  return classifyCrawl(state, derived, baseline, thresholds, nowMs);
}

const char* intentName(Intent intent) {
  switch (intent) {
    case Intent::Jump:
      return "JUMP";
    case Intent::Crawl:
      return "CRAWL";
    case Intent::Up:
      return "UP";
    case Intent::None:
      break;
  }
  return "";
}

void Recalibration::start(uint32_t nowMs) {
  phase_ = Phase::Capturing;
  startMs_ = nowMs;
  samples_ = 0;
  pitchSum_ = 0.0f;
  pitchMin_ = 0.0f;
  pitchMax_ = 0.0f;
}

bool Recalibration::addSample(const signals::Sample& sample, uint32_t nowMs) {
  if (!active()) {
    return false;
  }
  const float pitch = signals::derive(sample).pitch;
  if (samples_ == 0) {
    pitchMin_ = pitch;
    pitchMax_ = pitch;
  } else if (pitch < pitchMin_) {
    pitchMin_ = pitch;
  } else if (pitch > pitchMax_) {
    pitchMax_ = pitch;
  }
  pitchSum_ += pitch;
  ++samples_;

  if (nowMs - startMs_ >= config_.windowMs) {
    return finish();
  }
  return false;
}

bool Recalibration::tick(uint32_t nowMs) {
  if (!active() || nowMs - startMs_ < config_.windowMs) {
    return false;
  }
  return finish();
}

bool Recalibration::finish() {
  // Trustworthy only if the player held still: enough samples, and the pitch
  // never wandered further than the tolerated spread.
  const bool trusted = samples_ >= config_.minSamples &&
                       (pitchMax_ - pitchMin_) <= config_.maxSpreadDeg;
  if (trusted) {
    baseline_.pitch = pitchSum_ / static_cast<float>(samples_);
    phase_ = Phase::Done;
    failure_ = Failure::None;
  } else {
    phase_ = Phase::Failed;
    failure_ = samples_ < config_.minSamples ? Failure::TooFewSamples : Failure::TooNoisy;
  }
  return true;
}

}  // namespace classifier
