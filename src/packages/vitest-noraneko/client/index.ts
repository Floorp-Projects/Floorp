import WebSocket from "ws";
import { createBirpc } from "birpc";
import NoranekoRunner from "./runner";
import { RunnerRPC, RuntimeRPC } from "vitest";

export function connectVitestPoolNoraneko(url:string) {
  const ws = new WebSocket(url);
  createBirpc<RuntimeRPC,RunnerRPC>(new NoranekoRunner(),{
    post: data => ws.send(data),
    on: callback => ws.on('message', callback),
    // these are required when using WebSocket
    serialize: v => JSON.stringify(v),
    deserialize: v => JSON.parse(v),
  })
}