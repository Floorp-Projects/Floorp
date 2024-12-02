import { defineConfig } from "vite";
import tsconfigPaths from "vite-tsconfig-paths";
import path from "node:path";
import react from "@vitejs/plugin-react-swc";
import Icons from "unplugin-icons/vite";
import AutoImport from "unplugin-auto-import/vite";
import IconsResolver from "unplugin-icons/resolver";
import CustomHmr from "./react-18n-hmr";

const r = (dir: string) => {
  return path.resolve(import.meta.dirname, dir);
};

export default defineConfig({
  publicDir: r("public"),
  server: {
    port: 5183,
  },
  build: {
    sourcemap: true,
    reportCompressedSize: false,
    minify: false,
    cssMinify: false,
    emptyOutDir: true,
    assetsInlineLimit: 0,
    modulePreload: false,
    target: "firefox128",

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
    CustomHmr(),
  ],
  optimizeDeps: {
    include: ["./node_modules/@nora"],
  },
  assetsInclude: ["**/*.json"],
});
