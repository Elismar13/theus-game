#include <cstring>
#include <math.h>
#include <string>
#include <unity.h>
#include <vector>

#include "Link.h"
#include "Protocol.h"

namespace {

struct Sent {
  uint8_t client;
  std::string frame;
};

struct Transport {
  Link link;
  std::vector<Sent> sent;
  std::vector<uint8_t> closed;
  std::vector<thresholds::Values> saved;
  /** The Score of each `state` frame the mirror saw, in arrival order. */
  std::vector<long> stateScores;

  static bool onSend(void* context, uint8_t client, const char* frame) {
    static_cast<Transport*>(context)->sent.push_back({client, frame});
    return true;
  }

  static void onClose(void* context, uint8_t client) {
    static_cast<Transport*>(context)->closed.push_back(client);
  }

  static void onSave(void* context, const thresholds::Values& values) {
    static_cast<Transport*>(context)->saved.push_back(values);
  }

  static void onState(void* context, const protocol::Object& state) {
    long score = -1;
    protocol::getInt(state, "score", score);
    static_cast<Transport*>(context)->stateScores.push_back(score);
  }

  void begin() {
    Link::Config config;
    config.fw = "0.1.0";
    config.dev = "AABBCC";
    config.caps = "\"cfg\",\"raw\"";
    link.begin(config, onSend, onClose, this, onSave);
  }

  void heardHello(uint8_t client, uint32_t nowMs) {
    link.onFrame(client, "{\"t\":\"hello\",\"v\":1,\"app\":\"theus-web\"}", nowMs);
  }
};

signals::Sample stillSample() {
  signals::Sample sample;
  sample.ax = 0.01f;
  sample.ay = 0.98f;
  sample.az = 0.12f;
  sample.gx = 0.5f;
  sample.gy = 0.1f;
  sample.gz = -0.2f;
  return sample;
}

/** A sample that derives to the given torso pitch (deg) and vertical accel (g). */
signals::Sample sampleAt(float pitchDeg, float vert) {
  const float kRad = 3.14159265358979323846f / 180.0f;
  signals::Sample sample;
  sample.ax = 0.0f;
  sample.ay = vert;
  sample.az = vert * tanf(pitchDeg * kRad);
  sample.gx = 0.0f;
  sample.gy = 0.0f;
  sample.gz = 0.0f;
  return sample;
}

/** The `e` field of an `evt` frame. */
std::string eventNameOf(const std::string& frame) {
  protocol::Object object;
  protocol::parse(frame.c_str(), object);
  char name[16];
  if (!protocol::getString(object, "e", name, sizeof(name))) {
    return "";
  }
  return name;
}

/** Every frame starts `{"t":"..."`; the tests only need the type. */
std::string typeOf(const std::string& frame) {
  const size_t start = frame.find("\"t\":\"") + 5;
  const size_t end = frame.find('"', start);
  return frame.substr(start, end - start);
}

std::string codeOf(const std::string& frame) {
  const size_t start = frame.find("\"code\":\"") + 8;
  const size_t end = frame.find('"', start);
  return frame.substr(start, end - start);
}

}  // namespace

void test_hello_opens_the_session_and_carries_the_thresholds(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(3, 0);

  TEST_ASSERT_EQUAL_UINT32(2, transport.sent.size());
  TEST_ASSERT_EQUAL_UINT8(3, transport.sent[0].client);
  TEST_ASSERT_EQUAL_STRING(
      "{\"t\":\"hello\",\"v\":1,\"fw\":\"0.1.0\",\"dev\":\"AABBCC\",\"caps\":[\"cfg\",\"raw\"]}",
      transport.sent[0].frame.c_str());
  TEST_ASSERT_EQUAL_UINT8(3, transport.sent[1].client);
  TEST_ASSERT_EQUAL_STRING(
      "{\"t\":\"cfg\",\"jump_g\":1.6,\"crawl_deg\":45,\"crawl_hold_ms\":150,"
      "\"jump_refractory_ms\":250,\"seq\":1}",
      transport.sent[1].frame.c_str());
  TEST_ASSERT_FALSE(transport.link.up());
}

void test_link_comes_up_on_the_peer_hello(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 5);

  TEST_ASSERT_TRUE(transport.link.up());
  TEST_ASSERT_EQUAL_INT(1, transport.link.session());
}

void test_link_goes_down_after_three_missed_heartbeats(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);

  transport.link.tick(2999);
  TEST_ASSERT_TRUE(transport.link.up());

  transport.link.tick(3001);
  TEST_ASSERT_FALSE(transport.link.up());
}

