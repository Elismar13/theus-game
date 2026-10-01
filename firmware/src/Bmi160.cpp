#include "Bmi160.h"

#include <BMI160Gen.h>
#include <Wire.h>

namespace {

constexpr int kI2cAddress = 0x68;

// 4 g of headroom keeps a Jump spike off the rail; 500 deg/s covers a fast
// torso swing. Both are the ranges the LSB factors below assume.
constexpr uint8_t kAccelRangeG = 4;
constexpr uint16_t kGyroRangeDps = 500;
constexpr float kAccelRateHz = 100.0f;
constexpr int kGyroRateHz = 100;

// BMI160 output is 16-bit two's complement, so full scale is 2^15 counts.
constexpr float kAccelLsbPerG = 32768.0f / static_cast<float>(kAccelRangeG);
constexpr float kGyroLsbPerDps = 32768.0f / static_cast<float>(kGyroRangeDps);

}  // namespace

bool Bmi160::begin() {
  // -1 for the interrupt pin: GPIO2 is a strapping pin (docs/hardware.md) and
  // the MVP does not use the sensor interrupt.
  if (!BMI160.begin(BMI160GenClass::I2C_MODE, kI2cAddress, -1)) {
    return false;
  }
  BMI160.setAccelerometerRange(kAccelRangeG);
  BMI160.setGyroRange(kGyroRangeDps);
  BMI160.setAccelerometerRate(kAccelRateHz);
  BMI160.setGyroRate(kGyroRateHz);

  // The library opens the bus at 100 kHz; 400 kHz keeps a 100 Hz burst read
  // comfortably off the Link's path.
  Wire.setClock(400000);
  return true;
}

void Bmi160::read(signals::Sample& out) {
  int ax = 0;
  int ay = 0;
  int az = 0;
  int gx = 0;
  int gy = 0;
  int gz = 0;
  BMI160.readMotionSensor(ax, ay, az, gx, gy, gz);

  out.ax = static_cast<float>(ax) / kAccelLsbPerG;
  out.ay = static_cast<float>(ay) / kAccelLsbPerG;
  out.az = static_cast<float>(az) / kAccelLsbPerG;
  out.gx = static_cast<float>(gx) / kGyroLsbPerDps;
  out.gy = static_cast<float>(gy) / kGyroLsbPerDps;
  out.gz = static_cast<float>(gz) / kGyroLsbPerDps;
}
