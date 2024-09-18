// import { initSidebar } from "./browser-sidebar";
import { initI18N } from "../i18n/config";
//console.log("run init");

const modules_common = import.meta.glob("./common/*/index.ts");

const modules = {
  common: {} as Record<string, () => Promise<unknown>>,
};

Object.entries(modules_common).map((v) => {
  modules.common[v[0].replace("./common/", "").replace("/index.ts", "")] =
    v[1] as () => Promise<unknown>;
});
console.log(modules);

const modules_keys = {
  common: Object.keys(modules.common),
};

export default async function initScripts() {
  //@ts-expect-error ii
  SessionStore.promiseInitialized.then(async () => {
    initI18N();
    Services.prefs
      .getDefaultBranch(null as unknown as string)
      .setStringPref("noraneko.features.all", JSON.stringify(modules_keys));
    Services.prefs.lockPref("noraneko.features.all");

    Services.prefs
      // biome-ignore lint/suspicious/noExplicitAny: <explanation>
      .getDefaultBranch(null as any)
      .setStringPref("noraneko.features.enabled", JSON.stringify(modules_keys));
    const enabled_features = JSON.parse(
      Services.prefs.getStringPref("noraneko.features.enabled", "{}"),
    ) as typeof modules_keys;
    //import("./example/counter/index");
    for (const [categoryKey, categoryValue] of Object.entries(modules)) {
      for (const moduleName in categoryValue) {
        (async () => {
          try {
            if (
              categoryKey in enabled_features &&
              enabled_features[
                categoryKey as keyof typeof enabled_features
              ].includes(moduleName)
            )
              (
                (await categoryValue[moduleName]()) as {
                  init?: typeof Function;
                }
              ).init?.();
          } catch (e) {
            console.error(e);
          }
        })();
      }
    }
    //CustomShortcutKey.getInstance();
  });
}
