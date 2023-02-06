/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The following globals are injected via the AboutTranslationsChild actor.
// translations.mjs is running in an unprivileged context, and these injected functions
// allow for the page to get access to additional privileged features.

/* global AT_getSupportedLanguages, AT_log, AT_getScriptDirection,
   AT_getAppLocale, AT_logError, AT_destroyEngine,
   AT_createTranslationsEngine, AT_translate */

/**
 * The model and controller for initializing about:translations.
 */
class TranslationsState {
  /**
   * This class is responsible for all UI updated.
   *
   * @type {TranslationsUI}
   */
  ui;

  /**
   * The language to translate from, in the form of a BCP 47 language tag,
   * e.g. "en" or "fr".
   *
   * @type {string}
   */
  fromLanguage = "";

  /**
   * The language to translate to, in the form of a BCP 47 language tag,
   * e.g. "en" or "fr".
   *
   * @type {string}
   */
  toLanguage = "";

  /**
   * The message to translate, cached so that it can be determined if the text
   * needs to be re-translated.
   *
   * @type {string}
   */
  messageToTranslate = "";

  /**
   * The translations engine is only valid for a single language pair, and needs
   * to be recreated if the language pair changes.
   *
   * @type {null | Promise<TranslationsEngine>}
   */
  engine = null;

  constructor() {
    this.supportedLanguages = AT_getSupportedLanguages();
    this.ui = new TranslationsUI(this);
    this.ui.setup();
  }

  /**
   * Only request translation when it's needed.
   */
  async maybeRequestTranslation() {
    // The contents of "this" can change between async steps, store a local variable
    // binding of these values.
    const { fromLanguage, toLanguage, messageToTranslate, engine } = this;

    if (!fromLanguage || !toLanguage || !messageToTranslate || !engine) {
      // Not everything is set for translation.
      this.ui.updateTranslation("");
      return;
    }

    // Ensure the engine is ready to go.
    await engine;

    // Check if the configuration has changed between each async step.
    const isStale = () =>
      this.engine !== engine ||
      this.fromLanguage !== fromLanguage ||
      this.toLanguage !== toLanguage ||
      this.messageToTranslate !== messageToTranslate;

    if (isStale()) {
      return;
    }

    const start = performance.now();
    const [translation] = await AT_translate([this.messageToTranslate]);

    if (isStale()) {
      return;
    }

    this.ui.updateTranslation(translation);
    const duration = performance.now() - start;
    AT_log(`Translation done in ${duration / 1000} seconds`);
  }

  /**
   * Any time a language pair is changed, the TranslationsEngine needs to be rebuilt.
   */
  async maybeRebuildWorker() {
    // If we may need to re-building the worker, the old translation is no longer valid.
    this.ui.updateTranslation("");

    if (!this.fromLanguage || !this.toLanguage) {
      // A from or to language could have been removed. Don't do any more translations
      // with it.
      if (this.engine) {
        // The engine is no longer needed.
        AT_destroyEngine();
        this.engine = null;
      }
      return;
    }

    const start = performance.now();
    AT_log(
      `Rebuilding the translations worker for "${this.fromLanguage}" to "${this.toLanguage}"`
    );

    this.engine = AT_createTranslationsEngine(
      this.fromLanguage,
      this.toLanguage
    );
    this.maybeRequestTranslation();

    try {
      await this.engine;
      const duration = performance.now() - start;
      AT_log(`Rebuilt the TranslationsEngine in ${duration / 1000} seconds`);
      // TODO (Bug 1813781) - Report this error in the UI.
    } catch (error) {
      AT_logError("Failed to get the Translations worker", error);
    }
  }

  /**
   * @param {string} lang
   */
  setFromLanguage(lang) {
    if (lang !== this.fromLanguage) {
      this.fromLanguage = lang;
      this.maybeRebuildWorker();
    }
  }

  /**
   * @param {string} lang
   */
  setToLanguage(lang) {
    if (lang !== this.toLanguage) {
      this.toLanguage = lang;
      this.maybeRebuildWorker();
    }
  }

  /**
   * @param {string} message
   */
  setMessageToTranslate(message) {
    if (message !== this.messageToTranslate) {
      this.messageToTranslate = message;
      this.maybeRequestTranslation();
    }
  }
}

/**
 *
 */
class TranslationsUI {
  /** @type {HTMLSelectElement} */
  languageFrom = document.getElementById("language-from");
  /** @type {HTMLSelectElement} */
  languageTo = document.getElementById("language-to");
  /** @type {HTMLTextAreaElement} */
  translationFrom = document.getElementById("translation-from");
  /** @type {HTMLDivElement} */
  translationTo = document.getElementById("translation-to");
  /** @type {HTMLDivElement} */
  translationToBlank = document.getElementById("translation-to-blank");
  /** @type {TranslationsState} */
  state;

  /**
   * @param {TranslationsState} state
   */
  constructor(state) {
    this.state = state;
    this.translationTo.style.visibility = "visible";
  }

  /**
   * Do the initial setup.
   */
  setup() {
    this.setupDropdowns();
    this.setupTextarea();
  }

