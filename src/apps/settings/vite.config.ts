import { defineConfig } from "vite";
import tsconfigPaths from "vite-tsconfig-paths";
import react from "@vitejs/plugin-react-swc";
import tailwindcss from "@tailwindcss/vite";
import path from "node:path"
//import { generateJarManifest } from "../common/scripts/gen_jarmanifest.ts";

export default defineConfig({
  server: {
    port: 5183,
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
      "@": path.resolve(import.meta.dirname, "./src"),
    },
  },
  plugins: [
    tsconfigPaths(),
    react(),
    tailwindcss(),
    // {
    //   name: "gen_jarmn",
    //   enforce: "post",
    //   async generateBundle(options, bundle, isWrite) {
    //     this.emitFile({
    //       type: "asset",
    //       fileName: "jar.mn",
    //       needsCodeReference: false,
    //       source: await generateJarManifest(bundle, {
    //         prefix: "content",
    //         namespace: "noraneko-settings",
    //         register_type: "content",
    //       }),
    //     });
    //     this.emitFile({
    //       type: "asset",
    //       fileName: "moz.build",
    //       needsCodeReference: false,
    //       source: `JAR_MANIFESTS += ["jar.mn"]`,
    //     });
    //   },
    // },
  ],
});
