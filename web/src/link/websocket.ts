import type { LinkSocket } from './link'

const MIN_BACKOFF_MS = 250
const MAX_BACKOFF_MS = 5000

export interface SocketHandlers {
  onOpen(): void
  onClose(): void
  onText(text: string): void
}

/**
 * Open the Link and keep it open. A dropped socket is retried with exponential
 * backoff from 250 ms to a 5 s cap, per `docs/protocol.md`. Sends made while the
 * socket is closed are dropped; the page re-sends `state` as soon as the next
 * `onOpen` arrives, so nothing needs queueing.
 */
export function connectLink(url: string, handlers: SocketHandlers): LinkSocket {
  let socket: WebSocket | null = null
  let backoffMs = MIN_BACKOFF_MS
  let timer: number | null = null
  let stopped = false

  function schedule(): void {
    if (stopped) return
    const delay = backoffMs
    backoffMs = Math.min(backoffMs * 2, MAX_BACKOFF_MS)
    timer = window.setTimeout(open, delay)
  }

  function open(): void {
    socket = new WebSocket(url)
    socket.onopen = () => {
      backoffMs = MIN_BACKOFF_MS
      handlers.onOpen()
    }
    socket.onmessage = (event) => {
      if (typeof event.data === 'string') handlers.onText(event.data)
    }
    socket.onclose = () => {
      socket = null
      handlers.onClose()
      schedule()
    }
    // An error is always followed by a close; let that drive the retry.
    socket.onerror = () => {}
  }

  open()

  return {
    send(text: string): void {
      if (socket !== null && socket.readyState === WebSocket.OPEN) socket.send(text)
    },
    close(): void {
      stopped = true
      if (timer !== null) window.clearTimeout(timer)
      socket?.close()
    },
  }
}
