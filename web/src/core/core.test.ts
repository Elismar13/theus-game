import { describe, expect, it } from 'vitest'
import { advance, advanceBy, CRAWL, FRAME_MS, IDLE, JUMP } from '../testing/harness'
import { HIGH_OBSTACLE, PLAYER, TUNING, WORLD } from './constants'
import { initial, runState, setGodMode, step } from './core'
import type { GameState, Obstacle, ObstacleKind } from './types'

/** A Run already on the ground and moving. */
function running(seed = 1): GameState {
  return advance(step(initial(seed), JUMP, FRAME_MS), IDLE, 40)
}

/** An obstacle parked wherever the test needs it. */
function obstacleAt(kind: ObstacleKind, x: number): Obstacle {
  if (kind === 'high') {
    return {
      id: 900,
      kind,
      x,
      top: HIGH_OBSTACLE.bottom - HIGH_OBSTACLE.height,
      width: HIGH_OBSTACLE.width,
      height: HIGH_OBSTACLE.height,
      gapAfter: 9999,
    }
  }
  return {
    id: 901,
    kind,
    x,
    top: WORLD.GROUND_Y - 20,
    width: 16,
    height: 20,
    gapAfter: 9999,
  }
}

function withObstacle(state: GameState, obstacle: Obstacle): GameState {
  return { ...state, obstacles: [obstacle] }
}

function runUntilHit(
  from: GameState,
  maxFrames = 6000,
): { before: GameState; after: GameState } {
  let state = from
  for (let i = 0; i < maxFrames; i += 1) {
    const next = step(state, IDLE, FRAME_MS)
    if (next.hearts < state.hearts) return { before: state, after: next }
    state = next
  }
  throw new Error('the player never hit an obstacle')
}

/** A Run that cannot die, so spawn behaviour can be watched over a long horizon. */
function survivor(seed: number): GameState {
  return { ...running(seed), hearts: 99 }
}

/** Every obstacle this seed spawns, in order, over the next 15 s. */
function obstacleLog(seed: number, frames = 900): string {
  let state = survivor(seed)
  const log: string[] = []
  let lastId = 0
  for (let i = 0; i < frames; i += 1) {
    state = step(state, IDLE, FRAME_MS)
    for (const obstacle of state.obstacles) {
      if (obstacle.id > lastId) {
        lastId = obstacle.id
        log.push(
          `${obstacle.id}|${obstacle.kind}|${obstacle.width}x${obstacle.height}|${obstacle.gapAfter}`,
        )
      }
    }
  }
  return log.join('\n')
}

function everHigh(from: GameState, frames: number): boolean {
  let state = from
  for (let i = 0; i < frames; i += 1) {
    state = step(state, IDLE, FRAME_MS)
    if (state.obstacles.some((obstacle) => obstacle.kind === 'high')) return true
  }
  return false
}

describe('Run State', () => {
  it('waits in READY until the first Jump', () => {
    const state = advance(initial(1), IDLE, 120)
    expect(runState(state)).toBe('READY')
    expect(state.distance).toBe(0)
    expect(state.score).toBe(0)
  })

  it('starts on the first Jump', () => {
    expect(runState(step(initial(1), JUMP, FRAME_MS))).toBe('JUMP')
  })

  it('lands back into RUN', () => {
    const landed = advance(step(initial(1), JUMP, FRAME_MS), IDLE, 60)
    expect(runState(landed)).toBe('RUN')
    expect(landed.grounded).toBe(true)
    expect(landed.playerY).toBe(0)
  })

  it('crawls while the Crawl Intent is held on the ground', () => {
    expect(runState(advance(running(), CRAWL, 1))).toBe('CRAWL')
  })

  it('stands back up when the Crawl Intent is released', () => {
    expect(runState(advance(advance(running(), CRAWL, 5), IDLE, 1))).toBe('RUN')
  })

  it('will not crawl in mid-air', () => {
    const airborne = step(initial(1), JUMP, FRAME_MS)
    expect(runState(step(airborne, CRAWL, FRAME_MS))).toBe('JUMP')
  })
})

