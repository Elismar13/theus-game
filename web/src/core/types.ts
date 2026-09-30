/** Where a Run is, derived from the Game Core's primitive fields. */
export type RunState = 'READY' | 'RUN' | 'JUMP' | 'CRAWL' | 'INVULN' | 'DEAD'

export type ObstacleKind = 'low' | 'high'

export interface Obstacle {
  readonly id: number
  readonly kind: ObstacleKind
  /** Left edge. */
  readonly x: number
  /** Top edge; the base sits at `top + height`. */
  readonly top: number
  readonly width: number
  readonly height: number
  /** Gap that must open behind this obstacle before the next one spawns. */
  readonly gapAfter: number
}

/** The Intents the Game Core consumes, as currently held. */
export interface Input {
  readonly jump: boolean
  readonly crawl: boolean
}

export interface GameState {
  readonly rng: number

  readonly started: boolean
  readonly dead: boolean
  readonly grounded: boolean
  readonly crouching: boolean
  /** Height of the player's feet above the ground line; 0 is grounded. */
  readonly playerY: number
  /** Vertical velocity, px/s, positive is up. */
  readonly playerVy: number
  /** The Jump Intent as it was held on the previous step, for edge detection. */
  readonly prevJump: boolean

  readonly obstacles: readonly Obstacle[]
  readonly nextObstacleId: number

  readonly distance: number
  readonly speed: number
  readonly runMs: number

  readonly score: number
  readonly highScore: number
  readonly hearts: number
  readonly invulnMs: number
  /** Non-zero briefly after a hit, for the renderer to flash the player. */
  readonly flashMs: number
}
