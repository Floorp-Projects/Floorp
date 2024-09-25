import { createHTTPServer } from "@trpc/server/adapters/standalone";
import { type AppRouter, appRouter, createContext } from "./defines";
import { WebSocketServer } from "ws";
import { applyWSSHandler } from "@trpc/server/adapters/ws";

// http server
const { server, listen } = createHTTPServer({
  router: appRouter,
  createContext,
});

// ws server
const wss = new WebSocketServer({ server });
applyWSSHandler<AppRouter>({
  wss,
  router: appRouter,
  createContext,
});

listen(5191);

console.log("Initialized Test Server for Dev | http:localhost:5191");
