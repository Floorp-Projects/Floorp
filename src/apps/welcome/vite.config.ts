import { defineConfig, type PluginOption } from "vite";
import tsconfigPaths from "vite-tsconfig-paths";
import react from "@vitejs/plugin-react-swc";
import tailwindcss from "@tailwindcss/vite";
import { generateJarManifest } from "../common/scripts/gen_jarmanifest.ts";
import { join } from "node:path";
import { dirname } from "node:path";

export default defineConfig({
  server: {
    port: 5187,
    strictPort: true,
  },
  css: {
    postcss: import.meta.dirname,
  },
  build: {
    outDir: "_dist",
  },
  resolve: {
    alias: {
      "@/": join(dirname(import.meta.url), "src"),
    },
  },
  plugins: [
    tsconfigPaths() as PluginOption,
    react(),
    tailwindcss(),
    {
      name: "gen_jarmn",
      enforce: "post",
      async generateBundle(options, bundle, isWrite) {
        this.emitFile({
          type: "asset",
          fileName: "jar.mn",
          needsCodeReference: false,
          source: await generateJarManifest(bundle, {
            prefix: "content-welcome",
            namespace: "noraneko-welcome",
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
