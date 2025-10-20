import i18n from "i18next";
import { initReactI18next } from "react-i18next";
import LanguageDetector from "i18next-browser-languagedetector";
import { initializeI18n } from "./i18n-shared.ts";

const translations = import.meta.glob("./locales/*.json", {
  eager: true,
  import: "default",
});

export default initializeI18n(
  {
    i18next: i18n,
    initReactI18next,
    LanguageDetector,
  },
  {
    translations,
    productName: "Floorp",
  },
);
