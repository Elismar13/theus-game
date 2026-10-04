#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Classifier.h"
#include "Signals.h"
#include "Thresholds.h"

namespace protocol {
struct Object;
}

// The Device's half of the Session/liveness state machine (`docs/protocol.md`).
//
// It owns no transport: the transport reports connects, disconnects and inbound
// frames, and the Link writes outbound frames back through a sink. Time comes in
// as a monotonic millisecond clock. That keeps the hello handshake, the
// single-Session takeover, the 1 Hz heartbeat, the Threshold config exchange and
// the raw debug stream testable on the host.
//
// It also owns no persistence: accepted Thresholds are handed to a SaveFn so the
// Device can write them to NVS while the host tests see a no-op.
class Link {
 public:
  // Sends one frame to a client; returns false if it could not be delivered.
  using SendFn = bool (*)(void* context, uint8_t clientId, const char* frame);
  // Closes a client's transport.
  using CloseFn = void (*)(void* context, uint8_t clientId);
  // Persists accepted Thresholds (NVS on the Device).
  using SaveFn = void (*)(void* context, const thresholds::Values& values);
  // Reports one in-order `state` message, already parsed. The Link does not
  // interpret Run State itself; the mirror decides what it means (#7).
  using StateFn = void (*)(void* context, const protocol::Object& state);

  struct Config {
    const char* fw = "0.0.0";
    const char* dev = "000000";
    // The body of the `caps` array, e.g. `"\"cfg\",\"raw\""`. Kept empty until
    // the features it advertises actually exist, so the page is not told the
    // Device can do what it cannot.
    const char* caps = "";
    uint16_t protocolVersion = 1;
    uint32_t hbIntervalMs = 1000;
    // Consecutive missed heartbeats that trip Link Down.
    uint8_t missedHeartbeats = 3;
    // Live Thresholds, loaded from NVS by the caller before begin().
    thresholds::Values thresholds{};
  };

  void begin(const Config& config, SendFn send, CloseFn close, void* context,
             SaveFn save = nullptr);

  // Registers the `state` sink. Optional: a bench Link without a mirror is fine.
  void setStateHandler(StateFn handler) { stateFn_ = handler; }

  // Periodic service: emits the heartbeat and watches for silence.
  void tick(uint32_t nowMs);

  // Session lifecycle, reported by the transport.
  void onClientConnected(uint8_t clientId, uint32_t nowMs);
  void onClientDisconnected(uint8_t clientId);

  // One inbound frame. Returns true when it was a well-formed, in-order message
  // from the Session.
  bool onFrame(uint8_t clientId, const char* frame, uint32_t nowMs);

  // One Sensor Module reading. Runs the Edge Classifier and, when the Link is
  // up, emits an `evt` for each Intent. In raw mode it also emits a `raw` frame
  // at about 50 Hz (ADR-0002).
  void onSample(const signals::Sample& sample, uint32_t nowMs);

  bool up() const { return up_; }
  // The current Session's client id, or -1 when none is bound.
  int16_t session() const { return session_; }

  // Raw debug mode is driven by the page's `mode` message; exposed so a bench
  // serial command can toggle it too.
  void setRawMode(bool on);
  bool rawMode() const { return rawMode_; }

  const thresholds::Values& thresholds() const { return thresholds_; }

  // The neutral posture the Edge Classifier measures Crawl against. A
  // Recalibration replaces it (#12); it is neutral until then and never
  // persisted.
  void setBaseline(const classifier::Baseline& baseline) { baseline_ = baseline; }

  // Battery voltage for `hb` telemetry; 0 until the sense is wired (#9).
  void setBatteryMillivolts(int millivolts) { batteryMv_ = millivolts; }

 private:
  void sendHello(uint8_t clientId);
  void sendHeartbeat(uint32_t nowMs);
  void sendCfg(uint8_t clientId);
  void sendEvent(classifier::Intent intent, uint32_t nowMs);
  void sendError(uint8_t clientId, const char* code, const char* message);
  // Validates a `cfg.set` patch and writes the merged result to `out`. Returns
  // false when any present field is mistyped or out of range.
  bool applyPatch(const protocol::Object& patch, thresholds::Values& out) const;

  Config config_{};
  thresholds::Values thresholds_{};
  SendFn send_ = nullptr;
  CloseFn close_ = nullptr;
  SaveFn save_ = nullptr;
  StateFn stateFn_ = nullptr;
  void* context_ = nullptr;

  int16_t session_ = -1;
  bool up_ = false;
  bool peerHelloSeen_ = false;
  bool rawMode_ = false;
  uint32_t lastInboundMs_ = 0;
  uint32_t lastHeartbeatMs_ = 0;
  uint32_t lastRawMs_ = 0;
  uint32_t outboundSeq_ = 0;
  uint32_t inboundSeq_ = 0;
  int batteryMv_ = 0;
  classifier::State classifierState_{};
  classifier::Baseline baseline_{};
};
