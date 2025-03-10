import { createRootHMR } from "@nora/solid-xul";
import i18next from "i18next";

const _modules = import.meta.glob("./*/*.json", { eager: true });

const modules: Record<string, Record<string, object>> = {};
for (const [idx, m] of Object.entries(_modules)) {
  const [lng, ns] = idx.replaceAll("./", "").replaceAll(".json", "").split("/");
  if (!Object.hasOwn(modules, lng)) {
    modules[lng] = {};
  }
  modules[lng][ns] = (m as any).default as object;
}

import { createEffect, createSignal } from "solid-js";

export const defaultNS = "default";

export const resources = modules;

export const languageMappings: Record<string, string> = {
  "ja": "ja-JP",
  "en": "en-US",
  "fr": "fr-FR",
  "de": "de-DE",
  "zh": "zh-CN",
  "es": "es-ES",
  "it": "it-IT",
  "ru": "ru-RU",
  "pt": "pt-BR",
  "nl": "nl-NL",
  "pl": "pl-PL",
  "ko": "ko-KR",
};

export function initI18N() {
  i18next.init({
    lng: "en-US",
    debug: true,
    resources,
    defaultNS,
    ns: ["undo"],
    fallbackLng: ["en-US", "dev"],

    supportedLngs: Object.values(languageMappings).concat(
      Object.keys(languageMappings),
    ),
    nonExplicitSupportedLngs: true,
    load: "currentOnly",

    interpolation: {
      escapeValue: false,
    },
  });

  const originalLookupFunction = i18next.services.languageUtils.lookup;
  i18next.services.languageUtils.lookup = function (
    code: string | number,
    ...args: Parameters<typeof originalLookupFunction>
  ) {
    const mappedCode = languageMappings[code] || code;
    return originalLookupFunction.call(this, mappedCode, ...args);
  };
}

const [lang, setLang] = createRootHMR(
  () => createSignal(getRequestedLang()),
  import.meta.hot,
);

export function getRequestedLang() {
  const requestedLang =
    Services.prefs.getCharPref("intl.locale.requested", "ja-JP").split(",")[0];
  return languageMappings[requestedLang] || requestedLang;
}

Services.prefs.addObserver("intl.locale.requested", {
  observe(_subject, _topic, _data) {
    setLang(getRequestedLang());
  },
});

export function addI18nObserver(observer: (locale: string) => void) {
  createEffect(() => {
    const currentLang = lang();
    i18next.changeLanguage(currentLang);
    observer(currentLang);
  });
}
