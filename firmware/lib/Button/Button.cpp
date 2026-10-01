#include "Button.h"

Button::Event Button::update(bool pressed, uint32_t nowMs) {
  if (!initialised_) {
    // Assume released until the debounce says otherwise, so a level already
    // held on the first sample still has to pass through the debounce.
    initialised_ = true;
    rawPressed_ = false;
    stablePressed_ = false;
    lastRawChangeMs_ = nowMs;
  }
  if (pressed != rawPressed_) {
    rawPressed_ = pressed;
    lastRawChangeMs_ = nowMs;
  }

  // Trust the raw level only once it has held steady for the debounce window.
  bool pressEdge = false;
  bool releaseEdge = false;
  if (rawPressed_ != stablePressed_ &&
      nowMs - lastRawChangeMs_ >= config_.debounceMs) {
    stablePressed_ = rawPressed_;
    if (stablePressed_) {
      pressEdge = true;
    } else {
      releaseEdge = true;
    }
  }

  switch (state_) {
    case State::Idle:
      if (pressEdge) {
        state_ = State::Pressed;
        pressStartMs_ = nowMs;
        longPressFired_ = false;
        secondPress_ = false;
        firstClickPending_ = false;
      }
      break;

    case State::Pressed:
      // Report a long press as soon as it crosses the threshold, not on
      // release, so a hold that triggers Recalibration feels immediate.
      if (!longPressFired_ &&
          nowMs - pressStartMs_ >= config_.longPressMs) {
        // A long press that follows a still-pending click is really two
        // events: report the click first, then the long press next update.
        if (secondPress_ && firstClickPending_) {
          firstClickPending_ = false;
          return Event::Click;
        }
        longPressFired_ = true;
        secondPress_ = false;
        return Event::LongPress;
      }
      if (releaseEdge) {
        if (longPressFired_) {
          state_ = State::Idle;
        } else if (secondPress_) {
          secondPress_ = false;
          firstClickPending_ = false;
          state_ = State::Idle;
          return Event::DoubleClick;
        } else {
          state_ = State::WaitForDouble;
          releaseMs_ = nowMs;
        }
      }
      break;

    case State::WaitForDouble:
      if (pressEdge) {
        state_ = State::Pressed;
        pressStartMs_ = nowMs;
        longPressFired_ = false;
        secondPress_ = true;
        firstClickPending_ = true;
      } else if (nowMs - releaseMs_ > config_.doubleClickMs) {
        state_ = State::Idle;
        return Event::Click;
      }
      break;
  }

  return Event::None;
}
