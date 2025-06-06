/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The pivot language is used to pivot between two different language translations
 * when there is not a model available to translate directly between the two. In this
 * case "en" is common between the various supported models.
 *
 * For instance given the following two models:
 *   "fr" -> "en"
 *   "en" -> "it"
 *
 * You can accomplish:
 *   "fr" -> "it"
 *
 * By doing:
 *   "fr" -> "en" -> "it"
 */
const PIVOT_LANGUAGE = "en";

const TRANSLATIONS_PERMISSION = "translations";
const ALWAYS_TRANSLATE_LANGS_PREF =
  "browser.translations.alwaysTranslateLanguages";
const NEVER_TRANSLATE_LANGS_PREF =
  "browser.translations.neverTranslateLanguages";

const lazy = {};

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

if (AppConstants.ENABLE_WEBDRIVER) {
  XPCOMUtils.defineLazyServiceGetter(
    lazy,
    "Marionette",
    "@mozilla.org/remote/marionette;1",
    "nsIMarionette"
  );

  XPCOMUtils.defineLazyServiceGetter(
    lazy,
    "RemoteAgent",
    "@mozilla.org/remote/agent;1",
    "nsIRemoteAgent"
  );
} else {
  lazy.Marionette = { running: false };
  lazy.RemoteAgent = { running: false };
}

XPCOMUtils.defineLazyServiceGetters(lazy, {
  BrowserHandler: ["@mozilla.org/browser/clh;1", "nsIBrowserHandler"],
});

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  TranslationsTelemetry:
    "chrome://global/content/translations/TranslationsTelemetry.sys.mjs",
  EngineProcess: "chrome://global/content/ml/EngineProcess.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "translationsEnabledPref",
  "browser.translations.enable"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chaosErrorsPref",
  "browser.translations.chaos.errors"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chaosTimeoutMSPref",
  "browser.translations.chaos.timeoutMS"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "automaticallyPopupPref",
  "browser.translations.automaticallyPopup"
);

/**
 * Returns the always-translate language tags as an array.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "alwaysTranslateLangTags",
  ALWAYS_TRANSLATE_LANGS_PREF,
  /* aDefaultPrefValue */ "",
  /* onUpdate */ null,
  /* aTransform */ rawLangTags =>
    rawLangTags ? new Set(rawLangTags.split(",")) : new Set()
);

/**
 * Returns the never-translate language tags as an array.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "neverTranslateLangTags",
  NEVER_TRANSLATE_LANGS_PREF,
  /* aDefaultPrefValue */ "",
  /* onUpdate */ null,
  /* aTransform */ rawLangTags =>
    rawLangTags ? new Set(rawLangTags.split(",")) : new Set()
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "simulateUnsupportedEnginePref",
  "browser.translations.simulateUnsupportedEngine"
);

// At this time the signatures of the files are not being checked when they are being
// loaded from disk. This signature check involves hitting the network, and translations
// are explicitly an offline-capable feature. See Bug 1827265 for re-enabling this
// check.
const VERIFY_SIGNATURES_FROM_FS = false;

/**
 * @typedef {import("../translations").TranslationModelRecord} TranslationModelRecord
 * @typedef {import("../translations").RemoteSettingsClient} RemoteSettingsClient
 * @typedef {import("../translations").LanguageTranslationModelFiles} LanguageTranslationModelFiles
 * @typedef {import("../translations").WasmRecord} WasmRecord
 * @typedef {import("../translations").LangTags} LangTags
 * @typedef {import("../translations").LanguagePair} LanguagePair
 * @typedef {import("../translations").SupportedLanguages} SupportedLanguages
 * @typedef {import("../translations").TranslationErrors} TranslationErrors
 */

/**
 * @typedef {object} TranslationPair
 * @property {string} fromLanguage
 * @property {string} toLanguage
 * @property {string} [fromDisplayLanguage]
 * @property {string} [toDisplayLanguage]
 */

/**
 * The state that is stored per a "top" ChromeWindow. This "top" ChromeWindow is the JS
 * global associated with a browser window. Some state is unique to a browser window, and
 * using the top ChromeWindow is a unique key that ensures the state will be unique to
 * that browser window.
 *
 * See BrowsingContext.webidl for information on the "top"
 * See the TranslationsParent JSDoc for more information on the state management.
 */
class StatePerTopChromeWindow {
  /**
   * The storage backing for the states.
   *
   * @type {WeakMap<ChromeWindow, StatePerTopChromeWindow>}
   */
  static #states = new WeakMap();

  /**
   * When reloading the page, store the translation pair that needs translating.
   *
   * @type {null | TranslationPair}
   */
  translateOnPageReload = null;

  /**
   * The page may auto-translate due to user settings. On a page restore, always
   * skip the page restore logic.
   *
   * @type {boolean}
   */
  isPageRestored = false;

  /**
   * Remember the detected languages on a page reload. This will keep the translations
   * button from disappearing and reappearing, which causes the button to lose focus.
   *
   * @type {LangTags | null} previousDetectedLanguages
   */
  previousDetectedLanguages = null;

  static #id = 0;
  /**
   * @param {ChromeWindow} topChromeWindow
   */
  constructor(topChromeWindow) {
    this.id = StatePerTopChromeWindow.#id++;
    StatePerTopChromeWindow.#states.set(topChromeWindow, this);
  }

  /**
   * @param {ChromeWindow} topChromeWindow
   * @returns {StatePerTopChromeWindow}
   */
  static getOrCreate(topChromeWindow) {
    let state = StatePerTopChromeWindow.#states.get(topChromeWindow);
    if (state) {
      return state;
    }
    state = new StatePerTopChromeWindow(topChromeWindow);
    StatePerTopChromeWindow.#states.set(topChromeWindow, state);
    return state;
  }
}

/**
 * The TranslationsParent is used to orchestrate translations in Firefox. It can
 * download the Wasm translation engine, and the language models. It manages the life
 * cycle for offering and performing translations.
 *
 * Care must be taken for the life cycle of the state management and data caching. The
 * following examples use a fictitious `myState` property to show how state can be stored.
 *
 * There is only 1 TranslationsParent static class in the parent process. At this
 * layer it is safe to store things like translation models and general browser
 * configuration as these don't change across browser windows. This is accessed like
 * `TranslationsParent.myState`
 *
 * The next layer down are the top ChromeWindows. These map to the UI and user's conception
 * of a browser window, such as what you would get by hitting cmd+n or ctrl+n to get a new
 * browser window. State such as whether a page is reloaded or general navigation events
 * must be unique per ChromeWindow. State here is stored in the `StatePerTopChromeWindow`
 * abstraction, like `this.getWindowState().myState`. This layer also consists of a
 * `FullPageTranslationsPanel` instance per top ChromeWindow (at least on Desktop).
 *
 * The final layer consists of the multiple tabs and navigation history inside of a
 * ChromeWindow. Data for this layer is safe to store on the TranslationsParent instance,
 * like `this.myState`.
 *
 * Below is an ascii diagram of this relationship.
 *
 *   ┌─────────────────────────────────────────────────────────────────────────────┐
 *   │                           static TranslationsParent                         │
 *   └─────────────────────────────────────────────────────────────────────────────┘
 *                  |                                       |
 *                  v                                       v
 * ┌──────────────────────────────────────┐   ┌──────────────────────────────────────┐
 * │         top ChromeWindow             │   │        top ChromeWindow              │
 * │ (FullPageTranslationsPanel instance) │   │ (FullPageTranslationsPanel instance) │
 * └──────────────────────────────────────┘   └──────────────────────────────────────┘
 *             |               |       |                |              |       |
 *             v               v       v                v              v       v
 *   ┌────────────────────┐ ┌─────┐ ┌─────┐  ┌────────────────────┐ ┌─────┐ ┌─────┐
 *   │ TranslationsParent │ │ ... │ │ ... │  │ TranslationsParent │ │ ... │ │ ... │
 *   │  (actor instance)  │ │     │ │     │  │  (actor instance)  │ │     │ │     │
 *   └────────────────────┘ └─────┘ └─────┘  └────────────────────┘ └─────┘ └─────┘
 */
export class TranslationsParent extends JSWindowActorParent {
  /**
   * The following constants control the major version for assets downloaded from
   * Remote Settings. When a breaking change is introduced, Nightly will have these
   * numbers incremented by one, but Beta and Release will still be on the previous
   * version. Remote Settings will ship both versions of the records, and the latest
   * asset released in that version will be used. For instance, with a major version
   * of "1", assets can be downloaded for "1.0", "1.2", "1.3beta", but assets marked
   * as "2.0", "2.1", etc will not be downloaded.
   *
   * Release docs:
   * https://firefox-source-docs.mozilla.org/toolkit/components/translations/resources/03_bergamot.html
   */
  static BERGAMOT_MAJOR_VERSION = 1;
  static LANGUAGE_MODEL_MAJOR_VERSION = 1;

  /**
   * Contains the state that would affect UI. Anytime this state is changed, a dispatch
   * event is sent so that UI can react to it. The actor is inside of /toolkit and
   * needs a way of notifying /browser code (or other users) of when the state changes.
   *
   * @type {TranslationsLanguageState}
   */
  languageState;

  /**
   * Allows the TranslationsEngineParent to resolve an engine once it is ready.
   *
   * @type {null | () => TranslationsEngineParent}
   */
  resolveEngine = null;

  /**
   * The cached URI spec where the panel was first ever shown, as determined by the
   * browser.translations.panelShown pref.
   *
   * Holding on to this URI value allows us to show the introductory message in the panel
   * when the panel opens, as long as the active panel is open on that particular URI.
   *
   * @type {string | null}
   */
  firstShowUriSpec = null;

  /**
   * Do not send queries or do work when the actor is already destroyed. This flag needs
   * to be checked after calls to `await`.
   */
  #isDestroyed = false;

  /**
   * There is only one static TranslationsParent for all of the top ChromeWindows.
   * The top ChromeWindow maps to the user's conception of a window such as when you hit
   * cmd+n or ctrl+n.
   *
   * @returns {StatePerTopChromeWindow}
   */
  getWindowState() {
    const state = StatePerTopChromeWindow.getOrCreate(
      this.browsingContext.top.embedderWindowGlobal
    );
    return state;
  }

  actorCreated() {
    this.innerWindowId = this.browsingContext.top.embedderElement.innerWindowID;
    const windowState = this.getWindowState();
    this.languageState = new TranslationsLanguageState(
      this,
      windowState.previousDetectedLanguages
    );
    windowState.previousDetectedLanguages = null;

    if (windowState.translateOnPageReload) {
      // The actor was recreated after a page reload, start the translation.
      const { fromLanguage, toLanguage } = windowState.translateOnPageReload;
      windowState.translateOnPageReload = null;

      lazy.console.log(
        `Translating on a page reload from "${fromLanguage}" to "${toLanguage}".`
      );

      this.translate(
        fromLanguage,
        toLanguage,
        false // reportAsAutoTranslate
      );
    }
  }

  /**
   * A map of the TranslationModelRecord["id"] to the record of the model in Remote Settings.
   * Used to coordinate the downloads.
   *
   * @type {null | Promise<Map<string, TranslationModelRecord>>}
   */
  static #translationModelRecords = null;

  /**
   * The RemoteSettingsClient that downloads the translation models.
   *
   * @type {RemoteSettingsClient | null}
   */
  static #translationModelsRemoteClient = null;

  /**
   * The RemoteSettingsClient that downloads the wasm binaries.
   *
   * @type {RemoteSettingsClient | null}
   */
  static #translationsWasmRemoteClient = null;

  /**
   * Allows the actor's behavior to be changed when the translations engine is mocked via
   * a dummy RemoteSettingsClient.
   *
   * @type {bool}
   */
  static #isTranslationsEngineMocked = false;

  /**
   * @type {null | Promise<boolean>}
   */
  static #isTranslationsEngineSupported = null;

