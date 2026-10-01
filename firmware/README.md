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

## The Link

The Device is a WiFi SoftAP named `THEUS-RUN` at `192.168.4.1` (mDNS
`theus.local`). It serves the built game page from LittleFS over HTTP on port 80
and runs the Link — a WebSocket carrying NDJSON — on port 81. The contract is
[`../docs/protocol.md`](../docs/protocol.md).

On boot, the Device sends `hello`, then `hb` every second. The game page answers
with `hello` and re-sends `state` at least every second; three consecutive missed
heartbeats (about 3 s) is Link Down. `LINK OK` / `LINK LOST` is shown on both the
page and the Status Board. Exactly one Session exists: a second page takes over
and the first is told `session_taken` and dropped.

The decision-making lives in two pure modules so it can run on the host:
`lib/Protocol` is the NDJSON codec and `lib/Link` is the Session/liveness state
machine. `src/LinkServer.{h,cpp}` is only the WiFi/HTTP/WebSocket transport and
the Status Board mirror.

To play during development, the Vite dev server needs a Link endpoint: set
`VITE_LINK_URL=ws://192.168.4.1:81/` (the default on `localhost`).

## Layout

- `lib/Button/` — the click / long-press / double-click recogniser (pure).
- `lib/Sounds/` — the buzzer's patterns (pure data).
- `lib/Protocol/` — the Link NDJSON codec (pure, host-tested).
- `lib/Link/` — the Session/liveness state machine (pure, host-tested).
- `src/Buzzer.{h,cpp}` — LEDC playback and the persisted mute.
- `src/StatusBoard.{h,cpp}` — the panel and its LEDC backlight.
- `src/LinkServer.{h,cpp}` — SoftAP, HTTP file server, WebSocket, Link glue.
- `src/main.cpp` — bring-up entry point.
- `test/` — host tests for `Button`, `Sounds`, `Protocol` and `Link`
  (`pio test -e native`).
- `data/` — the built game page (generated; `pio run -t uploadfs`).
