/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

import { PromiseUtils } from "resource://gre/modules/PromiseUtils.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AppProvidedSearchEngine:
    "resource://gre/modules/AppProvidedSearchEngine.sys.mjs",
  AddonSearchEngine: "resource://gre/modules/AddonSearchEngine.sys.mjs",
  IgnoreLists: "resource://gre/modules/IgnoreLists.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  OpenSearchEngine: "resource://gre/modules/OpenSearchEngine.sys.mjs",
  PolicySearchEngine: "resource://gre/modules/PolicySearchEngine.sys.mjs",
  Region: "resource://gre/modules/Region.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  SearchEngine: "resource://gre/modules/SearchEngine.sys.mjs",
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.sys.mjs",
  SearchEngineSelectorOld:
    "resource://gre/modules/SearchEngineSelectorOld.sys.mjs",
  SearchSettings: "resource://gre/modules/SearchSettings.sys.mjs",
  SearchStaticData: "resource://gre/modules/SearchStaticData.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  UserSearchEngine: "resource://gre/modules/UserSearchEngine.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "SearchService",
    maxLogLevel: lazy.SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "timerManager",
  "@mozilla.org/updates/timer-manager;1",
  "nsIUpdateTimerManager"
);

const TOPIC_LOCALES_CHANGE = "intl:app-locales-changed";
const QUIT_APPLICATION_TOPIC = "quit-application";

// The update timer for OpenSearch engines checks in once a day.
const OPENSEARCH_UPDATE_TIMER_TOPIC = "search-engine-update-timer";
const OPENSEARCH_UPDATE_TIMER_INTERVAL = 60 * 60 * 24;

// The default engine update interval, in days. This is only used if an engine
// specifies an updateURL, but not an updateInterval.
const OPENSEARCH_DEFAULT_UPDATE_INTERVAL = 7;

// This is the amount of time we'll be idle for before applying any configuration
// changes.
const RECONFIG_IDLE_TIME_SEC = 5 * 60;

/**
 * A reason that is used in the change of default search engine event telemetry.
 * These are mutally exclusive.
 */
const REASON_CHANGE_MAP = new Map([
  // The cause of the change is unknown.
  [Ci.nsISearchService.CHANGE_REASON_UNKNOWN, "unknown"],
  // The user changed the default search engine via the options in the
  // preferences UI.
  [Ci.nsISearchService.CHANGE_REASON_USER, "user"],
  // The change resulted from the user toggling the "Use this search engine in
  // Private Windows" option in the preferences UI.
  [Ci.nsISearchService.CHANGE_REASON_USER_PRIVATE_SPLIT, "user_private_split"],
  // The user changed the default via keys (cmd/ctrl-up/down) in the separate
  // search bar.
  [Ci.nsISearchService.CHANGE_REASON_USER_SEARCHBAR, "user_searchbar"],
  // The user changed the default via context menu on the one-off buttons in the
  // separate search bar.
  [
    Ci.nsISearchService.CHANGE_REASON_USER_SEARCHBAR_CONTEXT,
    "user_searchbar_context",
  ],
  // An add-on requested the change of default on install, which was either
  // accepted automatically or by the user.
  [Ci.nsISearchService.CHANGE_REASON_ADDON_INSTALL, "addon-install"],
  // An add-on was uninstalled, which caused the engine to be uninstalled.
  [Ci.nsISearchService.CHANGE_REASON_ADDON_UNINSTALL, "addon-uninstall"],
  // A configuration update caused a change of default.
  [Ci.nsISearchService.CHANGE_REASON_CONFIG, "config"],
  // A locale update caused a change of default.
  [Ci.nsISearchService.CHANGE_REASON_LOCALE, "locale"],
  // A region update caused a change of default.
  [Ci.nsISearchService.CHANGE_REASON_REGION, "region"],
  // Turning on/off an experiment caused a change of default.
  [Ci.nsISearchService.CHANGE_REASON_EXPERIMENT, "experiment"],
  // An enterprise policy caused a change of default.
  [Ci.nsISearchService.CHANGE_REASON_ENTERPRISE, "enterprise"],
  // The UI Tour caused a change of default.
  [Ci.nsISearchService.CHANGE_REASON_UITOUR, "uitour"],
]);

/**
 * The ParseSubmissionResult contains getter methods that return attributes
 * about the parsed submission url.
 *
 * @implements {nsIParseSubmissionResult}
 */
class ParseSubmissionResult {
  constructor(engine, terms, termsParameterName) {
    this.#engine = engine;
    this.#terms = terms;
    this.#termsParameterName = termsParameterName;
  }

  get engine() {
    return this.#engine;
  }

  get terms() {
    return this.#terms;
  }

  get termsParameterName() {
    return this.#termsParameterName;
  }

  /**
   * The search engine associated with the URL passed in to
   * nsISearchEngine::parseSubmissionURL, or null if the URL does not represent
   * a search submission.
   *
   * @type {nsISearchEngine|null}
   */
  #engine;

  /**
   * String containing the sought terms. This can be an empty string in case no
   * terms were specified or the URL does not represent a search submission.*
   *
   * @type {string}
   */
  #terms;

  /**
   * The name of the query parameter used by `engine` for queries. E.g. "q".
   *
   * @type {string}
   */
  #termsParameterName;

  QueryInterface = ChromeUtils.generateQI(["nsISearchParseSubmissionResult"]);
}

const gEmptyParseSubmissionResult = Object.freeze(
  new ParseSubmissionResult(null, "", "")
);

/**
 * The search service handles loading and maintaining of search engines. It will
 * also work out the default lists for each locale/region.
 *
 * @implements {nsISearchService}
 */
export class SearchService {
  constructor() {
    // this._engines is prefixed with _ rather than # because it is called from
    // a test.
    this._engines = new Map();
    this._settings = new lazy.SearchSettings(this);

    this.#defineLazyPreferenceGetters();
  }

  classID = Components.ID("{7319788a-fe93-4db3-9f39-818cf08f4256}");

  get defaultEngine() {
    this.#ensureInitialized();
    return this._getEngineDefault(false);
  }

  set defaultEngine(newEngine) {
    this.#ensureInitialized();
    this.#setEngineDefault(false, newEngine);
  }

  get defaultPrivateEngine() {
    this.#ensureInitialized();
    return this._getEngineDefault(this.#separatePrivateDefault);
  }

  set defaultPrivateEngine(newEngine) {
    this.#ensureInitialized();
    if (!this._separatePrivateDefaultPrefValue) {
      Services.prefs.setBoolPref(
        lazy.SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
        true
      );
    }
    this.#setEngineDefault(this.#separatePrivateDefault, newEngine);
  }

  async getDefault() {
    await this.init();
    return this.defaultEngine;
  }

  async setDefault(engine, changeSource) {
    await this.init();
    this.#setEngineDefault(false, engine, changeSource);
  }

  async getDefaultPrivate() {
    await this.init();
    return this.defaultPrivateEngine;
  }

  async setDefaultPrivate(engine, changeSource) {
    await this.init();
    if (!this._separatePrivateDefaultPrefValue) {
      Services.prefs.setBoolPref(
        lazy.SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
        true
      );
    }
    this.#setEngineDefault(this.#separatePrivateDefault, engine, changeSource);
  }

  /**
   * @returns {SearchEngine}
   *   The engine that is the default for this locale/region, ignoring any
   *   user changes to the default engine.
   */
  get appDefaultEngine() {
    return this.#appDefaultEngine();
  }

  /**
   * @returns {SearchEngine}
   *   The engine that is the default for this locale/region in private browsing
   *   mode, ignoring any user changes to the default engine.
   *   Note: if there is no default for this locale/region, then the non-private
   *   browsing engine will be returned.
   */
  get appPrivateDefaultEngine() {
    return this.#appDefaultEngine(this.#separatePrivateDefault);
  }

  /**
   * Determine whether initialization has been completed.
   *
   * Clients of the service can use this attribute to quickly determine whether
   * initialization is complete, and decide to trigger some immediate treatment,
   * to launch asynchronous initialization or to bailout.
   *
   * Note that this attribute does not indicate that initialization has
   * succeeded, use hasSuccessfullyInitialized() for that.
   *
   * @returns {boolean}
   *  |true | if the search service has finished its attempt to initialize and
   *          we have an outcome. It could have failed or succeeded during this
   *          process.
   *  |false| if initialization has not been triggered yet or initialization is
   *          still ongoing.
   */
  get isInitialized() {
    return (
      this.#initializationStatus == "success" ||
      this.#initializationStatus == "failed"
    );
  }

  /**
   * Determine whether initialization has been successfully completed.
   *
   * @returns {boolean}
   *  |true | if the search service has succesfully initialized.
   *  |false| if initialization has not been started yet, initialization is
   *          still ongoing or initializaiton has failed.
   */
  get hasSuccessfullyInitialized() {
    return this.#initializationStatus == "success";
  }

  /**
   * A promise that is resolved when initialization has finished. This does not
   * trigger initialization to begin.
   *
   * @returns {Promise}
   *   Resolved when initalization has successfully finished, and rejected if it
   *   has failed.
   */
  get promiseInitialized() {
    return this.#initDeferredPromise.promise;
  }

  getDefaultEngineInfo() {
    let [telemetryId, defaultSearchEngineData] = this.#getEngineInfo(
      this.defaultEngine
    );
    const result = {
      defaultSearchEngine: telemetryId,
      defaultSearchEngineData,
    };

    if (this.#separatePrivateDefault) {
      let [privateTelemetryId, defaultPrivateSearchEngineData] =
        this.#getEngineInfo(this.defaultPrivateEngine);
      result.defaultPrivateSearchEngine = privateTelemetryId;
      result.defaultPrivateSearchEngineData = defaultPrivateSearchEngineData;
    }

    return result;
  }

  /**
   * If possible, please call getEngineById() rather than getEngineByName()
   * because engines are stored as { id: object } in this._engine Map.
   *
   * Returns the engine associated with the name.
   *
   * @param {string} engineName
   *   The name of the engine.
   * @returns {SearchEngine}
   *   The associated engine if found, null otherwise.
   */
  getEngineByName(engineName) {
    this.#ensureInitialized();
    return this.#getEngineByName(engineName);
  }

  /**
   * Returns the engine associated with the name without initialization checks.
   *
   * @param {string} engineName
   *   The name of the engine.
   * @returns {SearchEngine}
   *   The associated engine if found, null otherwise.
   */
  #getEngineByName(engineName) {
    for (let engine of this._engines.values()) {
      if (engine.name == engineName) {
        return engine;
      }
    }