  /**
   * An ordered list of preferred languages based on:
   *   1. App languages
   *   2. Web requested languages
   *   3. OS language
   *
   * @type {null | string[]}
   */
  static #preferredLanguages = null;

  /**
   * The value of navigator.languages.
   *
   * @type {null | Set<string>}
   */
  static #webContentLanguages = null;

  static #observingLanguages = false;

  // On a fast connection, 10 concurrent downloads were measured to be the fastest when
  // downloading all of the language files.
  static MAX_CONCURRENT_DOWNLOADS = 10;
  static MAX_DOWNLOAD_RETRIES = 3;

  // The set of hosts that have already been offered for translations.
  static #hostsOffered = new Set();

  // Enable the translations popup offer in tests.
  static testAutomaticPopup = false;

  /**
   * Gecko preference for always translating a language.
   *
   * @type {string}
   */
  static ALWAYS_TRANSLATE_LANGS_PREF = ALWAYS_TRANSLATE_LANGS_PREF;

  /**
   * Gecko preference for never translating a language.
   *
   * @type {string}
   */
  static NEVER_TRANSLATE_LANGS_PREF = NEVER_TRANSLATE_LANGS_PREF;

  /**
   * Telemetry functions for Translations
   *
   * @returns {TranslationsTelemetry}
   */
  static telemetry() {
    return lazy.TranslationsTelemetry;
  }

  /**
   * TODO(Bug 1834306) - Cu.isInAutomation doesn't recognize Marionette and RemoteAgent
   * tests.
   */
  static isInAutomation() {
    return (
      Cu.isInAutomation || lazy.Marionette.running || lazy.RemoteAgent.running
    );
  }

  /**
   * Returns whether the Translations Engine is mocked for testing.
   *
   * @returns {boolean}
   */
  static isTranslationsEngineMocked() {
    return TranslationsParent.#isTranslationsEngineMocked;
  }

  /**
   * Offer translations (for instance by automatically opening the popup panel) whenever
   * languages are detected, but only do it once per host per session.
   *
   * @param {LangTags} detectedLanguages
   */
  maybeOfferTranslations(detectedLanguages) {
    if (!this.browsingContext.currentWindowGlobal) {
      return;
    }
    if (!lazy.automaticallyPopupPref) {
      return;
    }

    // On Android the BrowserHandler is intermittently not available (for unknown reasons).
    // Check that the component is available before de-lazifying lazy.BrowserHandler.
    if (Cc["@mozilla.org/browser/clh;1"] && lazy.BrowserHandler?.kiosk) {
      // Pop-ups should not be shown in kiosk mode.
      return;
    }
    const { documentURI } = this.browsingContext.currentWindowGlobal;

    if (
      TranslationsParent.isInAutomation() &&
      !TranslationsParent.testAutomaticPopup
    ) {
      // Do not offer translations in automation, as many tests do not expect this
      // behavior.
      lazy.console.log(
        "maybeOfferTranslations - Do not offer translations in automation.",
        documentURI.spec
      );
      return;
    }

    if (
      !detectedLanguages.docLangTag ||
      !detectedLanguages.userLangTag ||
      !detectedLanguages.isDocLangTagSupported
    ) {
      lazy.console.log(
        "maybeOfferTranslations - The detected languages were not supported.",
        detectedLanguages
      );
      return;
    }

    let host;
    try {
      host = documentURI.host;
    } catch {
      // nsIURI.host can throw if the URI scheme doesn't have a host. In this case
      // do not offer a translation.
      return;
    }
    if (TranslationsParent.#hostsOffered.has(host)) {
      // This host was already offered a translation.
      lazy.console.log(
        "maybeOfferTranslations - Host already offered a translation, so skip.",
        documentURI.spec
      );
      return;
    }
    const browser = this.browsingContext.top.embedderElement;
    if (!browser) {
      return;
    }
    TranslationsParent.#hostsOffered.add(host);
    const { CustomEvent } = browser.ownerGlobal;

    if (
      TranslationsParent.shouldNeverTranslateLanguage(
        detectedLanguages.docLangTag
      )
    ) {
      lazy.console.log(
        `maybeOfferTranslations - Should never translate language. "${detectedLanguages.docLangTag}"`,
        documentURI.spec
      );
      return;
    }
    if (this.shouldNeverTranslateSite()) {
      lazy.console.log(
        "maybeOfferTranslations - Should never translate site.",
        documentURI.spec
      );
      return;
    }

    if (detectedLanguages.docLangTag === detectedLanguages.userLangTag) {
      lazy.console.error(
        "maybeOfferTranslations - The document and user lang tag are the same, not offering a translation.",
        documentURI.spec
      );
      return;
    }

    // Only offer the translation if it's still the current page.
    let isCurrentPage = false;
    if (AppConstants.platform !== "android") {
      isCurrentPage =
        documentURI.spec ===
        this.browsingContext.topChromeWindow.gBrowser.selectedBrowser
          .documentURI.spec;
    } else {
      // In Android, the active window is the active tab.
      isCurrentPage = documentURI.spec === browser.documentURI.spec;
    }
    if (isCurrentPage) {
      lazy.console.log(
        "maybeOfferTranslations - Offering a translation",
        documentURI.spec,
        detectedLanguages
      );

      browser.dispatchEvent(
        new CustomEvent("TranslationsParent:OfferTranslation", {
          bubbles: true,
        })
      );
    }
  }

  /**
   * This is for testing purposes.
   */
  static resetHostsOffered() {
    TranslationsParent.#hostsOffered = new Set();
  }

  /**
   * Returns the word count of the text for a given language.
   *
   * @param {string} langTag - A BCP-47 language tag.
   * @param {string} text - The text for which to count words.
   *
   * @returns {number} - The count of words in the text.
   * @throws If a segmenter could not be created for the given language tag.
   */
  static countWords(langTag, text) {
    const segmenter = new Intl.Segmenter(langTag, { granularity: "word" });
    const segments = Array.from(segmenter.segment(text));
    return segments.filter(segment => segment.isWordLike).length;
  }

  /**
   * Retrieves the Translations actor from the current browser context.
   *
   * @param {object} browser - The browser object from which to get the context.
   *
   * @returns {object} The Translations actor for handling translation actions.
   * @throws {Error} Throws an error if the TranslationsParent actor cannot be found.
   */
  static getTranslationsActor(browser) {
    const actor =
      browser.browsingContext.currentWindowGlobal.getActor("Translations");

    if (!actor) {
      throw new Error("Unable to get the TranslationsParent actor.");
    }
    return actor;
  }

  /**
   * Detect if Wasm SIMD is supported, and cache the value. It's better to check
   * for support before downloading large binary blobs to a user who can't even
   * use the feature. This function also respects mocks and simulating unsupported
   * engines.
   *
   * @type {boolean}
   */
  static getIsTranslationsEngineSupported() {
    if (lazy.simulateUnsupportedEnginePref) {
      // Use the non-lazy console.log so that the user is always informed as to why
      // the translations engine is not working.
      console.log(
        "Translations: The translations engine is disabled through the pref " +
          '"browser.translations.simulateUnsupportedEngine".'
      );

      // The user is manually testing unsupported engines.
      return false;
    }

    if (TranslationsParent.#isTranslationsEngineMocked) {
      // A mocked translations engine is always supported.
      return true;
    }

    if (TranslationsParent.#isTranslationsEngineSupported === null) {
      TranslationsParent.#isTranslationsEngineSupported = detectSimdSupport();
    }

    return TranslationsParent.#isTranslationsEngineSupported;
  }

  /**
   * Only translate pages that match certain protocols, that way internal pages like
   * about:* pages will not be translated. Keep this logic up to date with the "matches"
   * array in the `toolkit/modules/ActorManagerParent.sys.mjs` definition.
   *
   * @param {object} gBrowser
   * @returns {boolean}
   */
  static isFullPageTranslationsRestrictedForPage(gBrowser) {
    const contentType = gBrowser.selectedBrowser.documentContentType;
    const scheme = gBrowser.currentURI.scheme;

    if (contentType === "application/pdf") {
      return true;
    }

    // Keep this logic up to date with the "matches" array in the
    // `toolkit/modules/ActorManagerParent.sys.mjs` definition.
    switch (scheme) {
      case "https":
      case "http":
      case "file":
        return false;
    }
    return true;
  }

