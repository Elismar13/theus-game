# Theus Game

A chest-worn motion controller and a browser runner game. You really jump and really crouch; the character obeys, and the state of your run is mirrored onto a small panel on your chest.

> **Status:** in progress. The walking skeleton is up: the Device serves the
> game page and the Link carries NDJSON; see the roadmap in the issue tracker.

## How it fits together

```
   you (jump / crawl)
        │
        ▼
 ┌──────────────────┐   WiFi SoftAP + WebSocket    ┌──────────────────┐
 │      Device      │ ───────────────────────────► │   game page      │
 │  ESP32 · BMI160  │ ◄─────────────────────────── │  Chrome, canvas  │
 │  ST7735S panel   │      NDJSON (Link)           │  TS + Vite       │
 │  button · buzzer │                              └──────────────────┘
 └──────────────────┘
        ▲
        └── Status Board: Score · Hearts · Link · battery
```

The Device classifies motion into Intents at the edge and sends them over the Link. The game page owns Run State — Score, Hearts, Run State — and pushes it back, which is what the Status Board renders.

Design decisions and the reasoning behind them live in [`docs/adr/`](docs/adr/). The vocabulary is in [`CONTEXT.md`](CONTEXT.md); the wire contract is in [`docs/protocol.md`](docs/protocol.md).

## Repository layout

```
theus-game/
├─ CONTEXT.md          # glossary
├─ README.md
├─ docs/
│  ├─ protocol.md      # the Link contract
│  ├─ hardware.md      # BOM, pin map, power, mounting
│  └─ adr/             # decision records
├─ firmware/           # PlatformIO · Arduino framework · board = esp32dev
└─ web/                # Vite · TypeScript · canvas 2D
```

## Hardware

An ESP32-WROOM-32D devkit, a BMI160 on I2C, a 0.96" 80×160 ST7735S IPS panel on SPI, a momentary button, a passive buzzer, and a protected 1S LiPo behind a charger and a 5 V boost. Full pin map, power topology, and panel notes: [`docs/hardware.md`](docs/hardware.md).

## Building and flashing

The game page is bundled into the firmware's LittleFS filesystem, so flashing is
two steps: the page, then the firmware.

```
# web
cd web && npm install
npm run dev                               # development; set VITE_LINK_URL to the Device's WS endpoint
npm run build                             # output lands in firmware/data/ for LittleFS

# firmware
cd firmware
pio run -t uploadfs                       # write firmware/data/ to the Device's flash
pio run -t upload                         # flash the firmware itself
pio test -e native                        # host tests for the pure modules
```

`pio run -t buildfs` builds the filesystem image on its own, without a Device.

## Definition of done

The MVP is done when all of the following hold:

1. The Device boots, auto-calibrates (`CALIBRATING → READY` on the Status Board), and comes up as an access point.
2. The game page opens from a phone **and** a laptop — from the LittleFS build and from the Vite dev server — with `LINK OK` shown on both ends.
3. A player wearing the Device, over a two-minute Run, achieves **≥95% Jump recognition**, **≥90% Crawl recognition**, and **<5% false positives**.
4. End-to-end Jump latency is **<100 ms p95**.
5. Score and Hearts agree between the canvas and the Status Board.
6. Link loss pauses both ends (`LINK LOST`) and resumes on a countdown.
7. Recalibration works from the button and from the page.
8. Battery voltage on the Status Board is within ±0.2 V of a multimeter reading.
9. Restart works from the button and from the page.
10. A firmware update flashes over the air while the Device runs on battery.

Criterion 3 is the one that decides whether this is a game or a demo.

## Roadmap

Past the MVP: a day/night cycle, near-miss scoring, and — if the chest-mounted Status Board proves too hard to glance at — a separate wrist-mounted display module.

## License

MIT. See [`LICENSE`](LICENSE).
