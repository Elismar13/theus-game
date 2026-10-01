#pragma once

// The two derived features of a Sensor Module reading that the Edge Classifier
// leans on. Pure: no I/O, no Arduino, so the derivation runs on the host.
//
// Axes are the player's body (docs/hardware.md): X to the right, Y up the
// torso, Z out through the chest. A reading at rest reads about +1 g on Y.
namespace signals {

// One Sensor Module reading, scaled to physical units.
struct Sample {
  float ax;  // g
  float ay;  // g
  float az;  // g
  float gx;  // deg/s
  float gy;  // deg/s
  float gz;  // deg/s
};

struct Derived {
  // Torso pitch in degrees, the rotation about X: 0 upright, growing as the
  // chest tips forward. The sign convention is refined on hardware in #12.
  float pitch;
  // Vertical acceleration on the torso-up axis, in g. A Jump is a positive
  // spike here.
  float vert;
};

// Derives pitch and vertical acceleration from one reading.
Derived derive(const Sample& sample);

}  // namespace signals
