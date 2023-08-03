/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  TranslationsEngine:
    "chrome://global/content/translations/translations-engine.sys.mjs",
  // The fastText languageIdEngine
  LanguageIdEngine:
    "chrome://global/content/translations/language-id-engine.sys.mjs",
  // The CLD2 language detector
  LanguageDetector:
    "resource://gre/modules/translation/LanguageDetector.sys.mjs",
});

/**
 * This file is extremely sensitive to memory size and performance!
 */
export class TranslationsChild extends JSWindowActorChild {
  /**
   * Store this since the window may be dead when the value is needed.
   * @type {number | null}
   */
  innerWindowId = null;
  #wasTranslationsEngineCreated = false;

  handleEvent(event) {
    switch (event.type) {
      case "DOMContentLoaded":
        this.innerWindowId =
          this.contentWindow?.windowGlobalChild.innerWindowId;
        this.sendAsyncMessage("Translations:ReportLangTags", {
          documentElementLang: this.document.documentElement.lang,
        });
        break;
    }
  }

  async receiveMessage({ name, data }) {
    switch (name) {
      case "Translations:TranslatePage": {
        lazy.TranslationsEngine.translatePage(this, data).then(
          () => {
            this.#wasTranslationsEngineCreated = true;
          },
          () => {
            this.sendAsyncMessage("Translations:FullPageTranslationFailed", {
              reason: "engine-load-failure",
            });
          }
        );
        return undefined;
      }
      case "Translations:GetDocumentElementLang":
        return this.document.documentElement.lang;
      case "Translations:IdentifyLanguage": {
        try {
          // Try to use the fastText engine if directed to do so.
          if (data.useFastText) {
            const engine = await this.getOrCreateLanguageIdEngine();
            if (!engine) {
              return null;
            }
            return engine.identifyLanguageFromDocument(this.document);
          }

          // Use the CLD2 language detector otherwise.
          return lazy.LanguageDetector.detectLanguageFromDocument(
            this.document
          );
        } catch (error) {
          return null;
        }
      }
      default:
        throw new Error("Unknown message.", name);
    }
  }

  sendTelemetryError(error) {
    const errorMessage = String(error);
    this.sendAsyncMessage("Translations:SendTelemetryError", { errorMessage });
  }

  getSupportedLanguages() {
    return this.sendQuery("Translations:GetSupportedLanguages");
  }

  sendEngineIsReady() {
    this.sendAsyncMessage("Translations:EngineIsReady");
  }

  isTranslationsEngineSupported() {
    return this.sendQuery("Translations:IsTranslationsEngineSupported");
  }

  getTranslationsEnginePayload(fromLanguage, toLanguage) {
    return this.sendQuery("Translations:GetTranslationsEnginePayload", {
      fromLanguage,
      toLanguage,
    });
  }

  getOrCreateLanguageIdEngine() {
    return lazy.LanguageIdEngine.getOrCreate(() => {
      if (!this.manager || !this.manager.isCurrentGlobal) {
        throw new Error("The page was already hidden.");
      }
      return this.sendQuery("Translations:GetLanguageIdEnginePayload");
    });
  }

  createTranslationsEngine(fromLanguage, toLanguage) {
    // Bypass the engine cache and always create a new one.
    return lazy.TranslationsEngine.create(this, fromLanguage, toLanguage);
  }

  didDestroy() {
    if (this.#wasTranslationsEngineCreated) {
      // Only run this if needed, as it will de-lazify the code.
      lazy.TranslationsEngine.discardTranslationQueue(
        this.manager.innerWindowId
      );
    }
  }
}
