#pragma once

#include <stddef.h>
#include <stdint.h>

// The Device's half of the Session/liveness state machine (`docs/protocol.md`).
//
// It owns no transport: the transport reports connects, disconnects and inbound
// frames, and the Link writes outbound frames back through a sink. Time comes in
// as a monotonic millisecond clock. That keeps the hello handshake, the
// single-Session takeover, the 1 Hz heartbeat and the 3-missed-heartbeat Link
// Down testable on the host.
class Link {
 public:
  // Sends one frame to a client; returns false if it could not be delivered.
  using SendFn = bool (*)(void* context, uint8_t clientId, const char* frame);
  // Closes a client's transport.
  using CloseFn = void (*)(void* context, uint8_t clientId);

  struct Config {
    const char* fw = "0.0.0";
    const char* dev = "000000";
    // The body of the `caps` array, e.g. `"cal","cfg","raw"`. Kept empty until
    // the features it advertises actually exist, so the page is not told the
    // Device can do what it cannot.
    const char* caps = "";
    uint16_t protocolVersion = 1;
    uint32_t hbIntervalMs = 1000;
    // Consecutive missed heartbeats that trip Link Down.
    uint8_t missedHeartbeats = 3;
  };

  void begin(const Config& config, SendFn send, CloseFn close, void* context);

  // Periodic service: emits the heartbeat and watches for silence.
  void tick(uint32_t nowMs);

  // Session lifecycle, reported by the transport.
  void onClientConnected(uint8_t clientId, uint32_t nowMs);
  void onClientDisconnected(uint8_t clientId);

  // One inbound frame. Returns true when it was a well-formed, in-order message
  // from the Session.
  bool onFrame(uint8_t clientId, const char* frame, uint32_t nowMs);

  bool up() const { return up_; }
  // The current Session's client id, or -1 when none is bound.
  int16_t session() const { return session_; }

  // Battery voltage for `hb` telemetry; 0 until the sense is wired (#9).
  void setBatteryMillivolts(int millivolts) { batteryMv_ = millivolts; }

 private:
  void sendHello(uint8_t clientId);
  void sendHeartbeat(uint32_t nowMs);
  void sendError(uint8_t clientId, const char* code, const char* message);

  Config config_{};
  SendFn send_ = nullptr;
  CloseFn close_ = nullptr;
  void* context_ = nullptr;

  int16_t session_ = -1;
  bool up_ = false;
  bool peerHelloSeen_ = false;
  uint32_t lastInboundMs_ = 0;
  uint32_t lastHeartbeatMs_ = 0;
  uint32_t outboundSeq_ = 0;
  uint32_t inboundSeq_ = 0;
  int batteryMv_ = 0;
};
