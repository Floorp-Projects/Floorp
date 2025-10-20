import { LANGUAGE_MAPPINGS } from "./languageMappings.ts";

export interface I18nModules {
  i18next: any;
  initReactI18next: any;
  LanguageDetector: any;
}

export interface I18nConfig {
  translations: Record<string, any>;
  fallbackLng?: string;
  debug?: boolean;
  defaultNS?: string;
  productName?: string;
}

export function initializeI18n(modules: I18nModules, config: I18nConfig) {
  const { i18next, initReactI18next, LanguageDetector } = modules;
  const {
    translations,
    fallbackLng = "en-US",
    debug = false,
    defaultNS = "translations",
    productName = "Floorp",
  } = config;

  i18next.on("languageChanged", function (lng: string) {
    if (LANGUAGE_MAPPINGS[lng]) {
      i18next.changeLanguage(LANGUAGE_MAPPINGS[lng]);
    }
  });

  const jsonLanguages = Object.keys(translations).map((path) => {
    return path.match(/locales\/(.*)\.json/)![1];
  });

  const supportedLanguages = [
    ...Object.keys(LANGUAGE_MAPPINGS),
    ...Object.values(LANGUAGE_MAPPINGS),
    ...jsonLanguages,
  ].filter((value, index, self) => self.indexOf(value) === index);

  i18next
    .use(LanguageDetector)
    .use(initReactI18next)
    .init({
      fallbackLng,
      debug,
      defaultNS,
      detection: {
        order: ["navigator", "querystring", "htmlTag"],
        caches: [],
      },
      interpolation: {
        escapeValue: false,
        defaultVariables: {
          productName,
        },
      },
      react: {
        useSuspense: false,
      },
      supportedLngs: supportedLanguages,
    });

  for (const [path, resources] of Object.entries(translations)) {
    const lng = path.match(/locales\/(.*)\.json/)![1];
    i18next.addResourceBundle(lng, defaultNS, resources, true, true);
  }

  return i18next;
}
