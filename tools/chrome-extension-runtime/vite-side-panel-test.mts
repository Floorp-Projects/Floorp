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
  // CI builds this fixture before feles-build prepares its directory links.
  // Resolve the same checked-in sources directly instead of requiring those
  // generated links to exist in a fresh dependency checkout.
  if (!Array.isArray(base.resolve?.alias)) {
    throw new Error(
      "Native side panel test requires the production alias list",
    );
  }
  const alias = base.resolve.alias.map((entry) => {
    const source = entry.find === "#features-chrome"
      ? "../../browser-features/chrome"
      : entry.find === "#i18n"
      ? "../../i18n"
      : undefined;
    return source
      ? {
        ...entry,
        replacement: fileURLToPath(new URL(source, import.meta.url)),
      }
      : entry;
  });
  return {
    root: fileURLToPath(
      new URL("../../bridge/loader-features", import.meta.url),
    ),
    cacheDir: fileURLToPath(
      new URL("../../node_modules/.vite/side-panel-test", import.meta.url),
    ),
    publicDir: false,
    define: base.define,
    resolve: { ...base.resolve, alias },
    plugins: base.plugins?.slice(0, 3),
    build: {
      target: "firefox157",
      minify: false,
      sourcemap: true,
      outDir: fileURLToPath(
        new URL("../../_dist/side-panel-ui", import.meta.url),
      ),
      emptyOutDir: true,
      lib: {
        entry: fileURLToPath(
          new URL(
            "../../browser-features/chrome/common/panel-sidebar/test/ChromeSidePanelUI.entry.tsx",
            import.meta.url,
          ),
        ),
        formats: ["es"],
        fileName: "side-panel-ui",
      },
      rollupOptions: { output: { inlineDynamicImports: true } },
    },
  };
});
