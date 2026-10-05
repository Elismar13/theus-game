#include <Arduino.h>

#include "Bmi160.h"
#include "Button.h"
#include "Buzzer.h"
#include "LinkServer.h"
#include "Signals.h"
#include "StatusBoard.h"

namespace {

// docs/hardware.md: button on GPIO32 to GND, internal pull-up.
constexpr uint8_t kButtonPin = 32;

// The Sensor Module runs at 100 Hz, so sample every 10 ms. The loop services
// the Link between samples, so the read never blocks it.
constexpr uint32_t kSampleIntervalMs = 10;

StatusBoard statusBoard;
Button button;
Buzzer buzzer;
Bmi160 sensor;
LinkServer linkServer;
bool sensorReady = false;
uint32_t lastSampleMs = 0;

// The Device's half of play: a long press Recalibrates, and a click begins a new
// Run on the page (#14). Double-click mutes.
void handleEvent(Button::Event event) {
  switch (event) {
    case Button::Event::Click:
      linkServer.restartRun();
      Serial.println("click -> restart");
      break;
    case Button::Event::DoubleClick:
      buzzer.toggleMute();
      Serial.printf("double-click -> mute %s\n", buzzer.muted() ? "on" : "off");
      break;
    case Button::Event::LongPress:
      // Recalibrate the Baseline without touching a keyboard.
      buzzer.play(sounds::Sound::Recalibration);
      linkServer.startRecalibration(millis());
      Serial.println("long-press -> recalibrate");
      break;
    case Button::Event::None:
      break;
  }
}

// Plays a pattern on demand, so each of the five can be checked by ear.
void handleSerialCommand() {
  if (!Serial.available()) {
    return;
  }
  switch (Serial.read()) {
    case 'j':
      buzzer.play(sounds::Sound::Jump);
      break;
    case 'h':
      buzzer.play(sounds::Sound::HeartLoss);
      break;
    case 'g':
      buzzer.play(sounds::Sound::GameOver);
      break;
    case 'r':
      buzzer.play(sounds::Sound::Recalibration);
      linkServer.startRecalibration(millis());
      break;
    case 'b':
      buzzer.play(sounds::Sound::LowBattery);
      break;
    case 'm':
      buzzer.toggleMute();
      Serial.printf("mute %s\n", buzzer.muted() ? "on" : "off");
      break;
    case 'x':
      linkServer.toggleRawMode();
      break;
    default:
      break;
  }
}

void printHelp() {
  Serial.println(
      "commands: j jump  h heart loss  g game over  r recalibrate  b low "
      "battery  m mute  x raw mode");
  Serial.printf("mute is %s\n", buzzer.muted() ? "on" : "off");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("theus-game: walking skeleton");

  statusBoard.begin();
  buzzer.begin();
  pinMode(kButtonPin, INPUT_PULLUP);
  sensorReady = sensor.begin();
  if (!sensorReady) {
    Serial.println("BMI160 not found on I2C; no raw stream");
  }

  // The access point and the Link come up before the panel self-test, so the
  // page can connect while the bench proof runs.
  linkServer.begin(statusBoard, buzzer);
  statusBoard.selfTest();
  // The self-test leaves its own proof on the panel; restore the read-out.
  linkServer.refreshBoard();

  // With the bench proof cleared, auto-calibrate: the player holds still, the
  // panel says CALIBRATING, and it settles on READY.
  if (sensorReady) {
    linkServer.startRecalibration(millis());
  }

  printHelp();
}

void loop() {
  linkServer.loop();

  const uint32_t now = millis();
  if (sensorReady && now - lastSampleMs >= kSampleIntervalMs) {
    lastSampleMs = now;
    signals::Sample sample;
    sensor.read(sample);
    linkServer.onSample(sample, now);
  }

  const bool pressed = digitalRead(kButtonPin) == LOW;
  handleEvent(button.update(pressed, millis()));

  handleSerialCommand();
  buzzer.update(millis());

  delay(5);
}