void test_heartbeat_is_sent_every_second_with_a_rising_seq(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.link.setBatteryMillivolts(3900);
  transport.sent.clear();

  transport.link.tick(999);
  TEST_ASSERT_EQUAL_UINT32(0, transport.sent.size());

  transport.link.tick(1000);
  TEST_ASSERT_EQUAL_UINT32(1, transport.sent.size());
  TEST_ASSERT_EQUAL_STRING("hb", typeOf(transport.sent[0].frame).c_str());
  // The connect-time `cfg` already took seq 1.
  TEST_ASSERT_EQUAL_STRING(
      "{\"t\":\"hb\",\"up_ms\":1000,\"batt_mv\":3900,\"seq\":2}",
      transport.sent[0].frame.c_str());

  transport.link.tick(2000);
  TEST_ASSERT_EQUAL_UINT32(2, transport.sent.size());
  TEST_ASSERT_EQUAL_STRING(
      "{\"t\":\"hb\",\"up_ms\":2000,\"batt_mv\":3900,\"seq\":3}",
      transport.sent[1].frame.c_str());
}

void test_unknown_message_is_refused_without_closing_the_link(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.sent.clear();

  TEST_ASSERT_FALSE(transport.link.onFrame(1, "{\"t\":\"wat\"}", 10));
  TEST_ASSERT_EQUAL_UINT32(1, transport.sent.size());
  TEST_ASSERT_EQUAL_STRING("err", typeOf(transport.sent[0].frame).c_str());
  TEST_ASSERT_EQUAL_STRING("bad_message", codeOf(transport.sent[0].frame).c_str());

  // A bad message must not close the Link: no close callback, the Session is
  // still bound, and the next well-formed frame is accepted.
  TEST_ASSERT_EQUAL_UINT32(0, transport.closed.size());
  TEST_ASSERT_TRUE(transport.link.up());
  TEST_ASSERT_EQUAL_INT(1, transport.link.session());
  TEST_ASSERT_TRUE(transport.link.onFrame(1, "{\"t\":\"state\",\"score\":1,\"seq\":1}", 11));
}

void test_a_dropped_stale_frame_does_not_count_as_liveness(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);

  // Accept seq 5, so anything at or below it is stale.
  TEST_ASSERT_TRUE(transport.link.onFrame(
      1, "{\"t\":\"state\",\"score\":5,\"seq\":5}", 0));
  // A stale frame at 2000 is dropped, and must not refresh liveness.
  TEST_ASSERT_FALSE(transport.link.onFrame(
      1, "{\"t\":\"state\",\"score\":4,\"seq\":4}", 2000));

  // The last accepted frame was at 0, so three missed heartbeats trips Down.
  transport.link.tick(3001);
  TEST_ASSERT_FALSE(transport.link.up());
}

void test_protocol_version_mismatch_closes_the_client(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.sent.clear();

  TEST_ASSERT_FALSE(
      transport.link.onFrame(1, "{\"t\":\"hello\",\"v\":2,\"app\":\"theus-web\"}", 10));
  TEST_ASSERT_EQUAL_UINT32(1, transport.sent.size());
  TEST_ASSERT_EQUAL_STRING("protocol_version", codeOf(transport.sent[0].frame).c_str());
  TEST_ASSERT_EQUAL_UINT32(1, transport.closed.size());
  TEST_ASSERT_EQUAL_UINT8(1, transport.closed[0]);
  TEST_ASSERT_EQUAL_INT(-1, transport.link.session());
}

void test_a_second_client_takes_over_the_session(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.sent.clear();

  transport.link.onClientConnected(2, 20);

  TEST_ASSERT_EQUAL_INT(2, transport.link.session());
  TEST_ASSERT_EQUAL_UINT32(3, transport.sent.size());
  TEST_ASSERT_EQUAL_UINT8(1, transport.sent[0].client);
  TEST_ASSERT_EQUAL_STRING("session_taken", codeOf(transport.sent[0].frame).c_str());
  TEST_ASSERT_EQUAL_UINT8(2, transport.sent[1].client);
  TEST_ASSERT_EQUAL_STRING("hello", typeOf(transport.sent[1].frame).c_str());
  TEST_ASSERT_EQUAL_STRING("cfg", typeOf(transport.sent[2].frame).c_str());
  TEST_ASSERT_EQUAL_UINT32(1, transport.closed.size());
  TEST_ASSERT_EQUAL_UINT8(1, transport.closed[0]);
  TEST_ASSERT_FALSE(transport.link.up());
}

