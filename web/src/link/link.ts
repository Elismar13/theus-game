import {
  decode,
  encode,
  pageCfg,
  pageHello,
  pageMode,
  pageState,
  PROTOCOL_VERSION,
  type DeviceMessage,
  type Thresholds,
  type WireRunState,
} from '../protocol'

/**
 * The game page's half of the Session/liveness state machine.
 *
 * It owns no transport: frames arrive through `receive`, the WebSocket's open
 * and close are reported through `onOpen`/`onClose`, and time is read from an
 * injectable clock. That keeps the interesting behaviour — the hello handshake,
 * the 3-missed-heartbeat Link Down and the `seq` filter — testable without a
 * browser or a socket.
 */

export type LinkStatus = 'up' | 'down'

export interface LinkSocket {
  send(text: string): void
  close(): void
}

/** What the page mirrors back to the Status Board. */
export interface LinkSnapshot {
  readonly score: number
  readonly hi: number
  readonly hearts: number
  readonly maxHearts: number
  readonly run: WireRunState
}

export interface LinkOptions {
  readonly socket: LinkSocket
  readonly app: string
  readonly snapshot: () => LinkSnapshot
  readonly now?: () => number
  /** Device heartbeat period; both sides agree on this. */
  readonly heartbeatMs?: number
  /** Consecutive missed heartbeats that trip Link Down. */
  readonly missedHeartbeats?: number
  readonly onStatus?: (status: LinkStatus) => void
  readonly onMessage?: (message: DeviceMessage) => void
}

export interface Link {
  /** The transport connected; opens the Session with `hello`. */
  onOpen(): void
  /** The transport closed; the Link is down until the next open. */
  onClose(): void
  /** One inbound text frame. */
  receive(text: string): void
  /** Call often; sends `state` and watches for missed heartbeats. */
  tick(): void
  /** Ask the Device to apply a subset of Thresholds. */
  sendCfg(set: Partial<Thresholds>): void
  /** Switch the Device between play and raw debug mode. */
  sendMode(mode: 'play' | 'raw'): void
  readonly status: LinkStatus
}

export function createLink(options: LinkOptions): Link {
  const now = options.now ?? (() => performance.now())
  const heartbeatMs = options.heartbeatMs ?? 1000
  const timeoutMs = heartbeatMs * (options.missedHeartbeats ?? 3)

  let open = false
  let status: LinkStatus = 'down'
  let peerHelloSeen = false
  let lastInboundMs = 0
  let lastStateMs = 0
  let outboundSeq = 0
  let inboundSeq = 0

  function setStatus(next: LinkStatus): void {
    if (next === status) return
    status = next
    options.onStatus?.(next)
  }

  function send(text: string): void {
    if (open) options.socket.send(text)
  }

  function sendState(): void {
    outboundSeq += 1
    lastStateMs = now()
    const snapshot = options.snapshot()
    send(
      encode(
        pageState({
          score: snapshot.score,
          hi: snapshot.hi,
          hearts: snapshot.hearts,
          max_hearts: snapshot.maxHearts,
          run: snapshot.run,
          seq: outboundSeq,
        }),
      ),
    )
  }

  return {
    onOpen(): void {
      open = true
      peerHelloSeen = false
      outboundSeq = 0
      inboundSeq = 0
      lastInboundMs = now()
      lastStateMs = 0
      setStatus('down')
      send(encode(pageHello(options.app, PROTOCOL_VERSION)))
      // The protocol asks for an immediate `state` on reconnect.
      sendState()
    },

    onClose(): void {
      open = false
      peerHelloSeen = false
      setStatus('down')
    },

    receive(text: string): void {
      if (!open) return
      const message = decode(text)
      if (message === null) return

      if ('seq' in message) {
        if (message.seq <= inboundSeq) return // stale across a reconnect
        inboundSeq = message.seq
      }

      // Liveness is the Device's heartbeat, not just any traffic: a burst of
      // `raw` in debug mode must not mask a stopped `hb`.
      if (message.t === 'hb') lastInboundMs = now()

      if (message.t === 'hello' && message.v === PROTOCOL_VERSION) {
        peerHelloSeen = true
      }
      if (peerHelloSeen) setStatus('up')
      options.onMessage?.(message)
    },

    tick(): void {
      if (!open) return
      const nowMs = now()
      if (peerHelloSeen && nowMs - lastInboundMs >= timeoutMs) setStatus('down')
      if (nowMs - lastStateMs >= heartbeatMs) sendState()
    },

    sendCfg(set: Partial<Thresholds>): void {
      send(encode(pageCfg(set)))
    },

    sendMode(mode: 'play' | 'raw'): void {
      send(encode(pageMode(mode)))
    },

    get status(): LinkStatus {
      return status
    },
  }
}
