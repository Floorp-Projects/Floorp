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
  // Import required modules and initialize i18n
  ChromeUtils.importESModule("resource://noraneko/modules/BrowserGlue.sys.mjs");
  initI18N();

  // Set up preferences for features
  const prefs = Services.prefs.getDefaultBranch(null as unknown as string);
  prefs.setStringPref("noraneko.features.all", JSON.stringify(modules_keys));
  Services.prefs.lockPref("noraneko.features.all");
  prefs.setStringPref(
    "noraneko.features.enabled",
    JSON.stringify(modules_keys),
  );

  // Get enabled features from preferences
  const enabled_features = JSON.parse(
    Services.prefs.getStringPref("noraneko.features.enabled", "{}"),
  ) as typeof modules_keys;

  // Load enabled modules
  const modules = await loadEnabledModules(enabled_features);

  // Initialize modules after session is ready
  await initializeModules(modules);
}

async function loadEnabledModules(enabled_features: typeof modules_keys) {
  const modules: Array<{ init?: typeof Function }> = [];

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
            modules.push(module as { init?: typeof Function });
          } catch (e) {
            console.error(`Failed to load module ${moduleName}:`, e);
          }
        }
      }),
  );

  await Promise.all(loadModulePromises);
  return modules;
}

async function initializeModules(modules: Array<{ init?: typeof Function }>) {
  // @ts-expect-error SessionStore type not defined
  await SessionStore.promiseInitialized;

  modules.forEach((module) => {
    try {
      module?.init?.();
    } catch {}
  });
}