  static #resetPreferredLanguages() {
    TranslationsParent.#webContentLanguages = null;
    TranslationsParent.#preferredLanguages = null;
    TranslationsParent.getPreferredLanguages();
  }

  static async observe(_subject, topic, _data) {
    switch (topic) {
      case "nsPref:changed":
      case "intl:app-locales-changed": {
        TranslationsParent.#resetPreferredLanguages();
        break;
      }
      default:
        throw new Error("Unknown observer event", topic);
    }
  }

  /**
   * Provide a way for tests to override the system locales.
   *
   * @type {null | string[]}
   */
  static mockedSystemLocales = null;

  /**
   * The "Accept-Language" values that the localizer or user has indicated for
   * the preferences for the web. https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Accept-Language
   *
   * Note that this preference always has English in the fallback chain, even if the
   * user doesn't actually speak English, and to other languages they potentially do
   * not speak. However, this preference will be used as an indication that a user may
   * prefer this language.
   *
   * https://transvision.flod.org/string/?entity=toolkit/chrome/global/intl.properties:intl.accept_languages&repo=gecko_strings
   */
  static getWebContentLanguages() {
    if (!TranslationsParent.#webContentLanguages) {
      const values = Services.prefs
        .getComplexValue("intl.accept_languages", Ci.nsIPrefLocalizedString)
        .data.split(/\s*,\s*/g);

      TranslationsParent.#webContentLanguages = new Set();

      for (const locale of values) {
        try {
          // Wrap this in a try statement since users can manually edit this pref.
          TranslationsParent.#webContentLanguages.add(
            new Intl.Locale(locale).language
          );
        } catch {
          // The locale was invalid, discard it.
        }
      }

      if (
        !Services.prefs.prefHasUserValue("intl.accept_languages") &&
        Services.locale.appLocaleAsBCP47 !== "en" &&
        !Services.locale.appLocaleAsBCP47.startsWith("en-")
      ) {
        // The user hasn't customized their accept languages, this means that English
        // is always provided as a fallback language, even if it is not available.
        TranslationsParent.#webContentLanguages.delete("en");
      }

      if (TranslationsParent.#webContentLanguages.size === 0) {
        // The user has removed all of their web content languages, default to the
        // app locale.
        TranslationsParent.#webContentLanguages.add(
          new Intl.Locale(Services.locale.appLocaleAsBCP47).language
        );
      }
    }

    return TranslationsParent.#webContentLanguages;
  }

  /**
   * An ordered list of preferred languages based on:
   *
   *   1. App languages
   *   2. Web requested languages
   *   3. OS language
   *
   * @returns {string[]}
   */
  static getPreferredLanguages() {
    if (TranslationsParent.#preferredLanguages) {
      return TranslationsParent.#preferredLanguages;
    }

    if (!TranslationsParent.#observingLanguages) {
      Services.obs.addObserver(
        TranslationsParent.#resetPreferredLanguages,
        "intl:app-locales-changed"
      );
      Services.prefs.addObserver(
        "intl.accept_languages",
        TranslationsParent.#resetPreferredLanguages
      );
      TranslationsParent.#observingLanguages = true;
    }

    // The system language could also be a good option for a language to offer the user.
    const osPrefs = Cc["@mozilla.org/intl/ospreferences;1"].getService(
      Ci.mozIOSPreferences
    );
    const systemLocales =
      TranslationsParent.mockedSystemLocales ?? osPrefs.systemLocales;

    // Combine the locales together.
    const preferredLocales = new Set([
      ...TranslationsParent.getWebContentLanguages(),
      ...Services.locale.appLocalesAsBCP47,
      ...systemLocales,
    ]);

    // Attempt to convert the locales to lang tags. Do not completely trust the
    // values coming from preferences and the OS to have been validated as correct
    // BCP 47 locale identifiers.
    const langTags = new Set();
    for (const locale of preferredLocales) {
      try {
        langTags.add(new Intl.Locale(locale).language);
      } catch (_) {
        // The locale was invalid, discard it.
      }
    }

    // Convert the Set to an array to indicate that it is an ordered listing of languages.
    TranslationsParent.#preferredLanguages = [...langTags];

    return TranslationsParent.#preferredLanguages;
  }

  /**
   * Requests a new translations port.
   *
   * @param {number} innerWindowId - The id of the current window.
   * @param {string} fromLanguage - The BCP-47 from-language tag.
   * @param {string} toLanguage - The BCP-47 to-language tag.
   *
   * @returns {Promise<MessagePort | undefined>} The port for communication with the translation engine, or undefined on failure.
   */
  static async requestTranslationsPort(
    innerWindowId,
    fromLanguage,
    toLanguage
  ) {
    let translationsEngineParent;
    try {
      translationsEngineParent =
        await lazy.EngineProcess.getTranslationsEngineParent();
    } catch (error) {
      console.error("Failed to get the translation engine process", error);
      return undefined;
    }

    // The MessageChannel will be used for communicating directly between the content
    // process and the engine's process.
    const { port1, port2 } = new MessageChannel();
    translationsEngineParent.startTranslation(
      fromLanguage,
      toLanguage,
      port1,
      innerWindowId
    );

    return port2;
  }

  async receiveMessage({ name, data }) {
    switch (name) {
      case "Translations:ReportLangTags": {
        const { documentElementLang, href } = data;
        const detectedLanguages = await this.getDetectedLanguages(
          documentElementLang,
          href
        ).catch(error => {
          // Detecting the languages can fail if the page gets destroyed before it
          // can be completed. This runs on every page that doesn't have a lang tag,
          // so only report the error if you have Translations logging turned on to
          // avoid console spam.
          lazy.console.log("Failed to get the detected languages.", error);
        });

        if (!detectedLanguages) {
          // The actor was already destroyed, and the detectedLanguages weren't reported
          // in time.
          return undefined;
        }

        this.languageState.detectedLanguages = detectedLanguages;

        if (this.shouldAutoTranslate(detectedLanguages)) {
          this.translate(
            detectedLanguages.docLangTag,
            detectedLanguages.userLangTag,
            true // reportAsAutoTranslate
          );
        } else {
          this.maybeOfferTranslations(detectedLanguages);
        }
        return undefined;
      }
      case "Translations:RequestPort": {
        const { requestedTranslationPair } = this.languageState;
        if (!requestedTranslationPair) {
          lazy.console.error(
            "A port was requested but no translation pair was previously requested"
          );
          return undefined;
        }

        let actor;
        try {
          actor = await lazy.EngineProcess.getTranslationsEngineParent();
        } catch (error) {
          console.error("Failed to get the translation engine process", error);
          return undefined;
        }

        if (this.#isDestroyed) {
          // This actor was already destroyed.
          return undefined;
        }

        if (!this.innerWindowId) {
          throw new Error(
            "The innerWindowId for the TranslationsParent was not available."
          );
        }

        // The MessageChannel will be used for communicating directly between the content
        // process and the engine's process.
        const { port1, port2 } = new MessageChannel();
        actor.startTranslation(
          requestedTranslationPair.fromLanguage,
          requestedTranslationPair.toLanguage,
          port1,
          this.innerWindowId,
          this
        );

        this.sendAsyncMessage(
          "Translations:AcquirePort",
          { port: port2 },
          [port2] // Mark the port as transferable.
        );

        return undefined;
      }
      case "Translations:ReportFirstVisibleChange": {
        this.languageState.hasVisibleChange = true;
      }
    }
    return undefined;
  }

  /**
   * @param {string} fromLanguage
   * @param {string} toLanguage
   */
  static async getTranslationsEnginePayload(fromLanguage, toLanguage) {
    const wasmStartTime = Cu.now();
    const bergamotWasmArrayBufferPromise =
      TranslationsParent.#getBergamotWasmArrayBuffer();
    bergamotWasmArrayBufferPromise
      .then(() => {
        ChromeUtils.addProfilerMarker(
          "TranslationsParent",
          { innerWindowId: this.innerWindowId, startTime: wasmStartTime },
          "Loading bergamot wasm array buffer"
        );
      })
      .catch(() => {
        // Do nothing.
      });

    const modelStartTime = Cu.now();
    let files = await TranslationsParent.getLanguageTranslationModelFiles(
      fromLanguage,
      toLanguage
    );

    let languageModelFiles;
    if (files) {
      languageModelFiles = [files];
    } else {
      // No matching model was found, try to pivot between English.
      const [files1, files2] = await Promise.all([
        TranslationsParent.getLanguageTranslationModelFiles(
          fromLanguage,
          PIVOT_LANGUAGE
        ),
        TranslationsParent.getLanguageTranslationModelFiles(
          PIVOT_LANGUAGE,
          toLanguage
        ),
      ]);
      if (!files1 || !files2) {
        throw new Error(
          `No language models were found for ${fromLanguage} to ${toLanguage}`
        );
      }
      languageModelFiles = [files1, files2];
    }

    ChromeUtils.addProfilerMarker(
      "TranslationsParent",
      { innerWindowId: this.innerWindowId, startTime: modelStartTime },
      "Loading translation model files"
    );

    const bergamotWasmArrayBuffer = await bergamotWasmArrayBufferPromise;

    return {
      bergamotWasmArrayBuffer,
      languageModelFiles,
      isMocked: TranslationsParent.#isTranslationsEngineMocked,
    };
  }

  /**
   * Returns true if translations should auto-translate from the given
   * language, otherwise returns false.
   *
   * @param {LangTags} langTags
   * @returns {boolean}
   */
  #maybeAutoTranslate(langTags) {
    const windowState = this.getWindowState();
    if (windowState.isPageRestored) {
      // The user clicked the restore button. Respect it for one page load.
      windowState.isPageRestored = false;

      // Skip this auto-translation.
      return false;
    }

    return TranslationsParent.shouldAlwaysTranslateLanguage(langTags);
  }

  /**
   * Creates a lookup key that is unique to each fromLanguage-toLanguage pair.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {string}
   */
  static languagePairKey(fromLanguage, toLanguage) {
    return `${fromLanguage},${toLanguage}`;
  }

  /**
   * The cached language pairs.
   *
   * @type {Promise<Array<LanguagePair>> | null}
   */
  static #languagePairs = null;

  /**
   * Clears the cached list of language pairs, notifying observers that the
   * available language pairs have changed.
   */
  static #clearCachedLanguagePairs() {
    TranslationsParent.#languagePairs = null;
    Services.obs.notifyObservers(null, "translations:language-pairs-changed");
  }

  /**
   * Get the list of translation pairs supported by the translations engine.
   *
   * @returns {Promise<Array<LanguagePair>>}
   */
  static getLanguagePairs() {
    if (!TranslationsParent.#languagePairs) {
      TranslationsParent.#languagePairs =
        TranslationsParent.#getTranslationModelRecords().then(records => {
          const languagePairMap = new Map();

          for (const { fromLang, toLang } of records.values()) {
            const key = TranslationsParent.languagePairKey(fromLang, toLang);
            if (!languagePairMap.has(key)) {
              languagePairMap.set(key, { fromLang, toLang });
            }
          }
          return Array.from(languagePairMap.values());
        });
      TranslationsParent.#languagePairs.catch(() => {
        TranslationsParent.#clearCachedLanguagePairs();
      });
    }
    return TranslationsParent.#languagePairs;
  }

  /**
   * Get the list of languages and their display names, sorted by their display names.
   * This is more expensive of a call than getLanguagePairs since the display names
   * are looked up.
   *
   * This is all of the information needed to render dropdowns for translation
   * language selection.
   *
   * @returns {Promise<SupportedLanguages>}
   */
  static async getSupportedLanguages() {
    await chaosMode(1 / 4);
    const languagePairs = await TranslationsParent.getLanguagePairs();

    /** @type {Set<string>} */
    const fromLanguages = new Set();
    /** @type {Set<string>} */
    const toLanguages = new Set();

    for (const { fromLang, toLang } of languagePairs) {
      fromLanguages.add(fromLang);
      toLanguages.add(toLang);
    }

    // Build a map of the langTag to the display name.
    /** @type {Map<string, string>} */
    const displayNames = new Map();
    {
      const dn = new Services.intl.DisplayNames(undefined, {
        type: "language",
      });

      for (const langTagSet of [fromLanguages, toLanguages]) {
        for (const langTag of langTagSet.keys()) {
          if (displayNames.has(langTag)) {
            continue;
          }
          displayNames.set(langTag, dn.of(langTag));
        }
      }
    }

    const addDisplayName = langTag => ({
      langTag,
      displayName: displayNames.get(langTag),
    });

    const sort = (a, b) => a.displayName.localeCompare(b.displayName);

    return {
      languagePairs,
      fromLanguages: Array.from(fromLanguages.keys())
        .map(addDisplayName)
        .sort(sort),
      toLanguages: Array.from(toLanguages.keys())
        .map(addDisplayName)
        .sort(sort),
    };
  }

  /**
   * Create a unique list of languages, sorted by the display name.
   *
   * @param {object} supportedLanguages
   * @returns {Array<{ langTag: string, displayName: string}>}
   */
  static getLanguageList(supportedLanguages) {
    const displayNames = new Map();
    for (const languages of [
      supportedLanguages.fromLanguages,
      supportedLanguages.toLanguages,
    ]) {
      for (const { langTag, displayName } of languages) {
        displayNames.set(langTag, displayName);
      }
    }

    let appLangTag = new Intl.Locale(Services.locale.appLocaleAsBCP47).language;

    // Don't offer to download the app's language.
    displayNames.delete(appLangTag);

    // Sort the list of languages by the display names.
    return [...displayNames.entries()]
      .map(([langTag, displayName]) => ({
        langTag,
        displayName,
      }))
      .sort((a, b) => a.displayName.localeCompare(b.displayName));
  }

  /**
   * Handles records that were deleted in a Remote Settings "sync" event by
   * attempting to delete any previously downloaded attachments that are
   * associated with the deleted records.
   *
   * @param {RemoteSettingsClient} client
   *  - The Remote Settings client for which to handle deleted records.
   * @param {TranslationModelRecord[]} deletedRecords
   *  - The list of records that were deleted from the client's database.
   */
  static async #handleDeletedRecords(client, deletedRecords) {
    // Attempt to delete any downloaded attachments that are associated with deleted records.
    const failedDeletions = [];
    await Promise.all(
      deletedRecords.map(async record => {
        try {
          if (await client.attachments.isDownloaded(record)) {
            await client.attachments.deleteDownloaded(record);
          }
        } catch (error) {
          failedDeletions.push({ record, error });
        }
      })
    );

    // Report deletion failures if any occurred.
    if (failedDeletions.length) {
      lazy.console.warn(
        'Remote Settings "sync" event failed to delete attachments for deleted records.'
      );
      for (const { record, error } of failedDeletions) {
        lazy.console.error(
          `Failed to delete attachment for deleted record ${record.name}: ${error}`
        );
      }
    }
  }

  /**
   * Handles records that were updated in a Remote Settings "sync" event by
   * attempting to delete any previously downloaded attachments that are
   * associated with the old record versions, then downloading attachments
   * that are associated with the new record versions.
   *
   * @param {RemoteSettingsClient} client
   *  - The Remote Settings client for which to handle updated records.
   * @param {{old: TranslationModelRecord, new: TranslationModelRecord}[]} updatedRecords
   *  - The list of records that were updated in the client's database.
   */
  static async #handleUpdatedRecords(client, updatedRecords) {
    // Gather any updated records whose attachments were previously downloaded.
    const recordsWithAttachmentsToReplace = [];
    for (const {
      old: recordBeforeUpdate,
      new: recordAfterUpdate,
    } of updatedRecords) {
      if (await client.attachments.isDownloaded(recordBeforeUpdate)) {
        recordsWithAttachmentsToReplace.push({
          recordBeforeUpdate,
          recordAfterUpdate,
        });
      }
    }

    // Attempt to delete all of the attachments for the old versions of the updated records.
    const failedDeletions = [];
    await Promise.all(
      recordsWithAttachmentsToReplace.map(async ({ recordBeforeUpdate }) => {
        try {
          await client.attachments.deleteDownloaded(recordBeforeUpdate);
        } catch (error) {
          failedDeletions.push({ record: recordBeforeUpdate, error });
        }
      })
    );

    // Report deletion failures if any occurred.
    if (failedDeletions.length) {
      lazy.console.warn(
        'Remote Settings "sync" event failed to delete old record attachments for updated records.'
      );
      for (const { record, error } of failedDeletions) {
        lazy.console.error(
          `Failed to delete old attachment for updated record ${record.name}: ${error.reason}`
        );
      }
    }

    // Attempt to download all of the attachments for the new versions of the updated records.
    const failedDownloads = [];
    await Promise.all(
      recordsWithAttachmentsToReplace.map(async ({ recordAfterUpdate }) => {
        try {
          await client.attachments.download(recordAfterUpdate);
        } catch (error) {
          failedDownloads.push({ record: recordAfterUpdate, error });
        }
      })
    );

    // Report deletion failures if any occurred.
    if (failedDownloads.length) {
      lazy.console.warn(
        'Remote Settings "sync" event failed to download new record attachments for updated records.'
      );
      for (const { record, error } of failedDeletions) {
        lazy.console.error(
          `Failed to download new attachment for updated record ${record.name}: ${error.reason}`
        );
      }
    }
  }

  /**
   * Handles the "sync" event for the Translations Models Remote Settings collection.
   * This is called whenever models are created, updated, or deleted from the Remote Settings database.
   *
   * @param {object} event - The sync event.
   * @param {object} event.data - The data associated with the sync event.
   * @param {TranslationModelRecord[]} event.data.created
   *  - The list of Remote Settings records that were created in the sync event.
   * @param {{old: TranslationModelRecord, new: TranslationModelRecord}[]} event.data.updated
   *  - The list of Remote Settings records that were updated in the sync event.
   * @param {TranslationModelRecord[]} event.data.deleted
   *  - The list of Remote Settings records that were deleted in the sync event.
   */
  static async #handleTranslationsModelsSync({
    data: { created, updated, deleted },
  }) {
    const client = TranslationsParent.#translationModelsRemoteClient;
    if (!client) {
      lazy.console.error(
        "Translations models client was not present when receiving a sync event."
      );
      return;
    }

    // Invalidate cached data.
    TranslationsParent.#clearCachedLanguagePairs();
    TranslationsParent.#translationModelRecords = null;

    // Language model attachments will only be downloaded when they are used.
    lazy.console.log(
      `Remote Settings "sync" event for language-model records`,
      {
        created,
        updated,
        deleted,
      }
    );

    if (deleted.length) {
      await TranslationsParent.#handleDeletedRecords(client, deleted);
    }

    if (updated.length) {
      await TranslationsParent.#handleUpdatedRecords(client, updated);
    }

    // There is nothing to do for created records, since they will not have any previously downloaded attachments.
  }

  /**
   * Handles the "sync" event for the Translations WASM Remote Settings collection.
   * This is called whenever models are created, updated, or deleted from the Remote Settings database.
   *
   * @param {object} event - The sync event.
   * @param {object} event.data - The data associated with the sync event.
   * @param {TranslationModelRecord[]} event.data.created
   *  - The list of Remote Settings records that were created in the sync event.
   * @param {{old: TranslationModelRecord, new: TranslationModelRecord}[]} event.data.updated
   *  - The list of Remote Settings records that were updated in the sync event.
   * @param {TranslationModelRecord[]} event.data.deleted
   *  - The list of Remote Settings records that were deleted in the sync event.
   */
  static async #handleTranslationsWasmSync({
    data: { created, updated, deleted },
  }) {
    const client = TranslationsParent.#translationsWasmRemoteClient;
    if (!client) {
      lazy.console.error(
        "Translations WASM client was not present when receiving a sync event."
      );
      return;
    }

    lazy.console.log(`Remote Settings "sync" event for WASM records`, {
      created,
      updated,
      deleted,
    });

    // Invalidate cached data.
    TranslationsParent.#bergamotWasmRecord = null;

    if (deleted.length) {
      await TranslationsParent.#handleDeletedRecords(client, deleted);
    }

    if (updated.length) {
      await TranslationsParent.#handleUpdatedRecords(client, updated);
    }

    // There is nothing to do for created records, since they will not have any previously downloaded attachments.
  }

  /**
   * Lazily initializes the RemoteSettingsClient for the language models.
   *
   * @returns {RemoteSettingsClient}
   */
  static #getTranslationModelsRemoteClient() {
    if (TranslationsParent.#translationModelsRemoteClient) {
      return TranslationsParent.#translationModelsRemoteClient;
    }

    /** @type {RemoteSettingsClient} */
    const client = lazy.RemoteSettings("translations-models");
    TranslationsParent.#translationModelsRemoteClient = client;
    client.on("sync", TranslationsParent.#handleTranslationsModelsSync);

    return client;
  }

  /**
   * Retrieves the maximum major version of each record in the RemoteSettingsClient.
   *
   * If the client contains two different-version copies of the same record (e.g. 1.0 and 1.1)
   * then only the 1.1-version record will be returned in the resulting collection.
   *
   * @param {RemoteSettingsClient} remoteSettingsClient
   * @param {object} [options]
   *   @param {object} [options.filters={}]
   *     The filters to apply when retrieving the records from RemoteSettings.
   *     Filters should correspond to properties on the RemoteSettings records themselves.
   *     For example, A filter to retrieve only records with a `fromLang` value of "en" and a `toLang` value of "es":
   *     { filters: { fromLang: "en", toLang: "es" } }
   *   @param {number} options.majorVersion
   *   @param {Function} [options.lookupKey=(record => record.name)]
   *     The function to use to extract a lookup key from each record.
   *     This function should take a record as input and return a string that represents the lookup key for the record.
   *     For most record types, the name (default) is sufficient, however if a collection contains records with
   *     non-unique name values, it may be necessary to provide an alternative function here.
   * @returns {Array<TranslationModelRecord | WasmRecord>}
   */
  static async getMaxVersionRecords(
    remoteSettingsClient,
    { filters = {}, majorVersion, lookupKey = record => record.name } = {}
  ) {
    if (!majorVersion) {
      throw new Error("Expected the records to have a major version.");
    }
    try {
      await chaosMode(1 / 4);
    } catch (_error) {
      // Simulate an error by providing empty records.
      return [];
    }
    const retrievedRecords = await remoteSettingsClient.get({
      // Pull the records from the network if empty.
      syncIfEmpty: true,
      // Do not load the JSON dump if it is newer.
      //
      // The JSON dump comes from the Prod RemoteSettings channel
      // so we shouldn't ever have an issue with the Prod server
      // being older than the JSON dump itself (this is good).
      //
      // However, setting this to true will prevent us from
      // testing RemoteSettings on the Dev and Stage
      // environments if they happen to be older than the
      // most recent JSON dump from Prod.
      loadDumpIfNewer: false,
      // Don't verify the signature if the client is mocked.
      verifySignature: VERIFY_SIGNATURES_FROM_FS,
      // Apply any filters for retrieving the records.
      filters,
    });

    // Create a mapping to only the max version of each record discriminated by
    // the result of the lookupKey() function.
    const keyToRecord = new Map();

    for (const record of retrievedRecords) {
      const key = lookupKey(record);
      const existing = keyToRecord.get(key);

      if (!record.version) {
        lazy.console.error(record);
        throw new Error("Expected the record to have a version.");
      }
      if (
        TranslationsParent.isBetterRecordVersion(
          majorVersion,
          record.version,
          existing?.version
        )
      ) {
        keyToRecord.set(key, record);
      }
    }

    return Array.from(keyToRecord.values());
  }

  /**
   * Applies the constraint of matching for the best matching major version.
   *
   * @param {number} majorVersion
   * @param {string} nextVersion
   * @param {string} [existingVersion]
   *
   */
  static isBetterRecordVersion(majorVersion, nextVersion, existingVersion) {
    return (
      // Check that this is a major version record we can support.
      Services.vc.compare(`${majorVersion}.0a`, nextVersion) <= 0 &&
      Services.vc.compare(`${majorVersion + 1}.0a`, nextVersion) > 0 &&
      // Check that the new record is bigger version number
      (!existingVersion ||
        Services.vc.compare(existingVersion, nextVersion) < 0)
    );
  }

  /**
   * Lazily initializes the model records, and returns the cached ones if they
   * were already retrieved. The key of the returned `Map` is the record id.
   *
   * @returns {Promise<Map<string, TranslationModelRecord>>}
   */
  static async #getTranslationModelRecords() {
    if (!TranslationsParent.#translationModelRecords) {
      // Place the records into a promise to prevent any races.
      TranslationsParent.#translationModelRecords = (async () => {
        const records = new Map();
        const now = Date.now();
        const client = TranslationsParent.#getTranslationModelsRemoteClient();

        // Load the models. If no data is present, then there will be an initial sync.
        // Rely on Remote Settings for the syncing strategy for receiving updates.
        lazy.console.log(`Getting remote language models.`);

        /** @type {TranslationModelRecord[]} */
        const translationModelRecords =
          await TranslationsParent.getMaxVersionRecords(client, {
            majorVersion: TranslationsParent.LANGUAGE_MODEL_MAJOR_VERSION,
            // Names in this collection are not unique, so we are appending the languagePairKey
            // to guarantee uniqueness.
            lookupKey: record =>
              `${record.name}${TranslationsParent.languagePairKey(
                record.fromLang,
                record.toLang
              )}`,
          });

        if (translationModelRecords.length === 0) {
          throw new Error("Unable to retrieve the translation models.");
        }

        for (const record of TranslationsParent.ensureLanguagePairsHavePivots(
          translationModelRecords
        )) {
          records.set(record.id, record);
        }

        const duration = (Date.now() - now) / 1000;
        lazy.console.log(
          `Remote language models loaded in ${duration} seconds.`,
          records
        );

        return records;
      })();

      TranslationsParent.#translationModelRecords.catch(() => {
        TranslationsParent.#translationModelRecords = null;
      });
    }

    return TranslationsParent.#translationModelRecords;
  }

  /**
   * This implementation assumes that every language pair has access to the
   * pivot language. If any languages are added without a pivot language, or the
   * pivot language is changed, then this implementation will need a more complicated
   * language solver. This means that any UI pickers would need to be updated, and
   * the pivot language selection would need a solver.
   *
   * @param {TranslationModelRecord[] | LanguagePair[]} records
   */
  static ensureLanguagePairsHavePivots(records) {
    if (!AppConstants.DEBUG) {
      // Only run this check on debug builds as it's in the performance critical first
      // page load path.
      return records;
    }
    // lang -> pivot
    const hasToPivot = new Set();
    // pivot -> en
    const hasFromPivot = new Set();

    const fromLangs = new Set();
    const toLangs = new Set();

    for (const { fromLang, toLang } of records) {
      fromLangs.add(fromLang);
      toLangs.add(toLang);

      if (toLang === PIVOT_LANGUAGE) {
        // lang -> pivot
        hasToPivot.add(fromLang);
      }
      if (fromLang === PIVOT_LANGUAGE) {
        // pivot -> en
        hasFromPivot.add(toLang);
      }
    }

    const fromLangsToRemove = new Set();
    const toLangsToRemove = new Set();

    for (const lang of fromLangs) {
      if (lang === PIVOT_LANGUAGE) {
        continue;
      }
      // Check for "lang -> pivot"
      if (!hasToPivot.has(lang)) {
        TranslationsParent.reportError(
          new Error(
            `The "from" language model "${lang}" is being discarded as it doesn't have a pivot language.`
          )
        );
        fromLangsToRemove.add(lang);
      }
    }

    for (const lang of toLangs) {
      if (lang === PIVOT_LANGUAGE) {
        continue;
      }
      // Check for "pivot -> lang"
      if (!hasFromPivot.has(lang)) {
        TranslationsParent.reportError(
          new Error(
            `The "to" language model "${lang}" is being discarded as it doesn't have a pivot language.`
          )
        );
        toLangsToRemove.add(lang);
      }
    }

    const after = records.filter(record => {
      if (fromLangsToRemove.has(record.fromLang)) {
        return false;
      }
      if (toLangsToRemove.has(record.toLang)) {
        return false;
      }
      return true;
    });
    return after;
  }

  /**
   * Lazily initializes the RemoteSettingsClient for the downloaded wasm binary data.
   *
   * @returns {RemoteSettingsClient}
   */
  static #getTranslationsWasmRemoteClient() {
    if (TranslationsParent.#translationsWasmRemoteClient) {
      return TranslationsParent.#translationsWasmRemoteClient;
    }

    /** @type {RemoteSettingsClient} */
    const client = lazy.RemoteSettings("translations-wasm");
    TranslationsParent.#translationsWasmRemoteClient = client;
    client.on("sync", TranslationsParent.#handleTranslationsWasmSync);

    return client;
  }

  /** @type {Promise<WasmRecord> | null} */
  static #bergamotWasmRecord = null;

  /** @type {boolean} */
  static #lookForLocalWasmBuild = true;

  /**
   * This is used to load a local copy of the Bergamot translations engine, if it exists.
   * From a local build of Firefox:
   *
   * 1. Run the python script:
   *   ./toolkit/components/translations/bergamot-translator/build-bergamot.py --debug
   *
   * 2. Uncomment the .wasm file in: toolkit/components/translations/jar.mn
   * 3. Run: ./mach build
   * 4. Run: ./mach run
   */
  static async #maybeFetchLocalBergamotWasmArrayBuffer() {
    if (TranslationsParent.#lookForLocalWasmBuild) {
      // Attempt to get a local copy of the translator. Most likely this will be a 404.
      try {
        const response = await fetch(
          "chrome://global/content/translations/bergamot-translator-worker.wasm"
        );
        const arrayBuffer = response.arrayBuffer();
        lazy.console.log(`Using a local copy of Bergamot.`);
        return arrayBuffer;
      } catch {
        // Only attempt to fetch once, if it fails don't try again.
        TranslationsParent.#lookForLocalWasmBuild = false;
      }
    }
    return null;
  }

  /**
   * Bergamot is the translation engine that has been compiled to wasm. It is shipped
   * to the user via Remote Settings.
   *
   * https://github.com/mozilla/bergamot-translator/
   */
  /**
   * @returns {Promise<ArrayBuffer>}
   */
  static async #getBergamotWasmArrayBuffer() {
    const start = Date.now();
    const client = TranslationsParent.#getTranslationsWasmRemoteClient();

    const localCopy =
      await TranslationsParent.#maybeFetchLocalBergamotWasmArrayBuffer();
    if (localCopy) {
      return localCopy;
    }

    if (!TranslationsParent.#bergamotWasmRecord) {
      // Place the records into a promise to prevent any races.
      TranslationsParent.#bergamotWasmRecord = (async () => {
        // Load the wasm binary from remote settings, if it hasn't been already.
        lazy.console.log(`Getting remote bergamot-translator wasm records.`);

        /** @type {WasmRecord[]} */
        const wasmRecords = await TranslationsParent.getMaxVersionRecords(
          client,
          {
            filters: { name: "bergamot-translator" },
            majorVersion: TranslationsParent.BERGAMOT_MAJOR_VERSION,
          }
        );

        if (wasmRecords.length === 0) {
          // The remote settings client provides an empty list of records when there is
          // an error.
          throw new Error(
            "Unable to get the bergamot translator from Remote Settings."
          );
        }

        if (wasmRecords.length > 1) {
          TranslationsParent.reportError(
            new Error(
              "Expected the bergamot-translator to only have 1 record."
            ),
            wasmRecords
          );
        }
        const [record] = wasmRecords;
        lazy.console.log(
          `Using ${record.name}@${record.release} release version ${record.version} first released on Fx${record.fx_release}`,
          record
        );
        return record;
      })();
    }
    // Unlike the models, greedily download the wasm. It will pull it from a locale
    // cache on disk if it's already been downloaded. Do not retain a copy, as
    // this will be running in the parent process. It's not worth holding onto
    // this much memory, so reload it every time it is needed.

    try {
      await chaosModeError(1 / 3);

      /** @type {{buffer: ArrayBuffer}} */
      const { buffer } = await client.attachments.download(
        await TranslationsParent.#bergamotWasmRecord
      );

      const duration = Date.now() - start;
      lazy.console.log(
        `"bergamot-translator" wasm binary loaded in ${duration / 1000} seconds`
      );

      return buffer;
    } catch (error) {
      TranslationsParent.#bergamotWasmRecord = null;
      throw error;
    }
  }

  /**
   * Deletes language files that match a language.
   *
   * @param {string} language The BCP 47 language tag.
   */
  static async deleteLanguageFiles(language) {
    const client = TranslationsParent.#getTranslationModelsRemoteClient();
    const isForDeletion = true;
    return Promise.all(
      Array.from(
        await TranslationsParent.getRecordsForTranslatingToAndFromAppLanguage(
          language,
          isForDeletion
        )
      ).map(record => {
        lazy.console.log("Deleting record", record);
        return client.attachments.deleteDownloaded(record);
      })
    );
  }

  /**
   * Download language files that match a language.
   *
   * @param {string} language The BCP 47 language tag.
   */
  static async downloadLanguageFiles(language) {
    const client = TranslationsParent.#getTranslationModelsRemoteClient();

    const queue = [];

    for (const record of await TranslationsParent.getRecordsForTranslatingToAndFromAppLanguage(
      language
    )) {
      const download = () => {
        lazy.console.log("Downloading record", record.name, record.id);
        return client.attachments.download(record);
      };
      queue.push({ download });
    }

    return downloadManager(queue);
  }

  /**
   * Download all files used for translations.
   */
  static async downloadAllFiles() {
    const client = TranslationsParent.#getTranslationModelsRemoteClient();

    const queue = [];

    for (const record of (
      await TranslationsParent.#getTranslationModelRecords()
    ).values()) {
      queue.push({
        // The download may be attempted multiple times.
        onFailure: () => {
          console.error("Failed to download", record.name);
        },
        download: () => client.attachments.download(record),
      });
    }

    queue.push({
      download: () => TranslationsParent.#getBergamotWasmArrayBuffer(),
    });

    return downloadManager(queue);
  }

  /**
   * Delete all language model files.
   *
   * @returns {Promise<string[]>} A list of record IDs.
   */
  static async deleteAllLanguageFiles() {
    const client = TranslationsParent.#getTranslationModelsRemoteClient();
    await chaosMode();
    await client.attachments.deleteAll();
    return [...(await TranslationsParent.#getTranslationModelRecords()).keys()];
  }

  /**
   * Only returns true if all language files are present for a requested language.
   * It's possible only half the files exist for a pivot translation into another
   * language, or there was a download error, and we're still missing some files.
   *
   * @param {string} requestedLanguage The BCP 47 language tag.
   */
  static async hasAllFilesForLanguage(requestedLanguage) {
    const client = TranslationsParent.#getTranslationModelsRemoteClient();
    for (const record of await TranslationsParent.getRecordsForTranslatingToAndFromAppLanguage(
      requestedLanguage,
      true
    )) {
      if (!(await client.attachments.isDownloaded(record))) {
        return false;
      }
    }

    return true;
  }

  /**
   * Get the necessary files for translating to and from the app language and a
   * requested language. This may require the files for a pivot language translation
   * if there is no language model for a direct translation.
   *
   * @param {string} requestedLanguage The BCP 47 language tag.
   * @param {boolean} isForDeletion - Return a more restrictive set of languages, as
   *                  these files are marked for deletion. We don't want to remove
   *                  files that are needed for some other language's pivot translation.
   * @returns {Set<TranslationModelRecord>}
   */
  static async getRecordsForTranslatingToAndFromAppLanguage(
    requestedLanguage,
    isForDeletion = false
  ) {
    const records = await TranslationsParent.#getTranslationModelRecords();
    const appLanguage = new Intl.Locale(Services.locale.appLocaleAsBCP47)
      .language;

    let matchedRecords = new Set();

    if (requestedLanguage === appLanguage) {
      // There are no records if the requested language and app language are the same.
      return matchedRecords;
    }

    const addLanguagePair = (fromLang, toLang) => {
      let matchFound = false;
      for (const record of records.values()) {
        if (record.fromLang === fromLang && record.toLang === toLang) {
          matchedRecords.add(record);
          matchFound = true;
        }
      }
      return matchFound;
    };

    if (
      // Is there a direct translation?
      !addLanguagePair(requestedLanguage, appLanguage)
    ) {
      // This is no direct translation, get the pivot files.
      addLanguagePair(requestedLanguage, PIVOT_LANGUAGE);
      // These files may be required for other pivot translations, so don't remove
      // them if we are deleting records.
      if (!isForDeletion) {
        addLanguagePair(PIVOT_LANGUAGE, appLanguage);
      }
    }

    if (
      // Is there a direct translation?
      !addLanguagePair(appLanguage, requestedLanguage)
    ) {
      // This is no direct translation, get the pivot files.
      addLanguagePair(PIVOT_LANGUAGE, requestedLanguage);
      // These files may be required for other pivot translations, so don't remove
      // them if we are deleting records.
      if (!isForDeletion) {
        addLanguagePair(appLanguage, PIVOT_LANGUAGE);
      }
    }

    return matchedRecords;
  }

  /**
   * Gets the language model files in an array buffer by downloading attachments from
   * Remote Settings, or retrieving them from the local cache. Each translation
   * requires multiple files.
   *
   * Results are only returned if the model is found.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {boolean} withQualityEstimation
   * @returns {null | LanguageTranslationModelFiles}
   */
  static async getLanguageTranslationModelFiles(
    fromLanguage,
    toLanguage,
    withQualityEstimation = false
  ) {
    const client = TranslationsParent.#getTranslationModelsRemoteClient();

    lazy.console.log(
      `Beginning model downloads: "${fromLanguage}" to "${toLanguage}"`
    );

    const records = [
      ...(await TranslationsParent.#getTranslationModelRecords()).values(),
    ];

    /** @type {LanguageTranslationModelFiles} */
    let results;

    // Use Promise.all to download (or retrieve from cache) the model files in parallel.
    await Promise.all(
      records.map(async record => {
        if (record.fileType === "qualityModel" && !withQualityEstimation) {
          // Do not include the quality models if they aren't needed.
          return;
        }

        if (record.fromLang !== fromLanguage || record.toLang !== toLanguage) {
          // Only use models that match.
          return;
        }

        if (!results) {
          results = {};
        }

        const start = Date.now();

        // Download or retrieve from the local cache:

        await chaosMode(1 / 3);

        /** @type {{buffer: ArrayBuffer }} */
        const { buffer } = await client.attachments.download(record);

        results[record.fileType] = {
          buffer,
          record,
        };

        const duration = Date.now() - start;
        lazy.console.log(
          `Translation model fetched in ${duration / 1000} seconds:`,
          record.fromLang,
          record.toLang,
          record.fileType,
          record.version
        );
      })
    );

    if (!results) {
      // No model files were found, pivoting will be required.
      return null;
    }

    // Validate that all of the files we expected were actually available and
    // downloaded.

    if (!results.model) {
      throw new Error(
        `No model file was found for "${fromLanguage}" to "${toLanguage}."`
      );
    }

    if (!results.lex) {
      throw new Error(
        `No lex file was found for "${fromLanguage}" to "${toLanguage}."`
      );
    }

    if (withQualityEstimation && !results.qualityModel) {
      throw new Error(
        `No quality file was found for "${fromLanguage}" to "${toLanguage}."`
      );
    }

    if (results.vocab) {
      if (results.srcvocab) {
        throw new Error(
          `A srcvocab and vocab file were both included for "${fromLanguage}" to "${toLanguage}." Only one is needed.`
        );
      }
      if (results.trgvocab) {
        throw new Error(
          `A trgvocab and vocab file were both included for "${fromLanguage}" to "${toLanguage}." Only one is needed.`
        );
      }
    } else if (!results.srcvocab || !results.srcvocab) {
      throw new Error(
        `No vocab files were provided for "${fromLanguage}" to "${toLanguage}."`
      );
    }

    return results;
  }

  /**
   * Gets the expected download size that will occur (if any) if translate is called on two given languages for display purposes.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {boolean} withQualityEstimation
   * @returns {Promise<long>} Size in bytes of the expected download. A result of 0 indicates no download is expected for the request.
   */
  static async getExpectedTranslationDownloadSize(
    fromLanguage,
    toLanguage,
    withQualityEstimation = false
  ) {
    const directSize = await this.#getModelDownloadSize(
      fromLanguage,
      toLanguage,
      withQualityEstimation
    );

    // If a direct model is not found, then check pivots.
    if (directSize.downloadSize == 0 && !directSize.modelFound) {
      const indirectFrom = await TranslationsParent.#getModelDownloadSize(
        fromLanguage,
        PIVOT_LANGUAGE,
        withQualityEstimation
      );

      const indirectTo = await TranslationsParent.#getModelDownloadSize(
        PIVOT_LANGUAGE,
        toLanguage,
        withQualityEstimation
      );

      // Note, will also return 0 due to the models not being available as well.
      return (
        parseInt(indirectFrom.downloadSize) + parseInt(indirectTo.downloadSize)
      );
    }
    return directSize.downloadSize;
  }

  /**
   * Determines the language model download size for a specified translation for display purposes.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {boolean} withQualityEstimation
   * @returns {Promise<{downloadSize: long, modelFound: boolean}>} Download size is the
   *   size in bytes of the estimated download for display purposes. Model found indicates
   *   a model was found. e.g., a result of {size: 0, modelFound: false} indicates no
   *   bytes to download, because a model wasn't located.
   */
  static async #getModelDownloadSize(
    fromLanguage,
    toLanguage,
    withQualityEstimation = false
  ) {
    const client = TranslationsParent.#getTranslationModelsRemoteClient();
    const records = [
      ...(await TranslationsParent.#getTranslationModelRecords()).values(),
    ];

    let downloadSize = 0;
    let modelFound = false;

    await Promise.all(
      records.map(async record => {
        if (record.fileType === "qualityModel" && !withQualityEstimation) {
          return;
        }

        if (record.fromLang !== fromLanguage || record.toLang !== toLanguage) {
          return;
        }

        modelFound = true;
        const isDownloaded = await client.attachments.isDownloaded(record);
        if (!isDownloaded) {
          downloadSize += parseInt(record.attachment.size);
        }
      })
    );
    return { downloadSize, modelFound };
  }

  /**
   * For testing purposes, allow the Translations Engine to be mocked. If called
   * with `null` the mock is removed.
   *
   * @param {null | RemoteSettingsClient} [translationModelsRemoteClient]
   * @param {null | RemoteSettingsClient} [translationsWasmRemoteClient]
   */
  static mockTranslationsEngine(
    translationModelsRemoteClient,
    translationsWasmRemoteClient
  ) {
    lazy.console.log("Mocking RemoteSettings for the translations engine.");
    TranslationsParent.#translationModelsRemoteClient =
      translationModelsRemoteClient;
    TranslationsParent.#translationsWasmRemoteClient =
      translationsWasmRemoteClient;
    TranslationsParent.#isTranslationsEngineMocked = true;

    translationModelsRemoteClient.on(
      "sync",
      TranslationsParent.#handleTranslationsModelsSync
    );
    translationsWasmRemoteClient.on(
      "sync",
      TranslationsParent.#handleTranslationsWasmSync
    );
  }

  /**
   * Most values are cached for performance, in tests we want to be able to clear them.
   */
  static clearCache() {
    // Records.
    TranslationsParent.#bergamotWasmRecord = null;
    TranslationsParent.#translationModelRecords = null;

    // Clients.
    TranslationsParent.#translationModelsRemoteClient = null;
    TranslationsParent.#translationsWasmRemoteClient = null;

    // Derived data.
    TranslationsParent.#clearCachedLanguagePairs();
    TranslationsParent.#preferredLanguages = null;
    TranslationsParent.#isTranslationsEngineSupported = null;
  }

  /**
   * Remove the mocks for the translations engine, make sure and call clearCache after
   * to remove the cached values.
   */
  static unmockTranslationsEngine() {
    lazy.console.log(
      "Removing RemoteSettings mock for the translations engine."
    );
    TranslationsParent.#translationModelsRemoteClient.off(
      "sync",
      TranslationsParent.#handleTranslationsModelsSync
    );
    TranslationsParent.#translationsWasmRemoteClient.off(
      "sync",
      TranslationsParent.#handleTranslationsWasmSync
    );

    TranslationsParent.#isTranslationsEngineMocked = false;
  }

  /**
   * Report an error. Having this as a method allows tests to check that an error
   * was properly reported.
   *
   * @param {Error} error - Providing an Error object makes sure the stack is properly
   *                        reported.
   * @param {any[]} args - Any args to pass on to console.error.
   */
  static reportError(error, ...args) {
    lazy.console.log(error, ...args);
  }

  /**
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {boolean} reportAsAutoTranslate - In telemetry, report this as
   *   an auto-translate.
   */
  async translate(fromLanguage, toLanguage, reportAsAutoTranslate) {
    if (fromLanguage === toLanguage) {
      lazy.console.error(
        "A translation was requested where the from and to language match.",
        { fromLanguage, toLanguage, reportAsAutoTranslate }
      );
      return;
    }
    if (!fromLanguage || !toLanguage) {
      lazy.console.error(
        "A translation was requested but the fromLanguage or toLanguage was not set.",
        { fromLanguage, toLanguage, reportAsAutoTranslate }
      );
      return;
    }
    if (this.languageState.requestedTranslationPair) {
      // This page has already been translated, restore it and translate it
      // again once the actor has been recreated.
      const windowState = this.getWindowState();
      windowState.translateOnPageReload = { fromLanguage, toLanguage };
      this.restorePage(fromLanguage);
    } else {
      const { docLangTag } = this.languageState.detectedLanguages;

      let actor;
      try {
        actor = await lazy.EngineProcess.getTranslationsEngineParent();
      } catch (error) {
        console.error("Failed to get the translation engine process", error);
        return;
      }

      if (!this.innerWindowId) {
        throw new Error(
          "The innerWindowId for the TranslationsParent was not available."
        );
      }

      // The MessageChannel will be used for communicating directly between the content
      // process and the engine's process.
      const { port1, port2 } = new MessageChannel();
      actor.startTranslation(
        fromLanguage,
        toLanguage,
        port1,
        this.innerWindowId,
        this
      );

      this.languageState.requestedTranslationPair = {
        fromLanguage,
        toLanguage,
      };

      const preferredLanguages = TranslationsParent.getPreferredLanguages();
      const topPreferredLanguage =
        preferredLanguages && preferredLanguages.length
          ? preferredLanguages[0]
          : null;

      TranslationsParent.telemetry().onTranslate({
        docLangTag,
        fromLanguage,
        toLanguage,
        topPreferredLanguage,
        autoTranslate: reportAsAutoTranslate,
        requestTarget: "full_page",
      });

      this.sendAsyncMessage(
        "Translations:TranslatePage",
        {
          fromLanguage,
          toLanguage,
          port: port2,
        },
        // https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Transferable_objects
        // Mark the MessageChannel port as transferable.
        [port2]
      );
    }
  }

  /**
   * Restore the page to the original language by doing a hard reload.
   */
  restorePage() {
    TranslationsParent.telemetry().onRestorePage();
    // Skip auto-translate for one page load.
    const windowState = this.getWindowState();
    windowState.isPageRestored = true;
    this.languageState.hasVisibleChange = false;
    this.languageState.requestedTranslationPair = null;
    windowState.previousDetectedLanguages =
      this.languageState.detectedLanguages;

    const browser = this.browsingContext.embedderElement;
    browser.reload();
  }

  static onLocationChange(browser) {
    if (!lazy.translationsEnabledPref) {
      // The pref isn't enabled, so don't attempt to get the actor.
      return;
    }
    let actor;
    try {
      actor =
        browser.browsingContext.currentWindowGlobal.getActor("Translations");
    } catch {
      // The actor may not be supported on this page, which throws an error.
    }
    actor?.languageState.locationChanged();
  }

  async queryIdentifyLanguage() {
    if (
      TranslationsParent.isInAutomation() &&
      !TranslationsParent.#isTranslationsEngineMocked
    ) {
      return null;
    }
    return this.sendQuery("Translations:IdentifyLanguage").catch(error => {
      if (this.#isDestroyed) {
        // The actor was destroyed while this message was still being resolved.
        return null;
      }
      return Promise.reject(error);
    });
  }

  /**
   * Returns the language from the document element.
   *
   * @returns {Promise<string>}
   */
  queryDocumentElementLang() {
    return this.sendQuery("Translations:GetDocumentElementLang");
  }

  /**
   * @param {LangTags} langTags
   */
  shouldAutoTranslate(langTags) {
    if (
      langTags.docLangTag &&
      langTags.userLangTag &&
      langTags.isDocLangTagSupported &&
      this.#maybeAutoTranslate(langTags) &&
      !TranslationsParent.shouldNeverTranslateLanguage(langTags.docLangTag) &&
      !this.shouldNeverTranslateSite()
    ) {
      return true;
    }

    return false;
  }

  /**
   * Checks if a given language tag is supported for translation
   * when translating from this language into other languages.
   *
   * @param {string} langTag - A BCP-47 language tag.
   * @returns {Promise<boolean>}
   */
  static async isSupportedAsFromLang(langTag) {
    if (!langTag) {
      return false;
    }
    let languagePairs = await TranslationsParent.getLanguagePairs();
    return Boolean(languagePairs.find(({ fromLang }) => fromLang === langTag));
  }

  /**
   * Checks if a given language tag is supported for translation
   * when translating from other languages into this language.
   *
   * @param {string} langTag - A BCP-47 language tag.
   * @returns {Promise<boolean>}
   */
  static async isSupportedAsToLang(langTag) {
    if (!langTag) {
      return false;
    }
    let languagePairs = await TranslationsParent.getLanguagePairs();
    return Boolean(languagePairs.find(({ toLang }) => toLang === langTag));
  }

  /**
   * Retrieves the top preferred user language for which translation
   * is supported when translating to that language.
   */
  static async getTopPreferredSupportedToLang() {
    for (const langTag of TranslationsParent.getPreferredLanguages()) {
      if (await TranslationsParent.isSupportedAsToLang(langTag)) {
        return langTag;
      }
    }
    return PIVOT_LANGUAGE;
  }

  /**
   * Returns the lang tags that should be offered for translation. This is in the parent
   * rather than the child to remove the per-content process memory allocation amount.
   *
   * @param {string} [documentElementLang]
   * @param {string} [href]
   * @returns {Promise<LangTags | null>} - Returns null if the actor was destroyed before
   *   the result could be resolved.
   */
  async getDetectedLanguages(documentElementLang, href) {
    if (this.languageState.detectedLanguages) {
      return this.languageState.detectedLanguages;
    }
    const langTags = {
      docLangTag: null,
      userLangTag: null,
      isDocLangTagSupported: false,
    };
    if (!TranslationsParent.getIsTranslationsEngineSupported()) {
      return null;
    }

    if (documentElementLang === undefined) {
      documentElementLang = await this.queryDocumentElementLang();
      if (this.#isDestroyed) {
        return null;
      }
    }

    let languagePairs = await TranslationsParent.getLanguagePairs();
    if (this.#isDestroyed) {
      return null;
    }

    const determineIsDocLangTagSupported = () =>
      Boolean(
        languagePairs.find(({ fromLang }) => fromLang === langTags.docLangTag)
      );

    // First try to get the langTag from the document's markup.
    try {
      const docLocale = new Intl.Locale(documentElementLang);
      langTags.docLangTag = docLocale.language;
      langTags.isDocLangTagSupported = determineIsDocLangTagSupported();
    } catch (error) {}

    if (langTags.docLangTag) {
      // If it's not supported, try it again with a canonicalized version.
      if (!langTags.isDocLangTagSupported) {
        langTags.docLangTag = Intl.getCanonicalLocales(langTags.docLangTag)[0];
        langTags.isDocLangTagSupported = determineIsDocLangTagSupported();
      }

      // If it's still not supported, map macro language codes to specific ones.
      //   https://en.wikipedia.org/wiki/ISO_639_macrolanguage
      if (!langTags.isDocLangTagSupported) {
        // If more macro language codes are needed, this logic can be expanded.
        if (langTags.docLangTag === "no") {
          // Choose "Norwegian Bokmål" over "Norwegian Nynorsk" as it is more widely used.
          //
          // https://en.wikipedia.org/wiki/Norwegian_language#Bokm%C3%A5l_and_Nynorsk
          //
          //   > A 2005 poll indicates that 86.3% use primarily Bokmål as their daily
          //   > written language, 5.5% use both Bokmål and Nynorsk, and 7.5% use
          //   > primarily Nynorsk.
          langTags.docLangTag = "nb";
          langTags.isDocLangTagSupported = determineIsDocLangTagSupported();
        }
      }
    } else {
      // If the document's markup had no specified langTag, attempt to identify the page's language.
      langTags.docLangTag = await this.queryIdentifyLanguage();
      if (this.#isDestroyed) {
        return null;
      }
      langTags.isDocLangTagSupported = determineIsDocLangTagSupported();
    }

    const preferredLanguages = TranslationsParent.getPreferredLanguages();

    if (!langTags.docLangTag) {
      const message = "No valid language detected.";
      ChromeUtils.addProfilerMarker(
        "TranslationsParent",
        { innerWindowId: this.innerWindowId },
        message
      );
      lazy.console.log(message, href);

      const languagePairs = await TranslationsParent.getLanguagePairs();
      if (this.#isDestroyed) {
        return null;
      }

      // Attempt to find a good language to select for the user.
      langTags.userLangTag =
        preferredLanguages.find(langTag => langTag === languagePairs.toLang) ??
        null;

      return langTags;
    }

    if (TranslationsParent.getWebContentLanguages().has(langTags.docLangTag)) {
      // The doc language has been marked as a known language by the user, do not
      // offer a translation.
      const message =
        "The app and document languages match, so not translating.";
      ChromeUtils.addProfilerMarker(
        "TranslationsParent",
        { innerWindowId: this.innerWindowId },
        message
      );
      lazy.console.log(message, href);
      // The docLangTag will be set, while the userLangTag will be null.
      return langTags;
    }

    // Attempt to find a matching language pair for a preferred language.
    for (const preferredLangTag of preferredLanguages) {
      if (!langTags.isDocLangTagSupported) {
        if (languagePairs.some(({ toLang }) => toLang === preferredLangTag)) {
          // Only match the "to" language, since the "from" is not supported.
          langTags.userLangTag = preferredLangTag;
        }
        break;
      }

      // Is there a direct language pair match?
      if (
        languagePairs.some(
          ({ fromLang, toLang }) =>
            fromLang === langTags.docLangTag && toLang === preferredLangTag
        )
      ) {
        // A match was found in one of the preferred languages.
        langTags.userLangTag = preferredLangTag;
        break;
      }

      // Is there a pivot language match?
      if (
        // Match doc -> pivot
        languagePairs.some(
          ({ fromLang, toLang }) =>
            fromLang === langTags.docLangTag && toLang === PIVOT_LANGUAGE
        ) &&
        // Match pivot -> preferred language
        languagePairs.some(
          ({ fromLang, toLang }) =>
            fromLang === PIVOT_LANGUAGE && toLang === preferredLangTag
        )
      ) {
        langTags.userLangTag = preferredLangTag;
        break;
      }
    }

    if (!langTags.userLangTag) {
      // No language pairs match.
      const message = `No matching translation pairs were found for translating from "${langTags.docLangTag}".`;
      ChromeUtils.addProfilerMarker(
        "TranslationsParent",
        { innerWindowId: this.innerWindowId },
        message
      );
      lazy.console.log(message, languagePairs);
    }

    return langTags;
  }

  /**
   * The pref for if we can always offer a translation when it's available.
   */
  static shouldAlwaysOfferTranslations() {
    return lazy.automaticallyPopupPref;
  }

  /**
   * Returns true if the given language tag is present in the always-translate
   * languages preference, otherwise false.
   *
   * @param {LangTags} langTags
   * @returns {boolean}
   */
  static shouldAlwaysTranslateLanguage(langTags) {
    const { docLangTag, userLangTag } = langTags;
    if (docLangTag === userLangTag || !userLangTag) {
      // Do not auto-translate when the docLangTag matches the userLangTag, or when
      // the userLangTag is not set. The "always translate" is exposed via about:confg.
      // In case of users putting in non-sensical things here, we don't want to break
      // the experience. This behavior can lead to a "language degradation machine"
      // where we go from a source language -> pivot language -> source language.
      return false;
    }
    return lazy.alwaysTranslateLangTags.has(docLangTag);
  }

  /**
   * Returns true if the given language tag is present in the never-translate
   * languages preference, otherwise false.
   *
   * @param {string} langTag - A BCP-47 language tag
   * @returns {boolean}
   */
  static shouldNeverTranslateLanguage(langTag) {
    return lazy.neverTranslateLangTags.has(langTag);
  }

  /**
   * Returns true if the current site is denied permissions to translate,
   * otherwise returns false.
   *
   * @returns {Promise<boolean>}
   */
  shouldNeverTranslateSite() {
    const perms = Services.perms;
    const permission = perms.getPermissionObject(
      this.browsingContext.currentWindowGlobal.documentPrincipal,
      TRANSLATIONS_PERMISSION,
      /* exactHost */ false
    );
    return permission?.capability === perms.DENY_ACTION;
  }

  /**
   * Removes the given language tag from the given preference.
   *
   * @param {string} langTag - A BCP-47 language tag
   * @param {string} prefName - The pref name
   */
  static removeLangTagFromPref(langTag, prefName) {
    const langTags =
      prefName === ALWAYS_TRANSLATE_LANGS_PREF
        ? lazy.alwaysTranslateLangTags
        : lazy.neverTranslateLangTags;
    const newLangTags = [...langTags].filter(tag => tag !== langTag);
    Services.prefs.setCharPref(prefName, [...newLangTags].join(","));
  }

  /**
   * Adds the given language tag to the given preference.
   *
   * @param {string} langTag - A BCP-47 language tag
   * @param {string} prefName - The pref name
   */
  static addLangTagToPref(langTag, prefName) {
    const langTags =
      prefName === ALWAYS_TRANSLATE_LANGS_PREF
        ? lazy.alwaysTranslateLangTags
        : lazy.neverTranslateLangTags;
    if (!langTags.has(langTag)) {
      langTags.add(langTag);
    }
    Services.prefs.setCharPref(prefName, [...langTags].join(","));
  }

  /**
   * Toggles the always-translate language preference by adding the language
   * to the pref list if it is not present, or removing it if it is present.
   *
   * @param {LangTags} langTags
   * @returns {boolean}
   *  True if always-translate was enabled for this language.
   *  False if always-translate was disabled for this language.
   */
  static toggleAlwaysTranslateLanguagePref(langTags) {
    const { docLangTag, appLangTag } = langTags;

    if (appLangTag === docLangTag) {
      // In case somehow the user attempts to toggle this when the app and doc language
      // are the same, just remove the lang tag.
      this.removeLangTagFromPref(appLangTag, ALWAYS_TRANSLATE_LANGS_PREF);
      return false;
    }

    if (TranslationsParent.shouldAlwaysTranslateLanguage(langTags)) {
      // The pref was toggled off for this langTag
      this.removeLangTagFromPref(docLangTag, ALWAYS_TRANSLATE_LANGS_PREF);
      return false;
    }

    // The pref was toggled on for this langTag
    this.addLangTagToPref(docLangTag, ALWAYS_TRANSLATE_LANGS_PREF);
    this.removeLangTagFromPref(docLangTag, NEVER_TRANSLATE_LANGS_PREF);
    return true;
  }

  /**
   * Toggle the automatically popup pref, which will either
   * enable or disable translations being offered to the user.
   *
   * @returns {boolean}
   *  True if offering translations was enabled by this call.
   *  False if offering translations was disabled by this call.
   */
  static toggleAutomaticallyPopupPref() {
    const prefValueBeforeToggle = lazy.automaticallyPopupPref;
    Services.prefs.setBoolPref(
      "browser.translations.automaticallyPopup",
      !prefValueBeforeToggle
    );
    return !prefValueBeforeToggle;
  }

  /**
   * Toggles the never-translate language preference by adding the language
   * to the pref list if it is not present, or removing it if it is present.
   *
   * @param {string} langTag - A BCP-47 language tag
   * @returns {boolean} Whether the pref was toggled on or off for this langTag.
   *  True if never-translate was enabled for this language.
   *  False if never-translate was disabled for this language.
   */
  static toggleNeverTranslateLanguagePref(langTag) {
    if (TranslationsParent.shouldNeverTranslateLanguage(langTag)) {
      // The pref was toggled off for this langTag
      this.removeLangTagFromPref(langTag, NEVER_TRANSLATE_LANGS_PREF);
      return false;
    }

    // The pref was toggled on for this langTag
    this.addLangTagToPref(langTag, NEVER_TRANSLATE_LANGS_PREF);
    this.removeLangTagFromPref(langTag, ALWAYS_TRANSLATE_LANGS_PREF);
    return true;
  }

  /**
   * Toggles the never-translate site permissions by adding DENY_ACTION to
   * the site principal if it is not present, or removing it if it is present.
   *
   * @returns {boolean}
   *  True if never-translate was enabled for this site.
   *  False if never-translate was disabled for this site.
   */
  toggleNeverTranslateSitePermissions() {
    if (this.shouldNeverTranslateSite()) {
      return this.setNeverTranslateSitePermissions(false);
    }

    return this.setNeverTranslateSitePermissions(true);
  }

  /**
   * Sets the never-translate site permissions by adding DENY_ACTION to
   * the site principal.
   *
   * @param {string} neverTranslate - The never translate setting.
   * @returns {boolean}
   *  True if never-translate was enabled for this site.
   *  False if never-translate was disabled for this site.
   */
  setNeverTranslateSitePermissions(neverTranslate) {
    const { documentPrincipal } = this.browsingContext.currentWindowGlobal;
    return TranslationsParent.#setNeverTranslateSiteByPrincipal(
      neverTranslate,
      documentPrincipal
    );
  }

  /**
   * Sets the never-translate site permissions by creating a principal from the URL origin
   * and setting or unsetting the DENY_ACTION on the permission.
   *
   * @param {string} neverTranslate - The never translate setting to use.
   * @param {string} urlOrigin - The url origin to set the permission for.
   * @returns {boolean}
   *  True if never-translate was enabled for this origin.
   *  False if never-translate was disabled for this origin.
   */
  static setNeverTranslateSiteByOrigin(neverTranslate, urlOrigin) {
    const principal =
      Services.scriptSecurityManager.createContentPrincipalFromOrigin(
        urlOrigin
      );
    return TranslationsParent.#setNeverTranslateSiteByPrincipal(
      neverTranslate,
      principal
    );
  }

  /**
   * Sets the never-translate site permissions by adding DENY_ACTION to
   * the specified site principal.
   *
   * @param {string} neverTranslate - The never translate setting.
   * @param {string} principal - The principal that should have the permission attached.
   * @returns {boolean}
   *  True if never-translate was enabled for this principal.
   *  False if never-translate was disabled for this principal.
   */
  static #setNeverTranslateSiteByPrincipal(neverTranslate, principal) {
    const perms = Services.perms;

    if (!neverTranslate) {
      perms.removeFromPrincipal(principal, TRANSLATIONS_PERMISSION);
      return false;
    }

    perms.addFromPrincipal(
      principal,
      TRANSLATIONS_PERMISSION,
      perms.DENY_ACTION
    );
    return true;
  }

  /**
   * Creates a list of URLs that have a translations permission set on the resource.
   * These are the sites to never translate.
   *
   * @returns {Array<string>} String array with the URL of the sites that have the never translate permission.
   */
  static listNeverTranslateSites() {
    const neverTranslateSites = [];
    for (const perm of Services.perms.getAllByTypes([
      TRANSLATIONS_PERMISSION,
    ])) {
      if (perm.capability === Services.perms.DENY_ACTION) {
        neverTranslateSites.push(perm.principal.origin);
      }
    }
    let stripProtocol = s => s?.replace(/^\w+:/, "") || "";
    return neverTranslateSites.sort((a, b) => {
      return stripProtocol(a).localeCompare(stripProtocol(b));
    });
  }

  /**
   * Ensure that the translations are always destroyed, even if the content translations
   * are misbehaving.
   */
  #ensureTranslationsDiscarded() {
    if (!lazy.EngineProcess.translationsEngineParent) {
      return;
    }
    lazy.EngineProcess.translationsEngineParent
      // If the engine fails to load, ignore it since we are ending translations.
      .catch(() => null)
      .then(actor => {
        if (actor && this.languageState.requestedTranslationPair) {
          actor.discardTranslations(this.innerWindowId);
        }
      })
      // This error will be one from the endTranslation code, which we need to
      // surface.
      .catch(error => lazy.console.error(error));
  }

  didDestroy() {
    if (!this.innerWindowId) {
      throw new Error(
        "The innerWindowId for the TranslationsParent was not available."
      );
    }

    this.#ensureTranslationsDiscarded();

    this.#isDestroyed = true;
  }
}

