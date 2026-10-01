import type { DeviceMessage, RawMessage, Thresholds } from '../protocol'
import { drawPlot, PLOT_SERIES, SampleBuffer } from './plot'

export interface DevPanelOptions {
  readonly root: HTMLElement
  /** A Threshold patch from the form, ready for `Link.sendCfg`. */
  readonly onCfg: (set: Partial<Thresholds>) => void
  /** The raw/play switch, ready for `Link.sendMode`. */
  readonly onMode: (mode: 'play' | 'raw') => void
}

export interface DevPanel {
  /** Feed every Device frame; the panel ignores the ones it does not show. */
  handle(message: DeviceMessage): void
}

const CFG_FIELDS = ['jump_g', 'crawl_deg', 'crawl_hold_ms', 'jump_refractory_ms'] as const
type CfgField = (typeof CFG_FIELDS)[number]

const PLOT_CAPACITY = 160

function required<T extends Element>(root: ParentNode, selector: string): T {
  const element = root.querySelector<T>(selector)
  if (element === null) throw new Error(`dev panel is missing ${selector}`)
  return element
}

function readoutOf(sample: RawMessage): string {
  const n = (value: number): string => value.toFixed(2)
  return (
    `ax ${n(sample.ax)}  ay ${n(sample.ay)}  az ${n(sample.az)}\n` +
    `gx ${n(sample.gx)}  gy ${n(sample.gy)}  gz ${n(sample.gz)}\n` +
    `pitch ${n(sample.pitch)}  vert ${n(sample.vert)}`
  )
}

/**
 * The Dev Panel: a collapsible section that plots the raw debug stream, shows
 * the live numbers, and edits Thresholds. It owns no Link and no protocol — it
 * reports intent through the callbacks and renders the frames it is handed.
 */
export function createDevPanel(options: DevPanelOptions): DevPanel {
  const root = options.root
  const toggle = document.getElementById('dev-toggle')
  const raw = required<HTMLInputElement>(root, '#dev-raw')
  const plotCanvas = required<HTMLCanvasElement>(root, '#dev-plot')
  const readout = required<HTMLElement>(root, '#dev-readout')
  const form = required<HTMLFormElement>(root, '#dev-cfg')
  const status = required<HTMLElement>(root, '#dev-cfg-status')

  const plotContext = plotCanvas.getContext('2d')
  const buffer = new SampleBuffer(PLOT_SERIES, PLOT_CAPACITY)

  const draw = (): void => {
    if (plotContext !== null) {
      drawPlot(plotContext, plotCanvas.width, plotCanvas.height, PLOT_SERIES, buffer)
    }
  }

  toggle?.addEventListener('click', () => {
    root.hidden = !root.hidden
    toggle.setAttribute('aria-expanded', String(!root.hidden))
    draw()
  })

  raw.addEventListener('change', () => {
    options.onMode(raw.checked ? 'raw' : 'play')
    if (!raw.checked) {
      buffer.clear()
      draw()
    }
  })

  form.addEventListener('submit', (event) => {
    event.preventDefault()
    const set: Partial<Record<CfgField, number>> = {}
    for (const field of CFG_FIELDS) {
      const input = form.elements.namedItem(field)
      if (!(input instanceof HTMLInputElement) || input.value.trim() === '') continue
      const value = Number(input.value)
      if (Number.isFinite(value)) set[field] = value
    }
    options.onCfg(set)
    status.textContent = 'sent…'
  })

  function fillCfg(values: Thresholds): void {
    for (const field of CFG_FIELDS) {
      const input = form.elements.namedItem(field)
      // Do not fight someone mid-edit.
      if (input instanceof HTMLInputElement && document.activeElement !== input) {
        input.value = String(values[field])
      }
    }
  }

  return {
    handle(message: DeviceMessage): void {
      if (message.t === 'raw') {
        buffer.push(message)
        readout.textContent = readoutOf(message)
        draw()
        return
      }
      if (message.t === 'cfg') {
        fillCfg(message)
        status.textContent =
          `device: jump_g ${message.jump_g}, crawl_deg ${message.crawl_deg}, ` +
          `crawl_hold_ms ${message.crawl_hold_ms}, ` +
          `jump_refractory_ms ${message.jump_refractory_ms}`
        return
      }
      if (message.t === 'err' && message.code === 'cfg_rejected') {
        status.textContent = `rejected: ${message.msg}`
      }
    },
  }
}
