# Write the engine from scratch, borrow the tuning

The game is a Chrome-dino-style runner. Rather than fork the BSD-3 extraction of Chromium's dino (`wayou/t-rex-runner`) or salvage the earlier local prototype, the engine is written from scratch in TypeScript. The one thing taken from the dino is its *tuning* — jump arc, speed ramp, obstacle spacing, ground scroll rate — which is a set of numbers, not code.

## Considered options

- **Fork `wayou/t-rex-runner`** — the fastest path to something playable, but its state model has no Hearts, no external input, and no two-way link, so it would be fought more than reused.
- **Salvage the earlier local prototype** — deliberately abandoned; neither its art nor its structure was carried forward.
