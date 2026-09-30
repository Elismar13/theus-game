# Newline-delimited JSON over the Link

Every Link message is one JSON object on one line. At a 1 Hz heartbeat plus a handful of events per second, the traffic is far below what the Link can carry, so compact framing buys nothing measurable while plain text keeps the Link inspectable with an ordinary socket tool during development. `docs/protocol.md` is the single hand-maintained definition, mirrored by `web/src/protocol.ts` and `firmware/src/Protocol.h`.

## Considered options

- **Binary framing** — no measurable benefit at this message rate.
- **A generated shared schema** — a codegen pipeline to keep two tiny hand-mirrored files in sync costs more than the duplication it removes.
