import "vite/modulepreload-polyfill";

import { initI18N } from "../i18n/config";

const MODULES = {
  common: {} as Record<string, () => Promise<unknown>>,
  static: {} as Record<string, () => Promise<unknown>>
};

{
  const MODULES_COMMON = import.meta.glob("./common/*/index.ts");

  Object.entries(MODULES_COMMON).map((v) => {
    MODULES.common[v[0].replaceAll(/\.\/common\/([a-zA-Z-]+)\/index\.ts/g,"$1")] =
      v[1] as () => Promise<unknown>;
  });
}

{
  MODULES.static = {
    downloadbar: () => import("./static/downloadbar/index"),
    overrides: () => import("./static/overrides/index"),
    prefs: () => import("./static/prefs/index"),
  }
}

const modules_keys = {
  common: Object.keys(MODULES.common),
  static: Object.keys(MODULES.static),
};

export default async function initScripts() {
  // Import required modules and initialize i18n
  ChromeUtils.importESModule("resource://noraneko/modules/BrowserGlue.sys.mjs");
  initI18N();
  console.debug(`[noraneko-buildid2]\nuuid: ${import.meta.env.__BUILDID2__}\ndate: ${new Date(Number.parseInt(import.meta.env.__BUILDID2__.slice(0, 13).replace("-", ""), 16)).toISOString()}`);

  setPrefFeatures(modules_keys)

  // Get enabled features from preferences
  const enabled_features = JSON.parse(
    Services.prefs.getStringPref("noraneko.features.enabled", "{}"),
  ) as typeof modules_keys;

  // Load enabled modules
  const modules = await loadEnabledModules(enabled_features);

  // Initialize modules after session is ready
  await initializeModules(modules);
}

async function setPrefFeatures(all_features_keys: typeof modules_keys) {
  // Set up preferences for features
  const prefs = Services.prefs.getDefaultBranch(null as unknown as string);
  prefs.setStringPref("noraneko.features.all", JSON.stringify(all_features_keys));
  Services.prefs.lockPref("noraneko.features.all");

  prefs.setStringPref(
    "noraneko.features.enabled",
    JSON.stringify(all_features_keys),
  );

  if (import.meta.env.DEV && false) {
    prefs.setStringPref(
      "noraneko.features.enabled",
      JSON.stringify(
        {
          common:[
            "browser-share-mode",
            "browser-tab-color",
            "context-menu",
            "designs",
            "flex-order",
            "panel-sidebar",
            "private-container",
            "profile-manager",
            "splitView",
            "statusbar",
            "tab",
            "tabbar",
            "undo-closed-tab",
            "update-refresh-cache",
            "workspaces"
          ],
          static:["downloadbar","overrides"]
        }
      ),
    );
  }
}

async function loadEnabledModules(enabled_features: typeof modules_keys) {
  const modules: Array<{ init?: typeof Function, name: string }> = [];

  const loadModulePromises = Object.entries(MODULES).flatMap(
    ([categoryKey, categoryValue]) =>
      Object.keys(categoryValue).map(async (moduleName) => {
        if (
          categoryKey in enabled_features &&
          enabled_features[
            categoryKey as keyof typeof enabled_features
          ].includes(moduleName)
        ) {
          try {
            const module = await categoryValue[moduleName]();
            modules.push(Object.assign({name: moduleName},module as { init?: typeof Function, initBeforeSessionStoreInit?: typeof Function }));
          } catch (e) {
            console.error(`[noraneko] Failed to load module ${moduleName}:`, e);
          }
        }
      }),
  );

  await Promise.all(loadModulePromises);
  return modules;
}

async function initializeModules(modules: Array<{ init?: typeof Function, initBeforeSessionStoreInit?:typeof Function, name:string }>) {
  for (const module of modules) {
    try {
      await module?.initBeforeSessionStoreInit?.();
    }catch(e) {
      console.error(`[noraneko] Failed to initBeforeSessionStoreInit module ${module.name}:`, e);
    }
  }
  // @ts-expect-error SessionStore type not defined
  await SessionStore.promiseInitialized;

  for (const module of modules) {
    try {
      await module?.init?.();
      if (module && module.default) {
        new module.default();
      } else {
        // import.meta.hot?.accept(module?.name,async(m)=>{
        //   await m?.init?.();
        // });
      }
    } catch(e) {
      console.error(`[noraneko] Failed to init module ${module.name}:`, e);
    }
  }
}
