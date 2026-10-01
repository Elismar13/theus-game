#include "Signals.h"

#include <math.h>

namespace signals {
namespace {

constexpr float kRadiansToDegrees = 57.2957795f;

}  // namespace

Derived derive(const Sample& sample) {
  Derived derived;
  derived.pitch = atan2f(sample.az, sample.ay) * kRadiansToDegrees;
  derived.vert = sample.ay;
  return derived;
}

}  // namespace signals
