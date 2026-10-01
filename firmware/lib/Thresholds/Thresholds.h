#pragma once

#include <stdint.h>

// The configurable motion values that separate an Intent from noise
// (CONTEXT.md: Threshold). Pure, so validation can be tested on the host; the
// Device persists accepted values to NVS.
//
// Defaults and NVS keys are from the README / `docs/protocol.md`. The Baseline
// is deliberately not part of this and is never persisted.
namespace thresholds {

struct Values {
  float jump_g = 1.6f;            // NVS `jump_g`
  float crawl_deg = 45.0f;        // NVS `crawl_deg`
  int crawl_hold_ms = 150;        // NVS `crawl_hold_ms`
  int jump_refractory_ms = 250;   // NVS `jump_refractory_ms`
};

// The README defaults: jump_g 1.6, crawl_deg 45, crawl_hold_ms 150,
// jump_refractory_ms 250.
Values defaults();

// Per-field bounds. A value outside its bound is refused with
// `err cfg_rejected` and nothing changes.
constexpr float kJumpGMin = 1.0f;
constexpr float kJumpGMax = 4.0f;
constexpr float kCrawlDegMin = 10.0f;
constexpr float kCrawlDegMax = 90.0f;
constexpr int kCrawlHoldMsMin = 50;
constexpr int kCrawlHoldMsMax = 1000;
constexpr int kJumpRefractoryMsMin = 50;
constexpr int kJumpRefractoryMsMax = 1000;

bool validJumpG(float value);
bool validCrawlDeg(float value);
bool validCrawlHoldMs(int value);
bool validJumpRefractoryMs(int value);

// True when every field is within its bound.
bool valid(const Values& values);

}  // namespace thresholds
