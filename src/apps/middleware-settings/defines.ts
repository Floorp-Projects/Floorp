import { initTRPC } from "@trpc/server";
import type { CreateHTTPContextOptions } from "@trpc/server/adapters/standalone";

import type { CreateWSSContextFnOptions } from "@trpc/server/adapters/ws";

import { observable } from "@trpc/server/observable";

import { z } from "zod";
import { EventEmitter } from "node:events";

// This is how you initialize a context for the server
export function createContext(
  opts: CreateHTTPContextOptions | CreateWSSContextFnOptions,
) {
  return {};
}
type Context = Awaited<ReturnType<typeof createContext>>;

const t = initTRPC.context<Context>().create();

const publicProcedure = t.procedure;
const router = t.router;

export const ee = new EventEmitter();

const postRouter = router({
  initCompleted: publicProcedure.mutation(async ({ input }) => {}),
  onBroadcast: publicProcedure.subscription(() => {
    return observable<{ topic: string; data: string }>((emit) => {
      const onBroadcast = (data: { topic: string; data: string }) => {
        // emits a number every second
        emit.next({ topic: data.topic, data: data.data });
      };

      ee.on("broadcast", onBroadcast);

      return () => {
        ee.off("broadcast", onBroadcast);
      };
    });
  }),
  broadcast: publicProcedure
    .input(
      z.object({
        topic: z.string(),
        data: z.string(),
      }),
    )
    .mutation(async ({ input }) => {
      ee.emit("broadcast", input);
    }),
});

// Merge routers together
export const appRouter = router({
  post: postRouter,
});

export type AppRouter = typeof appRouter;
