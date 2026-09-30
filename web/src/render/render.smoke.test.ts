import { describe, expect, it, vi } from 'vitest'
import { advance, CRAWL, FRAME_MS, IDLE, JUMP } from '../testing/harness'
import { initial, step } from '../core/core'
import type { GameState } from '../core/types'
import { draw } from './render'

function stubContext() {
  const counts = { fillRect: 0, fillText: 0, fill: 0 }
  const gradient = { addColorStop: vi.fn() }
  const ctx = {
    fillStyle: '',
    font: '',
    textAlign: 'left',
    textBaseline: 'top',
    imageSmoothingEnabled: true,
    createLinearGradient: () => gradient,
    fillRect: () => {
      counts.fillRect += 1
    },
    fillText: () => {
      counts.fillText += 1
    },
    beginPath: () => {},
    moveTo: () => {},
    lineTo: () => {},
    closePath: () => {},
    fill: () => {
      counts.fill += 1
    },
  }
  return { ctx: ctx as unknown as CanvasRenderingContext2D, counts }
}

describe('renderer smoke', () => {
  it('draws every Run State without touching an undefined property', () => {
    const states: Array<[string, GameState]> = [
      ['READY', initial(1)],
      ['RUN', advance(step(initial(1), JUMP, FRAME_MS), IDLE, 40)],
      ['JUMP', step(initial(1), JUMP, FRAME_MS)],
      ['CRAWL', advance(advance(step(initial(1), JUMP, FRAME_MS), IDLE, 40), CRAWL, 2)],
      [
        'INVULN',
        {
          ...advance(step(initial(1), JUMP, FRAME_MS), IDLE, 40),
          invulnMs: 500,
          flashMs: 200,
        },
      ],
      ['DEAD', { ...initial(1), dead: true, started: true, score: 42 }],
    ]

    for (const [name, state] of states) {
      const { ctx, counts } = stubContext()
      expect(() => draw(ctx, state), name).not.toThrow()
      expect(counts.fillRect, name).toBeGreaterThan(10)
    }
  })

  it('draws obstacles of both kinds', () => {
    const { ctx, counts } = stubContext()
    const withObstacles: GameState = {
      ...initial(1),
      started: true,
      obstacles: [
        { id: 1, kind: 'low', x: 200, top: 148, width: 16, height: 20, gapAfter: 80 },
        { id: 2, kind: 'high', x: 260, top: 96, width: 22, height: 56, gapAfter: 100 },
      ],
    }
    expect(() => draw(ctx, withObstacles)).not.toThrow()
    expect(counts.fillRect).toBeGreaterThan(20)
  })
})
