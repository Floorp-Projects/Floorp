import "vite/modulepreload-polyfill";

// import { initSidebar } from "./browser-sidebar";
import { initI18N } from "../i18n/config";
//console.log("run init");

const MODULES_COMMON = import.meta.glob("./common/*/index.ts");

const MODULES = {
  common: {} as Record<string, () => Promise<unknown>>,
};

Object.entries(MODULES_COMMON).map((v) => {
  MODULES.common[v[0].replace("./common/", "").replace("/index.ts", "")] =
    v[1] as () => Promise<unknown>;
});
console.log(MODULES);

const modules_keys = {
  common: Object.keys(MODULES.common),
};

export default async function initScripts() {
  ChromeUtils.importESModule("resource://noraneko/modules/BrowserGlue.sys.mjs");
  initI18N();
  Services.prefs
    .getDefaultBranch(null as unknown as string)
    //.setStringPref("noraneko.features.all", JSON.stringify(modules_keys));
    .setStringPref(
      "noraneko.features.all",
      `{"common":["update-refresh-cache"]}`,
    );
  Services.prefs.lockPref("noraneko.features.all");

  Services.prefs
    // biome-ignore lint/suspicious/noExplicitAny: <explanation>
    .getDefaultBranch(null as any)
    //.setStringPref("noraneko.features.enabled", JSON.stringify(modules_keys));
    .setStringPref(
      "noraneko.features.enabled",
      `{"common":["update-refresh-cache"]}`,
    );
  const enabled_features = JSON.parse(
    Services.prefs.getStringPref("noraneko.features.enabled", "{}"),
  ) as typeof modules_keys;
  const modules: Array<{
    init?: typeof Function;
  }> = [];
  //import("./example/counter/index");
  for (const [categoryKey, categoryValue] of Object.entries(MODULES)) {
    for (const moduleName in categoryValue) {
      (async () => {
        try {
          if (
            categoryKey in enabled_features &&
            enabled_features[
              categoryKey as keyof typeof enabled_features
            ].includes(moduleName)
          )
            modules.push(
              (await categoryValue[moduleName]()) as {
                init?: typeof Function;
              },
            );
        } catch (e) {
          console.error(e);
        }
      })();
    }
  }
  window.addEventListener("DOMContentLoaded", () => {
    modules.forEach((m) => {
      try {
        m?.init?.();
      } catch {}
    });
  });
  Services.obs.addObserver(() => {}, "browser-delayed-startup-finished");
  //@ts-expect-error ii
  SessionStore.promiseInitialized.then(async () => {
    //CustomShortcutKey.getInstance();
  });
}
