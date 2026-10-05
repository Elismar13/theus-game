#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#include "Link.h"
#include "Readout.h"
#include "Settings.h"

class Buzzer;
class StatusBoard;

// Binds the Link to the hardware: a WiFi SoftAP that also serves the game page
// from LittleFS over HTTP (port 80), plus the WebSocket server (port 81). Every
// protocol decision lives in `Link`; this class is only transport, persistence
// and the Status Board mirror, which keeps those decisions host-testable.
class LinkServer {
 public:
  static constexpr uint16_t kHttpPort = 80;
  static constexpr uint16_t kSocketPort = 81;

  void begin(StatusBoard& board, Buzzer& buzzer);
  void loop();

  // One Sensor Module reading, forwarded to the Link's raw stream.
  void onSample(const signals::Sample& sample, uint32_t nowMs);
  // Bench convenience: raw mode is normally driven by the page's `mode`.
  void toggleRawMode();

  // Starts a Recalibration from the Device side (boot or button long-press).
  void startRecalibration(uint32_t nowMs);

  // Asks the page to begin a new Run (button click, #14).
  void restartRun();

  bool linkUp() const { return link_.up(); }

  // Repaints the whole Status Board from the current mirror. Used after the
  // panel self-test, which leaves its own proof on the screen.
  void refreshBoard();

 private:
  static bool sendFrame(void* context, uint8_t clientId, const char* frame);
  static void closeClient(void* context, uint8_t clientId);
  static void saveThresholds(void* context, const thresholds::Values& values);
  static void onState(void* context, const protocol::Object& state);
  static void onRecalibration(void* context, classifier::Recalibration::Phase phase);
  static void onIntent(void* context, classifier::Intent intent);

  void onSocketEvent(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length);
  bool serveFile(const String& path);

  WebServer http_{kHttpPort};
  WebSocketsServer socket_{kSocketPort};
  Link link_;
  Settings settings_;
  StatusBoard* board_ = nullptr;
  Buzzer* buzzer_ = nullptr;
  readout::State readout_;
  bool shownLinkUp_ = false;
  // True while the panel is showing a recalibration screen, so the next `state`
  // frame restores the whole read-out rather than one band over a blank panel.
  bool recalibrationScreen_ = false;
  char deviceId_[16] = {};
};
