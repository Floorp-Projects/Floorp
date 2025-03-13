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
const projectDir = resolve(
  rootDir,
  "src/apps/main/core/utils/modal-child",
);
console.info("[worker:modal-child] Setting working directory to:", projectDir);

const server = await createServer({
  configFile: resolve(projectDir, "vite.config.ts"),
  root: projectDir,
});

await server.listen();
server.printUrls();
