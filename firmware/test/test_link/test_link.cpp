#include <cstring>
#include <string>
#include <unity.h>
#include <vector>

#include "Link.h"

namespace {

struct Sent {
  uint8_t client;
  std::string frame;
};

struct Transport {
  Link link;
  std::vector<Sent> sent;
  std::vector<uint8_t> closed;

  static bool onSend(void* context, uint8_t client, const char* frame) {
    static_cast<Transport*>(context)->sent.push_back({client, frame});
    return true;
  }

  static void onClose(void* context, uint8_t client) {
    static_cast<Transport*>(context)->closed.push_back(client);
  }

  void begin() {
    Link::Config config;
    config.fw = "0.1.0";
    config.dev = "AABBCC";
    link.begin(config, onSend, onClose, this);
  }

  void heardHello(uint8_t client, uint32_t nowMs) {
    link.onFrame(client, "{\"t\":\"hello\",\"v\":1,\"app\":\"theus-web\"}", nowMs);
  }
};

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

void test_hello_opens_the_session_on_connect(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(3, 0);

  TEST_ASSERT_EQUAL_UINT32(1, transport.sent.size());
  TEST_ASSERT_EQUAL_UINT8(3, transport.sent[0].client);
  TEST_ASSERT_EQUAL_STRING(
      "{\"t\":\"hello\",\"v\":1,\"fw\":\"0.1.0\",\"dev\":\"AABBCC\",\"caps\":[]}",
      transport.sent[0].frame.c_str());
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
  TEST_ASSERT_EQUAL_STRING(
      "{\"t\":\"hb\",\"up_ms\":1000,\"batt_mv\":3900,\"seq\":1}",
      transport.sent[0].frame.c_str());

  transport.link.tick(2000);
  TEST_ASSERT_EQUAL_UINT32(2, transport.sent.size());
  TEST_ASSERT_EQUAL_STRING(
      "{\"t\":\"hb\",\"up_ms\":2000,\"batt_mv\":3900,\"seq\":2}",
      transport.sent[1].frame.c_str());
}

void test_unknown_message_is_refused_with_an_error(void) {
  Transport transport;
  transport.begin();
  transport.link.onClientConnected(1, 0);
  transport.heardHello(1, 0);
  transport.sent.clear();

  TEST_ASSERT_FALSE(transport.link.onFrame(1, "{\"t\":\"wat\"}", 10));
  TEST_ASSERT_EQUAL_UINT32(1, transport.sent.size());
  TEST_ASSERT_EQUAL_STRING("err", typeOf(transport.sent[0].frame).c_str());
  TEST_ASSERT_EQUAL_STRING("bad_message", codeOf(transport.sent[0].frame).c_str());
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
  TEST_ASSERT_EQUAL_UINT32(2, transport.sent.size());
  TEST_ASSERT_EQUAL_UINT8(1, transport.sent[0].client);
  TEST_ASSERT_EQUAL_STRING("session_taken", codeOf(transport.sent[0].frame).c_str());
  TEST_ASSERT_EQUAL_UINT8(2, transport.sent[1].client);
  TEST_ASSERT_EQUAL_STRING("hello", typeOf(transport.sent[1].frame).c_str());
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

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_hello_opens_the_session_on_connect);
  RUN_TEST(test_link_comes_up_on_the_peer_hello);
  RUN_TEST(test_link_goes_down_after_three_missed_heartbeats);
  RUN_TEST(test_heartbeat_is_sent_every_second_with_a_rising_seq);
  RUN_TEST(test_unknown_message_is_refused_with_an_error);
  RUN_TEST(test_protocol_version_mismatch_closes_the_client);
  RUN_TEST(test_a_second_client_takes_over_the_session);
  RUN_TEST(test_stale_seq_is_dropped);
  RUN_TEST(test_frames_from_a_non_session_client_are_ignored);
  RUN_TEST(test_disconnect_clears_the_session);
  return UNITY_END();
}
