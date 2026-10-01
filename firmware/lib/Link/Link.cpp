#include "Link.h"

#include <string.h>

#include "Protocol.h"

void Link::begin(const Config& config, SendFn send, CloseFn close, void* context) {
  config_ = config;
  send_ = send;
  close_ = close;
  context_ = context;

  session_ = -1;
  up_ = false;
  peerHelloSeen_ = false;
  lastInboundMs_ = 0;
  lastHeartbeatMs_ = 0;
  outboundSeq_ = 0;
  inboundSeq_ = 0;
}

void Link::onClientConnected(uint8_t clientId, uint32_t nowMs) {
  // Exactly one Session exists: a later client takes over, and the previous one
  // is told and dropped.
  if (session_ >= 0 && session_ != static_cast<int16_t>(clientId)) {
    const uint8_t previous = static_cast<uint8_t>(session_);
    sendError(previous, "session_taken", "another session took over");
    if (close_ != nullptr) {
      close_(context_, previous);
    }
  }

  session_ = clientId;
  up_ = false;
  peerHelloSeen_ = false;
  outboundSeq_ = 0;
  inboundSeq_ = 0;
  lastInboundMs_ = nowMs;
  lastHeartbeatMs_ = nowMs;

  sendHello(clientId);
}

void Link::onClientDisconnected(uint8_t clientId) {
  if (session_ < 0 || clientId != static_cast<uint8_t>(session_)) {
    return;
  }
  session_ = -1;
  up_ = false;
  peerHelloSeen_ = false;
}

void Link::tick(uint32_t nowMs) {
  if (session_ < 0) {
    return;
  }

  if (nowMs - lastHeartbeatMs_ >= config_.hbIntervalMs) {
    lastHeartbeatMs_ = nowMs;
    sendHeartbeat(nowMs);
  }

  if (up_ && nowMs - lastInboundMs_ >=
                 static_cast<uint32_t>(config_.hbIntervalMs) * config_.missedHeartbeats) {
    up_ = false;
  }
}

bool Link::onFrame(uint8_t clientId, const char* frame, uint32_t nowMs) {
  if (session_ < 0 || clientId != static_cast<uint8_t>(session_)) {
    return false;
  }

  protocol::Object object;
  if (!protocol::parse(frame, object)) {
    sendError(clientId, "bad_message", "malformed json");
    return false;
  }

  char type[24];
  if (!protocol::getString(object, "t", type, sizeof(type))) {
    sendError(clientId, "bad_message", "missing type");
    return false;
  }

  lastInboundMs_ = nowMs;

  // `seq` is per sender and starts at 1; anything at or below the last accepted
  // value is a stale frame from before a reconnect.
  long seq = 0;
  if (protocol::getInt(object, "seq", seq)) {
    if (seq <= 0 || static_cast<uint32_t>(seq) <= inboundSeq_) {
      return false;
    }
    inboundSeq_ = static_cast<uint32_t>(seq);
  }

  if (strcmp(type, "hello") == 0) {
    long version = 0;
    if (!protocol::getInt(object, "v", version) || version != config_.protocolVersion) {
      sendError(clientId, "protocol_version", "protocol version mismatch");
      // The mismatch is fatal: drop the Session ourselves rather than waiting
      // for the transport's disconnect callback.
      session_ = -1;
      up_ = false;
      peerHelloSeen_ = false;
      if (close_ != nullptr) {
        close_(context_, clientId);
      }
      return false;
    }
    peerHelloSeen_ = true;
    up_ = true;
    return true;
  }

  if (peerHelloSeen_) {
    up_ = true;
  }

  // Acknowledge the messages this ticket knows; later tickets read their
  // payloads. `evt` is Device-to-page, so it is not accepted here.
  if (strcmp(type, "state") == 0 || strcmp(type, "cal") == 0 ||
      strcmp(type, "cfg") == 0 || strcmp(type, "mode") == 0) {
    return true;
  }

  sendError(clientId, "bad_message", "unknown message type");
  return false;
}

void Link::sendHello(uint8_t clientId) {
  if (send_ == nullptr) {
    return;
  }
  char buffer[192];
  protocol::Writer writer(buffer, sizeof(buffer));
  writer.objectStart();
  writer.key("t");
  writer.string("hello");
  writer.key("v");
  writer.number(static_cast<long>(config_.protocolVersion));
  writer.key("fw");
  writer.string(config_.fw);
  writer.key("dev");
  writer.string(config_.dev);
  writer.key("caps");
  writer.raw("[");
  writer.raw(config_.caps);
  writer.raw("]");
  writer.objectEnd();
  if (writer.ok()) {
    send_(context_, clientId, buffer);
  }
}

void Link::sendHeartbeat(uint32_t nowMs) {
  if (send_ == nullptr || session_ < 0) {
    return;
  }
  char buffer[128];
  protocol::Writer writer(buffer, sizeof(buffer));
  writer.objectStart();
  writer.key("t");
  writer.string("hb");
  writer.key("up_ms");
  writer.number(static_cast<long>(nowMs));
  writer.key("batt_mv");
  writer.number(static_cast<long>(batteryMv_));
  writer.key("seq");
  writer.number(static_cast<long>(++outboundSeq_));
  writer.objectEnd();
  if (writer.ok()) {
    send_(context_, static_cast<uint8_t>(session_), buffer);
  }
}

void Link::sendError(uint8_t clientId, const char* code, const char* message) {
  if (send_ == nullptr) {
    return;
  }
  char buffer[128];
  protocol::Writer writer(buffer, sizeof(buffer));
  writer.objectStart();
  writer.key("t");
  writer.string("err");
  writer.key("code");
  writer.string(code);
  writer.key("msg");
  writer.string(message);
  writer.objectEnd();
  if (writer.ok()) {
    send_(context_, clientId, buffer);
  }
}
