import { defineConfig } from "vite";
import {generateJarManifest} from "../common/scripts/gen_jarmanifest"

import fs from "fs/promises"

let entry = []
for await (const x of fs.glob(import.meta.dirname + "/src/**/*.mts")) {
  entry.push(x);
}

export default defineConfig({
  base: "resource://noraneko",
  build: {
    target:"firefox133",
    assetsInlineLimit: 0,
    outDir: "_dist",
    reportCompressedSize: false,
    modulePreload: false,
    lib: {
      entry,
      formats: ["es"],
      fileName(_format, entryName) {
        return entryName + ".mjs";
      },
    },
    rollupOptions: {
      external(source, _importer, _isResolved) {
        return (
          source.startsWith("resource://") || source.startsWith("chrome://")
        );
      },
      output: {
        //* Tells rollup to preserve file structure
        //https://stackoverflow.com/a/78546497
        preserveModules: true,
        preserveModulesRoot: import.meta.dirname +"/src"
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
            prefix: "resource",
            namespace: "noraneko",
            register_type: "resource",
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
  ]
});
