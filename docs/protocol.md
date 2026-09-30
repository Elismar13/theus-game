# Link Protocol

Version **1**. This is the contract between the Device and the game page. Terms (`Link`, `Session`, `Intent`, `Run State`, `Threshold`) are defined in [`CONTEXT.md`](../CONTEXT.md).

## Transport

- The Device is a WiFi SoftAP named `THEUS-RUN` at `192.168.4.1` (mDNS `theus.local`) and, when configured, also joins a station network for internet and OTA.
- The game page is served over **HTTP on port 80** — from LittleFS in the field, or from the Vite dev server on `localhost` during development.
- The Link is a **WebSocket on port 81** at `/`.
- Framing: each WebSocket text frame carries exactly one compact JSON object, serialized on a single line. A sender must never emit a raw newline inside a message.
- Timestamps and counters are integers. Sensor values may be floats. Timestamps are milliseconds on the sender's monotonic clock (`ts` on the Device side, not comparable across devices).

## Session

- Exactly one Session exists. The first client to connect becomes the Session. A later connection takes over: the Device sends `err {code:"session_taken"}` to the previous Session and closes it.
- On connect the Device sends `hello` immediately and the client replies with `hello`.
- If the two `hello` messages disagree on `v`, the Device sends `err {code:"protocol_version"}` and closes.

## Liveness

- The Device sends `hb` every 1000 ms.
- Either side treats three consecutive missed heartbeats (about 3 s) as **Link Down**.
- On Link Down: the browser pauses the Run and shows `LINK LOST`; the Device shows `LINK LOST` on the Status Board and stops emitting `evt`.
- Reconnect uses exponential backoff from 250 ms to a 5 s cap. On reconnect the browser immediately re-sends `state`.

## Ordering

- Each direction carries `seq`, a per-sender counter starting at 1.
- A receiver drops any message whose `seq` is less than or equal to the last accepted `seq` from that sender. This discards frames that went stale across a reconnect.

## Messages

### Device → game page

| `t` | Purpose |
| --- | --- |
| `hello` | Opens the Session |
| `evt` | An Intent |
| `cal` | Recalibration progress |
| `cfg` | Current Thresholds |
| `hb` | Heartbeat and device telemetry |
| `raw` | Debug signal stream |
| `err` | Protocol error |

**`hello`**

| Field | Type | Meaning |
| --- | --- | --- |
| `v` | int | Protocol version |
| `fw` | string | Firmware version |
| `dev` | string | Device identifier, derived from the MAC |
| `caps` | string[] | Supported features, e.g. `["cal","cfg","raw"]` |

**`evt`** — an Intent

| Field | Type | Meaning |
| --- | --- | --- |
| `e` | string | `"JUMP"`, `"CRAWL"`, or `"UP"` |
| `ts` | int | Device monotonic ms at detection |
| `seq` | int | Sender counter |
| `conf` | number | Optional classifier confidence, 0–1 |

**`cal`**

| Field | Type | Meaning |
| --- | --- | --- |
| `phase` | string | `"started"`, `"done"`, or `"failed"` |
| `reason` | string | Present when `phase` is `"failed"` |

**`cfg`** — current Thresholds

| Field | Type | Unit |
| --- | --- | --- |
| `jump_g` | number | g |
| `crawl_deg` | number | degrees |
| `crawl_hold_ms` | int | ms |
| `jump_refractory_ms` | int | ms |
| `seq` | int | Sender counter |

**`hb`**

| Field | Type | Meaning |
| --- | --- | --- |
| `up_ms` | int | Device uptime |
| `batt_mv` | int | Battery voltage in millivolts |
| `rssi` | int | Station RSSI; omitted when not joined |
| `seq` | int | Sender counter |

**`raw`** — debug only, emitted at about 50 Hz in raw mode

| Field | Type | Meaning |
| --- | --- | --- |
| `ax` `ay` `az` | number | Acceleration in g |
| `gx` `gy` `gz` | number | Angular rate in deg/s |
| `pitch` | number | Torso pitch in degrees |
| `vert` | number | Vertical acceleration in g |
| `ts` | int | Device monotonic ms |

**`err`**

| Field | Type | Meaning |
| --- | --- | --- |
| `code` | string | `"session_taken"`, `"protocol_version"`, `"bad_message"`, or `"cfg_rejected"` |
| `msg` | string | Human-readable detail |

### Game page → Device

**`hello`**: `v` (int), `app` (string)

**`state`** — sent on change, and at least every 1000 ms

| Field | Type | Meaning |
| --- | --- | --- |
| `score` | int | Current Score |
| `hi` | int | High Score |
| `hearts` | int | Remaining Hearts |
| `max_hearts` | int | Hearts at the start of a Run |
| `run` | string | Run State: `"READY"`, `"RUN"`, `"JUMP"`, `"CRAWL"`, `"INVULN"`, `"DEAD"`, or `"PAUSED"` |
| `seq` | int | Sender counter |

**`cal`**: `action` (string) — `"recalibrate"`

**`cfg`**: `set` (object) — any subset of the `cfg` fields above

**`mode`**: `m` (string) — `"play"` or `"raw"`

## Recalibration

1. Either side starts it: the browser sends `cal {action:"recalibrate"}`, or the player long-presses the button and the Device starts unsolicited.
2. The Device replies `cal {phase:"started"}` and shows `CALIBRATING — STAND STILL` on the Status Board.
3. The browser **must** freeze Run input while a Recalibration is in flight, or the player will lose a Heart to a phantom Jump.
4. After roughly 1.5 s of stillness the Device replies `cal {phase:"done"}`, or `cal {phase:"failed"}` if the readings were too noisy.
5. The new Baseline is used for the rest of the power cycle. It is never persisted.

## Tuning

- On connect the Device sends `cfg` with its live values, and the browser shows them in the dev panel.
- The browser may send `cfg {set:{...}}`. The Device validates the values, persists them to NVS, and echoes a fresh `cfg` as the acknowledgement.
- Invalid values are refused with `err {code:"cfg_rejected"}` and nothing changes.

## Debug stream

- `mode {m:"raw"}` turns on `raw` at about 50 Hz. `evt` continues to be emitted in raw mode, so signal and detection can be watched together.
- Raw mode is a development tool and is not used during play.

## Versioning

`v` is an integer, incremented on any breaking change to the message set. A mismatch is a hard failure: `err {code:"protocol_version"}`, then close.

## Measuring latency

The end-to-end budget is the sum of two measurements, each taken where it can be observed:

- **Device:** motion to `evt` emission. Reported in `hb`.
- **Browser:** `evt` receipt to the frame in which the character reacts. Exposed in the debug overlay.

Neither side can measure the whole path alone, so neither is allowed to claim the number.
