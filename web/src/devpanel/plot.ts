import type { RawMessage } from '../protocol'

/** One trace on the dev plot: how to read it, how to scale it, what colour. */
export interface PlotSeries {
  readonly label: string
  readonly color: string
  readonly min: number
  readonly max: number
  readonly of: (sample: RawMessage) => number
}

/**
 * The two features the Edge Classifier will lean on (#11), with the ranges they
 * are expected to move in. Vertical is torso-up acceleration in g; pitch is the
 * forward torso angle in degrees.
 */
export const PLOT_SERIES: readonly PlotSeries[] = [
  { label: 'vert', color: '#57c785', min: -0.5, max: 2.5, of: (sample) => sample.vert },
  { label: 'pitch', color: '#f4a259', min: -90, max: 90, of: (sample) => sample.pitch },
]

/** Maps a value in [min, max] to a y pixel, with y growing downward. */
export function valueToY(value: number, min: number, max: number, height: number): number {
  const span = max - min
  if (span <= 0) return height
  const clamped = Math.max(min, Math.min(max, value))
  return height - ((clamped - min) / span) * height
}

/** A fixed-length rolling window of samples, one column per series. */
export class SampleBuffer {
  private readonly columns: number[][]

  constructor(
    private readonly series: readonly PlotSeries[],
    readonly capacity: number,
  ) {
    this.columns = series.map(() => [])
  }

  push(sample: RawMessage): void {
    this.series.forEach((spec, index) => {
      const column = this.columns[index]
      if (column === undefined) return
      column.push(spec.of(sample))
      if (column.length > this.capacity) column.shift()
    })
  }

  clear(): void {
    for (const column of this.columns) column.length = 0
  }

  column(index: number): readonly number[] {
    return this.columns[index] ?? []
  }

  get size(): number {
    return this.columns[0]?.length ?? 0
  }
}

/** Draws the rolling window, anchored to the right so it scrolls leftward. */
export function drawPlot(
  ctx: CanvasRenderingContext2D,
  width: number,
  height: number,
  series: readonly PlotSeries[],
  buffer: SampleBuffer,
): void {
  ctx.clearRect(0, 0, width, height)

  ctx.strokeStyle = '#241d3a'
  ctx.lineWidth = 1
  for (const y of [0, Math.round(height / 2), height - 1]) {
    ctx.beginPath()
    ctx.moveTo(0, y)
    ctx.lineTo(width, y)
    ctx.stroke()
  }

  const stepX = width / Math.max(1, buffer.capacity - 1)
  series.forEach((spec, index) => {
    const values = buffer.column(index)
    if (values.length < 2) return
    const offset = buffer.capacity - values.length
    ctx.strokeStyle = spec.color
    ctx.lineWidth = 1.5
    ctx.beginPath()
    values.forEach((value, i) => {
      const x = (offset + i) * stepX
      const y = valueToY(value, spec.min, spec.max, height - 2) + 1
      if (i === 0) ctx.moveTo(x, y)
      else ctx.lineTo(x, y)
    })
    ctx.stroke()
  })
}
