// import { createHTTPServer } from "@trpc/server/adapters/standalone";
// import { type AppRouter, appRouter, createContext } from "./defines";
// import { WebSocketServer } from "ws";
// import { applyWSSHandler } from "@trpc/server/adapters/ws";

// // http server
// const { server, listen } = createHTTPServer({
//   router: appRouter,
//   createContext,
// });

// // ws server
// const wss = new WebSocketServer({ server });
// applyWSSHandler<AppRouter>({
//   wss,
//   router: appRouter,
//   createContext,
// });

// listen(5191);

// console.log("Initialized Test Server for Dev | http:localhost:5191");

import { createBirpc } from "birpc";
import type { ServerFunctions, ClientFunctions } from "../common/test/types";
import { WebSocketServer } from "ws";
import fs from "node:fs/promises";

const serverFunctions: ServerFunctions = {
  hi(name: string) {
    return `Hi ${name} from server`;
  },
  async saveImage(name, image) {
    await fs.writeFile(
      `${name}.png`,
      image.replace(/^data:image\/png;base64,/, ""),
      "base64",
    );
    return true;
  },
};

const wss = new WebSocketServer({ port: 5191 });

wss.on("connection", async (ws) => {
  const rpc = createBirpc<ClientFunctions, ServerFunctions>(serverFunctions, {
    post: (data) => ws.send(data),
    on: (fn) =>
      ws.addEventListener("message", (ev) => {
        fn(ev.data);
      }),
    serialize: (v) => JSON.stringify(v),
    deserialize: (v) => JSON.parse(v),
  });

  await rpc.hey("Server"); // Hey Server from client
});
