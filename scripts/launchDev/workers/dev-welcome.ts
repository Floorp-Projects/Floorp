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
const settingsDir = resolve(rootDir, "src/apps/welcome");
console.info("[worker:welcome] Setting working directory to:", settingsDir);

const server = await createServer({
  configFile: resolve(settingsDir, "vite.config.ts"),
  root: settingsDir,
});

await server.listen();
server.printUrls();
