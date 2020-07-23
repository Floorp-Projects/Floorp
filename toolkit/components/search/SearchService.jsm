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

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  clearTimeout: "resource://gre/modules/Timer.jsm",
  IgnoreLists: "resource://gre/modules/IgnoreLists.jsm",
  OpenSearchEngine: "resource://gre/modules/OpenSearchEngine.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Region: "resource://gre/modules/Region.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  SearchCache: "resource://gre/modules/SearchCache.jsm",
  SearchEngine: "resource://gre/modules/SearchEngine.jsm",
  SearchEngineSelector: "resource://gre/modules/SearchEngineSelector.jsm",
  SearchStaticData: "resource://gre/modules/SearchStaticData.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  gEnvironment: ["@mozilla.org/process/environment;1", "nsIEnvironment"],
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gGeoSpecificDefaultsEnabled",
  SearchUtils.BROWSER_SEARCH_PREF + "geoSpecificDefaults",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gModernConfig",
  SearchUtils.BROWSER_SEARCH_PREF + "modernConfig",
  false,
  () => {
    // We'll re-init the service, regardless of which way the pref-flip went.
    Services.search.reInit();
  }
);

XPCOMUtils.defineLazyGetter(this, "logConsole", () => {
  return console.createInstance({
    prefix: "SearchService",
    maxLogLevel: SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

// Directory service keys
const NS_APP_DISTRIBUTION_SEARCH_DIR_LIST = "SrchPluginsDistDL";

// We load plugins from EXT_SEARCH_PREFIX, where a list.json
// file needs to exist to list available engines.
const EXT_SEARCH_PREFIX = "resource://search-extensions/";

// The address we use to sign the built in search extensions with.
const EXT_SIGNING_ADDRESS = "search.mozilla.org";

const TOPIC_LOCALES_CHANGE = "intl:app-locales-changed";
const QUIT_APPLICATION_TOPIC = "quit-application";

// The default engine update interval, in days. This is only used if an engine
// specifies an updateURL, but not an updateInterval.
const SEARCH_DEFAULT_UPDATE_INTERVAL = 7;

// The default interval before checking again for the name of the
// default engine for the region, in seconds. Only used if the response
// from the server doesn't specify an interval.
const SEARCH_GEO_DEFAULT_UPDATE_INTERVAL = 2592000; // 30 days.

// This is the amount of time we'll be idle for before applying any configuration
// changes.
const REINIT_IDLE_TIME_SEC = 5 * 60;

// Some extensions package multiple locales into a single extension, for those
// engines we use engine-locale to address the engine.
// This is to be removed in https://bugzilla.mozilla.org/show_bug.cgi?id=1532246
const MULTI_LOCALE_ENGINES = [
  "amazon",
  "amazondotcom",
  "bolcom",
  "ebay",
  "google",
  "marktplaats",
  "mercadolibre",
  "wikipedia",
  "wiktionary",
  "yandex",
  "multilocale",
];

// A method that tries to determine our region via an XHR geoip lookup.
var ensureKnownRegion = async function(ss) {
  try {
    if (gGeoSpecificDefaultsEnabled && !gModernConfig) {
      // The territory default we have already fetched may have expired.
      let expired =
        (ss._cache.getAttribute("searchDefaultExpir") || 0) <= Date.now();
      // If we have a default engine or a list of visible default engines
      // saved, the hashes should be valid, verify them now so that we can
      // refetch if they have been tampered with.
      let defaultEngine = ss._cache.getVerifiedAttribute("searchDefault");
      let visibleDefaultEngines = ss._cache.getVerifiedAttribute(
        "visibleDefaultEngines"
      );
      let hasValidHashes =
        (defaultEngine || defaultEngine === undefined) &&
        (visibleDefaultEngines || visibleDefaultEngines === undefined);
      if (expired || !hasValidHashes) {
        await new Promise(resolve => {
          let timeoutMS = Services.prefs.getIntPref(
            "geo.provider.network.timeToWaitBeforeSending"
          );
          let timerId = setTimeout(() => {
            timerId = null;
            resolve();
          }, timeoutMS);

          let callback = () => {
            clearTimeout(timerId);
            resolve();
          };
          fetchRegionDefault(ss)
            .then(callback)
            .catch(err => {
              Cu.reportError(err);
              callback();
            });
        });
      }
    }
  } catch (ex) {
    Cu.reportError(ex);
  } finally {
    // Since bug 1492475, we don't block our init flows on the region fetch as
    // performed here. But we'd still like to unit-test its implementation, thus
    // we fire this observer notification.
    Services.obs.notifyObservers(
      null,
      SearchUtils.TOPIC_SEARCH_SERVICE,
      "ensure-known-region-done"
    );
  }
};

// This converts our legacy google engines to the
// new codes. We have to manually change them here
// because we can't change the default name in absearch.
function convertGoogleEngines(engineNames) {
  let overrides = {
    google: "google-b-d",
    "google-2018": "google-b-1-d",
  };

  for (let engine in overrides) {
    let index = engineNames.indexOf(engine);
    if (index > -1) {
      engineNames[index] = overrides[engine];
    }
  }
  return engineNames;
}

// This will make an HTTP request to a Mozilla server that will return
// JSON data telling us what engine should be set as the default for
// the current region, and how soon we should check again.
//
// The optional cohort value returned by the server is to be kept locally
// and sent to the server the next time we ping it. It lets the server
// identify profiles that have been part of a specific experiment.
//
// This promise may take up to 100s to resolve, it's the caller's
// responsibility to ensure with a timer that we are not going to
// block the async init for too long.
// @deprecated Unused in the modern config.
var fetchRegionDefault = ss =>
  new Promise(resolve => {
    let urlTemplate = Services.prefs
      .getDefaultBranch(SearchUtils.BROWSER_SEARCH_PREF)
      .getCharPref("geoSpecificDefaults.url");
    let endpoint = Services.urlFormatter.formatURL(urlTemplate);

    // As an escape hatch, no endpoint means no region specific defaults.
    if (!endpoint) {
      resolve();
      return;
    }

    // Append the optional cohort value.
    const cohortPref = "browser.search.cohort";
    let cohort = Services.prefs.getCharPref(cohortPref, "");
    if (cohort) {
      endpoint += "/" + cohort;
    }

    logConsole.debug("fetchRegionDefault starting with endpoint ", endpoint);

    let startTime = Date.now();
    let request = new XMLHttpRequest();
    request.timeout = 100000; // 100 seconds as the last-chance fallback
    request.onload = function(event) {
      let took = Date.now() - startTime;

      let status = event.target.status;
      if (status != 200) {
        logConsole.debug("fetchRegionDefault failed with HTTP code ", status);
        let retryAfter = request.getResponseHeader("retry-after");
        if (retryAfter) {
          ss._cache.setAttribute(
            "searchDefaultExpir",
            Date.now() + retryAfter * 1000
          );
        }
        resolve();
        return;
      }

      let response = event.target.response || {};
      logConsole.debug("received", response.toSource());

      if (response.cohort) {
        Services.prefs.setCharPref(cohortPref, response.cohort);
      } else {
        Services.prefs.clearUserPref(cohortPref);
      }

      if (response.settings && response.settings.searchDefault) {
        let defaultEngine = response.settings.searchDefault;
        ss._cache.setVerifiedAttribute("searchDefault", defaultEngine);
        logConsole.debug(
          "fetchRegionDefault saved searchDefault:",
          defaultEngine
        );
      }

      if (response.settings && response.settings.visibleDefaultEngines) {
        let visibleDefaultEngines = response.settings.visibleDefaultEngines;
        let string = visibleDefaultEngines.join(",");
        ss._cache.setVerifiedAttribute("visibleDefaultEngines", string);
        logConsole.debug(
          "fetchRegionDefault saved visibleDefaultEngines:",
          string
        );
      }

      let interval = response.interval || SEARCH_GEO_DEFAULT_UPDATE_INTERVAL;
      let milliseconds = interval * 1000; // |interval| is in seconds.
      ss._cache.setAttribute("searchDefaultExpir", Date.now() + milliseconds);

      logConsole.debug(
        "fetchRegionDefault got success response in",
        took,
        "ms"
      );
      // If we're doing this somewhere during the app's lifetime, reload the list
      // of engines in order to pick up any geo-specific changes.
      ss._maybeReloadEngines().finally(resolve);
    };
    request.ontimeout = function(event) {
      logConsole.debug("fetchRegionDefault: XHR finally timed-out");
      resolve();
    };
    request.onerror = function(event) {
      logConsole.debug(
        "fetchRegionDefault: failed to retrieve territory default information"
      );
      resolve();
    };
    request.open("GET", endpoint, true);
    request.setRequestHeader("Content-Type", "application/json");
    request.responseType = "json";
    request.send();
  });

/**
 * Wrapper for nsIPrefBranch::getComplexValue.
 * @param {string} prefName
 *   The name of the pref to get.
 * @param {*} defaultValue
 *   The value to return if the preference isn't found.
 * @returns {*}
 *   Returns either the preference value, or the default value.
 */
function getLocalizedPref(prefName, defaultValue) {
  try {
    return Services.prefs.getComplexValue(prefName, Ci.nsIPrefLocalizedString)
      .data;
  } catch (ex) {}

  return defaultValue;
}

var gInitialized = false;
var gReinitializing = false;

// nsISearchParseSubmissionResult
function ParseSubmissionResult(
  engine,
  terms,
  termsParameterName,
  termsOffset,
  termsLength
) {
  this._engine = engine;
  this._terms = terms;
  this._termsParameterName = termsParameterName;
  this._termsOffset = termsOffset;
  this._termsLength = termsLength;
}
ParseSubmissionResult.prototype = {
  get engine() {
    return this._engine;
  },
  get terms() {
    return this._terms;
  },
  get termsParameterName() {
    return this._termsParameterName;
  },
  get termsOffset() {
    return this._termsOffset;
  },
  get termsLength() {
    return this._termsLength;
  },
  QueryInterface: ChromeUtils.generateQI(["nsISearchParseSubmissionResult"]),
};

const gEmptyParseSubmissionResult = Object.freeze(
  new ParseSubmissionResult(null, "", "", -1, 0)
);

/**
 * The search service handles loading and maintaining of search engines. It will
 * also work out the default lists for each locale/region.
 *
 * @implements {nsISearchService}
 */
function SearchService() {
  this._initObservers = PromiseUtils.defer();
  this._engines = new Map();
  this._cache = new SearchCache(this);
}

SearchService.prototype = {
  classID: Components.ID("{7319788a-fe93-4db3-9f39-818cf08f4256}"),

  // The current status of initialization. Note that it does not determine if
  // initialization is complete, only if an error has been encountered so far.
  _initRV: Cr.NS_OK,

  // The boolean indicates that the initialization has started or not.
  _initStarted: null,

  // The engine selector singleton that is managing the engine configuration.
  _engineSelector: null,

  _ensureKnownRegionPromise: null,

  /**
   * Various search engines may be ignored if their submission urls contain a
   * string that is in the list. The list is controlled via remote settings.
   */
  _submissionURLIgnoreList: [],

  /**
   * Various search engines may be ignored if their load path is contained
   * in this list. The list is controlled via remote settings.
   */
  _loadPathIgnoreList: [],

  /**
   * A map of engine display names to `SearchEngine`.
   */
  _engines: null,

  /**
   * An array of engine short names sorted into display order.
   */
  __sortedEngines: null,

  /**
   * A flag to prevent setting of useDBForOrder when there's non-user
   * activity happening.
   */
  _dontSetUseDBForOrder: false,

  /**
   * This holds the current list of visible engines from the configuration,
   * and is used to update the cache. If the cache value is different to those
   * in the configuration, then the configuration has changed. The engines
   * are loaded using both the new set, and the user's current set (if they
   * still exist).
   * @deprecated Unused in the modern configuration.
   */
  _visibleDefaultEngines: [],

  /**
   * An object containing the {id, locale} of the WebExtension for the default
   * engine, as suggested by the configuration.
   * For the legacy configuration, this is the user visible name.
   */
  _searchDefault: null,

  /**
   * An object containing the {id, locale} of the WebExtension for the default
   * engine for private browsing mode, as suggested by the configuration.
   * For the legacy configuration, this is the user visible name.
   */
  _searchPrivateDefault: null,

  /**
   * The suggested order of engines from the configuration.
   * For modern configuration:
   *   This is an array of objects containing the WebExtension ID and Locale.
   *   This is only needed whilst we investigate issues with cache corruption
   *   (bug 1589710).
   * For legacy configuration:
   *   This is an array of strings which are the display name of the engines.
   */
  _searchOrder: [],

  /**
   * A Set of installed search extensions reported by AddonManager
   * startup before SearchSevice has started. Will be installed
   * during init().
   */
  _startupExtensions: new Set(),

  // A reference to the handler for the default override allow list.
  _defaultOverrideAllowlist: null,

  // This reflects the combined values of the prefs for enabling the separate
  // private default UI, and for the user choosing a separate private engine.
  // If either one is disabled, then we don't enable the separate private default.
  get _separatePrivateDefault() {
    return (
      this._separatePrivateDefaultPrefValue &&
      this._separatePrivateDefaultEnabledPrefValue
    );
  },

  /**
   * Resets the locally stored data to the original empty values in preparation
   * for a reinit or a reset.
   */
  _resetLocalData() {
    this._engines.clear();
    this.__sortedEngines = null;
    this._currentEngine = null;
    this._currentPrivateEngine = null;
    this._visibleDefaultEngines = [];
    this._searchDefault = null;
    this._searchPrivateDefault = null;
    this._searchOrder = [];
    this._metaData = {};
    this._maybeReloadDebounce = false;
  },

  // If initialization has not been completed yet, perform synchronous
  // initialization.
  // Throws in case of initialization error.
  _ensureInitialized() {
    if (gInitialized) {
      if (!Components.isSuccessCode(this._initRV)) {
        logConsole.debug("_ensureInitialized: failure");
        throw Components.Exception(
          "SearchService previously failed to initialize",
          this._initRV
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
  },

  /**
   * Asynchronous implementation of the initializer.
   *
   * @returns {number}
   *   A Components.results success code on success, otherwise a failure code.
   */
  async _init() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_separatePrivateDefaultPrefValue",
      SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
      false,
      this._onSeparateDefaultPrefChanged.bind(this)
    );

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_separatePrivateDefaultEnabledPrefValue",
      SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
      false,
      this._onSeparateDefaultPrefChanged.bind(this)
    );

    // We need to catch the region being updated
    // during initialisation so we start listening
    // straight away.
    Services.obs.addObserver(this, Region.REGION_TOPIC);

    try {
      if (gModernConfig) {
        // Create the search engine selector.
        this._engineSelector = new SearchEngineSelector(
          this._handleConfigurationUpdated.bind(this)
        );
      }

      // See if we have a cache file so we don't have to parse a bunch of XML.
      let cache = await this._cache.get();

      // The init flow is not going to block on a fetch from an external service,
      // but we're kicking it off as soon as possible to prevent UI flickering as
      // much as possible.
      this._ensureKnownRegionPromise = ensureKnownRegion(this)
        .catch(ex => logConsole.error("_init: failure determining region:", ex))
        .finally(() => (this._ensureKnownRegionPromise = null));

      this._setupRemoteSettings().catch(Cu.reportError);

      await this._loadEngines(cache);

      // If we've got this far, but the application is now shutting down,
      // then we need to abandon any further work, especially not writing
      // the cache. We do this, because the add-on manager has also
      // started shutting down and as a result, we might have an incomplete
      // picture of the installed search engines. Writing the cache at
      // this stage would potentially mean the user would loose their engine
      // data.
      // We will however, rebuild the cache on next start up if we detect
      // it is necessary.
      if (Services.startup.shuttingDown) {
        logConsole.warn("_init: abandoning init due to shutting down");
        this._initRV = Cr.NS_ERROR_ABORT;
        this._initObservers.reject(this._initRV);
        return this._initRV;
      }

      // Make sure the current list of engines is persisted, without the need to wait.
      logConsole.debug("_init: engines loaded, writing cache");
      this._addObservers();
    } catch (ex) {
      this._initRV = ex.result !== undefined ? ex.result : Cr.NS_ERROR_FAILURE;
      logConsole.error("_init: failure initializing search:", ex.result);
    }
    gInitialized = true;
    if (Components.isSuccessCode(this._initRV)) {
      this._initObservers.resolve(this._initRV);
    } else {
      this._initObservers.reject(this._initRV);
    }
    Services.obs.notifyObservers(
      null,
      SearchUtils.TOPIC_SEARCH_SERVICE,
      "init-complete"
    );

    logConsole.debug("Completed _init");
    return this._initRV;
  },

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
  async _setupRemoteSettings() {
    // Now we have the values, listen for future updates.
    let listener = this._handleIgnoreListUpdated.bind(this);

    const current = await IgnoreLists.getAndSubscribe(listener);
    // Only save the listener after the subscribe, otherwise for tests it might
    // not be fully set up by the time we remove it again.
    this._ignoreListListener = listener;

    await this._handleIgnoreListUpdated({ data: { current } });
    Services.obs.notifyObservers(
      null,
      SearchUtils.TOPIC_SEARCH_SERVICE,
      "settings-update-complete"
    );
  },

  /**
   * This handles updating of the ignore list settings, and removing any ignored
   * engines.
   *
   * @param {object} eventData
   *   The event in the format received from RemoteSettings.
   */
  async _handleIgnoreListUpdated(eventData) {
    logConsole.debug("_handleIgnoreListUpdated");
    const {
      data: { current },
    } = eventData;

    for (const entry of current) {
      if (entry.id == "load-paths") {
        this._loadPathIgnoreList = [...entry.matches];
      } else if (entry.id == "submission-urls") {
        this._submissionURLIgnoreList = [...entry.matches];
      }
    }

    // If we have not finished initializing, then we wait for the initialization
    // to complete.
    if (!this.isInitialized) {
      await this._initObservers;
    }
    // We try to remove engines manually, as this should be more efficient and
    // we don't really want to cause a re-init as this upsets unit tests.
    let engineRemoved = false;
    for (let engine of this._engines.values()) {
      if (this._engineMatchesIgnoreLists(engine)) {
        await this.removeEngine(engine);
        engineRemoved = true;
      }
    }
    // If we've removed an engine, and we don't have any left, we need to do
    // a re-init - it is possible the cache just had one engine in it, and that
    // is now empty, so we need to load from our main list.
    if (engineRemoved && !this._engines.size) {
      this._reInit();
    }
  },

  /**
   * Determines if a given engine matches the ignorelists or not.
   *
   * @param {Engine} engine
   *   The engine to check against the ignorelists.
   * @returns {boolean}
   *   Returns true if the engine matches a ignorelists entry.
   */
  _engineMatchesIgnoreLists(engine) {
    if (this._loadPathIgnoreList.includes(engine._loadPath)) {
      return true;
    }
    let url = engine
      ._getURLOfType("text/html")
      .getSubmission("dummy", engine)
      .uri.spec.toLowerCase();
    if (
      this._submissionURLIgnoreList.some(code =>
        url.includes(code.toLowerCase())
      )
    ) {
      return true;
    }
    return false;
  },

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

    if (!this._defaultOverrideAllowlist) {
      this._defaultOverrideAllowlist = new SearchDefaultOverrideAllowlistHandler();
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
        !(await this._defaultOverrideAllowlist.canOverride(
          extension,
          engine._extensionID
        ))
      ) {
        logConsole.debug(
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
      logConsole.debug(
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
      (await this._defaultOverrideAllowlist.canOverride(
        extension,
        engine._extensionID
      ))
    ) {
      engine.overrideWithExtension(extension.id, extension.manifest);
      logConsole.debug(
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
  },

  /**
   * Handles the search configuration being - adds a wait on the user
   * being idle, before the search engine update gets handled.
   */
  _handleConfigurationUpdated() {
    if (this._queuedIdle) {
      return;
    }

    this._queuedIdle = true;

    this.idleService.addIdleObserver(this, REINIT_IDLE_TIME_SEC);
  },

  _listJSONURL: `${EXT_SEARCH_PREFIX}list.json`,

  get _sortedEngines() {
    if (!this.__sortedEngines) {
      return this._buildSortedEngineList();
    }
    return this.__sortedEngines;
  },

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
  _originalDefaultEngine(privateMode = false) {
    // The modern configuration doesn't need the verified attributes from the
    // cache as we can calculate it all on startup anyway from the engines
    // configuration.
    if (gModernConfig) {
      let defaultEngine = this._getEngineByWebExtensionDetails(
        privateMode && this._searchPrivateDefault
          ? this._searchPrivateDefault
          : this._searchDefault
      );

      if (defaultEngine) {
        return defaultEngine;
      }

      if (privateMode) {
        // If for some reason we can't find the private mode engine, fall back
        // to the non-private one.
        return this._originalDefaultEngine(false);
      }

      // Something unexpected as happened. In order to recover the original
      // default engine, use the first visible engine which is the best we can do.
      return this._getSortedEngines(false)[0];
    }

    // Legacy configuration

    let defaultEngineName = this._cache.getVerifiedAttribute(
      privateMode ? "searchDefaultPrivate" : "searchDefault"
    );
    if (!defaultEngineName) {
      // We only allow the old defaultenginename pref for distributions
      // We can't use isPartnerBuild because we need to allow reading
      // of the defaultengine name pref for funnelcakes.
      if (SearchUtils.distroID && !privateMode) {
        let defaultPrefB = Services.prefs.getDefaultBranch(
          SearchUtils.BROWSER_SEARCH_PREF
        );
        try {
          defaultEngineName = defaultPrefB.getComplexValue(
            "defaultenginename",
            Ci.nsIPrefLocalizedString
          ).data;
        } catch (ex) {
          // If the default pref is invalid (e.g. an add-on set it to a bogus value)
          // use the default engine from the list.json.
          // This should eventually be the common case. We should only have the
          // defaultenginename pref for distributions.
          // Worst case, getEngineByName will just return null, which is the best we can do.
          defaultEngineName = this._searchDefault;
        }
      } else {
        defaultEngineName = privateMode
          ? this._searchPrivateDefault
          : this._searchDefault;
      }
    }

    if (!defaultEngineName && privateMode) {
      // We don't have a separate engine, fall back to the non-private one.
      return this._originalDefaultEngine(false);
    }

    let defaultEngine = this.getEngineByName(defaultEngineName);
    if (!defaultEngine) {
      // The cache might have an out-of-date value if we've changed locale or
      // region. Fall back to the value from the configuration as that might be
      // more accurate.
      defaultEngineName = privateMode
        ? this._searchPrivateDefault
        : this._searchDefault;
      defaultEngine = this.getEngineByName(defaultEngineName);

      if (!defaultEngine) {
        // Something unexpected as happened. In order to recover the original default engine,
        // use the first visible engine which is the best we can do.
        return this._getSortedEngines(false)[0];
      }
    }

    return defaultEngine;
  },

  /**
   * @returns {SearchEngine}
   *   The engine that is the default for this locale/region, ignoring any
   *   user changes to the default engine.
   */
  get originalDefaultEngine() {
    return this._originalDefaultEngine();
  },

  /**
   * @returns {SearchEngine}
   *   The engine that is the default for this locale/region in private browsing
   *   mode, ignoring any user changes to the default engine.
   *   Note: if there is no default for this locale/region, then the non-private
   *   browsing engine will be returned.
   */
  get originalPrivateDefaultEngine() {
    return this._originalDefaultEngine(this._separatePrivateDefault);
  },

  resetToOriginalDefaultEngine() {
    let originalDefaultEngine = this.originalDefaultEngine;
    originalDefaultEngine.hidden = false;
    this.defaultEngine = originalDefaultEngine;
  },

  /**
   * Loads engines asynchronously.
   *
   * @param {object} cache
   *   An object representing the search engine cache.
   * @param {boolean} isReload
   *   Set to true if this load is happening during a reload.
   */
  async _loadEngines(cache, isReload) {
    if (!gModernConfig) {
      await this._loadEnginesLegacy(cache, isReload);
      return;
    }

    logConsole.debug("_loadEngines: start");
    let { engines, privateDefault } = await this._fetchEngineSelectorEngines();
    this._setDefaultAndOrdersFromSelector(engines, privateDefault);

    let enginesCorrupted = false;

    // If this is an update of Firefox, then the expected engines might have
    // changed so we can't do the corrupt cache check.
    let majorChange =
      !cache.engines ||
      cache.version != SearchUtils.CACHE_VERSION ||
      cache.locale != Services.locale.requestedLocale ||
      cache.buildID != Services.appinfo.platformBuildID;

    if (!majorChange) {
      const engineInCacheList = engine => {
        return cache.builtInEngineList.find(details => {
          return (
            engine.webExtension.id == details.id &&
            engine.webExtension.locale == details.locale
          );
        });
      };

      if (
        // If the cache builtInEngineList matches what we expect from the
        // selector engines...
        cache.builtInEngineList &&
        cache.builtInEngineList.length == engines.length &&
        engines.every(engineInCacheList) &&
        // ...and the cached built-in cached do not match our expectation...
        cache.engines.filter(e => e._isAppProvided).length !=
          cache.builtInEngineList.length
      ) {
        // ...then the cache is corrupted in some manner.
        enginesCorrupted = true;
      }
    }

    Services.telemetry.scalarSet(
      "browser.searchinit.engines_cache_corrupted",
      enginesCorrupted
    );

    let newEngines = await this._loadEnginesFromConfig(engines, isReload);
    for (let engine of newEngines) {
      this._addEngineToStore(engine);
    }

    logConsole.debug(
      "_loadEngines: loading",
      this._startupExtensions.size,
      "engines reported by AddonManager startup"
    );
    for (let extension of this._startupExtensions) {
      await this._installExtensionEngine(
        extension,
        [SearchUtils.DEFAULT_TAG],
        true
      );
    }
    this._startupExtensions.clear();

    this._loadEnginesFromCache(cache, true);

    this._loadEnginesMetadataFromCache(cache);

    logConsole.debug("_loadEngines: done");
  },

  /**
   * Loads engines asynchronously (legacy configuration).
   *
   * @param {object} cache
   *   An object representing the search engine cache.
   * @param {boolean} isReload
   *   Set to true if this load is happening during a reload.
   */
  async _loadEnginesLegacy(cache, isReload) {
    logConsole.debug("_loadEnginesLegacy: start");
    let engines = await this._findEnginesLegacy();

    let buildID = Services.appinfo.platformBuildID;
    let rebuildCache =
      gEnvironment.get("RELOAD_ENGINES") ||
      !cache.engines ||
      cache.version != SearchUtils.CACHE_VERSION ||
      cache.locale != Services.locale.requestedLocale ||
      cache.buildID != buildID;

    let enginesCorrupted = false;
    if (!rebuildCache) {
      function notInCacheVisibleEngines(engineName) {
        return !cache.visibleDefaultEngines.includes(engineName);
      }
      // Legacy config.
      rebuildCache =
        cache.visibleDefaultEngines.length !=
          this._visibleDefaultEngines.length ||
        this._visibleDefaultEngines.some(notInCacheVisibleEngines);

      // We don't do a built-in list comparison with distributions because they
      // have a different set of built-ins to that given from the configuration.
      if (
        !rebuildCache &&
        SearchUtils.distroID == "" &&
        cache.engines.filter(e => e._isAppProvided).length !=
          cache.visibleDefaultEngines.length
      ) {
        rebuildCache = true;
        enginesCorrupted = true;
      }
    }

    Services.telemetry.scalarSet(
      "browser.searchinit.engines_cache_corrupted",
      enginesCorrupted
    );

    if (!rebuildCache) {
      logConsole.debug("_loadEnginesLegacy: loading from cache directories");
      this._loadEnginesFromCache(cache);
      if (this._engines.size) {
        logConsole.debug("_loadEnginesLegacy: done using existing cache");
        return;
      }
      logConsole.debug(
        "_loadEnginesLegacy: No valid engines found in cache. Loading engines from disk."
      );
    }

    logConsole.debug(
      "_loadEnginesLegacy: Absent or outdated cache. Loading engines from disk."
    );
    let distDirs = await this._getDistibutionEngineDirectories();
    for (let loadDir of distDirs) {
      let enginesFromDir = await this._loadEnginesFromDir(loadDir);
      enginesFromDir.forEach(this._addEngineToStore, this);
    }

    let engineList = this._enginesToLocales(engines);
    for (let [id, locales] of engineList) {
      await this.ensureBuiltinExtension(id, locales, isReload);
    }

    logConsole.debug(
      "_loadEnginesLegacy: loading",
      this._startupExtensions.size,
      "engines reported by AddonManager startup"
    );
    for (let extension of this._startupExtensions) {
      await this._installExtensionEngine(
        extension,
        [SearchUtils.DEFAULT_TAG],
        true
      );
    }

    logConsole.debug(
      "_loadEnginesLegacy: loading user-installed engines from the obsolete cache"
    );
    this._loadEnginesFromCache(cache, true);

    this._loadEnginesMetadataFromCache(cache);

    logConsole.debug("_loadEnginesLegacy: done using rebuilt cache");
  },

  /**
   * Loads engines as specified by the configuration. We only expect
   * configured engines here, user engines should not be listed.
   *
   * @param {array} engineConfigs
   *   An array of engines configurations based on the schema.
   * @param {boolean} [isReload]
   *   Set to true to indicate a reload is happening.
   * @returns {array.<nsISearchEngine>}
   *   Returns an array of the loaded search engines. This may be
   *   smaller than the original list if not all engines can be loaded.
   */
  async _loadEnginesFromConfig(engineConfigs, isReload = false) {
    logConsole.debug("_loadEnginesFromConfig");
    let engines = [];
    for (let config of engineConfigs) {
      try {
        let engine = await this.makeEngineFromConfig(config, isReload);
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
  },

  /**
   * Get the directories that contain distribution engines.
   *
   * @returns {array}
   *   Returns an array of directories that contain distribution engines.
   */
  async _getDistibutionEngineDirectories() {
    if (gModernConfig) {
      throw new Error(
        "_getDistibutionEngineDirectories is obsolete for modern config."
      );
    }
    // Get the non-empty distribution directories into distDirs...
    let distDirs = [];
    let locations;
    try {
      locations = Services.dirsvc.get(
        NS_APP_DISTRIBUTION_SEARCH_DIR_LIST,
        Ci.nsISimpleEnumerator
      );
    } catch (e) {
      // NS_APP_DISTRIBUTION_SEARCH_DIR_LIST is defined by each app
      // so this throws during unit tests (but not xpcshell tests).
      locations = [];
    }
    for (let dir of locations) {
      let iterator = new OS.File.DirectoryIterator(dir.path, {
        winPattern: "*.xml",
      });
      try {
        // Add dir to distDirs if it contains any files.
        let { done } = await iterator.next();
        if (!done) {
          distDirs.push(dir);
        }
      } catch (ex) {
        if (!(ex instanceof OS.File.Error)) {
          throw ex;
        }
        if (ex.becauseAccessDenied) {
          Cu.reportError(
            "Not loading distribution files because access was denied."
          );
        } else if (!ex.becauseNoSuchFile) {
          throw ex;
        }
      } finally {
        // If there's an issue on close, we can't do anything about it. It could
        // be that reading the iterator never fully opened.
        iterator.close().catch(Cu.reportError);
      }
    }
    return distDirs;
  },

  /**
   * Ensures a built in search WebExtension is installed, installing
   * it if necessary.
   *
   * @param {string} id
   *   The WebExtension ID.
   * @param {Array<string>} locales
   *   An array of locales to use for the WebExtension. If more than
   *   one is specified, different versions of the same engine may
   *   be installed.
   * @param {boolean} [isReload]
   *   is being called from maybeReloadEngines.
   */
  async ensureBuiltinExtension(
    id,
    locales = [SearchUtils.DEFAULT_TAG],
    isReload = false
  ) {
    logConsole.debug("ensureBuiltinExtension: ", id);
    try {
      let policy = await this._getExtensionPolicy(id);
      await this._installExtensionEngine(
        policy.extension,
        locales,
        false,
        isReload
      );
      logConsole.debug("ensureBuiltinExtension: ", id, " installed.");
    } catch (err) {
      Cu.reportError(
        "Failed to install engine: " + err.message + "\n" + err.stack
      );
    }
  },

  /**
   * Converts array of engines into a Map of extensions + the locales
   * of those extensions to install.
   *
   * @param {array} engines
   *   An array of engines
   * @returns {Map} A Map of extension names + locales.
   */
  _enginesToLocales(engines) {
    let engineLocales = new Map();
    for (let engine of engines) {
      let [extensionName, locale] = this._parseEngineName(engine);
      let id = extensionName + "@" + EXT_SIGNING_ADDRESS;
      let locales = engineLocales.get(id) || new Set();
      locales.add(locale);
      engineLocales.set(id, locales);
    }
    return engineLocales;
  },

  /**
   * Parse the engine name into the extension name + locale pair
   * some engines will be exempt (ie yahoo-jp-auctions), can turn
   * this from a whitelist to a blacklist when more engines
   * are multilocale than not.
   *
   * @param {string} engineName
   *   The engine name to parse.
   * @returns {Array} The extension name and the locale to use.
   */
  _parseEngineName(engineName) {
    let [name, locale] = engineName.split(/-(.+)/);

    if (!MULTI_LOCALE_ENGINES.includes(name)) {
      return [engineName, SearchUtils.DEFAULT_TAG];
    }

    if (!locale) {
      locale = SearchUtils.DEFAULT_TAG;
    }
    return [name, locale];
  },

  /**
   * Reloads engines asynchronously, but only when
   * the service has already been initialized.
   */
  async _maybeReloadEngines() {
    if (!gInitialized) {
      if (this._maybeReloadDebounce) {
        logConsole.debug(
          "We're already waiting for init to finish and reload engines after."
        );
        return;
      }
      this._maybeReloadDebounce = true;
      // Schedule a reload to happen at most 10 seconds after initialization of
      // the service finished, during an idle moment.
      this._initObservers.promise.then(() => {
        Services.tm.idleDispatchToMainThread(() => {
          if (!this._maybeReloadDebounce) {
            return;
          }
          delete this._maybeReloadDebounce;
          this._maybeReloadEngines();
        }, 10000);
      });
      logConsole.debug(
        "Post-poning maybeReloadEngines() as we're currently initializing."
      );
      return;
    }
    // There's no point in already reloading the list of engines, when the service
    // hasn't even initialized yet.
    if (!gInitialized) {
      logConsole.debug("Ignoring maybeReloadEngines() as inside init()");
      return;
    }
    logConsole.debug("Running maybeReloadEngines");

    // Handle the legacy code (this returns at the end).
    if (!gModernConfig) {
      // Capture the current engine state, in case we need to notify below.
      const prevCurrentEngine = this._currentEngine;
      const prevPrivateEngine = this._currentPrivateEngine;
      // Clear cached objects as they may get replaced.
      this._currentEngine = null;
      this._currentPrivateEngine = null;
      this._dontSetUseDBForOrder = true;
      // Ensure we generate a new __sortedEngines list instead
      // of appending new engines to the end and fixing the
      // engine order.
      this.__sortedEngines = null;
      await this._loadEngines(await this._cache.get(), true);

      // If the defaultEngine has changed between the previous load and this one,
      // dispatch the appropriate notifications.
      if (prevCurrentEngine && this.defaultEngine !== prevCurrentEngine) {
        SearchUtils.notifyAction(
          this._currentEngine,
          SearchUtils.MODIFIED_TYPE.DEFAULT
        );
        // If we've not got a separate private active, notify update of the
        // private so that the UI updates correctly.
        if (!this._separatePrivateDefault) {
          SearchUtils.notifyAction(
            this._currentEngine,
            SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
          );
        }
      }
      if (
        this._separatePrivateDefault &&
        prevPrivateEngine &&
        this.defaultPrivateEngine !== prevPrivateEngine
      ) {
        SearchUtils.notifyAction(
          this._currentPrivateEngine,
          SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
        );
      }
      Services.obs.notifyObservers(
        null,
        SearchUtils.TOPIC_SEARCH_SERVICE,
        "engines-reloaded"
      );
      logConsole.debug("maybeReloadEngines complete");
      return;
    }

    // Capture the current engine state, in case we need to notify below.
    const prevCurrentEngine = this._currentEngine;
    const prevPrivateEngine = this._currentPrivateEngine;

    // Ensure that we don't set the UseDBForOrder flag whilst we're doing this.
    // This isn't a user action, so we shouldn't be switching it.
    this._dontSetUseDBForOrder = true;

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
      if (index == -1) {
        enginesToRemove.push(engine);
      } else {
        // This is an existing engine that we should update (we don't know if
        // the configuration for this engine has changed or not).
        let policy = await this._getExtensionPolicy(engine._extensionID);

        let manifest = policy.extension.manifest;
        let locale = engine._locale || SearchUtils.DEFAULT_TAG;
        if (locale != SearchUtils.DEFAULT_TAG) {
          manifest = await policy.extension.getLocalizedManifest(locale);
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
    }

    // Any remaining configuration engines are ones that we need to add.
    for (let engine of configEngines) {
      try {
        let newEngine = await this.makeEngineFromConfig(engine, false);
        this._addEngineToStore(newEngine, true);
      } catch (ex) {
        logConsole.warn(
          `Could not load engine ${
            "webExtension" in engine ? engine.webExtension.id : "unknown"
          }: ${ex}`
        );
      }
    }

    // Now set the sort out the default engines and notify as appropriate.
    this._currentEngine = null;
    this._currentPrivateEngine = null;

    this._setDefaultAndOrdersFromSelector(
      originalConfigEngines,
      privateDefault
    );

    // If the defaultEngine has changed between the previous load and this one,
    // dispatch the appropriate notifications.
    if (prevCurrentEngine && this.defaultEngine !== prevCurrentEngine) {
      SearchUtils.notifyAction(
        this._currentEngine,
        SearchUtils.MODIFIED_TYPE.DEFAULT
      );
      // If we've not got a separate private active, notify update of the
      // private so that the UI updates correctly.
      if (!this._separatePrivateDefault) {
        SearchUtils.notifyAction(
          this._currentEngine,
          SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
        );
      }
    }
    if (
      this._separatePrivateDefault &&
      prevPrivateEngine &&
      this.defaultPrivateEngine !== prevPrivateEngine
    ) {
      SearchUtils.notifyAction(
        this._currentPrivateEngine,
        SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
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
          this._internalRemoveEngine(engine);

          let addon = await AddonManager.getAddonByID(engine._extensionID);
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
        this._internalRemoveEngine(engine);
      }
      SearchUtils.notifyAction(engine, SearchUtils.MODIFIED_TYPE.REMOVED);
    }

    this._dontSetUseDBForOrder = false;
    // Clear out the sorted engines cache, so that we re-sort it if necessary.
    this.__sortedEngines = null;
    Services.obs.notifyObservers(
      null,
      SearchUtils.TOPIC_SEARCH_SERVICE,
      "engines-reloaded"
    );
    logConsole.debug("maybeReloadEngines complete");
  },

  _reInit(origin) {
    logConsole.debug("_reInit");
    // Re-entrance guard, because we're using an async lambda below.
    if (gReinitializing) {
      logConsole.debug("_reInit: already re-initializing, bailing out.");
      return;
    }
    gReinitializing = true;

    // Start by clearing the initialized state, so we don't abort early.
    gInitialized = false;

    // Tests want to know when we've started.
    Services.obs.notifyObservers(
      null,
      SearchUtils.TOPIC_SEARCH_SERVICE,
      "reinit-started"
    );

    (async () => {
      try {
        // This echos what we do in init() and is only here to support
        // dynamically switching to and from modern config.
        if (gModernConfig && !this._engineSelector) {
          this._engineSelector = new SearchEngineSelector(
            this._handleConfigurationUpdated.bind(this)
          );
        }

        this._initObservers = PromiseUtils.defer();

        // Clear the engines, too, so we don't stick with the stale ones.
        this._resetLocalData();

        // Tests that want to force a synchronous re-initialization need to
        // be notified when we are done uninitializing.
        Services.obs.notifyObservers(
          null,
          SearchUtils.TOPIC_SEARCH_SERVICE,
          "uninit-complete"
        );

        let cache = await this._cache.get(origin);
        // The init flow is not going to block on a fetch from an external service,
        // but we're kicking it off as soon as possible to prevent UI flickering as
        // much as possible.
        this._ensureKnownRegionPromise = ensureKnownRegion(this)
          .catch(ex =>
            logConsole.error("_reInit: failure determining region:", ex)
          )
          .finally(() => (this._ensureKnownRegionPromise = null));

        await this._loadEngines(cache);

        // If we've got this far, but the application is now shutting down,
        // then we need to abandon any further work, especially not writing
        // the cache. We do this, because the add-on manager has also
        // started shutting down and as a result, we might have an incomplete
        // picture of the installed search engines. Writing the cache at
        // this stage would potentially mean the user would loose their engine
        // data.
        // We will however, rebuild the cache on next start up if we detect
        // it is necessary.
        if (Services.startup.shuttingDown) {
          logConsole.warn("_reInit: abandoning reInit due to shutting down");
          this._initObservers.reject(Cr.NS_ERROR_ABORT);
          return;
        }

        // Typically we'll re-init as a result of a pref observer,
        // so signal to 'callers' that we're done.
        gInitialized = true;
        this._initObservers.resolve();
        Services.obs.notifyObservers(
          null,
          SearchUtils.TOPIC_SEARCH_SERVICE,
          "init-complete"
        );
      } catch (err) {
        logConsole.error("Reinit failed:", err);
        Services.obs.notifyObservers(
          null,
          SearchUtils.TOPIC_SEARCH_SERVICE,
          "reinit-failed"
        );
      } finally {
        gReinitializing = false;
        Services.obs.notifyObservers(
          null,
          SearchUtils.TOPIC_SEARCH_SERVICE,
          "reinit-complete"
        );
      }
    })();
  },

  /**
   * Reset SearchService data.
   */
  reset() {
    gInitialized = false;
    this._resetLocalData();
    this._initObservers = PromiseUtils.defer();
    this._initStarted = null;
    this._startupExtensions = new Set();
  },

  _addEngineToStore(engine, skipDuplicateCheck = false) {
    if (this._engineMatchesIgnoreLists(engine)) {
      logConsole.debug("_addEngineToStore: Ignoring engine");
      return;
    }

    logConsole.debug("_addEngineToStore: Adding engine:", engine.name);

    // See if there is an existing engine with the same name. However, if this
    // engine is updating another engine, it's allowed to have the same name.
    var hasSameNameAsUpdate =
      engine._engineToUpdate && engine.name == engine._engineToUpdate.name;
    if (
      !skipDuplicateCheck &&
      this._engines.has(engine.name) &&
      !hasSameNameAsUpdate
    ) {
      logConsole.debug("_addEngineToStore: Duplicate engine found, aborting!");
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
      SearchUtils.notifyAction(engine, SearchUtils.MODIFIED_TYPE.CHANGED);
    } else {
      // Not an update, just add the new engine.
      this._engines.set(engine.name, engine);
      // Only add the engine to the list of sorted engines if the initial list
      // has already been built (i.e. if this.__sortedEngines is non-null). If
      // it hasn't, we're loading engines from disk and the sorted engine list
      // will be built once we need it.
      if (this.__sortedEngines && !this._dontSetUseDBForOrder) {
        this.__sortedEngines.push(engine);
        this._saveSortedEngineList();
      }
      SearchUtils.notifyAction(engine, SearchUtils.MODIFIED_TYPE.ADDED);
    }

    // Let the engine know it can start notifying new updates.
    engine._engineAddedToStore = true;

    if (engine._hasUpdates) {
      // Schedule the engine's next update, if it isn't already.
      if (!engine.getAttr("updateexpir")) {
        engineUpdateService.scheduleNextUpdate(engine);
      }
    }
  },

  _loadEnginesMetadataFromCache(cache) {
    if (!cache.engines) {
      return;
    }

    for (let engine of cache.engines) {
      let name = engine._name;
      if (this._engines.has(name)) {
        logConsole.debug(
          "_loadEnginesMetadataFromCache, transfering metadata for",
          name
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
  },

  // TODO: Remove the second argument when we remove gModernConfig.
  // (the function should then always skip-builtin engines).
  _loadEnginesFromCache(cache, skipAppProvided) {
    if (!cache.engines) {
      return;
    }

    logConsole.debug(
      "_loadEnginesFromCache: Loading",
      cache.engines.length,
      "engines from cache"
    );

    let skippedEngines = 0;
    for (let engine of cache.engines) {
      // We renamed isBuiltin to isAppProvided in 1631898,
      // keep checking isBuiltin for older caches.
      if (skipAppProvided && (engine._isAppProvided || engine._isBuiltin)) {
        ++skippedEngines;
        continue;
      }

      this._loadEngineFromCache(engine);
    }

    if (skippedEngines) {
      logConsole.debug(
        "_loadEnginesFromCache: skipped",
        skippedEngines,
        "built-in engines."
      );
    }
  },

  _loadEngineFromCache(json) {
    try {
      let engine = new SearchEngine({
        shortName: json._shortName,
        // We renamed isBuiltin to isAppProvided in 1631898,
        // keep checking isBuiltin for older caches.
        isAppProvided: !!json._isAppProvided || !!json._isBuiltin,
        loadPath: json._loadPath,
      });
      engine._initWithJSON(json);
      this._addEngineToStore(engine);
    } catch (ex) {
      logConsole.error("Failed to load", json._name, "from cache:", ex);
      logConsole.debug("Engine JSON:", json.toSource());
    }
  },

  /**
   * Loads engines from a given directory asynchronously.
   *
   * @param {OS.File}
   *   dir the directory.
   * @returns {Array<SearchEngine>}
   *   An array of search engines that were found.
   */
  async _loadEnginesFromDir(dir) {
    logConsole.debug(
      "_loadEnginesFromDir: Searching in",
      dir.path,
      "for search engines."
    );

    let iterator = new OS.File.DirectoryIterator(dir.path);

    let osfiles = await iterator.nextBatch();
    iterator.close();

    let engines = [];
    for (let osfile of osfiles) {
      if (osfile.isDir || osfile.isSymLink) {
        continue;
      }

      let fileInfo = await OS.File.stat(osfile.path);
      if (fileInfo.size == 0) {
        continue;
      }

      let parts = osfile.path.split(".");
      if (parts.length <= 1 || parts.pop().toLowerCase() != "xml") {
        // Not an engine
        continue;
      }

      let addedEngine = null;
      try {
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        file.initWithPath(osfile.path);
        addedEngine = new OpenSearchEngine({
          fileURI: file,
          isAppProvided: true,
        });
        await addedEngine._initFromFile(file);
        engines.push(addedEngine);
      } catch (ex) {
        logConsole.error("Failed to load", osfile.path, ex);
      }
    }
    return engines;
  },

  async _fetchEngineSelectorEngines() {
    let locale = Services.locale.appLocaleAsBCP47;
    let region = Region.home || "default";

    let channel = AppConstants.MOZ_APP_VERSION_DISPLAY.endsWith("esr")
      ? "esr"
      : AppConstants.MOZ_UPDATE_CHANNEL;
    let {
      engines,
      privateDefault,
    } = await this._engineSelector.fetchEngineConfiguration(
      locale,
      region,
      channel,
      SearchUtils.distroID
    );

    for (let e of engines) {
      if (!e.webExtension) {
        e.webExtension = {};
      }
      e.webExtension.locale = e.webExtension?.locale ?? SearchUtils.DEFAULT_TAG;
    }

    return { engines, privateDefault };
  },

  _setDefaultAndOrdersFromSelector(engines, privateDefault) {
    const defaultEngine = engines[0];
    this._searchDefault = {
      id: defaultEngine.webExtension.id,
      locale: defaultEngine.webExtension.locale,
    };
    this._searchOrder = engines.map(e => {
      return {
        id: e.webExtension.id,
        locale: e.webExtension.locale,
      };
    });
    if (privateDefault) {
      this._searchPrivateDefault = {
        id: privateDefault.webExtension.id,
        locale: privateDefault.webExtension.locale,
      };
    }
  },

  /**
   * Loads the list of engines from list.json
   *
   * @returns {Array<string>}
   *   Returns an array of engine names.
   */
  async _findEnginesLegacy() {
    logConsole.debug("_findEnginesLegacy: looking for engines in list.json");

    let chan = SearchUtils.makeChannel(this._listJSONURL);
    if (!chan) {
      logConsole.debug(
        "_findEnginesLegacy:",
        this._listJSONURL,
        "isn't registered"
      );
      return [];
    }

    // Read list.json to find the engines we need to load.
    let request = new XMLHttpRequest();
    request.overrideMimeType("text/plain");
    let list = await new Promise(resolve => {
      request.onload = function(event) {
        resolve(event.target.responseText);
      };
      request.onerror = function(event) {
        logConsole.debug(
          "_findEnginesLegacy: failed to read",
          this._listJSONURL
        );
        resolve();
      };
      request.open("GET", Services.io.newURI(this._listJSONURL).spec, true);
      request.send();
    });

    return this._parseListJSON(list);
  },

  /**
   * @deprecated Unused in the modern config.
   *
   * @param {string} list
   *   The engine list in json format.
   * @returns {Array<string>}
   *   Returns an array of engine names.
   */
  async _parseListJSON(list) {
    let json;
    try {
      json = JSON.parse(list);
    } catch (ex) {
      logConsole.error("parseListJSON: Failed to parse list.json:", ex);
      return [];
    }

    let searchRegion = Region.home;

    let searchSettings;
    let locale = Services.locale.appLocaleAsBCP47;
    if ("locales" in json && locale in json.locales) {
      searchSettings = json.locales[locale];
    } else {
      // No locales were found, so use the JSON as is.
      // It should have a default section.
      if (!("default" in json)) {
        Cu.reportError("parseListJSON: Missing default in list.json");
        dump("parseListJSON: Missing default in list.json\n");
        return [];
      }
      searchSettings = json;
    }

    // Check if we have a useable region specific list of visible default engines.
    // This will only be set if we got the list from the Mozilla search server;
    // it will not be set for distributions.
    let engineNames;
    let visibleDefaultEngines = this._cache.getVerifiedAttribute(
      "visibleDefaultEngines"
    );
    if (visibleDefaultEngines) {
      let jarNames = new Set();
      for (let region in searchSettings) {
        // Artifact builds use the full list.json which parses
        // slightly differently
        if (!("visibleDefaultEngines" in searchSettings[region])) {
          continue;
        }
        for (let engine of searchSettings[region].visibleDefaultEngines) {
          jarNames.add(engine);
        }
        if ("regionOverrides" in json && searchRegion in json.regionOverrides) {
          for (let engine in json.regionOverrides[searchRegion]) {
            jarNames.add(json.regionOverrides[searchRegion][engine]);
          }
        }
      }

      engineNames = visibleDefaultEngines.split(",");
      // absearch can't be modified to use the new engine names.
      // Convert them here.
      engineNames = convertGoogleEngines(engineNames);

      for (let engineName of engineNames) {
        // If all engineName values are part of jarNames,
        // then we can use the region specific list, otherwise ignore it.
        // The visibleDefaultEngines string containing the name of an engine we
        // don't ship indicates the server is misconfigured to answer requests
        // from the specific Firefox version we are running, so ignoring the
        // value altogether is safer.
        if (!jarNames.has(engineName)) {
          logConsole.debug(
            "_parseListJSON: ignoring visibleDefaultEngines value because",
            engineName,
            "is not in the jar engines we have found"
          );
          engineNames = null;
          break;
        }
      }
    }

    // Fallback to building a list based on the regions in the JSON
    if (!engineNames || !engineNames.length) {
      if (
        searchRegion &&
        searchRegion in searchSettings &&
        "visibleDefaultEngines" in searchSettings[searchRegion]
      ) {
        engineNames = searchSettings[searchRegion].visibleDefaultEngines;
      } else {
        engineNames = searchSettings.default.visibleDefaultEngines;
      }
    }

    // Remove any engine names that are supposed to be ignored.
    // This pref is only allowed in a partner distribution.
    let branch = Services.prefs.getDefaultBranch(
      SearchUtils.BROWSER_SEARCH_PREF
    );
    if (
      SearchUtils.isPartnerBuild() &&
      branch.getPrefType("ignoredJAREngines") == branch.PREF_STRING
    ) {
      let ignoredJAREngines = branch
        .getCharPref("ignoredJAREngines")
        .split(",");
      let filteredEngineNames = engineNames.filter(
        e => !ignoredJAREngines.includes(e)
      );
      // Don't allow all engines to be hidden
      if (filteredEngineNames.length) {
        engineNames = filteredEngineNames;
      }
    }

    if ("regionOverrides" in json && searchRegion in json.regionOverrides) {
      for (let engine in json.regionOverrides[searchRegion]) {
        let index = engineNames.indexOf(engine);
        if (index > -1) {
          engineNames[index] = json.regionOverrides[searchRegion][engine];
        }
      }
    }

    // ESR uses different codes. Convert them here.
    if (AppConstants.MOZ_APP_VERSION_DISPLAY.endsWith("esr")) {
      let esrOverrides = {
        "google-b-d": "google-b-e",
        "google-b-1-d": "google-b-1-e",
      };

      for (let engine in esrOverrides) {
        let index = engineNames.indexOf(engine);
        if (index > -1) {
          engineNames[index] = esrOverrides[engine];
        }
      }
    }

    // Store this so that it can be used while writing the cache file.
    this._visibleDefaultEngines = engineNames;

    if (
      searchRegion &&
      searchRegion in searchSettings &&
      "searchDefault" in searchSettings[searchRegion]
    ) {
      this._searchDefault = searchSettings[searchRegion].searchDefault;
    } else if ("searchDefault" in searchSettings.default) {
      this._searchDefault = searchSettings.default.searchDefault;
    } else {
      this._searchDefault = json.default.searchDefault;
    }

    if (!this._searchDefault) {
      Cu.reportError("parseListJSON: No searchDefault");
    }

    if (
      searchRegion &&
      searchRegion in searchSettings &&
      "searchPrivateDefault" in searchSettings[searchRegion]
    ) {
      this._searchPrivateDefault =
        searchSettings[searchRegion].searchPrivateDefault;
    } else if ("searchPrivateDefault" in searchSettings.default) {
      this._searchPrivateDefault = searchSettings.default.searchPrivateDefault;
    } else {
      this._searchPrivateDefault = json.default.searchPrivateDefault;
    }

    if (!this._searchPrivateDefault) {
      // Fallback to the normal default if nothing is specified for private mode.
      this._searchPrivateDefault = this._searchDefault;
    }

    if (
      searchRegion &&
      searchRegion in searchSettings &&
      "searchOrder" in searchSettings[searchRegion]
    ) {
      this._searchOrder = searchSettings[searchRegion].searchOrder;
    } else if ("searchOrder" in searchSettings.default) {
      this._searchOrder = searchSettings.default.searchOrder;
    } else if ("searchOrder" in json.default) {
      this._searchOrder = json.default.searchOrder;
    }
    return [...engineNames];
  },

  _saveSortedEngineList() {
    logConsole.debug("_saveSortedEngineList");

    // Set the useDB pref to indicate that from now on we should use the order
    // information stored in the database.
    Services.prefs.setBoolPref(
      SearchUtils.BROWSER_SEARCH_PREF + "useDBForOrder",
      true
    );

    var engines = this._getSortedEngines(true);

    for (var i = 0; i < engines.length; ++i) {
      engines[i].setAttr("order", i + 1);
    }
  },

  _buildSortedEngineList() {
    // We must initialise __sortedEngines here to avoid infinite recursion
    // in the case of tests which don't define a default search engine.
    // If there's no default defined, then we revert to the first item in the
    // sorted list, but we can't do that if we don't have a list.
    this.__sortedEngines = [];

    // If the user has specified a custom engine order, read the order
    // information from the metadata instead of the default prefs.
    if (
      Services.prefs.getBoolPref(
        SearchUtils.BROWSER_SEARCH_PREF + "useDBForOrder",
        false
      )
    ) {
      logConsole.debug("_buildSortedEngineList: using db for order");
      let addedEngines = {};

      // Flag to keep track of whether or not we need to call _saveSortedEngineList.
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
          // We need to call _saveSortedEngineList so this gets sorted out.
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
        this._saveSortedEngineList();
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
    logConsole.debug("_buildSortedEngineList: using default orders");

    return (this.__sortedEngines = this._sortEnginesByDefaults(
      Array.from(this._engines.values())
    ));
  },

  /**
   * Sorts engines by the default settings (prefs, configuration values).
   *
   * @param {Array} engines
   *   An array of engine objects to sort.
   * @returns {Array}
   *   The sorted array of engine objects.
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

    if (!gModernConfig && SearchUtils.distroID) {
      try {
        var extras = Services.prefs.getChildList(
          SearchUtils.BROWSER_SEARCH_PREF + "order.extra."
        );

        // getChildList doesn't guarantee the order of the prefs, but we
        // expect them alphabetical, so sort them here.
        extras.sort();

        for (const prefName of extras) {
          const engineName = Services.prefs.getCharPref(prefName);
          maybeAddEngineToSort(engines.find(e => e.name == engineName));
        }
      } catch (e) {}

      let i = 0;
      while (++i) {
        const prefName = `${SearchUtils.BROWSER_SEARCH_PREF}order.${i}`;
        const engineName = getLocalizedPref(prefName);
        if (!engineName) {
          break;
        }

        maybeAddEngineToSort(engines.find(e => e.name == engineName));
      }
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

    if (gModernConfig) {
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
    } else {
      for (let engineName of this._searchOrder) {
        maybeAddEngineToSort(engines.find(e => e.name == engineName));
      }

      remainingEngines = [];

      for (let engine of engines.values()) {
        if (!addedEngines.has(engine.name)) {
          remainingEngines.push(engine);
        }
      }

      remainingEngines.sort((a, b) => {
        return collator.compare(a.name, b.name);
      });
    }
    return [...sortedEngines, ...remainingEngines];
  },

  /**
   * Get a sorted array of engines.
   *
   * @param {boolean} withHidden
   *   True if hidden plugins should be included in the result.
   * @returns {Array<SearchEngine>}
   *   The sorted array.
   */
  _getSortedEngines(withHidden) {
    if (withHidden) {
      return this._sortedEngines;
    }

    return this._sortedEngines.filter(function(engine) {
      return !engine.hidden;
    });
  },

  // nsISearchService
  async init() {
    logConsole.debug("init");
    if (this._initStarted) {
      return this._initObservers.promise;
    }

    TelemetryStopwatch.start("SEARCH_SERVICE_INIT_MS");
    this._initStarted = true;
    try {
      // Complete initialization by calling asynchronous initializer.
      await this._init();
      TelemetryStopwatch.finish("SEARCH_SERVICE_INIT_MS");
    } catch (ex) {
      Services.telemetry.scalarSet(
        "browser.searchinit.init_result_status_code",
        // Scalar is a string due to bug 1651210 when the scalar was created.
        ex.result?.toString(10)
      );
      TelemetryStopwatch.cancel("SEARCH_SERVICE_INIT_MS");
      this._initObservers.reject(ex.result);
      throw ex;
    }
    Services.telemetry.scalarSet(
      "browser.searchinit.init_result_status_code",
      // Scalar is a string due to bug 1651210 when the scalar was created.
      this._initRV?.toString(10)
    );

    if (!Components.isSuccessCode(this._initRV)) {
      throw Components.Exception(
        "SearchService initialization failed",
        this._initRV
      );
    }
    return this._initRV;
  },

  get isInitialized() {
    return gInitialized;
  },

  // reInit is currently only exposed for testing purposes
  async reInit() {
    return this._reInit("test");
  },

  async getEngines() {
    await this.init();
    logConsole.debug("getEngines: getting all engines");
    return this._getSortedEngines(true);
  },

  async getVisibleEngines() {
    await this.init(true);
    logConsole.debug("getVisibleEngines: getting all visible engines");
    return this._getSortedEngines(false);
  },

  async getDefaultEngines() {
    await this.init();

    return this._sortEnginesByDefaults(
      this._sortedEngines.filter(e => e.isAppProvided)
    );
  },

  async getEnginesByExtensionID(extensionID) {
    await this.init();
    logConsole.debug("getEngines: getting all engines for", extensionID);
    var engines = this._getSortedEngines(true).filter(function(engine) {
      return engine._extensionID == extensionID;
    });
    return engines;
  },

  /**
   * Returns the engine associated with the name.
   *
   * @param {string} engineName
   *   The name of the engine.
   * @returns {SearchEngine}
   *   The associated engine if found, null otherwise.
   */
  getEngineByName(engineName) {
    this._ensureInitialized();
    return this._engines.get(engineName) || null;
  },

  getEngineByAlias(alias) {
    this._ensureInitialized();
    for (var engine of this._engines.values()) {
      if (
        engine &&
        (engine.alias == alias || engine._internalAliases.includes(alias))
      ) {
        return engine;
      }
    }
    return null;
  },

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
  _getEngineByWebExtensionDetails(details) {
    for (const engine of this._engines.values()) {
      if (
        engine._extensionID == details.id &&
        engine._locale == details.locale
      ) {
        return engine;
      }
    }
    return null;
  },

  /**
   * Adds a search engine that is specified from enterprise policies.
   *
   * @param {object} details
   *   An object that simulates the manifest object from a WebExtension. See
   *   the idl for more details.
   */
  async addPolicyEngine(details) {
    await this._createAndAddEngine({
      extensionID: "set-via-policy",
      extensionBaseURI: "",
      isAppProvided: false,
      manifest: details,
    });
  },

  /**
   * Adds an engine with specific details, only used for tests and should
   * be considered obsolete, see bug 1649186.
   *
   * @param {string} name
   *   The name of the engine to add.
   * @param {object} details
   *   The details of the engine to add.
   */
  async addEngineWithDetails(name, details) {
    let manifest = {
      description: details.description,
      iconURL: details.iconURL,
      chrome_settings_overrides: {
        search_provider: {
          name,
          encoding: `windows-1252`,
          search_url: encodeURI(details.template),
          keyword: details.alias,
          search_url_get_params: details.searchGetParams,
          search_url_post_params: details.postData || details.searchPostParams,
          suggest_url: details.suggestURL,
        },
      },
    };
    return this._createAndAddEngine({
      extensionID: details.extensionID ?? `${name}@test.engine`,
      extensionBaseURI: "",
      isAppProvided: false,
      manifest,
    });
  },

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
   * @param {boolean} [options.isReload]
   *   Set to true if we're in a reload cycle.
   */
  async _createAndAddEngine({
    extensionID,
    extensionBaseURI,
    isAppProvided,
    manifest,
    locale = SearchUtils.DEFAULT_TAG,
    initEngine = false,
    isReload = false,
  }) {
    if (!extensionID) {
      throw Components.Exception(
        "Empty extensionID passed to _createAndAddEngine!",
        Cr.NS_ERROR_INVALID_ARG
      );
    }
    let searchProvider = manifest.chrome_settings_overrides.search_provider;
    let name = searchProvider.name.trim();
    logConsole.debug("_createAndAddEngine: Adding", name);
    let isCurrent = false;

    // We install search extensions during the init phase, both built in
    // web extensions freshly installed (via addEnginesFromExtension) or
    // user installed extensions being reenabled calling this directly.
    if (!gInitialized && !isAppProvided && !initEngine) {
      await this.init();
    }
    let existingEngine = this._engines.get(name);
    // In the modern configuration, distributions are app-provided engines,
    // so we don't need this separate check.
    if (
      !gModernConfig &&
      existingEngine &&
      existingEngine._loadPath.startsWith("[distribution]")
    ) {
      throw Components.Exception(
        "Not loading engine due to having a distribution engine with the same name",
        Cr.NS_ERROR_FILE_ALREADY_EXISTS
      );
    }
    if (!isReload && existingEngine) {
      if (
        extensionID &&
        existingEngine._loadPath.startsWith(
          `jar:[profile]/extensions/${extensionID}`
        )
      ) {
        // This is a legacy extension engine that needs to be migrated to WebExtensions.
        isCurrent = this.defaultEngine == existingEngine;
        await this.removeEngine(existingEngine);
      } else {
        throw Components.Exception(
          "An engine with that name already exists!",
          Cr.NS_ERROR_FILE_ALREADY_EXISTS
        );
      }
    }

    let newEngine = new SearchEngine({
      name,
      isAppProvided,
      loadPath: `[other]addEngineWithDetails:${extensionID}`,
    });
    newEngine._initFromManifest(
      extensionID,
      extensionBaseURI,
      manifest,
      locale
    );
    if (isReload && this._engines.has(newEngine.name)) {
      newEngine._engineToUpdate = this._engines.get(newEngine.name);
    }

    this._addEngineToStore(newEngine);
    if (isCurrent) {
      this.defaultEngine = newEngine;
    }
    return newEngine;
  },

  /**
   * Called from the AddonManager when it either installs a new
   * extension containing a search engine definition or an upgrade
   * to an existing one.
   *
   * @param {object} extension
   *   An Extension object containing data about the extension.
   */
  async addEnginesFromExtension(extension) {
    logConsole.debug("addEnginesFromExtension: " + extension.id);
    // Treat add-on upgrade and downgrades the same - either way, the search
    // engine gets updated, not added. Generally, we don't expect a downgrade,
    // but just in case...
    if (
      extension.startupReason == "ADDON_UPGRADE" ||
      extension.startupReason == "ADDON_DOWNGRADE"
    ) {
      return this._upgradeExtensionEngine(extension);
    }

    if (extension.isAppProvided) {
      let inConfig = this._searchOrder.filter(el => el.id == extension.id);
      if (gInitialized && inConfig.length) {
        return this._installExtensionEngine(
          extension,
          inConfig.map(el => el.locale)
        );
      }
      logConsole.debug("addEnginesFromExtension: Ignoring builtIn engine.");
      return [];
    }

    // If we havent started SearchService yet, store this extension
    // to install in SearchService.init().
    if (!gInitialized) {
      this._startupExtensions.add(extension);
      return [];
    }

    return this._installExtensionEngine(extension, [SearchUtils.DEFAULT_TAG]);
  },

  /**
   * Called when we see an upgrade to an existing search extension.
   *
   * @param {object} extension
   *   An Extension object containing data about the extension.
   */
  async _upgradeExtensionEngine(extension) {
    if (!gModernConfig) {
      let extensionEngines = await this.getEnginesByExtensionID(extension.id);

      for (let engine of extensionEngines) {
        let manifest = extension.manifest;
        let locale = engine._locale || SearchUtils.DEFAULT_TAG;
        if (locale != SearchUtils.DEFAULT_TAG) {
          manifest = await extension.getLocalizedManifest(locale);
        }
        engine._updateFromManifest(
          extension.id,
          extension.baseURI,
          manifest,
          locale
        );
      }
      return extensionEngines;
    }

    let { engines } = await this._fetchEngineSelectorEngines();
    let extensionEngines = await this.getEnginesByExtensionID(extension.id);

    for (let engine of extensionEngines) {
      let manifest = extension.manifest;
      let locale = engine._locale || SearchUtils.DEFAULT_TAG;
      if (locale != SearchUtils.DEFAULT_TAG) {
        manifest = await extension.getLocalizedManifest(locale);
      }
      let configuration =
        engines.find(
          e =>
            e.webExtension.id == extension.id && e.webExtension.locale == locale
        ) ?? {};
      engine._updateFromManifest(
        extension.id,
        extension.baseURI,
        manifest,
        locale,
        configuration
      );
    }
    return extensionEngines;
  },

  /**
   * Create an engine object from the search configuration details.
   *
   * @param {object} config
   *   The configuration object that defines the details of the engine
   *   webExtensionId etc.
   * @param {boolean} isReload (optional)
   *   Is being called as part of maybeReloadEngines.
   * @returns {nsISearchEngine}
   *   Returns the search engine object.
   */
  async makeEngineFromConfig(config, isReload = false) {
    logConsole.debug("makeEngineFromConfig:", config);
    let policy = await this._getExtensionPolicy(config.webExtension.id);
    let locale =
      "locale" in config.webExtension
        ? config.webExtension.locale
        : SearchUtils.DEFAULT_TAG;

    let manifest = policy.extension.manifest;
    if (locale != SearchUtils.DEFAULT_TAG) {
      manifest = await policy.extension.getLocalizedManifest(locale);
    }

    let engine = new SearchEngine({
      // No need to sanitize the name, as shortName uses the WebExtension id
      // which should already be sanitized.
      // TODO: Setting the name here is a little incorrect, since it is
      // setting the short name which gets overridden by _initFromManifest.
      // When we split out WebExtensions into their own object (bug 1650761)
      // we should look at simplifying this.
      name: manifest.chrome_settings_overrides.search_provider.name.trim(),
      // shortName: engineParams.shortName,
      isAppProvided: policy.extension.isAppProvided,
      loadPath: `[other]addEngineWithDetails:${policy.extension.id}`,
    });
    engine._initFromManifest(
      policy.extension.id,
      policy.extension.baseURI,
      manifest,
      locale,
      config
    );
    if (isReload && this._engines.has(engine.name)) {
      engine._engineToUpdate = this._engines.get(engine.name);
    }
    return engine;
  },

  async _installExtensionEngine(
    extension,
    locales,
    initEngine = false,
    isReload = false
  ) {
    logConsole.debug("installExtensionEngine:", extension.id);

    let installLocale = async locale => {
      let manifest =
        locale == SearchUtils.DEFAULT_TAG
          ? extension.manifest
          : await extension.getLocalizedManifest(locale);
      return this._addEngineForManifest(
        extension,
        manifest,
        locale,
        initEngine,
        isReload
      );
    };

    let engines = [];
    for (let locale of locales) {
      logConsole.debug(
        "addEnginesFromExtension: installing:",
        extension.id,
        ":",
        locale
      );
      engines.push(await installLocale(locale));
    }
    return engines;
  },

  async _addEngineForManifest(
    extension,
    manifest,
    locale = SearchUtils.DEFAULT_TAG,
    initEngine = false,
    isReload
  ) {
    // If we're in the startup cycle, and we've already loaded this engine,
    // then we use the existing one rather than trying to start from scratch.
    // This also avoids console errors.
    if (extension.startupReason == "APP_STARTUP") {
      let engine = this._getEngineByWebExtensionDetails({
        id: extension.id,
        locale,
      });
      if (engine) {
        logConsole.debug(
          "Engine already loaded via cache, skipping due to APP_STARTUP:",
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
      isReload,
    });
  },

  async addOpenSearchEngine(engineURL, iconURL) {
    logConsole.debug("addEngine: Adding", engineURL);
    await this.init();
    let errCode;
    try {
      var engine = new OpenSearchEngine({
        uri: engineURL,
        isAppProvided: false,
      });
      engine._setIcon(iconURL, false);
      errCode = await new Promise(resolve => {
        engine._installCallback = function(errorCode) {
          resolve(errorCode);
          // Clear the reference to the callback now that it's been invoked.
          engine._installCallback = null;
        };
        engine._initFromURIAndLoad(engineURL);
      });
      if (errCode) {
        throw errCode;
      }
    } catch (ex) {
      // Drop the reference to the callback, if set
      if (engine) {
        engine._installCallback = null;
      }
      throw Components.Exception(
        "addEngine: Error adding engine:\n" + ex,
        errCode || Cr.NS_ERROR_FAILURE
      );
    }
    return engine;
  },

  async removeWebExtensionEngine(id) {
    logConsole.debug("removeWebExtensionEngine:", id);
    for (let engine of await this.getEnginesByExtensionID(id)) {
      await this.removeEngine(engine);
    }
  },

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
      this._currentEngine = null;
    }

    // Bug 1575649 - We can't just check the default private engine here when
    // we're not using separate, as that re-checks the normal default, and
    // triggers update of the default search engine, which messes up various
    // tests. Really, removeEngine should always commit to updating any
    // changed defaults.
    if (
      this._separatePrivateDefault &&
      engineToRemove == this.defaultPrivateEngine
    ) {
      this._currentPrivateEngine = null;
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
      this._internalRemoveEngine(engineToRemove);

      // Since we removed an engine, we may need to update the preferences.
      if (!this._dontSetUseDBForOrder) {
        this._saveSortedEngineList();
      }
    }
    SearchUtils.notifyAction(engineToRemove, SearchUtils.MODIFIED_TYPE.REMOVED);
  },

  _internalRemoveEngine(engine) {
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
  },

  async moveEngine(engine, newIndex) {
    await this.init();
    if (newIndex > this._sortedEngines.length || newIndex < 0) {
      throw Components.Exception("moveEngine: Index out of bounds!");
    }
    if (
      !(engine instanceof Ci.nsISearchEngine) &&
      !(engine instanceof SearchEngine)
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

    var currentIndex = this._sortedEngines.indexOf(engine);
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
    var newIndexEngine = this._getSortedEngines(false)[newIndex];
    if (!newIndexEngine) {
      throw Components.Exception(
        "moveEngine: Can't find engine to replace!",
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    for (var i = 0; i < this._sortedEngines.length; ++i) {
      if (newIndexEngine == this._sortedEngines[i]) {
        break;
      }
      if (this._sortedEngines[i].hidden) {
        newIndex++;
      }
    }

    if (currentIndex == newIndex) {
      return;
    } // nothing to do!

    // Move the engine
    var movedEngine = this.__sortedEngines.splice(currentIndex, 1)[0];
    this.__sortedEngines.splice(newIndex, 0, movedEngine);

    SearchUtils.notifyAction(engine, SearchUtils.MODIFIED_TYPE.CHANGED);

    // Since we moved an engine, we need to update the preferences.
    this._saveSortedEngineList();
  },

  restoreDefaultEngines() {
    this._ensureInitialized();
    for (let e of this._engines.values()) {
      // Unhide all default engines
      if (e.hidden && e.isAppProvided) {
        e.hidden = false;
      }
    }
  },

  /**
   * Helper function to get the current default engine.
   *
   * @param {boolean} privateMode
   *   If true, returns the default engine for private browsing mode, otherwise
   *   the default engine for the normal mode. Note, this function does not
   *   check the "separatePrivateDefault" preference - that is up to the caller.
   * @returns {nsISearchEngine|null}
   *   The appropriate search engine, or null if one could not be determined.
   */
  _getEngineDefault(privateMode) {
    this._ensureInitialized();
    const currentEngine = privateMode
      ? "_currentPrivateEngine"
      : "_currentEngine";
    if (!this[currentEngine]) {
      const attributeName = privateMode ? "private" : "current";
      let name = this._cache.getAttribute(attributeName);
      let engine = this.getEngineByName(name);
      if (
        engine &&
        (engine.isAppProvided ||
          this._cache.getAttribute(this._cache.getHashName(attributeName)) ==
            SearchUtils.getVerificationHash(name))
      ) {
        // If the current engine is a default one, we can relax the
        // verification hash check to reduce the annoyance for users who
        // backup/sync their profile in custom ways.
        this[currentEngine] = engine;
      }
      if (!name) {
        this[currentEngine] = privateMode
          ? this.originalPrivateDefaultEngine
          : this.originalDefaultEngine;
      }
    }

    // If the current engine is not set or hidden, we fallback...
    if (!this[currentEngine] || this[currentEngine].hidden) {
      // first to the original default engine
      let originalDefault = privateMode
        ? this.originalPrivateDefaultEngine
        : this.originalDefaultEngine;
      if (!originalDefault || originalDefault.hidden) {
        // then to the first visible engine
        let firstVisible = this._getSortedEngines(false)[0];
        if (firstVisible && !firstVisible.hidden) {
          if (privateMode) {
            this.defaultPrivateEngine = firstVisible;
          } else {
            this.defaultEngine = firstVisible;
          }
          return firstVisible;
        }
        // and finally as a last resort we unhide the original default engine.
        if (originalDefault) {
          originalDefault.hidden = false;
        }
      }
      if (!originalDefault) {
        return null;
      }

      // If the current engine wasn't set or was hidden, we used a fallback
      // to pick a new current engine. As soon as we return it, this new
      // current engine will become user-visible, so we should persist it.
      // by calling the setter.
      if (privateMode) {
        this.defaultPrivateEngine = originalDefault;
      } else {
        this.defaultEngine = originalDefault;
      }
    }

    return this[currentEngine];
  },

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
  _setEngineDefault(privateMode, newEngine) {
    this._ensureInitialized();
    // Sometimes we get wrapped nsISearchEngine objects (external XPCOM callers),
    // and sometimes we get raw Engine JS objects (callers in this file), so
    // handle both.
    if (
      !(newEngine instanceof Ci.nsISearchEngine) &&
      !(newEngine instanceof SearchEngine)
    ) {
      throw Components.Exception(
        "Invalid argument passed to defaultEngine setter",
        Cr.NS_ERROR_INVALID_ARG
      );
    }

    const newCurrentEngine = this.getEngineByName(newEngine.name);
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
      let loadPathHash = SearchUtils.getVerificationHash(
        newCurrentEngine._loadPath
      );
      let currentHash = newCurrentEngine.getAttr("loadPathHash");
      if (!currentHash || currentHash != loadPathHash) {
        newCurrentEngine.setAttr("loadPathHash", loadPathHash);
        SearchUtils.notifyAction(
          newCurrentEngine,
          SearchUtils.MODIFIED_TYPE.CHANGED
        );
      }
    }

    const currentEngine = `_current${privateMode ? "Private" : ""}Engine`;

    if (newCurrentEngine == this[currentEngine]) {
      return;
    }

    // Ensure that we reset an engine override if it was previously overridden.
    this[currentEngine]?.removeExtensionOverride();

    this[currentEngine] = newCurrentEngine;

    // If we change the default engine in the future, that change should impact
    // users who have switched away from and then back to the build's "default"
    // engine. So clear the user pref when the currentEngine is set to the
    // build's default engine, so that the currentEngine getter falls back to
    // whatever the default is.
    let newName = this[currentEngine].name;
    const originalDefault = privateMode
      ? this.originalPrivateDefaultEngine
      : this.originalDefaultEngine;
    if (this[currentEngine] == originalDefault) {
      newName = "";
    }

    this._cache.setVerifiedAttribute(
      privateMode ? "private" : "current",
      newName
    );

    SearchUtils.notifyAction(
      this[currentEngine],
      SearchUtils.MODIFIED_TYPE[privateMode ? "DEFAULT_PRIVATE" : "DEFAULT"]
    );
    // If we've not got a separate private active, notify update of the
    // private so that the UI updates correctly.
    if (!privateMode && !this._separatePrivateDefault) {
      SearchUtils.notifyAction(
        this[currentEngine],
        SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
      );
    }
  },

  get defaultEngine() {
    return this._getEngineDefault(false);
  },

  set defaultEngine(newEngine) {
    this._setEngineDefault(false, newEngine);
  },

  get defaultPrivateEngine() {
    return this._getEngineDefault(this._separatePrivateDefault);
  },

  set defaultPrivateEngine(newEngine) {
    if (!this._separatePrivateDefaultPrefValue) {
      Services.prefs.setBoolPref(
        SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
        true
      );
    }
    this._setEngineDefault(this._separatePrivateDefault, newEngine);
  },

  async getDefault() {
    await this.init();
    return this.defaultEngine;
  },

  async setDefault(engine) {
    await this.init();
    return (this.defaultEngine = engine);
  },

  async getDefaultPrivate() {
    await this.init();
    return this.defaultPrivateEngine;
  },

  async setDefaultPrivate(engine) {
    await this.init();
    return (this.defaultPrivateEngine = engine);
  },

  _onSeparateDefaultPrefChanged() {
    // We should notify if the normal default, and the currently saved private
    // default are different. Otherwise, save the energy.
    if (this.defaultEngine != this._getEngineDefault(true)) {
      SearchUtils.notifyAction(
        // Always notify with the new private engine, the function checks
        // the preference value for us.
        this.defaultPrivateEngine,
        SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
      );
    }
  },

  async _getEngineInfo(engine) {
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
        let loadPathHash = SearchUtils.getVerificationHash(engine._loadPath);
        engineData.origin =
          currentHash == loadPathHash ? "verified" : "invalid";
      }
    }

    // For privacy, we only collect the submission URL for default engines...
    let sendSubmissionURL = engine.isAppProvided;

    // ... or engines sorted by default near the top of the list.
    if (!sendSubmissionURL) {
      let extras = Services.prefs.getChildList(
        SearchUtils.BROWSER_SEARCH_PREF + "order.extra."
      );

      for (let prefName of extras) {
        try {
          if (engineData.name == Services.prefs.getCharPref(prefName)) {
            sendSubmissionURL = true;
            break;
          }
        } catch (e) {}
      }

      let i = 0;
      while (!sendSubmissionURL) {
        let prefName = `${SearchUtils.BROWSER_SEARCH_PREF}order.${++i}`;
        let engineName = getLocalizedPref(prefName);
        if (!engineName) {
          break;
        }
        if (engineData.name == engineName) {
          sendSubmissionURL = true;
          break;
        }
      }
    }

    if (!sendSubmissionURL) {
      // ... or engines that are the same domain as a default engine.
      let engineHost = engine._getURLOfType(SearchUtils.URL_TYPE.SEARCH)
        .templateHost;
      for (let innerEngine of this._engines.values()) {
        if (!innerEngine.isAppProvided) {
          continue;
        }

        let innerEngineURL = innerEngine._getURLOfType(
          SearchUtils.URL_TYPE.SEARCH
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
  },

  async getDefaultEngineInfo() {
    let [shortName, defaultSearchEngineData] = await this._getEngineInfo(
      this.defaultEngine
    );
    const result = {
      defaultSearchEngine: shortName,
      defaultSearchEngineData,
    };

    if (this._separatePrivateDefault) {
      let [
        privateShortName,
        defaultPrivateSearchEngineData,
      ] = await this._getEngineInfo(this.defaultPrivateEngine);
      result.defaultPrivateSearchEngine = privateShortName;
      result.defaultPrivateSearchEngineData = defaultPrivateSearchEngineData;
    }

    return result;
  },

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
  _parseSubmissionMap: null,

  _buildParseSubmissionMap() {
    this._parseSubmissionMap = new Map();

    // Used only while building the map, indicates which entries do not refer to
    // the main domain of the engine but to an alternate domain, for example
    // "www.google.fr" for the "www.google.com" search engine.
    let keysOfAlternates = new Set();

    for (let engine of this._sortedEngines) {
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
        let existingEntry = this._parseSubmissionMap.get(key);
        if (!existingEntry) {
          if (isAlternate) {
            keysOfAlternates.add(key);
          }
        } else if (!isAlternate && keysOfAlternates.has(key)) {
          keysOfAlternates.delete(key);
        } else {
          return;
        }

        this._parseSubmissionMap.set(key, mapValueForEngine);
      };

      processDomain(urlParsingInfo.mainDomain, false);
      SearchStaticData.getAlternateDomains(
        urlParsingInfo.mainDomain
      ).forEach(d => processDomain(d, true));
    }
  },

  parseSubmissionURL(url) {
    if (!gInitialized) {
      // If search is not initialized, do nothing.
      // This allows us to use this function early in telemetry.
      // The only other consumer of this (places) uses it much later.
      return gEmptyParseSubmissionResult;
    }

    if (!this._parseSubmissionMap) {
      this._buildParseSubmissionMap();
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
    let mapEntry = this._parseSubmissionMap.get(soughtKey);
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
  },

  /**
   * Gets the WebExtensionPolicy for an add-on.
   *
   * @param {string} id
   *   The WebExtension id.
   * @returns {WebExtensionPolicy}
   */
  async _getExtensionPolicy(id) {
    let policy = WebExtensionPolicy.getByID(id);
    if (!policy) {
      let idPrefix = id.split("@")[0];
      let path = `resource://search-extensions/${idPrefix}/`;
      await AddonManager.installBuiltinAddon(path);
      policy = WebExtensionPolicy.getByID(id);
    }
    // On startup the extension may have not finished parsing the
    // manifest, wait for that here.
    await policy.readyPromise;
    return policy;
  },

  // nsIObserver
  observe(engine, topic, verb) {
    switch (topic) {
      case SearchUtils.TOPIC_ENGINE_MODIFIED:
        switch (verb) {
          case SearchUtils.MODIFIED_TYPE.LOADED:
            engine = engine.QueryInterface(Ci.nsISearchEngine);
            logConsole.debug("observe: Done installation of ", engine.name);
            this._addEngineToStore(engine.wrappedJSObject);
            // The addition of the engine to the store always triggers an ADDED
            // or a CHANGED notification, that will trigger the task below.
            break;
          case SearchUtils.MODIFIED_TYPE.ADDED:
          case SearchUtils.MODIFIED_TYPE.CHANGED:
          case SearchUtils.MODIFIED_TYPE.REMOVED:
            // Invalidate the map used to parse URLs to search engines.
            this._parseSubmissionMap = null;
            break;
        }
        break;

      case "idle": {
        this.idleService.removeIdleObserver(this, REINIT_IDLE_TIME_SEC);
        this._queuedIdle = false;
        logConsole.debug(
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
        // be able to avoid the reInit on shutdown, and we'll rebuild the cache
        // on next start instead.
        // This also helps to avoid issues with the add-on manager shutting
        // down at the same time (see _reInit for more info).
        Services.tm.dispatchToMainThread(() => {
          // FYI, This is also used by the search tests to do an async reinit.
          // Locales are removed during shutdown, so ignore this message
          if (!Services.startup.shuttingDown) {
            this._reInit(verb);
          }
        });
        break;
      case Region.REGION_TOPIC:
        if (verb == Region.REGION_UPDATED) {
          logConsole.debug("Region updated:", Region.home);
          ensureKnownRegion(this)
            .then(this._maybeReloadEngines.bind(this))
            .catch(Cu.reportError);
        }
        break;
    }
  },

  // nsITimerCallback
  notify(timer) {
    logConsole.debug("_notify: checking for updates");

    if (
      !Services.prefs.getBoolPref(
        SearchUtils.BROWSER_SEARCH_PREF + "update",
        true
      )
    ) {
      return;
    }

    // Our timer has expired, but unfortunately, we can't get any data from it.
    // Therefore, we need to walk our engine-list, looking for expired engines
    var currentTime = Date.now();
    logConsole.debug("currentTime:" + currentTime);
    for (let e of this._engines.values()) {
      let engine = e.wrappedJSObject;
      if (!engine._hasUpdates) {
        continue;
      }

      var expirTime = engine.getAttr("updateexpir");
      logConsole.debug(
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
        logConsole.debug("skipping engine");
        continue;
      }

      logConsole.debug(engine.name, "has expired");

      engineUpdateService.update(engine);

      // Schedule the next update
      engineUpdateService.scheduleNextUpdate(engine);
    } // end engine iteration
  },

  _addObservers() {
    if (this._observersAdded) {
      // There might be a race between synchronous and asynchronous
      // initialization for which we try to register the observers twice.
      return;
    }
    this._observersAdded = true;

    Services.obs.addObserver(this, SearchUtils.TOPIC_ENGINE_MODIFIED);
    Services.obs.addObserver(this, QUIT_APPLICATION_TOPIC);
    Services.obs.addObserver(this, TOPIC_LOCALES_CHANGE);

    this._cache.addObservers();

    // The current stage of shutdown. Used to help analyze crash
    // signatures in case of shutdown timeout.
    let shutdownState = {
      step: "Not started",
      latestError: {
        message: undefined,
        stack: undefined,
      },
    };
    OS.File.profileBeforeChange.addBlocker(
      "Search service: shutting down",
      () =>
        (async () => {
          // If we are in initialization or re-initialization, then don't attempt
          // to save the cache. It is likely that shutdown will have caused the
          // add-on manager to stop, which can cause initialization to fail.
          // Hence at that stage, we could have a broken cache which we don't
          // want to write.
          // The good news is, that if we don't write the cache here, we'll
          // detect the out-of-date cache on next state, and automatically
          // rebuild it.
          if (!gInitialized || gReinitializing) {
            logConsole.warn(
              "not saving cache on shutdown due to initializing."
            );
            return;
          }

          try {
            await this._cache.shutdown(shutdownState);
          } catch (ex) {
            // Ensure that error is reported and that it causes tests
            // to fail, otherwise ignore it.
            Promise.reject(ex);
          }
        })(),

      () => shutdownState
    );
  },
  _observersAdded: false,

  _removeObservers() {
    if (this._ignoreListListener) {
      IgnoreLists.unsubscribe(this._ignoreListListener);
      delete this._ignoreListListener;
    }
    if (this._queuedIdle) {
      this.idleService.removeIdleObserver(this, REINIT_IDLE_TIME_SEC);
      this._queuedIdle = false;
    }

    this._cache.removeObservers();

    Services.obs.removeObserver(this, SearchUtils.TOPIC_ENGINE_MODIFIED);
    Services.obs.removeObserver(this, QUIT_APPLICATION_TOPIC);
    Services.obs.removeObserver(this, TOPIC_LOCALES_CHANGE);
    Services.obs.removeObserver(this, Region.REGION_TOPIC);
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsISearchService",
    "nsIObserver",
    "nsITimerCallback",
  ]),
};

var engineUpdateService = {
  scheduleNextUpdate(engine) {
    var interval = engine._updateInterval || SEARCH_DEFAULT_UPDATE_INTERVAL;
    var milliseconds = interval * 86400000; // |interval| is in days
    engine.setAttr("updateexpir", Date.now() + milliseconds);
  },

  update(engine) {
    engine = engine.wrappedJSObject;
    logConsole.debug("update called for", engine._name);
    if (
      !Services.prefs.getBoolPref(
        SearchUtils.BROWSER_SEARCH_PREF + "update",
        true
      ) ||
      !engine._hasUpdates
    ) {
      return;
    }

    let testEngine = null;
    let updateURL = engine._getURLOfType(SearchUtils.URL_TYPE.OPENSEARCH);
    let updateURI =
      updateURL && updateURL._hasRelation("self")
        ? updateURL.getSubmission("", engine).uri
        : SearchUtils.makeURI(engine._updateURL);
    if (updateURI) {
      if (engine.isAppProvided && !updateURI.schemeIs("https")) {
        logConsole.debug("Invalid scheme for default engine update");
        return;
      }

      logConsole.debug("updating", engine.name, updateURI.spec);
      testEngine = new OpenSearchEngine({
        uri: updateURI,
        isAppProvided: false,
      });
      testEngine._engineToUpdate = engine;
      testEngine._initFromURIAndLoad(updateURI);
    } else {
      logConsole.debug("invalid updateURI");
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
    this._remoteConfig = RemoteSettings(SearchUtils.SETTINGS_ALLOWLIST_KEY);
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
    logConsole.debug("Allow list is:", result);
    return result;
  }
}

var EXPORTED_SYMBOLS = ["SearchService"];