void test_stale_seq_is_dropped(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);

  TEST_ASSERT_TRUE(transport.link.onFrame(
      1, "{\"t\":\"state\",\"score\":10,\"seq\":5}", 10));
  TEST_ASSERT_FALSE(transport.link.onFrame(
      1, "{\"t\":\"state\",\"score\":9,\"seq\":4}", 11));
  TEST_ASSERT_FALSE(transport.link.onFrame(
      1, "{\"t\":\"state\",\"score\":10,\"seq\":5}", 12));
  TEST_ASSERT_TRUE(transport.link.onFrame(
      1, "{\"t\":\"state\",\"score\":11,\"seq\":6}", 13));
}

void test_frames_from_a_non_session_client_are_ignored(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.sent.clear();

  TEST_ASSERT_FALSE(transport.link.onFrame(9, "{\"t\":\"state\",\"seq\":1}", 10));
  TEST_ASSERT_EQUAL_UINT32(0, transport.sent.size());
}

void test_disconnect_clears_the_session(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);

  transport.link.onClientDisconnected(1);
  TEST_ASSERT_EQUAL_INT(-1, transport.link.session());
  TEST_ASSERT_FALSE(transport.link.up());
}

void test_raw_mode_streams_samples_at_about_50hz(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  TEST_ASSERT_TRUE(transport.link.onFrame(1, "{\"t\":\"mode\",\"m\":\"raw\"}", 10));
  TEST_ASSERT_TRUE(transport.link.rawMode());
  transport.sent.clear();

  transport.link.onSample(stillSample(), 100);
  transport.link.onSample(stillSample(), 110);  // inside the interval: dropped
  transport.link.onSample(stillSample(), 120);

  TEST_ASSERT_EQUAL_UINT32(2, transport.sent.size());
  TEST_ASSERT_EQUAL_STRING("raw", typeOf(transport.sent[0].frame).c_str());
  protocol::Object raw;
  TEST_ASSERT_TRUE(protocol::parse(transport.sent[0].frame.c_str(), raw));
  double ay = 0;
  double vert = 0;
  double pitch = 0;
  TEST_ASSERT_TRUE(protocol::getNumber(raw, "ay", ay));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.98, ay);
  TEST_ASSERT_TRUE(protocol::getNumber(raw, "vert", vert));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 0.98, vert);
  TEST_ASSERT_TRUE(protocol::getNumber(raw, "pitch", pitch));
  TEST_ASSERT_FLOAT_WITHIN(0.01, 6.98, pitch);
  long ts = 0;
  TEST_ASSERT_TRUE(protocol::getInt(raw, "ts", ts));
  TEST_ASSERT_EQUAL_INT(100, ts);
}

void test_play_mode_stops_the_raw_stream(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.link.onFrame(1, "{\"t\":\"mode\",\"m\":\"raw\"}", 10);
  transport.link.onFrame(1, "{\"t\":\"mode\",\"m\":\"play\"}", 11);
  TEST_ASSERT_FALSE(transport.link.rawMode());
  transport.sent.clear();

  transport.link.onSample(stillSample(), 100);
  TEST_ASSERT_EQUAL_UINT32(0, transport.sent.size());
}

void test_raw_is_not_emitted_before_the_link_is_up(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  // The peer has not answered `hello`, so the Link is not up.
  transport.link.setRawMode(true);
  transport.sent.clear();

  transport.link.onSample(stillSample(), 100);
  TEST_ASSERT_EQUAL_UINT32(0, transport.sent.size());
}

void test_unknown_mode_is_refused(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.sent.clear();

  TEST_ASSERT_FALSE(transport.link.onFrame(1, "{\"t\":\"mode\",\"m\":\"loud\"}", 10));
  TEST_ASSERT_EQUAL_STRING("bad_message", codeOf(transport.sent[0].frame).c_str());
}

void test_valid_cfg_patch_updates_echoes_and_persists(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.sent.clear();

  TEST_ASSERT_TRUE(transport.link.onFrame(
      1, "{\"t\":\"cfg\",\"set\":{\"jump_g\":1.7,\"crawl_hold_ms\":200}}", 10));

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.7f, transport.link.thresholds().jump_g);
  TEST_ASSERT_EQUAL_INT(200, transport.link.thresholds().crawl_hold_ms);
  // Unmentioned fields keep their previous values.
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 45.0f, transport.link.thresholds().crawl_deg);
  TEST_ASSERT_EQUAL_INT(1, transport.saved.size());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.7f, transport.saved[0].jump_g);

  TEST_ASSERT_EQUAL_UINT32(1, transport.sent.size());
  TEST_ASSERT_EQUAL_STRING("cfg", typeOf(transport.sent[0].frame).c_str());
  TEST_ASSERT_EQUAL_STRING(
      "{\"t\":\"cfg\",\"jump_g\":1.7,\"crawl_deg\":45,\"crawl_hold_ms\":200,"
      "\"jump_refractory_ms\":250,\"seq\":2}",
      transport.sent[0].frame.c_str());
}

