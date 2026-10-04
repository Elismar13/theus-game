#include "StatusBoard.h"

namespace {

// Backlight (docs/hardware.md): GPIO25, LEDC PWM for dimming. TFT_eSPI is
// configured without a TFT_BL, so it leaves this pin to us.
//
// Channel 0 is reserved for the buzzer: Arduino-ESP32's tone() defaults to it,
// and a shared channel would make the backlight follow the buzzer.
constexpr uint8_t kBacklightPin = 25;
constexpr uint8_t kBacklightChannel = 1;
constexpr uint32_t kBacklightFrequency = 5000;
constexpr uint8_t kBacklightResolutionBits = 8;  // duty 0..255
constexpr uint32_t kBacklightDutyMax = (1u << kBacklightResolutionBits) - 1;

// Fixed regions in portrait orientation. Each value owns one band, so a change
// repaints that band alone. They tile the 80x160 window without overlapping.
struct Region {
  int16_t y;
  int16_t height;
};

constexpr Region kLinkRegion = {0, 24};
constexpr Region kScoreRegion = {26, 30};
constexpr Region kHighScoreRegion = {58, 30};
constexpr Region kHeartsRegion = {90, 30};
constexpr Region kRunRegion = {122, 36};

constexpr int16_t kLabelX = StatusBoard::kWidth / 2;

// The slot row is driven by max_hearts; a Run starts with all of them full.
// The panel is 80 px wide, so a hand's worth of slots is all that fits.
constexpr int kMaxHeartSlots = 5;
constexpr int kHeartSlotWidth = 12;
constexpr int kHeartSlotGap = 4;

// A full Heart slot. The panel has no glyph for it, so it is two lobes over a
// point. `full` fills it red; otherwise the outline is left as an empty slot.
void drawHeart(TFT_eSPI& panel, int16_t x, int16_t y, bool full) {
  const int16_t lobe = 3;
  const int16_t left = x + lobe;
  const int16_t right = x + kHeartSlotWidth - lobe;
  const int16_t top = y + 4;
  if (full) {
    panel.fillCircle(left, top, lobe, TFT_RED);
    panel.fillCircle(right, top, lobe, TFT_RED);
    panel.fillTriangle(x, top + 1, x + kHeartSlotWidth, top + 1, x + kHeartSlotWidth / 2,
                       y + 12, TFT_RED);
  } else {
    panel.drawCircle(left, top, lobe, TFT_DARKGREY);
    panel.drawCircle(right, top, lobe, TFT_DARKGREY);
    panel.drawTriangle(x, top + 1, x + kHeartSlotWidth, top + 1, x + kHeartSlotWidth / 2,
                       y + 12, TFT_DARKGREY);
  }
}

// A label above a value, both centred. Clearing the band first keeps a shorter
// value from leaving the tail of the previous one behind.
void drawValue(TFT_eSPI& panel, const Region& region, const char* label,
               const char* value, uint16_t valueColor) {
  panel.fillRect(0, region.y, StatusBoard::kWidth, region.height, TFT_BLACK);
  panel.setTextDatum(TC_DATUM);
  panel.setTextColor(TFT_DARKGREY, TFT_BLACK);
  panel.setTextSize(1);
  panel.drawString(label, kLabelX, region.y + 2);
  panel.setTextColor(valueColor, TFT_BLACK);
  panel.setTextSize(2);
  panel.drawString(value, kLabelX, region.y + 12);
}

// Two centred lines for the full-panel messages (Recalibration). The longest is
// 11 characters, which is the most the 80 px width fits at text size 1.
void drawCentredMessage(TFT_eSPI& panel, const char* top, const char* bottom,
                        uint16_t color) {
  panel.setTextDatum(MC_DATUM);
  panel.setTextColor(color, TFT_BLACK);
  panel.setTextSize(1);
  panel.drawString(top, StatusBoard::kWidth / 2, StatusBoard::kHeight / 2 - 10);
  panel.drawString(bottom, StatusBoard::kWidth / 2, StatusBoard::kHeight / 2 + 6);
}

}  // namespace

