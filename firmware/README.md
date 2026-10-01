# Firmware

PlatformIO, Arduino framework, `board = esp32dev`. The pin map is in
[`../docs/hardware.md`](../docs/hardware.md).

## Build and upload

```sh
cd firmware
pio run              # build
pio run -t upload    # flash
pio device monitor   # 115200 baud
```

## Status Board bring-up

The panel is a 0.96" 80x160 IPS ST7735S. Its TFT_eSPI configuration lives in
`platformio.ini` (`build_flags`) rather than in the library's `User_Setup.h`, so
the wiring sits next to the build.

On boot the firmware runs `StatusBoard::selfTest()`:

- a white border is drawn on the physical edges of the 80x160 window,
- red/green/blue/yellow corner markers sit flush in the corners, so a detached
  corner reveals a window offset and a swapped red/blue reveals an RGB panel
  driven as BGR,
- `THEUS / STATUS BOARD / 80x160` is drawn at a known, centred position,
- the backlight sweeps through its eight dimmable steps and settles at full.

If the image is offset or clipped, switch the tab define to
`-DST7735_REDTAB160x80` (24-column offset instead of 26) and add
`-DTFT_INVERSION_ON`, because only `GREENTAB160x80` inverts on its own. A unit
marked GC9106 is driven by the ST7735 init in practice.

## Layout

- `src/StatusBoard.{h,cpp}` — the panel and its LEDC backlight.
- `src/main.cpp` — bring-up entry point.
