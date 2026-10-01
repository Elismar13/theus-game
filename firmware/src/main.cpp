#include <Arduino.h>

#include "Button.h"
#include "Buzzer.h"
#include "StatusBoard.h"

namespace {

// docs/hardware.md: button on GPIO32 to GND, internal pull-up.
constexpr uint8_t kButtonPin = 32;

StatusBoard statusBoard;
Button button;
Buzzer buzzer;

// The button's job in the finished Device is Recalibration (long press) and
// mute (double-click); its other bindings arrive with the integration ticket.
// Here it only proves the events and toggles mute.
void handleEvent(Button::Event event) {
  switch (event) {
    case Button::Event::Click:
      Serial.println("click");
      break;
    case Button::Event::DoubleClick:
      buzzer.toggleMute();
      Serial.printf("double-click -> mute %s\n", buzzer.muted() ? "on" : "off");
      break;
    case Button::Event::LongPress:
      Serial.println("long-press");
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
      break;
    case 'b':
      buzzer.play(sounds::Sound::LowBattery);
      break;
    case 'm':
      buzzer.toggleMute();
      Serial.printf("mute %s\n", buzzer.muted() ? "on" : "off");
      break;
    default:
      break;
  }
}

void printHelp() {
  Serial.println(
      "commands: j jump  h heart loss  g game over  r recalibrate  b low "
      "battery  m mute");
  Serial.printf("mute is %s\n", buzzer.muted() ? "on" : "off");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("theus-game: button and buzzer bring-up");

  statusBoard.begin();
  statusBoard.selfTest();

  buzzer.begin();
  pinMode(kButtonPin, INPUT_PULLUP);

  printHelp();
}

void loop() {
  const bool pressed = digitalRead(kButtonPin) == LOW;
  handleEvent(button.update(pressed, millis()));

  handleSerialCommand();
  buzzer.update(millis());

  delay(5);
}
