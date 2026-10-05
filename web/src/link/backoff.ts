export const MIN_BACKOFF_MS = 250
export const MAX_BACKOFF_MS = 5000

export interface Backoff {
  /** The delay for the next attempt, then advance toward the cap. */
  nextDelay(): number
  /** Back to the first delay after a successful connection. */
  reset(): void
}

/**
 * Exponential reconnect backoff: doubling from 250 ms, capped at 5 s, per
 * `docs/protocol.md`. Pure and timer-free so the schedule is testable without a
 * socket; `websocket.ts` owns the scheduling around it.
 */
export function createBackoff(): Backoff {
  let current = MIN_BACKOFF_MS
  return {
    nextDelay(): number {
      const delay = current
      current = Math.min(current * 2, MAX_BACKOFF_MS)
      return delay
    },
    reset(): void {
      current = MIN_BACKOFF_MS
    },
  }
}
