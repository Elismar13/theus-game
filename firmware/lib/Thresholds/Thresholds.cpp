#include "Thresholds.h"

namespace thresholds {

Values defaults() {
  return Values{};
}

bool validJumpG(float value) {
  return value >= kJumpGMin && value <= kJumpGMax;
}

bool validCrawlDeg(float value) {
  return value >= kCrawlDegMin && value <= kCrawlDegMax;
}

bool validCrawlHoldMs(int value) {
  return value >= kCrawlHoldMsMin && value <= kCrawlHoldMsMax;
}

bool validJumpRefractoryMs(int value) {
  return value >= kJumpRefractoryMsMin && value <= kJumpRefractoryMsMax;
}

bool valid(const Values& values) {
  return validJumpG(values.jump_g) && validCrawlDeg(values.crawl_deg) &&
         validCrawlHoldMs(values.crawl_hold_ms) &&
         validJumpRefractoryMs(values.jump_refractory_ms);
}

}  // namespace thresholds
