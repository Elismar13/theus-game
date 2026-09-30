# Classify Intents on the Device

The Edge Classifier runs on the Device at 100 Hz and emits Intents across the Link; the browser never sees raw motion except in an explicit debug mode. Classifying at the edge keeps a Jump off the Link's latency and jitter, and the Status Board needs the same Intent stream regardless of what the browser later does with it.

## Consequences

A raw debug stream exists so Thresholds can be tuned without reflashing the Device on every adjustment. It is a development mode, not part of play, and it is why the protocol carries a `raw` message at all.
