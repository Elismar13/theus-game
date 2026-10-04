#include <cstdio>
#include <cstring>
#include <string>
#include <unity.h>
#include <vector>

#include "Protocol.h"

namespace {

/**
 * The golden files in docs/ are the shared contract with `web/src/protocol.test.ts`.
 * PlatformIO runs the native test from the project directory, but a couple of
 * fallbacks keep this working if it is launched from the repo root instead.
 */
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

}  // namespace

void test_parses_every_page_to_device_fixture(void) {
  const std::vector<std::string> lines = fixtureLines("protocol.page-to-device.ndjson");
  TEST_ASSERT_TRUE_MESSAGE(lines.size() >= 6, "page-to-device fixture is missing");

  for (const std::string& line : lines) {
    protocol::Object object;
    TEST_ASSERT_TRUE_MESSAGE(protocol::parse(line.c_str(), object), line.c_str());
    char type[24];
    TEST_ASSERT_TRUE(protocol::getString(object, "t", type, sizeof(type)));
    TEST_ASSERT_TRUE(strlen(type) > 0);
  }
}

void test_reads_hello_and_state_fields(void) {
  protocol::Object hello;
  TEST_ASSERT_TRUE(protocol::parse("{\"t\":\"hello\",\"v\":1,\"app\":\"theus-web\"}", hello));
  long version = 0;
  TEST_ASSERT_TRUE(protocol::getInt(hello, "v", version));
  TEST_ASSERT_EQUAL_INT(1, version);
  char app[32];
  TEST_ASSERT_TRUE(protocol::getString(hello, "app", app, sizeof(app)));
  TEST_ASSERT_EQUAL_STRING("theus-web", app);

  protocol::Object state;
  TEST_ASSERT_TRUE(protocol::parse(
      "{\"t\":\"state\",\"score\":120,\"hi\":340,\"hearts\":2,\"max_hearts\":3,"
      "\"run\":\"RUN\",\"seq\":2}",
      state));
  long score = 0;
  TEST_ASSERT_TRUE(protocol::getInt(state, "score", score));
  TEST_ASSERT_EQUAL_INT(120, score);
  char run[16];
  TEST_ASSERT_TRUE(protocol::getString(state, "run", run, sizeof(run)));
  TEST_ASSERT_EQUAL_STRING("RUN", run);
  long missing = 0;
  TEST_ASSERT_FALSE(protocol::getInt(state, "rssi", missing));
}

void test_keeps_nested_values_verbatim(void) {
  protocol::Object object;
  TEST_ASSERT_TRUE(protocol::parse("{\"t\":\"cfg\",\"set\":{\"jump_g\":1.5}}", object));
  char type[24];
  TEST_ASSERT_TRUE(protocol::getString(object, "t", type, sizeof(type)));
  TEST_ASSERT_EQUAL_STRING("cfg", type);
  TEST_ASSERT_EQUAL_STRING("{\"jump_g\":1.5}", object.find("set"));
}

void test_rejects_malformed_objects(void) {
  protocol::Object object;
  TEST_ASSERT_FALSE(protocol::parse("not json", object));
  TEST_ASSERT_FALSE(protocol::parse("[]", object));
  TEST_ASSERT_FALSE(protocol::parse("{\"a\":}", object));
  TEST_ASSERT_FALSE(protocol::parse("{\"a\":1", object));
  TEST_ASSERT_FALSE(protocol::parse("{\"a\":1,}", object));
  TEST_ASSERT_TRUE(protocol::parse("{}", object));
  TEST_ASSERT_EQUAL_UINT32(0, object.count);
}

