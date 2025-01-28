import { createServer, ViteDevServer } from "vite"
import {resolve} from "pathe"
import packageJson from "../../package.json" assert { type: "json" };

let pDevVite: ViteDevServer[] = []

const r = (value:string) : string => {
  return resolve(import.meta.dirname,"../..",value)
}

async function launchDev(mode:string,buildid2:string) {
  pDevVite = [
    await createServer({
      mode,
      configFile: r("./src/apps/main/vite.config.ts"),
      root: r("./src/apps/main"),
      define: {
        "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
        "import.meta.env.__VERSION2__": `"${packageJson.version}"`
      },
    }),
    await createServer({
      mode,
      configFile: r("./src/apps/designs/vite.config.ts"),
      root: r("./src/apps/designs"),
    }),
    await createServer({
      mode,
      configFile: r("./src/apps/settings/vite.config.ts"),
      root: r("./src/apps/settings"),
    })
  ]
  for (const i of pDevVite) {
    await i.listen()
    i.printUrls()
  }
}

async function shutdownDev() {
  for (const i of pDevVite) {
    await i.close()
  }
}

{ //* main
  process.stdin.on("data",async (d)=>{
    if (d.toString().startsWith("s")) {
      console.log("[child-dev] Shutdown ViteDevServer")
      await shutdownDev()
      process.exit(1);
    }
  });
  await launchDev(process.argv[1],process.argv[2])
}