/**
 * Validate some simple Wasm that uses a SIMD operation.
 */
function detectSimdSupport() {
  try {
    return WebAssembly.validate(
      new Uint8Array(
        // ```
        // ;; Detect SIMD support.
        // ;; Compile by running: wat2wasm --enable-all simd-detect.wat
        //
        // (module
        //   (func (result v128)
        //     i32.const 0
        //     i8x16.splat
        //     i8x16.popcnt
        //   )
        // )
        // ```

        // prettier-ignore
        [
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x60, 0x00,
        0x01, 0x7b, 0x03, 0x02, 0x01, 0x00, 0x0a, 0x0a, 0x01, 0x08, 0x00, 0x41, 0x00,
        0xfd, 0x0f, 0xfd, 0x62, 0x0b
      ]
      )
    );
  } catch {
    return false;
  }
}

/**
 * State that affects the UI. Any of the state that gets set triggers a dispatch to update
 * the UI.
 */
class TranslationsLanguageState {
  /**
   * @param {TranslationsParent} actor
   * @param {LangTags | null} previousDetectedLanguages
   */
  constructor(actor, previousDetectedLanguages = null) {
    this.#actor = actor;
    this.#detectedLanguages = previousDetectedLanguages;
  }

  /**
   * The data members for TranslationsLanguageState, see the getters for their
   * documentation.
   */

