import { resolve } from "pathe";
import { build } from "vite";
import packageJson from "../../package.json" with { type: "json" };
import { $, usePwsh } from "zx";
import process from "node:process";

switch (process.platform) {
  case "win32":
    usePwsh();
}

const r = (value: string): string => {
  return resolve(import.meta.dirname as string, "../..", value);
};

async function launchBuild(mode: string, buildid2: string) {
  if (mode.startsWith("dev")) {
    await Promise.all([
      $({ cwd: r("./src/apps/startup") })`deno task build ${mode}`,
      build({
        configFile: r("./src/apps/modules/vite.config.ts"),
        root: r("./src/apps/modules"),
        define: {
          "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
          "import.meta.env.__VERSION2__": `"${packageJson.version}"`,
        },
      }),
      build({
        configFile: r("./src/apps/newtab/vite.config.ts"),
        root: r("./src/apps/newtab"),
      }),
    ]);
  } else {
    await Promise.all([
      $({ cwd: r("./src/apps/startup") })`deno task build ${mode}`,
      build({
        root: r("./src/apps/main"),
        configFile: r("./src/apps/main/vite.config.ts"),
        define: {
          "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
          "import.meta.env.__VERSION2__": `"${packageJson.version}"`,
        },
        base: "chrome://noraneko/content",
      }),
      build({
        root: r("./src/apps/modules"),
        configFile: r("./src/apps/modules/vite.config.ts"),
        define: {
          "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
          "import.meta.env.__VERSION2__": `"${packageJson.version}"`,
        },
        base: "resource://noraneko",
      }),
      build({
        configFile: r("./src/apps/newtab/vite.config.ts"),
        root: r("./src/apps/newtab"),
        base: "chrome://noraneko-newtab/content",
      }),
      build({
        configFile: r("./src/apps/settings/vite.config.ts"),
        root: r("./src/apps/settings"),
        base: "chrome://noraneko-settings/content",
      }),
      build({
        configFile: r("./src/apps/main/core/utils/modal-child/vite.config.ts"),
        root: r("./src/apps/main/core/utils/modal-child"),
        base: "chrome://noraneko-modal-child/content",
      }),
    ]);
  }
}

{ //* main
  await launchBuild(Deno.args[0], Deno.args[1]);
}
