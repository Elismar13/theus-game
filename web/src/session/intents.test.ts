import { describe, expect, it } from 'vitest'
import { initial as initialState, step } from '../core/core'
import type { GameState } from '../core/types'
import {
  applyIntent,
  bindingInput,
  type IntentBinding,
  initialBinding,
  releaseJump,
} from './intents'

const STEP_MS = 1000 / 60

/**
 * One frame exactly as `main.ts` runs it: apply any Intent that arrived, hold
 * the binding as input for a step, then reconcile the Jump latch with the new
 * grounded flag.
 */
function frame(
  state: GameState,
  binding: IntentBinding,
  event?: 'JUMP' | 'CRAWL' | 'UP',
): [GameState, IntentBinding] {
  const withIntent = event === undefined ? binding : applyIntent(binding, event)
  const next = step(state, bindingInput(withIntent), STEP_MS)
  return [next, releaseJump(withIntent, next.grounded)]
}

describe('IntentBinding', () => {
  it('starts neutral', () => {
    expect(initialBinding()).toEqual({ jump: false, crawl: false })
    expect(bindingInput(initialBinding())).toEqual({ jump: false, crawl: false })
  })

  it('latches a Crawl until UP', () => {
    let binding = applyIntent(initialBinding(), 'CRAWL')
    expect(bindingInput(binding).crawl).toBe(true)

    binding = applyIntent(binding, 'UP')
    expect(bindingInput(binding).crawl).toBe(false)
  })

  it('leaves a held Jump alone when a Crawl releases', () => {
    let binding = initialBinding()
    binding = applyIntent(binding, 'JUMP')
    binding = applyIntent(binding, 'CRAWL')
    binding = applyIntent(binding, 'UP')
    expect(bindingInput(binding)).toEqual({ jump: true, crawl: false })
  })

  it('releases the latch only once grounded', () => {
    const latched = applyIntent(initialBinding(), 'JUMP')
    // Airborne: the contract is to leave the latch alone.
    expect(releaseJump(latched, false)).toEqual(latched)
    // Grounded after a step: release it.
    expect(bindingInput(releaseJump(latched, true)).jump).toBe(false)
  })
})

describe('IntentBinding with the Game Core', () => {
  it('holds the Jump latch for the full arc and releases it on landing', () => {
    let state = initialState(1)
    let binding = initialBinding()

    ;[state, binding] = frame(state, binding, 'JUMP')
    expect(state.grounded).toBe(false)
    // Airborne: still held.
    ;[state, binding] = frame(state, binding)
    expect(state.grounded).toBe(false)
    expect(bindingInput(binding).jump).toBe(true)

    let guard = 0
    while (!state.grounded && guard < 240) {
      ;[state, binding] = frame(state, binding)
      guard += 1
    }
    expect(state.grounded).toBe(true)
    expect(bindingInput(binding).jump).toBe(false)
  })

  it('does not let a Jump swallowed at touchdown stick forever', () => {
    let state = initialState(1)
    let binding = initialBinding()

    ;[state, binding] = frame(state, binding, 'JUMP')
    let guard = 0
    while (!state.grounded && guard < 240) {
      ;[state, binding] = frame(state, binding)
      guard += 1
    }
    expect(state.grounded).toBe(true)

    // A JUMP arriving in the one-step gap after touchdown is swallowed by the
    // core (its prevJump is still held), but it must not wedge the latch.
    ;[state, binding] = frame(state, binding, 'JUMP')
    expect(bindingInput(binding).jump).toBe(false)

    // A clean frame lets prevJump fall; the next Jump must leave the ground.
    ;[state, binding] = frame(state, binding)
    ;[state, binding] = frame(state, binding, 'JUMP')
    expect(state.grounded).toBe(false)
  })
})