void StatusBoard::begin() {
  ledcSetup(kBacklightChannel, kBacklightFrequency, kBacklightResolutionBits);
  ledcAttachPin(kBacklightPin, kBacklightChannel);
  setBrightness(kBrightnessLevelMax);

  panel_.init();
  panel_.setRotation(0);  // portrait, 80x160
  panel_.fillScreen(TFT_BLACK);

  linkDrawn_ = false;
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
  // window is offset; if red and blue are swapped, the panel is RGB, not BGR.
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

void StatusBoard::showLink(bool up) {
  if (linkDrawn_ && linkUp_ == up) {
    return;
  }
  drawLinkRegion(up);
  linkDrawn_ = true;
  linkUp_ = up;
}

void StatusBoard::drawLinkRegion(bool up) {
  panel_.fillRect(0, kLinkRegion.y, kWidth, kLinkRegion.height, TFT_BLACK);
  panel_.setTextDatum(TC_DATUM);
  panel_.setTextColor(TFT_DARKGREY, TFT_BLACK);
  panel_.setTextSize(1);
  panel_.drawString("LINK", kLabelX, kLinkRegion.y + 1);
  panel_.setTextColor(up ? TFT_GREEN : TFT_RED, TFT_BLACK);
  panel_.setTextSize(2);
  panel_.drawString(up ? "OK" : "LOST", kLabelX, kLinkRegion.y + 8);
}

void StatusBoard::render(const readout::State& state, uint32_t changed) {
  if ((changed & readout::kScore) != 0) {
    drawScore(state.score);
  }
  if ((changed & readout::kHighScore) != 0) {
    drawHighScore(state.highScore);
  }
  if ((changed & readout::kHearts) != 0) {
    drawHearts(state.hearts, state.maxHearts);
  }
  if ((changed & readout::kRun) != 0) {
    drawRun(state.run);
  }
}

void StatusBoard::repaint(const readout::State& state, bool linkUp) {
  panel_.fillScreen(TFT_BLACK);
  linkDrawn_ = false;
  render(state, readout::kAll);
  showLink(linkUp);
}

void StatusBoard::showCalibration(classifier::Calibration::Phase phase) {
  if (phase != classifier::Calibration::Phase::Capturing &&
      phase != classifier::Calibration::Phase::Failed) {
    return;
  }
  panel_.fillScreen(TFT_BLACK);
  // The message owns the whole window; force the next Link paint to redraw.
  linkDrawn_ = false;
  if (phase == classifier::Calibration::Phase::Capturing) {
    drawCentredMessage(panel_, "CALIBRATING", "STAND STILL", TFT_CYAN);
  } else {
    drawCentredMessage(panel_, "CALIBRATION", "FAILED", TFT_RED);
  }
}

void StatusBoard::drawScore(int score) {
  char text[12];
  snprintf(text, sizeof(text), "%d", score);
  drawValue(panel_, kScoreRegion, "SCORE", text, TFT_WHITE);
}

void StatusBoard::drawHighScore(int highScore) {
  char text[12];
  snprintf(text, sizeof(text), "%d", highScore);
  drawValue(panel_, kHighScoreRegion, "HIGH SCORE", text, TFT_WHITE);
}

void StatusBoard::drawHearts(int hearts, int maxHearts) {
  const Region& region = kHeartsRegion;
  panel_.fillRect(0, region.y, kWidth, region.height, TFT_BLACK);
  panel_.setTextDatum(TC_DATUM);
  panel_.setTextColor(TFT_DARKGREY, TFT_BLACK);
  panel_.setTextSize(1);
  panel_.drawString("HEARTS", kLabelX, region.y + 2);

  int slots = maxHearts;
  if (slots > kMaxHeartSlots) {
    slots = kMaxHeartSlots;
  }
  if (slots < 0) {
    slots = 0;
  }
  const int total =
      slots * kHeartSlotWidth + (slots > 0 ? (slots - 1) * kHeartSlotGap : 0);
  const int16_t startX = static_cast<int16_t>((kWidth - total) / 2);
  for (int i = 0; i < slots; ++i) {
    const int16_t x = startX + i * (kHeartSlotWidth + kHeartSlotGap);
    drawHeart(panel_, x, region.y + 12, i < hearts);
  }
}

void StatusBoard::drawRun(readout::RunState run) {
  const uint16_t color = run == readout::RunState::Dead ? TFT_RED : TFT_WHITE;
  drawValue(panel_, kRunRegion, "RUN STATE", readout::runStateName(run), color);
}
