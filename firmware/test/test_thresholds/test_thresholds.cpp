#include <unity.h>

#include "Thresholds.h"

void test_defaults_are_the_readme_values(void) {
  const thresholds::Values values = thresholds::defaults();
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.6f, values.jump_g);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 45.0f, values.crawl_deg);
  TEST_ASSERT_EQUAL_INT(150, values.crawl_hold_ms);
  TEST_ASSERT_EQUAL_INT(250, values.jump_refractory_ms);
  TEST_ASSERT_TRUE(thresholds::valid(values));
}

void test_jump_g_bounds(void) {
  TEST_ASSERT_TRUE(thresholds::validJumpG(1.0f));
  TEST_ASSERT_TRUE(thresholds::validJumpG(4.0f));
  TEST_ASSERT_FALSE(thresholds::validJumpG(0.99f));
  TEST_ASSERT_FALSE(thresholds::validJumpG(4.01f));
}

void test_crawl_deg_bounds(void) {
  TEST_ASSERT_TRUE(thresholds::validCrawlDeg(10.0f));
  TEST_ASSERT_TRUE(thresholds::validCrawlDeg(90.0f));
  TEST_ASSERT_FALSE(thresholds::validCrawlDeg(9.9f));
  TEST_ASSERT_FALSE(thresholds::validCrawlDeg(90.1f));
}

void test_crawl_hold_ms_bounds(void) {
  TEST_ASSERT_TRUE(thresholds::validCrawlHoldMs(50));
  TEST_ASSERT_TRUE(thresholds::validCrawlHoldMs(1000));
  TEST_ASSERT_FALSE(thresholds::validCrawlHoldMs(49));
  TEST_ASSERT_FALSE(thresholds::validCrawlHoldMs(1001));
}

void test_jump_refractory_ms_bounds(void) {
  TEST_ASSERT_TRUE(thresholds::validJumpRefractoryMs(50));
  TEST_ASSERT_TRUE(thresholds::validJumpRefractoryMs(1000));
  TEST_ASSERT_FALSE(thresholds::validJumpRefractoryMs(49));
  TEST_ASSERT_FALSE(thresholds::validJumpRefractoryMs(1001));
}

void test_valid_rejects_a_single_out_of_range_field(void) {
  thresholds::Values values = thresholds::defaults();
  values.crawl_hold_ms = 5000;
  TEST_ASSERT_FALSE(thresholds::valid(values));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_are_the_readme_values);
  RUN_TEST(test_jump_g_bounds);
  RUN_TEST(test_crawl_deg_bounds);
  RUN_TEST(test_crawl_hold_ms_bounds);
  RUN_TEST(test_jump_refractory_ms_bounds);
  RUN_TEST(test_valid_rejects_a_single_out_of_range_field);
  return UNITY_END();
}
