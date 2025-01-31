import {defineConfig} from "vitest/config"

export default defineConfig({
  root: ".",
  test: {
    name: "Noraneko",
    pool: "./server/pool.ts",
    typecheck: {
      enabled: true
    }
  }
})