    return null;
  }

  /**
   * Returns the engine associated with the id.
   *
   * @param {string} engineId
   *   The id of the engine.
   * @returns {SearchEngine}
   *   The associated engine if found, null otherwise.
   */
  getEngineById(engineId) {
    this.#ensureInitialized();
    return this._engines.get(engineId) || null;
  }

  async getEngineByAlias(alias) {
    await this.init();
    for (var engine of this._engines.values()) {
      if (engine && engine.aliases.includes(alias)) {
        return engine;
      }
    }
    return null;
  }

  async getEngines() {
    await this.init();
    lazy.logConsole.debug("getEngines: getting all engines");
    return this.#sortedEngines;
  }

  async getVisibleEngines() {
    await this.init(true);
    lazy.logConsole.debug("getVisibleEngines: getting all visible engines");
    return this.#sortedVisibleEngines;
  }

  async getAppProvidedEngines() {
    await this.init();

    return this._sortEnginesByDefaults(
      this.#sortedEngines.filter(e => e.isAppProvided)
    );
  }

  async getEnginesByExtensionID(extensionID) {
    await this.init();
    return this.#getEnginesByExtensionID(extensionID);
  }

  /**
   * This function calls #init to start initialization when it has not been
   * started yet. Otherwise, it returns the pending promise.
   *
   * @returns {Promise}
   *   Returns the pending Promise when #init has started but not yet finished.
   *   | Resolved | when initialization has successfully finished.
   *   | Rejected | when initialization has failed.
   *
   */
  async init() {
    if (["started", "success", "failed"].includes(this.#initializationStatus)) {
      return this.promiseInitialized;
    }
    this.#initializationStatus = "started";
    return this.#init();
  }

  /**
   * Runs background checks for the search service. This is called from
   * BrowserGlue and may be run once per session if the user is idle for
   * long enough.
   */
  async runBackgroundChecks() {
    await this.init();
    await this.#migrateLegacyEngines();
    await this.#checkWebExtensionEngines();
    await this.#addOpenSearchTelemetry();
  }

  /**
   * Test only - reset SearchService data. Ideally this should be replaced
   */
  reset() {
    this.#initializationStatus = "not initialized";
    this.#initDeferredPromise = PromiseUtils.defer();
    this.#startupExtensions = new Set();
    this._engines.clear();
    this._cachedSortedEngines = null;
    this.#currentEngine = null;
    this.#currentPrivateEngine = null;
    this._searchDefault = null;
    this.#searchPrivateDefault = null;
    this.#maybeReloadDebounce = false;
    this._settings._batchTask?.disarm();
  }

  // Test-only function to set SearchService initialization status
  forceInitializationStatusForTests(status) {
    this.#initializationStatus = status;
  }

  /**
   * Test only variable to indicate an error should occur during
   * search service initialization.
   *
   * @type {string}
   */
  errorToThrowInTest = null;

  // Test-only function to reset just the engine selector so that it can
  // load a different configuration.
  resetEngineSelector() {
    if (lazy.SearchUtils.newSearchConfigEnabled) {
      this.#engineSelector = new lazy.SearchEngineSelector(
        this.#handleConfigurationUpdated.bind(this)
      );
    } else {
      this.#engineSelector = new lazy.SearchEngineSelectorOld(
        this.#handleConfigurationUpdated.bind(this)
      );
    }
  }

  resetToAppDefaultEngine() {
    let appDefaultEngine = this.appDefaultEngine;
    appDefaultEngine.hidden = false;
    this.defaultEngine = appDefaultEngine;
  }

  async maybeSetAndOverrideDefault(extension) {
    let searchProvider =
      extension.manifest.chrome_settings_overrides.search_provider;
    let engine = this.getEngineByName(searchProvider.name);
    if (!engine || !engine.isAppProvided || engine.hidden) {
      // If the engine is not application provided, then we shouldn't simply
      // set default to it.
      // If the engine is application provided, but hidden, then we don't
      // switch to it, nor do we try to install it.
      return {
        canChangeToAppProvided: false,
        canInstallEngine: !engine?.hidden,
      };
    }

    if (!this.#defaultOverrideAllowlist) {
      this.#defaultOverrideAllowlist =
        new SearchDefaultOverrideAllowlistHandler();
    }

    if (
      extension.startupReason === "ADDON_INSTALL" ||
      extension.startupReason === "ADDON_ENABLE"
    ) {
      // Don't allow an extension to set the default if it is already the default.
      if (this.defaultEngine.name == searchProvider.name) {
        return {
          canChangeToAppProvided: false,
          canInstallEngine: false,
        };
      }
      if (
        !(await this.#defaultOverrideAllowlist.canOverride(
          extension,
          engine._extensionID
        ))
      ) {
        lazy.logConsole.debug(
          "Allowing default engine to be set to app-provided.",
          extension.id
        );
        // We don't allow overriding the engine in this case, but we can allow
        // the extension to change the default engine.
        return {
          canChangeToAppProvided: true,
          canInstallEngine: false,
        };
      }
      // We're ok to override.
      engine.overrideWithExtension(extension.id, extension.manifest);
      lazy.logConsole.debug(
        "Allowing default engine to be set to app-provided and overridden.",
        extension.id
      );
      return {
        canChangeToAppProvided: true,
        canInstallEngine: false,
      };
    }

    if (
      engine.getAttr("overriddenBy") == extension.id &&
      (await this.#defaultOverrideAllowlist.canOverride(
        extension,
        engine._extensionID
      ))
    ) {
      engine.overrideWithExtension(extension.id, extension.manifest);
      lazy.logConsole.debug(
        "Re-enabling overriding of core extension by",
        extension.id
      );
      return {
        canChangeToAppProvided: true,
        canInstallEngine: false,
      };
    }

    return {
      canChangeToAppProvided: false,
      canInstallEngine: false,
    };
  }

  /**
   * Adds a search engine that is specified from enterprise policies.
   *
   * @param {object} details
   *   An object that simulates the manifest object from a WebExtension. See
   *   the idl for more details.
   */
  async #addPolicyEngine(details) {
    let newEngine = new lazy.PolicySearchEngine({ details });
    let existingEngine = this.#getEngineByName(newEngine.name);
    if (existingEngine) {
      throw Components.Exception(
        "An engine with that name already exists!",
        Cr.NS_ERROR_FILE_ALREADY_EXISTS
      );
    }
    lazy.logConsole.debug("Adding Policy Engine:", newEngine.name);
    this.#addEngineToStore(newEngine);
  }

  /**
   * Adds a search engine that is specified by the user.
   *
   * @param {string} name
   *   The name of the search engine
   * @param {string} url
   *   The url that the search engine uses for searches
   * @param {string} alias
   *   An alias for the search engine
   */
  async addUserEngine(name, url, alias) {
    await this.init();

    let newEngine = new lazy.UserSearchEngine({
      details: { name, url, alias },
    });
    let existingEngine = this.#getEngineByName(newEngine.name);
    if (existingEngine) {
      throw Components.Exception(
        "An engine with that name already exists!",
        Cr.NS_ERROR_FILE_ALREADY_EXISTS
      );
    }
    lazy.logConsole.debug(`Adding ${newEngine.name}`);
    this.#addEngineToStore(newEngine);
  }

  /**
   * Called from the AddonManager when it either installs a new
   * extension containing a search engine definition or an upgrade
   * to an existing one.
   *
   * @param {object} extension
   *   An Extension object containing data about the extension.
   */
  async addEnginesFromExtension(extension) {
    lazy.logConsole.debug("addEnginesFromExtension: " + extension.id);
    // Treat add-on upgrade and downgrades the same - either way, the search
    // engine gets updated, not added. Generally, we don't expect a downgrade,
    // but just in case...
    if (
      extension.startupReason == "ADDON_UPGRADE" ||
      extension.startupReason == "ADDON_DOWNGRADE"
    ) {
      // Bug 1679861 An a upgrade or downgrade could be adding a search engine
      // that was not in a prior version, or the addon may have been blocklisted.
      // In either case, there will not be an existing engine.
      let existing = await this.#upgradeExtensionEngine(extension);
      if (existing?.length) {
        return existing;
      }
    }

    if (extension.isAppProvided) {
      // If we are in the middle of initialization or reloading engines,
      // don't add the engine here. This has been called as the result
      // of _makeEngineFromConfig installing the extension, and that is already
      // handling the addition of the engine.
      if (this.isInitialized && !this._reloadingEngines) {
        let { engines } = await this._fetchEngineSelectorEngines();
        let inConfig = engines.filter(el => el.webExtension.id == extension.id);
        if (inConfig.length) {
          return this.#installExtensionEngine(
            extension,
            inConfig.map(el => el.webExtension.locale)
          );
        }
      }
      lazy.logConsole.debug(
        "addEnginesFromExtension: Ignoring builtIn engine."
      );
      return [];
    }

    // If we havent started SearchService yet, store this extension
    // to install in SearchService.init().
    if (!this.isInitialized) {
      this.#startupExtensions.add(extension);
      return [];
    }

    return this.#installExtensionEngine(extension, [
      lazy.SearchUtils.DEFAULT_TAG,
    ]);
  }

  async addOpenSearchEngine(engineURL, iconURL) {
    lazy.logConsole.debug("addEngine: Adding", engineURL);
    await this.init();
    let errCode;
    try {
      var engine = new lazy.OpenSearchEngine();
      engine._setIcon(iconURL, false);
      errCode = await new Promise(resolve => {
        engine.install(engineURL, errorCode => {
          resolve(errorCode);
        });
      });
      if (errCode) {
        throw errCode;
      }
    } catch (ex) {
      throw Components.Exception(
        "addEngine: Error adding engine:\n" + ex,
        errCode || Cr.NS_ERROR_FAILURE
      );
    }
    this.#maybeStartOpenSearchUpdateTimer();
    return engine;
  }

  async removeWebExtensionEngine(id) {
    if (!this.isInitialized) {
      lazy.logConsole.debug(
        "Delaying removing extension engine on startup:",
        id
      );
      this.#startupRemovedExtensions.add(id);
      return;
    }

    lazy.logConsole.debug("removeWebExtensionEngine:", id);
    for (let engine of this.#getEnginesByExtensionID(id)) {
      await this.removeEngine(engine);
    }
  }

  async removeEngine(engine) {
    await this.init();
    if (!engine) {
      throw Components.Exception(
        "no engine passed to removeEngine!",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    var engineToRemove = null;
    for (var e of this._engines.values()) {
      if (engine.wrappedJSObject == e) {
        engineToRemove = e;
      }
    }

    if (!engineToRemove) {
      throw Components.Exception(
        "removeEngine: Can't find engine to remove!",
        Cr.NS_ERROR_FILE_NOT_FOUND
      );
    }

    engineToRemove.pendingRemoval = true;

    if (engineToRemove == this.defaultEngine) {
      this.#findAndSetNewDefaultEngine({
        privateMode: false,
      });
    }

    // Bug 1575649 - We can't just check the default private engine here when
    // we're not using separate, as that re-checks the normal default, and
    // triggers update of the default search engine, which messes up various
    // tests. Really, removeEngine should always commit to updating any
    // changed defaults.
    if (
      this.#separatePrivateDefault &&
      engineToRemove == this.defaultPrivateEngine
    ) {
      this.#findAndSetNewDefaultEngine({
        privateMode: true,
      });
    }

    if (engineToRemove.inMemory) {
      // Just hide it (the "hidden" setter will notify) and remove its alias to
      // avoid future conflicts with other engines.
      engineToRemove.hidden = true;
      engineToRemove.alias = null;
      engineToRemove.pendingRemoval = false;
    } else {
      // Remove the engine file from disk if we had a legacy file in the profile.
      if (engineToRemove._filePath) {
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        file.persistentDescriptor = engineToRemove._filePath;
        if (file.exists()) {
          file.remove(false);
        }
        engineToRemove._filePath = null;
      }
      this.#internalRemoveEngine(engineToRemove);

      // Since we removed an engine, we may need to update the preferences.
      if (!this.#dontSetUseSavedOrder) {
        this.#saveSortedEngineList();
      }
    }
    lazy.SearchUtils.notifyAction(
      engineToRemove,
      lazy.SearchUtils.MODIFIED_TYPE.REMOVED
    );
  }

  async moveEngine(engine, newIndex) {
    await this.init();
    if (newIndex > this.#sortedEngines.length || newIndex < 0) {
      throw Components.Exception(
        "moveEngine: Index out of bounds!",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    if (
      !(engine instanceof Ci.nsISearchEngine) &&
      !(engine instanceof lazy.SearchEngine)
    ) {
      throw Components.Exception(
        "moveEngine: Invalid engine passed to moveEngine!",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    if (engine.hidden) {
      throw Components.Exception(
        "moveEngine: Can't move a hidden engine!",
        Cr.NS_ERROR_FAILURE
      );
    }

    engine = engine.wrappedJSObject;

    var currentIndex = this.#sortedEngines.indexOf(engine);
    if (currentIndex == -1) {
      throw Components.Exception(
        "moveEngine: Can't find engine to move!",
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    // Our callers only take into account non-hidden engines when calculating
    // newIndex, but we need to move it in the array of all engines, so we
    // need to adjust newIndex accordingly. To do this, we count the number
    // of hidden engines in the list before the engine that we're taking the
    // place of. We do this by first finding newIndexEngine (the engine that
    // we were supposed to replace) and then iterating through the complete
    // engine list until we reach it, increasing newIndex for each hidden
    // engine we find on our way there.
    //
    // This could be further simplified by having our caller pass in
    // newIndexEngine directly instead of newIndex.
    var newIndexEngine = this.#sortedVisibleEngines[newIndex];
    if (!newIndexEngine) {
      throw Components.Exception(
        "moveEngine: Can't find engine to replace!",
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    for (var i = 0; i < this.#sortedEngines.length; ++i) {
      if (newIndexEngine == this.#sortedEngines[i]) {
        break;
      }
      if (this.#sortedEngines[i].hidden) {
        newIndex++;
      }
    }

    if (currentIndex == newIndex) {
      return;
    } // nothing to do!

    // Move the engine
    var movedEngine = this._cachedSortedEngines.splice(currentIndex, 1)[0];
    this._cachedSortedEngines.splice(newIndex, 0, movedEngine);

    lazy.SearchUtils.notifyAction(
      engine,
      lazy.SearchUtils.MODIFIED_TYPE.CHANGED
    );

    // Since we moved an engine, we need to update the preferences.
    this.#saveSortedEngineList();
  }

  restoreDefaultEngines() {
    this.#ensureInitialized();
    for (let e of this._engines.values()) {
      // Unhide all default engines
      if (e.hidden && e.isAppProvided) {
        e.hidden = false;
      }
    }
  }

  parseSubmissionURL(url) {
    if (!this.hasSuccessfullyInitialized) {
      // If search is not initialized or failed initializing, do nothing.
      // This allows us to use this function early in telemetry.
      // The only other consumer of this (places) uses it much later.
      return gEmptyParseSubmissionResult;
    }

    if (!this.#parseSubmissionMap) {
      this.#buildParseSubmissionMap();
    }

    // Extract the elements of the provided URL first.
    let soughtKey, soughtQuery;
    try {
      let soughtUrl = Services.io.newURI(url);

      // Exclude any URL that is not HTTP or HTTPS from the beginning.
      if (soughtUrl.schemeIs("http") && soughtUrl.schemeIs("https")) {
        return gEmptyParseSubmissionResult;
      }

      // Reading these URL properties may fail and raise an exception.
      soughtKey = soughtUrl.host + soughtUrl.filePath.toLowerCase();
      soughtQuery = soughtUrl.query;
    } catch (ex) {
      // Errors while parsing the URL or accessing the properties are not fatal.
      return gEmptyParseSubmissionResult;
    }

    // Look up the domain and path in the map to identify the search engine.
    let mapEntry = this.#parseSubmissionMap.get(soughtKey);
    if (!mapEntry) {
      return gEmptyParseSubmissionResult;
    }

    // Extract the search terms from the parameter, for example "caff%C3%A8"
    // from the URL "https://www.google.com/search?q=caff%C3%A8&client=firefox".
    // We cannot use `URLSearchParams` here as the terms might not be
    // encoded in UTF-8.
    let encodedTerms = null;
    for (let param of soughtQuery.split("&")) {
      let equalPos = param.indexOf("=");
      if (
        equalPos != -1 &&
        param.substr(0, equalPos) == mapEntry.termsParameterName
      ) {
        // This is the parameter we are looking for.
        encodedTerms = param.substr(equalPos + 1);
        break;
      }
    }
    if (encodedTerms === null) {
      return gEmptyParseSubmissionResult;
    }

    // Decode the terms using the charset defined in the search engine.
    let terms;
    try {
      terms = Services.textToSubURI.UnEscapeAndConvert(
        mapEntry.engine.queryCharset,
        encodedTerms.replace(/\+/g, " ")
      );
    } catch (ex) {
      // Decoding errors will cause this match to be ignored.
      return gEmptyParseSubmissionResult;
    }

    return new ParseSubmissionResult(
      mapEntry.engine,
      terms,
      mapEntry.termsParameterName
    );
  }

  /**
   * This is a nsITimerCallback for the timerManager notification that is
   * registered for handling updates to search engines. Only OpenSearch engines
   * have these updates and hence, only those are handled here.
   */
  notify() {
    lazy.logConsole.debug("notify: checking for updates");

    // Walk the engine list, looking for engines whose update time has expired.
    var currentTime = Date.now();
    lazy.logConsole.debug("currentTime:" + currentTime);
    for (let engine of this._engines.values()) {
      if (!(engine instanceof lazy.OpenSearchEngine && engine._hasUpdates)) {
        continue;
      }

      var expirTime = engine.getAttr("updateexpir");
      lazy.logConsole.debug(
        engine.name,
        "expirTime:",
        expirTime,
        "updateURL:",
        engine._updateURL,
        "iconUpdateURL:",
        engine._iconUpdateURL
      );

      var engineExpired = expirTime <= currentTime;

      if (!expirTime || !engineExpired) {
        lazy.logConsole.debug("skipping engine");
        continue;
      }

      lazy.logConsole.debug(engine.name, "has expired");

      engineUpdateService.update(engine);

      // Schedule the next update
      engineUpdateService.scheduleNextUpdate(engine);
    } // end engine iteration
  }

  #currentEngine;
  #currentPrivateEngine;
  #queuedIdle;

  /**
   * A deferred promise that is resolved when initialization has finished.
   *
   * @type {Promise}
   *   Resolved when initalization has successfully finished, and rejected if it
   *   has failed.
   */
  #initDeferredPromise = PromiseUtils.defer();

  /**
   * Indicates if initialization has started, failed, succeeded or has not
   * started yet.
   *
   * These are the statuses:
   *   "not initialized" - The SearchService has not started initialization.
   *   "started" - The SearchService has started initializaiton.
   *   "success" - The SearchService successfully completed initialization.
   *   "failed" - The SearchService failed during initialization.
   *
   * @type {string}
   */
  #initializationStatus = "not initialized";

  /**
   * Indicates if we're already waiting for maybeReloadEngines to be called.
   *
   * @type {boolean}
   */
  #maybeReloadDebounce = false;

  /**
   * Indicates if we're currently in maybeReloadEngines.
   *
   * This is prefixed with _ rather than # because it is
   * called in a test.
   *
   * @type {boolean}
   */
  _reloadingEngines = false;

  /**
   * The engine selector singleton that is managing the engine configuration.
   *
   * @type {SearchEngineSelector|null}
   */
  #engineSelector = null;

  /**
   * Various search engines may be ignored if their submission urls contain a
   * string that is in the list. The list is controlled via remote settings.
   *
   * @type {Array}
   */
  #submissionURLIgnoreList = [];

  /**
   * Various search engines may be ignored if their load path is contained
   * in this list. The list is controlled via remote settings.
   *
   * @type {Array}
   */
  #loadPathIgnoreList = [];

  /**
   * A map of engine display names to `SearchEngine`.
   *
   * @type {Map<string, object>|null}
   */
  _engines = null;

  /**
   * An array of engine short names sorted into display order.
   *
   * @type {Array}
   */
  _cachedSortedEngines = null;

  /**
   * A flag to prevent setting of useSavedOrder when there's non-user
   * activity happening.
   *
   * @type {boolean}
   */
  #dontSetUseSavedOrder = false;

  /**
   * An object containing the {id, locale} of the WebExtension for the default
   * engine, as suggested by the configuration.
   * For the legacy configuration, this is the user visible name.
   *
   * @type {object}
   *
   * This is prefixed with _ rather than # because it is
   * called in a test.
   */
  _searchDefault = null;

  /**
   * An object containing the {id, locale} of the WebExtension for the default
   * engine for private browsing mode, as suggested by the configuration.
   * For the legacy configuration, this is the user visible name.
   *
   * @type {object}
   */
  #searchPrivateDefault = null;

  /**
   * A Set of installed search extensions reported by AddonManager
   * startup before SearchSevice has started. Will be installed
   * during init().
   *
   * @type {Set<object>}
   */
  #startupExtensions = new Set();

  /**
   * A Set of removed search extensions reported by AddonManager
   * startup before SearchSevice has started. Will be removed
   * during init().
   *
   * @type {Set<object>}
   */
  #startupRemovedExtensions = new Set();

  /**
   * A reference to the handler for the default override allow list.
   *
   * @type {SearchDefaultOverrideAllowlistHandler|null}
   */
  #defaultOverrideAllowlist = null;

  /**
   * This map is built lazily after the available search engines change.  It
   * allows quick parsing of an URL representing a search submission into the
   * search engine name and original terms.
   *
   * The keys are strings containing the domain name and lowercase path of the
   * engine submission, for example "www.google.com/search".
   *
   * The values are objects with these properties:
   * {
   *   engine: The associated nsISearchEngine.
   *   termsParameterName: Name of the URL parameter containing the search
   *                       terms, for example "q".
   * }
   */
  #parseSubmissionMap = null;

  /**
   * Keep track of observers have been added.
   *
   * @type {boolean}
   */
  #observersAdded = false;

  /**
   * Keeps track to see if the OpenSearch update timer has been started or not.
   *
   * @type {boolean}
   */
  #openSearchUpdateTimerStarted = false;

  get #sortedEngines() {
    if (!this._cachedSortedEngines) {
      return this.#buildSortedEngineList();
    }
    return this._cachedSortedEngines;
  }
  /**
   * This reflects the combined values of the prefs for enabling the separate
   * private default UI, and for the user choosing a separate private engine.
   * If either one is disabled, then we don't enable the separate private default.
   *
   * @returns {boolean}
   */
  get #separatePrivateDefault() {
    return (
      this._separatePrivateDefaultPrefValue &&
      this._separatePrivateDefaultEnabledPrefValue
    );
  }

  #getEnginesByExtensionID(extensionID) {
    lazy.logConsole.debug("getEngines: getting all engines for", extensionID);
    var engines = this.#sortedEngines.filter(function (engine) {
      return engine._extensionID == extensionID;
    });
    return engines;
  }

  /**
   * Returns the engine associated with the WebExtension details.
   *
   * @param {object} details
   *   Details of the WebExtension.
   * @param {string} details.id
   *   The WebExtension ID
   * @param {string} details.locale
   *   The WebExtension locale
   * @returns {nsISearchEngine|null}
   *   The found engine, or null if no engine matched.
   */
  #getEngineByWebExtensionDetails(details) {
    for (const engine of this._engines.values()) {
      if (
        engine._extensionID == details.id &&
        engine._locale == details.locale
      ) {
        return engine;
      }
    }
    return null;
  }

  /**
   * Helper function to get the current default engine.
   *
   * This is prefixed with _ rather than # because it is
   * called in test_remove_engine_notification_box.js
   *
   * @param {boolean} privateMode
   *   If true, returns the default engine for private browsing mode, otherwise
   *   the default engine for the normal mode. Note, this function does not
   *   check the "separatePrivateDefault" preference - that is up to the caller.
   * @returns {nsISearchEngine|null}
   *   The appropriate search engine, or null if one could not be determined.
   */
  _getEngineDefault(privateMode) {
    let currentEngine = privateMode
      ? this.#currentPrivateEngine
      : this.#currentEngine;

    if (currentEngine && !currentEngine.hidden) {
      return currentEngine;
    }

    // No default loaded, so find it from settings.
    const attributeName = privateMode
      ? "privateDefaultEngineId"
      : "defaultEngineId";

    let engineId = this._settings.getMetaDataAttribute(attributeName);
    let engine = this._engines.get(engineId) || null;
    if (
      engine &&
      this._settings.getVerifiedMetaDataAttribute(
        attributeName,
        engine.isAppProvided
      )
    ) {
      if (privateMode) {
        this.#currentPrivateEngine = engine;
      } else {
        this.#currentEngine = engine;
      }
    }
    if (!engineId) {
      if (privateMode) {
        this.#currentPrivateEngine = this.appPrivateDefaultEngine;
      } else {
        this.#currentEngine = this.appDefaultEngine;
      }
    }

    currentEngine = privateMode
      ? this.#currentPrivateEngine
      : this.#currentEngine;
    if (currentEngine && !currentEngine.hidden) {
      return currentEngine;
    }
    // No default in settings or it is hidden, so find the new default.
    return this.#findAndSetNewDefaultEngine({ privateMode });
  }

  /**
   * If initialization has not been completed yet, perform synchronous
   * initialization.
   * Throws in case of initialization error.
   */
  #ensureInitialized() {
    if (this.#initializationStatus === "success") {
      return;
    }

    if (this.#initializationStatus === "failed") {
      throw new Error("SearchService failed while it was initializing.");
    }

    let err = new Error(
      "Something tried to use the search service before it finished " +
        "initializing. Please examine the stack trace to figure out what and " +
        "where to fix it:\n"
    );
    err.message += err.stack;
    throw err;
  }

  /**
   * Define lazy preference getters for separate private default engine in
   * private browsing mode.
   */
  #defineLazyPreferenceGetters() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_separatePrivateDefaultPrefValue",
      lazy.SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
      false,
      this.#onSeparateDefaultPrefChanged.bind(this)
    );

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_separatePrivateDefaultEnabledPrefValue",
      lazy.SearchUtils.BROWSER_SEARCH_PREF +
        "separatePrivateDefault.ui.enabled",
      false,
      this.#onSeparateDefaultPrefChanged.bind(this)
    );

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "separatePrivateDefaultUrlbarResultEnabled",
      lazy.SearchUtils.BROWSER_SEARCH_PREF +
        "separatePrivateDefault.urlbarResult.enabled",
      false
    );
  }

  /**
   * This function adds observers, retrieves the search engine ignore list, and
   * initializes the Search Engine Selector prior to doing the core tasks of
   * search service initialization.
   *
   */
  #doPreInitWork() {
    // We need to catch the region being updated during initialization so we
    // start listening straight away.
    Services.obs.addObserver(this, lazy.Region.REGION_TOPIC);

    this.#getIgnoreListAndSubscribe().catch(ex =>
      console.error(ex, "Search Service could not get the ignore list.")
    );

    if (lazy.SearchUtils.newSearchConfigEnabled) {
      this.#engineSelector = new lazy.SearchEngineSelector(
        this.#handleConfigurationUpdated.bind(this)
      );
    } else {
      this.#engineSelector = new lazy.SearchEngineSelectorOld(
        this.#handleConfigurationUpdated.bind(this)
      );
    }
  }

  /**
   * This function fetches information to load search engines and ensures the
   * search service is in the correct state for external callers to interact
   * with it.
   *
   * This function sets #initDeferredPromise to resolve or reject.
   *   | Resolved | when initalization has successfully finished.
   *   | Rejected | when initialization has failed.
   */
  async #init() {
    lazy.logConsole.debug("init");

    const timerId = Glean.searchService.startupTime.start();

    this.#doPreInitWork();

    let initSection;
    try {
      initSection = "Settings";
      this.#maybeThrowErrorInTest(initSection);
      const settings = await this._settings.get();

      initSection = "FetchEngines";
      this.#maybeThrowErrorInTest(initSection);
      const { engines, privateDefault } =
        await this._fetchEngineSelectorEngines();

      initSection = "LoadEngines";
      this.#maybeThrowErrorInTest(initSection);
      await this.#loadEngines(settings, engines, privateDefault);
    } catch (ex) {
      Glean.searchService.initializationStatus[`failed${initSection}`].add();
      Glean.searchService.startupTime.cancel(timerId);

      lazy.logConsole.error("#init: failure initializing search:", ex);
      this.#initializationStatus = "failed";
      this.#initDeferredPromise.reject(ex);

      throw ex;
    }

    // If we've got this far, but the application is now shutting down,
    // then we need to abandon any further work, especially not writing
    // the settings. We do this, because the add-on manager has also
    // started shutting down and as a result, we might have an incomplete
    // picture of the installed search engines. Writing the settings at
    // this stage would potentially mean the user would loose their engine
    // data.
    // We will however, rebuild the settings on next start up if we detect
    // it is necessary.
    if (Services.startup.shuttingDown) {
      Glean.searchService.startupTime.cancel(timerId);

      let ex = Components.Exception(
        "#init: abandoning init due to shutting down",
        Cr.NS_ERROR_ABORT
      );

      this.#initializationStatus = "failed";
      this.#initDeferredPromise.reject(ex);
      throw ex;
    }

    this.#initializationStatus = "success";
    Glean.searchService.initializationStatus.success.add();
    this.#initDeferredPromise.resolve();
    this.#addObservers();

    Glean.searchService.startupTime.stopAndAccumulate(timerId);

    this.#recordTelemetryData();

    Services.obs.notifyObservers(
      null,
      lazy.SearchUtils.TOPIC_SEARCH_SERVICE,
      "init-complete"
    );

    lazy.logConsole.debug("Completed #init");
    this.#doPostInitWork();
  }

  /**
   * This function records telemetry, checks experiment updates, sets up a timer
   * for opensearch, removes any necessary Add-on engines immediately after the
   * search service has successfully initialized.
   *
   */
  #doPostInitWork() {
    // It is possible that Nimbus could have called onUpdate before
    // we started listening, so do a check on startup.
    Services.tm.dispatchToMainThread(async () => {
      await lazy.NimbusFeatures.searchConfiguration.ready();
      this.#checkNimbusPrefs(true);
    });

    this.#maybeStartOpenSearchUpdateTimer();

    if (this.#startupRemovedExtensions.size) {
      Services.tm.dispatchToMainThread(async () => {
        // Now that init() has successfully finished, we remove any engines
        // that have had their add-ons removed by the add-on manager.
        // We do this after init() has complete, as that allows us to use
        // removeEngine to look after any default engine changes as well.
        // This could cause a slight flicker on startup, but it should be
        // a rare action.
        lazy.logConsole.debug("Removing delayed extension engines");
        for (let id of this.#startupRemovedExtensions) {
          for (let engine of this.#getEnginesByExtensionID(id)) {
            // Only do this for non-application provided engines. We shouldn't
            // ever get application provided engines removed here, but just in case.
            if (!engine.isAppProvided) {
              await this.removeEngine(engine);
            }
          }
        }
        this.#startupRemovedExtensions.clear();
      });
    }
  }

  /**
   * Obtains the ignore list from remote settings. This should only be
   * called from init(). Any subsequent updates to the remote settings are
   * handled via a sync listener.
   *
   */
  async #getIgnoreListAndSubscribe() {
    let listener = this.#handleIgnoreListUpdated.bind(this);
    const current = await lazy.IgnoreLists.getAndSubscribe(listener);

    // Only save the listener after the subscribe, otherwise for tests it might
    // not be fully set up by the time we remove it again.
    this.ignoreListListener = listener;

    await this.#handleIgnoreListUpdated({ data: { current } });
    Services.obs.notifyObservers(
      null,
      lazy.SearchUtils.TOPIC_SEARCH_SERVICE,
      "settings-update-complete"
    );
  }

  /**
   * This handles updating of the ignore list settings, and removing any ignored
   * engines.
   *
   * @param {object} eventData
   *   The event in the format received from RemoteSettings.
   */
  async #handleIgnoreListUpdated(eventData) {
    lazy.logConsole.debug("#handleIgnoreListUpdated");
    const {
      data: { current },
    } = eventData;

    for (const entry of current) {
      if (entry.id == "load-paths") {
        this.#loadPathIgnoreList = [...entry.matches];
      } else if (entry.id == "submission-urls") {
        this.#submissionURLIgnoreList = [...entry.matches];
      }
    }

    try {
      await this.promiseInitialized;
    } catch (ex) {
      // If there's a problem with initialization return early to allow
      // search service to continue in a limited mode without engines.
      return;
    }

    // We try to remove engines manually, as this should be more efficient and
    // we don't really want to cause a re-init as this upsets unit tests.
    let engineRemoved = false;
    for (let engine of this._engines.values()) {
      if (this.#engineMatchesIgnoreLists(engine)) {
        await this.removeEngine(engine);
        engineRemoved = true;
      }
    }
    // If we've removed an engine, and we don't have any left, we need to
    // reload the engines - it is possible the settings just had one engine in it,
    // and that is now empty, so we need to load from our main list.
    if (engineRemoved && !this._engines.size) {
      this._maybeReloadEngines().catch(console.error);
    }
  }

  /**
   * Determines if a given engine matches the ignorelists or not.
   *
   * @param {Engine} engine
   *   The engine to check against the ignorelists.
   * @returns {boolean}
   *   Returns true if the engine matches a ignorelists entry.
   */
  #engineMatchesIgnoreLists(engine) {
    if (this.#loadPathIgnoreList.includes(engine._loadPath)) {
      return true;
    }
    let url = engine.searchURLWithNoTerms.spec.toLowerCase();
    if (
      this.#submissionURLIgnoreList.some(code =>
        url.includes(code.toLowerCase())
      )
    ) {
      return true;
    }
    return false;
  }

  /**
   * Handles the search configuration being - adds a wait on the user
   * being idle, before the search engine update gets handled.
   */
  #handleConfigurationUpdated() {
    if (this.#queuedIdle) {
      return;
    }

    this.#queuedIdle = true;

    this.idleService.addIdleObserver(this, RECONFIG_IDLE_TIME_SEC);
  }

  /**
   * Returns the engine that is the default for this locale/region, ignoring any
   * user changes to the default engine.
   *
   * @param {boolean} privateMode
   *   Set to true to return the default engine in private mode,
   *   false for normal mode.
   * @returns {SearchEngine}
   *   The engine that is default.
   */
  #appDefaultEngine(privateMode = false) {
    let defaultEngine = this.#getEngineByWebExtensionDetails(
      privateMode && this.#searchPrivateDefault
        ? this.#searchPrivateDefault
        : this._searchDefault
    );

    if (Services.policies?.status == Ci.nsIEnterprisePolicies.ACTIVE) {
      let activePolicies = Services.policies.getActivePolicies();
      if (activePolicies.SearchEngines) {
        if (activePolicies.SearchEngines.Default) {
          return this.#getEngineByName(activePolicies.SearchEngines.Default);
        }
        if (activePolicies.SearchEngines.Remove?.includes(defaultEngine.name)) {
          defaultEngine = null;
        }
      }
    }

    if (defaultEngine) {
      return defaultEngine;
    }

    if (privateMode) {
      // If for some reason we can't find the private mode engine, fall back
      // to the non-private one.
      return this.#appDefaultEngine(false);
    }

    // Something unexpected has happened. In order to recover the app default
    // engine, use the first visible engine that is also a general purpose engine.
    // Worst case, we just use the first visible engine.
    defaultEngine = this.#sortedVisibleEngines.find(
      e => e.isGeneralPurposeEngine
    );
    return defaultEngine ? defaultEngine : this.#sortedVisibleEngines[0];
  }

  /**
   * Loads engines asynchronously.
   *
   * @param {object} settings
   *   An object representing the search engine settings.
   * @param {Array} engines
   *   An array containing the engines objects from remote settings.
   * @param {object} privateDefault
   *   An object representing the private default search engine.
   */
  async #loadEngines(settings, engines, privateDefault) {
    // Get user's current settings and search engine before we load engines from
    // config. These values will be compared after engines are loaded.
    let prevMetaData = { ...settings?.metaData };
    let prevCurrentEngineId = prevMetaData.defaultEngineId;
    let prevAppDefaultEngineId = prevMetaData?.appDefaultEngineId;

    lazy.logConsole.debug("#loadEngines: start");
    this.#setDefaultAndOrdersFromSelector(engines, privateDefault);

    // We've done what we can without the add-on manager, now ensure that
    // it has finished starting before we continue.
    if (!lazy.SearchUtils.newSearchConfigEnabled) {
      await lazy.AddonManager.readyPromise;
    }

    let newEngines = await this.#loadEnginesFromConfig(engines);
    for (let engine of newEngines) {
      this.#addEngineToStore(engine);
    }

    if (
      this.#startupExtensions.size &&
      lazy.SearchUtils.newSearchConfigEnabled
    ) {
      await lazy.AddonManager.readyPromise;
    }

    lazy.logConsole.debug(
      "#loadEngines: loading",
      this.#startupExtensions.size,
      "engines reported by AddonManager startup"
    );
    for (let extension of this.#startupExtensions) {
      try {
        await this.#installExtensionEngine(
          extension,
          [lazy.SearchUtils.DEFAULT_TAG],
          true
        );
      } catch (ex) {
        lazy.logConsole.error(
          `#installExtensionEngine failed for ${extension.id}`,
          ex
        );
      }
    }
    this.#startupExtensions.clear();

    this.#loadEnginesFromPolicies();

    this.#loadEnginesFromSettings(settings.engines);

    // Settings file version 6 and below will need a migration to store the
    // engine ids rather than engine names.
    this._settings.migrateEngineIds(settings);

    this.#loadEnginesMetadataFromSettings(settings.engines);

    lazy.logConsole.debug("#loadEngines: done");

    let newCurrentEngine = this._getEngineDefault(false);
    let newCurrentEngineId = newCurrentEngine?.id;

    this._settings.setMetaDataAttribute(
      "appDefaultEngineId",
      this.appDefaultEngine?.id
    );

    if (
      this.#shouldDisplayRemovalOfEngineNotificationBox(
        settings,
        prevMetaData,
        newCurrentEngineId,
        prevCurrentEngineId,
        prevAppDefaultEngineId
      )
    ) {
      let newCurrentEngineName = newCurrentEngine?.name;

      let [prevCurrentEngineName, prevAppDefaultEngineName] = [
        settings.engines.find(e => e.id == prevCurrentEngineId)?._name,
        settings.engines.find(e => e.id == prevAppDefaultEngineId)?._name,
      ];

      this._showRemovalOfSearchEngineNotificationBox(
        prevCurrentEngineName || prevAppDefaultEngineName,
        newCurrentEngineName
      );
    }
  }

  /**
   * Helper function to determine if the removal of search engine notification
   * box should be displayed.
   *
   * @param { object } settings
   *   The user's search engine settings.
   * @param { object } prevMetaData
   *   The user's previous search settings metadata.
   * @param { object } newCurrentEngineId
   *   The user's new current default engine.
   * @param { object } prevCurrentEngineId
   *   The user's previous default engine.
   * @param { object } prevAppDefaultEngineId
   *   The user's previous app default engine.
   * @returns { boolean }
   *   Return true if the previous default engine has been removed and
   *   notification box should be displayed.
   */
  #shouldDisplayRemovalOfEngineNotificationBox(
    settings,
    prevMetaData,
    newCurrentEngineId,
    prevCurrentEngineId,
    prevAppDefaultEngineId
  ) {
    if (
      !Services.prefs.getBoolPref("browser.search.removeEngineInfobar.enabled")
    ) {
      return false;
    }

    // If for some reason we were unable to install any engines and hence no
    // default engine, do not display the notification box
    if (!newCurrentEngineId) {
      return false;
    }

    // If the previous engine is still available, don't show the notification
    // box.
    if (prevCurrentEngineId && this._engines.has(prevCurrentEngineId)) {
      return false;
    }
    if (!prevCurrentEngineId && this._engines.has(prevAppDefaultEngineId)) {
      return false;
    }

    // Don't show the notification if the previous engine was an enterprise engine -
    // the text doesn't quite make sense.
    // let checkPolicyEngineId = prevCurrentEngineId ? prevCurrentEngineId : prevAppDefaultEngineId;
    let checkPolicyEngineId = prevCurrentEngineId || prevAppDefaultEngineId;
    if (checkPolicyEngineId) {
      let engineSettings = settings.engines.find(
        e => e.id == checkPolicyEngineId
      );
      if (engineSettings?._loadPath?.startsWith("[policy]")) {
        return false;
      }
    }

    // If the user's previous engine id is different than the new current
    // engine id, or if the user was using the app default engine and the
    // app default engine id is different than the new current engine id,
    // we check if the user's settings metadata has been upddated.
    if (
      (prevCurrentEngineId && prevCurrentEngineId !== newCurrentEngineId) ||
      (!prevCurrentEngineId &&
        prevAppDefaultEngineId &&
        prevAppDefaultEngineId !== newCurrentEngineId)
    ) {
      // Check settings metadata to detect an update to locale. Sometimes when
      // the user changes their locale it causes a change in engines.
      // If there is no update to settings metadata then the engine change was
      // caused by an update to config rather than a user changing their locale.
      if (!this.#didSettingsMetaDataUpdate(prevMetaData)) {
        return true;
      }
    }

    return false;
  }

  /**
   * Loads engines as specified by the configuration. We only expect
   * configured engines here, user engines should not be listed.
   *
   * @param {Array} engineConfigs
   *   An array of engines configurations based on the schema.
   * @returns {Array.<nsISearchEngine>}
   *   Returns an array of the loaded search engines. This may be
   *   smaller than the original list if not all engines can be loaded.
   */
  async #loadEnginesFromConfig(engineConfigs) {
    lazy.logConsole.debug("#loadEnginesFromConfig");
    let engines = [];
    for (let config of engineConfigs) {
      try {
        let engine = await this._makeEngineFromConfig(config);
        engines.push(engine);
      } catch (ex) {
        console.error(
          `Could not load engine ${
            "webExtension" in config ? config.webExtension.id : "unknown"
          }: ${ex}`
        );
      }
    }
    return engines;
  }

  /**
   * Reloads engines asynchronously, but only when
   * the service has already been initialized.
   *
   * This is prefixed with _ rather than # because it is
   * called in test_reload_engines.js
   *
   * @param {integer} changeReason
   *   The reason reload engines is being called, one of
   *   Ci.nsISearchService.CHANGE_REASON*
   */
  async _maybeReloadEngines(changeReason) {
    if (this.#maybeReloadDebounce) {
      lazy.logConsole.debug("We're already waiting to reload engines.");
      return;
    }

    if (!this.isInitialized || this._reloadingEngines) {
      this.#maybeReloadDebounce = true;
      // Schedule a reload to happen at most 10 seconds after the current run.
      Services.tm.idleDispatchToMainThread(() => {
        if (!this.#maybeReloadDebounce) {
          return;
        }
        this.#maybeReloadDebounce = false;
        this._maybeReloadEngines(changeReason).catch(console.error);
      }, 10000);
      lazy.logConsole.debug(
        "Post-poning maybeReloadEngines() as we're currently initializing."
      );
      return;
    }

    // Before entering `_reloadingEngines` get the settings which we'll need.
    // This also ensures that any pending settings have finished being written,
    // which could otherwise cause data loss.
    let settings = await this._settings.get();

    lazy.logConsole.debug("Running maybeReloadEngines");
    this._reloadingEngines = true;

    try {
      await this._reloadEngines(settings, changeReason);
    } catch (ex) {
      lazy.logConsole.error("maybeReloadEngines failed", ex);
    }
    this._reloadingEngines = false;
    lazy.logConsole.debug("maybeReloadEngines complete");
  }

  // This is prefixed with _ rather than # because it is called in
  // test_remove_engine_notification_box.js
  async _reloadEngines(settings, changeReason) {
    // Capture the current engine state, in case we need to notify below.
    let prevCurrentEngine = this.#currentEngine;
    let prevPrivateEngine = this.#currentPrivateEngine;
    let prevMetaData = { ...settings?.metaData };

    // Ensure that we don't set the useSavedOrder flag whilst we're doing this.
    // This isn't a user action, so we shouldn't be switching it.
    this.#dontSetUseSavedOrder = true;

    // The order of work here is designed to avoid potential issues when updating
    // the default engines, so that we're not removing active defaults or trying
    // to set a default to something that hasn't been added yet. The order is:
    //
    // 1) Update exising engines that are in both the old and new configuration.
    // 2) Add any new engines from the new configuration.
    // 3) Update the default engines.
    // 4) Remove any old engines.

    let { engines: appDefaultConfigEngines, privateDefault } =
      await this._fetchEngineSelectorEngines();

    let configEngines = [...appDefaultConfigEngines];
    let oldEngineList = [...this._engines.values()];

    for (let engine of oldEngineList) {
      if (!engine.isAppProvided) {
        if (engine instanceof lazy.AddonSearchEngine) {
          // If this is an add-on search engine, check to see if it needs
          // an update.
          await engine.update();
        }
        continue;
      }

      let index = configEngines.findIndex(
        e =>
          e.webExtension.id == engine._extensionID &&
          e.webExtension.locale == engine._locale
      );

      if (index == -1) {
        // No engines directly match on id and locale, however, check to see
        // if we have a new entry that matches on id and name - we might just
        // be swapping the in-use locale.
        let replacementEngines = configEngines.filter(
          e => e.webExtension.id == engine._extensionID
        );
        // If there's no possible, or more than one, we treat these as distinct
        // engines so we'll remove the existing engine and add new later if
        // necessary.
        if (replacementEngines.length != 1) {
          engine.pendingRemoval = true;
          continue;
        }

        // Update the index so we can handle the updating below.
        index = configEngines.findIndex(
          e =>
            e.webExtension.id == replacementEngines[0].webExtension.id &&
            e.webExtension.locale == replacementEngines[0].webExtension.locale
        );
        let locale =
          replacementEngines[0].webExtension.locale ||
          lazy.SearchUtils.DEFAULT_TAG;

        // If the name is different, then we must treat the engine as different,
        // and go through the remove and add cycle, rather than modifying the
        // existing one.
        let hasUpdated = await engine.updateIfNoNameChange({
          configuration: configEngines[index],
          locale,
        });
        if (!hasUpdated) {
          // No matching name, so just remove it.
          engine.pendingRemoval = true;
          continue;
        }
      } else {
        // This is an existing engine that we should update (we don't know if
        // the configuration for this engine has changed or not).
        await engine.update({
          configuration: configEngines[index],
          locale: engine._locale,
        });
      }

      configEngines.splice(index, 1);
    }

    // Any remaining configuration engines are ones that we need to add.
    for (let engine of configEngines) {
      try {
        let newEngine = await this._makeEngineFromConfig(engine);
        this.#addEngineToStore(newEngine, true);
      } catch (ex) {
        lazy.logConsole.warn(
          `Could not load engine ${
            "webExtension" in engine ? engine.webExtension.id : "unknown"
          }: ${ex}`
        );
      }
    }
    this.#loadEnginesMetadataFromSettings(settings.engines);

    // Now set the sort out the default engines and notify as appropriate.

    // Clear the current values, so that we'll completely reset.
    this.#currentEngine = null;
    this.#currentPrivateEngine = null;

    // If the user's default is one of the private engines that is being removed,
    // reset the stored setting, so that we correctly detect the change in
    // in default.
    if (prevCurrentEngine?.pendingRemoval) {
      this._settings.setMetaDataAttribute("defaultEngineId", "");
    }
    if (prevPrivateEngine?.pendingRemoval) {
      this._settings.setMetaDataAttribute("privateDefaultEngineId", "");
    }

    this.#setDefaultAndOrdersFromSelector(
      appDefaultConfigEngines,
      privateDefault
    );

    // If the defaultEngine has changed between the previous load and this one,
    // dispatch the appropriate notifications.
    if (prevCurrentEngine && this.defaultEngine !== prevCurrentEngine) {
      this.#recordDefaultChangedEvent(
        false,
        prevCurrentEngine,
        this.defaultEngine,
        changeReason
      );
      lazy.SearchUtils.notifyAction(
        this.#currentEngine,
        lazy.SearchUtils.MODIFIED_TYPE.DEFAULT
      );
      // If we've not got a separate private active, notify update of the
      // private so that the UI updates correctly.
      if (!this.#separatePrivateDefault) {
        lazy.SearchUtils.notifyAction(
          this.#currentEngine,
          lazy.SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
        );
      }

      if (
        prevMetaData &&
        settings.metaData &&
        !this.#didSettingsMetaDataUpdate(prevMetaData) &&
        prevCurrentEngine?.pendingRemoval &&
        Services.prefs.getBoolPref("browser.search.removeEngineInfobar.enabled")
      ) {
        this._showRemovalOfSearchEngineNotificationBox(
          prevCurrentEngine.name,
          this.defaultEngine.name
        );
      }
    }

    if (
      this.#separatePrivateDefault &&
      prevPrivateEngine &&
      this.defaultPrivateEngine !== prevPrivateEngine
    ) {
      this.#recordDefaultChangedEvent(
        true,
        prevPrivateEngine,
        this.defaultPrivateEngine,
        changeReason
      );
      lazy.SearchUtils.notifyAction(
        this.#currentPrivateEngine,
        lazy.SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
      );
    }

    // Finally, remove any engines that need removing. We do this after sorting
    // out the new default, as otherwise this could cause multiple notifications
    // and the wrong engine to be selected as default.

    for (let engine of this._engines.values()) {
      if (!engine.pendingRemoval) {
        continue;
      }

      // If we have other engines that use the same extension ID, then
      // we do not want to remove the add-on - only remove the engine itself.
      let inUseEngines = [...this._engines.values()].filter(
        e => e._extensionID == engine._extensionID
      );

      if (inUseEngines.length <= 1) {
        if (inUseEngines.length == 1 && inUseEngines[0] == engine) {
          // No other engines are using this extension ID.

          // The internal remove is done first to avoid a call to removeEngine
          // which could adjust the sort order when we don't want it to.
          this.#internalRemoveEngine(engine);

          let addon = await lazy.AddonManager.getAddonByID(engine._extensionID);
          if (addon) {
            // AddonManager won't call removeEngine if an engine with the
            // WebExtension id doesn't exist in the search service.
            await addon.uninstall();
          }
        }
        // For the case where `inUseEngines[0] != engine`:
        // This is a situation where there was an engine added earlier in this
        // function with the same name.
        // For example, eBay has the same name for both US and GB, but has
        // a different domain and uses a different locale of the same
        // WebExtension.
        // The result of this is the earlier addition has already replaced
        // the engine in `this._engines` (which is indexed by name), so all that
        // needs to be done here is to pretend the old engine was removed
        // which is notified below.
      } else {
        // More than one engine is using this extension ID, so we don't want to
        // remove the add-on.
        this.#internalRemoveEngine(engine);
      }
      lazy.SearchUtils.notifyAction(
        engine,
        lazy.SearchUtils.MODIFIED_TYPE.REMOVED
      );
    }

    // Save app default engine to the user's settings metaData incase it has
    // been updated
    this._settings.setMetaDataAttribute(
      "appDefaultEngineId",
      this.appDefaultEngine?.id
    );

    // If we are leaving an experiment, and the default is the same as the
    // application default, we reset the user's setting to blank, so that
    // future changes of the application default engine may take effect.
    if (
      prevMetaData.experiment &&
      !this._settings.getMetaDataAttribute("experiment")
    ) {
      if (this.defaultEngine == this.appDefaultEngine) {
        this._settings.setVerifiedMetaDataAttribute("defaultEngineId", "");
      }
      if (
        this.#separatePrivateDefault &&
        this.defaultPrivateEngine == this.appPrivateDefaultEngine
      ) {
        this._settings.setVerifiedMetaDataAttribute(
          "privateDefaultEngineId",
          ""
        );
      }
    }

    this.#dontSetUseSavedOrder = false;
    // Clear out the sorted engines settings, so that we re-sort it if necessary.
    this._cachedSortedEngines = null;
    Services.obs.notifyObservers(
      null,
      lazy.SearchUtils.TOPIC_SEARCH_SERVICE,
      "engines-reloaded"
    );
  }

  #addEngineToStore(engine, skipDuplicateCheck = false) {
    if (this.#engineMatchesIgnoreLists(engine)) {
      lazy.logConsole.debug("#addEngineToStore: Ignoring engine");
      return;
    }

    lazy.logConsole.debug("#addEngineToStore: Adding engine:", engine.name);

    // See if there is an existing engine with the same name. However, if this
    // engine is updating another engine, it's allowed to have the same name.
    var hasSameNameAsUpdate =
      engine._engineToUpdate && engine.name == engine._engineToUpdate.name;
    if (
      !skipDuplicateCheck &&
      this.#getEngineByName(engine.name) &&
      !hasSameNameAsUpdate
    ) {
      lazy.logConsole.debug(
        "#addEngineToStore: Duplicate engine found, aborting!"
      );
      return;
    }

    if (engine._engineToUpdate) {
      // Update the old engine by copying over the properties of the new engine
      // that is loaded. It is necessary to copy over all the "private"
      // properties (those without a getter or setter) from one object to the
      // other. Other callers may hold a reference to the old engine, therefore,
      // anywhere else that has a reference to the old engine will receive
      // the properties that are updated because those other callers
      // are referencing the same nsISearchEngine object in memory.
      for (let p in engine) {
        if (
          !(
            Object.getOwnPropertyDescriptor(engine, p)?.get ||
            Object.getOwnPropertyDescriptor(engine, p)?.set
          )
        ) {
          engine._engineToUpdate[p] = engine[p];
        }
      }

      // The old engine is now updated
      engine = engine._engineToUpdate;
      engine._engineToUpdate = null;

      // Update the engine Map with the updated engine
      this._engines.set(engine.id, engine);

      lazy.SearchUtils.notifyAction(
        engine,
        lazy.SearchUtils.MODIFIED_TYPE.CHANGED
      );
    } else {
      // Not an update, just add the new engine.
      this._engines.set(engine.id, engine);
      // Only add the engine to the list of sorted engines if the initial list
      // has already been built (i.e. if this._cachedSortedEngines is non-null). If
      // it hasn't, we're loading engines from disk and the sorted engine list
      // will be built once we need it.
      if (this._cachedSortedEngines && !this.#dontSetUseSavedOrder) {
        this._cachedSortedEngines.push(engine);
        this.#saveSortedEngineList();
      }
      lazy.SearchUtils.notifyAction(
        engine,
        lazy.SearchUtils.MODIFIED_TYPE.ADDED
      );
    }

    // Let the engine know it can start notifying new updates.
    engine._engineAddedToStore = true;

    if (engine._hasUpdates) {
      // Schedule the engine's next update, if it isn't already.
      if (!engine.getAttr("updateexpir")) {
        engineUpdateService.scheduleNextUpdate(engine);
      }
    }
  }

  #loadEnginesMetadataFromSettings(engineSettings) {
    if (!engineSettings) {
      return;
    }

    for (let engineSetting of engineSettings) {
      let eng = this.#getEngineByName(engineSetting._name);
      if (eng) {
        lazy.logConsole.debug(
          "#loadEnginesMetadataFromSettings, transfering metadata for",
          engineSetting._name,
          engineSetting._metaData
        );

        // We used to store the alias in metadata.alias, in 1621892 that was
        // changed to only store the user set alias in metadata.alias, remove
        // it from metadata if it was previously set to the internal value.
        if (eng._alias === engineSetting?._metaData?.alias) {
          delete engineSetting._metaData.alias;
        }
        eng._metaData = engineSetting._metaData || {};
      }
    }
  }

  #loadEnginesFromPolicies() {
    if (Services.policies?.status != Ci.nsIEnterprisePolicies.ACTIVE) {
      return;
    }

    let activePolicies = Services.policies.getActivePolicies();
    if (!activePolicies.SearchEngines) {
      return;
    }
    for (let engineDetails of activePolicies.SearchEngines.Add ?? []) {
      let details = {
        description: engineDetails.Description,
        iconURL: engineDetails.IconURL ? engineDetails.IconURL.href : null,
        name: engineDetails.Name,
        // If the encoding is not specified or is falsy, we will fall back to
        // the default encoding.
        encoding: engineDetails.Encoding,
        search_url: encodeURI(engineDetails.URLTemplate),
        keyword: engineDetails.Alias,
        search_url_post_params:
          engineDetails.Method == "POST" ? engineDetails.PostData : undefined,
        suggest_url: engineDetails.SuggestURLTemplate,
      };
      this.#addPolicyEngine(details);
    }
  }

  #loadEnginesFromSettings(enginesCache) {
    if (!enginesCache) {
      return;
    }

    lazy.logConsole.debug(
      "#loadEnginesFromSettings: Loading",
      enginesCache.length,
      "engines from settings"
    );

    let skippedEngines = 0;
    for (let engineJSON of enginesCache) {
      // We renamed isBuiltin to isAppProvided in bug 1631898,
      // keep checking isBuiltin for older settings.
      if (engineJSON._isAppProvided || engineJSON._isBuiltin) {
        ++skippedEngines;
        continue;
      }

      // Some OpenSearch type engines are now obsolete and no longer supported.
      // These were application provided engines that used to use the OpenSearch
      // format before gecko transitioned to WebExtensions.
      // These will sometimes have been missed in migration due to various
      // reasons, and due to how the settings saves everything. We therefore
      // explicitly ignore them here to drop them, and let the rest of the code
      // fallback to the application/distribution default if necessary.
      let loadPath = engineJSON._loadPath?.toLowerCase();
      if (
        loadPath &&
        // Replaced by application provided in Firefox 79.
        (loadPath.startsWith("[distribution]") ||
          // Langpack engines moved in-app in Firefox 62.
          // Note: these may be prefixed by jar:,
          loadPath.includes("[app]/extensions/langpack") ||
          loadPath.includes("[other]/langpack") ||
          loadPath.includes("[profile]/extensions/langpack") ||
          // Old omni.ja engines also moved to in-app in Firefox 62.
          loadPath.startsWith("jar:[app]/omni.ja"))
      ) {
        continue;
      }

      try {
        let engine;
        if (loadPath?.startsWith("[policy]")) {
          skippedEngines++;
          continue;
        } else if (loadPath?.startsWith("[user]")) {
          engine = new lazy.UserSearchEngine({ json: engineJSON });
        } else if (engineJSON.extensionID ?? engineJSON._extensionID) {
          engine = new lazy.AddonSearchEngine({
            isAppProvided: false,
            json: engineJSON,
          });
        } else {
          engine = new lazy.OpenSearchEngine({
            json: engineJSON,
          });
        }
        this.#addEngineToStore(engine);
      } catch (ex) {
        lazy.logConsole.error(
          "Failed to load",
          engineJSON._name,
          "from settings:",
          ex,
          engineJSON
        );
      }
    }

    if (skippedEngines) {
      lazy.logConsole.debug(
        "#loadEnginesFromSettings: skipped",
        skippedEngines,
        "built-in/policy engines."
      );
    }
  }

  // This is prefixed with _ rather than # because it is
  // called in test_remove_engine_notification_box.js
  async _fetchEngineSelectorEngines() {
    let searchEngineSelectorProperties = {
      locale: Services.locale.appLocaleAsBCP47,
      region: lazy.Region.home || "default",
      channel: lazy.SearchUtils.MODIFIED_APP_CHANNEL,
      experiment:
        lazy.NimbusFeatures.searchConfiguration.getVariable("experiment") ?? "",
      distroID: lazy.SearchUtils.distroID ?? "",
    };

    for (let [key, value] of Object.entries(searchEngineSelectorProperties)) {
      this._settings.setMetaDataAttribute(key, value);
    }

    let { engines, privateDefault } =
      await this.#engineSelector.fetchEngineConfiguration(
        searchEngineSelectorProperties
      );

    for (let e of engines) {
      if (!e.webExtension) {
        e.webExtension = {};
      }
      e.webExtension.locale =
        e.webExtension?.locale ?? lazy.SearchUtils.DEFAULT_TAG;
    }

    return { engines, privateDefault };
  }

  #setDefaultAndOrdersFromSelector(engines, privateDefault) {
    const defaultEngine = engines[0];
    this._searchDefault = {
      id: defaultEngine.webExtension.id,
      locale: defaultEngine.webExtension.locale,
    };
    if (privateDefault) {
      this.#searchPrivateDefault = {
        id: privateDefault.webExtension.id,
        locale: privateDefault.webExtension.locale,
      };
    }
  }

  #saveSortedEngineList() {
    lazy.logConsole.debug("#saveSortedEngineList");

    // Set the useSavedOrder attribute to indicate that from now on we should
    // use the user's order information stored in settings.
    this._settings.setMetaDataAttribute("useSavedOrder", true);

    var engines = this.#sortedEngines;

    for (var i = 0; i < engines.length; ++i) {
      engines[i].setAttr("order", i + 1);
    }
  }

  #buildSortedEngineList() {
    // We must initialise _cachedSortedEngines here to avoid infinite recursion
    // in the case of tests which don't define a default search engine.
    // If there's no default defined, then we revert to the first item in the
    // sorted list, but we can't do that if we don't have a list.
    this._cachedSortedEngines = [];

    // If the user has specified a custom engine order, read the order
    // information from the metadata instead of the default prefs.
    if (this._settings.getMetaDataAttribute("useSavedOrder")) {
      lazy.logConsole.debug("#buildSortedEngineList: using saved order");
      let addedEngines = {};

      // Flag to keep track of whether or not we need to call #saveSortedEngineList.
      let needToSaveEngineList = false;

      for (let engine of this._engines.values()) {
        var orderNumber = engine.getAttr("order");

        // Since the DB isn't regularly cleared, and engine files may disappear
        // without us knowing, we may already have an engine in this slot. If
        // that happens, we just skip it - it will be added later on as an
        // unsorted engine.
        if (orderNumber && !this._cachedSortedEngines[orderNumber - 1]) {
          this._cachedSortedEngines[orderNumber - 1] = engine;
          addedEngines[engine.name] = engine;
        } else {
          // We need to call #saveSortedEngineList so this gets sorted out.
          needToSaveEngineList = true;
        }
      }

      // Filter out any nulls for engines that may have been removed
      var filteredEngines = this._cachedSortedEngines.filter(function (a) {
        return !!a;
      });
      if (this._cachedSortedEngines.length != filteredEngines.length) {
        needToSaveEngineList = true;
      }
      this._cachedSortedEngines = filteredEngines;

      if (needToSaveEngineList) {
        this.#saveSortedEngineList();
      }

      // Array for the remaining engines, alphabetically sorted.
      let alphaEngines = [];

      for (let engine of this._engines.values()) {
        if (!(engine.name in addedEngines)) {
          alphaEngines.push(engine);
        }
      }

      const collator = new Intl.Collator();
      alphaEngines.sort((a, b) => {
        return collator.compare(a.name, b.name);
      });
      return (this._cachedSortedEngines =
        this._cachedSortedEngines.concat(alphaEngines));
    }
    lazy.logConsole.debug("#buildSortedEngineList: using default orders");

    return (this._cachedSortedEngines = this._sortEnginesByDefaults(
      Array.from(this._engines.values())
    ));
  }

  /**
   * Sorts engines by the default settings (prefs, configuration values).
   *
   * @param {Array} engines
   *   An array of engine objects to sort.
   * @returns {Array}
   *   The sorted array of engine objects.
   *
   * This is a private method with _ rather than # because it is
   * called in a test.
   */
  _sortEnginesByDefaults(engines) {
    const sortedEngines = [];
    const addedEngines = new Set();

    function maybeAddEngineToSort(engine) {
      if (!engine || addedEngines.has(engine.name)) {
        return;
      }

      sortedEngines.push(engine);
      addedEngines.add(engine.name);
    }

    // The app default engine should always be first in the list (except
    // for distros, that we should respect).
    const appDefault = this.appDefaultEngine;
    maybeAddEngineToSort(appDefault);

    // If there's a private default, and it is different to the normal
    // default, then it should be second in the list.
    const appPrivateDefault = this.appPrivateDefaultEngine;
    if (appPrivateDefault && appPrivateDefault != appDefault) {
      maybeAddEngineToSort(appPrivateDefault);
    }

    let remainingEngines;
    const collator = new Intl.Collator();

    remainingEngines = engines.filter(e => !addedEngines.has(e.name));

    // We sort by highest orderHint first, then alphabetically by name.
    remainingEngines.sort((a, b) => {
      if (a._orderHint && b._orderHint) {
        if (a._orderHint == b._orderHint) {
          return collator.compare(a.name, b.name);
        }
        return b._orderHint - a._orderHint;
      }
      if (a._orderHint) {
        return -1;
      }
      if (b._orderHint) {
        return 1;
      }
      return collator.compare(a.name, b.name);
    });

    return [...sortedEngines, ...remainingEngines];
  }

  /**
   * Get a sorted array of the visible engines.
   *
   * @returns {Array<SearchEngine>}
   */

  get #sortedVisibleEngines() {
    return this.#sortedEngines.filter(engine => !engine.hidden);
  }

  /**
   * Migrates legacy add-ons which used the OpenSearch definitions to
   * WebExtensions, if an equivalent WebExtension is installed.
   *
   * Run during the background checks.
   */
  async #migrateLegacyEngines() {
    lazy.logConsole.debug("Running migrate legacy engines");

    const matchRegExp = /extensions\/(.*?)\.xpi!/i;
    for (let engine of this._engines.values()) {
      if (
        !engine.isAppProvided &&
        !engine._extensionID &&
        engine._loadPath.includes("[profile]/extensions/")
      ) {
        let match = engine._loadPath.match(matchRegExp);
        if (match?.[1]) {
          // There's a chance here that the WebExtension might not be
          // installed any longer, even though the engine is. We'll deal
          // with that in `checkWebExtensionEngines`.
          let engines = await this.getEnginesByExtensionID(match[1]);
          if (engines.length) {
            lazy.logConsole.debug(
              `Migrating ${engine.name} to WebExtension install`
            );

            if (this.defaultEngine == engine) {
              this.defaultEngine = engines[0];
            }
            await this.removeEngine(engine);
          }
        }
      }
    }

    lazy.logConsole.debug("Migrate legacy engines complete");
  }

  /**
   * Checks if Search Engines associated with WebExtensions are valid and
   * up-to-date, and reports them via telemetry if not.
   *
   * Run during the background checks.
   */
  async #checkWebExtensionEngines() {
    lazy.logConsole.debug("Running check on WebExtension engines");

    for (let engine of this._engines.values()) {
      if (engine instanceof lazy.AddonSearchEngine && !engine.isAppProvided) {
        await engine.checkAndReportIfSettingsValid();
      }
    }
    lazy.logConsole.debug("WebExtension engine check complete");
  }

  /**
   * Counts the number of secure, insecure, securely updated and insecurely
   * updated OpenSearch engines the user has installed and reports those
   * counts via telemetry.
   *
   * Run during the background checks.
   */
  async #addOpenSearchTelemetry() {
    let totalSecure = 0;
    let totalInsecure = 0;
    let totalWithSecureUpdates = 0;
    let totalWithInsecureUpdates = 0;

    let engine;
    let searchURI;
    let updateURI;
    for (let elem of this._engines) {
      engine = elem[1];
      if (engine instanceof lazy.OpenSearchEngine) {
        searchURI = engine.searchURLWithNoTerms;
        updateURI = engine._updateURI;

        if (lazy.SearchUtils.isSecureURIForOpenSearch(searchURI)) {
          totalSecure++;
        } else {
          totalInsecure++;
        }

        if (updateURI && lazy.SearchUtils.isSecureURIForOpenSearch(updateURI)) {
          totalWithSecureUpdates++;
        } else if (updateURI) {
          totalWithInsecureUpdates++;
        }
      }
    }

    Services.telemetry.scalarSet(
      "browser.searchinit.secure_opensearch_engine_count",
      totalSecure
    );
    Services.telemetry.scalarSet(
      "browser.searchinit.insecure_opensearch_engine_count",
      totalInsecure
    );
    Services.telemetry.scalarSet(
      "browser.searchinit.secure_opensearch_update_count",
      totalWithSecureUpdates
    );
    Services.telemetry.scalarSet(
      "browser.searchinit.insecure_opensearch_update_count",
      totalWithInsecureUpdates
    );
  }

  /**
   * Creates and adds a WebExtension based engine.
   *
   * @param {object} options
   *   Options for the engine.
   * @param {Extension} options.extension
   *   An Extension object containing data about the extension.
   * @param {string} [options.locale]
   *   The locale to use within the WebExtension. Defaults to the WebExtension's
   *   default locale.
   * @param {initEngine} [options.initEngine]
   *   Set to true if this engine is being loaded during initialization.
   */
  async _createAndAddEngine({
    extension,
    locale = lazy.SearchUtils.DEFAULT_TAG,
    initEngine = false,
  }) {
    // If we're in the startup cycle, and we've already loaded this engine,
    // then we use the existing one rather than trying to start from scratch.
    // This also avoids console errors.
    if (extension.startupReason == "APP_STARTUP") {
      let engine = this.#getEngineByWebExtensionDetails({
        id: extension.id,
        locale,
      });
      if (engine) {
        lazy.logConsole.debug(
          "Engine already loaded via settings, skipping due to APP_STARTUP:",
          extension.id
        );
        return engine;
      }
    }

    // We install search extensions during the init phase, both built in
    // web extensions freshly installed (via addEnginesFromExtension) or
    // user installed extensions being reenabled calling this directly.
    if (!this.isInitialized && !extension.isAppProvided && !initEngine) {
      await this.init();
    }

    let isCurrent = false;

    for (let engine of this._engines.values()) {
      if (
        !engine.extensionID &&
        engine._loadPath.startsWith(`jar:[profile]/extensions/${extension.id}`)
      ) {
        // This is a legacy extension engine that needs to be migrated to WebExtensions.
        lazy.logConsole.debug("Migrating existing engine");
        isCurrent = isCurrent || this.defaultEngine == engine;
        await this.removeEngine(engine);
      }
    }

    let newEngine = new lazy.AddonSearchEngine({
      isAppProvided: extension.isAppProvided,
      details: {
        extensionID: extension.id,
        locale,
      },
    });
    await newEngine.init({
      extension,
      locale,
    });

    let existingEngine = this.#getEngineByName(newEngine.name);
    if (existingEngine) {
      throw Components.Exception(
        `An engine called ${newEngine.name} already exists!`,
        Cr.NS_ERROR_FILE_ALREADY_EXISTS
      );
    }

    this.#addEngineToStore(newEngine);
    if (isCurrent) {
      this.defaultEngine = newEngine;
    }
    return newEngine;
  }

  /**
   * Called when we see an upgrade to an existing search extension.
   *
   * @param {object} extension
   *   An Extension object containing data about the extension.
   */
  async #upgradeExtensionEngine(extension) {
    let { engines } = await this._fetchEngineSelectorEngines();
    let extensionEngines = await this.getEnginesByExtensionID(extension.id);

    for (let engine of extensionEngines) {
      let isDefault = engine == this.defaultEngine;
      let isDefaultPrivate = engine == this.defaultPrivateEngine;

      let originalName = engine.name;
      let locale = engine._locale || lazy.SearchUtils.DEFAULT_TAG;
      let configuration =
        engines.find(
          e =>
            e.webExtension.id == extension.id && e.webExtension.locale == locale
        ) ?? {};

      await engine.update({
        configuration,
        extension,
        locale,
      });

      if (engine.name != originalName) {
        if (isDefault) {
          this._settings.setVerifiedMetaDataAttribute(
            "defaultEngineId",
            engine.id
          );
        }
        if (isDefaultPrivate) {
          this._settings.setVerifiedMetaDataAttribute(
            "privateDefaultEngineId",
            engine.id
          );
        }
        this._cachedSortedEngines = null;
      }
    }
    return extensionEngines;
  }

  async #installExtensionEngine(extension, locales, initEngine = false) {
    lazy.logConsole.debug("installExtensionEngine:", extension.id);

    let installLocale = async locale => {
      return this._createAndAddEngine({ extension, locale, initEngine });
    };

    let engines = [];
    for (let locale of locales) {
      lazy.logConsole.debug(
        "addEnginesFromExtension: installing:",
        extension.id,
        ":",
        locale
      );
      engines.push(await installLocale(locale));
    }
    return engines;
  }

  #internalRemoveEngine(engine) {
    // Remove the engine from _sortedEngines
    if (this._cachedSortedEngines) {
      var index = this._cachedSortedEngines.indexOf(engine);
      if (index == -1) {
        throw Components.Exception(
          "Can't find engine to remove in _sortedEngines!",
          Cr.NS_ERROR_FAILURE
        );
      }
      this._cachedSortedEngines.splice(index, 1);
    }

    // Remove the engine from the internal store
    this._engines.delete(engine.id);
  }

  /**
   * Helper function to find a new default engine and set it. This could
   * be used if there is not default set yet, or if the current default is
   * being removed.
   *
   * This function will not consider engines that have a `pendingRemoval`
   * property set to true.
   *
   * The new default will be chosen from (in order):
   *
   * - Existing default from configuration, if it is not hidden.
   * - The first non-hidden engine that is a general search engine.
   * - If all other engines are hidden, unhide the default from the configuration.
   * - If the default from the configuration is the one being removed, unhide
   *   the first general search engine, or first visible engine.
   *
   * @param {boolean} privateMode
   *   If true, returns the default engine for private browsing mode, otherwise
   *   the default engine for the normal mode. Note, this function does not
   *   check the "separatePrivateDefault" preference - that is up to the caller.
   * @returns {nsISearchEngine|null}
   *   The appropriate search engine, or null if one could not be determined.
   */
  #findAndSetNewDefaultEngine({ privateMode }) {
    // First to the app default engine...
    let newDefault = privateMode
      ? this.appPrivateDefaultEngine
      : this.appDefaultEngine;

    if (!newDefault || newDefault.hidden || newDefault.pendingRemoval) {
      let sortedEngines = this.#sortedVisibleEngines;
      let generalSearchEngines = sortedEngines.filter(
        e => e.isGeneralPurposeEngine
      );

      // then to the first visible general search engine that isn't excluded...
      let firstVisible = generalSearchEngines.find(e => !e.pendingRemoval);
      if (firstVisible) {
        newDefault = firstVisible;
      } else if (newDefault) {
        // then to the app default if it is not the one that is excluded...
        if (!newDefault.pendingRemoval) {
          newDefault.hidden = false;
        } else {
          newDefault = null;
        }
      }

      // and finally as a last resort we unhide the first engine
      // even if the name is the same as the excluded one (should never happen).
      if (!newDefault) {
        if (!firstVisible) {
          sortedEngines = this.#sortedEngines;
          firstVisible = sortedEngines.find(e => e.isGeneralPurposeEngine);
          if (!firstVisible) {
            firstVisible = sortedEngines[0];
          }
        }
        if (firstVisible) {
          firstVisible.hidden = false;
          newDefault = firstVisible;
        }
      }
    }
    // We tried out best but something went very wrong.
    if (!newDefault) {
      lazy.logConsole.error("Could not find a replacement default engine.");
      return null;
    }

    // If the current engine wasn't set or was hidden, we used a fallback
    // to pick a new current engine. As soon as we return it, this new
    // current engine will become user-visible, so we should persist it.
    // by calling the setter.
    this.#setEngineDefault(privateMode, newDefault);

    return privateMode ? this.#currentPrivateEngine : this.#currentEngine;
  }

  /**
   * Helper function to set the current default engine.
   *
   * @param {boolean} privateMode
   *   If true, sets the default engine for private browsing mode, otherwise
   *   sets the default engine for the normal mode. Note, this function does not
   *   check the "separatePrivateDefault" preference - that is up to the caller.
   * @param {nsISearchEngine} newEngine
   *   The search engine to select
   * @param {SearchUtils.REASON_CHANGE_MAP} changeSource
   *   The source of the change of engine.
   */
  #setEngineDefault(privateMode, newEngine, changeSource) {
    // Sometimes we get wrapped nsISearchEngine objects (external XPCOM callers),
    // and sometimes we get raw Engine JS objects (callers in this file), so
    // handle both.
    if (
      !(newEngine instanceof Ci.nsISearchEngine) &&
      !(newEngine instanceof lazy.SearchEngine)
    ) {
      throw Components.Exception(
        "Invalid argument passed to defaultEngine setter",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    const newCurrentEngine = this._engines.get(newEngine.id);
    if (!newCurrentEngine) {
      throw Components.Exception(
        "Can't find engine in store!",
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    if (!newCurrentEngine.isAppProvided) {
      // If a non default engine is being set as the current engine, ensure
      // its loadPath has a verification hash.
      if (!newCurrentEngine._loadPath) {
        newCurrentEngine._loadPath = "[other]unknown";
      }
      let loadPathHash = lazy.SearchUtils.getVerificationHash(
        newCurrentEngine._loadPath
      );
      let currentHash = newCurrentEngine.getAttr("loadPathHash");
      if (!currentHash || currentHash != loadPathHash) {
        newCurrentEngine.setAttr("loadPathHash", loadPathHash);
        lazy.SearchUtils.notifyAction(
          newCurrentEngine,
          lazy.SearchUtils.MODIFIED_TYPE.CHANGED
        );
      }
    }

    let currentEngine = privateMode
      ? this.#currentPrivateEngine
      : this.#currentEngine;

    if (newCurrentEngine == currentEngine) {
      return;
    }

    // Ensure that we reset an engine override if it was previously overridden.
    currentEngine?.removeExtensionOverride();

    if (privateMode) {
      this.#currentPrivateEngine = newCurrentEngine;
    } else {
      this.#currentEngine = newCurrentEngine;
    }

    // If we change the default engine in the future, that change should impact
    // users who have switched away from and then back to the build's
    // "app default" engine. So clear the user pref when the currentEngine is
    // set to the build's app default engine, so that the currentEngine getter
    // falls back to whatever the default is.
    // However, we do not do this whilst we are running an experiment - an
    // experiment must preseve the user's choice of default engine during it's
    // runtime and when it ends. Once the experiment ends, we will reset the
    // attribute elsewhere.
    let newId = newCurrentEngine.id;
    const appDefaultEngine = privateMode
      ? this.appPrivateDefaultEngine
      : this.appDefaultEngine;
    if (
      newCurrentEngine == appDefaultEngine &&
      !lazy.NimbusFeatures.searchConfiguration.getVariable("experiment")
    ) {
      newId = "";
    }

    this._settings.setVerifiedMetaDataAttribute(
      privateMode ? "privateDefaultEngineId" : "defaultEngineId",
      newId
    );

    // Only do this if we're initialized though - this function can get called
    // during initalization.
    if (this.isInitialized) {
      this.#recordDefaultChangedEvent(
        privateMode,
        currentEngine,
        newCurrentEngine,
        changeSource
      );
      this.#recordTelemetryData();
    }

    lazy.SearchUtils.notifyAction(
      newCurrentEngine,
      lazy.SearchUtils.MODIFIED_TYPE[
        privateMode ? "DEFAULT_PRIVATE" : "DEFAULT"
      ]
    );
    // If we've not got a separate private active, notify update of the
    // private so that the UI updates correctly.
    if (!privateMode && !this.#separatePrivateDefault) {
      lazy.SearchUtils.notifyAction(
        newCurrentEngine,
        lazy.SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
      );
    }
  }

  #onSeparateDefaultPrefChanged(prefName, previousValue, currentValue) {
    // Clear out the sorted engines settings, so that we re-sort it if necessary.
    this._cachedSortedEngines = null;
    // We should notify if the normal default, and the currently saved private
    // default are different. Otherwise, save the energy.
    if (this.defaultEngine != this._getEngineDefault(true)) {
      lazy.SearchUtils.notifyAction(
        // Always notify with the new private engine, the function checks
        // the preference value for us.
        this.defaultPrivateEngine,
        lazy.SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
      );
    }
    // Always notify about the change of status of private default if the user
    // toggled the UI.
    if (
      prefName ==
      lazy.SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault"
    ) {
      if (!previousValue && currentValue) {
        this.#recordDefaultChangedEvent(
          true,
          null,
          this._getEngineDefault(true),
          Ci.nsISearchService.CHANGE_REASON_USER_PRIVATE_SPLIT
        );
      } else {
        this.#recordDefaultChangedEvent(
          true,
          this._getEngineDefault(true),
          null,
          Ci.nsISearchService.CHANGE_REASON_USER_PRIVATE_SPLIT
        );
      }
    }
    // Update the telemetry data.
    this.#recordTelemetryData();
  }

  #getEngineInfo(engine) {
    if (!engine) {
      // The defaultEngine getter will throw if there's no engine at all,
      // which shouldn't happen unless an add-on or a test deleted all of them.
      // Our preferences UI doesn't let users do that.
      console.error("getDefaultEngineInfo: No default engine");
      return ["NONE", { name: "NONE" }];
    }

    const engineData = {
      loadPath: engine._loadPath,
      name: engine.name ? engine.name : "",
    };

    if (engine.isAppProvided) {
      engineData.origin = "default";
    } else {
      let currentHash = engine.getAttr("loadPathHash");
      if (!currentHash) {
        engineData.origin = "unverified";
      } else {
        let loadPathHash = lazy.SearchUtils.getVerificationHash(
          engine._loadPath
        );
        engineData.origin =
          currentHash == loadPathHash ? "verified" : "invalid";
      }
    }

    // For privacy, we only collect the submission URL for default engines...
    let sendSubmissionURL = engine.isAppProvided;

    if (!sendSubmissionURL) {
      // ... or engines that are the same domain as a default engine.
      let engineHost = engine.searchUrlDomain;
      for (let innerEngine of this._engines.values()) {
        if (!innerEngine.isAppProvided) {
          continue;
        }

        if (innerEngine.searchUrlDomain == engineHost) {
          sendSubmissionURL = true;
          break;
        }
      }

      if (!sendSubmissionURL) {
        // ... or well known search domains.
        //
        // Starts with: www.google., search.aol., yandex.
        // or
        // Ends with: search.yahoo.com, .ask.com, .bing.com, .startpage.com, baidu.com, duckduckgo.com
        const urlTest =
          /^(?:www\.google\.|search\.aol\.|yandex\.)|(?:search\.yahoo|\.ask|\.bing|\.startpage|\.baidu|duckduckgo)\.com$/;
        sendSubmissionURL = urlTest.test(engineHost);
      }
    }

    if (sendSubmissionURL) {
      let uri = engine.searchURLWithNoTerms;
      uri = uri
        .mutate()
        .setUserPass("") // Avoid reporting a username or password.
        .finalize();
      engineData.submissionURL = uri.spec;
    }

    return [engine.telemetryId, engineData];
  }

  /**
   * Records an event for where the default engine is changed. This is
   * recorded to both Glean and Telemetry.
   *
   * The Glean GIFFT functionality is not used here because we use longer
   * names in the extra arguments to the event.
   *
   * @param {boolean} isPrivate
   *   True if this is a event about a private engine.
   * @param {SearchEngine} [previousEngine]
   *   The previously default search engine.
   * @param {SearchEngine} [newEngine]
   *   The new default search engine.
   * @param {string} changeSource
   *   The source of the change of default.
   */
  #recordDefaultChangedEvent(
    isPrivate,
    previousEngine,
    newEngine,
    changeSource = Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  ) {
    changeSource = REASON_CHANGE_MAP.get(changeSource) ?? "unknown";
    Services.telemetry.setEventRecordingEnabled("search", true);
    let telemetryId;
    let engineInfo;
    // If we are toggling the separate private browsing settings, we might not
    // have an engine to record.
    if (newEngine) {
      [telemetryId, engineInfo] = this.#getEngineInfo(newEngine);
    } else {
      telemetryId = "";
      engineInfo = {
        name: "",
        loadPath: "",
        submissionURL: "",
      };
    }

    let submissionURL = engineInfo.submissionURL ?? "";
    Services.telemetry.recordEvent(
      "search",
      "engine",
      isPrivate ? "change_private" : "change_default",
      changeSource,
      {
        // In docshell tests, the previous engine does not exist, so we allow
        // for the previousEngine to be undefined.
        prev_id: previousEngine?.telemetryId ?? "",
        new_id: telemetryId,
        new_name: engineInfo.name,
        new_load_path: engineInfo.loadPath,
        // Telemetry has a limit of 80 characters.
        new_sub_url: submissionURL.slice(0, 80),
      }
    );

    let extraArgs = {
      // In docshell tests, the previous engine does not exist, so we allow
      // for the previousEngine to be undefined.
      previous_engine_id: previousEngine?.telemetryId ?? "",
      new_engine_id: telemetryId,
      new_display_name: engineInfo.name,
      new_load_path: engineInfo.loadPath,
      // Glean has a limit of 100 characters.
      new_submission_url: submissionURL.slice(0, 100),
      change_source: changeSource,
    };
    if (isPrivate) {
      Glean.searchEnginePrivate.changed.record(extraArgs);
    } else {
      Glean.searchEngineDefault.changed.record(extraArgs);
    }
  }

  /**
   * Records the user's current default engine (normal and private) data to
   * telemetry.
   */
  #recordTelemetryData() {
    let info = this.getDefaultEngineInfo();

    Glean.searchEngineDefault.engineId.set(info.defaultSearchEngine);
    Glean.searchEngineDefault.displayName.set(
      info.defaultSearchEngineData.name
    );
    Glean.searchEngineDefault.loadPath.set(
      info.defaultSearchEngineData.loadPath
    );
    Glean.searchEngineDefault.submissionUrl.set(
      info.defaultSearchEngineData.submissionURL ?? "blank:"
    );
    Glean.searchEngineDefault.verified.set(info.defaultSearchEngineData.origin);

    Glean.searchEnginePrivate.engineId.set(
      info.defaultPrivateSearchEngine ?? ""
    );

    if (info.defaultPrivateSearchEngineData) {
      Glean.searchEnginePrivate.displayName.set(
        info.defaultPrivateSearchEngineData.name
      );
      Glean.searchEnginePrivate.loadPath.set(
        info.defaultPrivateSearchEngineData.loadPath
      );
      Glean.searchEnginePrivate.submissionUrl.set(
        info.defaultPrivateSearchEngineData.submissionURL ?? "blank:"
      );
      Glean.searchEnginePrivate.verified.set(
        info.defaultPrivateSearchEngineData.origin
      );
    } else {
      Glean.searchEnginePrivate.displayName.set("");
      Glean.searchEnginePrivate.loadPath.set("");
      Glean.searchEnginePrivate.submissionUrl.set("blank:");
      Glean.searchEnginePrivate.verified.set("");
    }
  }

  /**
   * This function is called at the beginning of search service init.
   * If the error type set in a test environment matches errorType
   * passed to this function, we throw an error.
   *
   * @param {string} errorType
   *   The error that can occur during search service init.
   *
   */
  #maybeThrowErrorInTest(errorType) {
    if (
      Services.env.exists("XPCSHELL_TEST_PROFILE_DIR") &&
      this.errorToThrowInTest === errorType
    ) {
      throw new Error(
        `Fake ${errorType} error during search service initialization.`
      );
    }
  }

  #buildParseSubmissionMap() {
    this.#parseSubmissionMap = new Map();

    // Used only while building the map, indicates which entries do not refer to
    // the main domain of the engine but to an alternate domain, for example
    // "www.google.fr" for the "www.google.com" search engine.
    let keysOfAlternates = new Set();

    for (let engine of this.#sortedEngines) {
      if (engine.hidden) {
        continue;
      }

      let urlParsingInfo = engine.getURLParsingInfo();
      if (!urlParsingInfo) {
        continue;
      }

      // Store the same object on each matching map key, as an optimization.
      let mapValueForEngine = {
        engine,
        termsParameterName: urlParsingInfo.termsParameterName,
      };

      let processDomain = (domain, isAlternate) => {
        let key = domain + urlParsingInfo.path;

        // Apply the logic for which main domains take priority over alternate
        // domains, even if they are found later in the ordered engine list.
        let existingEntry = this.#parseSubmissionMap.get(key);
        if (!existingEntry) {
          if (isAlternate) {
            keysOfAlternates.add(key);
          }
        } else if (!isAlternate && keysOfAlternates.has(key)) {
          keysOfAlternates.delete(key);
        } else {
          return;
        }

        this.#parseSubmissionMap.set(key, mapValueForEngine);
      };

      processDomain(urlParsingInfo.mainDomain, false);
      lazy.SearchStaticData.getAlternateDomains(
        urlParsingInfo.mainDomain
      ).forEach(d => processDomain(d, true));
    }
  }

  #nimbusSearchUpdatedFun = null;

  async #nimbusSearchUpdated() {
    this.#checkNimbusPrefs();
    Services.search.wrappedJSObject._maybeReloadEngines(
      Ci.nsISearchService.CHANGE_REASON_EXPERIMENT
    );
  }

  /**
   * Check the prefs are correctly updated for users enrolled in a Nimbus experiment.
   *
   * @param {boolean} isStartup
   *   Whether this function was called as part of the startup flow.
   */
  #checkNimbusPrefs(isStartup = false) {
    // If we are in an experiment we may need to check the status on startup, otherwise
    // ignore the call to check on startup so we do not reset users prefs when they are
    // not an experiment.
    if (
      isStartup &&
      !lazy.NimbusFeatures.searchConfiguration.getVariable("experiment")
    ) {
      return;
    }
    let nimbusPrivateDefaultUIEnabled =
      lazy.NimbusFeatures.searchConfiguration.getVariable(
        "seperatePrivateDefaultUIEnabled"
      );
    let nimbusPrivateDefaultUrlbarResultEnabled =
      lazy.NimbusFeatures.searchConfiguration.getVariable(
        "seperatePrivateDefaultUrlbarResultEnabled"
      );

    let previousPrivateDefault = this.defaultPrivateEngine;
    let uiWasEnabled = this._separatePrivateDefaultEnabledPrefValue;
    if (
      this._separatePrivateDefaultEnabledPrefValue !=
      nimbusPrivateDefaultUIEnabled
    ) {
      Services.prefs.setBoolPref(
        `${lazy.SearchUtils.BROWSER_SEARCH_PREF}separatePrivateDefault.ui.enabled`,
        nimbusPrivateDefaultUIEnabled
      );
      let newPrivateDefault = this.defaultPrivateEngine;
      if (previousPrivateDefault != newPrivateDefault) {
        if (!uiWasEnabled) {
          this.#recordDefaultChangedEvent(
            true,
            null,
            newPrivateDefault,
            Ci.nsISearchService.CHANGE_REASON_EXPERIMENT
          );
        } else {
          this.#recordDefaultChangedEvent(
            true,
            previousPrivateDefault,
            null,
            Ci.nsISearchService.CHANGE_REASON_EXPERIMENT
          );
        }
      }
    }
    if (
      this.separatePrivateDefaultUrlbarResultEnabled !=
      nimbusPrivateDefaultUrlbarResultEnabled
    ) {
      Services.prefs.setBoolPref(
        `${lazy.SearchUtils.BROWSER_SEARCH_PREF}separatePrivateDefault.urlbarResult.enabled`,
        nimbusPrivateDefaultUrlbarResultEnabled
      );
    }
  }

  #addObservers() {
    if (this.#observersAdded) {
      // There might be a race between synchronous and asynchronous
      // initialization for which we try to register the observers twice.
      return;
    }
    this.#observersAdded = true;

    this.#nimbusSearchUpdatedFun = this.#nimbusSearchUpdated.bind(this);
    lazy.NimbusFeatures.searchConfiguration.onUpdate(
      this.#nimbusSearchUpdatedFun
    );

    Services.obs.addObserver(this, lazy.SearchUtils.TOPIC_ENGINE_MODIFIED);
    Services.obs.addObserver(this, QUIT_APPLICATION_TOPIC);
    Services.obs.addObserver(this, TOPIC_LOCALES_CHANGE);

    this._settings.addObservers();

    // The current stage of shutdown. Used to help analyze crash
    // signatures in case of shutdown timeout.
    let shutdownState = {
      step: "Not started",
      latestError: {
        message: undefined,
        stack: undefined,
      },
    };
    IOUtils.profileBeforeChange.addBlocker(
      "Search service: shutting down",
      () =>
        (async () => {
          // If we are in initialization, then don't attempt to save the settings.
          // It is likely that shutdown will have caused the add-on manager to
          // stop, which can cause initialization to fail.
          // Hence at that stage, we could have broken settings which we don't
          // want to write.
          // The good news is, that if we don't write the settings here, we'll
          // detect the out-of-date settings on next state, and automatically
          // rebuild it.
          if (!this.isInitialized) {
            lazy.logConsole.warn(
              "not saving settings on shutdown due to initializing."
            );
            return;
          }

          try {
            await this._settings.shutdown(shutdownState);
          } catch (ex) {
            // Ensure that error is reported and that it causes tests
            // to fail, otherwise ignore it.
            Promise.reject(ex);
          }
        })(),

      () => shutdownState
    );
  }

  // This is prefixed with _ rather than # because it is
  // called in a test.
  _removeObservers() {
    if (this.ignoreListListener) {
      lazy.IgnoreLists.unsubscribe(this.ignoreListListener);
      delete this.ignoreListListener;
    }
    if (this.#queuedIdle) {
      this.idleService.removeIdleObserver(this, RECONFIG_IDLE_TIME_SEC);
      this.#queuedIdle = false;
    }

    this._settings.removeObservers();

    lazy.NimbusFeatures.searchConfiguration.offUpdate(
      this.#nimbusSearchUpdatedFun
    );

    Services.obs.removeObserver(this, lazy.SearchUtils.TOPIC_ENGINE_MODIFIED);
    Services.obs.removeObserver(this, QUIT_APPLICATION_TOPIC);
    Services.obs.removeObserver(this, TOPIC_LOCALES_CHANGE);
    Services.obs.removeObserver(this, lazy.Region.REGION_TOPIC);
  }

  QueryInterface = ChromeUtils.generateQI([
    "nsISearchService",
    "nsIObserver",
    "nsITimerCallback",
  ]);

  // nsIObserver
  observe(engine, topic, verb) {
    switch (topic) {
      case lazy.SearchUtils.TOPIC_ENGINE_MODIFIED:
        switch (verb) {
          case lazy.SearchUtils.MODIFIED_TYPE.LOADED:
            engine = engine.QueryInterface(Ci.nsISearchEngine);
            lazy.logConsole.debug(
              "observe: Done installation of ",
              engine.name
            );
            this.#addEngineToStore(engine.wrappedJSObject);
            // The addition of the engine to the store always triggers an ADDED
            // or a CHANGED notification, that will trigger the task below.
            break;
          case lazy.SearchUtils.MODIFIED_TYPE.ADDED:
          case lazy.SearchUtils.MODIFIED_TYPE.CHANGED:
          case lazy.SearchUtils.MODIFIED_TYPE.REMOVED:
            // Invalidate the map used to parse URLs to search engines.
            this.#parseSubmissionMap = null;
            break;
        }
        break;

      case "idle": {
        this.idleService.removeIdleObserver(this, RECONFIG_IDLE_TIME_SEC);
        this.#queuedIdle = false;
        lazy.logConsole.debug(
          "Reloading engines after idle due to configuration change"
        );
        this._maybeReloadEngines(
          Ci.nsISearchService.CHANGE_REASON_CONFIG
        ).catch(console.error);
        break;
      }

      case QUIT_APPLICATION_TOPIC:
        this._removeObservers();
        break;

      case TOPIC_LOCALES_CHANGE:
        // Locale changed. Re-init. We rely on observers, because we can't
        // return this promise to anyone.

        // At the time of writing, when the user does a "Apply and Restart" for
        // a new language the preferences code triggers the locales change and
        // restart straight after, so we delay the check, which means we should
        // be able to avoid the reload on shutdown, and we'll sort it out
        // on next startup.
        // This also helps to avoid issues with the add-on manager shutting
        // down at the same time (see _reInit for more info).
        Services.tm.dispatchToMainThread(() => {
          if (!Services.startup.shuttingDown) {
            this._maybeReloadEngines(
              Ci.nsISearchService.CHANGE_REASON_LOCALE
            ).catch(console.error);
          }
        });
        break;
      case lazy.Region.REGION_TOPIC:
        lazy.logConsole.debug("Region updated:", lazy.Region.home);
        this._maybeReloadEngines(
          Ci.nsISearchService.CHANGE_REASON_REGION
        ).catch(console.error);
        break;
    }
  }

  /**
   * Create an engine object from the search configuration details.
   *
   * This method is prefixed with _ rather than # because it is
   * called in a test.
   *
   * @param {object} config
   *   The configuration object that defines the details of the engine
   *   webExtensionId etc.
   * @returns {nsISearchEngine}
   *   Returns the search engine object.
   */
  async _makeEngineFromConfig(config) {
    lazy.logConsole.debug("_makeEngineFromConfig:", config);
    let locale =
      "locale" in config.webExtension
        ? config.webExtension.locale
        : lazy.SearchUtils.DEFAULT_TAG;

    if (!lazy.SearchUtils.newSearchConfigEnabled) {
      let engine = new lazy.AddonSearchEngine({
        isAppProvided: true,
        details: {
          extensionID: config.webExtension.id,
          locale,
        },
      });
      await engine.init({
        locale,
        config,
      });
      return engine;
    }

    return new lazy.AppProvidedSearchEngine({
      details: {
        locale,
        config,
      },
    });
  }

  /**
   * @param {object} metaData
   *    The metadata object that defines the details of the engine.
   * @returns {boolean}
   *    Returns true if metaData has different property values than
   *    the cached _metaData.
   */
  #didSettingsMetaDataUpdate(metaData) {
    let metaDataProperties = [
      "locale",
      "region",
      "channel",
      "experiment",
      "distroID",
    ];

    return metaDataProperties.some(p => {
      return metaData?.[p] !== this._settings.getMetaDataAttribute(p);
    });
  }

  /**
   * Shows an infobar to notify the user their default search engine has been
   * removed and replaced by a new default search engine.
   *
   * This method is prefixed with _ rather than # because it is
   * called in a test.
   *
   * @param {string} prevCurrentEngineName
   *   The name of the previous default engine that will be replaced.
   * @param {string} newCurrentEngineName
   *   The name of the engine that will be the new default engine.
   *
   */
  _showRemovalOfSearchEngineNotificationBox(
    prevCurrentEngineName,
    newCurrentEngineName
  ) {
    let win = Services.wm.getMostRecentBrowserWindow();
    win.BrowserSearch.removalOfSearchEngineNotificationBox(
      prevCurrentEngineName,
      newCurrentEngineName
    );
  }

  /**
   * Maybe starts the timer for OpenSearch engine updates. This will be set
   * only if updates are enabled and there are OpenSearch engines installed
   * which have updates.
   */
  #maybeStartOpenSearchUpdateTimer() {
    if (
      this.#openSearchUpdateTimerStarted ||
      !Services.prefs.getBoolPref(
        lazy.SearchUtils.BROWSER_SEARCH_PREF + "update",
        true
      )
    ) {
      return;
    }

    let engineWithUpdates = [...this._engines.values()].find(
      engine => engine instanceof lazy.OpenSearchEngine && engine._hasUpdates
    );

    if (engineWithUpdates) {
      lazy.logConsole.debug("Engine with updates found, setting update timer");
      lazy.timerManager.registerTimer(
        OPENSEARCH_UPDATE_TIMER_TOPIC,
        this,
        OPENSEARCH_UPDATE_TIMER_INTERVAL,
        true
      );
      this.#openSearchUpdateTimerStarted = true;
    }
  }
} // end SearchService class

