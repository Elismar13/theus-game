#include "Link.h"

#include <string.h>

#include "Protocol.h"

namespace {

// The Sensor Module is read at 100 Hz (#8), and the raw debug stream is half
// that — about 50 Hz — per `docs/protocol.md`.
constexpr uint32_t kRawIntervalMs = 20;

}  // namespace

void Link::begin(const Config& config, SendFn send, CloseFn close, void* context,
                 SaveFn save) {
  config_ = config;
  thresholds_ = config.thresholds;
  send_ = send;
  close_ = close;
  save_ = save;
  context_ = context;

  session_ = -1;
  up_ = false;
  peerHelloSeen_ = false;
  rawMode_ = false;
  lastInboundMs_ = 0;
  lastHeartbeatMs_ = 0;
  lastRawMs_ = 0;
  outboundSeq_ = 0;
  inboundSeq_ = 0;
  classifierState_ = classifier::State{};
  calibration_ = classifier::Calibration{};
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
  rawMode_ = false;
  outboundSeq_ = 0;
  inboundSeq_ = 0;
  lastInboundMs_ = nowMs;
  lastHeartbeatMs_ = nowMs;
  lastRawMs_ = 0;
  // A new Session starts with no half-finished gesture from the old one.
  classifierState_ = classifier::State{};

  sendHello(clientId);
  // The protocol asks the Device to send its live Thresholds on connect.
  sendCfg(clientId);
}

void Link::onClientDisconnected(uint8_t clientId) {
  if (session_ < 0 || clientId != static_cast<uint8_t>(session_)) {
    return;
  }
  session_ = -1;
  up_ = false;
  peerHelloSeen_ = false;
  // Discard any in-flight gesture: no `evt` may straddle a disconnect.
  classifierState_ = classifier::State{};
}

