import { createHTTPServer } from "@trpc/server/adapters/standalone";
import { type AppRouter, appRouter, createContext } from "./defines";
import { WebSocketServer } from "ws";
import { applyWSSHandler } from "@trpc/server/adapters/ws";

// http server
const { server, listen } = createHTTPServer({
  router: appRouter,
  createContext,
  responseMeta: (_opts) => {
    return {
      headers: {
        "Access-Control-Allow-Origin": "http://localhost:5183",
        "Access-Control-Allow-Headers":
          "Origin, X-Requested-With, Content-Type, Accept",
      },
      status: 200,
    };
  },
});

// ws server
const wss = new WebSocketServer({ server });
applyWSSHandler<AppRouter>({
  wss,
  router: appRouter,
  createContext,
});

listen(5192);

console.log("Initialized Settings Middleware for Dev | http:localhost:5192");
