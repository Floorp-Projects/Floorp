import { defineConfig } from "vite";
import tsconfigPaths from "vite-tsconfig-paths";
import path from "node:path";
import react from "@vitejs/plugin-react-swc";
import Icons from "unplugin-icons/vite";
import AutoImport from "unplugin-auto-import/vite";
import IconsResolver from "unplugin-icons/resolver";

import { generateJarManifest } from "../common/scripts/gen_jarmanifest";

const r = (dir: string) => {
  return path.resolve(import.meta.dirname, dir);
};


export default defineConfig({
  publicDir: r("public"),
  server: {
    port: 5183,
  },
  build: {
    reportCompressedSize: false,
    emptyOutDir: true,
    assetsInlineLimit: 0,
    modulePreload: false,
    target: "firefox128",
    sourcemap: false,
    cssMinify: true,
    minify: "terser",
    terserOptions: {
      compress: {
        drop_console: true,
        drop_debugger: true,
      },
    },
    rollupOptions: {
      output: {
        manualChunks: {
          vendor: ["react", "react-dom", "react-router-dom"],
          chakra: ["@chakra-ui/react", "@emotion/react", "@emotion/styled"],
          i18n: ["i18next", "react-i18next"],
        },
      },
    },

    outDir: r("_dist"),
  },

  css: {
    transformer: "lightningcss",
  },

  plugins: [
    react(),
    Icons({ compiler: "jsx", jsx: "react", autoInstall: true }),
    AutoImport({
      resolvers: [
        IconsResolver({
          prefix: "Icon",
          extension: "jsx",
        }),
      ],
      dts: true,
    }),
    tsconfigPaths(),
    // CustomHmr(),

    {
      name: "gen_jarmn",
      enforce: "post",
      async generateBundle(options, bundle, isWrite) {
        this.emitFile({
          type: "asset",
          fileName: "jar.mn",
          needsCodeReference: false,
          source: await generateJarManifest(bundle, {
            prefix: "settings",
            namespace: "noraneko-settings",
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
  optimizeDeps: {
    include: ["./node_modules/@nora"],
  },
});
