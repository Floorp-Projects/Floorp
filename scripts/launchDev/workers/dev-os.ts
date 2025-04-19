import { createServer } from "npm:vite@6.0.11";
import { resolve } from "npm:pathe";

let rootDirPromiseResolve: (value: string) => void;
const rootDirPromise = new Promise<string>((resolve) => {
  rootDirPromiseResolve = resolve;
});

self.onmessage = async (e) => {
  if (e.data.type === "setRootDir") {
    rootDirPromiseResolve(e.data.rootDir);
  } else if (e.data.type === "shutdown") {
    await server.close();
  }
};

const rootDir = await rootDirPromise;
const workDir = resolve(rootDir, "src/apps/os");
console.info("[worker:os] Setting working directory to:", workDir);

const server = await createServer({
  configFile: resolve(workDir, "vite.config.ts"),
  root: workDir,
});

await server.listen();
server.printUrls();
