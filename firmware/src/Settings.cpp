#include "Settings.h"

namespace {

constexpr const char* kNamespace = "theus";
constexpr const char* kJumpGKey = "jump_g";
constexpr const char* kCrawlDegKey = "crawl_deg";
constexpr const char* kCrawlHoldMsKey = "crawl_hold_ms";
constexpr const char* kJumpRefractoryMsKey = "jump_refractory_ms";

}  // namespace

void Settings::begin() {
  preferences_.begin(kNamespace, false);

  values_ = thresholds::defaults();
  values_.jump_g = preferences_.getFloat(kJumpGKey, values_.jump_g);
  values_.crawl_deg = preferences_.getFloat(kCrawlDegKey, values_.crawl_deg);
  values_.crawl_hold_ms = preferences_.getInt(kCrawlHoldMsKey, values_.crawl_hold_ms);
  values_.jump_refractory_ms =
      preferences_.getInt(kJumpRefractoryMsKey, values_.jump_refractory_ms);

  // A value written by an older build, or a corrupted store, must not leave the
  // Device classifying against nonsense.
  if (!thresholds::valid(values_)) {
    values_ = thresholds::defaults();
  }
}

void Settings::save(const thresholds::Values& values) {
  values_ = values;
  preferences_.putFloat(kJumpGKey, values_.jump_g);
  preferences_.putFloat(kCrawlDegKey, values_.crawl_deg);
  preferences_.putInt(kCrawlHoldMsKey, values_.crawl_hold_ms);
  preferences_.putInt(kJumpRefractoryMsKey, values_.jump_refractory_ms);
}
