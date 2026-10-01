#pragma once

#include <Preferences.h>

#include "Thresholds.h"

// The Device's persisted Thresholds, in NVS under the `theus` namespace
// (README NVS keys). The Baseline is deliberately never persisted (#12).
class Settings {
 public:
  // Opens NVS and loads the saved Thresholds, falling back to the defaults
  // when a stored value is missing or out of range.
  void begin();

  const thresholds::Values& thresholds() const { return values_; }

  // Writes every field. Callers pass values that already validated.
  void save(const thresholds::Values& values);

 private:
  Preferences preferences_;
  thresholds::Values values_{};
};