void test_out_of_range_cfg_is_rejected_and_changes_nothing(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.sent.clear();

  TEST_ASSERT_FALSE(
      transport.link.onFrame(1, "{\"t\":\"cfg\",\"set\":{\"jump_g\":9.9}}", 10));

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.6f, transport.link.thresholds().jump_g);
  TEST_ASSERT_EQUAL_INT(0, transport.saved.size());
  TEST_ASSERT_EQUAL_UINT32(1, transport.sent.size());
  TEST_ASSERT_EQUAL_STRING("err", typeOf(transport.sent[0].frame).c_str());
  TEST_ASSERT_EQUAL_STRING("cfg_rejected", codeOf(transport.sent[0].frame).c_str());
}

void test_unknown_threshold_is_rejected(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.sent.clear();

  TEST_ASSERT_FALSE(
      transport.link.onFrame(1, "{\"t\":\"cfg\",\"set\":{\"nonsense\":1}}", 10));
  TEST_ASSERT_EQUAL_STRING("cfg_rejected", codeOf(transport.sent[0].frame).c_str());
}

void test_a_misspelled_threshold_rejects_the_whole_patch(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.sent.clear();

  // `crawl_degs` is a typo: the whole patch must be refused, not partly applied.
  TEST_ASSERT_FALSE(transport.link.onFrame(
      1, "{\"t\":\"cfg\",\"set\":{\"jump_g\":1.7,\"crawl_degs\":45}}", 10));

  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.6f, transport.link.thresholds().jump_g);
  TEST_ASSERT_EQUAL_INT(0, transport.saved.size());
  TEST_ASSERT_EQUAL_STRING("cfg_rejected", codeOf(transport.sent[0].frame).c_str());
}

void test_missing_cfg_set_is_rejected(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.sent.clear();

  TEST_ASSERT_FALSE(transport.link.onFrame(1, "{\"t\":\"cfg\"}", 10));
  TEST_ASSERT_EQUAL_STRING("cfg_rejected", codeOf(transport.sent[0].frame).c_str());
}

void test_state_reaches_the_mirror_and_stale_seq_is_dropped(void) {
  Transport transport;
  transport.begin();
  transport.link.setStateHandler(Transport::onState);
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);

  TEST_ASSERT_TRUE(
      transport.link.onFrame(1, "{\"t\":\"state\",\"score\":120,\"seq\":1}", 10));
  TEST_ASSERT_EQUAL_UINT32(1, transport.stateScores.size());
  TEST_ASSERT_EQUAL_INT(120, transport.stateScores[0]);

  // A stale frame is dropped before the mirror ever sees it.
  TEST_ASSERT_FALSE(
      transport.link.onFrame(1, "{\"t\":\"state\",\"score\":9,\"seq\":1}", 11));
  TEST_ASSERT_EQUAL_UINT32(1, transport.stateScores.size());

  TEST_ASSERT_TRUE(
      transport.link.onFrame(1, "{\"t\":\"state\",\"score\":130,\"seq\":2}", 12));
  TEST_ASSERT_EQUAL_UINT32(2, transport.stateScores.size());
  TEST_ASSERT_EQUAL_INT(130, transport.stateScores[1]);
}

void test_a_jump_sample_emits_evt(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.sent.clear();

  transport.link.onSample(stillSample(), 100);
  transport.link.onSample(sampleAt(0.0f, 1.9f), 110);

  TEST_ASSERT_EQUAL_UINT32(1, transport.sent.size());
  TEST_ASSERT_EQUAL_STRING("evt", typeOf(transport.sent[0].frame).c_str());
  TEST_ASSERT_EQUAL_STRING("JUMP", eventNameOf(transport.sent[0].frame).c_str());

  protocol::Object event;
  TEST_ASSERT_TRUE(protocol::parse(transport.sent[0].frame.c_str(), event));
  long ts = 0;
  TEST_ASSERT_TRUE(protocol::getInt(event, "ts", ts));
  TEST_ASSERT_EQUAL_INT(110, ts);
  long seq = 0;
  TEST_ASSERT_TRUE(protocol::getInt(event, "seq", seq));
  TEST_ASSERT_TRUE(seq > 0);
}

