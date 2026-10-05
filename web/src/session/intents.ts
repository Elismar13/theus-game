import type { Input } from '../core/types'
import type { EvtMessage } from '../protocol'

/** The Intent names that travel on the Link (`docs/protocol.md`). */
export type IntentEvent = EvtMessage['e']

/**
 * The Device's Intents as they are currently held, ready to become Game Core
 * input. A Jump is a one-shot, so it is latched until the character lands; a
 * Crawl is held until its `UP`.
 */
export interface IntentBinding {
  readonly jump: boolean
  readonly crawl: boolean
}

export function initialBinding(): IntentBinding {
  return { jump: false, crawl: false }
}

/**
 * Folds one `evt` into the binding. `UP` is the release of a Crawl: a Jump is
 * released by landing, not by `UP`.
 */
export function applyIntent(binding: IntentBinding, event: IntentEvent): IntentBinding {
  switch (event) {
    case 'JUMP':
      return { ...binding, jump: true }
    case 'CRAWL':
      return { ...binding, crawl: true }
    case 'UP':
      return { ...binding, crawl: false }
  }
}

/**
 * Reconciles the Jump latch with the Game Core after a step. The latch is held
 * for the whole arc and released the moment the character is back on the ground,
 * so the next Jump is a fresh rising edge. Only call this once at least one step
 * has run: a grounded frame with no step would otherwise release the latch
 * before the core had consumed it, and a latch the core never consumed — because
 * `prevJump` was still held from the previous arc — would stick forever.
 */
export function releaseJump(binding: IntentBinding, grounded: boolean): IntentBinding {
  return binding.jump && grounded ? { ...binding, jump: false } : binding
}

/** The binding as the Game Core's held input. */
export function bindingInput(binding: IntentBinding): Input {
  return { jump: binding.jump, crawl: binding.crawl }
}
