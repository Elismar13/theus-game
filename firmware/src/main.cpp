#include <Arduino.h>

#include "StatusBoard.h"

StatusBoard statusBoard;

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("theus-game: status board bring-up");
  Serial.printf("panel %dx%d, backlight levels 0..%d\n", StatusBoard::kWidth,
                StatusBoard::kHeight, StatusBoard::kBrightnessLevelMax);

  statusBoard.begin();
  statusBoard.selfTest();
}

void loop() {
  // The bring-up image stays on screen. The panel is a Status Board, so the
  // pattern *is* the proof and there is nothing to redraw.
  delay(1000);
}