void test_writer_matches_the_golden_frames(void) {
  const std::vector<std::string> lines = fixtureLines("protocol.device-to-page.ndjson");
  TEST_ASSERT_TRUE_MESSAGE(lines.size() >= 14, "device-to-page fixture is missing");

  char hello[192];
  protocol::Writer helloWriter(hello, sizeof(hello));
  helloWriter.objectStart();
  helloWriter.key("t");
  helloWriter.string("hello");
  helloWriter.key("v");
  helloWriter.number(1L);
  helloWriter.key("fw");
  helloWriter.string("0.1.0");
  helloWriter.key("dev");
  helloWriter.string("AABBCC");
  helloWriter.key("caps");
  helloWriter.raw("[\"cal\",\"cfg\",\"raw\"]");
  helloWriter.objectEnd();
  TEST_ASSERT_TRUE(helloWriter.ok());
  TEST_ASSERT_EQUAL_STRING(lines[0].c_str(), hello);

  char heartbeat[128];
  protocol::Writer hbWriter(heartbeat, sizeof(heartbeat));
  hbWriter.objectStart();
  hbWriter.key("t");
  hbWriter.string("hb");
  hbWriter.key("up_ms");
  hbWriter.number(1234L);
  hbWriter.key("batt_mv");
  hbWriter.number(3900L);
  hbWriter.key("seq");
  hbWriter.number(1L);
  hbWriter.objectEnd();
  TEST_ASSERT_TRUE(hbWriter.ok());
  TEST_ASSERT_EQUAL_STRING(lines[1].c_str(), heartbeat);

  char events[96];
  protocol::Writer eventWriter(events, sizeof(events));
  eventWriter.objectStart();
  eventWriter.key("t");
  eventWriter.string("evt");
  eventWriter.key("e");
  eventWriter.string("JUMP");
  eventWriter.key("ts");
  eventWriter.number(1000L);
  eventWriter.key("seq");
  eventWriter.number(3L);
  eventWriter.objectEnd();
  TEST_ASSERT_TRUE(eventWriter.ok());
  TEST_ASSERT_EQUAL_STRING(lines[9].c_str(), events);

  char cfg[160];
  protocol::Writer cfgWriter(cfg, sizeof(cfg));
  cfgWriter.objectStart();
  cfgWriter.key("t");
  cfgWriter.string("cfg");
  cfgWriter.key("jump_g");
  cfgWriter.number(1.6);
  cfgWriter.key("crawl_deg");
  cfgWriter.number(45.0);
  cfgWriter.key("crawl_hold_ms");
  cfgWriter.number(150L);
  cfgWriter.key("jump_refractory_ms");
  cfgWriter.number(250L);
  cfgWriter.key("seq");
  cfgWriter.number(6L);
  cfgWriter.objectEnd();
  TEST_ASSERT_TRUE(cfgWriter.ok());
  TEST_ASSERT_EQUAL_STRING(lines[12].c_str(), cfg);

  char raw[192];
  protocol::Writer rawWriter(raw, sizeof(raw));
  rawWriter.objectStart();
  rawWriter.key("t");
  rawWriter.string("raw");
  rawWriter.key("ax");
  rawWriter.number(0.01);
  rawWriter.key("ay");
  rawWriter.number(-0.98);
  rawWriter.key("az");
  rawWriter.number(0.12);
  rawWriter.key("gx");
  rawWriter.number(0.5);
  rawWriter.key("gy");
  rawWriter.number(0.1);
  rawWriter.key("gz");
  rawWriter.number(-0.2);
  rawWriter.key("pitch");
  rawWriter.number(3.4);
  rawWriter.key("vert");
  rawWriter.number(0.05);
  rawWriter.key("ts");
  rawWriter.number(2000L);
  rawWriter.objectEnd();
  TEST_ASSERT_TRUE(rawWriter.ok());
  TEST_ASSERT_EQUAL_STRING(lines[13].c_str(), raw);
}

void test_reads_a_cfg_patch_verbatim(void) {
  protocol::Object object;
  TEST_ASSERT_TRUE(
      protocol::parse("{\"t\":\"cfg\",\"set\":{\"jump_g\":1.7,\"crawl_hold_ms\":200}}", object));
  TEST_ASSERT_EQUAL_STRING("{\"jump_g\":1.7,\"crawl_hold_ms\":200}", object.find("set"));

  protocol::Object patch;
  TEST_ASSERT_TRUE(protocol::parse(object.find("set"), patch));
  double jumpG = 0;
  TEST_ASSERT_TRUE(protocol::getNumber(patch, "jump_g", jumpG));
  TEST_ASSERT_FLOAT_WITHIN(0.001, 1.7, jumpG);
  long holdMs = 0;
  TEST_ASSERT_TRUE(protocol::getInt(patch, "crawl_hold_ms", holdMs));
  TEST_ASSERT_EQUAL_INT(200, holdMs);
}

void test_writer_reports_overflow(void) {
  char small[8];
  protocol::Writer writer(small, sizeof(small));
  writer.objectStart();
  writer.key("t");
  writer.string("hello");
  TEST_ASSERT_FALSE(writer.ok());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_parses_every_page_to_device_fixture);
  RUN_TEST(test_reads_hello_and_state_fields);
  RUN_TEST(test_keeps_nested_values_verbatim);
  RUN_TEST(test_rejects_malformed_objects);
  RUN_TEST(test_writer_matches_the_golden_frames);
  RUN_TEST(test_reads_a_cfg_patch_verbatim);
  RUN_TEST(test_writer_reports_overflow);
  return UNITY_END();
}