  /** @type {TranslationsParent} */
  #actor;

  /** @type {TranslationPair | null} */
  #requestedTranslationPair = null;

  /** @type {LangTags | null} */
  #detectedLanguages = null;

  /** @type {boolean} */
  #hasVisibleChange = false;

  /** @type {null | TranslationErrors} */
  #error = null;

  #isEngineReady = false;

  /**
   * Dispatch anytime the language details change, so that any UI can react to it.
   */
  dispatch() {
    const browser = this.#actor.browsingContext.top.embedderElement;
    if (!browser) {
      return;
    }
    const { CustomEvent } = browser.ownerGlobal;
    browser.dispatchEvent(
      new CustomEvent("TranslationsParent:LanguageState", {
        bubbles: true,
        detail: {
          actor: this.#actor,
        },
      })
    );
  }

  /**
   * When a translation is requested, this contains the translation pair. This means
   * that the TranslationsChild should be creating a TranslationsDocument and keep
   * the page updated with the target language.
   *
   * @returns {TranslationPair | null}
   */
  get requestedTranslationPair() {
    return this.#requestedTranslationPair;
  }

  set requestedTranslationPair(requestedTranslationPair) {
    if (this.#requestedTranslationPair === requestedTranslationPair) {
      return;
    }

    this.#error = null;
    this.#isEngineReady = false;
    this.#requestedTranslationPair = requestedTranslationPair;
    this.dispatch();
  }

