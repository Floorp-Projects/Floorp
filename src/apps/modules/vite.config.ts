import { defineConfig } from "vite";

import swc from "unplugin-swc"
import fs from "fs/promises"

let entry = []
for await (const x of fs.glob(import.meta.dirname + "/src/**/*.mts")) {
  entry.push(x);
}

export default defineConfig({
  build: {
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
    swc.vite({
      "jsc": {
        "target":"esnext",
        "parser": {
          "syntax":"typescript"
        }
      }
    }),
  ]
});
