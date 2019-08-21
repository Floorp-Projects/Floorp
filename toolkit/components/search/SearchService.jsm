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
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  ExtensionParent: "resource://gre/modules/ExtensionParent.jsm",
  getVerificationHash: "resource://gre/modules/SearchEngine.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  IgnoreLists: "resource://gre/modules/IgnoreLists.jsm",
  SearchEngine: "resource://gre/modules/SearchEngine.jsm",
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
  "gSeparatePrivateDefault",
  SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gGeoSpecificDefaultsEnabled",
  SearchUtils.BROWSER_SEARCH_PREF + "geoSpecificDefaults",
  false
);

// Can't use defineLazyPreferenceGetter because we want the value
// from the default branch
XPCOMUtils.defineLazyGetter(this, "distroID", () => {
  return Services.prefs.getDefaultBranch("distribution.").getCharPref("id", "");
});

// A text encoder to UTF8, used whenever we commit the cache to disk.
XPCOMUtils.defineLazyGetter(this, "gEncoder", function() {
  return new TextEncoder();
});

// Directory service keys
const NS_APP_DISTRIBUTION_SEARCH_DIR_LIST = "SrchPluginsDistDL";

// We load plugins from EXT_SEARCH_PREFIX, where a list.json
// file needs to exist to list available engines.
const EXT_SEARCH_PREFIX = "resource://search-extensions/";
const APP_SEARCH_PREFIX = "resource://search-plugins/";

// The address we use to sign the built in search extensions with.
const EXT_SIGNING_ADDRESS = "search.mozilla.org";

const TOPIC_LOCALES_CHANGE = "intl:app-locales-changed";
const QUIT_APPLICATION_TOPIC = "quit-application";

// The following constants are left undocumented in nsISearchService.idl
// For the moment, they are meant for testing/debugging purposes only.

// Delay for batching invalidation of the JSON cache (ms)
const CACHE_INVALIDATION_DELAY = 1000;

// Current cache version. This should be incremented if the format of the cache
// file is modified.
const CACHE_VERSION = 1;

const CACHE_FILENAME = "search.json.mozlz4";

// The default engine update interval, in days. This is only used if an engine
// specifies an updateURL, but not an updateInterval.
const SEARCH_DEFAULT_UPDATE_INTERVAL = 7;

// The default interval before checking again for the name of the
// default engine for the region, in seconds. Only used if the response
// from the server doesn't specify an interval.
const SEARCH_GEO_DEFAULT_UPDATE_INTERVAL = 2592000; // 30 days.

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
  "twitter",
  "wikipedia",
  "wiktionary",
  "yandex",
  "multilocale",
];

// A tag to denote when we are using the "default_locale" of an engine
const DEFAULT_TAG = "default";

function isPartnerBuild() {
  // Mozilla-provided builds (i.e. funnelcakes) are not partner builds
  return distroID && !distroID.startsWith("mozilla");
}

// A method that tries to determine if this user is in a US geography.
function isUSTimezone() {
  // Timezone assumptions! We assume that if the system clock's timezone is
  // between Newfoundland and Hawaii, that the user is in North America.

  // This includes all of South America as well, but we have relatively few
  // en-US users there, so that's OK.

  // 150 minutes = 2.5 hours (UTC-2.5), which is
  // Newfoundland Daylight Time (http://www.timeanddate.com/time/zones/ndt)

  // 600 minutes = 10 hours (UTC-10), which is
  // Hawaii-Aleutian Standard Time (http://www.timeanddate.com/time/zones/hast)

  let UTCOffset = new Date().getTimezoneOffset();
  return UTCOffset >= 150 && UTCOffset <= 600;
}