  /**
   * The stored results for the detected languages.
   *
   * @returns {LangTags | null}
   */
  get detectedLanguages() {
    return this.#detectedLanguages;
  }

  set detectedLanguages(detectedLanguages) {
    if (this.#detectedLanguages === detectedLanguages) {
      return;
    }

    this.#detectedLanguages = detectedLanguages;
    this.dispatch();
  }

  /**
   * A visual translation change occurred on the DOM.
   *
   * @returns {boolean}
   */
  get hasVisibleChange() {
    return this.#hasVisibleChange;
  }

  set hasVisibleChange(hasVisibleChange) {
    if (this.#hasVisibleChange === hasVisibleChange) {
      return;
    }

    this.#hasVisibleChange = hasVisibleChange;
    this.dispatch();
  }

  /**
   * When the location changes remove the previous error and dispatch a change event
   * so that any browser chrome UI that needs to be updated can get the latest state.
   */
  locationChanged() {
    this.#error = null;
    this.dispatch();
  }

  /**
   * The last error that occured during translation.
   */
  get error() {
    return this.#error;
  }

  set error(error) {
    if (this.#error === error) {
      return;
    }
    this.#error = error;
    // Setting an error invalidates the requested translation pair.
    this.#requestedTranslationPair = null;
    this.#isEngineReady = false;
    this.dispatch();
  }

