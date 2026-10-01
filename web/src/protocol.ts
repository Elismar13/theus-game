/**
 * The Link wire contract, in TypeScript.
 *
 * `docs/protocol.md` is the hand-maintained definition; this file mirrors it for
 * the game page and `firmware/lib/Protocol/Protocol.{h,cpp}` mirrors it for the
 * Device (ADR-0006). Every message is one compact JSON object on one line, so
 * `encode` is a `JSON.stringify` and `decode` is a `JSON.parse` plus validation.
 *
 * Only the messages the game page actually reads and writes live here; the
 * Device is the authority on the rest. Builders exist for the outbound
 * messages so the field order matches the golden fixtures in `docs/`.
 */

import type { RunState } from './core/types'

/** Incremented on any breaking change to the message set. */
export const PROTOCOL_VERSION = 1

/**
 * The Run States that travel on the wire: the Game Core's set plus `PAUSED`,
 * which the Link introduces before the core has a pause of its own (#15).
 */
export type WireRunState = RunState | 'PAUSED'

export const ERR_CODES = [
  'session_taken',
  'protocol_version',
  'bad_message',
  'cfg_rejected',
] as const

export type ErrCode = (typeof ERR_CODES)[number]

export interface HelloMessage {
  readonly t: 'hello'
  readonly v: number
  readonly fw: string
  readonly dev: string
  readonly caps: readonly string[]
}

export interface HbMessage {
  readonly t: 'hb'
  readonly up_ms: number
  readonly batt_mv: number
  readonly rssi?: number
  readonly seq: number
}

export interface ErrMessage {
  readonly t: 'err'
  readonly code: ErrCode
  readonly msg: string
}

export interface CalMessage {
  readonly t: 'cal'
  readonly phase: 'started' | 'done' | 'failed'
  readonly reason?: string
}

/** The four tunable Thresholds, shared by the `cfg` message and its editor. */
export interface Thresholds {
  readonly jump_g: number
  readonly crawl_deg: number
  readonly crawl_hold_ms: number
  readonly jump_refractory_ms: number
}

export interface CfgMessage extends Thresholds {
  readonly t: 'cfg'
  readonly seq: number
}

export interface EvtMessage {
  readonly t: 'evt'
  readonly e: 'JUMP' | 'CRAWL' | 'UP'
  readonly ts: number
  readonly seq: number
  readonly conf?: number
}

export interface RawMessage {
  readonly t: 'raw'
  readonly ax: number
  readonly ay: number
  readonly az: number
  readonly gx: number
  readonly gy: number
  readonly gz: number
  readonly pitch: number
  readonly vert: number
  readonly ts: number
}

export type DeviceMessage =
  | HelloMessage
  | HbMessage
  | ErrMessage
  | CalMessage
  | CfgMessage
  | EvtMessage
  | RawMessage

export interface PageHello {
  readonly t: 'hello'
  readonly v: number
  readonly app: string
}

export interface PageState {
  readonly t: 'state'
  readonly score: number
  readonly hi: number
  readonly hearts: number
  readonly max_hearts: number
  readonly run: WireRunState
  readonly seq: number
}

export interface PageCal {
  readonly t: 'cal'
  readonly action: 'recalibrate'
}

export interface PageCfg {
  readonly t: 'cfg'
  readonly set: Partial<Thresholds>
}

export interface PageMode {
  readonly t: 'mode'
  readonly m: 'play' | 'raw'
}

export type PageMessage = PageHello | PageState | PageCal | PageCfg | PageMode

export function pageHello(app: string, v = PROTOCOL_VERSION): PageHello {
  return { t: 'hello', v, app }
}

export function pageState(fields: Omit<PageState, 't'>): PageState {
  return {
    t: 'state',
    score: fields.score,
    hi: fields.hi,
    hearts: fields.hearts,
    max_hearts: fields.max_hearts,
    run: fields.run,
    seq: fields.seq,
  }
}