// A method that tries to determine our region via an XHR geoip lookup.
var ensureKnownRegion = async function(ss) {
  // If we have a region already stored in our prefs we trust it.
  let region = Services.prefs.getCharPref("browser.search.region", "");
  try {
    if (!region) {
      // We don't have it cached, so fetch it. fetchRegion() will call
      // storeRegion if it gets a result (even if that happens after the
      // promise resolves) and fetchRegionDefault.
      await fetchRegion(ss);
    } else if (gGeoSpecificDefaultsEnabled) {
      // The territory default we have already fetched may have expired.
      let expired = (ss.getGlobalAttr("searchDefaultExpir") || 0) <= Date.now();
      // If we have a default engine or a list of visible default engines
      // saved, the hashes should be valid, verify them now so that we can
      // refetch if they have been tampered with.
      let defaultEngine = ss.getVerifiedGlobalAttr("searchDefault");
      let visibleDefaultEngines = ss.getVerifiedGlobalAttr(
        "visibleDefaultEngines"
      );
      let hasValidHashes =
        (defaultEngine || defaultEngine === undefined) &&
        (visibleDefaultEngines || visibleDefaultEngines === undefined);
      if (expired || !hasValidHashes) {
        await new Promise(resolve => {
          let timeoutMS = Services.prefs.getIntPref(
            "browser.search.geoip.timeout"
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

// Store the result of the geoip request as well as any other values and
// telemetry which depend on it.
async function storeRegion(region) {
  let isTimezoneUS = isUSTimezone();
  // If it's a US region, but not a US timezone, we don't store the value.
  // This works because no region defaults to ZZ (unknown) in nsURLFormatter
  if (region != "US" || isTimezoneUS) {
    Services.prefs.setCharPref("browser.search.region", region);
  }

  // and telemetry...
  if (region == "US" && !isTimezoneUS) {
    SearchUtils.log("storeRegion mismatch - US Region, non-US timezone");
    Services.telemetry
      .getHistogramById("SEARCH_SERVICE_US_COUNTRY_MISMATCHED_TIMEZONE")
      .add(1);
  }
  if (region != "US" && isTimezoneUS) {
    SearchUtils.log("storeRegion mismatch - non-US Region, US timezone");
    Services.telemetry
      .getHistogramById("SEARCH_SERVICE_US_TIMEZONE_MISMATCHED_COUNTRY")
      .add(1);
  }
  // telemetry to compare our geoip response with platform-specific country data.
  // On Mac and Windows, we can get a country code via sysinfo
  let platformCC = await Services.sysinfo.countryCode;
  if (platformCC) {
    let probeUSMismatched, probeNonUSMismatched;
    switch (Services.appinfo.OS) {
      case "Darwin":
        probeUSMismatched = "SEARCH_SERVICE_US_COUNTRY_MISMATCHED_PLATFORM_OSX";
        probeNonUSMismatched =
          "SEARCH_SERVICE_NONUS_COUNTRY_MISMATCHED_PLATFORM_OSX";
        break;
      case "WINNT":
        probeUSMismatched = "SEARCH_SERVICE_US_COUNTRY_MISMATCHED_PLATFORM_WIN";
        probeNonUSMismatched =
          "SEARCH_SERVICE_NONUS_COUNTRY_MISMATCHED_PLATFORM_WIN";
        break;
      default:
        Cu.reportError(
          "Platform " +
            Services.appinfo.OS +
            " has system country code but no search service telemetry probes"
        );
        break;
    }
    if (probeUSMismatched && probeNonUSMismatched) {
      if (region == "US" || platformCC == "US") {
        // one of the 2 said US, so record if they are the same.
        Services.telemetry
          .getHistogramById(probeUSMismatched)
          .add(region != platformCC);
      } else {
        // non-US - record if they are the same
        Services.telemetry
          .getHistogramById(probeNonUSMismatched)
          .add(region != platformCC);
      }
    }
  }
}

// Get the region we are in via a XHR geoip request.
function fetchRegion(ss) {
  // values for the SEARCH_SERVICE_COUNTRY_FETCH_RESULT 'enum' telemetry probe.
  const TELEMETRY_RESULT_ENUM = {
    SUCCESS: 0,
    SUCCESS_WITHOUT_DATA: 1,
    XHRTIMEOUT: 2,
    ERROR: 3,
    // Note that we expect to add finer-grained error types here later (eg,
    // dns error, network error, ssl error, etc) with .ERROR remaining as the
    // generic catch-all that doesn't fit into other categories.
  };
  let endpoint = Services.urlFormatter.formatURLPref(
    "browser.search.geoip.url"
  );
  SearchUtils.log("_fetchRegion starting with endpoint " + endpoint);
  // As an escape hatch, no endpoint means no geoip.
  if (!endpoint) {
    return Promise.resolve();
  }
  let startTime = Date.now();
  return new Promise(resolve => {
    // Instead of using a timeout on the xhr object itself, we simulate one
    // using a timer and let the XHR request complete.  This allows us to
    // capture reliable telemetry on what timeout value should actually be
    // used to ensure most users don't see one while not making it so large
    // that many users end up doing a sync init of the search service and thus
    // would see the jank that implies.
    // (Note we do actually use a timeout on the XHR, but that's set to be a
    // large value just incase the request never completes - we don't want the
    // XHR object to live forever)
    let timeoutMS = Services.prefs.getIntPref("browser.search.geoip.timeout");
    let geoipTimeoutPossible = true;
    let timerId = setTimeout(() => {
      SearchUtils.log("_fetchRegion: timeout fetching region information");
      if (geoipTimeoutPossible) {
        Services.telemetry
          .getHistogramById("SEARCH_SERVICE_COUNTRY_TIMEOUT")
          .add(1);
      }
      timerId = null;
      resolve();
    }, timeoutMS);

    let resolveAndReportSuccess = (result, reason) => {
      // Even if we timed out, we want to save the region and everything
      // related so next startup sees the value and doesn't retry this dance.
      if (result) {
        storeRegion(result).catch(Cu.reportError);
      }
      Services.telemetry
        .getHistogramById("SEARCH_SERVICE_COUNTRY_FETCH_RESULT")
        .add(reason);

      // This notification is just for tests...
      Services.obs.notifyObservers(
        null,
        SearchUtils.TOPIC_SEARCH_SERVICE,
        "geoip-lookup-xhr-complete"
      );

      if (timerId) {
        Services.telemetry
          .getHistogramById("SEARCH_SERVICE_COUNTRY_TIMEOUT")
          .add(0);
        geoipTimeoutPossible = false;
      }

      let callback = () => {
        // If we've already timed out then we've already resolved the promise,
        // so there's nothing else to do.
        if (timerId == null) {
          return;
        }
        clearTimeout(timerId);
        resolve();
      };

      if (result && gGeoSpecificDefaultsEnabled) {
        fetchRegionDefault(ss)
          .then(callback)
          .catch(err => {
            Cu.reportError(err);
            callback();
          });
      } else {
        callback();
      }
    };

    let request = new XMLHttpRequest();
    // This notification is just for tests...
    Services.obs.notifyObservers(
      request,
      SearchUtils.TOPIC_SEARCH_SERVICE,
      "geoip-lookup-xhr-starting"
    );
    request.timeout = 100000; // 100 seconds as the last-chance fallback
    request.onload = function(event) {
      let took = Date.now() - startTime;
      let region = event.target.response && event.target.response.country_code;
      SearchUtils.log(
        "_fetchRegion got success response in " + took + "ms: " + region
      );
      Services.telemetry
        .getHistogramById("SEARCH_SERVICE_COUNTRY_FETCH_TIME_MS")
        .add(took);
      let reason = region
        ? TELEMETRY_RESULT_ENUM.SUCCESS
        : TELEMETRY_RESULT_ENUM.SUCCESS_WITHOUT_DATA;
      resolveAndReportSuccess(region, reason);
    };
    request.ontimeout = function(event) {
      SearchUtils.log(
        "_fetchRegion: XHR finally timed-out fetching region information"
      );
      resolveAndReportSuccess(null, TELEMETRY_RESULT_ENUM.XHRTIMEOUT);
    };
    request.onerror = function(event) {
      SearchUtils.log("_fetchRegion: failed to retrieve region information");
      resolveAndReportSuccess(null, TELEMETRY_RESULT_ENUM.ERROR);
    };
    request.open("POST", endpoint, true);
    request.setRequestHeader("Content-Type", "application/json");
    request.responseType = "json";
    request.send("{}");
  });
}

// This converts our legacy google engines to the
// new codes. We have to manually change them here
// because we can't change the default name in absearch.
function convertGoogleEngines(engineNames) {
  let overrides = {
    google: "google-b-d",
    "google-2018": "google-b-1-d",
  };

  let mobileOverrides = {
    google: "google-b-m",
    "google-2018": "google-b-1-m",
  };

  if (AppConstants.platform == "android") {
    overrides = mobileOverrides;
  }
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

    SearchUtils.log("fetchRegionDefault starting with endpoint " + endpoint);

    let startTime = Date.now();
    let request = new XMLHttpRequest();
    request.timeout = 100000; // 100 seconds as the last-chance fallback
    request.onload = function(event) {
      let took = Date.now() - startTime;

      let status = event.target.status;
      if (status != 200) {
        SearchUtils.log("fetchRegionDefault failed with HTTP code " + status);
        let retryAfter = request.getResponseHeader("retry-after");
        if (retryAfter) {
          ss.setGlobalAttr(
            "searchDefaultExpir",
            Date.now() + retryAfter * 1000
          );
        }
        resolve();
        return;
      }

      let response = event.target.response || {};
      SearchUtils.log("received " + response.toSource());

      if (response.cohort) {
        Services.prefs.setCharPref(cohortPref, response.cohort);
      } else {
        Services.prefs.clearUserPref(cohortPref);
      }

      if (response.settings && response.settings.searchDefault) {
        let defaultEngine = response.settings.searchDefault;
        ss.setVerifiedGlobalAttr("searchDefault", defaultEngine);
        SearchUtils.log(
          "fetchRegionDefault saved searchDefault: " + defaultEngine
        );
      }

      if (response.settings && response.settings.visibleDefaultEngines) {
        let visibleDefaultEngines = response.settings.visibleDefaultEngines;
        let string = visibleDefaultEngines.join(",");
        ss.setVerifiedGlobalAttr("visibleDefaultEngines", string);
        SearchUtils.log(
          "fetchRegionDefault saved visibleDefaultEngines: " + string
        );
      }

      let interval = response.interval || SEARCH_GEO_DEFAULT_UPDATE_INTERVAL;
      let milliseconds = interval * 1000; // |interval| is in seconds.
      ss.setGlobalAttr("searchDefaultExpir", Date.now() + milliseconds);

      SearchUtils.log(
        "fetchRegionDefault got success response in " + took + "ms"
      );
      // If we're doing this somewhere during the app's lifetime, reload the list
      // of engines in order to pick up any geo-specific changes.
      ss._maybeReloadEngines().finally(resolve);
    };
    request.ontimeout = function(event) {
      SearchUtils.log("fetchRegionDefault: XHR finally timed-out");
      resolve();
    };
    request.onerror = function(event) {
      SearchUtils.log(
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
function ParseSubmissionResult(engine, terms, termsOffset, termsLength) {
  this._engine = engine;
  this._terms = terms;
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
  get termsOffset() {
    return this._termsOffset;
  },
  get termsLength() {
    return this._termsLength;
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsISearchParseSubmissionResult]),
};

const gEmptyParseSubmissionResult = Object.freeze(
  new ParseSubmissionResult(null, "", -1, 0)
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
}

SearchService.prototype = {
  classID: Components.ID("{7319788a-fe93-4db3-9f39-818cf08f4256}"),

  // The current status of initialization. Note that it does not determine if
  // initialization is complete, only if an error has been encountered so far.
  _initRV: Cr.NS_OK,

  // The boolean indicates that the initialization has started or not.
  _initStarted: null,

  _ensureKnownRegionPromise: null,

  // Reading the JSON cache file is the first thing done during initialization.
  // During the async init, we save it in a field so that if we have to do a
  // sync init before the async init finishes, we can avoid reading the cache
  // with sync disk I/O and handling lz4 decompression synchronously.
  // This is set back to null as soon as the initialization is finished.
  _cacheFileJSON: null,

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
   * A map of engine short names to `SearchEngine`.
   */
  _engines: null,

  /**
   * An array of engine short names sorted into display order.
   */
  __sortedEngines: null,

  /**
   * This holds the current list of visible engines from the configuration,
   * and is used to update the cache. If the cache value is different to those
   * in the configuration, then the configuration has changed. The engines
   * are loaded using both the new set, and the user's current set (if they
   * still exist).
   */
  _visibleDefaultEngines: [],

  /**
   * The user visible name of the configuration suggested default search engine.
   */
  _searchDefault: null,

  /**
   * The user visible name of the configuration suggested default search engine
   * for private browsing mode.
   */
  _searchPrivateDefault: null,

  /**
   * The suggested order of engines from the configuration.
   */
  _searchOrder: [],

  /**
   * A Set of installed search extensions reported by AddonManager
   * startup before SearchSevice has started. Will be installed
   * during init().
   */
  _startupExtensions: new Set(),

  /**
   * The current metadata stored in the cache. This stores:
   *   - current
   *       The current user-set default engine
   *   - searchDefault
   *       The current default engine (if any) specified by the region server.
   *   - searchDefaultExpir
   *       The expiry time for the default engine when the region server should
   *       be re-checked.
   *   - visibleDefaultEngines
   *       The list of visible default engines supplied by the region server.
   *
   * All of the above except `searchDefaultExpir` have associated hash fields
   * to validate the value is set by the application.
   */
  _metaData: {},

  /**
   * Resets the locally stored data to the original empty values in preparation
   * for a reinit or a reset.
   */
  _resetLocalData() {
    this._engines.clear();
    this.__sortedEngines = null;
    this._currentEngine = null;
    this._privateEngine = null;
    this._visibleDefaultEngines = [];
    this._searchDefault = null;
    this._searchPrivateDefault = null;
    this._searchOrder = [];
    this._metaData = {};
  },

  // If initialization has not been completed yet, perform synchronous
  // initialization.
  // Throws in case of initialization error.
  _ensureInitialized() {
    if (gInitialized) {
      if (!Components.isSuccessCode(this._initRV)) {
        SearchUtils.log("_ensureInitialized: failure");
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
   * @param {boolean} [skipRegionCheck]
   *   Indicates whether we should explicitly await the the region check process to
   *   complete, which may be fetched remotely. Pass in `false` if the caller needs
   *   to be absolutely certain of the correct default engine and/ or ordering of
   *   visible engines.
   * @returns {number}
   *   A Components.results success code on success, otherwise a failure code.
   */
  async _init(skipRegionCheck) {
    SearchUtils.log("_init start");

    try {
      // See if we have a cache file so we don't have to parse a bunch of XML.
      let cache = await this._readCacheFile();

      // The init flow is not going to block on a fetch from an external service,
      // but we're kicking it off as soon as possible to prevent UI flickering as
      // much as possible.
      this._ensureKnownRegionPromise = ensureKnownRegion(this)
        .catch(ex =>
          SearchUtils.log("_init: failure determining region: " + ex)
        )
        .finally(() => (this._ensureKnownRegionPromise = null));
      if (!skipRegionCheck) {
        await this._ensureKnownRegionPromise;
      }

      this._setupRemoteSettings().catch(Cu.reportError);

      await this._loadEngines(cache);

      // Make sure the current list of engines is persisted, without the need to wait.
      SearchUtils.log("_init: engines loaded, writing cache");
      this._buildCache();
      this._addObservers();
    } catch (ex) {
      this._initRV = ex.result !== undefined ? ex.result : Cr.NS_ERROR_FAILURE;
      SearchUtils.log(
        "_init: failure initializng search: " + ex + "\n" + ex.stack
      );
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

    SearchUtils.log("_init: Completed _init");
    return this._initRV;
  },

  /**
   * Obtains the remote settings for the search service. This should only be
   * called from init(). Any subsequent updates to the remote settings are
   * handled via a sync listener.
   *
   * For desktop, the initial remote settings are obtained from dumps in
   * `services/settings/dumps/main/`. These are not shipped with Android, and
   * hence the `get` may take a while to return.
   */
  async _setupRemoteSettings() {
    // Now we have the values, listen for future updates.
    this._ignoreListListener = this._handleIgnoreListUpdated.bind(this);

    const current = await IgnoreLists.getAndSubscribe(this._ignoreListListener);

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
    SearchUtils.log("_handleIgnoreListUpdated");
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

  setGlobalAttr(name, val) {
    this._metaData[name] = val;
    this.batchTask.disarm();
    this.batchTask.arm();
  },
  setVerifiedGlobalAttr(name, val) {
    this.setGlobalAttr(name, val);
    this.setGlobalAttr(name + "Hash", getVerificationHash(val));
  },

  getGlobalAttr(name) {
    return this._metaData[name] || undefined;
  },
  getVerifiedGlobalAttr(name) {
    let val = this.getGlobalAttr(name);
    if (val && this.getGlobalAttr(name + "Hash") != getVerificationHash(val)) {
      SearchUtils.log("getVerifiedGlobalAttr, invalid hash for " + name);
      return "";
    }
    return val;
  },

  _listJSONURL:
    (AppConstants.platform == "android"
      ? APP_SEARCH_PREFIX
      : EXT_SEARCH_PREFIX) + "list.json",

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
    let defaultEngineName = this.getVerifiedGlobalAttr(
      privateMode ? "searchDefaultPrivate" : "searchDefault"
    );
    if (!defaultEngineName) {
      // We only allow the old defaultenginename pref for distributions
      // We can't use isPartnerBuild because we need to allow reading
      // of the defaultengine name pref for funnelcakes.
      if (distroID && !privateMode) {
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
      // Something unexpected as happened. In order to recover the original default engine,
      // use the first visible engine which is the best we can do.
      return this._getSortedEngines(false)[0];
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
    return this._originalDefaultEngine(gSeparatePrivateDefault);
  },

  resetToOriginalDefaultEngine() {
    let originalDefaultEngine = this.originalDefaultEngine;
    originalDefaultEngine.hidden = false;
    this.defaultEngine = originalDefaultEngine;
  },

  async _buildCache() {
    if (this._batchTask) {
      this._batchTask.disarm();
    }

    let cache = {};
    let locale = Services.locale.requestedLocale;
    let buildID = Services.appinfo.platformBuildID;
    let appVersion = Services.appinfo.version;

    // Allows us to force a cache refresh should the cache format change.
    cache.version = CACHE_VERSION;
    // We don't want to incur the costs of stat()ing each plugin on every
    // startup when the only (supported) time they will change is during
    // app updates (where the buildID is obviously going to change).
    // Extension-shipped plugins are the only exception to this, but their
    // directories are blown away during updates, so we'll detect their changes.
    cache.buildID = buildID;
    // Store the appVersion as well so we can do extra things during major updates.
    cache.appVersion = appVersion;
    cache.locale = locale;

    cache.visibleDefaultEngines = this._visibleDefaultEngines;
    cache.metaData = this._metaData;
    cache.engines = [...this._engines.values()];

    try {
      if (!cache.engines.length) {
        throw new Error("cannot write without any engine.");
      }

      SearchUtils.log("_buildCache: Writing to cache file.");
      let path = OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME);
      let data = gEncoder.encode(JSON.stringify(cache));
      await OS.File.writeAtomic(path, data, {
        compression: "lz4",
        tmpPath: path + ".tmp",
      });
      SearchUtils.log("_buildCache: cache file written to disk.");
      Services.obs.notifyObservers(
        null,
        SearchUtils.TOPIC_SEARCH_SERVICE,
        "write-cache-to-disk-complete"
      );
    } catch (ex) {
      SearchUtils.log("_buildCache: Could not write to cache file: " + ex);
    }
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
    SearchUtils.log("_loadEngines: start");
    let engines = await this._findEngines();
    SearchUtils.log("_loadEngines: loading - " + engines.join(","));

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
      } finally {
        iterator.close();
      }
    }

    function notInCacheVisibleEngines(engineName) {
      return !cache.visibleDefaultEngines.includes(engineName);
    }

    let buildID = Services.appinfo.platformBuildID;
    let rebuildCache =
      gEnvironment.get("RELOAD_ENGINES") ||
      !cache.engines ||
      cache.version != CACHE_VERSION ||
      cache.locale != Services.locale.requestedLocale ||
      cache.buildID != buildID ||
      cache.visibleDefaultEngines.length !=
        this._visibleDefaultEngines.length ||
      this._visibleDefaultEngines.some(notInCacheVisibleEngines);

    if (!rebuildCache) {
      SearchUtils.log("_loadEngines: loading from cache directories");
      this._loadEnginesFromCache(cache);
      if (this._engines.size) {
        SearchUtils.log("_loadEngines: done using existing cache");
        return;
      }
      SearchUtils.log(
        "_loadEngines: No valid engines found in cache. Loading engines from disk."
      );
    }

    SearchUtils.log(
      "_loadEngines: Absent or outdated cache. Loading engines from disk."
    );
    for (let loadDir of distDirs) {
      let enginesFromDir = await this._loadEnginesFromDir(loadDir);
      enginesFromDir.forEach(this._addEngineToStore, this);
    }
    if (AppConstants.platform == "android") {
      let enginesFromURLs = await this._loadFromChromeURLs(engines, isReload);
      enginesFromURLs.forEach(this._addEngineToStore, this);
    } else {
      let engineList = this._enginesToLocales(engines);
      for (let [id, locales] of engineList) {
        await this.ensureBuiltinExtension(id, locales);
      }

      SearchUtils.log(
        "_loadEngines: loading " +
          this._startupExtensions.size +
          " engines reported by AddonManager startup"
      );
      for (let extension of this._startupExtensions) {
        await this._installExtensionEngine(extension, [DEFAULT_TAG], true);
      }
    }

    SearchUtils.log(
      "_loadEngines: loading user-installed engines from the obsolete cache"
    );
    this._loadEnginesFromCache(cache, true);

    this._loadEnginesMetadataFromCache(cache);

    SearchUtils.log("_loadEngines: done using rebuilt cache");
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
   */
  async ensureBuiltinExtension(id, locales = [DEFAULT_TAG]) {
    SearchUtils.log("ensureBuiltinExtension: " + id);
    try {
      let policy = WebExtensionPolicy.getByID(id);
      if (!policy) {
        SearchUtils.log("ensureBuiltinExtension: Installing " + id);
        let path = EXT_SEARCH_PREFIX + id.split("@")[0] + "/";
        await AddonManager.installBuiltinAddon(path);
        policy = WebExtensionPolicy.getByID(id);
      }
      // On startup the extension may have not finished parsing the
      // manifest, wait for that here.
      await policy.readyPromise;
      await this._installExtensionEngine(policy.extension, locales);
      SearchUtils.log("ensureBuiltinExtension: " + id + " installed.");
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
      return [engineName, DEFAULT_TAG];
    }

    if (!locale) {
      locale = DEFAULT_TAG;
    }
    return [name, locale];
  },

  /**
   * Reloads engines asynchronously, but only when the service has already been
   * initialized.
   */
  async _maybeReloadEngines() {
    // There's no point in already reloading the list of engines, when the service
    // hasn't even initialized yet.
    if (!gInitialized) {
      return;
    }

    // Before we read the cache file, first make sure all pending tasks are clear.
    if (this._batchTask) {
      let task = this._batchTask;
      this._batchTask = null;
      await task.finalize();
    }
    // Capture the current engine state, in case we need to notify below.
    let prevCurrentEngine = this._currentEngine;
    this._currentEngine = null;

    await this._loadEngines(await this._readCacheFile(), true);
    // Make sure the current list of engines is persisted.
    await this._buildCache();

    // If the defaultEngine has changed between the previous load and this one,
    // dispatch the appropriate notifications.
    if (prevCurrentEngine && this.defaultEngine !== prevCurrentEngine) {
      SearchUtils.notifyAction(
        this._currentEngine,
        SearchUtils.MODIFIED_TYPE.DEFAULT
      );
    }
    Services.obs.notifyObservers(
      null,
      SearchUtils.TOPIC_SEARCH_SERVICE,
      "engines-reloaded"
    );
  },

  _reInit(origin, skipRegionCheck = true) {
    SearchUtils.log("_reInit");
    // Re-entrance guard, because we're using an async lambda below.
    if (gReinitializing) {
      SearchUtils.log("_reInit: already re-initializing, bailing out.");
      return;
    }
    gReinitializing = true;

    // Start by clearing the initialized state, so we don't abort early.
    gInitialized = false;

    (async () => {
      try {
        this._initObservers = PromiseUtils.defer();
        if (this._batchTask) {
          SearchUtils.log("finalizing batch task");
          let task = this._batchTask;
          this._batchTask = null;
          // Tests manipulate the cache directly, so let's not double-write with
          // stale cache data here.
          if (origin == "test") {
            task.disarm();
          } else {
            await task.finalize();
          }
        }

        // Clear the engines, too, so we don't stick with the stale ones.
        this._resetLocalData();

        // Tests that want to force a synchronous re-initialization need to
        // be notified when we are done uninitializing.
        Services.obs.notifyObservers(
          null,
          SearchUtils.TOPIC_SEARCH_SERVICE,
          "uninit-complete"
        );

        let cache = await this._readCacheFile();
        // The init flow is not going to block on a fetch from an external service,
        // but we're kicking it off as soon as possible to prevent UI flickering as
        // much as possible.
        this._ensureKnownRegionPromise = ensureKnownRegion(this)
          .catch(ex =>
            SearchUtils.log("_reInit: failure determining region: " + ex)
          )
          .finally(() => (this._ensureKnownRegionPromise = null));

        if (!skipRegionCheck) {
          await this._ensureKnownRegionPromise;
        }

        await this._loadEngines(cache);
        // Make sure the current list of engines is persisted.
        await this._buildCache();

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
        SearchUtils.log("Reinit failed: " + err);
        SearchUtils.log(err.stack);
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

  /**
   * Read the cache file asynchronously.
   */
  async _readCacheFile() {
    let json;
    try {
      let cacheFilePath = OS.Path.join(
        OS.Constants.Path.profileDir,
        CACHE_FILENAME
      );
      let bytes = await OS.File.read(cacheFilePath, { compression: "lz4" });
      json = JSON.parse(new TextDecoder().decode(bytes));
      if (!json.engines || !json.engines.length) {
        throw new Error("no engine in the file");
      }
      // Reset search default expiration on major releases
      if (
        json.appVersion != Services.appinfo.version &&
        gGeoSpecificDefaultsEnabled &&
        json.metaData
      ) {
        json.metaData.searchDefaultExpir = 0;
      }
    } catch (ex) {
      SearchUtils.log("_readCacheFile: Error reading cache file: " + ex);
      json = {};
    }
    if (!gInitialized && json.metaData) {
      this._metaData = json.metaData;
    }

    return json;
  },

  _batchTask: null,
  get batchTask() {
    if (!this._batchTask) {
      let task = async () => {
        SearchUtils.log("batchTask: Invalidating engine cache");
        await this._buildCache();
      };
      this._batchTask = new DeferredTask(task, CACHE_INVALIDATION_DELAY);
    }
    return this._batchTask;
  },

  _addEngineToStore(engine) {
    if (this._engineMatchesIgnoreLists(engine)) {
      SearchUtils.log("_addEngineToStore: Ignoring engine");
      return;
    }

    SearchUtils.log('_addEngineToStore: Adding engine: "' + engine.name + '"');

    // See if there is an existing engine with the same name. However, if this
    // engine is updating another engine, it's allowed to have the same name.
    var hasSameNameAsUpdate =
      engine._engineToUpdate && engine.name == engine._engineToUpdate.name;
    if (this._engines.has(engine.name) && !hasSameNameAsUpdate) {
      SearchUtils.log("_addEngineToStore: Duplicate engine found, aborting!");
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
      if (this.__sortedEngines) {
        this.__sortedEngines.push(engine);
        this._saveSortedEngineList();
      }
      SearchUtils.notifyAction(engine, SearchUtils.MODIFIED_TYPE.ADDED);
    }

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
        SearchUtils.log(
          "_loadEnginesMetadataFromCache, transfering metadata for " + name
        );
        this._engines.get(name)._metaData = engine._metaData || {};
      }
    }
  },

  _loadEnginesFromCache(cache, skipReadOnly) {
    if (!cache.engines) {
      return;
    }

    SearchUtils.log(
      "_loadEnginesFromCache: Loading " +
        cache.engines.length +
        " engines from cache"
    );

    let skippedEngines = 0;
    for (let engine of cache.engines) {
      if (skipReadOnly && engine._readOnly == undefined) {
        ++skippedEngines;
        continue;
      }

      this._loadEngineFromCache(engine);
    }

    if (skippedEngines) {
      SearchUtils.log(
        "_loadEnginesFromCache: skipped " +
          skippedEngines +
          " read-only engines."
      );
    }
  },

  _loadEngineFromCache(json) {
    try {
      let engine = new SearchEngine({
        name: json._shortName,
        readOnly: json._readOnly == undefined,
      });
      engine._initWithJSON(json);
      this._addEngineToStore(engine);
    } catch (ex) {
      SearchUtils.log("Failed to load " + json._name + " from cache: " + ex);
      SearchUtils.log("Engine JSON: " + json.toSource());
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
    SearchUtils.log(
      "_loadEnginesFromDir: Searching in " + dir.path + " for search engines."
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
        addedEngine = new SearchEngine({
          fileURI: file,
          readOnly: false,
        });
        await addedEngine._initFromFile(file);
        engines.push(addedEngine);
      } catch (ex) {
        SearchUtils.log(
          "_loadEnginesFromDir: Failed to load " + osfile.path + "!\n" + ex
        );
      }
    }
    return engines;
  },

  /**
   * Loads engines from Chrome URLs asynchronously.
   *
   * @param {array} urls
   *   a list of URLs.
   * @param {boolean} [isReload]
   *   is being called from maybeReloadEngines.
   * @returns {Array<SearchEngine>}
   *   An array of search engines that were loaded.
   */
  async _loadFromChromeURLs(urls, isReload = false) {
    let engines = [];
    for (let url of urls) {
      try {
        SearchUtils.log(
          "_loadFromChromeURLs: loading engine from chrome url: " + url
        );
        let uri = Services.io.newURI(APP_SEARCH_PREFIX + url + ".xml");
        let engine = new SearchEngine({
          uri,
          readOnly: true,
        });
        await engine._initFromURI(uri);
        // If there is an existing engine with the same name then update that engine.
        // Only do this during reloads so it doesn't interfere with distribution
        // engines
        if (isReload && this._engines.has(engine.name)) {
          engine._engineToUpdate = this._engines.get(engine.name);
        }
        engines.push(engine);
      } catch (ex) {
        SearchUtils.log("_loadFromChromeURLs: failed to load engine: " + ex);
      }
    }
    return engines;
  },

  /**
   * Loads the list of engines from list.json
   *
   * @returns {Array<string>}
   *   Returns an array of engine names.
   */
  async _findEngines() {
    SearchUtils.log("_findEngines: looking for engines in list.json");

    let chan = SearchUtils.makeChannel(this._listJSONURL);
    if (!chan) {
      SearchUtils.log(
        "_findEngines: " + this._listJSONURL + " isn't registered"
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
        SearchUtils.log("_findEngines: failed to read " + this._listJSONURL);
        resolve();
      };
      request.open("GET", Services.io.newURI(this._listJSONURL).spec, true);
      request.send();
    });

    return this._parseListJSON(list);
  },

  _parseListJSON(list) {
    let json;
    try {
      json = JSON.parse(list);
    } catch (e) {
      Cu.reportError("parseListJSON: Failed to parse list.json: " + e);
      dump("parseListJSON: Failed to parse list.json: " + e + "\n");
      return [];
    }

    let searchRegion = Services.prefs.getCharPref(
      "browser.search.region",
      null
    );

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
    let visibleDefaultEngines = this.getVerifiedGlobalAttr(
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
          SearchUtils.log(
            "_parseListJSON: ignoring visibleDefaultEngines value because " +
              engineName +
              " is not in the jar engines we have found"
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
      isPartnerBuild() &&
      branch.getPrefType("ignoredJAREngines") == branch.PREF_STRING
    ) {
      let ignoredJAREngines = branch
        .getCharPref("ignoredJAREngines")
        .split(",");
      let filteredEngineNames = engineNames.filter(
        e => !ignoredJAREngines.includes(e)
      );
      // Don't allow all engines to be hidden
      if (filteredEngineNames.length > 0) {
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
    SearchUtils.log("_saveSortedEngineList: starting");

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

    SearchUtils.log("_saveSortedEngineList: done");
  },

  _buildSortedEngineList() {
    SearchUtils.log("_buildSortedEngineList: building list");
    var addedEngines = {};
    this.__sortedEngines = [];

    // If the user has specified a custom engine order, read the order
    // information from the metadata instead of the default prefs.
    if (
      Services.prefs.getBoolPref(
        SearchUtils.BROWSER_SEARCH_PREF + "useDBForOrder",
        false
      )
    ) {
      SearchUtils.log("_buildSortedEngineList: using db for order");

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
    } else {
      // The DB isn't being used, so just read the engine order from the prefs
      var i = 0;
      var prefName;

      // The original default engine should always be first in the list
      if (this.originalDefaultEngine) {
        this.__sortedEngines.push(this.originalDefaultEngine);
        addedEngines[
          this.originalDefaultEngine.name
        ] = this.originalDefaultEngine;
      }

      if (distroID) {
        try {
          var extras = Services.prefs.getChildList(
            SearchUtils.BROWSER_SEARCH_PREF + "order.extra."
          );

          for (prefName of extras) {
            let engineName = Services.prefs.getCharPref(prefName);

            let engine = this._engines.get(engineName);
            if (!engine || engine.name in addedEngines) {
              continue;
            }

            this.__sortedEngines.push(engine);
            addedEngines[engine.name] = engine;
          }
        } catch (e) {}

        while (true) {
          prefName = `${SearchUtils.BROWSER_SEARCH_PREF}order.${++i}`;
          let engineName = getLocalizedPref(prefName);
          if (!engineName) {
            break;
          }

          let engine = this._engines.get(engineName);
          if (!engine || engine.name in addedEngines) {
            continue;
          }

          this.__sortedEngines.push(engine);
          addedEngines[engine.name] = engine;
        }
      }

      for (let engineName of this._searchOrder) {
        let engine = this._engines.get(engineName);
        if (!engine || engine.name in addedEngines) {
          continue;
        }

        this.__sortedEngines.push(engine);
        addedEngines[engine.name] = engine;
      }
    }

    // Array for the remaining engines, alphabetically sorted.
    let alphaEngines = [];

    for (let engine of this._engines.values()) {
      if (!(engine.name in addedEngines)) {
        alphaEngines.push(engine);
      }
    }

    let collation = Cc["@mozilla.org/intl/collation-factory;1"]
      .createInstance(Ci.nsICollationFactory)
      .CreateCollation();
    const strength = Ci.nsICollation.kCollationCaseInsensitiveAscii;
    let comparator = (a, b) =>
      collation.compareString(strength, a.name, b.name);
    alphaEngines.sort(comparator);
    return (this.__sortedEngines = this.__sortedEngines.concat(alphaEngines));
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
  async init(skipRegionCheck = false) {
    SearchUtils.log("SearchService.init");
    if (this._initStarted) {
      if (!skipRegionCheck) {
        await this._ensureKnownRegionPromise;
      }
      return this._initObservers.promise;
    }

    TelemetryStopwatch.start("SEARCH_SERVICE_INIT_MS");
    this._initStarted = true;
    try {
      // Complete initialization by calling asynchronous initializer.
      await this._init(skipRegionCheck);
      TelemetryStopwatch.finish("SEARCH_SERVICE_INIT_MS");
    } catch (ex) {
      if (ex.result == Cr.NS_ERROR_ALREADY_INITIALIZED) {
        // No need to pursue asynchronous because synchronous fallback was
        // called and has finished.
        TelemetryStopwatch.finish("SEARCH_SERVICE_INIT_MS");
      } else {
        this._initObservers.reject(ex.result);
        TelemetryStopwatch.cancel("SEARCH_SERVICE_INIT_MS");
        throw ex;
      }
    }
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
  async reInit(skipRegionCheck) {
    return this._reInit("test", skipRegionCheck);
  },

  async getEngines() {
    await this.init(true);
    SearchUtils.log("getEngines: getting all engines");
    return this._getSortedEngines(true);
  },

  async getVisibleEngines() {
    await this.init();
    SearchUtils.log("getVisibleEngines: getting all visible engines");
    return this._getSortedEngines(false);
  },

  async getDefaultEngines() {
    await this.init(true);
    function isDefault(engine) {
      return engine._isDefault;
    }
    var engines = this._sortedEngines.filter(isDefault);
    var engineOrder = {};
    var i = 1;

    // Build a list of engines which we have ordering information for.
    // We're rebuilding the list here because _sortedEngines contain the
    // current order, but we want the original order.

    if (distroID) {
      // First, look at the "browser.search.order.extra" branch.
      try {
        var extras = Services.prefs.getChildList(
          SearchUtils.BROWSER_SEARCH_PREF + "order.extra."
        );

        for (let prefName of extras) {
          let engineName = Services.prefs.getCharPref(prefName);

          if (!(engineName in engineOrder)) {
            engineOrder[engineName] = i++;
          }
        }
      } catch (e) {
        SearchUtils.log("Getting extra order prefs failed: " + e);
      }

      // Now look through the "browser.search.order" branch.
      for (var j = 1; ; j++) {
        let prefName = `${SearchUtils.BROWSER_SEARCH_PREF}order.${j}`;
        let engineName = getLocalizedPref(prefName);
        if (!engineName) {
          break;
        }

        if (!(engineName in engineOrder)) {
          engineOrder[engineName] = i++;
        }
      }
    }

    // Now look at list.json
    for (let engineName of this._searchOrder) {
      engineOrder[engineName] = i++;
    }

    SearchUtils.log(
      "getDefaultEngines: engineOrder: " + engineOrder.toSource()
    );

    function compareEngines(a, b) {
      var aIdx = engineOrder[a.name];
      var bIdx = engineOrder[b.name];

      if (aIdx && bIdx) {
        return aIdx - bIdx;
      }
      if (aIdx) {
        return -1;
      }
      if (bIdx) {
        return 1;
      }

      return a.name.localeCompare(b.name);
    }
    engines.sort(compareEngines);
    return engines;
  },

  async getEnginesByExtensionID(extensionID) {
    await this.init(true);
    SearchUtils.log("getEngines: getting all engines for " + extensionID);
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

  async addEngineWithDetails(name, details) {
    SearchUtils.log('addEngineWithDetails: Adding "' + name + '".');
    let isCurrent = false;
    var params = details;

    let isBuiltin = !!params.isBuiltin;
    // We install search extensions during the init phase, both built in
    // web extensions freshly installed (via addEnginesFromExtension) or
    // user installed extensions being reenabled calling this directly.
    if (!gInitialized && !isBuiltin && !params.initEngine) {
      await this.init(true);
    }
    if (!name) {
      SearchUtils.fail("Invalid name passed to addEngineWithDetails!");
    }
    if (!params.template) {
      SearchUtils.fail("Invalid template passed to addEngineWithDetails!");
    }
    let existingEngine = this._engines.get(name);
    if (existingEngine) {
      if (
        params.extensionID &&
        existingEngine._loadPath.startsWith(
          `jar:[profile]/extensions/${params.extensionID}`
        )
      ) {
        // This is a legacy extension engine that needs to be migrated to WebExtensions.
        isCurrent = this.defaultEngine == existingEngine;
        await this.removeEngine(existingEngine);
      } else {
        SearchUtils.fail(
          "An engine with that name already exists!",
          Cr.NS_ERROR_FILE_ALREADY_EXISTS
        );
      }
    }

    let newEngine = new SearchEngine({
      name,
      readOnly: isBuiltin,
      sanitizeName: true,
    });
    newEngine._initFromMetadata(name, params);
    newEngine._loadPath = "[other]addEngineWithDetails";
    if (params.extensionID) {
      newEngine._loadPath += ":" + params.extensionID;
    }

    this._addEngineToStore(newEngine);
    if (isCurrent) {
      this.defaultEngine = newEngine;
    }
    return newEngine;
  },

  async addEnginesFromExtension(extension) {
    SearchUtils.log("addEnginesFromExtension: " + extension.id);
    if (extension.addonData.builtIn) {
      SearchUtils.log("addEnginesFromExtension: Ignoring builtIn engine.");
      return [];
    }
    // If we havent started SearchService yet, store this extension
    // to install in SearchService.init().
    if (!gInitialized) {
      this._startupExtensions.add(extension);
      return [];
    }
    return this._installExtensionEngine(extension, [DEFAULT_TAG]);
  },

  async _installExtensionEngine(extension, locales, initEngine) {
    SearchUtils.log("installExtensionEngine: " + extension.id);

    let installLocale = async locale => {
      let manifest =
        locale === DEFAULT_TAG
          ? extension.manifest
          : await extension.getLocalizedManifest(locale);
      return this._addEngineForManifest(
        extension,
        manifest,
        locale,
        initEngine
      );
    };

    let engines = [];
    for (let locale of locales) {
      SearchUtils.log(
        "addEnginesFromExtension: installing locale: " +
          extension.id +
          ":" +
          locale
      );
      engines.push(await installLocale(locale));
    }
    return engines;
  },

  async _addEngineForManifest(
    extension,
    manifest,
    locale = DEFAULT_TAG,
    initEngine = false
  ) {
    let params = this.getEngineParams(extension, manifest, locale, {
      initEngine,
    });
    return this.addEngineWithDetails(params.name, params);
  },

  getEngineParams(extension, manifest, locale, extraParams = {}) {
    let { IconDetails } = ExtensionParent;

    // General set of icons for an engine.
    let icons = extension.manifest.icons;
    let iconList = [];
    if (icons) {
      iconList = Object.entries(icons).map(icon => {
        return {
          width: icon[0],
          height: icon[0],
          url: extension.baseURI.resolve(icon[1]),
        };
      });
    }
    let preferredIconUrl =
      icons &&
      extension.baseURI.resolve(IconDetails.getPreferredIcon(icons).icon);
    let searchProvider = manifest.chrome_settings_overrides.search_provider;

    // Filter out any untranslated parameters, the extension has to list all
    // possible mozParams for each engine where a 'locale' may only provide
    // actual values for some (or none).
    if (searchProvider.params) {
      searchProvider.params = searchProvider.params.filter(param => {
        return !(param.value && param.value.startsWith("__MSG_"));
      });
    }

    let shortName = extension.id.split("@")[0];
    if (locale != DEFAULT_TAG) {
      shortName += "-" + locale;
    }

    let searchUrlGetParams = searchProvider.search_url_get_params;
    if (extraParams.code) {
      searchUrlGetParams = "?" + extraParams.code;
    }

    let params = {
      name: searchProvider.name.trim(),
      shortName,
      description: extension.manifest.description,
      searchForm: searchProvider.search_form,
      // AddonManager will sometimes encode the URL via `new URL()`. We want
      // to ensure we're always dealing with decoded urls.
      template: decodeURI(searchProvider.search_url),
      searchGetParams: searchUrlGetParams,
      searchPostParams: searchProvider.search_url_post_params,
      iconURL: searchProvider.favicon_url || preferredIconUrl,
      icons: iconList,
      alias: searchProvider.keyword,
      extensionID: extension.id,
      isBuiltin: extension.addonData.builtIn,
      // suggest_url doesn't currently get encoded.
      suggestURL: searchProvider.suggest_url,
      suggestPostParams: searchProvider.suggest_url_post_params,
      suggestGetParams: searchProvider.suggest_url_get_params,
      queryCharset: searchProvider.encoding || "UTF-8",
      mozParams: searchProvider.params,
      initEngine: extraParams.initEngine || false,
    };

    return params;
  },

  async addEngine(engineURL, iconURL, confirm, extensionID) {
    SearchUtils.log('addEngine: Adding "' + engineURL + '".');
    await this.init(true);
    let errCode;
    try {
      var engine = new SearchEngine({
        uri: engineURL,
        readOnly: false,
      });
      engine._setIcon(iconURL, false);
      engine._confirm = confirm;
      if (extensionID) {
        engine._extensionID = extensionID;
      }
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
      SearchUtils.fail(
        "addEngine: Error adding engine:\n" + ex,
        errCode || Cr.NS_ERROR_FAILURE
      );
    }
    return engine;
  },

  async removeWebExtensionEngine(id) {
    SearchUtils.log("removeWebExtensionEngine: " + id);
    for (let engine of await this.getEnginesByExtensionID(id)) {
      await this.removeEngine(engine);
    }
  },

  async removeEngine(engine) {
    await this.init(true);
    if (!engine) {
      SearchUtils.fail("no engine passed to removeEngine!");
    }

    var engineToRemove = null;
    for (var e of this._engines.values()) {
      if (engine.wrappedJSObject == e) {
        engineToRemove = e;
      }
    }

    if (!engineToRemove) {
      SearchUtils.fail(
        "removeEngine: Can't find engine to remove!",
        Cr.NS_ERROR_FILE_NOT_FOUND
      );
    }

    if (engineToRemove == this.defaultEngine) {
      this._currentEngine = null;
    }

    if (engineToRemove._readOnly || engineToRemove.isBuiltin) {
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

      // Remove the engine from _sortedEngines
      var index = this._sortedEngines.indexOf(engineToRemove);
      if (index == -1) {
        SearchUtils.fail(
          "Can't find engine to remove in _sortedEngines!",
          Cr.NS_ERROR_FAILURE
        );
      }
      this.__sortedEngines.splice(index, 1);

      // Remove the engine from the internal store
      this._engines.delete(engineToRemove.name);

      // Since we removed an engine, we need to update the preferences.
      this._saveSortedEngineList();
    }
    SearchUtils.notifyAction(engineToRemove, SearchUtils.MODIFIED_TYPE.REMOVED);
  },

  async moveEngine(engine, newIndex) {
    await this.init(true);
    if (newIndex > this._sortedEngines.length || newIndex < 0) {
      SearchUtils.fail("moveEngine: Index out of bounds!");
    }
    if (
      !(engine instanceof Ci.nsISearchEngine) &&
      !(engine instanceof SearchEngine)
    ) {
      SearchUtils.fail("moveEngine: Invalid engine passed to moveEngine!");
    }
    if (engine.hidden) {
      SearchUtils.fail(
        "moveEngine: Can't move a hidden engine!",
        Cr.NS_ERROR_FAILURE
      );
    }

    engine = engine.wrappedJSObject;

    var currentIndex = this._sortedEngines.indexOf(engine);
    if (currentIndex == -1) {
      SearchUtils.fail(
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
      SearchUtils.fail(
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
      if (e.hidden && e._isDefault) {
        e.hidden = false;
      }
    }
  },

  get defaultEngine() {
    this._ensureInitialized();
    if (!this._currentEngine) {
      let name = this.getGlobalAttr("current");
      let engine = this.getEngineByName(name);
      if (
        engine &&
        (this.getGlobalAttr("hash") == getVerificationHash(name) ||
          engine._isDefault)
      ) {
        // If the current engine is a default one, we can relax the
        // verification hash check to reduce the annoyance for users who
        // backup/sync their profile in custom ways.
        this._currentEngine = engine;
      }
      if (!name) {
        this._currentEngine = this.originalDefaultEngine;
      }
    }

    // If the current engine is not set or hidden, we fallback...
    if (!this._currentEngine || this._currentEngine.hidden) {
      // first to the original default engine
      let originalDefault = this.originalDefaultEngine;
      if (!originalDefault || originalDefault.hidden) {
        // then to the first visible engine
        let firstVisible = this._getSortedEngines(false)[0];
        if (firstVisible && !firstVisible.hidden) {
          this.defaultEngine = firstVisible;
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
      this.defaultEngine = originalDefault;
    }

    return this._currentEngine;
  },

  set defaultEngine(val) {
    this._ensureInitialized();
    // Sometimes we get wrapped nsISearchEngine objects (external XPCOM callers),
    // and sometimes we get raw Engine JS objects (callers in this file), so
    // handle both.
    if (
      !(val instanceof Ci.nsISearchEngine) &&
      !(val instanceof SearchEngine)
    ) {
      SearchUtils.fail("Invalid argument passed to defaultEngine setter");
    }

    var newCurrentEngine = this.getEngineByName(val.name);
    if (!newCurrentEngine) {
      SearchUtils.fail("Can't find engine in store!", Cr.NS_ERROR_UNEXPECTED);
    }

    if (!newCurrentEngine._isDefault) {
      // If a non default engine is being set as the current engine, ensure
      // its loadPath has a verification hash.
      if (!newCurrentEngine._loadPath) {
        newCurrentEngine._loadPath = "[other]unknown";
      }
      let loadPathHash = getVerificationHash(newCurrentEngine._loadPath);
      let currentHash = newCurrentEngine.getAttr("loadPathHash");
      if (!currentHash || currentHash != loadPathHash) {
        newCurrentEngine.setAttr("loadPathHash", loadPathHash);
        SearchUtils.notifyAction(
          newCurrentEngine,
          SearchUtils.MODIFIED_TYPE.CHANGED
        );
      }
    }

    if (newCurrentEngine == this._currentEngine) {
      return;
    }

    this._currentEngine = newCurrentEngine;

    // If we change the default engine in the future, that change should impact
    // users who have switched away from and then back to the build's "default"
    // engine. So clear the user pref when the currentEngine is set to the
    // build's default engine, so that the currentEngine getter falls back to
    // whatever the default is.
    let newName = this._currentEngine.name;
    if (this._currentEngine == this.originalDefaultEngine) {
      newName = "";
    }

    this.setGlobalAttr("current", newName);
    this.setGlobalAttr("hash", getVerificationHash(newName));

    SearchUtils.notifyAction(
      this._currentEngine,
      SearchUtils.MODIFIED_TYPE.DEFAULT
    );
  },

  async getDefault() {
    await this.init(true);
    return this.defaultEngine;
  },

  async setDefault(engine) {
    await this.init(true);
    return (this.defaultEngine = engine);
  },

  get defaultPrivateEngine() {
    return this.defaultEngine;
  },

  set defaultPrivateEngine(engine) {
    return (this.defaultEngine = engine);
  },

  async getDefaultPrivate() {
    await this.init(true);
    return this.defaultEngine;
  },

  async setDefaultPrivate(engine) {
    await this.init(true);
    return (this.defaultEngine = engine);
  },

  async getDefaultEngineInfo() {
    let result = {};

    let engine;
    try {
      engine = await this.getDefault();
    } catch (e) {
      // The defaultEngine getter will throw if there's no engine at all,
      // which shouldn't happen unless an add-on or a test deleted all of them.
      // Our preferences UI doesn't let users do that.
      Cu.reportError("getDefaultEngineInfo: No default engine");
    }

    if (!engine) {
      result.name = "NONE";
    } else {
      if (engine.name) {
        result.name = engine.name;
      }

      result.loadPath = engine._loadPath;

      let origin;
      if (engine._isDefault) {
        origin = "default";
      } else {
        let currentHash = engine.getAttr("loadPathHash");
        if (!currentHash) {
          origin = "unverified";
        } else {
          let loadPathHash = getVerificationHash(engine._loadPath);
          origin = currentHash == loadPathHash ? "verified" : "invalid";
        }
      }
      result.origin = origin;

      // For privacy, we only collect the submission URL for default engines...
      let sendSubmissionURL = engine._isDefault;

      // ... or engines sorted by default near the top of the list.
      if (!sendSubmissionURL) {
        let extras = Services.prefs.getChildList(
          SearchUtils.BROWSER_SEARCH_PREF + "order.extra."
        );

        for (let prefName of extras) {
          try {
            if (result.name == Services.prefs.getCharPref(prefName)) {
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
          if (result.name == engineName) {
            sendSubmissionURL = true;
            break;
          }
        }

        for (let engineName of this._searchOrder) {
          if (result.name == engineName) {
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
          if (!innerEngine._isDefault) {
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
        result.submissionURL = uri.spec;
      }
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
      SearchStaticData.getAlternateDomains(urlParsingInfo.mainDomain).forEach(
        d => processDomain(d, true)
      );
    }
  },

  /**
   * Checks to see if any engine has an EngineURL of type SearchUtils.URL_TYPE.SEARCH
   * for this request-method, template URL, and query params.
   *
   * @param {string} method
   *   The method of the request.
   * @param {string} template
   *   The URL template of the request.
   * @param {object} formData
   *   Form data associated with the request.
   * @returns {boolean}
   *   Returns true if an engine is found.
   */
  hasEngineWithURL(method, template, formData) {
    this._ensureInitialized();

    // Quick helper method to ensure formData filtered/sorted for compares.
    let getSortedFormData = data => {
      return data
        .filter(a => a.name && a.value)
        .sort((a, b) => {
          if (a.name > b.name) {
            return 1;
          } else if (b.name > a.name) {
            return -1;
          } else if (a.value > b.value) {
            return 1;
          }
          return b.value > a.value ? -1 : 0;
        });
    };

    // Sanitize method, ensure formData is pre-sorted.
    let methodUpper = method.toUpperCase();
    let sortedFormData = getSortedFormData(formData);
    let sortedFormLength = sortedFormData.length;

    return this._getSortedEngines(false).some(engine => {
      return engine._urls.some(url => {
        // Not an engineURL match if type, method, url, #params don't match.
        if (
          url.type != SearchUtils.URL_TYPE.SEARCH ||
          url.method != methodUpper ||
          url.template != template ||
          url.params.length != sortedFormLength
        ) {
          return false;
        }

        // Ensure engineURL formData is pre-sorted. Then, we're
        // not an engineURL match if any queryParam doesn't compare.
        let sortedParams = getSortedFormData(url.params);
        for (let i = 0; i < sortedFormLength; i++) {
          let data = sortedFormData[i];
          let param = sortedParams[i];
          if (
            param.name != data.name ||
            param.value != data.value ||
            param.purpose != data.purpose
          ) {
            return false;
          }
        }
        // Else we're a match.
        return true;
      });
    });
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

    return new ParseSubmissionResult(mapEntry.engine, terms, offset, length);
  },

  // nsIObserver
  observe(engine, topic, verb) {
    switch (topic) {
      case SearchUtils.TOPIC_ENGINE_MODIFIED:
        switch (verb) {
          case SearchUtils.MODIFIED_TYPE.LOADED:
            engine = engine.QueryInterface(Ci.nsISearchEngine);
            SearchUtils.log(
              "nsSearchService::observe: Done installation of " +
                engine.name +
                "."
            );
            this._addEngineToStore(engine.wrappedJSObject);
            if (engine.wrappedJSObject._useNow) {
              SearchUtils.log("nsSearchService::observe: setting current");
              this.defaultEngine = engine;
            }
            // The addition of the engine to the store always triggers an ADDED
            // or a CHANGED notification, that will trigger the task below.
            break;
          case SearchUtils.MODIFIED_TYPE.ADDED:
          case SearchUtils.MODIFIED_TYPE.CHANGED:
          case SearchUtils.MODIFIED_TYPE.REMOVED:
            this.batchTask.disarm();
            this.batchTask.arm();
            // Invalidate the map used to parse URLs to search engines.
            this._parseSubmissionMap = null;
            break;
        }
        break;

      case QUIT_APPLICATION_TOPIC:
        this._removeObservers();
        break;

      case TOPIC_LOCALES_CHANGE:
        // Locale changed. Re-init. We rely on observers, because we can't
        // return this promise to anyone.
        // FYI, This is also used by the search tests to do an async reinit.
        // Locales are removed during shutdown, so ignore this message
        if (!Services.startup.shuttingDown) {
          this._reInit(verb);
        }
        break;
    }
  },

  // nsITimerCallback
  notify(timer) {
    SearchUtils.log("_notify: checking for updates");

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
    SearchUtils.log("currentTime: " + currentTime);
    for (let e of this._engines.values()) {
      let engine = e.wrappedJSObject;
      if (!engine._hasUpdates) {
        continue;
      }

      SearchUtils.log("checking " + engine.name);

      var expirTime = engine.getAttr("updateexpir");
      SearchUtils.log(
        "expirTime: " +
          expirTime +
          "\nupdateURL: " +
          engine._updateURL +
          "\niconUpdateURL: " +
          engine._iconUpdateURL
      );

      var engineExpired = expirTime <= currentTime;

      if (!expirTime || !engineExpired) {
        SearchUtils.log("skipping engine");
        continue;
      }

      SearchUtils.log(engine.name + " has expired");

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
          if (this._batchTask) {
            shutdownState.step = "Finalizing batched task";
            try {
              await this._batchTask.finalize();
              shutdownState.step = "Batched task finalized";
            } catch (ex) {
              shutdownState.step = "Batched task failed to finalize";

              shutdownState.latestError.message = "" + ex;
              if (ex && typeof ex == "object") {
                shutdownState.latestError.stack = ex.stack || undefined;
              }

              // Ensure that error is reported and that it causes tests
              // to fail.
              Promise.reject(ex);
            }
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

    Services.obs.removeObserver(this, SearchUtils.TOPIC_ENGINE_MODIFIED);
    Services.obs.removeObserver(this, QUIT_APPLICATION_TOPIC);
    Services.obs.removeObserver(this, TOPIC_LOCALES_CHANGE);
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsISearchService,
    Ci.nsIObserver,
    Ci.nsITimerCallback,
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
    this._log("update called for " + engine._name);
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
      if (engine._isDefault && !updateURI.schemeIs("https")) {
        this._log("Invalid scheme for default engine update");
        return;
      }

      this._log("updating " + engine.name + " from " + updateURI.spec);
      testEngine = new SearchEngine({
        uri: updateURI,
        readOnly: false,
      });
      testEngine._engineToUpdate = engine;
      testEngine._initFromURIAndLoad(updateURI);
    } else {
      this._log("invalid updateURI");
    }

    if (engine._iconUpdateURL) {
      // If we're updating the engine too, use the new engine object,
      // otherwise use the existing engine object.
      (testEngine || engine)._setIcon(engine._iconUpdateURL, true);
    }
  },

  /**
   * Outputs text to the JavaScript console as well as to stdout, if the search
   * logging pref (browser.search.update.log) is set to true.
   *
   * @param {string} text
   *   The message to log.
   */
  _log(text) {
    if (
      Services.prefs.getBoolPref(
        SearchUtils.BROWSER_SEARCH_PREF + "update.log",
        false
      )
    ) {
      dump("*** Search update: " + text + "\n");
      Services.console.logStringMessage(text);
    }
  },
};

var EXPORTED_SYMBOLS = ["SearchService"];
