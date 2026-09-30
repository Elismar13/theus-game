# Hardware

The Device is a chest-worn ESP32 console: one inertial sensor, one small panel, one button, one buzzer, and a battery.

## Bill of materials

| Part | Spec | Notes |
| --- | --- | --- |
| MCU board | ESP32-WROOM-32D devkit | PlatformIO `board = esp32dev`, Arduino framework |
| Sensor | BMI160 | Accelerometer + gyroscope, I2C |
| Panel | 0.96" 80×160 IPS, ST7735S | SPI, write-only, driven by TFT_eSPI |
| Battery | 1S LiPo, protected, ~1500 mAh | Must be a protected cell |
| Charger | TP4056-class module | With protection and load-sharing so USB and battery do not fight |
| Boost | 5 V step-up | Feeds the devkit's `VIN`; the devkit's own LDO makes 3.3 V |
| Button | Momentary, to GND | Internal pull-up |
| Buzzer | Passive piezo | Driven by LEDC |
| Mount | Chest strap / harness | Sensor flat against the sternum |

## Pin map

| Signal | GPIO | Notes |
| --- | --- | --- |
| I2C SDA | 21 | BMI160 |
| I2C SCL | 22 | BMI160 |
| Sensor INT | 27 | Optional; motion interrupt |
| SPI SCLK | 18 | VSPI |
| SPI MOSI (SDA) | 23 | VSPI |
| Panel CS | 17 | |
| Panel DC | 16 | |
| Panel RES | 4 | |
| Panel backlight | 25 | LEDC PWM for dimming |
| Battery sense | 34 | ADC1, via resistor divider |
| Button | 32 | Input, pull-up |
| Buzzer | 26 | LEDC |

Deliberately unused: `0/2/12/15` (strapping), `6–11` (flash), `1/3` (UART0), `35/36/39` (spare input-only).

**ADC2 is unusable while WiFi is active**, which is always, so battery sensing is on ADC1. The divider should bring a 4.2 V full cell to roughly 2.1 V (a 1:1 divider with a 100 nF bypass capacitor), read with 11 dB attenuation.

## Power

```
  USB  ──►  charger / load-share  ──►  1S LiPo
                     │
                     ├──► 5 V boost ──► ESP32 VIN
                     │
                     └──► divider ──► GPIO34 (battery sense)
```

## Panel notes

The 0.96" 80×160 "ST7735S" panel is a known-troublesome part: some units are mislabeled GC9106, and the visible window is offset from the controller's memory. TFT_eSPI drives it, but expect a custom `User_Setup` and possibly offset corrections before the image fills the window. Wire it carefully — it is unforgiving of loose jumpers.

The panel is write-only; MISO is not connected.

## Mounting and axes

The Sensor Module sits flat against the sternum. Axes are named from the player's body:

- **X** — to the player's right
- **Y** — up the torso
- **Z** — out through the chest

With that convention, a Jump is a positive spike on **Y**, and a Crawl is a sustained forward **pitch** (rotation about X). If the module is mounted differently, the axis mapping is corrected during Recalibration rather than by rewiring.