var engineUpdateService = {
  scheduleNextUpdate(engine) {
    var interval = engine._updateInterval || OPENSEARCH_DEFAULT_UPDATE_INTERVAL;
    var milliseconds = interval * 86400000; // |interval| is in days
    engine.setAttr("updateexpir", Date.now() + milliseconds);
  },

  update(engine) {
    engine = engine.wrappedJSObject;
    lazy.logConsole.debug("update called for", engine._name);
    if (
      !Services.prefs.getBoolPref(
        lazy.SearchUtils.BROWSER_SEARCH_PREF + "update",
        true
      ) ||
      !engine._hasUpdates
    ) {
      return;
    }

    let testEngine = null;
    let updateURI = engine._updateURI;
    if (updateURI) {
      lazy.logConsole.debug("updating", engine.name, updateURI.spec);
      testEngine = new lazy.OpenSearchEngine();
      testEngine._engineToUpdate = engine;
      try {
        testEngine.install(updateURI);
      } catch (ex) {
        lazy.logConsole.error("Failed to update", engine.name, ex);
      }
    } else {
      lazy.logConsole.debug("invalid updateURI");
    }

    if (engine._iconUpdateURL) {
      // If we're updating the engine too, use the new engine object,
      // otherwise use the existing engine object.
      (testEngine || engine)._setIcon(engine._iconUpdateURL, true);
    }
  },
};

