/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonSearchEngine: "resource://gre/modules/AddonSearchEngine.jsm",
  IgnoreLists: "resource://gre/modules/IgnoreLists.jsm",
  OpenSearchEngine: "resource://gre/modules/OpenSearchEngine.jsm",
  PolicySearchEngine: "resource://gre/modules/PolicySearchEngine.jsm",
  Region: "resource://gre/modules/Region.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  SearchEngine: "resource://gre/modules/SearchEngine.jsm",
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.jsm",
  SearchSettings: "resource://gre/modules/SearchSettings.jsm",
  SearchStaticData: "resource://gre/modules/SearchStaticData.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  UserSearchEngine: "resource://gre/modules/UserSearchEngine.jsm",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "SearchService",
    maxLogLevel: lazy.SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

const TOPIC_LOCALES_CHANGE = "intl:app-locales-changed";
const QUIT_APPLICATION_TOPIC = "quit-application";

// The default engine update interval, in days. This is only used if an engine
// specifies an updateURL, but not an updateInterval.
const SEARCH_DEFAULT_UPDATE_INTERVAL = 7;

// This is the amount of time we'll be idle for before applying any configuration
// changes.
const RECONFIG_IDLE_TIME_SEC = 5 * 60;

/**
 * The ParseSubmissionResult contains getter methods that return attributes
 * about the parsed submission url.
 *
 * @implements {nsIParseSubmissionResult}
 */
