import { Plugin } from "vite";
import { createBirpc } from "birpc";

export default function NoranekoTestPlugin() {
  return {
    name: "vitest-noraneko",
    configureServer(server) {
      server.ws.on('vitest-noraneko:init', (data, client) => {
        console.log("init",data)
        createBirpc({},{
          post: data => client.send('vitest-noraneko:message', data),
          on: callback => client.on('vitest-noraneko:message', callback),
          // these are required when using WebSocket
          serialize: v => JSON.stringify(v),
          deserialize: v => JSON.parse(v),
        })
      })
    }
  } satisfies Plugin
}