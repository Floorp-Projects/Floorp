// SPDX-License-Identifier: MPL-2.0
import { defineConfig } from "vite";
import base from "../../bridge/loader-features/vite.config.ts";

export default defineConfig({
  root: new URL("../../bridge/loader-features", import.meta.url).pathname,
  cacheDir: new URL("../../node_modules/.vite/side-panel-test", import.meta.url)
    .pathname,
  publicDir: false,
  define: base.define,
  resolve: base.resolve,
  plugins: base.plugins?.slice(0, 3),
  build: {
    target: "firefox157",
    minify: false,
    sourcemap: true,
    outDir: new URL("../../_dist/side-panel-ui", import.meta.url).pathname,
    emptyOutDir: true,
    lib: {
      entry: new URL("./side-panel-ui-entry.mts", import.meta.url).pathname,
      formats: ["es"],
      fileName: "side-panel-ui",
    },
    rollupOptions: { output: { inlineDynamicImports: true } },
  },
});