XPCOMUtils.defineLazyServiceGetter(
  SearchService.prototype,
  "idleService",
  "@mozilla.org/widget/useridleservice;1",
  "nsIUserIdleService"
);

/**
 * Handles getting and checking extensions against the allow list.
 */
class SearchDefaultOverrideAllowlistHandler {
  /**
   * @param {Function} listener
   *   A listener for configuration update changes.
   */
  constructor(listener) {
    this._remoteConfig = lazy.RemoteSettings(
      lazy.SearchUtils.SETTINGS_ALLOWLIST_KEY
    );
  }

  /**
   * Determines if a search engine extension can override a default one
   * according to the allow list.
   *
   * @param {object} extension
   *   The extension object (from add-on manager) that will override the
   *   app provided search engine.
   * @param {string} appProvidedExtensionId
   *   The id of the search engine that will be overriden.
   * @returns {boolean}
   *   Returns true if the search engine extension may override the app provided
   *   instance.
   */
  async canOverride(extension, appProvidedExtensionId) {
    const overrideTable = await this._getAllowlist();

    let entry = overrideTable.find(e => e.thirdPartyId == extension.id);
    if (!entry) {
      return false;
    }

    if (appProvidedExtensionId != entry.overridesId) {
      return false;
    }

    let searchProvider =
      extension.manifest.chrome_settings_overrides.search_provider;

    return entry.urls.some(
      e =>
        searchProvider.search_url == e.search_url &&
        searchProvider.search_form == e.search_form &&
        searchProvider.search_url_get_params == e.search_url_get_params &&
        searchProvider.search_url_post_params == e.search_url_post_params
    );
  }

  /**
   * Obtains the configuration from remote settings. This includes
   * verifying the signature of the record within the database.
   *
   * If the signature in the database is invalid, the database will be wiped
   * and the stored dump will be used, until the settings next update.
   *
   * Note that this may cause a network check of the certificate, but that
   * should generally be quick.
   *
   * @returns {Array}
   *   An array of objects in the database, or an empty array if none
   *   could be obtained.
   */
  async _getAllowlist() {
    let result = [];
    try {
      result = await this._remoteConfig.get();
    } catch (ex) {
      // Don't throw an error just log it, just continue with no data, and hopefully
      // a sync will fix things later on.
      console.error(ex);
    }
    lazy.logConsole.debug("Allow list is:", result);
    return result;
  }
}
