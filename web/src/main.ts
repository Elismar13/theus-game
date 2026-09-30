import './style.css'
import { WORLD } from './core/constants'
import { initial, step } from './core/core'
import type { GameState } from './core/types'
import { createKeyboard } from './input/keyboard'
import { draw } from './render/render'

const STEP_MS = 1000 / 60
const RESTART_GRACE_MS = 750
const HIGH_SCORE_KEY = 'theus-game.high-score'
const MAX_FRAME_MS = 250

const canvasEl = document.getElementById('game')
if (!(canvasEl instanceof HTMLCanvasElement)) throw new Error('#game canvas is missing')
const canvas: HTMLCanvasElement = canvasEl

const context = canvas.getContext('2d')
if (context === null) throw new Error('the 2d canvas context is unavailable')
const ctx: CanvasRenderingContext2D = context
ctx.imageSmoothingEnabled = false

const keyboard = createKeyboard()

function readHighScore(): number {
  const raw = window.localStorage.getItem(HIGH_SCORE_KEY)
  const value = raw === null ? 0 : Number.parseInt(raw, 10)
  return Number.isFinite(value) && value > 0 ? value : 0
}

/** Integer scaling only — a fractional scale would blur the pixel art. */
function fit(): void {
  const scale = Math.max(
    1,
    Math.floor(
      Math.min(
        window.innerWidth / WORLD.WIDTH,
        (window.innerHeight - 64) / WORLD.HEIGHT,
      ),
    ),
  )
  canvas.style.width = `${WORLD.WIDTH * scale}px`
  canvas.style.height = `${WORLD.HEIGHT * scale}px`
}

let seed = (Date.now() ^ Math.floor(Math.random() * 0xffff)) >>> 0
let state: GameState = initial(seed, readHighScore())
let accumulator = 0
let previous = performance.now()
let diedAt: number | null = null

function restart(): void {
  seed = (seed + 1) >>> 0
  state = initial(seed, state.highScore)
  diedAt = null
  accumulator = 0
}

function frame(now: number): void {
  accumulator += Math.min(now - previous, MAX_FRAME_MS)
  previous = now

  if (state.dead) {
    if (diedAt === null) {
      diedAt = now
      window.localStorage.setItem(HIGH_SCORE_KEY, String(state.highScore))
    } else if (now - diedAt >= RESTART_GRACE_MS && keyboard.takeRestart()) {
      // The grace period stops a held Jump key from restarting the instant you
      // die. Restart is only consumed once it has passed, so a press during the
      // grace is remembered instead of being swallowed.
      restart()
    }
  } else {
    // A stray Enter mid-Run is not a restart.
    keyboard.takeRestart()
  }

  // A fixed timestep keeps the simulation independent of the display rate.
  const input = keyboard.input()
  while (accumulator >= STEP_MS) {
    state = step(state, input, STEP_MS)
    accumulator -= STEP_MS
  }

  draw(ctx, state)
  requestAnimationFrame(frame)
}

window.addEventListener('resize', fit)
fit()
requestAnimationFrame(frame)
