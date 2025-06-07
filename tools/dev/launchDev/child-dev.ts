import { createServer, type ViteDevServer } from "vite";
import { resolve } from "pathe";
import packageJson from "../../../package.json" with { type: "json" };

let pDevVite: ViteDevServer[] = [];

const r = (value: string): string => {
  return resolve(import.meta.dirname!, "../../..", value);
};

async function launchDev(mode: string, buildid2: string) {
  pDevVite = [
    await createServer({
      mode,
      configFile: r("./src/core/glue/loader-features/vite.config.ts"),
      root: r("./src/core/glue/loader-features"),
      define: {
        "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
        "import.meta.env.__VERSION2__": `"${packageJson.version}"`,
      },
    }),
  ];

  for (const i of pDevVite) {
    await i.listen();
    i.printUrls();
  }
  console.log("nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-dev");
}

async function shutdownDev() {
  for (const i of pDevVite) {
    await i.close();
  }
  console.log("[child-dev] Completed Shutdown ViteDevServer✅");
}

{ //* main
  const decoder = new TextDecoder();
  (async () => {
    for await (const chunk of Deno.stdin.readable) {
      const text = decoder.decode(chunk);
      if (text.startsWith("q")) {
        console.log("[child-dev] Shutdown ViteDevServer");
        await shutdownDev();
        Deno.exit(0);
      }
    }
  })();
  await launchDev(Deno.args[0] || "dev", Deno.args[1] || "default-build-id");
}
