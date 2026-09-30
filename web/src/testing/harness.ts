import { step } from '../core/core'
import type { GameState, Input } from '../core/types'

/** One frame at 60 fps. */
export const FRAME_MS = 1000 / 60

export const IDLE: Input = { jump: false, crawl: false }
export const JUMP: Input = { jump: true, crawl: false }
export const CRAWL: Input = { jump: false, crawl: true }

export function advance(from: GameState, input: Input, frames: number): GameState {
  return advanceBy(from, input, FRAME_MS, frames)
}

export function advanceBy(
  from: GameState,
  input: Input,
  dtMs: number,
  frames: number,
): GameState {
  let state = from
  for (let i = 0; i < frames; i += 1) state = step(state, input, dtMs)
  return state
}