describe('Jumping', () => {
  it('rises then falls back to the ground', () => {
    const airborne = step(initial(1), JUMP, FRAME_MS)
    const rising = advance(airborne, JUMP, 10)
    const falling = advance(airborne, JUMP, 30)
    expect(rising.playerY).toBeGreaterThan(0)
    expect(falling.playerY).toBeLessThan(rising.playerY)
    expect(falling.playerVy).toBeLessThan(0)
  })

  it('keeps its apex and arc independent of frame rate', () => {
    // Both runs advance the same 300 ms of game time, in different step sizes.
    const fine = advanceBy(step(initial(1), JUMP, 1), JUMP, 1, 300)
    const coarse = advanceBy(step(initial(1), JUMP, 1), JUMP, 4, 75)
    expect(Math.abs(fine.playerY - coarse.playerY)).toBeLessThan(2)
  })

  it('goes higher, and stays up longer, when held than when released early', () => {
    const airborne = step(initial(1), JUMP, FRAME_MS)
    const held = advance(airborne, JUMP, 17)
    const cut = advance(airborne, IDLE, 17)
    expect(held.playerY).toBeGreaterThan(cut.playerY + 5)
  })

  it('does not jump again in mid-air', () => {
    const airborne = step(initial(1), JUMP, FRAME_MS)
    const released = step(airborne, IDLE, FRAME_MS)
    const pressedAgain = step(released, JUMP, FRAME_MS)
    expect(pressedAgain.playerVy).toBeLessThan(released.playerVy)
    expect(pressedAgain.grounded).toBe(false)
  })
})

describe('Hitboxes', () => {
  it('hits a standing player with a high obstacle', () => {
    const after = step(
      withObstacle(running(), obstacleAt('high', PLAYER.X - 4)),
      IDLE,
      FRAME_MS,
    )
    expect(after.hearts).toBe(TUNING.HEARTS - 1)
  })

  it('lets a crouched player pass under that same high obstacle', () => {
    const crouched = advance(running(), CRAWL, 1)
    const after = step(
      withObstacle(crouched, obstacleAt('high', PLAYER.X - 4)),
      CRAWL,
      FRAME_MS,
    )
    expect(after.hearts).toBe(TUNING.HEARTS)
  })

  it('cannot be jumped over a high obstacle', () => {
    const airborne = advance(step(initial(1), JUMP, FRAME_MS), JUMP, 16)
    const after = step(
      withObstacle(airborne, obstacleAt('high', PLAYER.X - 4)),
      JUMP,
      FRAME_MS,
    )
    expect(after.hearts).toBe(TUNING.HEARTS - 1)
  })

  it('still hits a crouched player with a low obstacle', () => {
    const crouched = advance(running(), CRAWL, 1)
    const after = step(
      withObstacle(crouched, obstacleAt('low', PLAYER.X - 4)),
      CRAWL,
      FRAME_MS,
    )
    expect(after.hearts).toBe(TUNING.HEARTS - 1)
  })

  it('clears a low obstacle while airborne', () => {
    const airborne = advance(step(initial(1), JUMP, FRAME_MS), JUMP, 16)
    const after = step(
      withObstacle(airborne, obstacleAt('low', PLAYER.X - 4)),
      JUMP,
      FRAME_MS,
    )
    expect(after.hearts).toBe(TUNING.HEARTS)
  })
})

describe('Hearts and invulnerability', () => {
  it('costs a Heart and grants invulnerability on a hit', () => {
    const { after } = runUntilHit(running(7))
    expect(after.hearts).toBe(TUNING.HEARTS - 1)
    expect(after.invulnMs).toBeGreaterThan(0)
    expect(runState(after)).toBe('INVULN')
  })

  it('ignores further collisions until invulnerability expires', () => {
    let state = runUntilHit(running(7)).after
    while (state.invulnMs > 0) {
      state = step(state, IDLE, FRAME_MS)
      expect(state.hearts).toBe(TUNING.HEARTS - 1)
    }
    expect(state.dead).toBe(false)
  })

  it('ends the Run on the third hit', () => {
    let state = running(7)
    for (let hit = 0; hit < TUNING.HEARTS; hit += 1) {
      state = runUntilHit(state).after
      while (state.invulnMs > 0 && !state.dead) state = step(state, IDLE, FRAME_MS)
    }
    expect(state.hearts).toBe(0)
    expect(runState(state)).toBe('DEAD')
  })

  it('freezes once dead', () => {
    let state = running(7)
    for (let hit = 0; hit < TUNING.HEARTS; hit += 1) {
      state = runUntilHit(state).after
      while (state.invulnMs > 0 && !state.dead) state = step(state, IDLE, FRAME_MS)
    }
    expect(step(state, JUMP, FRAME_MS)).toBe(state)
  })
})