void Link::tick(uint32_t nowMs) {
  // The time failsafe runs even with no Session: a capture must not hang.
  if (calibration_.active() && calibration_.tick(nowMs)) {
    finishCalibration();
  }

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

void Link::startRecalibration(uint32_t nowMs) {
  if (calibration_.active()) {
    return;
  }
  // Freeze the Edge Classifier: no gesture may straddle the capture.
  classifierState_ = classifier::State{};
  calibration_.start(nowMs);
  sendCal("started");
  if (calFn_ != nullptr) {
    calFn_(context_, calibration_.phase());
  }
}

void Link::setRawMode(bool on) {
  rawMode_ = on;
  // Emit the next sample promptly rather than waiting out the interval.
  lastRawMs_ = 0;
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

  // A frame only counts as liveness once it is accepted. A stale or repeated
  // `seq` is a frame from before a reconnect; dropping it must also mean it
  // cannot hold the Link up. Only messages that carry a `seq` are filtered;
  // `hello`, `cal`, `cfg` and `mode` do not.
  long seq = 0;
  if (protocol::getInt(object, "seq", seq)) {
    if (seq <= 0 || static_cast<uint32_t>(seq) <= inboundSeq_) {
      return false;
    }
    inboundSeq_ = static_cast<uint32_t>(seq);
  }

  lastInboundMs_ = nowMs;

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

  if (strcmp(type, "state") == 0) {
    // Run State and Score are the page's to own; the Device mirrors them (#7).
    if (stateFn_ != nullptr) {
      stateFn_(context_, object);
    }
    return true;
  }

  if (strcmp(type, "cal") == 0) {
    char action[16];
    if (!protocol::getString(object, "action", action, sizeof(action)) ||
        strcmp(action, "recalibrate") != 0) {
      sendError(clientId, "bad_message", "unknown cal action");
      return false;
    }
    startRecalibration(nowMs);
    return true;
  }

  if (strcmp(type, "mode") == 0) {
    char mode[8];
    if (!protocol::getString(object, "m", mode, sizeof(mode))) {
      sendError(clientId, "bad_message", "missing mode");
      return false;
    }
    if (strcmp(mode, "raw") == 0) {
      setRawMode(true);
      return true;
    }
    if (strcmp(mode, "play") == 0) {
      setRawMode(false);
      return true;
    }
    sendError(clientId, "bad_message", "unknown mode");
    return false;
  }

  if (strcmp(type, "cfg") == 0) {
    const char* set = object.find("set");
    protocol::Object patch;
    thresholds::Values updated;
    if (set == nullptr || !protocol::parse(set, patch) || !applyPatch(patch, updated)) {
      sendError(clientId, "cfg_rejected", "invalid thresholds");
      return false;
    }
    thresholds_ = updated;
    if (save_ != nullptr) {
      save_(context_, thresholds_);
    }
    // The fresh `cfg` is the acknowledgement.
    sendCfg(clientId);
    return true;
  }

  sendError(clientId, "bad_message", "unknown message type");
  return false;
}

bool Link::applyPatch(const protocol::Object& patch, thresholds::Values& out) const {
  thresholds::Values candidate = thresholds_;
  bool any = false;
  double number = 0;
  long integer = 0;

  // Every key must be a known Threshold. An unrecognised name is a typo, not a
  // no-op: accepting it would drop the value the page meant to change.
  for (size_t i = 0; i < patch.count; ++i) {
    const char* key = patch.fields[i].key;
    if (strcmp(key, "jump_g") == 0) {
      if (!protocol::getNumber(patch, key, number) ||
          !thresholds::validJumpG(static_cast<float>(number))) {
        return false;
      }
      candidate.jump_g = static_cast<float>(number);
    } else if (strcmp(key, "crawl_deg") == 0) {
      if (!protocol::getNumber(patch, key, number) ||
          !thresholds::validCrawlDeg(static_cast<float>(number))) {
        return false;
      }
      candidate.crawl_deg = static_cast<float>(number);
    } else if (strcmp(key, "crawl_hold_ms") == 0) {
      if (!protocol::getInt(patch, key, integer) ||
          !thresholds::validCrawlHoldMs(static_cast<int>(integer))) {
        return false;
      }
      candidate.crawl_hold_ms = static_cast<int>(integer);
    } else if (strcmp(key, "jump_refractory_ms") == 0) {
      if (!protocol::getInt(patch, key, integer) ||
          !thresholds::validJumpRefractoryMs(static_cast<int>(integer))) {
        return false;
      }
      candidate.jump_refractory_ms = static_cast<int>(integer);
    } else {
      return false;
    }
    any = true;
  }

  if (!any) {
    return false;
  }
  out = candidate;
  return true;
}

void Link::onSample(const signals::Sample& sample, uint32_t nowMs) {
  if (calibration_.active()) {
    // A capture owns the samples: the Edge Classifier is frozen so no `evt` can
    // fire from the motion of settling, but the raw stream still runs.
    if (calibration_.addSample(sample, nowMs)) {
      finishCalibration();
    }
    maybeSendRaw(sample, nowMs);
    return;
  }

  if (!up_ || session_ < 0 || send_ == nullptr) {
    return;
  }

  // The Edge Classifier runs on every sample, whatever the debug mode, so an
  // Intent is never missed and raw mode can watch signal and detection together
  // (ADR-0002).
  const classifier::Intent intent =
      classifier::step(classifierState_, sample, baseline_, thresholds_, nowMs);
  if (intent != classifier::Intent::None) {
    sendEvent(intent, nowMs);
  }

  maybeSendRaw(sample, nowMs);
}

void Link::maybeSendRaw(const signals::Sample& sample, uint32_t nowMs) {
  if (!up_ || !rawMode_ || session_ < 0 || send_ == nullptr) {
    return;
  }
  if (lastRawMs_ != 0 && nowMs - lastRawMs_ < kRawIntervalMs) {
    return;
  }
  lastRawMs_ = nowMs;

  const signals::Derived derived = signals::derive(sample);
  char buffer[224];
  protocol::Writer writer(buffer, sizeof(buffer));
  writer.objectStart();
  writer.key("t");
  writer.string("raw");
  writer.key("ax");
  writer.number(static_cast<double>(sample.ax));
  writer.key("ay");
  writer.number(static_cast<double>(sample.ay));
  writer.key("az");
  writer.number(static_cast<double>(sample.az));
  writer.key("gx");
  writer.number(static_cast<double>(sample.gx));
  writer.key("gy");
  writer.number(static_cast<double>(sample.gy));
  writer.key("gz");
  writer.number(static_cast<double>(sample.gz));
  writer.key("pitch");
  writer.number(static_cast<double>(derived.pitch));
  writer.key("vert");
  writer.number(static_cast<double>(derived.vert));
  writer.key("ts");
  writer.number(static_cast<long>(nowMs));
  writer.objectEnd();
  if (writer.ok()) {
    send_(context_, static_cast<uint8_t>(session_), buffer);
  }
}

void Link::finishCalibration() {
  if (calibration_.phase() == classifier::Calibration::Phase::Done) {
    baseline_ = calibration_.baseline();
    sendCal("done");
  } else {
    sendCal("failed", "too noisy");
  }
  if (calFn_ != nullptr) {
    calFn_(context_, calibration_.phase());
  }
}

void Link::sendCal(const char* phase, const char* reason) {
  if (send_ == nullptr || session_ < 0) {
    return;
  }
  char buffer[96];
  protocol::Writer writer(buffer, sizeof(buffer));
  writer.objectStart();
  writer.key("t");
  writer.string("cal");
  writer.key("phase");
  writer.string(phase);
  if (reason != nullptr) {
    writer.key("reason");
    writer.string(reason);
  }
  writer.objectEnd();
  if (writer.ok()) {
    send_(context_, static_cast<uint8_t>(session_), buffer);
  }
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

void Link::sendCfg(uint8_t clientId) {
  if (send_ == nullptr) {
    return;
  }
  char buffer[160];
  protocol::Writer writer(buffer, sizeof(buffer));
  writer.objectStart();
  writer.key("t");
  writer.string("cfg");
  writer.key("jump_g");
  writer.number(static_cast<double>(thresholds_.jump_g));
  writer.key("crawl_deg");
  writer.number(static_cast<double>(thresholds_.crawl_deg));
  writer.key("crawl_hold_ms");
  writer.number(static_cast<long>(thresholds_.crawl_hold_ms));
  writer.key("jump_refractory_ms");
  writer.number(static_cast<long>(thresholds_.jump_refractory_ms));
  writer.key("seq");
  writer.number(static_cast<long>(++outboundSeq_));
  writer.objectEnd();
  if (writer.ok()) {
    send_(context_, clientId, buffer);
  }
}

void Link::sendEvent(classifier::Intent intent, uint32_t nowMs) {
  if (send_ == nullptr || session_ < 0) {
    return;
  }
  char buffer[96];
  protocol::Writer writer(buffer, sizeof(buffer));
  writer.objectStart();
  writer.key("t");
  writer.string("evt");
  writer.key("e");
  writer.string(classifier::intentName(intent));
  writer.key("ts");
  writer.number(static_cast<long>(nowMs));
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
