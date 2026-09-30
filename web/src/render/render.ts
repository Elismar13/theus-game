import { PLAYER, TUNING, WORLD } from '../core/constants'
import { runState } from '../core/core'
import type { GameState, Obstacle } from '../core/types'
import { PALETTE } from './palette'

type Ctx = CanvasRenderingContext2D

function px(ctx: Ctx, x: number, y: number, w: number, h: number, color: string): void {
  ctx.fillStyle = color
  ctx.fillRect(Math.round(x), Math.round(y), Math.round(w), Math.round(h))
}

function drawSky(ctx: Ctx): void {
  const sky = ctx.createLinearGradient(0, 0, 0, WORLD.GROUND_Y)
  sky.addColorStop(0, PALETTE.skyTop)
  sky.addColorStop(1, PALETTE.skyBottom)
  ctx.fillStyle = sky
  ctx.fillRect(0, 0, WORLD.WIDTH, WORLD.GROUND_Y)
}

/** Two bands of hills, the far one drifting slower, both wrapping seamlessly. */
function drawHills(ctx: Ctx, distance: number): void {
  const bands = [
    { period: 150, height: 44, speed: 0.12, color: PALETTE.hillFar },
    { period: 90, height: 28, speed: 0.24, color: PALETTE.hillNear },
  ]
  for (const band of bands) {
    const offset = (distance * band.speed) % band.period
    for (let i = -1; i * band.period - offset < WORLD.WIDTH + band.period; i += 1) {
      const base = i * band.period - offset
      ctx.fillStyle = band.color
      ctx.beginPath()
      ctx.moveTo(base, WORLD.GROUND_Y)
      ctx.lineTo(base + band.period / 2, WORLD.GROUND_Y - band.height)
      ctx.lineTo(base + band.period, WORLD.GROUND_Y)
      ctx.closePath()
      ctx.fill()
    }
  }
}

function drawGround(ctx: Ctx, distance: number): void {
  px(ctx, 0, WORLD.GROUND_Y, WORLD.WIDTH, WORLD.HEIGHT - WORLD.GROUND_Y, PALETTE.ground)
  px(ctx, 0, WORLD.GROUND_Y, WORLD.WIDTH, 2, PALETTE.groundGrass)
  const offset = distance % 16
  for (let x = -offset; x < WORLD.WIDTH; x += 16) {
    px(ctx, x, WORLD.GROUND_Y + 6, 6, 2, PALETTE.groundDash)
  }
}

function drawCactus(ctx: Ctx, obstacle: Obstacle): void {
  const x = Math.round(obstacle.x)
  const top = Math.round(obstacle.top)
  px(ctx, x, top, obstacle.width, obstacle.height, PALETTE.cactus)
  px(ctx, x, top, 2, obstacle.height, PALETTE.cactusDark)
  px(ctx, x + obstacle.width - 2, top + 2, 2, obstacle.height - 4, PALETTE.cactusDark)
  const armY = top + Math.round(obstacle.height * 0.35)
  px(ctx, x - 4, armY, 4, 3, PALETTE.cactus)
  px(ctx, x - 4, armY, 2, 8, PALETTE.cactus)
  const armY2 = top + Math.round(obstacle.height * 0.55)
  px(ctx, x + obstacle.width, armY2, 4, 3, PALETTE.cactus)
  px(ctx, x + obstacle.width + 2, armY2, 2, 7, PALETTE.cactus)
}

function drawVine(ctx: Ctx, obstacle: Obstacle): void {
  const x = Math.round(obstacle.x)
  const top = Math.round(obstacle.top)
  px(ctx, x, top, obstacle.width, obstacle.height, PALETTE.vine)
  px(ctx, x, top, 2, obstacle.height, PALETTE.vineDark)
  px(ctx, x + obstacle.width - 2, top, 2, obstacle.height, PALETTE.vineDark)
  for (let y = top + 6; y < top + obstacle.height - 6; y += 12) {
    px(ctx, x - 5, y, 5, 3, PALETTE.leaf)
    px(ctx, x + obstacle.width, y + 6, 5, 3, PALETTE.leaf)
  }
  px(ctx, x, top + obstacle.height - 4, obstacle.width, 4, PALETTE.vineDark)
}

function drawStanding(ctx: Ctx, state: GameState, feet: number, airborne: boolean): void {
  const x = PLAYER.X
  const w = PLAYER.WIDTH
  const h = PLAYER.HEIGHT
  const top = feet - h
  const lift = Math.floor(state.distance / 6) % 2 === 0 ? 0 : 2

  if (airborne) {
    px(ctx, x + 3, top + h - 4, 6, 4, PALETTE.playerDark)
    px(ctx, x + 11, top + h - 4, 6, 4, PALETTE.playerDark)
  } else {
    px(ctx, x + 3, top + h - 5 - lift, 5, 5, PALETTE.playerDark)
    px(ctx, x + 11, top + h - 5 - (2 - lift), 5, 5, PALETTE.playerDark)
  }

  px(ctx, x - 5, top + 10, 6, 4, PALETTE.player) // tail
  px(ctx, x + 1, top + 7, w - 2, h - 12, PALETTE.player) // body
  px(ctx, x + 1, top + h - 7, w - 2, 2, PALETTE.playerLight)
  px(ctx, x + 3, top, w - 5, 8, PALETTE.player) // head
  px(ctx, x + 3, top, 4, 3, PALETTE.playerLight)
  px(ctx, x + w - 4, top + 4, 5, 4, PALETTE.player) // snout
  px(ctx, x + w - 5, top + 2, 2, 2, PALETTE.eye)
  px(ctx, x - 7, top + 8, 9, 2, PALETTE.scarf) // scarf trailing behind
  px(ctx, x + 1, top + 8, w - 4, 3, PALETTE.scarf)
}

