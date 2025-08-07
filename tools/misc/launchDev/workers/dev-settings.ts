import { createServer } from "npm:vite@6.3.3";

Deno.chdir("./src/ui/settings");

const server = await createServer({
  configFile: "./vite.config.ts",
});

await server.listen();
server.printUrls();

self.onmessage = async (e) => {
  server.close();
};
