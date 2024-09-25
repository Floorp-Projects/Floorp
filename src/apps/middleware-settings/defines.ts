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

async function broadcast(input: {
  topic: string;
  data: string;
  returnTopic?: string;
}) {
  ee.emit("broadcast", input);
  if (input.returnTopic) {
    return new Promise((resolve, reject) => {
      const onReturnTopic = (_in: typeof input) => {
        if (input.returnTopic === _in.topic) {
          ee.off("broadcast", onReturnTopic);
          resolve(JSON.parse(_in.data));
        }
      };
      ee.on("broadcast", onReturnTopic);
    });
  }
}

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
  /**
   * * For client, only use for experimental method
   * * This is good for prototyping
   */
  broadcast: publicProcedure
    .input(
      z.object({
        topic: z.string(),
        data: z.string(),
        returnTopic: z.string().optional(),
      }),
    )
    .mutation(async ({ input, ctx }) => {
      const topic = input.topic;
      const data = input.data;
      if (topic && data)
        broadcast({
          topic,
          data,
          returnTopic: input.returnTopic,
        });
    }),
  /**
   * * Currently not used but good to move from broadcast
   */
  getBoolPref: publicProcedure
    .input(
      z.object({
        prefType: z.literal("boolean"),
        prefName: z.string(),
      }),
    )
    .mutation(async ({ input }) => {
      const retType = z.object({
        prefType: z.literal("boolean"),
        prefName: z.string(),
        value: z.boolean(),
      });
      return retType.parse(
        await broadcast({
          topic: "settings-parent:getPref",
          data: JSON.stringify(input),
          returnTopic: "settings-child:getPref",
        }),
      );
    }),
});

// Merge routers together
export const appRouter = router({
  post: postRouter,
});

export type AppRouter = typeof appRouter;