function drawCrouched(ctx: Ctx, feet: number): void {
  const x = PLAYER.X
  const w = PLAYER.CROUCH_WIDTH
  const h = PLAYER.CROUCH_HEIGHT
  const top = feet - h

  px(ctx, x, top + h - 3, w, 3, PALETTE.playerDark) // legs tucked
  px(ctx, x + 1, top + 4, w - 3, h - 6, PALETTE.player) // low body
  px(ctx, x + 2, top + h - 5, w - 7, 2, PALETTE.playerLight)
  px(ctx, x + w - 9, top, 8, 8, PALETTE.player) // head pushed forward
  px(ctx, x + w - 8, top, 3, 3, PALETTE.playerLight)
  px(ctx, x + w - 3, top + 3, 3, 3, PALETTE.eye)
  px(ctx, x - 5, top + 5, 7, 2, PALETTE.scarf)
  px(ctx, x + 1, top + 5, w - 10, 3, PALETTE.scarf)
}

/** The hit flash blinks the player on and off; this is one blink phase, in ms. */
const FLASH_BLINK_MS = 60

function drawPlayer(ctx: Ctx, state: GameState): void {
  // Blink on and off for the length of a hit flash, so a hit reads as a hit.
  if (state.flashMs > 0 && Math.floor(state.flashMs / FLASH_BLINK_MS) % 2 === 0) return

  // The pose follows the body, not the Run State: an invulnerable player is
  // still crawling or airborne, and has to be drawn that way.
  const feet = WORLD.GROUND_Y - Math.round(state.playerY)
  if (state.crouching && state.grounded) {
    drawCrouched(ctx, feet)
  } else {
    drawStanding(ctx, state, feet, !state.grounded)
  }
}

const HEART = ['01110', '11111', '11111', '01110', '00100'] as const

function drawHeart(ctx: Ctx, x: number, y: number, color: string): void {
  for (let row = 0; row < HEART.length; row += 1) {
    const bits = HEART[row] ?? ''
    for (let col = 0; col < bits.length; col += 1) {
      if (bits[col] === '1') px(ctx, x + col, y + row, 1, 1, color)
    }
  }
}

function drawHud(ctx: Ctx, state: GameState): void {
  ctx.textBaseline = 'top'

  for (let i = 0; i < TUNING.HEARTS; i += 1) {
    drawHeart(ctx, 6 + i * 9, 6, i < state.hearts ? PALETTE.heart : PALETTE.heartEmpty)
  }

  ctx.textAlign = 'right'
  ctx.font = 'bold 10px ui-monospace, SFMono-Regular, Menlo, monospace'
  ctx.fillStyle = PALETTE.text
  ctx.fillText(String(state.score).padStart(5, '0'), WORLD.WIDTH - 6, 5)

  ctx.font = '8px ui-monospace, SFMono-Regular, Menlo, monospace'
  ctx.fillStyle = PALETTE.textDim
  ctx.fillText(`HI ${String(state.highScore).padStart(5, '0')}`, WORLD.WIDTH - 6, 17)
}

function centreText(ctx: Ctx, text: string, y: number, font: string, color: string): void {
  ctx.textAlign = 'center'
  ctx.font = font
  ctx.fillStyle = color
  ctx.fillText(text, WORLD.WIDTH / 2, y)
}

function drawOverlay(ctx: Ctx, state: GameState): void {
  const phase = runState(state)
  if (phase !== 'READY' && phase !== 'DEAD') return

  ctx.fillStyle = PALETTE.overlay
  ctx.fillRect(0, 0, WORLD.WIDTH, WORLD.HEIGHT)

  if (phase === 'READY') {
    centreText(ctx, 'THEUS GAME', 56, 'bold 16px ui-monospace, monospace', PALETTE.text)
    centreText(ctx, 'JUMP OR PRESS SPACE', 84, '10px ui-monospace, monospace', PALETTE.textDim)
    centreText(ctx, 'JUMP OVER LOW · CRAWL UNDER HIGH', 100, '8px ui-monospace, monospace', PALETTE.textDim)
    return
  }

  centreText(ctx, 'GAME OVER', 52, 'bold 16px ui-monospace, monospace', PALETTE.text)
  centreText(ctx, `SCORE ${state.score}`, 78, '10px ui-monospace, monospace', PALETTE.text)
  centreText(ctx, `BEST ${state.highScore}`, 92, '10px ui-monospace, monospace', PALETTE.textDim)
  centreText(ctx, 'ENTER TO RUN AGAIN', 114, '9px ui-monospace, monospace', PALETTE.textDim)
}

export function draw(ctx: Ctx, state: GameState): void {
  ctx.imageSmoothingEnabled = false
  drawSky(ctx)
  drawHills(ctx, state.distance)
  drawGround(ctx, state.distance)
  for (const obstacle of state.obstacles) {
    if (obstacle.kind === 'high') drawVine(ctx, obstacle)
    else drawCactus(ctx, obstacle)
  }
  drawPlayer(ctx, state)
  drawHud(ctx, state)
  drawOverlay(ctx, state)
}
