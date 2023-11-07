/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The following globals are injected via the AboutTranslationsChild actor.
// translations.mjs is running in an unprivileged context, and these injected functions
// allow for the page to get access to additional privileged features.

/* global AT_getSupportedLanguages, AT_log, AT_getScriptDirection,
   AT_logError, AT_createTranslationsPort, AT_isHtmlTranslation,
   AT_isTranslationEngineSupported, AT_identifyLanguage */

// Allow tests to override this value so that they can run faster.
// This is the delay in milliseconds.
window.DEBOUNCE_DELAY = 200;
// Allow tests to test the debounce behavior by counting debounce runs.
window.DEBOUNCE_RUN_COUNT = 0;

/**
 * @typedef {import("../translations").SupportedLanguages} SupportedLanguages
 */

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
   * The translator is only valid for a single language pair, and needs
   * to be recreated if the language pair changes.
   *
   * @type {null | Promise<Translator>}
   */
  translator = null;

  /**
   * @param {boolean} isSupported
   */
  constructor(isSupported) {
    /**
     * Is the engine supported by the device?
     * @type {boolean}
     */
    this.isTranslationEngineSupported = isSupported;

    /**
     * @type {SupportedLanguages}
     */
    this.supportedLanguages = isSupported
      ? AT_getSupportedLanguages()
      : Promise.resolve([]);

    this.ui = new TranslationsUI(this);
    this.ui.setup();

    // Set the UI as ready after all of the state promises have settled.
    this.supportedLanguages
      .then(() => {
        this.ui.setAsReady();
      })
      .catch(error => {
        AT_logError("Failed to load the supported languages", error);
      });
  }

  /**
   * Identifies the human language in which the message is written and returns
   * the BCP 47 language tag of the language it is determined to be.
   *
   * e.g. "en" for English.
   *
   * @param {string} message
   */
  async identifyLanguage(message) {
    const start = performance.now();
    const { langTag, confidence } = await AT_identifyLanguage(message);
    const duration = performance.now() - start;
    AT_log(
      `[ ${langTag}(${(confidence * 100).toFixed(2)}%) ]`,
      `Source language identified in ${duration / 1000} seconds`
    );
    return langTag;
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
        translator: translatorPromise,
      } = this;

      if (!this.isTranslationEngineSupported) {
        // Never translate when the engine isn't supported.
        return;
      }

      if (
        !fromLanguage ||
        !toLanguage ||
        !messageToTranslate ||
        !translatorPromise
      ) {
        // Not everything is set for translation.
        this.ui.updateTranslation("");
        return;
      }

      const [translator] = await Promise.all([
        // Ensure the engine is ready to go.
        translatorPromise,
        // Ensure the previous translation has finished so that only the latest
        // translation goes through.
        this.translationRequest,
      ]);

      if (
        // Check if the current configuration has changed and if this is stale. If so
        // then skip this request, as there is already a newer request with more up to
        // date information.
        this.translator !== translatorPromise ||
        this.fromLanguage !== fromLanguage ||
        this.toLanguage !== toLanguage ||
        this.messageToTranslate !== messageToTranslate
      ) {
        return;
      }

      const start = performance.now();

      this.translationRequest = translator.translate(messageToTranslate);
      const translation = await this.translationRequest;

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
   * Any time a language pair is changed, a new Translator needs to be created.
   */
  async maybeCreateNewTranslator() {
    // If we may need to re-building the worker, the old translation is no longer valid.
    this.ui.updateTranslation("");

    // These are cases in which it wouldn't make sense or be possible to load any translations models.
    if (
      // If fromLanguage or toLanguage are unpopulated we cannot load anything.
      !this.fromLanguage ||
      !this.toLanguage ||
      // If fromLanguage's value is "detect", rather than a BCP 47 language tag, then no language
      // has been detected yet.
      this.fromLanguage === "detect" ||
      // If fromLanguage and toLanguage are the same, this means that the detected language
      // is the same as the toLanguage, and we do not want to translate from one language to itself.
      this.fromLanguage === this.toLanguage
    ) {
      if (this.translator) {
        // The engine is no longer needed.
        this.translator.then(translator => translator.destroy());
        this.translator = null;
      }
      return;
    }

    const start = performance.now();
    AT_log(
      `Creating a new translator for "${this.fromLanguage}" to "${this.toLanguage}"`
    );

    this.translator = Translator.create(this.fromLanguage, this.toLanguage);
    this.maybeRequestTranslation();

    try {
      await this.translator;
      const duration = performance.now() - start;
      // Signal to tests that the translator was created so they can exit.
      window.postMessage("translator-ready");
      AT_log(`Created a new Translator in ${duration / 1000} seconds`);
    } catch (error) {
      this.ui.showInfo("about-translations-engine-error");
      AT_logError("Failed to get the Translations worker", error);
    }
  }

  /**
   * Updates the fromLanguage to match the detected language only if the
   * about-translations-detect option is selected in the language-from dropdown.
   *
   * If the new fromLanguage is different than the previous fromLanguage this
   * may update the UI to display the new language and may rebuild the translations
   * worker if there is a valid selected target language.
   */
  async maybeUpdateDetectedLanguage() {
    if (!this.ui.detectOptionIsSelected() || this.messageToTranslate === "") {
      // If we are not detecting languages or if the message has been cleared
      // we should ensure that the UI is not displaying a detected language
      // and there is no need to run any language detection.
      this.ui.setDetectOptionTextContent("");
      return;
    }

    const [langTag, supportedLanguages] = await Promise.all([
      this.identifyLanguage(this.messageToTranslate),
      this.supportedLanguages,
    ]);

    // Only update the language if the detected language matches
    // one of our supported languages.
    const entry = supportedLanguages.fromLanguages.find(
      ({ langTag: existingTag }) => existingTag === langTag
    );
    if (entry) {
      const { displayName } = entry;
      await this.setFromLanguage(langTag);
      this.ui.setDetectOptionTextContent(displayName);
    }
  }

  /**
   * @param {string} lang
   */
  async setFromLanguage(lang) {
    if (lang !== this.fromLanguage) {
      this.fromLanguage = lang;
      await this.maybeCreateNewTranslator();
    }
  }

  /**
   * @param {string} lang
   */
  setToLanguage(lang) {
    if (lang !== this.toLanguage) {
      this.toLanguage = lang;
      this.maybeCreateNewTranslator();
    }
  }

  /**
   * @param {string} message
   */
  async setMessageToTranslate(message) {
    if (message !== this.messageToTranslate) {
      this.messageToTranslate = message;
      await this.maybeUpdateDetectedLanguage();
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
  /** @type {HTMLDivElement} */
  translationInfo = document.getElementById("translation-info");
  /** @type {HTMLDivElement} */
  translationInfoMessage = document.getElementById("translation-info-message");
  /** @type {TranslationsState} */
  state;

  /**
   * The detect-language option element. We want to maintain a handle to this so that
   * we can dynamically update its display text to include the detected language.
   *
   * @type {HTMLOptionElement}
   */
  #detectOption;

  /**
   * @param {TranslationsState} state
   */
  constructor(state) {
    this.state = state;
    this.translationTo.style.visibility = "visible";
    this.#detectOption = document.querySelector('option[value="detect"]');
  }

  /**
   * Do the initial setup.
   */
  setup() {
    if (!this.state.isTranslationEngineSupported) {
      this.showInfo("about-translations-no-support");
      this.disableUI();
      return;
    }
    this.setupDropdowns();
    this.setupTextarea();
  }

  /**
   * Signals that the UI is ready, for tests.
   */
  setAsReady() {
    document.body.setAttribute("ready", "");
  }

  /**
   * Once the models have been synced from remote settings, populate them with the display
   * names of the languages.
   */
  async setupDropdowns() {
    const supportedLanguages = await this.state.supportedLanguages;

    // Update the DOM elements with the display names.
    for (const { langTag, displayName } of supportedLanguages.toLanguages) {
      const option = document.createElement("option");
      option.value = langTag;
      option.text = displayName;
      this.languageTo.add(option);
    }

    for (const { langTag, displayName } of supportedLanguages.fromLanguages) {
      const option = document.createElement("option");
      option.value = langTag;
      option.text = displayName;
      this.languageFrom.add(option);
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
   * Show an info message to the user.
   *
   * @param {string} l10nId
   */
  showInfo(l10nId) {
    document.l10n.setAttributes(this.translationInfoMessage, l10nId);
    this.translationInfo.style.display = "flex";
  }

  /**
   * Hides the info UI.
   */
  hideInfo() {
    this.translationInfo.style.display = "none";
  }

  /**
   * Returns true if about-translations-detect is the currently
   * selected option in the language-from dropdown, otherwise false.
   *
   * @returns {boolean}
   */
  detectOptionIsSelected() {
    return this.languageFrom.value === "detect";
  }

  /**
   * Sets the textContent of the about-translations-detect option in the
   * language-from dropdown to include the detected language's display name.
   *
   * @param {string} displayName
   */
  setDetectOptionTextContent(displayName) {
    // Set the text to the fluent value that takes an arg to display the language name.
    if (displayName) {
      document.l10n.setAttributes(
        this.#detectOption,
        "about-translations-detect-lang",
        { language: displayName }
      );
    } else {
      // Reset the text to the fluent value that does not display any language name.
      document.l10n.setAttributes(
        this.#detectOption,
        "about-translations-detect"
      );
    }
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
    this.state.maybeUpdateDetectedLanguage();
  }

  /**
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

  disableUI() {
    this.translationFrom.disabled = true;
    this.languageFrom.disabled = true;
    this.languageTo.disabled = true;
  }

  /**
   * @param {string} message
   */
  updateTranslation(message) {
    this.translationTo.innerText = message;
    if (message) {
      this.translationTo.style.visibility = "visible";
      this.translationToBlank.style.visibility = "hidden";
      this.hideInfo();
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
      AT_isTranslationEngineSupported().then(isSupported => {
        window.translationsState = new TranslationsState(isSupported);
      });
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

/**
 * Perform transalations over a `MessagePort`. This class manages the communications to
 * the translations engine.
 */
class Translator {
  /**
   * @type {MessagePort}
   */
  #port;

  /**
   * An id for each message sent. This is used to match up the request and response.
   */
  #nextMessageId = 0;

  /**
   * Tie together a message id to a resolved response.
   * @type {Map<number, TranslationRequest}
   */
  #requests = new Map();

  engineStatus = "initializing";

  /**
   * @param {MessagePort} port
   */
  constructor(port) {
    this.#port = port;

    // Create a promise that will be resolved when the engine is ready.
    let engineLoaded;
    let engineFailed;
    this.ready = new Promise((resolve, reject) => {
      engineLoaded = resolve;
      engineFailed = reject;
    });

    // Match up a response on the port to message that was sent.
    port.onmessage = ({ data }) => {
      switch (data.type) {
        case "TranslationsPort:TranslationResponse": {
          const { targetText, messageId } = data;
          // A request may not match match a messageId if there is a race during the pausing
          // and discarding of the queue.
          this.#requests.get(messageId)?.resolve(targetText);
          break;
        }
        case "TranslationsPort:GetEngineStatusResponse": {
          if (data.status === "ready") {
            engineLoaded();
          } else {
            engineFailed();
          }
          break;
        }
        default:
          AT_logError("Unknown translations port message: " + data.type);
          break;
      }
    };

    port.postMessage({ type: "TranslationsPort:GetEngineStatusRequest" });
  }

  /**
   * Opens up a port and creates a new translator.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {Promise<Translator>}
   */
  static create(fromLanguage, toLanguage) {
    return new Promise((resolve, reject) => {
      AT_createTranslationsPort(fromLanguage, toLanguage);

      function getResponse({ data }) {
        if (
          data.type == "GetTranslationsPort" &&
          fromLanguage === data.fromLanguage &&
          toLanguage === data.toLanguage
        ) {
          // The response matches, resolve the port.
          const translator = new Translator(data.port);

          // Resolve the translator once it is ready, or propagate the rejection
          // if it failed.
          translator.ready.then(() => resolve(translator), reject);
          window.removeEventListener("message", getResponse);
        }
      }

      // Listen for a response for the message port.
      window.addEventListener("message", getResponse);
    });
  }

  /**
   * Send a request to translate text to the Translations Engine. If it returns `null`
   * then the request is stale. A rejection means there was an error in the translation.
   * This request may be queued.
   *
   * @param {string} sourceText
   * @returns {Promise<string>}
   */
  translate(sourceText) {
    return new Promise((resolve, reject) => {
      const messageId = this.#nextMessageId++;
      // Store the "resolve" for the promise. It will be matched back up with the
      // `messageId` in #handlePortMessage.
      const isHTML = AT_isHtmlTranslation();
      this.#requests.set(messageId, {
        sourceText,
        isHTML,
        resolve,
        reject,
      });
      this.#port.postMessage({
        type: "TranslationsPort:TranslationRequest",
        messageId,
        sourceText,
        isHTML,
      });
    });
  }

  /**
   * Close the port and remove any pending or queued requests.
   */
  destroy() {
    this.#port.close();
  }
}
