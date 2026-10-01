import { describe, expect, it } from 'vitest'
import type { DeviceMessage } from '../protocol'
import { createLink, type LinkSocket, type LinkStatus } from './link'

/** The page's own outbound frames, read back as plain JSON. */
function sentMessage(text: string | undefined): unknown {
  return text === undefined ? null : JSON.parse(text)
}

const DEVICE_HELLO = '{"t":"hello","v":1,"fw":"0.1.0","dev":"AABBCC","caps":["cal","cfg","raw"]}'
const HB = (seq: number): string => `{"t":"hb","up_ms":${seq * 1000},"batt_mv":3900,"seq":${seq}}`

function harness() {
  const sent: string[] = []
  const socket: LinkSocket = { send: (text) => sent.push(text), close: () => {} }
  const statuses: LinkStatus[] = []
  const messages: DeviceMessage[] = []
  let clock = 0

  const link = createLink({
    socket,
    app: 'theus-web',
    now: () => clock,
    snapshot: () => ({ score: 42, hi: 99, hearts: 2, maxHearts: 3, run: 'RUN' }),
    onStatus: (status) => statuses.push(status),
    onMessage: (message) => messages.push(message),
  })

  return {
    link,
    sent,
    statuses,
    messages,
    advance: (ms: number): void => {
      clock += ms
    },
  }
}

describe('Session', () => {
  it('opens with hello and an immediate state', () => {
    const { link, sent } = harness()
    link.onOpen()

    expect(sentMessage(sent[0])).toEqual({ t: 'hello', v: 1, app: 'theus-web' })
    expect(sentMessage(sent[1])).toEqual({
      t: 'state',
      score: 42,
      hi: 99,
      hearts: 2,
      max_hearts: 3,
      run: 'RUN',
      seq: 1,
    })
  })

  it('goes up only once the Device hello arrives', () => {
    const { link, statuses } = harness()
    link.onOpen()
    expect(link.status).toBe('down')

    link.receive(DEVICE_HELLO)
    expect(link.status).toBe('up')
    expect(statuses).toEqual(['up'])
  })

  it('resets the Session on reconnect and re-handshakes', () => {
    const { link, sent, statuses } = harness()
    link.onOpen()
    link.receive(DEVICE_HELLO)
    expect(link.status).toBe('up')

    link.onClose()
    expect(link.status).toBe('down')

    sent.length = 0
    link.onOpen()
    expect(sentMessage(sent[0])).toEqual({ t: 'hello', v: 1, app: 'theus-web' })
    expect(sentMessage(sent[1])).toMatchObject({ t: 'state', seq: 1 })
    expect(statuses).toEqual(['up', 'down'])
  })
})

describe('Liveness', () => {
  it('trips Link Down after three missed heartbeats', () => {
    const { link, advance } = harness()
    link.onOpen()
    link.receive(DEVICE_HELLO)
    expect(link.status).toBe('up')

    advance(2999)
    link.tick()
    expect(link.status).toBe('up')

    advance(1)
    link.tick()
    expect(link.status).toBe('down')
  })

  it('does not treat other traffic as a heartbeat', () => {
    const { link, advance } = harness()
    link.onOpen()
    link.receive(DEVICE_HELLO)

    for (let i = 1; i <= 4; i += 1) {
      advance(1000)
      link.receive(`{"t":"evt","e":"JUMP","ts":${i * 1000},"seq":${i}}`)
      link.tick()
    }
    expect(link.status).toBe('down')
  })

  it('keeps sending state while the socket is open', () => {
    const { link, sent, advance } = harness()
    link.onOpen()
    sent.length = 0

    advance(1000)
    link.tick()
    expect(sent).toHaveLength(1)
    expect(sentMessage(sent[0])).toMatchObject({ t: 'state', seq: 2 })

    advance(500)
    link.tick()
    expect(sent).toHaveLength(1)
  })
})

describe('Ordering', () => {
  it('drops stale seq from the Device', () => {
    const { link, messages } = harness()
    link.onOpen()
    link.receive(HB(5))
    link.receive(HB(4))
    link.receive(HB(6))
    expect(messages.map((message) => message.t)).toEqual(['hb', 'hb'])
    expect(messages.map((message) => (message.t === 'hb' ? message.seq : 0))).toEqual([5, 6])
  })

  it('ignores malformed and unknown frames', () => {
    const { link, messages, advance } = harness()
    link.onOpen()
    link.receive('not json')
    link.receive('{"t":"wat"}')
    link.receive('{"t":"hb","up_ms":"soon","batt_mv":1,"seq":1}')
    expect(messages).toHaveLength(0)

    // The Link is still healthy: a real heartbeat still lands.
    advance(1000)
    link.receive(HB(1))
    expect(messages).toHaveLength(1)
  })
})

describe('Tuning', () => {
  it('sends a Threshold patch and a mode switch', () => {
    const { link, sent } = harness()
    link.onOpen()
    sent.length = 0

    link.sendCfg({ jump_g: 1.7, crawl_hold_ms: 200 })
    link.sendMode('raw')

    expect(sentMessage(sent[0])).toEqual({
      t: 'cfg',
      set: { jump_g: 1.7, crawl_hold_ms: 200 },
    })
    expect(sentMessage(sent[1])).toEqual({ t: 'mode', m: 'raw' })
  })

  it('drops tuning frames while the socket is closed', () => {
    const { link, sent } = harness()
    link.sendCfg({ jump_g: 1.7 })
    link.sendMode('raw')
    expect(sent).toHaveLength(0)
  })

  it('forwards cfg and raw from the Device to onMessage', () => {
    const { link, messages } = harness()
    link.onOpen()
    link.receive(
      '{"t":"cfg","jump_g":1.6,"crawl_deg":45,"crawl_hold_ms":150,"jump_refractory_ms":250,"seq":1}',
    )
    link.receive(
      '{"t":"raw","ax":0,"ay":1,"az":0,"gx":0,"gy":0,"gz":0,"pitch":0,"vert":1,"ts":10}',
    )
    expect(messages.map((message) => message.t)).toEqual(['cfg', 'raw'])
  })
})