  /**
   * Stores when the translations engine is ready. The wasm and language files must
   * be downloaded, which can take some time.
   */
  get isEngineReady() {
    return this.#isEngineReady;
  }

  set isEngineReady(isEngineReady) {
    if (this.#isEngineReady === isEngineReady) {
      return;
    }
    this.#isEngineReady = isEngineReady;
    this.dispatch();
  }
}

/**
 * @typedef {object} QueueItem
 * @property {Function} download
 * @property {Function} [onSuccess]
 * @property {Function} [onFailure]
 * @property {number} [retriesLeft]
 */

/**
 * Manage the download of the files by providing a maximum number of concurrent files
 * and the ability to retry a file download in case of an error.
 *
 * @param {QueueItem[]} queue
 */
async function downloadManager(queue) {
  const NOOP = () => {};

  const pendingDownloadAttempts = new Set();
  let failCount = 0;
  let index = 0;
  const start = Date.now();
  const originalQueueLength = queue.length;

  while (index < queue.length || pendingDownloadAttempts.size > 0) {
    // Start new downloads up to the maximum limit
    while (
      index < queue.length &&
      pendingDownloadAttempts.size < TranslationsParent.MAX_CONCURRENT_DOWNLOADS
    ) {
      lazy.console.log(`Starting download ${index + 1} of ${queue.length}`);

      const {
        download,
        onSuccess = NOOP,
        onFailure = NOOP,
        retriesLeft = TranslationsParent.MAX_DOWNLOAD_RETRIES,
      } = queue[index];

      const handleFailedDownload = error => {
        // The download failed. Either retry it, or report the failure.
        TranslationsParent.reportError(
          new Error("Failed to download file."),
          error
        );

        const newRetriesLeft = retriesLeft - 1;

        if (retriesLeft > 0) {
          lazy.console.log(
            `Queueing another attempt. ${newRetriesLeft} attempts left.`
          );
          queue.push({
            download,
            retriesLeft: newRetriesLeft,
            onSuccess,
            onFailure,
          });
        } else {
          // Give up on this download.
          failCount++;
          onFailure();
        }
      };

      const afterDownloadAttempt = () => {
        pendingDownloadAttempts.delete(downloadAttempt);
      };

      // Kick off the download. If it fails, retry it a certain number of attempts.
      // This is done asynchronously from the rest of the for loop.
      const downloadAttempt = download()
        .then(onSuccess, handleFailedDownload)
        .then(afterDownloadAttempt);

      pendingDownloadAttempts.add(downloadAttempt);
      index++;
    }

    // Wait for any active downloads to complete.
    await Promise.race(pendingDownloadAttempts);
  }

  const duration = ((Date.now() - start) / 1000).toFixed(3);

  if (failCount > 0) {
    const message = `Finished downloads in ${duration} seconds, but ${failCount} download(s) failed.`;
    lazy.console.log(
      `Finished downloads in ${duration} seconds, but ${failCount} download(s) failed.`
    );
    throw new Error(message);
  }

  lazy.console.log(
    `Finished ${originalQueueLength} downloads in ${duration} seconds.`
  );
}

