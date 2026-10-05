# Firmware

PlatformIO, Arduino framework, `board = esp32dev`. The pin map is in
[`../docs/hardware.md`](../docs/hardware.md).

## Build and upload

```sh
cd firmware
pio run              # build
pio run -t upload    # flash
pio device monitor   # 115200 baud
pio test -e native   # host tests for the pure modules
```

### Getting the game page onto the Device

The page is served from LittleFS, not compiled into the firmware. `npm run
build` in `web/` already writes into `firmware/data/` (its Vite `outDir`), so the
whole deploy is:

```sh
cd web && npm run build        # typecheck + bundle into firmware/data/
cd ../firmware
pio run -t uploadfs            # write firmware/data/ to the LittleFS partition
pio run -t upload              # flash the firmware itself
```

`pio run -t buildfs` builds the filesystem image without a Device, which is
useful as a check. `firmware/data/` is generated and git-ignored.

## Status Board bring-up

The panel is a 1.8" 128x160 ST7735S. Its TFT_eSPI configuration lives in
`platformio.ini` (`build_flags`) rather than in the library's `User_Setup.h`, so
the wiring sits next to the build.

On boot the firmware runs `StatusBoard::selfTest()`:

- a white border is drawn on the physical edges of the 128x160 window,
- a full-width line at mid-height proves every column is addressable, not just
  the extreme rows the border covers,
- red/green/blue/yellow corner markers sit flush in the corners, so a detached
  corner reveals a window offset and a swapped red/blue reveals an RGB panel
  driven as BGR,
- `THEUS / STATUS BOARD / 128x160` is drawn at a known, centred position,
- the backlight sweeps through its eight dimmable steps and settles at full.

The red-PCB module is driven as `ST7735_REDTAB` (no offset, no inversion). If the
image is offset or clipped, fall back to a green-tab variant (`ST7735_GREENTAB`
or `ST7735_GREENTAB2` at offset 2,1; `ST7735_GREENTAB3` at 2,3); if the colours
look inverted, add `-DTFT_INVERSION_ON`.

## Button and buzzer bring-up

The Device's two physical controls. `Button` (pure, in `lib/`, so it is
host-testable) recognises Click, LongPress and DoubleClick from a debounced
digital input. `Buzzer` plays the `sounds::Sound` patterns from `lib/Sounds`
over LEDC without blocking: `play()` selects a pattern and the main loop's
`update()` advances it tone by tone.

On the Device, double-click toggles mute, persisted in NVS (`theus` / `mute`),
while click and long-press (>= 1.5 s) print their event. From the serial
monitor, `j h g r b` play the five patterns (Jump, Heart loss, Game over,
Recalibration, low battery) so each can be checked by ear, and `m` toggles mute.

The backlight and buzzer share the LEDC peripheral: the backlight is on channel
1 and the buzzer on channel 0, the channel Arduino-ESP32's `tone()` claims.

## Sensor Module bring-up

The BMI160 sits on I2C (SDA 21, SCL 22, address 0x68; `docs/hardware.md`).
`src/Bmi160.{h,cpp}` wraps `mncc8337/BMI160-Arduino-Extended`, configured for
100 Hz acceleration and angular rate at +/-4 g and +/-500 deg/s, and scales the
raw counts into g and deg/s. The main loop reads it every 10 ms and hands the reading
to the Link; nothing in the read path blocks the Link.

`lib/Signals` is pure: it derives the two features the Edge Classifier will lean
on — `vert`, the torso-up acceleration in g, and `pitch`, the forward torso angle
from `atan2(az, ay)` in degrees. `lib/Thresholds` holds the four tunable values
and their validation bounds. Both run in the host tests.

Raw debug mode is driven from the page (`mode {m:"raw"}`), which plots the live
signal in the Dev Panel at about 50 Hz; the serial command `x` toggles the same
mode for a bench check. Thresholds are edited in the Dev Panel, validated on the
Device, persisted to NVS (`theus` / `jump_g`, `crawl_deg`, `crawl_hold_ms`,
`jump_refractory_ms`) by `src/Settings.{h,cpp}`, and echoed back as a fresh
`cfg`. An out-of-range value is refused with `err {code:"cfg_rejected"}` and
nothing changes.

## The Link

The Device is a WiFi SoftAP named `THEUS-RUN` at `192.168.4.1` (mDNS
`theus.local`). It serves the built game page from LittleFS over HTTP on port 80
and runs the Link — a WebSocket carrying NDJSON — on port 81. The contract is
[`../docs/protocol.md`](../docs/protocol.md).

On connect the Device sends `hello` (advertising `caps:["cfg","raw"]`) and its
live `cfg`, then `hb` every second. The game page answers with `hello` and
re-sends `state` at least every second; three consecutive missed heartbeats
(about 3 s) is Link Down. `LINK OK` / `LINK LOST` is shown on both the page and
the Status Board. Exactly one Session exists: a second page takes over and the
first is told `session_taken` and dropped.

The decision-making lives in pure modules so it can run on the host:
`lib/Protocol` is the NDJSON codec and `lib/Link` is the Session/liveness state
machine, which also validates Threshold patches and gates the raw stream.
`src/LinkServer.{h,cpp}` is only the WiFi/HTTP/WebSocket transport, NVS
persistence and the Status Board mirror.

To play during development, the Vite dev server needs a Link endpoint: set
`VITE_LINK_URL=ws://192.168.4.1:81/` (the default on `localhost`).

## Layout

- `lib/Button/` — the click / long-press / double-click recogniser (pure).
- `lib/Sounds/` — the buzzer's patterns (pure data).
- `lib/Signals/` — pitch / vertical derivation from a Sensor Module reading (pure).
- `lib/Thresholds/` — the tunable values, defaults and validation bounds (pure).
- `lib/Protocol/` — the Link NDJSON codec (pure, host-tested).
- `lib/Link/` — the Session/liveness state machine, Threshold validation and raw
  gating (pure, host-tested).
- `src/Bmi160.{h,cpp}` — the BMI160 over I2C, scaled to g and deg/s.
- `src/Settings.{h,cpp}` — Thresholds persisted in NVS.
- `src/Buzzer.{h,cpp}` — LEDC playback and the persisted mute.
- `src/StatusBoard.{h,cpp}` — the panel and its LEDC backlight.
- `src/LinkServer.{h,cpp}` — SoftAP, HTTP file server, WebSocket, Link glue.
- `src/main.cpp` — bring-up entry point and the 100 Hz sampling loop.
- `test/` — host tests for `Button`, `Sounds`, `Signals`, `Thresholds`,
  `Protocol` and `Link` (`pio test -e native`).
- `data/` — the built game page (generated; `pio run -t uploadfs`).
