import { describe, expect, it, vi } from 'vitest'
import type { RawMessage } from '../protocol'
import { drawPlot, PLOT_SERIES, SampleBuffer, valueToY } from './plot'

function sample(overrides: Partial<RawMessage> = {}): RawMessage {
  return {
    t: 'raw',
    ax: 0,
    ay: 1,
    az: 0,
    gx: 0,
    gy: 0,
    gz: 0,
    pitch: 0,
    vert: 1,
    ts: 0,
    ...overrides,
  }
}

function stubContext() {
  const strokes = { count: 0 }
  const ctx = {
    clearRect: vi.fn(),
    beginPath: vi.fn(),
    moveTo: vi.fn(),
    lineTo: vi.fn(),
    stroke: () => {
      strokes.count += 1
    },
    strokeStyle: '',
    lineWidth: 1,
  }
  return { ctx: ctx as unknown as CanvasRenderingContext2D, strokes }
}

describe('valueToY', () => {
  it('maps the range onto the plot, top-down', () => {
    expect(valueToY(0, 0, 10, 100)).toBe(100)
    expect(valueToY(10, 0, 10, 100)).toBe(0)
    expect(valueToY(5, 0, 10, 100)).toBe(50)
  })

  it('clamps values outside the range', () => {
    expect(valueToY(-5, 0, 10, 100)).toBe(100)
    expect(valueToY(50, 0, 10, 100)).toBe(0)
  })

  it('does not divide by an empty range', () => {
    expect(valueToY(3, 4, 4, 100)).toBe(100)
  })
})

describe('SampleBuffer', () => {
  it('keeps one column per series and trims to capacity', () => {
    const buffer = new SampleBuffer(PLOT_SERIES, 3)
    for (let i = 0; i < 5; i += 1) buffer.push(sample({ vert: i, pitch: i * 10 }))

    expect(buffer.size).toBe(3)
    expect(buffer.column(0)).toEqual([2, 3, 4])
    expect(buffer.column(1)).toEqual([20, 30, 40])
  })

  it('clears every column', () => {
    const buffer = new SampleBuffer(PLOT_SERIES, 3)
    buffer.push(sample())
    buffer.clear()
    expect(buffer.size).toBe(0)
    expect(buffer.column(0)).toEqual([])
  })
})

describe('drawPlot smoke', () => {
  it('draws a stroke per series once there is a trace', () => {
    const buffer = new SampleBuffer(PLOT_SERIES, 8)
    for (let i = 0; i < 4; i += 1) buffer.push(sample({ vert: i / 4, pitch: i * 5 }))

    const { ctx, strokes } = stubContext()
    expect(() => drawPlot(ctx, 320, 120, PLOT_SERIES, buffer)).not.toThrow()
    // Three gridlines plus one stroke per series.
    expect(strokes.count).toBe(3 + PLOT_SERIES.length)
  })

  it('draws only the gridlines with a single sample', () => {
    const buffer = new SampleBuffer(PLOT_SERIES, 8)
    buffer.push(sample())

    const { ctx, strokes } = stubContext()
    drawPlot(ctx, 320, 120, PLOT_SERIES, buffer)
    expect(strokes.count).toBe(3)
  })
})
