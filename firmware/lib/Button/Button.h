#pragma once

#include <stdint.h>

// Recognises click, long-press and double-click from one digital input. It is
// deliberately pure: update() takes the raw pressed level and a monotonic
// millisecond clock, so the same code runs against a pin sample on the Device
// and against scripted samples in a host test.
//
// A single click is only reported once the double-click window has lapsed,
// because a click and the first half of a double-click are the same input
// until then.
class Button {
 public:
  enum class Event : uint8_t {
    None,
    Click,
    LongPress,
    DoubleClick,
  };

  struct Config {
    // How long the raw level must hold steady before it is trusted.
    uint16_t debounceMs = 30;
    // Held at least this long it is a LongPress, reported while still held.
    uint16_t longPressMs = 1500;
    // The longest gap between two short presses that still pairs them.
    uint16_t doubleClickMs = 300;
  };

  // Feed one sample. `pressed` is true while the button is held; `nowMs` is a
  // monotonic millisecond clock. Returns the event completed by this sample, or
  // Event::None. Call it often enough that the clock granularity is well under
  // the debounce window.
  Event update(bool pressed, uint32_t nowMs);

 private:
  enum class State : uint8_t {
    Idle,
    Pressed,
    WaitForDouble,
  };

  Config config_{};
  State state_ = State::Idle;
  bool initialised_ = false;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  bool longPressFired_ = false;
  bool secondPress_ = false;
  bool firstClickPending_ = false;
  uint32_t lastRawChangeMs_ = 0;
  uint32_t pressStartMs_ = 0;
  uint32_t releaseMs_ = 0;
};
