import './style.css'
import { TUNING, WORLD } from './core/constants'
import { initial, runState, step } from './core/core'
import type { GameState, Input } from './core/types'
import { createDevPanel } from './devpanel/panel'
import { createKeyboard } from './input/keyboard'
import { createLink, type Link } from './link/link'
import { connectLink } from './link/websocket'
import { draw } from './render/render'

const STEP_MS = 1000 / 60
const RESTART_GRACE_MS = 750
const HIGH_SCORE_KEY = 'theus-game.high-score'
const MAX_FRAME_MS = 250

/** Input held while a Recalibration freezes control (docs/protocol.md). */
const FROZEN_INPUT: Input = { jump: false, crawl: false }

const canvasEl = document.getElementById('game')
if (!(canvasEl instanceof HTMLCanvasElement)) throw new Error('#game canvas is missing')
const canvas: HTMLCanvasElement = canvasEl

const context = canvas.getContext('2d')
if (context === null) throw new Error('the 2d canvas context is unavailable')
const ctx: CanvasRenderingContext2D = context
ctx.imageSmoothingEnabled = false

const keyboard = createKeyboard()

/**
 * The Link endpoint. In the field the page is served by the Device, so the page
 * is already on its host; during development the Vite server is on `localhost`
 * and the Device is the `THEUS-RUN` access point at 192.168.4.1. Override with
 * `VITE_LINK_URL` to point anywhere else.
 */
function defaultLinkUrl(): string {
  const hostname = window.location.hostname
  if (hostname === 'localhost' || hostname === '127.0.0.1') return 'ws://192.168.4.1:81/'
  return `ws://${hostname}:81/`
}
const LINK_URL = import.meta.env.VITE_LINK_URL ?? defaultLinkUrl()

const linkEl = document.getElementById('link')

function showLink(status: 'up' | 'down'): void {
  if (linkEl === null) return
  linkEl.dataset.link = status
  linkEl.textContent = status === 'up' ? 'LINK OK' : 'LINK LOST'
}

// The page's Recalibration control. The Device drives it: the button is
// disabled and input frozen between `cal started` and `cal done`/`failed`.
const recalibrateEl = document.getElementById('recalibrate')
const calStatusEl = document.getElementById('cal-status')

function showRecalibration(phase: 'started' | 'done' | 'failed' | null): void {
  if (recalibrateEl instanceof HTMLButtonElement) {
    recalibrateEl.disabled = phase === 'started'
  }
  if (calStatusEl !== null) {
    calStatusEl.textContent =
      phase === 'started'
        ? 'CALIBRATING — STAND STILL'
        : phase === 'done'
          ? 'READY'
          : phase === 'failed'
            ? 'CALIBRATION FAILED'
            : ''
  }
}

if (recalibrateEl instanceof HTMLButtonElement) {
  recalibrateEl.addEventListener('click', () => link?.sendCal())
}

// The Dev Panel is optional: the game must run even if its markup is absent.
const devEl = document.getElementById('dev')
const devPanel =
  devEl === null
    ? null
    : createDevPanel({
        root: devEl,
        onCfg: (set) => link?.sendCfg(set),
        onMode: (mode) => link?.sendMode(mode),
      })

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

// The Link runs beside the game: it mirrors Run State to the Device and reports
// the Link state on the page. Playing from the keyboard works with no Device.
let link: Link | null = null
const socket = connectLink(LINK_URL, {
  onOpen: () => link?.onOpen(),
  onClose: () => link?.onClose(),
  onText: (text) => link?.receive(text),
})
link = createLink({
  socket,
  app: 'theus-web',
  snapshot: () => ({
    score: state.score,
    hi: state.highScore,
    hearts: state.hearts,
    maxHearts: TUNING.HEARTS,
    run: runState(state),
  }),
  onStatus: (status) => {
    showLink(status)
    // A dropped Link abandons any Recalibration in flight; unfreeze.
    if (status === 'down') showRecalibration(null)
  },
  onMessage: (message) => {
    devPanel?.handle(message)
    if (message.t === 'cal') showRecalibration(message.phase)
  },
})
showLink('down')

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
  // A Recalibration freezes Run input so no phantom Jump can cost a Heart.
  const liveInput = keyboard.input()
  const input: Input = link?.recalibrating ? FROZEN_INPUT : liveInput
  while (accumulator >= STEP_MS) {
    state = step(state, input, STEP_MS)
    accumulator -= STEP_MS
  }

  link?.tick()
  draw(ctx, state)
  requestAnimationFrame(frame)
}

window.addEventListener('resize', fit)
fit()
requestAnimationFrame(frame)
