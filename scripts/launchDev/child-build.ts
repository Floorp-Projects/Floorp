import { resolve } from "pathe";
import { build } from "vite";
import packageJson from "../../package.json" with { type: "json" };
import { $, usePwsh } from "zx";

switch (process.platform) {
  case "win32":
    usePwsh();
}

const r = (value: string): string => {
  return resolve(import.meta.dirname, "../..", value);
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
        configFile: r("./src/apps/designs/vite.config.ts"),
        root: r("./src/apps/designs"),
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
        configFile: r("./src/apps/designs/vite.config.ts"),
        root: r("./src/apps/designs"),
      }),
      build({
        configFile: r("./src/apps/settings/vite.config.ts"),
        root: r("./src/apps/settings"),
        base: "chrome://noraneko-settings/content",
      }),
    ]);
  }
}

{ //* main
  await launchBuild(Deno.args[0], Deno.args[1]);
}
