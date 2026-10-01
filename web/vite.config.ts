/// <reference types="vitest" />
import { defineConfig } from 'vite'

// `base: './'` keeps every asset URL relative, which is what lets the built
// bundle be served straight off the Device's flash filesystem. `npm run build`
// lands directly in the firmware's LittleFS data directory, so the deploy is
// just `pio run -t uploadfs` (see the root README).
export default defineConfig({
  base: './',
  build: {
    target: 'es2022',
    outDir: '../firmware/data',
    emptyOutDir: true,
  },
  test: {
    environment: 'node',
    include: ['src/**/*.test.ts'],
  },
})
