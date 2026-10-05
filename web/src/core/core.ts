import {
  FPS,
  HIGH_OBSTACLE,
  LOW_OBSTACLES,
  PLAYER,
  SPAWN,
  TUNING,
  WORLD,
} from './constants'
import { nextInt, nextRandom } from './rng'
import type { GameState, Input, Obstacle, ObstacleKind, RunState } from './types'

export function initial(seed: number, highScore = 0, godMode = false): GameState {
  return {
    rng: (seed >>> 0) || 1,
    started: false,
    dead: false,
    grounded: true,
    crouching: false,
    playerY: 0,
    playerVy: 0,
    prevJump: false,
    obstacles: [],
    nextObstacleId: 1,
    distance: 0,
    speed: TUNING.SPEED_START,
    runMs: 0,
    score: 0,
    highScore,
    hearts: TUNING.HEARTS,
    godMode,
    invulnMs: 0,
    flashMs: 0,
  }
}

/** Turns the debug god mode on or off on a live Run. */
export function setGodMode(state: GameState, on: boolean): GameState {
  return { ...state, godMode: on }
}

/**
 * Where the Run is, derived rather than stored, so it can never drift out of
 * step with the primitives it is computed from.
 */
export function runState(state: GameState): RunState {
  if (state.dead) return 'DEAD'
  if (!state.started) return 'READY'
  if (state.invulnMs > 0) return 'INVULN'
  if (!state.grounded) return 'JUMP'
  if (state.crouching) return 'CRAWL'
  return 'RUN'
}

interface Box {
  x: number
  y: number
  width: number
  height: number
}

/** The player's hitbox: shorter and wider when crouched. */
function playerBoxIn(state: GameState, playerY: number): Box {
  const crouched = state.crouching && state.grounded
  const width = crouched ? PLAYER.CROUCH_WIDTH : PLAYER.WIDTH
  const height = crouched ? PLAYER.CROUCH_HEIGHT : PLAYER.HEIGHT
  return { x: PLAYER.X, y: WORLD.GROUND_Y - playerY - height, width, height }
}

function overlaps(a: Box, b: Box): boolean {
  return (
    a.x < b.x + b.width &&
    a.x + a.width > b.x &&
    a.y < b.y + b.height &&
    a.y + a.height > b.y
  )
}

/**
 * Obstacles enter from the right edge, and the next one is held back until a
 * gap has opened behind the last — rex's rule, with its gap heuristic
 * `width * speed + base * GAP_COEFFICIENT`, capped at 1.5x.
 */
function spawnObstacle(
  obstacles: readonly Obstacle[],
  rng: number,
  nextId: number,
  runMs: number,
  speed: number,
): { obstacles: Obstacle[]; rng: number; nextId: number } {
  const list = [...obstacles]
  if (runMs < TUNING.OBSTACLE_DELAY_MS) return { obstacles: list, rng, nextId }

  const tail = list[list.length - 1]
  if (tail && tail.x + tail.width + tail.gapAfter >= WORLD.WIDTH) {
    return { obstacles: list, rng, nextId }
  }

  const kindRoll = nextRandom(rng)
  rng = kindRoll.state
  const high = speed >= SPAWN.HIGH_MIN_SPEED && kindRoll.value < SPAWN.HIGH_CHANCE

  let kind: ObstacleKind
  let width: number
  let height: number
  let baseGap: number

  if (high) {
    kind = 'high'
    width = HIGH_OBSTACLE.width
    height = HIGH_OBSTACLE.height
    baseGap = HIGH_OBSTACLE.baseGap
  } else {
    const pick = nextInt(rng, 0, LOW_OBSTACLES.length - 1)
    rng = pick.state
    const variant = LOW_OBSTACLES[pick.value] ?? LOW_OBSTACLES[0]
    kind = 'low'
    width = variant.width
    height = variant.height
    baseGap = variant.baseGap
  }

  const minGap = Math.round(width * (speed / FPS) + baseGap * SPAWN.GAP_COEFFICIENT)
  const maxGap = Math.round(minGap * SPAWN.MAX_GAP_COEFFICIENT)
  const gap = nextInt(rng, minGap, maxGap)
  rng = gap.state

  const top =
    kind === 'high' ? HIGH_OBSTACLE.bottom - height : WORLD.GROUND_Y - height

  list.push({ id: nextId, kind, x: WORLD.WIDTH, top, width, height, gapAfter: gap.value })
  return { obstacles: list, rng, nextId: nextId + 1 }
}

