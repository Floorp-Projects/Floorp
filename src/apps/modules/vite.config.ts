import { defineConfig } from "vite";

import swc from "unplugin-swc"

import fg from "fast-glob";

export default defineConfig({
  root: ".",
  build: {
    outDir: "_dist",
    reportCompressedSize: false,
    modulePreload: false,
    lib: {
      entry: [...(await fg("./src/**/*.mts"))],
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
        preserveModulesRoot: "./src"
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
