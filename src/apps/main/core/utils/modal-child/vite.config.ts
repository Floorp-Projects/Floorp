import { defineConfig, PluginOption } from "vite";
import tsconfigPaths from "vite-tsconfig-paths";
import react from "@vitejs/plugin-react-swc";
import tailwindcss from "@tailwindcss/vite";
import { generateJarManifest } from "../../../../common/scripts/gen_jarmanifest.ts";
import { fileURLToPath, URL as NodeURL } from "node:url";

export default defineConfig({
  server: {
    port: 5185,
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
      "@": fileURLToPath(new NodeURL("./src", import.meta.url)),
    },
  },
  plugins: [
    tsconfigPaths() as PluginOption,
    react(),
    tailwindcss(),
    {
      name: "gen_jarmn",
      enforce: "post",
      async generateBundle(_options, bundle, _isWrite) {
        this.emitFile({
          type: "asset",
          fileName: "jar.mn",
          needsCodeReference: false,
          source: await generateJarManifest(bundle, {
            prefix: "content-modal-child",
            namespace: "noraneko-modal-child",
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
