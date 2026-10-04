#include "LinkServer.h"

#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "StatusBoard.h"
#include "Protocol.h"

namespace {

constexpr const char* kAccessPointName = "THEUS-RUN";
constexpr const char* kHostname = "theus";
constexpr const char* kFirmwareVersion = "0.1.0";

// One frame is one JSON object; the largest in the protocol is a `raw` sample,
// so a few hundred bytes is comfortable.
constexpr size_t kMaxFrameLength = 512;

const char* contentTypeFor(const String& path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".js")) return "text/javascript";
  if (path.endsWith(".css")) return "text/css";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

}  // namespace

void LinkServer::begin(StatusBoard& board) {
  board_ = &board;
  settings_.begin();

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(kAccessPointName);
  Serial.printf("AP %s at %s\n", kAccessPointName, WiFi.softAPIP().toString().c_str());

  if (MDNS.begin(kHostname)) {
    MDNS.addService("http", "tcp", kHttpPort);
  } else {
    Serial.println("mDNS failed to start");
  }

  http_.onNotFound([this]() {
    if (!serveFile(http_.uri())) {
      http_.send(404, "text/plain", "not found");
    }
  });
  http_.begin();

  // A short, stable device id derived from the SoftAP MAC, per the `hello` spec.
  String mac = WiFi.softAPmacAddress();
  mac.replace(":", "");
  const String suffix = mac.length() >= 6 ? mac.substring(mac.length() - 6) : mac;
  snprintf(deviceId_, sizeof(deviceId_), "%s", suffix.c_str());

  Link::Config config;
  config.fw = kFirmwareVersion;
  config.dev = deviceId_;
  config.caps = "\"cfg\",\"raw\"";
  config.thresholds = settings_.thresholds();
  link_.begin(config, &LinkServer::sendFrame, &LinkServer::closeClient, this,
              &LinkServer::saveThresholds);
  // The page owns Run State; the Device mirrors it onto the Status Board (#7).
  link_.setStateHandler(&LinkServer::onState);
  // The board shows the Recalibration screen and READY when it lands (#12).
  link_.setCalibrationHandler(&LinkServer::onCalibration);

  socket_.begin();
  socket_.onEvent([this](uint8_t clientId, WStype_t type, uint8_t* payload, size_t length) {
    onSocketEvent(clientId, type, payload, length);
  });

  refreshBoard();
}

void LinkServer::refreshBoard() {
  if (board_ == nullptr) {
    return;
  }
  board_->repaint(readout_, link_.up());
}

void LinkServer::loop() {
  http_.handleClient();
  socket_.loop();
  link_.tick(millis());

  if (board_ != nullptr && link_.up() != shownLinkUp_) {
    shownLinkUp_ = link_.up();
    board_->showLink(shownLinkUp_);
  }
}

bool LinkServer::sendFrame(void* context, uint8_t clientId, const char* frame) {
  auto* self = static_cast<LinkServer*>(context);
  return self->socket_.sendTXT(clientId, frame);
}

void LinkServer::onSample(const signals::Sample& sample, uint32_t nowMs) {
  link_.onSample(sample, nowMs);
}

void LinkServer::toggleRawMode() {
  link_.setRawMode(!link_.rawMode());
  Serial.printf("raw mode %s\n", link_.rawMode() ? "on" : "off");
}

void LinkServer::startRecalibration(uint32_t nowMs) {
  link_.startRecalibration(nowMs);
}

void LinkServer::saveThresholds(void* context, const thresholds::Values& values) {
  static_cast<LinkServer*>(context)->settings_.save(values);
}

void LinkServer::closeClient(void* context, uint8_t clientId) {
  static_cast<LinkServer*>(context)->socket_.disconnect(clientId);
}

void LinkServer::onState(void* context, const protocol::Object& state) {
  auto* self = static_cast<LinkServer*>(context);
  // During a Recalibration the panel belongs to the calibration screen; a
  // `state` frame must not paint over it.
  if (self->link_.calibrating()) {
    return;
  }
  const uint32_t changed = readout::apply(self->readout_, state);
  // Only the fields that moved are repainted, so a Score tick leaves the rest
  // of the panel alone.
  if (changed != 0 && self->board_ != nullptr) {
    self->board_->render(self->readout_, changed);
  }
}

void LinkServer::onCalibration(void* context, classifier::Calibration::Phase phase) {
  auto* self = static_cast<LinkServer*>(context);
  if (self->board_ == nullptr) {
    return;
  }
  switch (phase) {
    case classifier::Calibration::Phase::Capturing:
    case classifier::Calibration::Phase::Failed:
      self->board_->showCalibration(phase);
      break;
    case classifier::Calibration::Phase::Done:
      // Restore the read-out; at boot its Run State is READY.
      self->board_->repaint(self->readout_, self->link_.up());
      break;
    case classifier::Calibration::Phase::Idle:
      break;
  }
}

void LinkServer::onSocketEvent(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("ws %u connected\n", clientId);
      link_.onClientConnected(clientId, millis());
      break;

    case WStype_DISCONNECTED:
      Serial.printf("ws %u disconnected\n", clientId);
      link_.onClientDisconnected(clientId);
      break;

    case WStype_TEXT: {
      // Copy the frame out so it is NUL-terminated for the parser.
      char frame[kMaxFrameLength];
      const size_t copy = length < sizeof(frame) - 1 ? length : sizeof(frame) - 1;
      memcpy(frame, payload, copy);
      frame[copy] = '\0';
      link_.onFrame(clientId, frame, millis());
      break;
    }

    default:
      break;
  }
}

bool LinkServer::serveFile(const String& path) {
  String target = path;
  if (target == "/" || target.endsWith("/")) {
    target += "index.html";
  }
  if (!LittleFS.exists(target)) {
    return false;
  }
  File file = LittleFS.open(target, "r");
  if (!file) {
    return false;
  }
  http_.streamFile(file, contentTypeFor(target));
  file.close();
  return true;
}
