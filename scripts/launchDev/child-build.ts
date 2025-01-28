import {resolve} from "pathe"
import {build} from "vite"
import packageJson from "../../package.json" assert { type: "json" };
import { execa } from "execa";

const r = (value:string) : string => {
  return resolve(import.meta.dirname,"../..",value)
}

async function launchBuild(mode:string,buildid2:string) {
  if (mode.startsWith("dev")) {
    await Promise.all([
      build({
        mode,
        configFile: r("./src/apps/designs/vite.config.ts"),
        root: r("./src/apps/designs"),
      }),
      build({
        configFile: r("./src/apps/modules/vite.config.ts"),
        root:r("./src/apps/modules"),
        define: {
          "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
          "import.meta.env.__VERSION2__": `"${packageJson.version}"`
        }
      })
    ])
    await execa({
      preferLocal: true,
      stdout: "inherit",
      cwd: r("./src/apps/modules"),
    })`pnpm genJarManifest`;
  } else {
    await Promise.all([
      build({
        configFile: r("./src/apps/startup/vite.config.ts"),
        root: r("./src/apps/startup"),
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
      build({
        configFile: r("./src/apps/modules/vite.config.ts"),
        root:r("./src/apps/modules"),
        define: {
          "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
          "import.meta.env.__VERSION2__": `"${packageJson.version}"`
        }
      })
    ]);
    await execa({
      preferLocal: true,
      stdout: "inherit",
      cwd: r("./src/apps/modules"),
    })`pnpm genJarManifest`;
  }
}

{ //* main
  await launchBuild(process.argv[1],process.argv[2])
}