export function step(state: GameState, input: Input, dtMs: number): GameState {
  if (state.dead) return state

  const clampedMs = Math.min(Math.max(dtMs, 0), TUNING.MAX_STEP_MS)
  const dt = clampedMs / 1000
  const jumpPressed = input.jump && !state.prevJump

  // READY only wakes on the first Jump Intent.
  if (!state.started && !jumpPressed) {
    return { ...state, prevJump: input.jump }
  }

  const runMs = state.runMs + clampedMs
  const speed = Math.min(TUNING.SPEED_MAX, state.speed + TUNING.ACCELERATION * dt)
  const distance = state.distance + speed * dt

  let playerY = state.playerY
  let playerVy = state.playerVy
  let grounded = state.grounded
  let crouching = state.crouching

  // A Jump only happens from the ground, and it wins over a Crawl.
  if (jumpPressed && grounded) {
    playerVy = TUNING.JUMP_VELOCITY
    grounded = false
    crouching = false
    playerY = 0
  }

  if (grounded) {
    crouching = input.crawl
  } else {
    // Releasing Jump cuts the arc, but not before the minimum height is reached.
    if (
      !input.jump &&
      playerVy > TUNING.DROP_VELOCITY &&
      playerY > TUNING.MIN_JUMP_HEIGHT
    ) {
      playerVy = TUNING.DROP_VELOCITY
    }
    // Crawl in mid-air is a fast fall.
    const gravity = TUNING.GRAVITY * (input.crawl ? TUNING.FAST_FALL : 1)
    playerVy -= gravity * dt
    playerY += playerVy * dt
    if (playerY <= 0) {
      playerY = 0
      playerVy = 0
      grounded = true
    }
  }

  const moved = state.obstacles
    .map((obstacle) => ({ ...obstacle, x: obstacle.x - speed * dt }))
    .filter((obstacle) => obstacle.x + obstacle.width > 0)
  const spawned = spawnObstacle(moved, state.rng, state.nextObstacleId, runMs, speed)

  const box = playerBoxIn({ ...state, crouching, grounded }, playerY)
  const hit =
    state.invulnMs <= 0 &&
    spawned.obstacles.some((obstacle) =>
      overlaps(box, {
        x: obstacle.x,
        y: obstacle.top,
        width: obstacle.width,
        height: obstacle.height,
      }),
    )

  let hearts = state.hearts
  let dead: boolean = state.dead
  let invulnMs = Math.max(0, state.invulnMs - clampedMs)
  let flashMs = Math.max(0, state.flashMs - clampedMs)

  if (hit) {
    // In god mode a hit is still shown (flash + invulnerability) so a mistimed
    // Intent reads clearly, but it costs no Heart and never ends the Run.
    if (!state.godMode) {
      hearts -= 1
      if (hearts <= 0) {
        hearts = 0
        dead = true
      }
    }
    invulnMs = TUNING.INVULN_MS
    flashMs = TUNING.FLASH_MS
  }

  const score = Math.floor(distance * TUNING.SCORE_COEFFICIENT)

  return {
    ...state,
    rng: spawned.rng,
    nextObstacleId: spawned.nextId,
    obstacles: spawned.obstacles,
    started: true,
    dead,
    grounded,
    crouching,
    playerY,
    playerVy,
    prevJump: input.jump,
    distance,
    speed,
    runMs,
    score,
    highScore: state.godMode ? state.highScore : Math.max(state.highScore, score),
    hearts,
    invulnMs,
    flashMs,
  }
}
