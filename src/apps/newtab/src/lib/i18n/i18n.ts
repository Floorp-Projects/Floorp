import i18n from "i18next";
import { initReactI18next } from "react-i18next";
import LanguageDetector from "i18next-browser-languagedetector";
import { LANGUAGE_MAPPINGS } from "../../../../i18n-supports/languageMappings.ts";

const translations = import.meta.glob("./locales/*.json", {
  eager: true,
  import: "default",
});

i18n.on("languageChanged", function (lng) {
  if (LANGUAGE_MAPPINGS[lng]) {
    i18n.changeLanguage(LANGUAGE_MAPPINGS[lng]);
  }
});

i18n
  .use(LanguageDetector)
  .use(initReactI18next)
  .init({
    fallbackLng: "en-US",
    debug: false,
    "defaultNS": "translations",
    detection: {
      order: ["navigator", "querystring", "htmlTag"],
      caches: [],
    },
    interpolation: {
      escapeValue: false,
      defaultVariables: {
        productName: "Floorp",
      },
    },
    react: {
      useSuspense: false,
    },
  });

for (const [path, resources] of Object.entries(translations)) {
  const lng = path.match(/locales\/(.*)\.json/)![1];
  i18n.addResourceBundle(lng, "translations", resources, true, true);
}

export default i18n;