void test_a_crouch_sequence_emits_crawl_then_up(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.sent.clear();

  for (uint32_t t = 0; t <= 200; t += 10) {
    transport.link.onSample(sampleAt(50.0f, 0.64f), t);
  }
  for (uint32_t t = 210; t <= 260; t += 10) {
    transport.link.onSample(stillSample(), t);
  }

  TEST_ASSERT_EQUAL_UINT32(2, transport.sent.size());
  TEST_ASSERT_EQUAL_STRING("evt", typeOf(transport.sent[0].frame).c_str());
  TEST_ASSERT_EQUAL_STRING("CRAWL", eventNameOf(transport.sent[0].frame).c_str());
  TEST_ASSERT_EQUAL_STRING("evt", typeOf(transport.sent[1].frame).c_str());
  TEST_ASSERT_EQUAL_STRING("UP", eventNameOf(transport.sent[1].frame).c_str());
}

void test_no_evt_before_the_link_is_up(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  // No peer `hello` yet, so the Link is not up.
  transport.sent.clear();

  transport.link.onSample(sampleAt(0.0f, 1.9f), 100);

  TEST_ASSERT_EQUAL_UINT32(0, transport.sent.size());
}

void test_evt_is_emitted_in_raw_mode_too(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.link.onFrame(1, "{\"t\":\"mode\",\"m\":\"raw\"}", 5);
  transport.sent.clear();

  transport.link.onSample(sampleAt(0.0f, 1.9f), 100);

  // The same sample yields both the Intent and the debug frame.
  TEST_ASSERT_EQUAL_UINT32(2, transport.sent.size());
  size_t events = 0;
  for (const Sent& sent : transport.sent) {
    if (typeOf(sent.frame) == "evt") {
      ++events;
      TEST_ASSERT_EQUAL_STRING("JUMP", eventNameOf(sent.frame).c_str());
    }
  }
  TEST_ASSERT_EQUAL_UINT32(1, events);
}

void test_reconnect_discards_an_in_flight_crouch(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);

  // Commit a Crawl, then drop the Link while still crouched.
  for (uint32_t t = 0; t <= 200; t += 10) {
    transport.link.onSample(sampleAt(50.0f, 0.64f), t);
  }
  TEST_ASSERT_EQUAL_STRING("CRAWL", eventNameOf(transport.sent.back().frame).c_str());
  transport.link.onClientDisconnected(1);

  // A fresh Session: standing up must not emit a phantom Up.
  transport.link.onClientConnected(2, 1000);
  transport.heardHello(2, 1000);
  transport.sent.clear();
  transport.link.onSample(stillSample(), 1010);

  TEST_ASSERT_EQUAL_UINT32(0, transport.sent.size());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_hello_opens_the_session_and_carries_the_thresholds);
  RUN_TEST(test_state_reaches_the_mirror_and_stale_seq_is_dropped);
  RUN_TEST(test_a_jump_sample_emits_evt);
  RUN_TEST(test_a_crouch_sequence_emits_crawl_then_up);
  RUN_TEST(test_no_evt_before_the_link_is_up);
  RUN_TEST(test_evt_is_emitted_in_raw_mode_too);
  RUN_TEST(test_reconnect_discards_an_in_flight_crouch);
  RUN_TEST(test_link_comes_up_on_the_peer_hello);
  RUN_TEST(test_link_goes_down_after_three_missed_heartbeats);
  RUN_TEST(test_heartbeat_is_sent_every_second_with_a_rising_seq);
  RUN_TEST(test_unknown_message_is_refused_without_closing_the_link);
  RUN_TEST(test_a_dropped_stale_frame_does_not_count_as_liveness);
  RUN_TEST(test_protocol_version_mismatch_closes_the_client);
  RUN_TEST(test_a_second_client_takes_over_the_session);
  RUN_TEST(test_stale_seq_is_dropped);
  RUN_TEST(test_frames_from_a_non_session_client_are_ignored);
  RUN_TEST(test_disconnect_clears_the_session);
  RUN_TEST(test_raw_mode_streams_samples_at_about_50hz);
  RUN_TEST(test_play_mode_stops_the_raw_stream);
  RUN_TEST(test_raw_is_not_emitted_before_the_link_is_up);
  RUN_TEST(test_unknown_mode_is_refused);
  RUN_TEST(test_valid_cfg_patch_updates_echoes_and_persists);
  RUN_TEST(test_out_of_range_cfg_is_rejected_and_changes_nothing);
  RUN_TEST(test_unknown_threshold_is_rejected);
  RUN_TEST(test_a_misspelled_threshold_rejects_the_whole_patch);
  RUN_TEST(test_missing_cfg_set_is_rejected);
  return UNITY_END();
}
