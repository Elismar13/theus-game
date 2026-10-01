#include <unity.h>

#include <vector>

#include "Button.h"

namespace {

// Drives a Button with scripted samples and records every gesture it reports.
struct Driver {
  Button button;
  uint32_t now = 0;
  std::vector<Button::Event> events;

  void feed(bool pressed, uint16_t durationMs, uint16_t stepMs = 5) {
    for (uint16_t elapsed = 0; elapsed < durationMs; elapsed += stepMs) {
      Button::Event event = button.update(pressed, now);
      if (event != Button::Event::None) {
        events.push_back(event);
      }
      now += stepMs;
    }
  }
};

}  // namespace

void test_single_click_waits_for_the_double_click_window(void) {
  Driver d;
  d.feed(true, 80);
  d.feed(false, 100);
  TEST_ASSERT_EQUAL_UINT32(0, d.events.size());

  d.feed(false, 300);
  TEST_ASSERT_EQUAL_UINT32(1, d.events.size());
  TEST_ASSERT_EQUAL(Button::Event::Click, d.events[0]);
}

void test_two_slow_clicks_are_two_clicks(void) {
  Driver d;
  d.feed(true, 80);
  d.feed(false, 400);
  d.feed(true, 80);
  d.feed(false, 400);

  TEST_ASSERT_EQUAL_UINT32(2, d.events.size());
  TEST_ASSERT_EQUAL(Button::Event::Click, d.events[0]);
  TEST_ASSERT_EQUAL(Button::Event::Click, d.events[1]);
}

void test_two_quick_clicks_are_one_double_click(void) {
  Driver d;
  d.feed(true, 80);
  d.feed(false, 120);
  d.feed(true, 80);
  d.feed(false, 400);

  TEST_ASSERT_EQUAL_UINT32(1, d.events.size());
  TEST_ASSERT_EQUAL(Button::Event::DoubleClick, d.events[0]);
}

void test_long_press_fires_while_held_and_not_again_on_release(void) {
  Driver d;
  d.feed(true, 1600);
  TEST_ASSERT_EQUAL_UINT32(1, d.events.size());
  TEST_ASSERT_EQUAL(Button::Event::LongPress, d.events[0]);

  d.feed(false, 400);
  TEST_ASSERT_EQUAL_UINT32(1, d.events.size());
}

void test_short_hold_is_a_click_not_a_long_press(void) {
  Driver d;
  d.feed(true, 1400);
  d.feed(false, 400);

  TEST_ASSERT_EQUAL_UINT32(1, d.events.size());
  TEST_ASSERT_EQUAL(Button::Event::Click, d.events[0]);
}

void test_bounce_does_not_misfire(void) {
  Driver d;
  // Contact bounce: the raw level flips faster than the debounce window.
  for (int sample = 0; sample < 12; ++sample) {
    d.feed(sample % 2 == 0, 5, 5);
  }
  d.feed(true, 100);
  d.feed(false, 400);

  TEST_ASSERT_EQUAL_UINT32(1, d.events.size());
  TEST_ASSERT_EQUAL(Button::Event::Click, d.events[0]);
}

void test_a_blip_shorter_than_the_debounce_is_ignored(void) {
  Driver d;
  d.feed(true, 10);
  d.feed(false, 400);

  TEST_ASSERT_EQUAL_UINT32(0, d.events.size());
}

void test_long_press_then_click_reports_both(void) {
  Driver d;
  d.feed(true, 1600);
  d.feed(false, 400);
  d.feed(true, 80);
  d.feed(false, 400);

  TEST_ASSERT_EQUAL_UINT32(2, d.events.size());
  TEST_ASSERT_EQUAL(Button::Event::LongPress, d.events[0]);
  TEST_ASSERT_EQUAL(Button::Event::Click, d.events[1]);
}

void test_click_then_long_press_reports_both(void) {
  Driver d;
  // A click whose double-click window is interrupted by a hold: both the
  // pending click and the long press are real, so both are reported.
  d.feed(true, 80);
  d.feed(false, 120);
  d.feed(true, 1600);
  d.feed(false, 100);

  TEST_ASSERT_EQUAL_UINT32(2, d.events.size());
  TEST_ASSERT_EQUAL(Button::Event::Click, d.events[0]);
  TEST_ASSERT_EQUAL(Button::Event::LongPress, d.events[1]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_single_click_waits_for_the_double_click_window);
  RUN_TEST(test_two_slow_clicks_are_two_clicks);
  RUN_TEST(test_two_quick_clicks_are_one_double_click);
  RUN_TEST(test_long_press_fires_while_held_and_not_again_on_release);
  RUN_TEST(test_short_hold_is_a_click_not_a_long_press);
  RUN_TEST(test_bounce_does_not_misfire);
  RUN_TEST(test_a_blip_shorter_than_the_debounce_is_ignored);
  RUN_TEST(test_long_press_then_click_reports_both);
  RUN_TEST(test_click_then_long_press_reports_both);
  return UNITY_END();
}
