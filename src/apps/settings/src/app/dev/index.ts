import { z } from "zod";
import { trpc } from "./client";

const zPrefType = z.union([
  z.object({
    prefType: z.literal("boolean"),
    prefName: z.string(),
    value: z.boolean(),
  }),
  z.object({
    prefType: z.literal("integer"),
    prefName: z.string(),
    value: z.number(),
  }),
  z.object({
    prefType: z.literal("string"),
    prefName: z.string(),
    value: z.string(),
  }),
]);

type PrefType = z.infer<typeof zPrefType>;

export function setBoolPref(prefName: string, value: boolean) {
  trpc.post.broadcast.mutate({
    topic: "settings-parent:setPref",
    data: JSON.stringify(
      zPrefType.parse({ prefType: "boolean", prefName, value }),
    ),
  });
}
export function setIntPref(prefName: string, value: number) {
  trpc.post.broadcast.mutate({
    topic: "settings-parent:setPref",
    data: JSON.stringify(
      zPrefType.parse({ prefType: "integer", prefName, value }),
    ),
  });
}
export function setStringPref(prefName: string, value: string) {
  trpc.post.broadcast.mutate({
    topic: "settings-parent:setPref",
    data: JSON.stringify(
      zPrefType.parse({ prefType: "string", prefName, value }),
    ),
  });
}