class ParseSubmissionResult {
  constructor(engine, terms, termsParameterName, termsOffset, termsLength) {
    this.#engine = engine;
    this.#terms = terms;
    this.#termsParameterName = termsParameterName;
    this.#termsOffset = termsOffset;
    this.#termsLength = termsLength;
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

  get termsOffset() {
    return this.#termsOffset;
  }

  get termsLength() {
    return this.#termsLength;
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

  /**
   * The offset of the string #terms in the URL passed in to
   * nsISearchEngine::parseSubmissionURL, or -1 if the URL does not represent
   * a search submission.
   *
   * @type {number}
   */
  #termsOffset;

  /**
   * The length of the #terms in the original encoding of the URL passed in to
   * nsISearchEngine::parseSubmissionURL. If the search term in the original
   * URL is encoded then this will be bigger than #terms.length
   *
   * @type {number}
   */
  #termsLength;

  QueryInterface = ChromeUtils.generateQI(["nsISearchParseSubmissionResult"]);
}

const gEmptyParseSubmissionResult = Object.freeze(
  new ParseSubmissionResult(null, "", "", -1, 0)
);

/**
 * The search service handles loading and maintaining of search engines. It will
 * also work out the default lists for each locale/region.
 *
 * @implements {nsISearchService}
 */
class SearchService {
  constructor() {
    this.#initObservers = PromiseUtils.defer();
    // this._engines is prefixed with _ rather than # because it is called from
    // a test.
    this._engines = new Map();
    this._settings = new lazy.SearchSettings(this);
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

  async setDefault(engine) {
    await this.init();
    return (this.defaultEngine = engine);
  }

  async getDefaultPrivate() {
    await this.init();
    return this.defaultPrivateEngine;
  }

  async setDefaultPrivate(engine) {
    await this.init();
    return (this.defaultPrivateEngine = engine);
  }

  /**
   * @returns {SearchEngine}
   *   The engine that is the default for this locale/region, ignoring any
   *   user changes to the default engine.
   */
  get originalDefaultEngine() {
    return this.#originalDefaultEngine();
  }

  /**
   * @returns {SearchEngine}
   *   The engine that is the default for this locale/region in private browsing
   *   mode, ignoring any user changes to the default engine.
   *   Note: if there is no default for this locale/region, then the non-private
   *   browsing engine will be returned.
   */
  get originalPrivateDefaultEngine() {
    return this.#originalDefaultEngine(this.#separatePrivateDefault);
  }

  get isInitialized() {
    return this._initialized;
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
      let [
        privateTelemetryId,
        defaultPrivateSearchEngineData,
      ] = this.#getEngineInfo(this.defaultPrivateEngine);
      result.defaultPrivateSearchEngine = privateTelemetryId;
      result.defaultPrivateSearchEngineData = defaultPrivateSearchEngineData;
    }

    return result;
  }

  /**
   * Returns the engine associated with the name.
   *
   * @param {string} engineName
   *   The name of the engine.
   * @returns {SearchEngine}
   *   The associated engine if found, null otherwise.
   */
  getEngineByName(engineName) {
    this.#ensureInitialized();
    return this._engines.get(engineName) || null;
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

  // nsISearchService
  async init() {
    lazy.logConsole.debug("init");
    if (this.#initStarted) {
      return this.#initObservers.promise;
    }

    TelemetryStopwatch.start("SEARCH_SERVICE_INIT_MS");
    this.#initStarted = true;
    try {
      // Complete initialization by calling asynchronous initializer.
      await this.#init();
      TelemetryStopwatch.finish("SEARCH_SERVICE_INIT_MS");
    } catch (ex) {
      TelemetryStopwatch.cancel("SEARCH_SERVICE_INIT_MS");
      this.#initObservers.reject(ex.result);
      throw ex;
    }

    if (!Components.isSuccessCode(this.#initRV)) {
      throw Components.Exception(
        "SearchService initialization failed",
        this.#initRV
      );
    } else if (this.#startupRemovedExtensions.size) {
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
    return this.#initRV;
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
  }

  /**
   * Test only - reset SearchService data. Ideally this should be replaced
   */
  reset() {
    this._initialized = false;
    this.#initObservers = PromiseUtils.defer();
    this.#initStarted = false;
    this.#startupExtensions = new Set();
    this._engines.clear();
    this.__sortedEngines = null;
    this.#currentEngine = null;
    this.#currentPrivateEngine = null;
    this._searchDefault = null;
    this.#searchPrivateDefault = null;
    this.#maybeReloadDebounce = false;
    this._settings._batchTask?.disarm();
  }

  resetToOriginalDefaultEngine() {
    let originalDefaultEngine = this.originalDefaultEngine;
    originalDefaultEngine.hidden = false;
    this.defaultEngine = originalDefaultEngine;
  }

  async maybeSetAndOverrideDefault(extension) {
    let searchProvider =
      extension.manifest.chrome_settings_overrides.search_provider;
    let engine = this._engines.get(searchProvider.name);
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
      this.#defaultOverrideAllowlist = new SearchDefaultOverrideAllowlistHandler();
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
  async addPolicyEngine(details) {
    await this.init();

    let newEngine = new lazy.PolicySearchEngine({ details });
    let existingEngine = this._engines.get(newEngine.name);
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
   * Updates a search engine that is specified from enterprise policies.
   *
   * @param {object} details
   *   An object that simulates the manifest object from a WebExtension. See
   *   the idl for more details.
   */
  async updatePolicyEngine(details) {
    let engine = this.getEngineByName(
      details.chrome_settings_overrides.search_provider.name
    );
    if (engine && !engine.isAppProvided) {
      engine.update(details);
    }
  }

  /**
   * Adds a search engine that is specified by the user.
   *
   * @param {string} name
   * @param {string} url
   * @param {string} alias
   */
  async addUserEngine(name, url, alias) {
    await this.init();

    let newEngine = new lazy.UserSearchEngine({
      details: { name, url, alias },
    });
    let existingEngine = this._engines.get(newEngine.name);
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
      if (this._initialized && !this._reloadingEngines) {
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
    if (!this._initialized) {
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
        engine._install(engineURL, errorCode => {
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

    if (engineToRemove == this.defaultEngine) {
      this.#findAndSetNewDefaultEngine({
        privateMode: false,
        excludeEngineName: engineToRemove.name,
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
        excludeEngineName: engineToRemove.name,
      });
    }

    if (engineToRemove._isAppProvided) {
      // Just hide it (the "hidden" setter will notify) and remove its alias to
      // avoid future conflicts with other engines.
      engineToRemove.hidden = true;
      engineToRemove.alias = null;
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
      throw Components.Exception("moveEngine: Index out of bounds!");
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
    var movedEngine = this.__sortedEngines.splice(currentIndex, 1)[0];
    this.__sortedEngines.splice(newIndex, 0, movedEngine);

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
    if (!this._initialized) {
      // If search is not initialized, do nothing.
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
      let soughtUrl = Services.io.newURI(url).QueryInterface(Ci.nsIURL);

      // Exclude any URL that is not HTTP or HTTPS from the beginning.
      if (soughtUrl.scheme != "http" && soughtUrl.scheme != "https") {
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

    let length = 0;
    let offset = url.indexOf("?") + 1;
    let query = url.slice(offset);
    // Iterate a second time over the original input string to determine the
    // correct search term offset and length in the original encoding.
    for (let param of query.split("&")) {
      let equalPos = param.indexOf("=");
      if (
        equalPos != -1 &&
        param.substr(0, equalPos) == mapEntry.termsParameterName
      ) {
        // This is the parameter we are looking for.
        offset += equalPos + 1;
        length = param.length - equalPos - 1;
        break;
      }
      offset += param.length + 1;
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

    let submission = new ParseSubmissionResult(
      mapEntry.engine,
      terms,
      mapEntry.termsParameterName,
      offset,
      length
    );
    return submission;
  }

  // nsITimerCallbactk
  notify(timer) {
    lazy.logConsole.debug("notify: checking for updates");

    if (
      !Services.prefs.getBoolPref(
        lazy.SearchUtils.BROWSER_SEARCH_PREF + "update",
        true
      )
    ) {
      return;
    }

    // Our timer has expired, but unfortunately, we can't get any data from it.
    // Therefore, we need to walk our engine-list, looking for expired engines
    var currentTime = Date.now();
    lazy.logConsole.debug("currentTime:" + currentTime);
    for (let e of this._engines.values()) {
      let engine = e.wrappedJSObject;
      if (!engine._hasUpdates) {
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

  #initObservers;
  #currentEngine;
  #currentPrivateEngine;
  #queuedIdle;

  /**
   * The current status of initialization. Note that it does not determine if
   * initialization is complete, only if an error has been encountered so far.
   *
   * @type {number}
   */
  #initRV = Cr.NS_OK;

  /**
   * Indicates that the initialization has started or not.
   *
   * @type {boolean}
   */
  #initStarted = false;

  /**
   * Indicates if initialization has been completed (successful or not).
   *
   * This is prefixed with _ rather than # because it is
   * called in browser/base/content/test/contextMenu/browser_contextmenu.js
   *
   * @type {boolean}
   */
  _initialized = false;

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
   * @type {array}
   */
  #submissionURLIgnoreList = [];

  /**
   * Various search engines may be ignored if their load path is contained
   * in this list. The list is controlled via remote settings.
   *
   * @type {array}
   */
  #loadPathIgnoreList = [];

  /**
   * A map of engine display names to `SearchEngine`.
   *
   * @type {Map<string, object>|null}
   */
  _engines = null;

  /**
   * An array of engine short names sorted into display order. The double __
   * represents cached sorted engines.
   *
   * @type {array}
   */
  __sortedEngines = null;

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

  get #sortedEngines() {
    if (!this.__sortedEngines) {
      return this.#buildSortedEngineList();
    }
    return this.__sortedEngines;
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
    var engines = this.#sortedEngines.filter(function(engine) {
      return engine._extensionID == extensionID;
    });
    return engines;
  }

  /**
   * Returns the engine associated with the WebExtension details.
   *
   * @param {object} details
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
    const attributeName = privateMode ? "private" : "current";
    let name = this._settings.getAttribute(attributeName);
    let engine = this._engines.get(name) || null;
    if (
      engine &&
      (engine.isAppProvided ||
        this._settings.getVerifiedAttribute(attributeName))
    ) {
      // If the current engine is a default one, we can relax the
      // verification hash check to reduce the annoyance for users who
      // backup/sync their profile in custom ways.
      if (privateMode) {
        this.#currentPrivateEngine = engine;
      } else {
        this.#currentEngine = engine;
      }
    }
    if (!name) {
      if (privateMode) {
        this.#currentPrivateEngine = this.originalPrivateDefaultEngine;
      } else {
        this.#currentEngine = this.originalDefaultEngine;
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
    if (this._initialized) {
      if (!Components.isSuccessCode(this.#initRV)) {
        lazy.logConsole.debug("#ensureInitialized: failure");
        throw Components.Exception(
          "SearchService previously failed to initialize",
          this.#initRV
        );
      }
      return;
    }

    let err = new Error(
      "Something tried to use the search service before it's been " +
        "properly intialized. Please examine the stack trace to figure out what and " +
        "where to fix it:\n"
    );
    err.message += err.stack;
    throw err;
  }

  /**
   * Asynchronous implementation of the initializer.
   *
   * @returns {number}
   *   A Components.results success code on success, otherwise a failure code.
   */
  async #init() {
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

    // We need to catch the region being updated
    // during initialisation so we start listening
    // straight away.
    Services.obs.addObserver(this, lazy.Region.REGION_TOPIC);

    try {
      // Create the search engine selector.
      this.#engineSelector = new lazy.SearchEngineSelector(
        this.#handleConfigurationUpdated.bind(this)
      );

      // See if we have a settings file so we don't have to parse a bunch of XML.
      let settings = await this._settings.get();

      this.#setupRemoteSettings().catch(Cu.reportError);

      await this.#loadEngines(settings);

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
        lazy.logConsole.warn("#init: abandoning init due to shutting down");
        this.#initRV = Cr.NS_ERROR_ABORT;
        this.#initObservers.reject(this.#initRV);
        return this.#initRV;
      }

      // Make sure the current list of engines is persisted, without the need to wait.
      lazy.logConsole.debug("#init: engines loaded, writing settings");
      this.#addObservers();
    } catch (ex) {
      this.#initRV = ex.result !== undefined ? ex.result : Cr.NS_ERROR_FAILURE;
      lazy.logConsole.error("#init: failure initializing search:", ex);
    }

    this._initialized = true;
    if (Components.isSuccessCode(this.#initRV)) {
      this.#initObservers.resolve(this.#initRV);
    } else {
      this.#initObservers.reject(this.#initRV);
    }

    this.#recordTelemetryData();

    Services.obs.notifyObservers(
      null,
      lazy.SearchUtils.TOPIC_SEARCH_SERVICE,
      "init-complete"
    );

    lazy.logConsole.debug("Completed #init");
    return this.#initRV;
  }

  /**
   * Obtains the remote settings for the search service. This should only be
   * called from init(). Any subsequent updates to the remote settings are
   * handled via a sync listener.
   *
   * For desktop, the initial remote settings are obtained from dumps in
   * `services/settings/dumps/main/`.
   *
   * When enabling for Android, be aware the dumps are not shipped there, and
   * hence the `get` may take a while to return.
   */
  async #setupRemoteSettings() {
    // Now we have the values, listen for future updates.
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

    // If we have not finished initializing, then we wait for the initialization
    // to complete.
    if (!this.isInitialized) {
      await this.#initObservers;
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
      this._maybeReloadEngines().catch(Cu.reportError);
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
    let url = engine
      ._getURLOfType("text/html")
      .getSubmission("dummy", engine)
      .uri.spec.toLowerCase();
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
  #originalDefaultEngine(privateMode = false) {
    let defaultEngine = this.#getEngineByWebExtensionDetails(
      privateMode && this.#searchPrivateDefault
        ? this.#searchPrivateDefault
        : this._searchDefault
    );

    if (defaultEngine) {
      return defaultEngine;
    }

    if (privateMode) {
      // If for some reason we can't find the private mode engine, fall back
      // to the non-private one.
      return this.#originalDefaultEngine(false);
    }

    // Something unexpected as happened. In order to recover the original
    // default engine, use the first visible engine which is the best we can do.
    return this.#sortedVisibleEngines[0];
  }

  /**
   * Loads engines asynchronously.
   *
   * @param {object} settings
   *   An object representing the search engine settings.
   */
  async #loadEngines(settings) {
    // Get user's current settings and search engine before we load engines from
    // config. These values will be compared after engines are loaded.
    let prevMetaData = { ...settings?.metaData };
    let prevCurrentEngine = prevMetaData.current;
    let prevAppDefaultEngine = prevMetaData?.appDefaultEngine;

    lazy.logConsole.debug("#loadEngines: start");
    let { engines, privateDefault } = await this._fetchEngineSelectorEngines();
    this.#setDefaultAndOrdersFromSelector(engines, privateDefault);

    // We've done what we can without the add-on manager, now ensure that
    // it has finished starting before we continue.
    await lazy.AddonManager.readyPromise;

    let newEngines = await this.#loadEnginesFromConfig(engines);
    for (let engine of newEngines) {
      this.#addEngineToStore(engine);
    }

    lazy.logConsole.debug(
      "#loadEngines: loading",
      this.#startupExtensions.size,
      "engines reported by AddonManager startup"
    );
    for (let extension of this.#startupExtensions) {
      await this.#installExtensionEngine(
        extension,
        [lazy.SearchUtils.DEFAULT_TAG],
        true
      );
    }
    this.#startupExtensions.clear();

    this.#loadEnginesFromSettings(settings.engines);

    this.#loadEnginesMetadataFromSettings(settings.engines);

    lazy.logConsole.debug("#loadEngines: done");

    let newCurrentEngine = this._getEngineDefault(false)?.name;
    this._settings.setAttribute(
      "appDefaultEngine",
      this.originalDefaultEngine?.name
    );

    if (
      this.#shouldDisplayRemovalOfEngineNotificationBox(
        settings,
        prevMetaData,
        newCurrentEngine,
        prevCurrentEngine,
        prevAppDefaultEngine
      )
    ) {
      this._showRemovalOfSearchEngineNotificationBox(
        prevCurrentEngine || prevAppDefaultEngine,
        newCurrentEngine
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
   * @param { object } newCurrentEngine
   *   The user's new current default engine.
   * @param { object } prevCurrentEngine
   *   The user's previous default engine.
   * @param { object } prevAppDefaultEngine
   *   The user's previous app default engine.
   * @returns { boolean }
   *   Return true if the previous default engine has been removed and
   *   notification box should be displayed.
   */
  #shouldDisplayRemovalOfEngineNotificationBox(
    settings,
    prevMetaData,
    newCurrentEngine,
    prevCurrentEngine,
    prevAppDefaultEngine
  ) {
    if (
      !Services.prefs.getBoolPref("browser.search.removeEngineInfobar.enabled")
    ) {
      return false;
    }

    // If for some reason we were unable to install any engines and hence no
    // default engine, do not display the notification box
    if (!newCurrentEngine) {
      return false;
    }

    // If the user's previous engine is different than the new current engine,
    // or if the user was using the app default engine and the app default
    // engine is different than the new current engine, we check if the user's
    // settings metadata has been upddated.
    if (
      (prevCurrentEngine && prevCurrentEngine !== newCurrentEngine) ||
      (!prevCurrentEngine &&
        prevAppDefaultEngine &&
        prevAppDefaultEngine !== newCurrentEngine)
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
   * @param {array} engineConfigs
   *   An array of engines configurations based on the schema.
   * @returns {array.<nsISearchEngine>}
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
   */
  async _maybeReloadEngines() {
    if (this.#maybeReloadDebounce) {
      lazy.logConsole.debug("We're already waiting to reload engines.");
      return;
    }

    if (!this._initialized || this._reloadingEngines) {
      this.#maybeReloadDebounce = true;
      // Schedule a reload to happen at most 10 seconds after the current run.
      Services.tm.idleDispatchToMainThread(() => {
        if (!this.#maybeReloadDebounce) {
          return;
        }
        this.#maybeReloadDebounce = false;
        this._maybeReloadEngines().catch(Cu.reportError);
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
      await this._reloadEngines(settings);
    } catch (ex) {
      lazy.logConsole.error("maybeReloadEngines failed", ex);
    }
    this._reloadingEngines = false;
    lazy.logConsole.debug("maybeReloadEngines complete");
  }

  // This is prefixed with _ rather than # because it is called in
  // test_remove_engine_notification_box.js
  async _reloadEngines(settings) {
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

    let {
      engines: originalConfigEngines,
      privateDefault,
    } = await this._fetchEngineSelectorEngines();

    let enginesToRemove = [];
    let configEngines = [...originalConfigEngines];
    let oldEngineList = [...this._engines.values()];

    for (let engine of oldEngineList) {
      if (!engine.isAppProvided) {
        continue;
      }

      let index = configEngines.findIndex(
        e =>
          e.webExtension.id == engine._extensionID &&
          e.webExtension.locale == engine._locale
      );

      let policy, manifest, locale;
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
          enginesToRemove.push(engine);
          continue;
        }

        policy = await this.#getExtensionPolicy(engine._extensionID);
        locale =
          replacementEngines[0].webExtension.locale ||
          lazy.SearchUtils.DEFAULT_TAG;
        manifest = await this.#getManifestForLocale(policy.extension, locale);

        // If the name is different, then we must treat the engine as different,
        // and go through the remove and add cycle, rather than modifying the
        // existing one.
        if (
          engine.name !=
          manifest.chrome_settings_overrides.search_provider.name.trim()
        ) {
          // No matching name, so just remove it.
          enginesToRemove.push(engine);
          continue;
        }

        // Update the index so we can handle the updating below.
        index = configEngines.findIndex(
          e =>
            e.webExtension.id == replacementEngines[0].webExtension.id &&
            e.webExtension.locale == replacementEngines[0].webExtension.locale
        );
      } else {
        // This is an existing engine that we should update (we don't know if
        // the configuration for this engine has changed or not).
        policy = await this.#getExtensionPolicy(engine._extensionID);
        locale = engine._locale;
        manifest = await this.#getManifestForLocale(policy.extension, locale);
      }
      engine._updateFromManifest(
        policy.extension.id,
        policy.extension.baseURI,
        manifest,
        locale,
        configEngines[index]
      );

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
    this.#currentEngine = null;
    this.#currentPrivateEngine = null;
    // If the user's default is one of the private engines that is being removed,
    // reset the stored setting, so that we correctly detect the change in
    // in default.
    if (
      prevCurrentEngine &&
      enginesToRemove.some(e => e.name == prevCurrentEngine.name)
    ) {
      this._settings.setAttribute("current", "");
    }
    if (
      prevPrivateEngine &&
      enginesToRemove.some(e => e.name == prevPrivateEngine.name)
    ) {
      this._settings.setAttribute("private", "");
    }

    this.#setDefaultAndOrdersFromSelector(
      originalConfigEngines,
      privateDefault
    );

    // If the defaultEngine has changed between the previous load and this one,
    // dispatch the appropriate notifications.
    if (prevCurrentEngine && this.defaultEngine !== prevCurrentEngine) {
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
      lazy.SearchUtils.notifyAction(
        this.#currentPrivateEngine,
        lazy.SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
      );
    }

    // Finally, remove any engines that need removing.

    for (let engine of enginesToRemove) {
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
    this._settings.setAttribute(
      "appDefaultEngine",
      this.originalDefaultEngine?.name
    );

    this.#dontSetUseSavedOrder = false;
    // Clear out the sorted engines settings, so that we re-sort it if necessary.
    this.__sortedEngines = null;
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
      this._engines.has(engine.name) &&
      !hasSameNameAsUpdate
    ) {
      lazy.logConsole.debug(
        "#addEngineToStore: Duplicate engine found, aborting!"
      );
      return;
    }

    if (engine._engineToUpdate) {
      // We need to replace engineToUpdate with the engine that just loaded.
      var oldEngine = engine._engineToUpdate;

      // Remove the old engine from the hash, since it's keyed by name, and our
      // name might change (the update might have a new name).
      this._engines.delete(oldEngine.name);

      // Hack: we want to replace the old engine with the new one, but since
      // people may be holding refs to the nsISearchEngine objects themselves,
      // we'll just copy over all "private" properties (those without a getter
      // or setter) from one object to the other.
      for (var p in engine) {
        if (!(engine.__lookupGetter__(p) || engine.__lookupSetter__(p))) {
          oldEngine[p] = engine[p];
        }
      }
      engine = oldEngine;
      engine._engineToUpdate = null;

      // Add the engine back
      this._engines.set(engine.name, engine);
      lazy.SearchUtils.notifyAction(
        engine,
        lazy.SearchUtils.MODIFIED_TYPE.CHANGED
      );
    } else {
      // Not an update, just add the new engine.
      this._engines.set(engine.name, engine);
      // Only add the engine to the list of sorted engines if the initial list
      // has already been built (i.e. if this.__sortedEngines is non-null). If
      // it hasn't, we're loading engines from disk and the sorted engine list
      // will be built once we need it.
      if (this.__sortedEngines && !this.#dontSetUseSavedOrder) {
        this.__sortedEngines.push(engine);
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

  #loadEnginesMetadataFromSettings(engines) {
    if (!engines) {
      return;
    }

    for (let engine of engines) {
      let name = engine._name;
      if (this._engines.has(name)) {
        lazy.logConsole.debug(
          "#loadEnginesMetadataFromSettings, transfering metadata for",
          name,
          engine._metaData
        );
        let eng = this._engines.get(name);
        // We used to store the alias in metadata.alias, in 1621892 that was
        // changed to only store the user set alias in metadata.alias, remove
        // it from metadata if it was previously set to the internal value.
        if (eng._alias === engine?._metaData?.alias) {
          delete engine._metaData.alias;
        }
        eng._metaData = engine._metaData || {};
      }
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
      // We renamed isBuiltin to isAppProvided in 1631898,
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
        if (loadPath?.includes("set-via-policy")) {
          engine = new lazy.PolicySearchEngine({ json: engineJSON });
        } else if (loadPath?.includes("set-via-user")) {
          engine = new lazy.UserSearchEngine({ json: engineJSON });
        } else if (engineJSON.extensionID ?? engineJSON._extensionID) {
          engine = new lazy.AddonSearchEngine({
            isAppProvided: false,
            json: engineJSON,
          });
        } else {
          engine = new lazy.SearchEngine({
            isAppProvided: false,
            loadPath: engineJSON._loadPath,
          });
          engine._initWithJSON(engineJSON);
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
        "built-in engines."
      );
    }
  }

  // This is prefixed with _ rather than # because it is
  // called in test_remove_engine_notification_box.js
  async _fetchEngineSelectorEngines() {
    let searchEngineSelectorProperties = {
      locale: Services.locale.appLocaleAsBCP47,
      region: lazy.Region.home || "default",
      channel: AppConstants.MOZ_APP_VERSION_DISPLAY.endsWith("esr")
        ? "esr"
        : AppConstants.MOZ_UPDATE_CHANNEL,
      experiment: lazy.NimbusFeatures.search.getVariable("experiment") ?? "",
      distroID: lazy.SearchUtils.distroID ?? "",
    };

    for (let [key, value] of Object.entries(searchEngineSelectorProperties)) {
      this._settings.setAttribute(key, value);
    }

    let {
      engines,
      privateDefault,
    } = await this.#engineSelector.fetchEngineConfiguration(
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
    this._settings.setAttribute("useSavedOrder", true);

    var engines = this.#sortedEngines;

    for (var i = 0; i < engines.length; ++i) {
      engines[i].setAttr("order", i + 1);
    }
  }

  #buildSortedEngineList() {
    // We must initialise __sortedEngines here to avoid infinite recursion
    // in the case of tests which don't define a default search engine.
    // If there's no default defined, then we revert to the first item in the
    // sorted list, but we can't do that if we don't have a list.
    this.__sortedEngines = [];

    // If the user has specified a custom engine order, read the order
    // information from the metadata instead of the default prefs.
    if (this._settings.getAttribute("useSavedOrder")) {
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
        if (orderNumber && !this.__sortedEngines[orderNumber - 1]) {
          this.__sortedEngines[orderNumber - 1] = engine;
          addedEngines[engine.name] = engine;
        } else {
          // We need to call #saveSortedEngineList so this gets sorted out.
          needToSaveEngineList = true;
        }
      }

      // Filter out any nulls for engines that may have been removed
      var filteredEngines = this.__sortedEngines.filter(function(a) {
        return !!a;
      });
      if (this.__sortedEngines.length != filteredEngines.length) {
        needToSaveEngineList = true;
      }
      this.__sortedEngines = filteredEngines;

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
      return (this.__sortedEngines = this.__sortedEngines.concat(alphaEngines));
    }
    lazy.logConsole.debug("#buildSortedEngineList: using default orders");

    return (this.__sortedEngines = this._sortEnginesByDefaults(
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

    // The original default engine should always be first in the list (except
    // for distros, that we should respect).
    const originalDefault = this.originalDefaultEngine;
    maybeAddEngineToSort(originalDefault);

    // If there's a private default, and it is different to the normal
    // default, then it should be second in the list.
    const originalPrivateDefault = this.originalPrivateDefaultEngine;
    if (originalPrivateDefault && originalPrivateDefault != originalDefault) {
      maybeAddEngineToSort(originalPrivateDefault);
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
   * @returns {Array<SearchEngine>}
   */

  get #sortedVisibleEngines() {
    this.__sortedVisibleEngines = this.#sortedEngines.filter(
      engine => !engine.hidden
    );
    return this.__sortedVisibleEngines;
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
      if (
        engine.isAppProvided ||
        !engine._extensionID ||
        engine._extensionID == "set-via-policy" ||
        engine._extensionID == "set-via-user"
      ) {
        continue;
      }

      let addon = await lazy.AddonManager.getAddonByID(engine._extensionID);

      if (!addon) {
        lazy.logConsole.debug(
          `Add-on ${engine._extensionID} for search engine ${engine.name} is not installed!`
        );
        Services.telemetry.keyedScalarSet(
          "browser.searchinit.engine_invalid_webextension",
          engine._extensionID,
          1
        );
      } else if (!addon.isActive) {
        lazy.logConsole.debug(
          `Add-on ${engine._extensionID} for search engine ${engine.name} is not active!`
        );
        Services.telemetry.keyedScalarSet(
          "browser.searchinit.engine_invalid_webextension",
          engine._extensionID,
          2
        );
      } else {
        let policy = await this.#getExtensionPolicy(engine._extensionID);
        let providerSettings =
          policy.extension.manifest?.chrome_settings_overrides?.search_provider;

        if (!providerSettings) {
          lazy.logConsole.debug(
            `Add-on ${engine._extensionID} for search engine ${engine.name} no longer has an engine defined`
          );
          Services.telemetry.keyedScalarSet(
            "browser.searchinit.engine_invalid_webextension",
            engine._extensionID,
            4
          );
        } else if (engine.name != providerSettings.name) {
          lazy.logConsole.debug(
            `Add-on ${engine._extensionID} for search engine ${engine.name} has a different name!`
          );
          Services.telemetry.keyedScalarSet(
            "browser.searchinit.engine_invalid_webextension",
            engine._extensionID,
            5
          );
        } else if (!engine.checkSearchUrlMatchesManifest(providerSettings)) {
          lazy.logConsole.debug(
            `Add-on ${engine._extensionID} for search engine ${engine.name} has out-of-date manifest!`
          );
          Services.telemetry.keyedScalarSet(
            "browser.searchinit.engine_invalid_webextension",
            engine._extensionID,
            6
          );
        }
      }
    }
    lazy.logConsole.debug("WebExtension engine check complete");
  }

  /**
   * Creates and adds a WebExtension based engine.
   * Note: this is currently used for enterprise policy engines as well.
   *
   * @param {object} options
   * @param {string} options.extensionID
   *   The extension ID being added for the engine.
   * @param {nsIURI} [options.extensionBaseURI]
   *   The base URI of the extension.
   * @param {boolean} options.isAppProvided
   *   True if the WebExtension is built-in or installed into the system scope.
   * @param {object} options.manifest
   *   An object that represents the extension's manifest.
   * @param {stirng} [options.locale]
   *   The locale to use within the WebExtension. Defaults to the WebExtension's
   *   default locale.
   * @param {initEngine} [options.initEngine]
   *   Set to true if this engine is being loaded during initialisation.
   */
  async _createAndAddEngine({
    extensionID,
    extensionBaseURI,
    isAppProvided,
    manifest,
    locale = lazy.SearchUtils.DEFAULT_TAG,
    initEngine = false,
  }) {
    // We install search extensions during the init phase, both built in
    // web extensions freshly installed (via addEnginesFromExtension) or
    // user installed extensions being reenabled calling this directly.
    if (!this._initialized && !isAppProvided && !initEngine) {
      await this.init();
    }

    let isCurrent = false;

    // Special search engines (policy and user) are skipped for migration as
    // there would never have been an OpenSearch engine associated with those.
    for (let engine of this._engines.values()) {
      if (
        !engine.extensionID &&
        engine._loadPath.startsWith(`jar:[profile]/extensions/${extensionID}`)
      ) {
        // This is a legacy extension engine that needs to be migrated to WebExtensions.
        lazy.logConsole.debug("Migrating existing engine");
        isCurrent = isCurrent || this.defaultEngine == engine;
        await this.removeEngine(engine);
      }
    }

    let newEngine = new lazy.AddonSearchEngine({
      isAppProvided,
      details: {
        extensionID,
        extensionBaseURI,
        manifest,
        locale,
      },
    });

    let existingEngine = this._engines.get(newEngine.name);
    if (existingEngine) {
      throw Components.Exception(
        "An engine with that name already exists!",
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
      let locale = engine._locale || lazy.SearchUtils.DEFAULT_TAG;
      let manifest = await this.#getManifestForLocale(extension, locale);
      let configuration =
        engines.find(
          e =>
            e.webExtension.id == extension.id && e.webExtension.locale == locale
        ) ?? {};

      let originalName = engine.name;
      let name = manifest.chrome_settings_overrides.search_provider.name.trim();
      if (originalName != name && this._engines.has(name)) {
        throw new Error("Can't upgrade to the same name as an existing engine");
      }

      let isDefault = engine == this.defaultEngine;
      let isDefaultPrivate = engine == this.defaultPrivateEngine;

      engine._updateFromManifest(
        extension.id,
        extension.baseURI,
        manifest,
        locale,
        configuration
      );

      if (originalName != engine.name) {
        this._engines.delete(originalName);
        this._engines.set(engine.name, engine);
        if (isDefault) {
          this._settings.setVerifiedAttribute("current", engine.name);
        }
        if (isDefaultPrivate) {
          this._settings.setVerifiedAttribute("private", engine.name);
        }
        this.__sortedEngines = null;
      }
    }
    return extensionEngines;
  }

  async #installExtensionEngine(extension, locales, initEngine = false) {
    lazy.logConsole.debug("installExtensionEngine:", extension.id);

    let installLocale = async locale => {
      let manifest = await this.#getManifestForLocale(extension, locale);
      return this.#addEngineForManifest(
        extension,
        manifest,
        locale,
        initEngine
      );
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

  async #addEngineForManifest(
    extension,
    manifest,
    locale = lazy.SearchUtils.DEFAULT_TAG,
    initEngine = false
  ) {
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

    return this._createAndAddEngine({
      extensionID: extension.id,
      extensionBaseURI: extension.baseURI,
      isAppProvided: extension.isAppProvided,
      manifest,
      locale,
      initEngine,
    });
  }

  #internalRemoveEngine(engine) {
    // Remove the engine from _sortedEngines
    if (this.__sortedEngines) {
      var index = this.__sortedEngines.indexOf(engine);
      if (index == -1) {
        throw Components.Exception(
          "Can't find engine to remove in _sortedEngines!",
          Cr.NS_ERROR_FAILURE
        );
      }
      this.__sortedEngines.splice(index, 1);
    }

    // Remove the engine from the internal store
    this._engines.delete(engine.name);
  }

  /**
   * Helper function to find a new default engine and set it. This could
   * be used if there is not default set yet, or if the current default is
   * being removed.
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
   * @param {string} [excludeEngineName]
   *   Exclude the given engine name from the search for a new engine. This is
   *   typically used when removing engines to ensure we do not try to reselect
   *   the same engine again.
   * @returns {nsISearchEngine|null}
   *   The appropriate search engine, or null if one could not be determined.
   */
  #findAndSetNewDefaultEngine({ privateMode, excludeEngineName = "" }) {
    // First to the original default engine...
    let newDefault = privateMode
      ? this.originalPrivateDefaultEngine
      : this.originalDefaultEngine;

    if (
      !newDefault ||
      newDefault.hidden ||
      newDefault.name == excludeEngineName
    ) {
      let sortedEngines = this.#sortedVisibleEngines;
      let generalSearchEngines = sortedEngines.filter(
        e => e.isGeneralPurposeEngine
      );

      // then to the first visible general search engine that isn't excluded...
      let firstVisible = generalSearchEngines.find(
        e => e.name != excludeEngineName
      );
      if (firstVisible) {
        newDefault = firstVisible;
      } else if (newDefault) {
        // then to the original if it is not the one that is excluded...
        if (newDefault.name != excludeEngineName) {
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
   */
  #setEngineDefault(privateMode, newEngine) {
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

    const newCurrentEngine = this._engines.get(newEngine.name);
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
    // users who have switched away from and then back to the build's "default"
    // engine. So clear the user pref when the currentEngine is set to the
    // build's default engine, so that the currentEngine getter falls back to
    // whatever the default is.
    let newName = newCurrentEngine.name;
    const originalDefault = privateMode
      ? this.originalPrivateDefaultEngine
      : this.originalDefaultEngine;
    if (newCurrentEngine == originalDefault) {
      newName = "";
    }

    this._settings.setVerifiedAttribute(
      privateMode ? "private" : "current",
      newName
    );

    // Only do this if we're initialized though - this function can get called
    // during initalization.
    if (this._initialized) {
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

  #onSeparateDefaultPrefChanged() {
    // Clear out the sorted engines settings, so that we re-sort it if necessary.
    this.__sortedEngines = null;
    // We should notify if the normal default, and the currently saved private
    // default are different. Otherwise, save the energy.
    if (this.defaultEngine != this._getEngineDefault(true)) {
      lazy.SearchUtils.notifyAction(
        // Always notify with the new private engine, the function checks
        // the preference value for us.
        this.defaultPrivateEngine,
        lazy.SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
      );
      // Also update the telemetry data.
      this.#recordTelemetryData();
    }
  }

  #getEngineInfo(engine) {
    if (!engine) {
      // The defaultEngine getter will throw if there's no engine at all,
      // which shouldn't happen unless an add-on or a test deleted all of them.
      // Our preferences UI doesn't let users do that.
      Cu.reportError("getDefaultEngineInfo: No default engine");
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
      let engineHost = engine._getURLOfType(lazy.SearchUtils.URL_TYPE.SEARCH)
        .templateHost;
      for (let innerEngine of this._engines.values()) {
        if (!innerEngine.isAppProvided) {
          continue;
        }

        let innerEngineURL = innerEngine._getURLOfType(
          lazy.SearchUtils.URL_TYPE.SEARCH
        );
        if (innerEngineURL.templateHost == engineHost) {
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
        const urlTest = /^(?:www\.google\.|search\.aol\.|yandex\.)|(?:search\.yahoo|\.ask|\.bing|\.startpage|\.baidu|duckduckgo)\.com$/;
        sendSubmissionURL = urlTest.test(engineHost);
      }
    }

    if (sendSubmissionURL) {
      let uri = engine
        ._getURLOfType("text/html")
        .getSubmission("", engine, "searchbar").uri;
      uri = uri
        .mutate()
        .setUserPass("") // Avoid reporting a username or password.
        .finalize();
      engineData.submissionURL = uri.spec;
    }

    return [engine.telemetryId, engineData];
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
      info.defaultSearchEngineData.submissionURL
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
        info.defaultPrivateSearchEngineData.submissionURL
      );
      Glean.searchEnginePrivate.verified.set(
        info.defaultPrivateSearchEngineData.origin
      );
    } else {
      Glean.searchEnginePrivate.displayName.set("");
      Glean.searchEnginePrivate.loadPath.set("");
      Glean.searchEnginePrivate.submissionUrl.set("");
      Glean.searchEnginePrivate.verified.set("");
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

  /**
   * Gets the WebExtensionPolicy for an add-on.
   *
   * @param {string} id
   *   The WebExtension id.
   * @returns {WebExtensionPolicy}
   */
  async #getExtensionPolicy(id) {
    let policy = WebExtensionPolicy.getByID(id);
    if (!policy) {
      let idPrefix = id.split("@")[0];
      let path = `resource://search-extensions/${idPrefix}/`;
      await lazy.AddonManager.installBuiltinAddon(path);
      policy = WebExtensionPolicy.getByID(id);
    }
    // On startup the extension may have not finished parsing the
    // manifest, wait for that here.
    await policy.readyPromise;
    return policy;
  }

  #addObservers() {
    if (this.#observersAdded) {
      // There might be a race between synchronous and asynchronous
      // initialization for which we try to register the observers twice.
      return;
    }
    this.#observersAdded = true;

    lazy.NimbusFeatures.search.onUpdate(() =>
      Services.search.wrappedJSObject._maybeReloadEngines()
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
          if (!this._initialized) {
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

    lazy.NimbusFeatures.search.off(() =>
      Services.search.wrappedJSObject._maybeReloadEngines()
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
        this._maybeReloadEngines().catch(Cu.reportError);
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
            this._maybeReloadEngines().catch(Cu.reportError);
          }
        });
        break;
      case lazy.Region.REGION_TOPIC:
        lazy.logConsole.debug("Region updated:", lazy.Region.home);
        this._maybeReloadEngines().catch(Cu.reportError);
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
    let policy = await this.#getExtensionPolicy(config.webExtension.id);
    let locale =
      "locale" in config.webExtension
        ? config.webExtension.locale
        : lazy.SearchUtils.DEFAULT_TAG;

    let manifest = await this.#getManifestForLocale(policy.extension, locale);

    return new lazy.AddonSearchEngine({
      isAppProvided: policy.extension.isAppProvided,
      details: {
        extensionID: policy.extension.id,
        extensionBaseURI: policy.extension.baseURI,
        manifest,
        locale,
        config,
      },
    });
  }

  /**
   * @param {object} metaData
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
      return metaData?.[p] !== this._settings.getAttribute(p);
    });
  }

  /**
   * Shows an infobar to notify the user their default search engine has been
   * removed and replaced by a new default search engine.
   *
   * This method is prefixed with _ rather than # because it is
   * called in a test.
   *
   * @param {string} prevCurrentEngine
   *   The engine that was previously the default engine and is to be replaced.
   * @param {string} newCurrentEngine
   *   The engine that will be the new the default engine.
   *
   */
  _showRemovalOfSearchEngineNotificationBox(
    prevCurrentEngine,
    newCurrentEngine
  ) {
    let win = Services.wm.getMostRecentBrowserWindow();
    win.BrowserSearch.removalOfSearchEngineNotificationBox(
      prevCurrentEngine,
      newCurrentEngine
    );
  }

  /**
   * Get the localized manifest from the WebExtension for the given locale or
   * manifest default locale.
   *
   * The search service configuration overloads the add-on manager concepts of
   * locales, and forces particular locales within the WebExtension to be used,
   * ignoring the user's current locale. The user's current locale is taken into
   * account within the configuration, just not in the WebExtension.
   *
   * @param {object} extension
   *   The extension to get the manifest from.
   * @param {string} locale
   *   The locale to load from the WebExtension. If this is `DEFAULT_TAG`, then
   *   the default locale is loaded.
   * @returns {object}
   *   The loaded manifest.
   */
  async #getManifestForLocale(extension, locale) {
    let manifest = extension.manifest;

    // If the locale we want from the WebExtension is the extension's default
    // then we get that from the manifest here. We do this because if we
    // are reloading due to the locale change, the add-on manager might not
    // have updated the WebExtension's manifest to the new version by the
    // time we hit this code.
    let localeToLoad =
      locale == lazy.SearchUtils.DEFAULT_TAG ? manifest.default_locale : locale;

    if (localeToLoad) {
      manifest = await extension.getLocalizedManifest(localeToLoad);
    }
    return manifest;
  }
} // end SearchService class

var engineUpdateService = {
  scheduleNextUpdate(engine) {
    var interval = engine._updateInterval || SEARCH_DEFAULT_UPDATE_INTERVAL;
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
    let updateURL = engine._getURLOfType(lazy.SearchUtils.URL_TYPE.OPENSEARCH);
    let updateURI =
      updateURL && updateURL._hasRelation("self")
        ? updateURL.getSubmission("", engine).uri
        : lazy.SearchUtils.makeURI(engine._updateURL);
    if (updateURI) {
      if (engine.isAppProvided && !updateURI.schemeIs("https")) {
        lazy.logConsole.debug("Invalid scheme for default engine update");
        return;
      }

      lazy.logConsole.debug("updating", engine.name, updateURI.spec);
      testEngine = new lazy.OpenSearchEngine();
      testEngine._engineToUpdate = engine;
      try {
        testEngine._install(updateURI);
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
   * @param {function} listener
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
   * @returns {array}
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
      Cu.reportError(ex);
    }
    lazy.logConsole.debug("Allow list is:", result);
    return result;
  }
}

var EXPORTED_SYMBOLS = ["SearchService"];
