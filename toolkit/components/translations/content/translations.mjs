/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The following globals are injected via the AboutTranslationsChild actor.
// translations.mjs is running in an unprivileged context, and these injected functions
// allow for the page to get access to additional privileged features.

/* global AT_getSupportedLanguages, AT_log, AT_getScriptDirection,
   AT_getAppLocale, AT_logError, AT_destroyTranslationsEngine,
   AT_createTranslationsEngine, AT_createLanguageIdEngine,
   AT_translate, AT_identifyLanguage */

// Allow tests to override this value so that they can run faster.
// This is the delay in milliseconds.
window.DEBOUNCE_DELAY = 200;
// Allow tests to test the debounce behavior by counting debounce runs.
window.DEBOUNCE_RUN_COUNT = 0;

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
   * Only send one translation in at a time to the worker.
   * @type {Promise<string[]>}
   */
  translationRequest = Promise.resolve([]);

  /**
   * The translations engine is only valid for a single language pair, and needs
   * to be recreated if the language pair changes.
   *
   * @type {null | Promise<TranslationsEngine>}
   */
  translationsEngine = null;

  constructor() {
    this.supportedLanguages = AT_getSupportedLanguages();
    this.ui = new TranslationsUI(this);
    this.ui.setup();
  }

  /**
   * Identifies the human language in which the message is written and logs the result.
   *
   * @param {string} message
   */
  async identifyLanguage(message) {
    const start = performance.now();
    const { languageLabel, confidence } = await AT_identifyLanguage(message);
    const duration = performance.now() - start;
    AT_log(
      `[ ${languageLabel.slice(-2)}(${(confidence * 100).toFixed(2)}%) ]`,
      `Source language identified in ${duration / 1000} seconds`
    );
  }

  /**
   * Only request a translation when it's ready.
   */
  maybeRequestTranslation = debounce({
    /**
     * Debounce the translation requests so that the worker doesn't fire for every
     * single keyboard input, but instead the keyboard events are ignored until
     * there is a short break, or enough events have happened that it's worth sending
     * in a new translation request.
     */
    onDebounce: async () => {
      // The contents of "this" can change between async steps, store a local variable
      // binding of these values.
      const {
        fromLanguage,
        toLanguage,
        messageToTranslate,
        translationsEngine,
      } = this;

      if (
        !fromLanguage ||
        !toLanguage ||
        !messageToTranslate ||
        !translationsEngine
      ) {
        // Not everything is set for translation.
        this.ui.updateTranslation("");
        return;
      }

      await Promise.all([
        // Ensure the engine is ready to go.
        translationsEngine,
        // Ensure the previous translation has finished so that only the latest
        // translation goes through.
        this.translationRequest,
      ]);

      if (
        // Check if the current configuration has changed and if this is stale. If so
        // then skip this request, as there is already a newer request with more up to
        // date information.
        this.translationsEngine !== translationsEngine ||
        this.fromLanguage !== fromLanguage ||
        this.toLanguage !== toLanguage ||
        this.messageToTranslate !== messageToTranslate
      ) {
        return;
      }

      const start = performance.now();

      this.translationRequest = AT_translate([messageToTranslate]);
      const [translation] = await this.translationRequest;

      // The measure events will show up in the Firefox Profiler.
      performance.measure(
        `Translations: Translate "${this.fromLanguage}" to "${this.toLanguage}" with ${messageToTranslate.length} characters.`,
        {
          start,
          end: performance.now(),
        }
      );

      this.ui.updateTranslation(translation);
      const duration = performance.now() - start;
      AT_log(`Translation done in ${duration / 1000} seconds`);
    },

    // Mark the events so that they show up in the Firefox Profiler. This makes it handy
    // to visualize the debouncing behavior.
    doEveryTime: () => {
      performance.mark(
        `Translations: input changed to ${this.messageToTranslate.length} characters`
      );
    },
  });

  /**
   * Any time a language pair is changed, the TranslationsEngine needs to be rebuilt.
   */
  async maybeRebuildWorker() {
    // If we may need to re-building the worker, the old translation is no longer valid.
    this.ui.updateTranslation("");

    if (!this.fromLanguage || !this.toLanguage) {
      // A from or to language could have been removed. Don't do any more translations
      // with it.
      if (this.translationsEngine) {
        // The engine is no longer needed.
        AT_destroyTranslationsEngine();
        this.translationsEngine = null;
      }
      return;
    }

    const start = performance.now();
    AT_log(
      `Rebuilding the translations worker for "${this.fromLanguage}" to "${this.toLanguage}"`
    );

    this.translationsEngine = AT_createTranslationsEngine(
      this.fromLanguage,
      this.toLanguage
    );
    this.maybeRequestTranslation();

    try {
      await this.translationsEngine;
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
      this.identifyLanguage(message);
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
    const start = performance.now();
    AT_createLanguageIdEngine();
    const duration = performance.now() - start;
    AT_log(`Created LanguageIdEngine in ${duration / 1000} seconds`);
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
      this.translationTo.setAttribute("lang", this.languageTo.value);
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

/**
 * Debounce a function so that it is only called after some wait time with no activity.
 * This is good for grouping text entry via keyboard.
 *
 * @param {Object} settings
 * @param {Function} settings.onDebounce
 * @param {Function} settings.doEveryTime
 * @returns {Function}
 */
function debounce({ onDebounce, doEveryTime }) {
  /** @type {number | null} */
  let timeoutId = null;
  let lastDispatch = null;

  return (...args) => {
    doEveryTime(...args);

    const now = Date.now();
    if (lastDispatch === null) {
      // This is the first call to the function.
      lastDispatch = now;
    }

    const timeLeft = lastDispatch + window.DEBOUNCE_DELAY - now;

    // Always discard the old timeout, either the function will run, or a new
    // timer will be scheduled.
    clearTimeout(timeoutId);

    if (timeLeft <= 0) {
      // It's been long enough to go ahead and call the function.
      timeoutId = null;
      lastDispatch = null;
      window.DEBOUNCE_RUN_COUNT += 1;
      onDebounce(...args);
      return;
    }

    // Re-set the timeout with the current time left.
    clearTimeout(timeoutId);

    timeoutId = setTimeout(() => {
      // Timeout ended, call the function.
      timeoutId = null;
      lastDispatch = null;
      window.DEBOUNCE_RUN_COUNT += 1;
      onDebounce(...args);
    }, timeLeft);
  };
}