  /**
   * Once the models have been synced from remote settings, populate them with the display
   * names of the languages.
   */
  async setupDropdowns() {
    const supportedLanguages = await this.state.supportedLanguages;
    // Update the DOM elements with the display names.
    for (const { langTag, displayName } of supportedLanguages) {
      let option = document.createElement("option");
      option.value = langTag;
      option.text = displayName;
      this.languageTo.add(option);

      option = document.createElement("option");
      option.value = langTag;
      option.text = displayName;
      this.languageFrom.add(option);
    }

    // Set the translate "from" to the app locale, if it is in the list.
    const appLocale = new Intl.Locale(AT_getAppLocale());
    for (const option of this.languageFrom.options) {
      if (option.value === appLocale.language) {
        this.languageFrom.value = option.value;
        break;
      }
    }

    // Enable the controls.
    this.languageFrom.disabled = false;
    this.languageTo.disabled = false;

    // Focus the language dropdowns if they are empty.
    if (this.languageFrom.value == "") {
      this.languageFrom.focus();
    } else if (this.languageTo.value == "") {
      this.languageTo.focus();
    }

    this.state.setFromLanguage(this.languageFrom.value);
    this.state.setToLanguage(this.languageTo.value);
    this.updateOnLanguageChange();

    this.languageFrom.addEventListener("input", () => {
      this.state.setFromLanguage(this.languageFrom.value);
      this.updateOnLanguageChange();
    });

    this.languageTo.addEventListener("input", () => {
      this.state.setToLanguage(this.languageTo.value);
      this.updateOnLanguageChange();
    });
  }

  /**
   * React to language changes.
   */
  updateOnLanguageChange() {
    this.#updateDropdownLanguages();
    this.#updateMessageDirections();
  }

  /**
   * You cant translate from one language to another language. Hide the options
   * if this is the case.
   */
  #updateDropdownLanguages() {
    for (const option of this.languageFrom.options) {
      option.hidden = false;
    }
    for (const option of this.languageTo.options) {
      option.hidden = false;
    }
    if (this.state.toLanguage) {
      const option = this.languageFrom.querySelector(
        `[value=${this.state.toLanguage}]`
      );
      if (option) {
        option.hidden = true;
      }
    }
    if (this.state.fromLanguage) {
      const option = this.languageTo.querySelector(
        `[value=${this.state.fromLanguage}]`
      );
      if (option) {
        option.hidden = true;
      }
    }
  }

  /**
   * TODO (Bug 1813783) - This needs automated testing.
   *
   * Define the direction of the language message text, otherwise it might not display
   * correctly. For instance English in an RTL UI would display incorrectly like so:
   *
   * LTR text in LTR UI:
   *
   * ┌──────────────────────────────────────────────┐
   * │ This is in English.                          │
   * └──────────────────────────────────────────────┘
   *
   * LTR text in RTL UI:
   * ┌──────────────────────────────────────────────┐
   * │                          .This is in English │
   * └──────────────────────────────────────────────┘
   *
   * LTR text in RTL UI, but in an LTR container:
   * ┌──────────────────────────────────────────────┐
   * │ This is in English.                          │
   * └──────────────────────────────────────────────┘
   *
   * The effects are similar, but reversed for RTL text in an LTR UI.
   */
  #updateMessageDirections() {
    if (this.state.toLanguage) {
      this.translationTo.setAttribute(
        "dir",
        AT_getScriptDirection(this.state.toLanguage)
      );
    } else {
      this.translationTo.removeAttribute("dir");
    }
    if (this.state.fromLanguage) {
      this.translationFrom.setAttribute(
        "dir",
        AT_getScriptDirection(this.state.fromLanguage)
      );
    } else {
      this.translationFrom.removeAttribute("dir");
    }
  }

  setupTextarea() {
    this.state.setMessageToTranslate(this.translationFrom.value);
    this.translationFrom.addEventListener("input", () => {
      this.state.setMessageToTranslate(this.translationFrom.value);
    });
  }

  /**
   * @param {string} message
   */
  updateTranslation(message) {
    this.translationTo.innerText = message;
    if (message) {
      this.translationTo.style.visibility = "visible";
      this.translationToBlank.style.visibility = "hidden";
    } else {
      this.translationTo.style.visibility = "hidden";
      this.translationToBlank.style.visibility = "visible";
    }
  }
}

/**
 * Listen for events coming from the AboutTranslations actor.
 */
window.addEventListener("AboutTranslationsChromeToContent", ({ detail }) => {
  switch (detail.type) {
    case "enable": {
      // While the feature is in development, hide the feature behind a pref. See the
      // "browser.translations.enable" pref in modules/libpref/init/all.js and Bug 971044
      // for the status of enabling this project.
      if (window.translationsState) {
        throw new Error("about:translations was already initialized.");
      }
      window.translationsState = new TranslationsState();
      document.body.style.visibility = "visible";
      break;
    }
    default:
      throw new Error("Unknown AboutTranslationsChromeToContent event.");
  }
});
