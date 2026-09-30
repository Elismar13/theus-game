/**
 * Tuning for the Game Core.
 *
 * These numbers are derived from Chromium's offline dino runner (BSD-3), which
 * is tuned for a 600x150 world at 60 fps. Our world is 320x180: narrower, so
 * the horizontal scale is about 320/600 = 0.533. Rather than scale pixels
 * blindly, we lifted rex's *ratios* and re-expressed them in px/s, which is
 * what makes the arc and the pacing survive the change of world size.
 *
 *   rex                          here            why
 *   SPEED             6 px/frame  -> 192 px/s    keeps the 1.67 s screen crossing
 *   MAX_SPEED        13 px/frame  -> 416 px/s    same ratio, halved world
 *   ACCELERATION  0.001 px/frame² ->   2 px/s²   same ~2 min ramp to top speed
 *   GRAVITY         0.6 px/frame² -> 780 px/s²   33.9 px apex, ~0.29 s to it
 *   JUMP_VELOCITY  -10 - speed/10 -> 230 px/s    apex time and height preserved
 *   DROP_VELOCITY    -5 px/frame  -> 110 px/s    releasing Jump cuts the arc
 *   MIN_JUMP_HEIGHT  30 px        ->  16 px      below this a release can't cut
 *   SPEED_DROP_COEFFICIENT 3      ->   3         Crawl mid-air slams you down
 *
 * The obstacles keep rex's two classes and its gap heuristic
 * (`width * speed + base * GAP_COEFFICIENT`, max 1.5x).
 */

export const FPS = 60

export const WORLD = {
  WIDTH: 320,
  HEIGHT: 180,
  /** y of the ground line; feet and obstacle bases rest here. */
  GROUND_Y: 168,
} as const

export const PLAYER = {
  X: 48,
  WIDTH: 20,
  HEIGHT: 24,
  CROUCH_WIDTH: 24,
  CROUCH_HEIGHT: 14,
} as const

export const TUNING = {
  SPEED_START: 192,
  SPEED_MAX: 416,
  ACCELERATION: 2,
  GRAVITY: 780,
  JUMP_VELOCITY: 230,
  DROP_VELOCITY: 110,
  MIN_JUMP_HEIGHT: 16,
  FAST_FALL: 3,
  /** A single step never advances more than this, so a backgrounded tab can't tunnel. */
  MAX_STEP_MS: 50,
  SCORE_COEFFICIENT: 0.025,
  HEARTS: 3,
  INVULN_MS: 1000,
  FLASH_MS: 300,
  /** Grace period before the first obstacle, so a Run starts calm. */
  OBSTACLE_DELAY_MS: 1800,
} as const

export const SPAWN = {
  GAP_COEFFICIENT: 0.6,
  MAX_GAP_COEFFICIENT: 1.5,
  /** Below this speed only low obstacles appear; the opening stretch is all jumps. */
  HIGH_MIN_SPEED: 208,
  /** Share of spawns that are high (crawl) obstacles, once they are unlocked. */
  HIGH_CHANCE: 0.35,
} as const

/** Low obstacles sit on the ground and are cleared by jumping. */
export const LOW_OBSTACLES = [
  { width: 12, height: 16, baseGap: 72 },
  { width: 16, height: 22, baseGap: 72 },
  { width: 20, height: 28, baseGap: 80 },
] as const

/**
 * High obstacles hang from the ceiling down to just above a crouched player,
 * and reach higher than a Jump apex — so they cannot be jumped, only crawled
 * under.
 */
export const HIGH_OBSTACLE = {
  width: 22,
  height: 56,
  baseGap: 104,
  /** Bottom edge, leaving a couple of pixels of clearance over a crouched player. */
  bottom: WORLD.GROUND_Y - PLAYER.CROUCH_HEIGHT - 2,
} as const
