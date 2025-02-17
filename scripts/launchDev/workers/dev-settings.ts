import { createServer } from "npm:vite@6.0.11";

Deno.chdir("./src/apps/settings");

const server = await createServer({
  configFile: "./vite.config.ts",
});

await server.listen();
server.printUrls();

self.onmessage = async (e) => {
  server.close();
};
