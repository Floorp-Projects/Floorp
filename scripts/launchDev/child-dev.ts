import { createServer, type ViteDevServer } from "vite";
import { resolve } from "pathe";
import packageJson from "../../package.json" with { type: "json" };
import { $, type Options, type ProcessPromise, usePwsh } from "zx";

if (Deno.build.os === "windows") {
  usePwsh();
}

let pDevVite: ViteDevServer[] = [];
const workers: Worker[] = [];

const r = (value: string): string => {
  return resolve(import.meta.dirname as string, "../..", value);
};

async function launchDev(mode: string, buildid2: string) {
  const rootDir = resolve(import.meta.dirname as string, "../..");
  console.log(`[child-dev] Root directory: ${rootDir}`);

  pDevVite = [
    await createServer({
      mode,
      configFile: r("./src/apps/main/vite.config.ts"),
      root: r("./src/apps/main"),
      define: {
        "import.meta.env.__BUILDID2__": `"${buildid2 ?? ""}"`,
        "import.meta.env.__VERSION2__": `"${packageJson.version}"`,
      },
    }),
  ];

  const workersDir = resolve(import.meta.dirname as string, "workers");

  try {
    for await (const entry of Deno.readDir(workersDir)) {
      if (entry.isFile && entry.name.startsWith("dev-")) {
        const workerUrl =
          new URL(`./workers/${entry.name}`, import.meta.url).href;
        console.log(`[child-dev] Starting worker: ${entry.name}`);

        const worker = new Worker(workerUrl, {
          type: "module",
        });

        worker.postMessage({ type: "setRootDir", rootDir });
        workers.push(worker);
      }
    }
  } catch (err: unknown) {
    const error = err as Error;
    console.error(`[child-dev] Error loading workers: ${error.message}`);
  }

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

  for (const worker of workers) {
    worker.postMessage({ type: "shutdown" });
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
  await launchDev(Deno.args[0], Deno.args[1]);
}
