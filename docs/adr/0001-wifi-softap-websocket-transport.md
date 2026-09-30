# Transport: WiFi SoftAP + WebSocket

The Device must be playable without a cable, and the Status Board needs data flowing *back* from the game page, so the Link has to be bidirectional. The ESP32 therefore runs a WebSocket server in SoftAP mode (plus a station connection for internet and OTA), and the game page speaks to it over that socket. Traffic is newline-delimited JSON (ADR-0006) on port 81, with the game page itself served over HTTP on port 80.

## Considered options

- **Web Serial over USB** — lowest latency and simplest to debug, but it tethers a device whose entire point is portability, and it drags in the browser's secure-context requirement.
- **BLE GATT** — needs no WiFi, but it is a heavier stack with pairing friction, and Web Bluetooth still demands a secure context.
- **BLE HID keyboard** — rejected outright: keyboard emulation is one-way, so the Score could never reach the Status Board.
