import i18n from "i18next";
import { initReactI18next } from "react-i18next";
import LanguageDetector from "i18next-browser-languagedetector";

const translations = import.meta.glob("../locales/*/translation.json",{eager:true,import:"default"})

i18n
  .use(LanguageDetector)
  .use(initReactI18next)
  .init({
    fallbackLng: "en",
    debug: false,
    "defaultNS":"translations",
    detection: {
      order: ["navigator", "querystring", "cookie", "localStorage", "htmlTag"],
      caches: [],
    },
    interpolation: {
      escapeValue: false,
      defaultVariables: {
        productName: "Noraneko",
      },
    },
    react: {
      useSuspense: false,
    },
  });
for (const [path,resources] of Object.entries(translations)) {
  const lng = /\.\.\/locales\/([a-zA-Z\-]+)\/translation.json/g.exec(path)![1]!;
  i18n.addResourceBundle(lng,"translations",resources,true,true);
}

export default i18n;
