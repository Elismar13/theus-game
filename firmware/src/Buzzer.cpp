#include "Buzzer.h"

namespace {

// docs/hardware.md: buzzer on GPIO26, driven by LEDC. Channel 0 is the buzz
// channel — Arduino-ESP32's tone() defaults to it — and StatusBoard's backlight
// deliberately sits on channel 1 so the two never fight.
constexpr uint8_t kBuzzerPin = 26;
constexpr uint8_t kBuzzerChannel = 0;
constexpr uint8_t kBuzzerResolutionBits = 10;
constexpr uint32_t kBuzzerIdleFrequency = 2000;

constexpr const char* kPreferencesNamespace = "theus";
constexpr const char* kMuteKey = "mute";

}  // namespace

void Buzzer::begin() {
  preferences_.begin(kPreferencesNamespace, false);
  muted_ = preferences_.getBool(kMuteKey, false);

  ledcSetup(kBuzzerChannel, kBuzzerIdleFrequency, kBuzzerResolutionBits);
  ledcAttachPin(kBuzzerPin, kBuzzerChannel);
  stop();
}

void Buzzer::play(sounds::Sound sound) {
  if (muted_) {
    return;
  }
  current_ = sounds::pattern(sound);
  index_ = 0;
  running_ = current_.count > 0;
  stepEndsMs_ = 0;  // start the first tone on the next update()
}

void Buzzer::update(uint32_t nowMs) {
  if (!running_ || nowMs < stepEndsMs_) {
    return;
  }
  if (index_ >= current_.count) {
    stop();
    return;
  }

  const sounds::Tone& tone = current_.tones[index_];
  ++index_;
  ledcWriteTone(kBuzzerChannel, tone.frequencyHz);
  stepEndsMs_ = nowMs + tone.durationMs;
}

void Buzzer::setMuted(bool muted) {
  muted_ = muted;
  preferences_.putBool(kMuteKey, muted_);
  if (muted_) {
    stop();
  }
}

void Buzzer::toggleMute() { setMuted(!muted_); }

void Buzzer::stop() {
  running_ = false;
  index_ = 0;
  ledcWriteTone(kBuzzerChannel, 0);  // 0 Hz: silence
}
