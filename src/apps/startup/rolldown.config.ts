import { defineConfig } from "rolldown";
import { generateJarManifest } from "../common/scripts/gen_jarmanifest";
import {resolve} from "pathe"

const r = (subpath: string): string =>
  resolve(import.meta.dirname, subpath);

let mode = "";
if (!process.argv.at(-1).includes(".")) mode = process.argv.at(-1)

export default defineConfig({
  input: [
    r("src/chrome_root.ts"),
    r("src/about-preferences.ts"),
    r("src/about-newtab.ts")
  ],
  output: {
    "dir":"_dist",
    chunkFileNames:"[name].js"
  },
  define: {
    "import.meta.env.MODE": `"${mode}"`
  },
  external(source, importer, isResolved) {
    if (source.startsWith("chrome://")) {
      return true;
    }
    return false;
  },
  plugins: [
    {
      name: "gen_jarmn",
      async generateBundle(options, bundle, isWrite) {
        this.emitFile({
          type: "asset",
          fileName: "jar.mn",
          source: await generateJarManifest(bundle, {
            prefix: "startup",
            namespace: "noraneko-startup",
            register_type: "content",
          }),
        });
        this.emitFile({
          type: "asset",
          fileName: "moz.build",
          source: `JAR_MANIFESTS += ["jar.mn"]`,
        });
      },
    },
  ],
});
