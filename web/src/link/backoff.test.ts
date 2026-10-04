import { describe, expect, it } from 'vitest'
import { createBackoff, MAX_BACKOFF_MS, MIN_BACKOFF_MS } from './backoff'

describe('reconnect backoff', () => {
  it('doubles from 250 ms and holds at the 5 s cap', () => {
    const backoff = createBackoff()
    const schedule = Array.from({ length: 8 }, () => backoff.next())

    expect(schedule).toEqual([250, 500, 1000, 2000, 4000, 5000, 5000, 5000])
  })

  it('starts at the minimum and resets after a successful connection', () => {
    expect(MIN_BACKOFF_MS).toBe(250)
    expect(MAX_BACKOFF_MS).toBe(5000)

    const backoff = createBackoff()
    backoff.next()
    backoff.next()
    backoff.reset()

    expect(backoff.next()).toBe(MIN_BACKOFF_MS)
  })
})
