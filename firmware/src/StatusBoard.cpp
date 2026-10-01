#include "StatusBoard.h"

namespace {

// Backlight (docs/hardware.md): GPIO25, LEDC PWM for dimming. TFT_eSPI is
// configured without a TFT_BL, so it leaves this pin to us.
constexpr uint8_t kBacklightPin = 25;
constexpr uint8_t kBacklightChannel = 0;
constexpr uint32_t kBacklightFrequency = 5000;
constexpr uint8_t kBacklightResolutionBits = 8;  // duty 0..255
constexpr uint32_t kBacklightDutyMax = (1u << kBacklightResolutionBits) - 1;

}  // namespace

void StatusBoard::begin() {
  ledcSetup(kBacklightChannel, kBacklightFrequency, kBacklightResolutionBits);
  ledcAttachPin(kBacklightPin, kBacklightChannel);
  setBrightness(kBrightnessLevelMax);

  panel_.init();
  panel_.setRotation(0);  // portrait, 80x160
  panel_.fillScreen(TFT_BLACK);
}

void StatusBoard::setBrightness(uint8_t level) {
  if (level > kBrightnessLevelMax) {
    level = kBrightnessLevelMax;
  }
  const uint32_t duty = kBacklightDutyMax * level / kBrightnessLevelMax;
  ledcWrite(kBacklightChannel, duty);
}

void StatusBoard::selfTest() {
  panel_.fillScreen(TFT_BLACK);

  // The border must land on the physical edges. A blank row or column at any
  // edge means the window offset does not match the panel.
  panel_.drawRect(0, 0, kWidth, kHeight, TFT_WHITE);

  // Corner markers sit flush against the border: if a corner is detached, the
  // window is offset; if red and green are swapped, the panel is RGB, not BGR.
  constexpr int16_t kMarker = 6;
  panel_.fillRect(1, 1, kMarker, kMarker, TFT_RED);
  panel_.fillRect(kWidth - 1 - kMarker, 1, kMarker, kMarker, TFT_GREEN);
  panel_.fillRect(1, kHeight - 1 - kMarker, kMarker, kMarker, TFT_BLUE);
  panel_.fillRect(kWidth - 1 - kMarker, kHeight - 1 - kMarker, kMarker, kMarker, TFT_YELLOW);

  // Text at a known position, centred on the panel.
  panel_.setTextDatum(TC_DATUM);
  panel_.setTextColor(TFT_WHITE, TFT_BLACK);
  panel_.setTextSize(2);
  panel_.drawString("THEUS", kWidth / 2, 60);
  panel_.setTextSize(1);
  panel_.drawString("STATUS BOARD", kWidth / 2, 84);
  panel_.drawString("80x160", kWidth / 2, 96);

  // Sweep every level so the dimming range is visible by eye, then settle at
  // full brightness.
  for (int level = kBrightnessLevelMax; level >= 0; --level) {
    setBrightness(level);
    delay(200);
  }
  setBrightness(kBrightnessLevelMax);
}
