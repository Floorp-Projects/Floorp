/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

ChromeUtils.defineESModuleGetters(lazy, {
  LanguageDetector:
    "resource://gre/modules/translation/LanguageDetector.sys.mjs",
});

/**
 * @typedef {import("./TranslationsChild.sys.mjs").TranslationsEngine} TranslationsEngine
 * @typedef {import("./TranslationsChild.sys.mjs").SupportedLanguages} SupportedLanguages
 */

/**
 * The AboutTranslationsChild is responsible for coordinating what privileged APIs
 * are exposed to the un-privileged scope of the about:translations page.
 */
export class AboutTranslationsChild extends JSWindowActorChild {
  /**
   * The translations engine uses text translations by default in about:translations,
   * but it can be changed to translate HTML by setting this pref to true. This is
   * useful for manually testing HTML translation behavior, but is not useful to surface
   * as a user-facing feature.
   *
   * @type {bool}
   */
  #isHtmlTranslation = Services.prefs.getBoolPref(
    "browser.translations.useHTML"
  );

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

  receiveMessage({ name, data }) {
    switch (name) {
      case "AboutTranslations:SendTranslationsPort":
        const { fromLanguage, toLanguage, port } = data;
        const transferables = [port];
        this.contentWindow.postMessage(
          {
            type: "GetTranslationsPort",
            fromLanguage,
            toLanguage,
            port,
          },
          "*",
          transferables
        );
        break;
      default:
        throw new Error("Unknown AboutTranslations message: " + name);
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
        let contentWindow;
        try {
          contentWindow = this.contentWindow;
        } catch (error) {
          // The content window is no longer available.
          reject();
          return;
        }
        // Create an error in the content window, if the content window is still around.
        let message = "An error occured in the AboutTranslations actor.";
        if (typeof error === "string") {
          message = error;
        }
        if (typeof error?.message === "string") {
          message = error.message;
        }
        if (typeof error?.stack === "string") {
          message += `\n\nOriginal stack:\n\n${error.stack}\n`;
        }

        reject(new contentWindow.Error(message));
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
      "AT_isTranslationEngineSupported",
      "AT_isHtmlTranslation",
      "AT_createTranslationsPort",
      "AT_identifyLanguage",
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
   * @returns {Promise<SupportedLanguages>}
   */
  AT_getSupportedLanguages() {
    return this.#convertToContentPromise(
      this.sendQuery("AboutTranslations:GetSupportedLanguages").then(data =>
        Cu.cloneInto(data, this.contentWindow)
      )
    );
  }

  /**
   * Does this device support the translation engine?
   * @returns {Promise<boolean>}
   */
  AT_isTranslationEngineSupported() {
    return this.#convertToContentPromise(
      this.sendQuery("AboutTranslations:IsTranslationsEngineSupported")
    );
  }

  /**
   * Expose the #isHtmlTranslation property.
   *
   * @returns {bool}
   */
  AT_isHtmlTranslation() {
    return this.#isHtmlTranslation;
  }

  /**
   * Requests a port to the TranslationsEngine process. An engine will be created on
   * the fly for translation requests through this port. This port is unique to its
   * language pair. In order to translate a different language pair, a new port must be
   * created for that pair. The lifecycle of the engine is managed by the
   * TranslationsEngine.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {void}
   */
  AT_createTranslationsPort(fromLanguage, toLanguage) {
    this.sendAsyncMessage("AboutTranslations:GetTranslationsPort", {
      fromLanguage,
      toLanguage,
    });
  }

  /**
   * Attempts to identify the human language in which the message is written.
   *
   * @param {string} message
   * @returns {Promise<{ langTag: string, confidence: number }>}
   */
  AT_identifyLanguage(message) {
    return this.#convertToContentPromise(
      lazy.LanguageDetector.detectLanguage(message).then(data =>
        Cu.cloneInto(
          // This language detector reports confidence as a boolean instead of
          // a percentage, so we need to map the confidence to 0.0 or 1.0.
          { langTag: data.language, confidence: data.confident ? 1.0 : 0.0 },
          this.contentWindow
        )
      )
    );
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
