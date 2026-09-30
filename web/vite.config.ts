/// <reference types="vitest" />
import { defineConfig } from 'vite'

// `base: './'` keeps every asset URL relative, which is what lets the built
// bundle be served straight off the Device's flash filesystem.
export default defineConfig({
  base: './',
  build: {
    target: 'es2022',
    outDir: 'dist',
  },
  test: {
    environment: 'node',
    include: ['src/**/*.test.ts'],
  },
})
