import { readFileSync } from 'node:fs'
import { describe, expect, it } from 'vitest'
import {
  decode,
  encode,
  pageCal,
  pageCfg,
  pageHello,
  pageMode,
  pageState,
  PROTOCOL_VERSION,
} from './protocol'

/**
 * The two golden files are the shared contract with the firmware tests:
 * `firmware/test/test_protocol` parses the same bytes this suite does.
 */
function fixture(name: string): string[] {
  const url = new URL(`../../docs/${name}`, import.meta.url)
  return readFileSync(url, 'utf8')
    .split('\n')
    .map((line) => line.trim())
    .filter((line) => line.length > 0 && !line.startsWith('#'))
}

const DEVICE_TO_PAGE = fixture('protocol.device-to-page.ndjson')
const PAGE_TO_DEVICE = fixture('protocol.page-to-device.ndjson')

/** Rebuild a page-to-device frame from its own JSON, in canonical field order. */
function rebuild(line: string): string {
  const raw = JSON.parse(line) as Record<string, unknown>
  switch (raw.t) {
    case 'hello':
      return encode(pageHello(raw.app as string, raw.v as number))
    case 'state':
      return encode(
        pageState({
          score: raw.score as number,
          hi: raw.hi as number,
          hearts: raw.hearts as number,
          max_hearts: raw.max_hearts as number,
          run: raw.run as never,
          seq: raw.seq as number,
        }),
      )
    case 'cal':
      return encode(pageCal())
    case 'cfg':
      return encode(pageCfg(raw.set as Record<string, number>))
    case 'mode':
      return encode(pageMode(raw.m as 'play' | 'raw'))
    default:
      throw new Error(`unhandled page message ${String(raw.t)}`)
  }
}

describe('device-to-page codec', () => {
  it('decodes every golden frame', () => {
    for (const line of DEVICE_TO_PAGE) {
      const message = decode(line)
      expect(message, line).not.toBeNull()
      expect(message?.t, line).toBe((JSON.parse(line) as { t: string }).t)
    }
  })

  it('reads the hello handshake', () => {
    const hello = decode(DEVICE_TO_PAGE[0] ?? '')
    expect(hello).toEqual({
      t: 'hello',
      v: PROTOCOL_VERSION,
      fw: '0.1.0',
      dev: 'AABBCC',
      caps: ['cfg', 'raw'],
    })
  })

  it('reads Thresholds and the raw debug stream', () => {
    expect(
      decode(
        '{"t":"cfg","jump_g":1.6,"crawl_deg":45,"crawl_hold_ms":150,"jump_refractory_ms":250,"seq":6}',
      ),
    ).toEqual({
      t: 'cfg',
      jump_g: 1.6,
      crawl_deg: 45,
      crawl_hold_ms: 150,
      jump_refractory_ms: 250,
      seq: 6,
    })
    expect(
      decode(
        '{"t":"raw","ax":0.01,"ay":-0.98,"az":0.12,"gx":0.5,"gy":0.1,"gz":-0.2,"pitch":3.4,"vert":0.05,"ts":2000}',
      ),
    ).toEqual({
      t: 'raw',
      ax: 0.01,
      ay: -0.98,
      az: 0.12,
      gx: 0.5,
      gy: 0.1,
      gz: -0.2,
      pitch: 3.4,
      vert: 0.05,
      ts: 2000,
    })
  })

  it('reads heartbeat telemetry, with rssi only when joined', () => {
    const first = decode(DEVICE_TO_PAGE[1] ?? '')
    expect(first).toEqual({ t: 'hb', up_ms: 1234, batt_mv: 3900, seq: 1 })

    const joined = decode(DEVICE_TO_PAGE[2] ?? '')
    expect(joined).toEqual({ t: 'hb', up_ms: 2234, batt_mv: 3880, rssi: -58, seq: 2 })
  })

  it('reads intents and calibration', () => {
    expect(decode('{"t":"evt","e":"JUMP","ts":1000,"seq":3}')).toEqual({
      t: 'evt',
      e: 'JUMP',
      ts: 1000,
      seq: 3,
    })
    expect(decode('{"t":"cal","phase":"failed","reason":"too noisy"}')).toEqual({
      t: 'cal',
      phase: 'failed',
      reason: 'too noisy',
    })
  })
})

describe('page-to-device codec', () => {
  it('re-encodes every golden frame byte for byte', () => {
    for (const line of PAGE_TO_DEVICE) {
      expect(rebuild(line), line).toBe(line)
    }
  })
})

describe('decode', () => {
  it('rejects malformed, unknown and mistyped frames', () => {
    expect(decode('not json')).toBeNull()
    expect(decode('[]')).toBeNull()
    expect(decode('{"t":"wat"}')).toBeNull()
    expect(decode('{"t":"hb","up_ms":"soon","batt_mv":3900,"seq":1}')).toBeNull()
    expect(decode('{"t":"err","code":"nope","msg":"x"}')).toBeNull()
  })
})