export function pageCal(): PageCal {
  return { t: 'cal', action: 'recalibrate' }
}

export function pageCfg(set: Partial<Thresholds>): PageCfg {
  return { t: 'cfg', set }
}

export function pageMode(m: 'play' | 'raw'): PageMode {
  return { t: 'mode', m }
}

/** One compact JSON object on one line, per the framing rule. */
export function encode(message: PageMessage): string {
  return JSON.stringify(message)
}

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value)
}

function number(value: unknown): value is number {
  return typeof value === 'number' && Number.isFinite(value)
}

function string(value: unknown): value is string {
  return typeof value === 'string'
}

/**
 * Parse one frame from the Device. Returns `null` for anything malformed or of
 * an unknown type, so the page is forward-compatible with a newer Device and
 * simply ignores what it does not understand.
 */
export function decode(text: string): DeviceMessage | null {
  let parsed: unknown
  try {
    parsed = JSON.parse(text)
  } catch {
    return null
  }
  if (!isObject(parsed)) return null

  switch (parsed.t) {
    case 'hello':
      if (number(parsed.v) && string(parsed.fw) && string(parsed.dev) && Array.isArray(parsed.caps)) {
        return {
          t: 'hello',
          v: parsed.v,
          fw: parsed.fw,
          dev: parsed.dev,
          caps: parsed.caps.filter(string),
        }
      }
      return null
    case 'hb':
      if (number(parsed.up_ms) && number(parsed.batt_mv) && number(parsed.seq)) {
        return {
          t: 'hb',
          up_ms: parsed.up_ms,
          batt_mv: parsed.batt_mv,
          ...(number(parsed.rssi) ? { rssi: parsed.rssi } : {}),
          seq: parsed.seq,
        }
      }
      return null
    case 'err': {
      const code = parsed.code
      if (string(code) && (ERR_CODES as readonly string[]).includes(code) && string(parsed.msg)) {
        return { t: 'err', code: code as ErrCode, msg: parsed.msg }
      }
      return null
    }
    case 'cal': {
      const phase = parsed.phase
      if (phase === 'started' || phase === 'done' || phase === 'failed') {
        return {
          t: 'cal',
          phase,
          ...(string(parsed.reason) ? { reason: parsed.reason } : {}),
        }
      }
      return null
    }
    case 'cfg':
      if (
        number(parsed.jump_g) &&
        number(parsed.crawl_deg) &&
        number(parsed.crawl_hold_ms) &&
        number(parsed.jump_refractory_ms) &&
        number(parsed.seq)
      ) {
        return {
          t: 'cfg',
          jump_g: parsed.jump_g,
          crawl_deg: parsed.crawl_deg,
          crawl_hold_ms: parsed.crawl_hold_ms,
          jump_refractory_ms: parsed.jump_refractory_ms,
          seq: parsed.seq,
        }
      }
      return null
    case 'evt': {
      const e = parsed.e
      if ((e === 'JUMP' || e === 'CRAWL' || e === 'UP') && number(parsed.ts) && number(parsed.seq)) {
        return {
          t: 'evt',
          e,
          ts: parsed.ts,
          seq: parsed.seq,
          ...(number(parsed.conf) ? { conf: parsed.conf } : {}),
        }
      }
      return null
    }
    case 'raw':
      if (
        number(parsed.ax) &&
        number(parsed.ay) &&
        number(parsed.az) &&
        number(parsed.gx) &&
        number(parsed.gy) &&
        number(parsed.gz) &&
        number(parsed.pitch) &&
        number(parsed.vert) &&
        number(parsed.ts)
      ) {
        return {
          t: 'raw',
          ax: parsed.ax,
          ay: parsed.ay,
          az: parsed.az,
          gx: parsed.gx,
          gy: parsed.gy,
          gz: parsed.gz,
          pitch: parsed.pitch,
          vert: parsed.vert,
          ts: parsed.ts,
        }
      }
      return null
    default:
      return null
  }
}
