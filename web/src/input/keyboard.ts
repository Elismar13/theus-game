import type { Input } from '../core/types'

const JUMP_CODES = new Set(['Space', 'ArrowUp', 'KeyW'])
const CRAWL_CODES = new Set(['ArrowDown', 'KeyS'])
const RESTART_CODES = new Set(['Enter', 'KeyR'])

export interface Keyboard {
  /** The Intents as they are currently held. */
  input(): Input
  /** True at most once per Enter/R press. */
  takeRestart(): boolean
}

export function createKeyboard(target: Window = window): Keyboard {
  const down = new Set<string>()
  let restart = false

  target.addEventListener('keydown', (event) => {
    if (JUMP_CODES.has(event.code) || CRAWL_CODES.has(event.code)) event.preventDefault()
    if (RESTART_CODES.has(event.code)) {
      event.preventDefault()
      if (!event.repeat) restart = true
    }
    down.add(event.code)
  })

  target.addEventListener('keyup', (event) => {
    down.delete(event.code)
  })

  // Losing focus must not leave an Intent stuck on.
  target.addEventListener('blur', () => {
    down.clear()
  })

  return {
    input(): Input {
      let jump = false
      let crawl = false
      for (const code of down) {
        if (JUMP_CODES.has(code)) jump = true
        if (CRAWL_CODES.has(code)) crawl = true
      }
      return { jump, crawl }
    },
    takeRestart(): boolean {
      const value = restart
      restart = false
      return value
    },
  }
}
