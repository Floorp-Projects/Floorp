import { defineConfig } from "vite";
import path from "node:path";
import react from "@vitejs/plugin-react-swc";

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

  //? https://github.com/parcel-bundler/lightningcss/issues/685
  //? lepton uses System Color and that occurs panic.
  //? when the issue resolved, gladly we can use lightningcss
  // css: {
  //   transformer: "lightningcss",
  // },

  plugins: [react()],
  optimizeDeps: {
    include: ["./node_modules/@nora"],
  },
});