/**
 * The translations code has lots of async code and fallible network requests. To test
 * this manually while using the feature, enable chaos mode by setting "errors" to true
 * and "timeoutMS" to a positive number of milliseconds.
 * prefs to true:
 *
 *  - browser.translations.chaos.timeoutMS
 *  - browser.translations.chaos.errors
 */
async function chaosMode(probability = 0.5) {
  await chaosModeTimer();
  await chaosModeError(probability);
}

/**
 * The translations code has lots of async code that relies on the network. To test
 * this manually while using the feature, enable chaos mode by setting the following pref
 * to a positive number of milliseconds.
 *
 *  - browser.translations.chaos.timeoutMS
 */
async function chaosModeTimer() {
  if (lazy.chaosTimeoutMSPref) {
    const timeout = Math.random() * lazy.chaosTimeoutMSPref;
    lazy.console.log(
      `Chaos mode timer started for ${(timeout / 1000).toFixed(1)} seconds.`
    );
    await new Promise(resolve => lazy.setTimeout(resolve, timeout));
  }
}

/**
 * The translations code has lots of async code that is fallible. To test this manually
 * while using the feature, enable chaos mode by setting the following pref to true.
 *
 *  - browser.translations.chaos.errors
 */
async function chaosModeError(probability = 0.5) {
  if (lazy.chaosErrorsPref && Math.random() < probability) {
    lazy.console.trace(`Chaos mode error generated.`);
    throw new Error(
      `Chaos Mode error from the pref "browser.translations.chaos.errors".`
    );
  }
}
