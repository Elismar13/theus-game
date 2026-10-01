#include <cstdio>
#include <string>
#include <unity.h>
#include <vector>

#include "Protocol.h"
#include "Readout.h"

namespace {

// The golden files in docs/ are the shared contract with the page. PlatformIO
// runs the native test from the project directory, but a couple of fallbacks
// keep this working if it is launched from the repo root instead.
std::string fixturePath(const char* name) {
  const char* roots[] = {"../docs/", "docs/", "../../docs/"};
  for (const char* root : roots) {
    const std::string candidate = std::string(root) + name;
    if (FILE* file = fopen(candidate.c_str(), "r")) {
      fclose(file);
      return candidate;
    }
  }
  return std::string("../docs/") + name;
}

std::vector<std::string> fixtureLines(const char* name) {
  std::vector<std::string> lines;
  FILE* file = fopen(fixturePath(name).c_str(), "r");
  if (file == nullptr) {
    return lines;
  }
  char buffer[512];
  while (fgets(buffer, sizeof(buffer), file) != nullptr) {
    std::string line(buffer);
    while (!line.empty() &&
           (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
      line.pop_back();
    }
    if (!line.empty() && line[0] != '#') {
      lines.push_back(line);
    }
  }
  fclose(file);
  return lines;
}

/** Parses one frame and applies it; fails the test on a parse error. */
uint32_t applyFrame(readout::State& state, const char* frame) {
  protocol::Object object;
  TEST_ASSERT_TRUE_MESSAGE(protocol::parse(frame, object), frame);
  return readout::apply(state, object);
}

}  // namespace

void test_a_full_state_message_moves_every_field(void) {
  readout::State state;
  const uint32_t changed = applyFrame(
      state, "{\"t\":\"state\",\"score\":120,\"hi\":340,\"hearts\":2,"
             "\"max_hearts\":3,\"run\":\"RUN\",\"seq\":2}");

  TEST_ASSERT_EQUAL_UINT32(readout::kAll, changed);
  TEST_ASSERT_EQUAL_INT(120, state.score);
  TEST_ASSERT_EQUAL_INT(340, state.highScore);
  TEST_ASSERT_EQUAL_INT(2, state.hearts);
  TEST_ASSERT_EQUAL_INT(3, state.maxHearts);
  TEST_ASSERT_TRUE(state.run == readout::RunState::Run);
}

void test_only_the_field_that_changed_is_flagged(void) {
  readout::State state;
  applyFrame(state,
             "{\"t\":\"state\",\"score\":120,\"hi\":340,\"hearts\":2,"
             "\"max_hearts\":3,\"run\":\"RUN\",\"seq\":1}");

  // Same Score, a new High Score: the panel must repaint the High Score only.
  const uint32_t changed = applyFrame(
      state, "{\"t\":\"state\",\"score\":120,\"hi\":500,\"hearts\":2,"
             "\"max_hearts\":3,\"run\":\"RUN\",\"seq\":2}");

  TEST_ASSERT_EQUAL_UINT32(readout::kHighScore, changed);
}

void test_an_unchanged_state_flags_nothing(void) {
  readout::State state;
  const char* frame =
      "{\"t\":\"state\",\"score\":10,\"hi\":20,\"hearts\":3,\"max_hearts\":3,"
      "\"run\":\"RUN\",\"seq\":1}";
  applyFrame(state, frame);

  TEST_ASSERT_EQUAL_UINT32(0, applyFrame(state, frame));
}

void test_hearts_and_max_hearts_share_one_region(void) {
  readout::State state;
  applyFrame(state,
             "{\"t\":\"state\",\"score\":0,\"hi\":0,\"hearts\":3,"
             "\"max_hearts\":3,\"run\":\"READY\",\"seq\":1}");

  // A Heart lost is a slot-row change, not a Score one.
  const uint32_t changed = applyFrame(
      state, "{\"t\":\"state\",\"score\":0,\"hi\":0,\"hearts\":2,"
             "\"max_hearts\":3,\"run\":\"RUN\",\"seq\":2}");

  TEST_ASSERT_EQUAL_UINT32(readout::kHearts | readout::kRun, changed);
  TEST_ASSERT_EQUAL_INT(2, state.hearts);
  TEST_ASSERT_EQUAL_INT(3, state.maxHearts);
}

void test_a_missing_field_keeps_its_previous_value(void) {
  readout::State state;
  applyFrame(state,
             "{\"t\":\"state\",\"score\":120,\"hi\":340,\"hearts\":2,"
             "\"max_hearts\":3,\"run\":\"RUN\",\"seq\":1}");

  // No `hearts` and no `run`: both keep what was last shown.
  const uint32_t changed =
      applyFrame(state, "{\"t\":\"state\",\"score\":130,\"hi\":340,\"seq\":2}");

  TEST_ASSERT_EQUAL_UINT32(readout::kScore, changed);
  TEST_ASSERT_EQUAL_INT(130, state.score);
  TEST_ASSERT_EQUAL_INT(2, state.hearts);
  TEST_ASSERT_TRUE(state.run == readout::RunState::Run);
}

void test_a_malformed_field_is_ignored_while_the_others_update(void) {
  readout::State state;
  applyFrame(state,
             "{\"t\":\"state\",\"score\":120,\"hi\":340,\"hearts\":2,"
             "\"max_hearts\":3,\"run\":\"RUN\",\"seq\":1}");

  // `score` is a string and `hearts` is negative: both keep their old values,
  // but the well-formed `hi` still lands.
  const uint32_t changed = applyFrame(
      state, "{\"t\":\"state\",\"score\":\"lots\",\"hi\":500,\"hearts\":-1,"
             "\"seq\":2}");

  TEST_ASSERT_EQUAL_UINT32(readout::kHighScore, changed);
  TEST_ASSERT_EQUAL_INT(120, state.score);
  TEST_ASSERT_EQUAL_INT(2, state.hearts);
  TEST_ASSERT_EQUAL_INT(500, state.highScore);
}

void test_a_malformed_heart_field_leaves_the_other_half_of_the_row(void) {
  readout::State state;
  applyFrame(state,
             "{\"t\":\"state\",\"score\":0,\"hi\":0,\"hearts\":2,"
             "\"max_hearts\":3,\"run\":\"RUN\",\"seq\":1}");

  // `hearts` is mistyped but `max_hearts` is well-formed: only the latter moves.
  const uint32_t changed = applyFrame(
      state, "{\"t\":\"state\",\"hearts\":\"two\",\"max_hearts\":5,\"seq\":2}");

  TEST_ASSERT_EQUAL_UINT32(readout::kHearts, changed);
  TEST_ASSERT_EQUAL_INT(2, state.hearts);
  TEST_ASSERT_EQUAL_INT(5, state.maxHearts);
}

void test_an_unknown_run_state_is_ignored(void) {
  readout::State state;
  applyFrame(state,
             "{\"t\":\"state\",\"score\":0,\"hi\":0,\"hearts\":3,"
             "\"max_hearts\":3,\"run\":\"RUN\",\"seq\":1}");

  const uint32_t changed = applyFrame(
      state, "{\"t\":\"state\",\"run\":\"FLYING\",\"seq\":2}");

  TEST_ASSERT_EQUAL_UINT32(0, changed);
  TEST_ASSERT_TRUE(state.run == readout::RunState::Run);
}

void test_every_run_state_name_round_trips(void) {
  const readout::RunState states[] = {
      readout::RunState::Ready, readout::RunState::Run,   readout::RunState::Jump,
      readout::RunState::Crawl, readout::RunState::Invuln, readout::RunState::Dead,
      readout::RunState::Paused,
  };
  for (readout::RunState run : states) {
    readout::RunState parsed = readout::RunState::Ready;
    TEST_ASSERT_TRUE(readout::parseRunState(readout::runStateName(run), parsed));
    TEST_ASSERT_TRUE(parsed == run);
  }
  readout::RunState parsed;
  TEST_ASSERT_FALSE(readout::parseRunState("FLYING", parsed));
}

void test_the_golden_state_frames_drive_the_readout(void) {
  readout::State state;
  bool sawState = false;
  for (const std::string& line : fixtureLines("protocol.page-to-device.ndjson")) {
    protocol::Object object;
    TEST_ASSERT_TRUE(protocol::parse(line.c_str(), object));
    char type[16];
    if (!protocol::getString(object, "t", type, sizeof(type))) continue;
    if (std::string(type) != "state") continue;
    sawState = true;
    readout::apply(state, object);
  }

  TEST_ASSERT_TRUE_MESSAGE(sawState, "no state frames in the fixture");
  // The last state frame is {"score":120,"hi":340,"hearts":2,...,"run":"RUN"}.
  TEST_ASSERT_EQUAL_INT(120, state.score);
  TEST_ASSERT_EQUAL_INT(340, state.highScore);
  TEST_ASSERT_EQUAL_INT(2, state.hearts);
  TEST_ASSERT_EQUAL_INT(3, state.maxHearts);
  TEST_ASSERT_TRUE(state.run == readout::RunState::Run);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_full_state_message_moves_every_field);
  RUN_TEST(test_only_the_field_that_changed_is_flagged);
  RUN_TEST(test_an_unchanged_state_flags_nothing);
  RUN_TEST(test_hearts_and_max_hearts_share_one_region);
  RUN_TEST(test_a_missing_field_keeps_its_previous_value);
  RUN_TEST(test_a_malformed_field_is_ignored_while_the_others_update);
  RUN_TEST(test_a_malformed_heart_field_leaves_the_other_half_of_the_row);
  RUN_TEST(test_an_unknown_run_state_is_ignored);
  RUN_TEST(test_every_run_state_name_round_trips);
  RUN_TEST(test_the_golden_state_frames_drive_the_readout);
  return UNITY_END();
}
