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
        break;
      }
      case "settings-parent:getPref": {
        const data = zPrefType.parse(JSON.parse(value.data));
        console.log(data);

        switch (data.prefType) {
          case "boolean": {
            trpc.post.broadcast.mutate({
              topic: "settings-child:getPref",
              data: JSON.stringify(
                zPrefType.parse({
                  prefType: "boolean",
                  prefName: data.prefName,
                  value: Services.prefs.getBoolPref(data.prefName),
                }),
              ),
            });
            break;
          }
          case "integer": {
            trpc.post.broadcast.mutate({
              topic: "settings-child:getPref",
              data: JSON.stringify(
                zPrefType.parse({
                  prefType: "integer",
                  prefName: data.prefName,
                  value: Services.prefs.getIntPref(data.prefName),
                }),
              ),
            });
            break;
          }
          case "string": {
            trpc.post.broadcast.mutate({
              topic: "settings-child:getPref",
              data: JSON.stringify(
                zPrefType.parse({
                  prefType: "string",
                  prefName: data.prefName,
                  value: Services.prefs.getStringPref(data.prefName),
                }),
              ),
            });
            break;
          }
        }
        break;
      }
      case "settings-parent:openChromeURL": {
        const data = z
          .object({ url: z.string() })
          .parse(JSON.parse(value.data));
        window.gBrowser.addTab(data.url, {
          inBackground: false,
          triggeringPrincipal:
            Services.scriptSecurityManager.getSystemPrincipal(),
        });
        break;
      }
    }
  },
});
