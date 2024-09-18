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

trpc.post.onBroadcast.subscribe(undefined, {
  onData(value) {
    switch (value.topic) {
      case "settings-parent:setPref": {
        const data = zPrefType.parse(JSON.parse(value.data));

        switch (data.prefType) {
          case "boolean": {
            Services.prefs.setBoolPref(data.prefName, data.value);
            break;
          }
          case "integer": {
            Services.prefs.setIntPref(data.prefName, data.value);
            break;
          }
          case "string": {
            Services.prefs.setStringPref(data.prefName, data.value);
            break;
          }
        }
      }
    }
  },
});
