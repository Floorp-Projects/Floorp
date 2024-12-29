import i18next from "i18next";

const _modules = import.meta.glob("./*/*.json",{eager:true});

const modules : Record<string, Record<string,object>>= {};
for (const [idx,m] of Object.entries(_modules)) {
  const [lng,ns] = idx.replaceAll("./","").replaceAll(".json","").split("/");
  if (!Object.hasOwn(modules,lng)) {
    modules[lng] = {}
  }
  modules[lng][ns] = (m as any).default as object;
}

import { createEffect, createSignal } from "solid-js";

export const defaultNS = "default";

export const resources = modules;
export function initI18N() {
  i18next.init({
    lng: "en-US",
    debug: true,
    resources,
    defaultNS,
    ns: ["undo"],
    fallbackLng: ["en-US", "dev"],
  });
}
const [lang, setLang] = createSignal("ja-JP");

/**
 *
 * @param observer
 * @description For HMR, please run this function in `createRootHMR`
 * @example
 * ```ts
 * import { createRootHMR } from "@nora/solid-xul";
 *
 * createRootHMR(
 *   () => {
 *     addI18nObserver(observer);
 *   },
 *   import.meta.hot
 * );
 * ```
 */
export function addI18nObserver(observer: (locale: string) => void) {
  createEffect(() => {
    observer(lang());
  });
}
