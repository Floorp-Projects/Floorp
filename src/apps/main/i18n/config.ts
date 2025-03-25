import i18next from "i18next";
import { LANGUAGE_MAPPINGS } from "../../i18n-supports/languageMappings.ts";

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

export function initI18N() {
  const availableLangs = Object.keys(resources);

  i18next.init({
    lng: "en-US",
    debug: false,
    resources,
    fallbackLng: ["en-US"],
    supportedLngs: [
      ...new Set([
        ...Object.values(LANGUAGE_MAPPINGS),
        ...Object.keys(LANGUAGE_MAPPINGS),
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
      Object.hasOwn(LANGUAGE_MAPPINGS, code) && LANGUAGE_MAPPINGS[code] !== code
    ) {
      return originalLookupFunction.call(
        this,
        LANGUAGE_MAPPINGS[code],
        ...args,
      );
    }
    return originalLookupFunction.call(this, code, ...args);
  };
}

export function getRequestedLang() {
  const requestedLang =
    Services.prefs.getStringPref("intl.locale.requested", "en-US").split(
      ",",
    )[0];
  return LANGUAGE_MAPPINGS[requestedLang] || requestedLang;
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