describe('God mode', () => {
  /** A hit that would normally cost a Heart, on a Run in god mode. */
  function hitInGodMode(overrides: Partial<GameState> = {}): GameState {
    return step(
      withObstacle({ ...running(), godMode: true, ...overrides }, obstacleAt('low', PLAYER.X - 4)),
      IDLE,
      FRAME_MS,
    )
  }

  it('survives a hit without losing a Heart or dying', () => {
    const after = hitInGodMode()
    expect(after.hearts).toBe(TUNING.HEARTS)
    expect(after.dead).toBe(false)
    expect(runState(after)).not.toBe('DEAD')
  })

  it('still flashes and grants invulnerability, so a mistimed Intent is visible', () => {
    const after = hitInGodMode()
    expect(after.flashMs).toBeGreaterThan(0)
    expect(after.invulnMs).toBeGreaterThan(0)
  })

  it('cannot die on the last Heart', () => {
    const after = hitInGodMode({ hearts: 1 })
    expect(after.hearts).toBe(1)
    expect(after.dead).toBe(false)
  })

  it('never ends the Run, however long it runs into obstacles', () => {
    let state: GameState = { ...running(7), godMode: true }
    for (let i = 0; i < 20_000; i += 1) state = step(state, IDLE, FRAME_MS)
    expect(state.dead).toBe(false)
    expect(state.hearts).toBe(TUNING.HEARTS)
    expect(runState(state)).not.toBe('DEAD')
  })

  it('is off unless a Run asks for it', () => {
    expect(initial(1).godMode).toBe(false)
    expect(initial(1, 0, true).godMode).toBe(true)
  })

  it('does not bank a High Score from a debug Run', () => {
    let state: GameState = { ...running(1), godMode: true, highScore: 5 }
    for (let i = 0; i < 600; i += 1) state = step(state, IDLE, FRAME_MS)
    // Score still climbs as feedback, but a test Run writes nothing to the record.
    expect(state.score).toBeGreaterThan(5)
    expect(state.highScore).toBe(5)
  })

  it('toggles a live Run', () => {
    const on = setGodMode(running(7), true)
    expect(on.godMode).toBe(true)
    expect(setGodMode(on, false).godMode).toBe(false)
  })
})

describe('Score and speed', () => {
  it('scores by distance, and keeps climbing', () => {
    const early = advance(running(1), IDLE, 60)
    const later = advance(early, IDLE, 60)
    expect(early.score).toBeGreaterThan(0)
    expect(later.score).toBeGreaterThan(early.score)
  })

  it('ramps up to a top speed and stops there', () => {
    const state = advance(survivor(1), IDLE, 60 * 200)
    expect(state.speed).toBe(TUNING.SPEED_MAX)
  })

  it('carries the High Score into the next Run', () => {
    let state = running(7)
    for (let hit = 0; hit < TUNING.HEARTS; hit += 1) {
      state = runUntilHit(state).after
      while (state.invulnMs > 0 && !state.dead) state = step(state, IDLE, FRAME_MS)
    }
    expect(state.highScore).toBeGreaterThan(0)

    const next = initial(99, state.highScore)
    expect(runState(next)).toBe('READY')
    expect(next.highScore).toBe(state.highScore)
    expect(next.hearts).toBe(TUNING.HEARTS)
  })
})

describe('Spawning', () => {
  it('holds obstacles back until the opening delay has passed', () => {
    const early = advance(running(1), IDLE, 60) // ~1.7 s of Run time
    expect(early.runMs).toBeLessThan(TUNING.OBSTACLE_DELAY_MS)
    expect(early.obstacles).toHaveLength(0)
  })

  it('starts spawning once the opening delay has passed', () => {
    expect(advance(running(1), IDLE, 120).obstacles.length).toBeGreaterThan(0)
  })

  it('forgets obstacles that have left the world', () => {
    const gone = withObstacle(running(1), obstacleAt('low', -100))
    expect(advance(gone, IDLE, 1).obstacles).toHaveLength(0)
  })

  it('produces an identical Run for the same seed, and a different one otherwise', () => {
    // Whole states are compared, not just the spawn log, so hidden
    // nondeterminism (a stray Math.random) fails rather than passing by
    // reflexivity.
    expect(advance(survivor(7), IDLE, 400)).toEqual(advance(survivor(7), IDLE, 400))
    expect(advance(survivor(7), IDLE, 400)).not.toEqual(advance(survivor(8), IDLE, 400))
    expect(obstacleLog(7).length).toBeGreaterThan(3)
  })

  it('holds high obstacles back until the Run is fast enough', () => {
    expect(everHigh(survivor(3), 300)).toBe(false)
    expect(everHigh(survivor(3), 1500)).toBe(true)
  })
})
