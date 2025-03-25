import i18next from "i18next";

const _modules = import.meta.glob("./locales/*.json", { eager: true });
const modules: Record<string, Record<string, object>> = {};
for (const [idx, m] of Object.entries(_modules)) {
  const lng = idx.replaceAll("./locales/", "").replaceAll(".json", "");
  if (!Object.hasOwn(modules, lng)) {
    modules[lng] = {};
  }
  modules[lng]["translation"] = (m as any).default as object;
}

export const resources = modules;

// This is a mapping of language codes to i18next language codes.
// If you redirect a language code to another language code, you should add it here.
export const languageMappings: Record<string, string> = {
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
  "ja-jp": "ja",
  "ja-JP": "ja",
};

export function initI18N() {
  const availableLangs = Object.keys(resources);

  i18next.init({
    lng: "en-US",
    debug: true,
    resources,
    fallbackLng: ["en-US"],
    supportedLngs: [
      ...new Set([
        ...Object.values(languageMappings),
        ...Object.keys(languageMappings),
        ...availableLangs,
      ]),
    ],
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
    if (
      Object.hasOwn(languageMappings, code) && languageMappings[code] !== code
    ) {
      return originalLookupFunction.call(this, languageMappings[code], ...args);
    }
    return originalLookupFunction.call(this, code, ...args);
  };
}

export function getRequestedLang() {
  const requestedLang =
    Services.prefs.getStringPref("intl.locale.requested", "en-US").split(
      ",",
    )[0];
  return languageMappings[requestedLang] || requestedLang;
}

export function addI18nObserver(observer: () => void) {
  observeHandler(observer);
  Services.prefs.addObserver("intl.locale.requested", {
    observe(_subject, _topic, _data) {
      observeHandler(observer);
    },
  });
}

function observeHandler(observer: () => void) {
  const currentLang = getRequestedLang();
  i18next.changeLanguage(currentLang).then(() => {
    observer();
  });
}
