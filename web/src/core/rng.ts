/** A pure mulberry32 step: same state in, same state and value out. */
export function nextRandom(state: number): { state: number; value: number } {
  const a = (state + 0x6d2b79f5) >>> 0
  let t = a
  t = Math.imul(t ^ (t >>> 15), t | 1)
  t ^= t + Math.imul(t ^ (t >>> 7), t | 61)
  return { state: a, value: ((t ^ (t >>> 14)) >>> 0) / 4294967296 }
}

/** Uniform integer in [lo, hi]. */
export function nextInt(
  state: number,
  lo: number,
  hi: number,
): { state: number; value: number } {
  const rolled = nextRandom(state)
  return { state: rolled.state, value: lo + Math.floor(rolled.value * (hi - lo + 1)) }
}
