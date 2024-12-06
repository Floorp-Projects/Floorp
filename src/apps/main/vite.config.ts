import { defineConfig } from "vite";
import path from "node:path";
import tsconfigPaths from "vite-tsconfig-paths";
import solidPlugin from "vite-plugin-solid";

import { generateJarManifest } from "../common/scripts/gen_jarmanifest";

const r = (dir: string) => {
  return path.resolve(import.meta.dirname, dir);
};

export default defineConfig({
  publicDir: r("public"),
  server: {
    port: 5181,
  },
  build: {
    sourcemap: true,
    reportCompressedSize: false,
    minify: false,
    cssMinify: false,
    emptyOutDir: true,
    assetsInlineLimit: 0,
    target: "firefox128",

    rollupOptions: {
      //https://github.com/vitejs/vite/discussions/14454
      preserveEntrySignatures: "allow-extension",
      input: {
        core: r("./core/index.ts"),
        "about-preferences": r("./about/preferences/index.ts"),
        "about-newtab": r("./about/newtab/index.ts"),
        //env: "./experiment/env.ts",
      },
      output: {
        esModule: true,
        entryFileNames: "[name].js",
        manualChunks(id, meta) {
          if (id.includes("node_modules")) {
            const arr_module_name = id
              .toString()
              .split("node_modules/")[1]
              .split("/");
            if (arr_module_name[0] === ".pnpm") {
              return "ext." + arr_module_name[1].toString();
            }
            return "ext." + arr_module_name[0].toString();
          }
          if (id.includes(".svg")) {
            return "svg." + id.split("/").at(-1);
          }
        },
        assetFileNames(assetInfo) {
          if (assetInfo.name?.endsWith(".svg")) {
            return "assets/svg/[name][extname]";
          }
          if (assetInfo.name?.endsWith(".css")) {
            return "assets/css/[name][extname]";
          }
          return "assets/[name][extname]";
        },
        chunkFileNames(chunkInfo) {
          if (chunkInfo.name.startsWith("svg.")) {
            return "assets/svg/glue/[name].js";
          }
          if (chunkInfo.name.startsWith("ext.")) {
            return "assets/js/external/[name].js";
          }
          return "assets/js/[name].js";
        },
      },
    },

    outDir: r("_dist"),
  },

  //? https://github.com/parcel-bundler/lightningcss/issues/685
  //? lepton uses System Color and that occurs panic.
  //? when the issue resolved, gladly we can use lightningcss
  // css: {
  //   transformer: "lightningcss",
  // },

  plugins: [
    tsconfigPaths(),
    {
      name: "solid-xul-refresh",
      apply: "serve",
      handleHotUpdate(ctx) {
        console.log(`handle hot : ${JSON.stringify(ctx.modules)}`);
      },
    },
    solidPlugin({
      solid: {
        generate: "universal",
        moduleName: "@nora/solid-xul",
        contextToCustomElements: false,
      },
      hot: false,
    }),
    {
      name: "gen_jarmn",
      enforce: "post",
      async generateBundle(options, bundle, isWrite) {
        this.emitFile({
          type: "asset",
          fileName: "jar.mn",
          needsCodeReference: false,
          source: await generateJarManifest(bundle, {
            prefix: "content",
            namespace: "noraneko",
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
  resolve: {
    preserveSymlinks: true,
  },
});
