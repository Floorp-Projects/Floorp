import {resolve} from "pathe"
import {build} from "vite"
import packageJson from "../../package.json" with { type: "json" };
import { $,usePwsh } from "zx";

switch (process.platform) {
  case "win32":
    usePwsh()
}

const r = (value:string) : string => {
  return resolve(import.meta.dirname,"../..",value)
}

async function launchBuild(mode:string,buildid2:string) {
  if (mode.startsWith("dev")) {
    await Promise.all([
      $({cwd:r("./src/apps/startup")})`pnpm build ${mode}`,
      build({
        configFile: r("./src/apps/modules/vite.config.ts"),
        root: r("./src/apps/modules"),
        define: {
          "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
          "import.meta.env.__VERSION2__": `"${packageJson.version}"`
        },
      }),
    ])
  } else {
    await Promise.all([
      $({cwd:r("./src/apps/startup")})`pnpm build ${mode}`,
      //$({cwd:r("./src/apps/modules")})`pnpm build ${JSON.stringify({mode,buildid2,version2:packageJson.version})}`,
      build({
        configFile: r("./src/apps/modules/vite.config.ts"),
        root: r("./src/apps/modules"),
        define: {
          "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
          "import.meta.env.__VERSION2__": `"${packageJson.version}"`
        },
      }),
      build({
        configFile: r("./src/apps/main/vite.config.ts"),
        root: r("./src/apps/main"),
        define: {
          "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
          "import.meta.env.__VERSION2__": `"${packageJson.version}"`
        },
        base: "chrome://noraneko/content"
      }),
      build({
        configFile: r("./src/apps/designs/vite.config.ts"),
        root: r("./src/apps/designs"),
      }),
      build({
        configFile: r("./src/apps/settings/vite.config.ts"),
        root: r("./src/apps/settings"),
        base: "chrome://noraneko-settings/content"
      }),
    ]);
  }
}

{ //* main
  await launchBuild(process.argv[2],process.argv[3])
}