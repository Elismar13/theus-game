#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

#include "Link.h"

class StatusBoard;

// Binds the Link to the hardware: a WiFi SoftAP that also serves the game page
// from LittleFS over HTTP (port 80), plus the WebSocket server (port 81). Every
// protocol decision lives in `Link`; this class is only transport and the
// Status Board mirror, which keeps those decisions host-testable.
class LinkServer {
 public:
  static constexpr uint16_t kHttpPort = 80;
  static constexpr uint16_t kSocketPort = 81;

  void begin(StatusBoard& board);
  void loop();

  bool linkUp() const { return link_.up(); }

 private:
  static bool sendFrame(void* context, uint8_t clientId, const char* frame);
  static void closeClient(void* context, uint8_t clientId);

  void onSocketEvent(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length);
  bool serveFile(const String& path);

  WebServer http_{kHttpPort};
  WebSocketsServer socket_{kSocketPort};
  Link link_;
  StatusBoard* board_ = nullptr;
  bool shownLinkUp_ = false;
  char deviceId_[16] = {};
};
