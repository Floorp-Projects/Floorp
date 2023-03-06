/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

/**
 * @typedef {import("./TranslationsChild.sys.mjs").LanguageIdEngine} LanguageIdEngine
 * @typedef {import("./TranslationsChild.sys.mjs").TranslationsEngine} TranslationsEngine
 */

/**
 * The AboutTranslationsChild is responsible for coordinating what privileged APIs
 * are exposed to the un-privileged scope of the about:translations page.
 */
export class AboutTranslationsChild extends JSWindowActorChild {
  /** @type {LanguageIdEngine | null} */
  languageIdEngine = null;

  /** @type {TranslationsEngine | null} */
  translationsEngine = null;

  handleEvent(event) {
    if (event.type === "DOMDocElementInserted") {
      this.#exportFunctions();
    }

    if (
      event.type === "DOMContentLoaded" &&
      Services.prefs.getBoolPref("browser.translations.enable")
    ) {
      this.#sendEventToContent({ type: "enable" });
    }
  }

  /**
   * @param {object} detail
   */
  #sendEventToContent(detail) {
    this.contentWindow.dispatchEvent(
      new this.contentWindow.CustomEvent("AboutTranslationsChromeToContent", {
        detail: Cu.cloneInto(detail, this.contentWindow),
      })
    );
  }

  /**
   * @returns {TranslationsChild}
   */
  #getTranslationsChild() {
    const child = this.contentWindow.windowGlobalChild.getActor("Translations");
    if (!child) {
      throw new Error("Unable to find the TranslationsChild");
    }
    return child;
  }

  /**
   * A privileged promise can't be used in the content page, so convert a privileged
   * promise into a content one.
   *
   * @param {Promise<any>} promise
   * @returns {Promise<any>}
   */
  #convertToContentPromise(promise) {
    return new this.contentWindow.Promise((resolve, reject) =>
      promise.then(resolve, error => {
        // Create an error in the content window, if the content window is still around.
        if (this.contentWindow) {
          let message = "An error occured in the AboutTranslations actor.";
          if (typeof error?.message === "string") {
            message = error.message;
          }
          if (typeof error?.stack === "string") {
            message += `\n\nOriginal stack:\n\n${error.stack}\n`;
          }

          reject(new this.contentWindow.Error(message));
        } else {
          reject();
        }
      })
    );
  }

  /**
   * Export any of the child functions that start with "AT_" to the unprivileged content
   * page. This restricts the security capabilities of the the content page.
   */
  #exportFunctions() {
    const window = this.contentWindow;

    const fns = [
      "AT_log",
      "AT_logError",
      "AT_getAppLocale",
      "AT_getSupportedLanguages",
      "AT_createLanguageIdEngine",
      "AT_createTranslationsEngine",
      "AT_identifyLanguage",
      "AT_translateText",
      "AT_destroyTranslationsEngine",
      "AT_getScriptDirection",
    ];
    for (const name of fns) {
      Cu.exportFunction(this[name].bind(this), window, { defineAs: name });
    }
  }

  /**
   * Log messages if "browser.translations.logLevel" is set to "All".
   *
   * @param {...any} args
   */
  AT_log(...args) {
    lazy.console.log(...args);
  }

  /**
   * Report an error to the console.
   *
   * @param {...any} args
   */
  AT_logError(...args) {
    lazy.console.error(...args);
  }

  /**
   * Returns the app's locale.
   *
   * @returns {Intl.Locale}
   */
  AT_getAppLocale() {
    return Services.locale.appLocaleAsBCP47;
  }

  /**
   * Wire this function to the TranslationsChild.
   *
   * @returns {Promise<Array<{ langTag: string, displayName }>>}
   */
  AT_getSupportedLanguages() {
    return this.#convertToContentPromise(
      this.#getTranslationsChild()
        .getSupportedLanguages()
        .then(data => Cu.cloneInto(data, this.contentWindow))
    );
  }

  /**
   * Creates the LanguageIdEngine which attempts to identify in which
   * human language a string is written.
   *
   * Unlike TranslationsEngine, which handles only a single language pair
   * and must be rebuilt to handle a new language pair, the LanguageIdEngine
   * is a one-to-many engine that can recognize all of its supported languages.
   *
   * Subsequent calls to this function after the engine is initialized will do nothing
   * instead of rebuilding the engine.
   *
   * @returns {Promise<void>}
   */
  AT_createLanguageIdEngine() {
    if (this.languageIdEngine) {
      return this.#convertToContentPromise(Promise.resolve());
    }
    return this.#convertToContentPromise(
      this.#getTranslationsChild()
        .createLanguageIdEngine()
        .then(engine => {
          this.languageIdEngine = engine;
        })
    );
  }

  /**
   * Creates the TranslationsEngine which is responsible for translating
   * from one language to the other.
   *
   * The instantiated TranslationsEngine is unique to its language pair.
   * In order to translate a different language pair, a new engine must be
   * created for that pair.
   *
   * Subsequent calls to this function will destroy the existing engine and
   * rebuild a new engine for the new language pair.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {Promise<void>}
   */
  AT_createTranslationsEngine(fromLanguage, toLanguage) {
    if (this.translationsEngine) {
      this.translationsEngine.terminate();
      this.translationsEngine = null;
    }
    return this.#convertToContentPromise(
      this.#getTranslationsChild()
        .createTranslationsEngine(fromLanguage, toLanguage)
        .then(engine => {
          this.translationsEngine = engine;
        })
    );
  }

  /**
   * Attempts to identify the human language in which the message is written.
   * @see LanguageIdEngine#identifyLanguage for more detailed documentation.
   *
   * @param {string} message
   * @returns {Promise<{ languageLabel: string, confidence: number }>}
   */
  AT_identifyLanguage(message) {
    if (!this.languageIdEngine) {
      return this.#convertToContentPromise(
        Promise.reject("The language identification was not created.")
      );
    }
    return this.#convertToContentPromise(
      this.languageIdEngine
        .identifyLanguage(message)
        .then(data => Cu.cloneInto(data, this.contentWindow))
    );
  }

  /**
   * @param {string[]} messageBatch
   * @returns {Promise<string[]>}
   */
  AT_translate(messageBatch) {
    if (!this.translationsEngine) {
      throw new this.contentWindow.Error(
        "The translations engine was not created."
      );
    }
    return this.#convertToContentPromise(
      this.translationsEngine
        .translateText(messageBatch)
        .then(translations => Cu.cloneInto(translations, this.contentWindow))
    );
  }

  /**
   * This is not strictly necessary, but could free up resources quicker.
   */
  AT_destroyTranslationsEngine() {
    if (this.translationsEngine) {
      this.translationsEngine.terminate();
      this.translationsEngine = null;
    }
  }

  /**
   * TODO - Remove this when Intl.Locale.prototype.textInfo is available to
   * content scripts.
   *
   * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Intl/Locale/textInfo
   * https://bugzilla.mozilla.org/show_bug.cgi?id=1693576
   *
   * @param {string} locale
   * @returns {string}
   */
  AT_getScriptDirection(locale) {
    return Services.intl.getScriptDirection(locale);
  }
}
