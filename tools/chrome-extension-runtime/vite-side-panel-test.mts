// SPDX-License-Identifier: MPL-2.0
import { defineConfig, loadConfigFromFile, type UserConfig } from "vite";
import { fileURLToPath } from "node:url";

export default defineConfig(async (environment): Promise<UserConfig> => {
  // Load the production build config through Vite's own loader. Its bundling and
  // workspace configuration stay authoritative for the native UI test build.
  const loaded = await loadConfigFromFile(
    environment,
    fileURLToPath(
      new URL("../../bridge/loader-features/vite.config.ts", import.meta.url),
    ),
  );
  if (!loaded) {
    throw new Error(
      "Native side panel test could not load the production Vite config",
    );
  }
  const base = loaded.config;
  return {
    root: new URL("../../bridge/loader-features", import.meta.url).pathname,
    cacheDir:
      new URL("../../node_modules/.vite/side-panel-test", import.meta.url)
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
        entry: new URL(
          "../../browser-features/chrome/common/panel-sidebar/test/ChromeSidePanelUI.entry.tsx",
          import.meta.url,
        ).pathname,
        formats: ["es"],
        fileName: "side-panel-ui",
      },
      rollupOptions: { output: { inlineDynamicImports: true } },
    },
  };
});
