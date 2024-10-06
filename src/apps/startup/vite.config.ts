import path from "node:path";
import { defineConfig } from "vite";

import { generateJarManifest } from "../common/scripts/gen_jarmanifest";

const r = (subpath: string): string =>
  path.resolve(import.meta.dirname, subpath);

export default defineConfig({
  build: {
    outDir: r("_dist"),
    target: "firefox128",
    lib: {
      formats: ["es"],
      entry: [
        r("src/chrome_root.ts"),
        r("src/about-preferences.ts"),
        r("src/about-newtab.ts"),
      ],
    },
    rollupOptions: {
      external(source, importer, isResolved) {
        if (source.startsWith("chrome://")) {
          return true;
        }
        return false;
      },
    },
  },
  plugins: [
    {
      name: "gen_jarmn",
      enforce: "post",
      async generateBundle(options, bundle, isWrite) {
        this.emitFile({
          type: "asset",
          fileName: "jar.mn",
          needsCodeReference: false,
          source: await generateJarManifest(bundle, {
            prefix: "startup",
            namespace: "noraneko-startup",
            register_type: "content",
          }),
        });
        this.emitFile({
          type: "asset",
          fileName: "moz.build",
          needsCodeReference: false,
          source: `JAR_MANIFESTS += ["jar.mn"]`,
        });
      },
    },
  ],
});
