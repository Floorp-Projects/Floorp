/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {PromiseUtils} = ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");
const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  SearchStaticData: "resource://gre/modules/SearchStaticData.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  clearTimeout: "resource://gre/modules/Timer.jsm",
  ExtensionParent: "resource://gre/modules/ExtensionParent.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  gEnvironment: ["@mozilla.org/process/environment;1", "nsIEnvironment"],
  gChromeReg: ["@mozilla.org/chrome/chrome-registry;1", "nsIChromeRegistry"],
});

const BROWSER_SEARCH_PREF = "browser.search.";

XPCOMUtils.defineLazyPreferenceGetter(this, "loggingEnabled", BROWSER_SEARCH_PREF + "log", false);
// Can't use defineLazyPreferenceGetter because we want the value
// from the default branch
XPCOMUtils.defineLazyGetter(this, "distroID", () => {
  return Services.prefs.getDefaultBranch("distribution.").getCharPref("id", "");
});

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream", "setInputStream");

XPCOMUtils.defineLazyGlobalGetters(this, ["DOMParser", "XMLHttpRequest", "URLSearchParams"]);

// A text encoder to UTF8, used whenever we commit the cache to disk.
XPCOMUtils.defineLazyGetter(this, "gEncoder",
                            function() {
                              return new TextEncoder();
                            });


// Directory service keys
const NS_APP_DISTRIBUTION_SEARCH_DIR_LIST = "SrchPluginsDistDL";

// We load plugins from APP_SEARCH_PREFIX, where a list.json
// file needs to exist to list available engines.
const APP_SEARCH_PREFIX = "resource://search-plugins/";

// See documentation in nsISearchService.idl.
const SEARCH_ENGINE_TOPIC        = "browser-search-engine-modified";
const TOPIC_LOCALES_CHANGE       = "intl:app-locales-changed";
const QUIT_APPLICATION_TOPIC     = "quit-application";

const SEARCH_ENGINE_REMOVED      = "engine-removed";
const SEARCH_ENGINE_ADDED        = "engine-added";
const SEARCH_ENGINE_CHANGED      = "engine-changed";
const SEARCH_ENGINE_LOADED       = "engine-loaded";
const SEARCH_ENGINE_CURRENT      = "engine-current";
const SEARCH_ENGINE_DEFAULT      = "engine-default";

// The following constants are left undocumented in nsISearchService.idl
// For the moment, they are meant for testing/debugging purposes only.

/**
 * Topic used for events involving the service itself.
 */
const SEARCH_SERVICE_TOPIC       = "browser-search-service";

/**
 * Sent whenever the cache is fully written to disk.
 */
const SEARCH_SERVICE_CACHE_WRITTEN  = "write-cache-to-disk-complete";

// Delay for batching invalidation of the JSON cache (ms)
const CACHE_INVALIDATION_DELAY = 1000;

// Current cache version. This should be incremented if the format of the cache
// file is modified.
const CACHE_VERSION = 1;

const CACHE_FILENAME = "search.json.mozlz4";

// Set an arbitrary cap on the maximum icon size. Without this, large icons can
// cause big delays when loading them at startup.
const MAX_ICON_SIZE   = 20000;

// Default charset to use for sending search parameters. ISO-8859-1 is used to
// match previous nsInternetSearchService behavior as a URL parameter. Label
// resolution causes windows-1252 to be actually used.
const DEFAULT_QUERY_CHARSET = "ISO-8859-1";

const SEARCH_BUNDLE = "chrome://global/locale/search/search.properties";
const BRAND_BUNDLE = "chrome://branding/locale/brand.properties";

const OPENSEARCH_NS_10  = "http://a9.com/-/spec/opensearch/1.0/";
const OPENSEARCH_NS_11  = "http://a9.com/-/spec/opensearch/1.1/";

// Although the specification at http://opensearch.a9.com/spec/1.1/description/
// gives the namespace names defined above, many existing OpenSearch engines
// are using the following versions.  We therefore allow either.
const OPENSEARCH_NAMESPACES = [
  OPENSEARCH_NS_11, OPENSEARCH_NS_10,
  "http://a9.com/-/spec/opensearchdescription/1.1/",
  "http://a9.com/-/spec/opensearchdescription/1.0/",
];

const OPENSEARCH_LOCALNAME = "OpenSearchDescription";

const MOZSEARCH_NS_10     = "http://www.mozilla.org/2006/browser/search/";
const MOZSEARCH_LOCALNAME = "SearchPlugin";

const URLTYPE_SUGGEST_JSON = "application/x-suggestions+json";
const URLTYPE_SEARCH_HTML  = "text/html";
const URLTYPE_OPENSEARCH   = "application/opensearchdescription+xml";

const USER_DEFINED = "searchTerms";

// Custom search parameters
const MOZ_PARAM_LOCALE         = "moz:locale";
const MOZ_PARAM_DIST_ID        = "moz:distributionID";
const MOZ_PARAM_OFFICIAL       = "moz:official";

// Supported OpenSearch parameters
// See http://opensearch.a9.com/spec/1.1/querysyntax/#core
const OS_PARAM_INPUT_ENCODING  = "inputEncoding";
const OS_PARAM_LANGUAGE        = "language";
const OS_PARAM_OUTPUT_ENCODING = "outputEncoding";

// Default values
const OS_PARAM_LANGUAGE_DEF         = "*";
const OS_PARAM_OUTPUT_ENCODING_DEF  = "UTF-8";
const OS_PARAM_INPUT_ENCODING_DEF   = "UTF-8";

// "Unsupported" OpenSearch parameters. For example, we don't support
// page-based results, so if the engine requires that we send the "page index"
// parameter, we'll always send "1".
const OS_PARAM_COUNT        = "count";
const OS_PARAM_START_INDEX  = "startIndex";
const OS_PARAM_START_PAGE   = "startPage";

// Default values
const OS_PARAM_COUNT_DEF        = "20"; // 20 results
const OS_PARAM_START_INDEX_DEF  = "1"; // start at 1st result
const OS_PARAM_START_PAGE_DEF   = "1"; // 1st page

// A array of arrays containing parameters that we don't fully support, and
// their default values. We will only send values for these parameters if
// required, since our values are just really arbitrary "guesses" that should
// give us the output we want.
var OS_UNSUPPORTED_PARAMS = [
  [OS_PARAM_COUNT, OS_PARAM_COUNT_DEF],
  [OS_PARAM_START_INDEX, OS_PARAM_START_INDEX_DEF],
  [OS_PARAM_START_PAGE, OS_PARAM_START_PAGE_DEF],
];

// The default engine update interval, in days. This is only used if an engine
// specifies an updateURL, but not an updateInterval.
const SEARCH_DEFAULT_UPDATE_INTERVAL = 7;

// The default interval before checking again for the name of the
// default engine for the region, in seconds. Only used if the response
// from the server doesn't specify an interval.
const SEARCH_GEO_DEFAULT_UPDATE_INTERVAL = 2592000; // 30 days.

/**
 * Prefixed to all search debug output.
 */
const SEARCH_LOG_PREFIX = "*** Search: ";

/**
 * Outputs aText to the JavaScript console as well as to stdout.
 */
function LOG(aText) {
  if (loggingEnabled) {
    dump(SEARCH_LOG_PREFIX + aText + "\n");
    Services.console.logStringMessage(aText);
  }
}

/**
 * Presents an assertion dialog in non-release builds and throws.
 * @param  message
 *         A message to display
 * @param  resultCode
 *         The NS_ERROR_* value to throw.
 * @throws resultCode
 */
function ERROR(message, resultCode) {
  throw Components.Exception(message, resultCode);
}

/**
 * Logs the failure message (if browser.search.log is enabled) and throws.
 * @param  message
 *         A message to display
 * @param  resultCode
 *         The NS_ERROR_* value to throw.
 * @throws resultCode or NS_ERROR_INVALID_ARG if resultCode isn't specified.
 */
function FAIL(message, resultCode) {
  LOG(message);
  throw Components.Exception(message, resultCode || Cr.NS_ERROR_INVALID_ARG);
}

/**
 * Truncates big blobs of (data-)URIs to console-friendly sizes
 * @param str
 *        String to tone down
 * @param len
 *        Maximum length of the string to return. Defaults to the length of a tweet.
 */
function limitURILength(str, len) {
  len = len || 140;
  if (str.length > len)
    return str.slice(0, len) + "...";
  return str;
}

/**
 * Ensures an assertion is met before continuing. Should be used to indicate
 * fatal errors.
 * @param  assertion
 *         An assertion that must be met
 * @param  message
 *         A message to display if the assertion is not met
 * @param  resultCode
 *         The NS_ERROR_* value to throw if the assertion is not met
 * @throws resultCode
 */
function ENSURE_WARN(assertion, message, resultCode) {
  if (!assertion)
    throw Components.Exception(message, resultCode);
}

function loadListener(aChannel, aEngine, aCallback) {
  this._channel = aChannel;
  this._bytes = [];
  this._engine = aEngine;
  this._callback = aCallback;
}
loadListener.prototype = {
  _callback: null,
  _channel: null,
  _countRead: 0,
  _engine: null,
  _stream: null,

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIRequestObserver,
    Ci.nsIStreamListener,
    Ci.nsIChannelEventSink,
    Ci.nsIInterfaceRequestor,
    Ci.nsIProgressEventSink,
  ]),

  // nsIRequestObserver
  onStartRequest: function SRCH_loadStartR(aRequest) {
    LOG("loadListener: Starting request: " + aRequest.name);
    this._stream = Cc["@mozilla.org/binaryinputstream;1"].
                   createInstance(Ci.nsIBinaryInputStream);
  },

  onStopRequest: function SRCH_loadStopR(aRequest, aStatusCode) {
    LOG("loadListener: Stopping request: " + aRequest.name);

    var requestFailed = !Components.isSuccessCode(aStatusCode);
    if (!requestFailed && (aRequest instanceof Ci.nsIHttpChannel))
      requestFailed = !aRequest.requestSucceeded;

    if (requestFailed || this._countRead == 0) {
      LOG("loadListener: request failed!");
      // send null so the callback can deal with the failure
      this._bytes = null;
    }
    this._callback(this._bytes, this._engine);
    this._channel = null;
    this._engine = null;
  },

  // nsIStreamListener
  onDataAvailable: function SRCH_loadDAvailable(aRequest,
                                                aInputStream, aOffset,
                                                aCount) {
    this._stream.setInputStream(aInputStream);

    // Get a byte array of the data
    this._bytes = this._bytes.concat(this._stream.readByteArray(aCount));
    this._countRead += aCount;
  },

  // nsIChannelEventSink
  asyncOnChannelRedirect: function SRCH_loadCRedirect(aOldChannel, aNewChannel,
                                                      aFlags, callback) {
    this._channel = aNewChannel;
    callback.onRedirectVerifyCallback(Cr.NS_OK);
  },

  // nsIInterfaceRequestor
  getInterface: function SRCH_load_GI(aIID) {
    return this.QueryInterface(aIID);
  },

  // nsIProgressEventSink
  onProgress(aRequest, aContext, aProgress, aProgressMax) {},
  onStatus(aRequest, aContext, aStatus, aStatusArg) {},
};

/**
 * Tries to rescale an icon to a given size.
 *
 * @param aByteArray Byte array containing the icon payload.
 * @param aContentType Mime type of the payload.
 * @param [optional] aSize desired icon size.
 * @throws if the icon cannot be rescaled or the rescaled icon is too big.
 */
function rescaleIcon(aByteArray, aContentType, aSize = 32) {
  if (aContentType == "image/svg+xml")
    throw new Error("Cannot rescale SVG image");

  let imgTools = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools);
  let arrayBuffer = (new Int8Array(aByteArray)).buffer;
  let container = imgTools.decodeImageFromArrayBuffer(arrayBuffer, aContentType);
  let stream = imgTools.encodeScaledImage(container, "image/png", aSize, aSize);
  let size = stream.available();
  if (size > MAX_ICON_SIZE)
    throw new Error("Icon is too big");
  let bis = new BinaryInputStream(stream);
  return [bis.readByteArray(size), "image/png"];
}

function isPartnerBuild() {
  // Mozilla-provided builds (i.e. funnelcakes) are not partner builds
  return distroID && !distroID.startsWith("mozilla");
}

// Method to determine if we should be using geo-specific defaults
function geoSpecificDefaultsEnabled() {
  return Services.prefs.getBoolPref("browser.search.geoSpecificDefaults", false);
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

  let UTCOffset = (new Date()).getTimezoneOffset();
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
    } else if (geoSpecificDefaultsEnabled()) {
      // The territory default we have already fetched may have expired.
      let expired = (ss.getGlobalAttr("searchDefaultExpir") || 0) <= Date.now();
      // If we have a default engine or a list of visible default engines
      // saved, the hashes should be valid, verify them now so that we can
      // refetch if they have been tampered with.
      let defaultEngine = ss.getVerifiedGlobalAttr("searchDefault");
      let visibleDefaultEngines = ss.getVerifiedGlobalAttr("visibleDefaultEngines");
      let hasValidHashes = (defaultEngine || defaultEngine === undefined) &&
                           (visibleDefaultEngines || visibleDefaultEngines === undefined);
      if (expired || !hasValidHashes) {
        await new Promise(resolve => {
          let timeoutMS = Services.prefs.getIntPref("browser.search.geoip.timeout");
          let timerId = setTimeout(() => {
            timerId = null;
            resolve();
          }, timeoutMS);

          let callback = () => {
            clearTimeout(timerId);
            resolve();
          };
          fetchRegionDefault(ss).then(callback).catch(err => {
            Cu.reportError(err);
            callback();
          });
        });
      }
    }

    // If gInitialized is true then the search service was forced to perform
    // a sync initialization during our XHRs - capture this via telemetry.
    Services.telemetry.getHistogramById("SEARCH_SERVICE_COUNTRY_FETCH_CAUSED_SYNC_INIT").add(gInitialized);
  } catch (ex) {
    Cu.reportError(ex);
  } finally {
    // Since bug 1492475, we don't block our init flows on the region fetch as
    // performed here. But we'd still like to unit-test its implementation, thus
    // we fire this observer notification.
    Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "ensure-known-region-done");
  }
};

// Store the result of the geoip request as well as any other values and
// telemetry which depend on it.
function storeRegion(region) {
  let isTimezoneUS = isUSTimezone();
  // If it's a US region, but not a US timezone, we don't store the value.
  // This works because no region defaults to ZZ (unknown) in nsURLFormatter
  if (region != "US" || isTimezoneUS) {
    Services.prefs.setCharPref("browser.search.region", region);
  }

  // and telemetry...
  if (region == "US" && !isTimezoneUS) {
    LOG("storeRegion mismatch - US Region, non-US timezone");
    Services.telemetry.getHistogramById("SEARCH_SERVICE_US_COUNTRY_MISMATCHED_TIMEZONE").add(1);
  }
  if (region != "US" && isTimezoneUS) {
    LOG("storeRegion mismatch - non-US Region, US timezone");
    Services.telemetry.getHistogramById("SEARCH_SERVICE_US_TIMEZONE_MISMATCHED_COUNTRY").add(1);
  }
  // telemetry to compare our geoip response with platform-specific country data.
  // On Mac and Windows, we can get a country code via sysinfo
  let platformCC = Services.sysinfo.get("countryCode");
  if (platformCC) {
    let probeUSMismatched, probeNonUSMismatched;
    switch (Services.appinfo.OS) {
      case "Darwin":
        probeUSMismatched = "SEARCH_SERVICE_US_COUNTRY_MISMATCHED_PLATFORM_OSX";
        probeNonUSMismatched = "SEARCH_SERVICE_NONUS_COUNTRY_MISMATCHED_PLATFORM_OSX";
        break;
      case "WINNT":
        probeUSMismatched = "SEARCH_SERVICE_US_COUNTRY_MISMATCHED_PLATFORM_WIN";
        probeNonUSMismatched = "SEARCH_SERVICE_NONUS_COUNTRY_MISMATCHED_PLATFORM_WIN";
        break;
      default:
        Cu.reportError("Platform " + Services.appinfo.OS +
          " has system country code but no search service telemetry probes");
        break;
    }
    if (probeUSMismatched && probeNonUSMismatched) {
      if (region == "US" || platformCC == "US") {
        // one of the 2 said US, so record if they are the same.
        Services.telemetry.getHistogramById(probeUSMismatched).add(region != platformCC);
      } else {
        // non-US - record if they are the same
        Services.telemetry.getHistogramById(probeNonUSMismatched).add(region != platformCC);
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
  let endpoint = Services.urlFormatter.formatURLPref("browser.search.geoip.url");
  LOG("_fetchRegion starting with endpoint " + endpoint);
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
      LOG("_fetchRegion: timeout fetching region information");
      if (geoipTimeoutPossible)
        Services.telemetry.getHistogramById("SEARCH_SERVICE_COUNTRY_TIMEOUT").add(1);
      timerId = null;
      resolve();
    }, timeoutMS);

    let resolveAndReportSuccess = (result, reason) => {
      // Even if we timed out, we want to save the region and everything
      // related so next startup sees the value and doesn't retry this dance.
      if (result) {
        storeRegion(result);
      }
      Services.telemetry.getHistogramById("SEARCH_SERVICE_COUNTRY_FETCH_RESULT").add(reason);

      // This notification is just for tests...
      Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "geoip-lookup-xhr-complete");

      if (timerId) {
        Services.telemetry.getHistogramById("SEARCH_SERVICE_COUNTRY_TIMEOUT").add(0);
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

      if (result && geoSpecificDefaultsEnabled()) {
        fetchRegionDefault(ss).then(callback).catch(err => {
          Cu.reportError(err);
          callback();
        });
      } else {
        callback();
      }
    };

    let request = new XMLHttpRequest();
    // This notification is just for tests...
    Services.obs.notifyObservers(request, SEARCH_SERVICE_TOPIC, "geoip-lookup-xhr-starting");
    request.timeout = 100000; // 100 seconds as the last-chance fallback
    request.onload = function(event) {
      let took = Date.now() - startTime;
      let region = event.target.response && event.target.response.country_code;
      LOG("_fetchRegion got success response in " + took + "ms: " + region);
      Services.telemetry.getHistogramById("SEARCH_SERVICE_COUNTRY_FETCH_TIME_MS").add(took);
      let reason = region ? TELEMETRY_RESULT_ENUM.SUCCESS : TELEMETRY_RESULT_ENUM.SUCCESS_WITHOUT_DATA;
      resolveAndReportSuccess(region, reason);
    };
    request.ontimeout = function(event) {
      LOG("_fetchRegion: XHR finally timed-out fetching region information");
      resolveAndReportSuccess(null, TELEMETRY_RESULT_ENUM.XHRTIMEOUT);
    };
    request.onerror = function(event) {
      LOG("_fetchRegion: failed to retrieve region information");
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
    "google": "google-b-d",
    "google-2018": "google-b-1-d",
  };

  let mobileOverrides = {
    "google": "google-b-m",
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
var fetchRegionDefault = (ss) => new Promise(resolve => {
  let urlTemplate = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF)
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
  if (cohort)
    endpoint += "/" + cohort;

  LOG("fetchRegionDefault starting with endpoint " + endpoint);

  let startTime = Date.now();
  let request = new XMLHttpRequest();
  request.timeout = 100000; // 100 seconds as the last-chance fallback
  request.onload = function(event) {
    let took = Date.now() - startTime;

    let status = event.target.status;
    if (status != 200) {
      LOG("fetchRegionDefault failed with HTTP code " + status);
      let retryAfter = request.getResponseHeader("retry-after");
      if (retryAfter) {
        ss.setGlobalAttr("searchDefaultExpir", Date.now() + retryAfter * 1000);
      }
      resolve();
      return;
    }

    let response = event.target.response || {};
    LOG("received " + response.toSource());

    if (response.cohort) {
      Services.prefs.setCharPref(cohortPref, response.cohort);
    } else {
      Services.prefs.clearUserPref(cohortPref);
    }

    if (response.settings && response.settings.searchDefault) {
      let defaultEngine = response.settings.searchDefault;
      ss.setVerifiedGlobalAttr("searchDefault", defaultEngine);
      LOG("fetchRegionDefault saved searchDefault: " + defaultEngine);
    }

    if (response.settings && response.settings.visibleDefaultEngines) {
      let visibleDefaultEngines = response.settings.visibleDefaultEngines;
      let string = visibleDefaultEngines.join(",");
      ss.setVerifiedGlobalAttr("visibleDefaultEngines", string);
      LOG("fetchRegionDefault saved visibleDefaultEngines: " + string);
    }

    let interval = response.interval || SEARCH_GEO_DEFAULT_UPDATE_INTERVAL;
    let milliseconds = interval * 1000; // |interval| is in seconds.
    ss.setGlobalAttr("searchDefaultExpir", Date.now() + milliseconds);

    LOG("fetchRegionDefault got success response in " + took + "ms");
    // If we're doing this somewhere during the app's lifetime, reload the list
    // of engines in order to pick up any geo-specific changes.
    ss._maybeReloadEngines().finally(resolve);
  };
  request.ontimeout = function(event) {
    LOG("fetchRegionDefault: XHR finally timed-out");
    resolve();
  };
  request.onerror = function(event) {
    LOG("fetchRegionDefault: failed to retrieve territory default information");
    resolve();
  };
  request.open("GET", endpoint, true);
  request.setRequestHeader("Content-Type", "application/json");
  request.responseType = "json";
  request.send();
});

function getVerificationHash(aName) {
  let disclaimer = "By modifying this file, I agree that I am doing so " +
    "only within $appName itself, using official, user-driven search " +
    "engine selection processes, and in a way which does not circumvent " +
    "user consent. I acknowledge that any attempt to change this file " +
    "from outside of $appName is a malicious act, and will be responded " +
    "to accordingly.";

  let salt = OS.Path.basename(OS.Constants.Path.profileDir) + aName +
             disclaimer.replace(/\$appName/g, Services.appinfo.name);

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";

  // Data is an array of bytes.
  let data = converter.convertToByteArray(salt, {});
  let hasher = Cc["@mozilla.org/security/hash;1"]
                 .createInstance(Ci.nsICryptoHash);
  hasher.init(hasher.SHA256);
  hasher.update(data, data.length);

  return hasher.finish(true);
}

/**
 * Wrapper function for nsIIOService::newURI.
 * @param aURLSpec
 *        The URL string from which to create an nsIURI.
 * @returns an nsIURI object, or null if the creation of the URI failed.
 */
function makeURI(aURLSpec, aCharset) {
  try {
    return Services.io.newURI(aURLSpec, aCharset);
  } catch (ex) { }

  return null;
}

/**
 * Wrapper function for nsIIOService::newChannel2.
 * @param url
 *        The URL string from which to create an nsIChannel.
 * @returns an nsIChannel object, or null if the url is invalid.
 */
function makeChannel(url) {
  try {
    let uri = typeof url == "string" ? Services.io.newURI(url) : url;
    return Services.io.newChannelFromURI(uri,
                                         null, /* loadingNode */
                                         Services.scriptSecurityManager.getSystemPrincipal(),
                                         null, /* triggeringPrincipal */
                                         Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                                         Ci.nsIContentPolicy.TYPE_OTHER);
  } catch (ex) { }

  return null;
}

/**
 * Gets a directory from the directory service.
 * @param aKey
 *        The directory service key indicating the directory to get.
 */
function getDir(aKey, aIFace) {
  if (!aKey)
    FAIL("getDir requires a directory key!");

  return Services.dirsvc.get(aKey, aIFace || Ci.nsIFile);
}

/**
 * Gets the current value of the locale.  It's possible for this preference to
 * be localized, so we have to do a little extra work here.  Similar code
 * exists in nsHttpHandler.cpp when building the UA string.
 */
function getLocale() {
  return Services.locale.requestedLocale;
}

/**
 * Wrapper for nsIPrefBranch::getComplexValue.
 * @param aPrefName
 *        The name of the pref to get.
 * @returns aDefault if the requested pref doesn't exist.
 */
function getLocalizedPref(aPrefName, aDefault) {
  const nsIPLS = Ci.nsIPrefLocalizedString;
  try {
    return Services.prefs.getComplexValue(aPrefName, nsIPLS).data;
  } catch (ex) {}

  return aDefault;
}

/**
 * @return a sanitized name to be used as a filename, or a random name
 *         if a sanitized name cannot be obtained (if aName contains
 *         no valid characters).
 */
function sanitizeName(aName) {
  const maxLength = 60;
  const minLength = 1;
  var name = aName.toLowerCase();
  name = name.replace(/\s+/g, "-");
  name = name.replace(/[^-a-z0-9]/g, "");

  // Use a random name if our input had no valid characters.
  if (name.length < minLength)
    name = Math.random().toString(36).replace(/^.*\./, "");

  // Force max length.
  return name.substring(0, maxLength);
}

/**
 * Retrieve a pref from the search param branch.
 *
 * @param prefName
 *        The name of the pref.
 **/
function getMozParamPref(prefName) {
  let branch = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF + "param.");
  return encodeURIComponent(branch.getCharPref(prefName));
}

/**
 * Notifies watchers of SEARCH_ENGINE_TOPIC about changes to an engine or to
 * the state of the search service.
 *
 * @param aEngine
 *        The nsISearchEngine object to which the change applies.
 * @param aVerb
 *        A verb describing the change.
 *
 * @see nsISearchService.idl
 */
var gInitialized = false;
var gReinitializing = false;
function notifyAction(aEngine, aVerb) {
  if (gInitialized) {
    LOG("NOTIFY: Engine: \"" + aEngine.name + "\"; Verb: \"" + aVerb + "\"");
    Services.obs.notifyObservers(aEngine, SEARCH_ENGINE_TOPIC, aVerb);
  }
}

/**
 * Simple object representing a name/value pair.
 */
function QueryParameter(aName, aValue, aPurpose) {
  if (!aName || (aValue == null))
    FAIL("missing name or value for QueryParameter!");

  this.name = aName;
  this.value = aValue;
  this.purpose = aPurpose;
}

/**
 * Perform OpenSearch parameter substitution on aParamValue.
 *
 * @param aParamValue
 *        A string containing OpenSearch search parameters.
 * @param aSearchTerms
 *        The user-provided search terms. This string will inserted into
 *        aParamValue as the value of the OS_PARAM_USER_DEFINED parameter.
 *        This value must already be escaped appropriately - it is inserted
 *        as-is.
 * @param aEngine
 *        The engine which owns the string being acted on.
 *
 * @see http://opensearch.a9.com/spec/1.1/querysyntax/#core
 */
function ParamSubstitution(aParamValue, aSearchTerms, aEngine) {
  const PARAM_REGEXP = /\{((?:\w+:)?\w+)(\??)\}/g;
  return aParamValue.replace(PARAM_REGEXP, function(match, name, optional) {
    // {searchTerms} is by far the most common param so handle it first.
    if (name == USER_DEFINED)
      return aSearchTerms;

    // {inputEncoding} is the second most common param.
    if (name == OS_PARAM_INPUT_ENCODING)
      return aEngine.queryCharset;

    // moz: parameters are only available for default search engines.
    if (name.startsWith("moz:") && aEngine._isDefault) {
      // {moz:locale} and {moz:distributionID} are common
      if (name == MOZ_PARAM_LOCALE)
        return getLocale();
      if (name == MOZ_PARAM_DIST_ID) {
        return Services.prefs.getCharPref(BROWSER_SEARCH_PREF + "distributionID",
                                          Services.appinfo.distributionID || "");
      }
      // {moz:official} seems to have little use.
      if (name == MOZ_PARAM_OFFICIAL) {
        if (Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "official",
                                       AppConstants.MOZ_OFFICIAL_BRANDING))
          return "official";
        return "unofficial";
      }
    }

    // Handle the less common OpenSearch parameters we're confident about.
    if (name == OS_PARAM_LANGUAGE)
      return getLocale() || OS_PARAM_LANGUAGE_DEF;
    if (name == OS_PARAM_OUTPUT_ENCODING)
      return OS_PARAM_OUTPUT_ENCODING_DEF;

    // At this point, if a parameter is optional, just omit it.
    if (optional)
      return "";

    // Replace unsupported parameters that only have hardcoded default values.
    for (let param of OS_UNSUPPORTED_PARAMS) {
      if (name == param[0])
        return param[1];
    }

    // Don't replace unknown non-optional parameters.
    return match;
  });
}

const ENGINE_ALIASES = new Map([
  ["google", ["@google"]],
  ["amazondotcom", ["@amazon"]],
  ["amazondotcom-de", ["@amazon"]],
  ["amazon-en-GB", ["@amazon"]],
  ["amazon-france", ["@amazon"]],
  ["amazon-jp", ["@amazon"]],
  ["amazon-it", ["@amazon"]],
  ["twitter", ["@twitter"]],
  ["wikipedia", ["@wikipedia"]],
  ["ebay", ["@ebay"]],
  ["bing", ["@bing"]],
  ["ddg", ["@duckduckgo", "@ddg"]],
  ["yandex", ["@\u044F\u043D\u0434\u0435\u043A\u0441", "@yandex"]],
  ["baidu", ["@\u767E\u5EA6", "@baidu"]],
]);

function getInternalAliases(engine) {
  if (!engine._isDefault) {
    return [];
  }
  for (let [name, aliases] of ENGINE_ALIASES) {
    if (engine._shortName.startsWith(name)) {
      return aliases;
    }
  }
  return [];
}

/**
 * Creates an engineURL object, which holds the query URL and all parameters.
 *
 * @param aType
 *        A string containing the name of the MIME type of the search results
 *        returned by this URL.
 * @param aMethod
 *        The HTTP request method. Must be a case insensitive value of either
 *        "GET" or "POST".
 * @param aTemplate
 *        The URL to which search queries should be sent. For GET requests,
 *        must contain the string "{searchTerms}", to indicate where the user
 *        entered search terms should be inserted.
 * @param aResultDomain
 *        The root domain for this URL.  Defaults to the template's host.
 *
 * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag
 *
 * @throws NS_ERROR_NOT_IMPLEMENTED if aType is unsupported.
 */
function EngineURL(aType, aMethod, aTemplate, aResultDomain) {
  if (!aType || !aMethod || !aTemplate)
    FAIL("missing type, method or template for EngineURL!");

  var method = aMethod.toUpperCase();
  var type   = aType.toLowerCase();

  if (method != "GET" && method != "POST")
    FAIL("method passed to EngineURL must be \"GET\" or \"POST\"");

  this.type     = type;
  this.method   = method;
  this.params   = [];
  this.rels     = [];
  // Don't serialize expanded mozparams
  this.mozparams = {};

  var templateURI = makeURI(aTemplate);
  if (!templateURI)
    FAIL("new EngineURL: template is not a valid URI!", Cr.NS_ERROR_FAILURE);

  switch (templateURI.scheme) {
    case "http":
    case "https":
    // Disable these for now, see bug 295018
    // case "file":
    // case "resource":
      this.template = aTemplate;
      break;
    default:
      FAIL("new EngineURL: template uses invalid scheme!", Cr.NS_ERROR_FAILURE);
  }

  this.templateHost = templateURI.host;
  // If no resultDomain was specified in the engine definition file, use the
  // host from the template.
  this.resultDomain = aResultDomain || this.templateHost;
}
EngineURL.prototype = {

  addParam: function SRCH_EURL_addParam(aName, aValue, aPurpose) {
    this.params.push(new QueryParameter(aName, aValue, aPurpose));
  },

  // Note: This method requires that aObj has a unique name or the previous MozParams entry with
  // that name will be overwritten.
  _addMozParam: function SRCH_EURL__addMozParam(aObj) {
    aObj.mozparam = true;
    this.mozparams[aObj.name] = aObj;
  },

  getSubmission: function SRCH_EURL_getSubmission(aSearchTerms, aEngine, aPurpose) {
    var url = ParamSubstitution(this.template, aSearchTerms, aEngine);
    // Default to searchbar if the purpose is not provided
    var purpose = aPurpose || "searchbar";

    // If a particular purpose isn't defined in the plugin, fallback to 'searchbar'.
    if (!this.params.some(p => p.purpose !== undefined && p.purpose == purpose))
      purpose = "searchbar";

    // Create an application/x-www-form-urlencoded representation of our params
    // (name=value&name=value&name=value)
    let dataArray = [];
    for (var i = 0; i < this.params.length; ++i) {
      var param = this.params[i];

      // If this parameter has a purpose, only add it if the purpose matches
      if (param.purpose !== undefined && param.purpose != purpose)
        continue;

      var value = ParamSubstitution(param.value, aSearchTerms, aEngine);

      dataArray.push(param.name + "=" + value);
    }
    let dataString = dataArray.join("&");

    var postData = null;
    if (this.method == "GET") {
      // GET method requests have no post data, and append the encoded
      // query string to the url...
      if (dataString) {
        if (url.includes("?")) {
          url = `${url}&${dataString}`;
        } else {
          url = `${url}?${dataString}`;
        }
      }
    } else if (this.method == "POST") {
      // POST method requests must wrap the encoded text in a MIME
      // stream and supply that as POSTDATA.
      var stringStream = Cc["@mozilla.org/io/string-input-stream;1"].
                         createInstance(Ci.nsIStringInputStream);
      stringStream.data = dataString;

      postData = Cc["@mozilla.org/network/mime-input-stream;1"].
                 createInstance(Ci.nsIMIMEInputStream);
      postData.addHeader("Content-Type", "application/x-www-form-urlencoded");
      postData.setData(stringStream);
    }

    return new Submission(Services.io.newURI(url), postData);
  },

  _getTermsParameterName: function SRCH_EURL__getTermsParameterName() {
    let queryParam = this.params.find(p => p.value == "{" + USER_DEFINED + "}");
    return queryParam ? queryParam.name : "";
  },

  _hasRelation: function SRC_EURL__hasRelation(aRel) {
    return this.rels.some(e => e == aRel.toLowerCase());
  },

  _initWithJSON: function SRC_EURL__initWithJSON(aJson, aEngine) {
    if (!aJson.params)
      return;

    this.rels = aJson.rels;

    for (let i = 0; i < aJson.params.length; ++i) {
      let param = aJson.params[i];
      if (param.mozparam) {
        if (param.condition == "pref") {
          let value = getMozParamPref(param.pref);
          this.addParam(param.name, value);
        }
        this._addMozParam(param);
      } else {
        this.addParam(param.name, param.value, param.purpose || undefined);
      }
    }
  },

  /**
   * Creates a JavaScript object that represents this URL.
   * @returns An object suitable for serialization as JSON.
   **/
  toJSON: function SRCH_EURL_toJSON() {
    var json = {
      template: this.template,
      rels: this.rels,
      resultDomain: this.resultDomain,
    };

    if (this.type != URLTYPE_SEARCH_HTML)
      json.type = this.type;
    if (this.method != "GET")
      json.method = this.method;

    function collapseMozParams(aParam) {
      return this.mozparams[aParam.name] || aParam;
    }
    json.params = this.params.map(collapseMozParams, this);

    return json;
  },
};

/**
 * nsISearchEngine constructor.
 * @param aLocation
 *        A nsIFile or nsIURI object representing the location of the
 *        search engine data file.
 * @param aIsReadOnly
 *        Boolean indicating whether the engine should be treated as read-only.
 */
function Engine(aLocation, aIsReadOnly) {
  this._readOnly = aIsReadOnly;
  this._urls = [];
  this._metaData = {};

  let file, uri;
  if (typeof aLocation == "string") {
    this._shortName = aLocation;
  } else if (aLocation instanceof Ci.nsIFile) {
    file = aLocation;
  } else if (aLocation instanceof Ci.nsIURI) {
    switch (aLocation.scheme) {
      case "https":
      case "http":
      case "ftp":
      case "data":
      case "file":
      case "resource":
      case "chrome":
        uri = aLocation;
        break;
      default:
        ERROR("Invalid URI passed to the nsISearchEngine constructor",
              Cr.NS_ERROR_INVALID_ARG);
    }
  } else {
    ERROR("Engine location is neither a File nor a URI object",
          Cr.NS_ERROR_INVALID_ARG);
  }

  if (!this._shortName) {
    // If we don't have a shortName at this point, it's the first time we load
    // this engine, so let's generate the shortName, id and loadPath values.
    let shortName;
    if (file) {
      shortName = file.leafName;
    } else if (uri && uri instanceof Ci.nsIURL) {
      if (aIsReadOnly || (gEnvironment.get("XPCSHELL_TEST_PROFILE_DIR") &&
                          uri.scheme == "resource")) {
        shortName = uri.fileName;
      }
    }
    if (shortName && shortName.endsWith(".xml")) {
      this._shortName = shortName.slice(0, -4);
    }
    this._loadPath = this.getAnonymizedLoadPath(file, uri);

    if (!shortName && !aIsReadOnly) {
      // We are in the process of downloading and installing the engine.
      // We'll have the shortName and id once we are done parsing it.
     return;
    }

    // Build the id used for the legacy metadata storage, so that we
    // can do a one-time import of data from old profiles.
    if (this._isDefault ||
        (uri && uri.spec.startsWith(APP_SEARCH_PREFIX))) {
      // The second part of the check is to catch engines from language packs.
      // They aren't default engines (because they aren't app-shipped), but we
      // still need to give their id an [app] prefix for backward compat.
      this._id = "[app]/" + this._shortName + ".xml";
    } else if (!aIsReadOnly) {
      this._id = "[profile]/" + this._shortName + ".xml";
    } else {
      // If the engine is neither a default one, nor a user-installed one,
      // it must be extension-shipped, so use the full path as id.
      LOG("Setting _id to full path for engine from " + this._loadPath);
      this._id = file ? file.path : uri.spec;
    }
  }
}

Engine.prototype = {
  // Data set by the user.
  _metaData: null,
  // The data describing the engine, in the form of an XML document element.
  _data: null,
  // Whether or not the engine is readonly.
  _readOnly: true,
  // Anonymized path of where we initially loaded the engine from.
  // This will stay null for engines installed in the profile before we moved
  // to a JSON storage.
  _loadPath: null,
  // The engine's description
  _description: "",
  // Used to store the engine to replace, if we're an update to an existing
  // engine.
  _engineToUpdate: null,
  // Set to true if the engine has a preferred icon (an icon that should not be
  // overridden by a non-preferred icon).
  _hasPreferredIcon: null,
  // The engine's name.
  _name: null,
  // The name of the charset used to submit the search terms.
  _queryCharset: null,
  // The engine's raw SearchForm value (URL string pointing to a search form).
  __searchForm: null,
  get _searchForm() {
    return this.__searchForm;
  },
  set _searchForm(aValue) {
    if (/^https?:/i.test(aValue))
      this.__searchForm = aValue;
    else
      LOG("_searchForm: Invalid URL dropped for " + this._name ||
          "the current engine");
  },
  // Whether to obtain user confirmation before adding the engine. This is only
  // used when the engine is first added to the list.
  _confirm: false,
  // Whether to set this as the current engine as soon as it is loaded.  This
  // is only used when the engine is first added to the list.
  _useNow: false,
  // A function to be invoked when this engine object's addition completes (or
  // fails). Only used for installation via addEngine.
  _installCallback: null,
  // The number of days between update checks for new versions
  _updateInterval: null,
  // The url to check at for a new update
  _updateURL: null,
  // The url to check for a new icon
  _iconUpdateURL: null,
  /* The extension ID if added by an extension. */
  _extensionID: null,
  // If the extension is builtin we treat it as a builtin search engine as well.
  // Both System and Distribution extensions are considered builtin for search engines.
  _isBuiltinExtension: false,

  /**
   * Retrieves the data from the engine's file asynchronously.
   * The document element is placed in the engine's data field.
   *
   * @param file The file to load the search plugin from.
   *
   * @returns {Promise} A promise, resolved successfully if initializing from
   * data succeeds, rejected if it fails.
   */
  async _initFromFile(file) {
    if (!file || !(await OS.File.exists(file.path)))
      FAIL("File must exist before calling initFromFile!", Cr.NS_ERROR_UNEXPECTED);

    let fileURI = Services.io.newFileURI(file);
    await this._retrieveSearchXMLData(fileURI.spec);

    // Now that the data is loaded, initialize the engine object
    this._initFromData();
  },

  /**
   * Retrieves the engine data from a URI. Initializes the engine, flushes to
   * disk, and notifies the search service once initialization is complete.
   *
   * @param uri The uri to load the search plugin from.
   */
  _initFromURIAndLoad: function SRCH_ENG_initFromURIAndLoad(uri) {
    ENSURE_WARN(uri instanceof Ci.nsIURI,
                "Must have URI when calling _initFromURIAndLoad!",
                Cr.NS_ERROR_UNEXPECTED);

    LOG("_initFromURIAndLoad: Downloading engine from: \"" + uri.spec + "\".");

    var chan = makeChannel(uri);

    if (this._engineToUpdate && (chan instanceof Ci.nsIHttpChannel)) {
      var lastModified = this._engineToUpdate.getAttr("updatelastmodified");
      if (lastModified)
        chan.setRequestHeader("If-Modified-Since", lastModified, false);
    }
    this._uri = uri;
    var listener = new loadListener(chan, this, this._onLoad);
    chan.notificationCallbacks = listener;
    chan.asyncOpen(listener);
  },

  /**
   * Retrieves the engine data from a URI asynchronously and initializes it.
   *
   * @param uri The uri to load the search plugin from.
   *
   * @returns {Promise} A promise, resolved successfully if retrieveing data
   * succeeds.
   */
  async _initFromURI(uri) {
    LOG("_initFromURI: Loading engine from: \"" + uri.spec + "\".");
    await this._retrieveSearchXMLData(uri.spec);
    // Now that the data is loaded, initialize the engine object
    this._initFromData();
  },

  /**
   * Retrieves the engine data for a given URI asynchronously.
   *
   * @returns {Promise} A promise, resolved successfully if retrieveing data
   * succeeds.
   */
  _retrieveSearchXMLData: function SRCH_ENG__retrieveSearchXMLData(aURL) {
    return new Promise(resolve => {
      let request = new XMLHttpRequest();
      request.overrideMimeType("text/xml");
      request.onload = (aEvent) => {
        let responseXML = aEvent.target.responseXML;
        this._data = responseXML.documentElement;
        resolve();
      };
      request.onerror = function(aEvent) {
        resolve();
      };
      request.open("GET", aURL, true);
      request.send();
    });
  },

  /**
   * Attempts to find an EngineURL object in the set of EngineURLs for
   * this Engine that has the given type string.  (This corresponds to the
   * "type" attribute in the "Url" node in the OpenSearch spec.)
   * This method will return the first matching URL object found, or null
   * if no matching URL is found.
   *
   * @param aType string to match the EngineURL's type attribute
   * @param aRel [optional] only return URLs that with this rel value
   */
  _getURLOfType: function SRCH_ENG__getURLOfType(aType, aRel) {
    for (let url of this._urls) {
      if (url.type == aType && (!aRel || url._hasRelation(aRel)))
        return url;
    }

    return null;
  },

  _confirmAddEngine: function SRCH_SVC_confirmAddEngine() {
    var stringBundle = Services.strings.createBundle(SEARCH_BUNDLE);
    var titleMessage = stringBundle.GetStringFromName("addEngineConfirmTitle");

    // Display only the hostname portion of the URL.
    var dialogMessage =
        stringBundle.formatStringFromName("addEngineConfirmation",
                                          [this._name, this._uri.host], 2);
    var checkboxMessage = null;
    if (!Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "noCurrentEngine", false))
      checkboxMessage = stringBundle.GetStringFromName("addEngineAsCurrentText");

    var addButtonLabel =
        stringBundle.GetStringFromName("addEngineAddButtonLabel");

    var ps = Services.prompt;
    var buttonFlags = (ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0) +
                      (ps.BUTTON_TITLE_CANCEL * ps.BUTTON_POS_1) +
                       ps.BUTTON_POS_0_DEFAULT;

    var checked = {value: false};
    // confirmEx returns the index of the button that was pressed.  Since "Add"
    // is button 0, we want to return the negation of that value.
    var confirm = !ps.confirmEx(null,
                                titleMessage,
                                dialogMessage,
                                buttonFlags,
                                addButtonLabel,
                                null, null, // button 1 & 2 names not used
                                checkboxMessage,
                                checked);

    return {confirmed: confirm, useNow: checked.value};
  },

  /**
   * Handle the successful download of an engine. Initializes the engine and
   * triggers parsing of the data. The engine is then flushed to disk. Notifies
   * the search service once initialization is complete.
   */
  _onLoad: function SRCH_ENG_onLoad(aBytes, aEngine) {
    /**
     * Handle an error during the load of an engine by notifying the engine's
     * error callback, if any.
     */
    function onError(errorCode = Ci.nsISearchService.ERROR_UNKNOWN_FAILURE) {
      // Notify the callback of the failure
      if (aEngine._installCallback) {
        aEngine._installCallback(errorCode);
      }
    }

    function promptError(strings = {}, error = undefined) {
      onError(error);

      if (aEngine._engineToUpdate) {
        // We're in an update, so just fail quietly
        LOG("updating " + aEngine._engineToUpdate.name + " failed");
        return;
      }
      var brandBundle = Services.strings.createBundle(BRAND_BUNDLE);
      var brandName = brandBundle.GetStringFromName("brandShortName");

      var searchBundle = Services.strings.createBundle(SEARCH_BUNDLE);
      var msgStringName = strings.error || "error_loading_engine_msg2";
      var titleStringName = strings.title || "error_loading_engine_title";
      var title = searchBundle.GetStringFromName(titleStringName);
      var text = searchBundle.formatStringFromName(msgStringName,
                                                   [brandName, aEngine._location],
                                                   2);

      Services.ww.getNewPrompter(null).alert(title, text);
    }

    if (!aBytes) {
      promptError();
      return;
    }

    var parser = new DOMParser();
    var doc = parser.parseFromBuffer(aBytes, "text/xml");
    aEngine._data = doc.documentElement;

    try {
      // Initialize the engine from the obtained data
      aEngine._initFromData();
    } catch (ex) {
      LOG("_onLoad: Failed to init engine!\n" + ex);
      // Report an error to the user
      if (ex.result == Cr.NS_ERROR_FILE_CORRUPTED) {
        promptError({ error: "error_invalid_engine_msg2",
                      title: "error_invalid_format_title",
                    });
      } else {
        promptError();
      }
      return;
    }

    if (aEngine._engineToUpdate) {
      let engineToUpdate = aEngine._engineToUpdate.wrappedJSObject;

      // Make this new engine use the old engine's shortName, and preserve
      // metadata.
      aEngine._shortName = engineToUpdate._shortName;
      Object.keys(engineToUpdate._metaData).forEach(key => {
        aEngine.setAttr(key, engineToUpdate.getAttr(key));
      });
      aEngine._loadPath = engineToUpdate._loadPath;

      // Keep track of the last modified date, so that we can make conditional
      // requests for future updates.
      aEngine.setAttr("updatelastmodified", (new Date()).toUTCString());

      // Set the new engine's icon, if it doesn't yet have one.
      if (!aEngine._iconURI && engineToUpdate._iconURI)
        aEngine._iconURI = engineToUpdate._iconURI;
    } else {
      // Check that when adding a new engine (e.g., not updating an
      // existing one), a duplicate engine does not already exist.
      if (Services.search.getEngineByName(aEngine.name)) {
        // If we're confirming the engine load, then display a "this is a
        // duplicate engine" prompt; otherwise, fail silently.
        if (aEngine._confirm) {
          promptError({ error: "error_duplicate_engine_msg",
                        title: "error_invalid_engine_title",
                      }, Ci.nsISearchService.ERROR_DUPLICATE_ENGINE);
        } else {
          onError(Ci.nsISearchService.ERROR_DUPLICATE_ENGINE);
        }
        LOG("_onLoad: duplicate engine found, bailing");
        return;
      }

      // If requested, confirm the addition now that we have the title.
      // This property is only ever true for engines added via
      // nsISearchService::addEngine.
      if (aEngine._confirm) {
        var confirmation = aEngine._confirmAddEngine();
        LOG("_onLoad: confirm is " + confirmation.confirmed +
            "; useNow is " + confirmation.useNow);
        if (!confirmation.confirmed) {
          onError();
          return;
        }
        aEngine._useNow = confirmation.useNow;
      }

      aEngine._shortName = sanitizeName(aEngine.name);
      aEngine._loadPath = aEngine.getAnonymizedLoadPath(null, aEngine._uri);
      if (aEngine._extensionID) {
        aEngine._loadPath += ":" + aEngine._extensionID;
      }
      aEngine.setAttr("loadPathHash", getVerificationHash(aEngine._loadPath));
    }

    // Notify the search service of the successful load. It will deal with
    // updates by checking aEngine._engineToUpdate.
    notifyAction(aEngine, SEARCH_ENGINE_LOADED);

    // Notify the callback if needed
    if (aEngine._installCallback) {
      aEngine._installCallback();
    }
  },

  /**
   * Creates a key by serializing an object that contains the icon's width
   * and height.
   *
   * @param aWidth
   *        Width of the icon.
   * @param aHeight
   *        Height of the icon.
   * @returns key string
   */
  _getIconKey: function SRCH_ENG_getIconKey(aWidth, aHeight) {
    let keyObj = {
     width: aWidth,
     height: aHeight,
    };

    return JSON.stringify(keyObj);
  },

  /**
   * Add an icon to the icon map used by getIconURIBySize() and getIcons().
   *
   * @param aWidth
   *        Width of the icon.
   * @param aHeight
   *        Height of the icon.
   * @param aURISpec
   *        String with the icon's URI.
   */
  _addIconToMap: function SRCH_ENG_addIconToMap(aWidth, aHeight, aURISpec) {
    if (aWidth == 16 && aHeight == 16) {
      // The 16x16 icon is stored in _iconURL, we don't need to store it twice.
      return;
    }

    // Use an object instead of a Map() because it needs to be serializable.
    this._iconMapObj = this._iconMapObj || {};
    let key = this._getIconKey(aWidth, aHeight);
    this._iconMapObj[key] = aURISpec;
  },

  /**
   * Sets the .iconURI property of the engine. If both aWidth and aHeight are
   * provided an entry will be added to _iconMapObj that will enable accessing
   * icon's data through getIcons() and getIconURIBySize() APIs.
   *
   *  @param aIconURL
   *         A URI string pointing to the engine's icon. Must have a http[s],
   *         ftp, or data scheme. Icons with HTTP[S] or FTP schemes will be
   *         downloaded and converted to data URIs for storage in the engine
   *         XML files, if the engine is not readonly.
   *  @param aIsPreferred
   *         Whether or not this icon is to be preferred. Preferred icons can
   *         override non-preferred icons.
   *  @param aWidth (optional)
   *         Width of the icon.
   *  @param aHeight (optional)
   *         Height of the icon.
   */
  _setIcon: function SRCH_ENG_setIcon(aIconURL, aIsPreferred, aWidth, aHeight) {
    var uri = makeURI(aIconURL);

    // Ignore bad URIs
    if (!uri)
      return;

    LOG("_setIcon: Setting icon url \"" + limitURILength(uri.spec) + "\" for engine \""
        + this.name + "\".");
    // Only accept remote icons from http[s] or ftp
    switch (uri.scheme) {
      case "resource":
      case "chrome":
        // We only allow chrome and resource icon URLs for built-in search engines
        if (!this._isDefault) {
          return;
        }
        // Fall through to the data case
      case "moz-extension":
      case "data":
        if (!this._hasPreferredIcon || aIsPreferred) {
          this._iconURI = uri;
          notifyAction(this, SEARCH_ENGINE_CHANGED);
          this._hasPreferredIcon = aIsPreferred;
        }

        if (aWidth && aHeight) {
          this._addIconToMap(aWidth, aHeight, aIconURL);
        }
        break;
      case "http":
      case "https":
      case "ftp":
        LOG("_setIcon: Downloading icon: \"" + uri.spec +
            "\" for engine: \"" + this.name + "\"");
        var chan = makeChannel(uri);

        let iconLoadCallback = function(aByteArray, aEngine) {
          // This callback may run after we've already set a preferred icon,
          // so check again.
          if (aEngine._hasPreferredIcon && !aIsPreferred)
            return;

          if (!aByteArray) {
            LOG("iconLoadCallback: load failed");
            return;
          }

          let contentType = chan.contentType;
          if (aByteArray.length > MAX_ICON_SIZE) {
            try {
              LOG("iconLoadCallback: rescaling icon");
              [aByteArray, contentType] = rescaleIcon(aByteArray, contentType);
            } catch (ex) {
              LOG("iconLoadCallback: got exception: " + ex);
              Cu.reportError("Unable to set an icon for the search engine because: " + ex);
              return;
            }
          }

          if (!contentType.startsWith("image/"))
            contentType = "image/x-icon";
          let dataURL = "data:" + contentType + ";base64," +
            btoa(String.fromCharCode.apply(null, aByteArray));

          aEngine._iconURI = makeURI(dataURL);

          if (aWidth && aHeight) {
            aEngine._addIconToMap(aWidth, aHeight, dataURL);
          }

          notifyAction(aEngine, SEARCH_ENGINE_CHANGED);
          aEngine._hasPreferredIcon = aIsPreferred;
        };

        // If we're currently acting as an "update engine", then the callback
        // should set the icon on the engine we're updating and not us, since
        // |this| might be gone by the time the callback runs.
        var engineToSet = this._engineToUpdate || this;

        var listener = new loadListener(chan, engineToSet, iconLoadCallback);
        chan.notificationCallbacks = listener;
        chan.asyncOpen(listener);
        break;
    }
  },

  /**
   * Initialize this Engine object from the collected data.
   */
  _initFromData: function SRCH_ENG_initFromData() {
    ENSURE_WARN(this._data, "Can't init an engine with no data!",
                Cr.NS_ERROR_UNEXPECTED);

    // Ensure we have a supported engine type before attempting to parse it.
    let element = this._data;
    if ((element.localName == MOZSEARCH_LOCALNAME &&
         element.namespaceURI == MOZSEARCH_NS_10) ||
        (element.localName == OPENSEARCH_LOCALNAME &&
         OPENSEARCH_NAMESPACES.includes(element.namespaceURI))) {
      LOG("_init: Initing search plugin from " + this._location);

      this._parse();
    } else {
      Cu.reportError("Invalid search plugin due to namespace not matching.");
      FAIL(this._location + " is not a valid search plugin.", Cr.NS_ERROR_FILE_CORRUPTED);
    }
    // No need to keep a ref to our data (which in some cases can be a document
    // element) past this point
    this._data = null;
  },

  /**
   * Initialize an EngineURL object from metadata.
   */
  _initEngineURLFromMetaData(aType, aParams) {
    let url = new EngineURL(aType, aParams.method || "GET", aParams.template);

    if (aParams.postParams) {
      let queries = new URLSearchParams(aParams.postParams);
      for (let [name, value] of queries) {
        url.addParam(name, value);
      }
    }

    if (aParams.mozParams) {
      for (let p of aParams.mozParams) {
        if ((p.condition || p.purpose) && !this._isDefault) {
          continue;
        }
        if (p.condition == "pref") {
          let value = getMozParamPref(p.pref);
          url.addParam(p.name, value);
          url._addMozParam(p);
        } else {
          url.addParam(p.name, p.value, p.purpose || undefined);
        }
      }
    }

    this._urls.push(url);
    return url;
  },

  /**
   * Initialize this Engine object from a collection of metadata.
   */
  _initFromMetadata: function SRCH_ENG_initMetaData(aName, aParams) {
    ENSURE_WARN(!this._readOnly,
                "Can't call _initFromMetaData on a readonly engine!",
                Cr.NS_ERROR_FAILURE);

    this._extensionID = aParams.extensionID;
    this._isBuiltinExtension = !!aParams.isBuiltIn;

    this._initEngineURLFromMetaData(URLTYPE_SEARCH_HTML, {
      method: (aParams.searchPostParams && "POST") || aParams.method || "GET",
      template: aParams.template,
      postParams: aParams.searchPostParams,
      mozParams: aParams.mozParams,
    });

    if (aParams.suggestURL) {
      this._initEngineURLFromMetaData(URLTYPE_SUGGEST_JSON, {
        method: (aParams.suggestPostParams && "POST") || aParams.method || "GET",
        template: aParams.suggestURL,
        postParams: aParams.suggestPostParams,
      });
    }

    if (aParams.queryCharset) {
      this._queryCharset = aParams.queryCharset;
    }
    if (aParams.postData) {
      let queries = new URLSearchParams(aParams.postData);
      for (let [name, value] of queries) {
        this.addParam(name, value);
      }
    }

    this._name = aName;
    this.alias = aParams.alias;
    this._description = aParams.description;
    if (aParams.iconURL) {
      this._setIcon(aParams.iconURL, true);
    }
    // Other sizes
    if (aParams.icons) {
      for (let icon of aParams.icons) {
        this._addIconToMap(icon.size, icon.size, icon.url);
      }
    }
  },

  /**
   * Extracts data from an OpenSearch URL element and creates an EngineURL
   * object which is then added to the engine's list of URLs.
   *
   * @throws NS_ERROR_FAILURE if a URL object could not be created.
   *
   * @see http://opensearch.a9.com/spec/1.1/querysyntax/#urltag.
   * @see EngineURL()
   */
  _parseURL: function SRCH_ENG_parseURL(aElement) {
    var type     = aElement.getAttribute("type");
    // According to the spec, method is optional, defaulting to "GET" if not
    // specified
    var method   = aElement.getAttribute("method") || "GET";
    var template = aElement.getAttribute("template");
    var resultDomain = aElement.getAttribute("resultdomain");

    let rels = [];
    if (aElement.hasAttribute("rel")) {
      rels = aElement.getAttribute("rel").toLowerCase().split(/\s+/);
    }

    // Support an alternate suggestion type, see bug 1425827 for details.
    if (type == "application/json" && rels.includes("suggestions")) {
      type = URLTYPE_SUGGEST_JSON;
    }

    try {
      var url = new EngineURL(type, method, template, resultDomain);
    } catch (ex) {
      FAIL("_parseURL: failed to add " + template + " as a URL",
           Cr.NS_ERROR_FAILURE);
    }

    if (rels.length) {
      url.rels = rels;
    }

    for (var i = 0; i < aElement.children.length; ++i) {
      var param = aElement.children[i];
      if (param.localName == "Param") {
        try {
          url.addParam(param.getAttribute("name"), param.getAttribute("value"));
        } catch (ex) {
          // Ignore failure
          LOG("_parseURL: Url element has an invalid param");
        }
      } else if (param.localName == "MozParam" &&
                 // We only support MozParams for default search engines
                 this._isDefault) {
        var value;
        let condition = param.getAttribute("condition");

        // MozParams must have a condition to be valid
        if (!condition) {
          let engineLoc = this._location;
          let paramName = param.getAttribute("name");
          LOG("_parseURL: MozParam (" + paramName +
            ") without a condition attribute found parsing engine: " + engineLoc);
          continue;
        }

        switch (condition) {
          case "purpose":
            url.addParam(param.getAttribute("name"),
                         param.getAttribute("value"),
                         param.getAttribute("purpose"));
            // _addMozParam is not needed here since it can be serialized fine without. _addMozParam
            // also requires a unique "name" which is not normally the case when @purpose is used.
            break;
          case "pref":
            try {
              value = getMozParamPref(param.getAttribute("pref"), value);
              url.addParam(param.getAttribute("name"), value);
              url._addMozParam({"pref": param.getAttribute("pref"),
                                "name": param.getAttribute("name"),
                                "condition": "pref"});
            } catch (e) { }
            break;
          default:
            let engineLoc = this._location;
            let paramName = param.getAttribute("name");
            LOG("_parseURL: MozParam (" + paramName + ") has an unknown condition: " +
              condition + ". Found parsing engine: " + engineLoc);
          break;
        }
      }
    }

    this._urls.push(url);
  },

  /**
   * Get the icon from an OpenSearch Image element.
   * @see http://opensearch.a9.com/spec/1.1/description/#image
   */
  _parseImage: function SRCH_ENG_parseImage(aElement) {
    LOG("_parseImage: Image textContent: \"" + limitURILength(aElement.textContent) + "\"");

    let width = parseInt(aElement.getAttribute("width"), 10);
    let height = parseInt(aElement.getAttribute("height"), 10);
    let isPrefered = width == 16 && height == 16;

    if (isNaN(width) || isNaN(height) || width <= 0 || height <= 0) {
      LOG("OpenSearch image element must have positive width and height.");
      return;
    }

    this._setIcon(aElement.textContent, isPrefered, width, height);
  },

  /**
   * Extract search engine information from the collected data to initialize
   * the engine object.
   */
  _parse: function SRCH_ENG_parse() {
    var doc = this._data;

    // The OpenSearch spec sets a default value for the input encoding.
    this._queryCharset = OS_PARAM_INPUT_ENCODING_DEF;

    for (var i = 0; i < doc.children.length; ++i) {
      var child = doc.children[i];
      switch (child.localName) {
        case "ShortName":
          this._name = child.textContent;
          break;
        case "Description":
          this._description = child.textContent;
          break;
        case "Url":
          try {
            this._parseURL(child);
          } catch (ex) {
            // Parsing of the element failed, just skip it.
            LOG("_parse: failed to parse URL child: " + ex);
          }
          break;
        case "Image":
          this._parseImage(child);
          break;
        case "InputEncoding":
          this._queryCharset = child.textContent.toUpperCase();
          break;

        // Non-OpenSearch elements
        case "SearchForm":
          this._searchForm = child.textContent;
          break;
        case "UpdateUrl":
          this._updateURL = child.textContent;
          break;
        case "UpdateInterval":
          this._updateInterval = parseInt(child.textContent);
          break;
        case "IconUpdateUrl":
          this._iconUpdateURL = child.textContent;
          break;
        case "ExtensionID":
          this._extensionID = child.textContent;
          break;
      }
    }
    if (!this.name || (this._urls.length == 0))
      FAIL("_parse: No name, or missing URL!", Cr.NS_ERROR_FAILURE);
    if (!this.supportsResponseType(URLTYPE_SEARCH_HTML))
      FAIL("_parse: No text/html result type!", Cr.NS_ERROR_FAILURE);
  },

  /**
   * Init from a JSON record.
   **/
  _initWithJSON: function SRCH_ENG__initWithJSON(aJson) {
    this._name = aJson._name;
    this._shortName = aJson._shortName;
    this._loadPath = aJson._loadPath;
    this._description = aJson.description;
    this._hasPreferredIcon = aJson._hasPreferredIcon == undefined;
    this._queryCharset = aJson.queryCharset || DEFAULT_QUERY_CHARSET;
    this.__searchForm = aJson.__searchForm;
    this._updateInterval = aJson._updateInterval || null;
    this._updateURL = aJson._updateURL || null;
    this._iconUpdateURL = aJson._iconUpdateURL || null;
    this._readOnly = aJson._readOnly == undefined;
    this._iconURI = makeURI(aJson._iconURL);
    this._iconMapObj = aJson._iconMapObj;
    this._metaData = aJson._metaData || {};
    if (aJson.filePath) {
      this._filePath = aJson.filePath;
    }
    if (aJson.extensionID) {
      this._extensionID = aJson.extensionID;
    }
    for (let i = 0; i < aJson._urls.length; ++i) {
      let url = aJson._urls[i];
      let engineURL = new EngineURL(url.type || URLTYPE_SEARCH_HTML,
                                    url.method || "GET", url.template,
                                    url.resultDomain || undefined);
      engineURL._initWithJSON(url, this);
      this._urls.push(engineURL);
    }
  },

  /**
   * Creates a JavaScript object that represents this engine.
   * @returns An object suitable for serialization as JSON.
   **/
  toJSON: function SRCH_ENG_toJSON() {
    var json = {
      _name: this._name,
      _shortName: this._shortName,
      _loadPath: this._loadPath,
      description: this.description,
      __searchForm: this.__searchForm,
      _iconURL: this._iconURL,
      _iconMapObj: this._iconMapObj,
      _metaData: this._metaData,
      _urls: this._urls,
    };

    if (this._updateInterval)
      json._updateInterval = this._updateInterval;
    if (this._updateURL)
      json._updateURL = this._updateURL;
    if (this._iconUpdateURL)
      json._iconUpdateURL = this._iconUpdateURL;
    if (!this._hasPreferredIcon)
      json._hasPreferredIcon = this._hasPreferredIcon;
    if (this.queryCharset != DEFAULT_QUERY_CHARSET)
      json.queryCharset = this.queryCharset;
    if (!this._readOnly)
      json._readOnly = this._readOnly;
    if (this._filePath) {
      // File path is stored so that we can remove legacy xml files
      // from the profile if the user removes the engine.
      json.filePath = this._filePath;
    }
    if (this._extensionID) {
      json.extensionID = this._extensionID;
    }

    return json;
  },

  setAttr(name, val) {
    this._metaData[name] = val;
  },

  getAttr(name) {
    return this._metaData[name] || undefined;
  },

  // nsISearchEngine
  get alias() {
    return this.getAttr("alias");
  },
  set alias(val) {
    var value = val ? val.trim() : null;
    this.setAttr("alias", value);
    notifyAction(this, SEARCH_ENGINE_CHANGED);
  },

  /**
   * Return the built-in identifier of app-provided engines.
   *
   * Note that this identifier is substantially similar to _id, with the
   * following exceptions:
   *
   * * There is no trailing file extension.
   * * There is no [app] prefix.
   *
   * @return a string identifier, or null.
   */
  get identifier() {
    // No identifier if If the engine isn't app-provided
    return this._isDefault ? this._shortName : null;
  },

  get description() {
    return this._description;
  },

  get hidden() {
    return this.getAttr("hidden") || false;
  },
  set hidden(val) {
    var value = !!val;
    if (value != this.hidden) {
      this.setAttr("hidden", value);
      notifyAction(this, SEARCH_ENGINE_CHANGED);
    }
  },

  get iconURI() {
    if (this._iconURI)
      return this._iconURI;
    return null;
  },

  get _iconURL() {
    if (!this._iconURI)
      return "";
    return this._iconURI.spec;
  },

  // Where the engine is being loaded from: will return the URI's spec if the
  // engine is being downloaded and does not yet have a file. This is only used
  // for logging and error messages.
  get _location() {
    if (this._uri)
      return this._uri.spec;

    return this._loadPath;
  },

  // This indicates where we found the .xml file to load the engine,
  // and attempts to hide user-identifiable data (such as username).
  getAnonymizedLoadPath(file, uri) {
    /* Examples of expected output:
     *   jar:[app]/omni.ja!browser/engine.xml
     *     'browser' here is the name of the chrome package, not a folder.
     *   [profile]/searchplugins/engine.xml
     *   [distribution]/searchplugins/common/engine.xml
     *   [other]/engine.xml
     */

    const NS_XPCOM_CURRENT_PROCESS_DIR = "XCurProcD";
    const NS_APP_USER_PROFILE_50_DIR = "ProfD";
    const XRE_APP_DISTRIBUTION_DIR = "XREAppDist";

    const knownDirs = {
      app: NS_XPCOM_CURRENT_PROCESS_DIR,
      profile: NS_APP_USER_PROFILE_50_DIR,
      distribution: XRE_APP_DISTRIBUTION_DIR,
    };

    let leafName = this._shortName;
    if (!leafName)
      return "null";
    leafName += ".xml";

    let prefix = "", suffix = "";
    if (!file) {
      if (uri.schemeIs("resource")) {
        uri = makeURI(Services.io.getProtocolHandler("resource")
                              .QueryInterface(Ci.nsISubstitutingProtocolHandler)
                              .resolveURI(uri));
      }
      let scheme = uri.scheme;
      let packageName = "";
      if (scheme == "chrome") {
        packageName = uri.hostPort;
        uri = gChromeReg.convertChromeURL(uri);
      }

      if (AppConstants.platform == "android") {
        // On Android the omni.ja file isn't at the same path as the binary
        // used to start the process. We tweak the path here so that the code
        // shared with Desktop will correctly identify files from the omni.ja
        // file as coming from the [app] folder.
        let appPath = Services.io.getProtocolHandler("resource")
                              .QueryInterface(Ci.nsIResProtocolHandler)
                              .getSubstitution("android");
        if (appPath) {
          appPath = appPath.spec;
          let spec = uri.spec;
          if (spec.includes(appPath)) {
            let appURI = Services.io.newFileURI(getDir(knownDirs.app));
            uri = Services.io.newURI(spec.replace(appPath, appURI.spec));
          }
        }
      }

      if (uri instanceof Ci.nsINestedURI) {
        prefix = "jar:";
        suffix = "!" + packageName + "/" + leafName;
        uri = uri.innermostURI;
      }
      if (uri instanceof Ci.nsIFileURL) {
        file = uri.file;
      } else {
        let path = "[" + scheme + "]";
        if (/^(?:https?|ftp)$/.test(scheme)) {
          path += uri.host;
        }
        return path + "/" + leafName;
      }
    }

    let id;
    let enginePath = file.path;

    for (let key in knownDirs) {
      let path;
      try {
        path = getDir(knownDirs[key]).path;
      } catch (e) {
        // Getting XRE_APP_DISTRIBUTION_DIR throws during unit tests.
        continue;
      }
      if (enginePath.startsWith(path)) {
        id = "[" + key + "]" + enginePath.slice(path.length).replace(/\\/g, "/");
        break;
      }
    }

    // If the folder doesn't have a known ancestor, don't record its path to
    // avoid leaking user identifiable data.
    if (!id)
      id = "[other]/" + file.leafName;

    return prefix + id + suffix;
  },

  get _isDefault() {
    if (this._extensionID) {
      return this._isBuiltinExtension;
    }

    // If we don't have a shortName, the engine is being parsed from a
    // downloaded file, so this can't be a default engine.
    if (!this._shortName)
      return false;

    // An engine is a default one if we initially loaded it from the application
    // or distribution directory.
    if (/^(?:jar:)?(?:\[app\]|\[distribution\])/.test(this._loadPath))
      return true;

    let uri = makeURI(APP_SEARCH_PREFIX + this._shortName + ".xml");
    if (this.getAnonymizedLoadPath(null, uri) == this._loadPath) {
      // This isn't a real default engine, but it's very close.
      LOG("_isDefault, pretending " + this._loadPath + " is a default engine");
      return true;
    }

    return false;
  },

  get _hasUpdates() {
    // Whether or not the engine has an update URL
    let selfURL = this._getURLOfType(URLTYPE_OPENSEARCH, "self");
    return !!(this._updateURL || this._iconUpdateURL || selfURL);
  },

  get name() {
    return this._name;
  },

  get searchForm() {
    return this._getSearchFormWithPurpose();
  },

  /* Internal aliases for default engines only. */
  __internalAliases: null,
  get _internalAliases() {
    if (!this.__internalAliases) {
      this.__internalAliases = getInternalAliases(this);
    }
    return this.__internalAliases;
  },

  _getSearchFormWithPurpose(aPurpose = "") {
    // First look for a <Url rel="searchform">
    var searchFormURL = this._getURLOfType(URLTYPE_SEARCH_HTML, "searchform");
    if (searchFormURL) {
      let submission = searchFormURL.getSubmission("", this, aPurpose);

      // If the rel=searchform URL is not type="get" (i.e. has postData),
      // ignore it, since we can only return a URL.
      if (!submission.postData)
        return submission.uri.spec;
    }

    if (!this._searchForm) {
      // No SearchForm specified in the engine definition file, use the prePath
      // (e.g. https://foo.com for https://foo.com/search.php?q=bar).
      var htmlUrl = this._getURLOfType(URLTYPE_SEARCH_HTML);
      ENSURE_WARN(htmlUrl, "Engine has no HTML URL!", Cr.NS_ERROR_UNEXPECTED);
      this._searchForm = makeURI(htmlUrl.template).prePath;
    }

    return ParamSubstitution(this._searchForm, "", this);
  },

  get queryCharset() {
    if (this._queryCharset)
      return this._queryCharset;
    return this._queryCharset = "windows-1252"; // the default
  },

  // from nsISearchEngine
  addParam: function SRCH_ENG_addParam(aName, aValue, aResponseType) {
    if (!aName || (aValue == null))
      FAIL("missing name or value for nsISearchEngine::addParam!");
    ENSURE_WARN(!this._readOnly,
                "called nsISearchEngine::addParam on a read-only engine!",
                Cr.NS_ERROR_FAILURE);
    if (!aResponseType)
      aResponseType = URLTYPE_SEARCH_HTML;

    var url = this._getURLOfType(aResponseType);
    if (!url)
      FAIL("Engine object has no URL for response type " + aResponseType,
           Cr.NS_ERROR_FAILURE);

    url.addParam(aName, aValue);
  },

  get _defaultMobileResponseType() {
    let type = URLTYPE_SEARCH_HTML;

    let isTablet = Services.sysinfo.get("tablet");
    if (isTablet && this.supportsResponseType("application/x-moz-tabletsearch")) {
      // Check for a tablet-specific search URL override
      type = "application/x-moz-tabletsearch";
    } else if (!isTablet && this.supportsResponseType("application/x-moz-phonesearch")) {
      // Check for a phone-specific search URL override
      type = "application/x-moz-phonesearch";
    }

    Object.defineProperty(this, "_defaultMobileResponseType", {
      value: type,
      configurable: true,
    });

    return type;
  },

  get _isWhiteListed() {
    let url = this._getURLOfType(URLTYPE_SEARCH_HTML).template;
    let hostname = makeURI(url).host;
    let whitelist = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF)
                            .getCharPref("reset.whitelist")
                            .split(",");
    if (whitelist.includes(hostname)) {
      LOG("The hostname " + hostname + " is white listed, " +
          "we won't show the search reset prompt");
      return true;
    }

    return false;
  },

  // from nsISearchEngine
  getSubmission: function SRCH_ENG_getSubmission(aData, aResponseType, aPurpose) {
    if (!aResponseType) {
      aResponseType = AppConstants.platform == "android" ? this._defaultMobileResponseType :
                                                           URLTYPE_SEARCH_HTML;
    }

    var url = this._getURLOfType(aResponseType);

    if (!url)
      return null;

    if (!aData) {
      // Return a dummy submission object with our searchForm attribute
      return new Submission(makeURI(this._getSearchFormWithPurpose(aPurpose)));
    }

    LOG("getSubmission: In data: \"" + aData + "\"; Purpose: \"" + aPurpose + "\"");
    var data = "";
    try {
      data = Services.textToSubURI.ConvertAndEscape(this.queryCharset, aData);
    } catch (ex) {
      LOG("getSubmission: Falling back to default queryCharset!");
      data = Services.textToSubURI.ConvertAndEscape(DEFAULT_QUERY_CHARSET, aData);
    }
    LOG("getSubmission: Out data: \"" + data + "\"");
    return url.getSubmission(data, this, aPurpose);
  },

  // from nsISearchEngine
  supportsResponseType: function SRCH_ENG_supportsResponseType(type) {
    return (this._getURLOfType(type) != null);
  },

  // from nsISearchEngine
  getResultDomain: function SRCH_ENG_getResultDomain(aResponseType) {
    if (!aResponseType) {
      aResponseType = AppConstants.platform == "android" ? this._defaultMobileResponseType :
                                                           URLTYPE_SEARCH_HTML;
    }

    LOG("getResultDomain: responseType: \"" + aResponseType + "\"");

    let url = this._getURLOfType(aResponseType);
    if (url)
      return url.resultDomain;
    return "";
  },

  /**
   * Returns URL parsing properties used by _buildParseSubmissionMap.
   */
  getURLParsingInfo() {
    let responseType = AppConstants.platform == "android" ? this._defaultMobileResponseType :
                                                            URLTYPE_SEARCH_HTML;

    let url = this._getURLOfType(responseType);
    if (!url || url.method != "GET") {
      return null;
    }

    let termsParameterName = url._getTermsParameterName();
    if (!termsParameterName) {
      return null;
    }

    let templateUrl = Services.io.newURI(url.template).QueryInterface(Ci.nsIURL);
    return {
      mainDomain: templateUrl.host,
      path: templateUrl.filePath.toLowerCase(),
      termsParameterName,
    };
  },

  // nsISupports
  QueryInterface: ChromeUtils.generateQI([Ci.nsISearchEngine]),

  get wrappedJSObject() {
    return this;
  },

  /**
   * Returns a string with the URL to an engine's icon matching both width and
   * height. Returns null if icon with specified dimensions is not found.
   *
   * @param width
   *        Width of the requested icon.
   * @param height
   *        Height of the requested icon.
   */
  getIconURLBySize: function SRCH_ENG_getIconURLBySize(aWidth, aHeight) {
    if (aWidth == 16 && aHeight == 16)
      return this._iconURL;

    if (!this._iconMapObj)
      return null;

    let key = this._getIconKey(aWidth, aHeight);
    if (key in this._iconMapObj) {
      return this._iconMapObj[key];
    }
    return null;
  },

  /**
   * Gets an array of all available icons. Each entry is an object with
   * width, height and url properties. width and height are numeric and
   * represent the icon's dimensions. url is a string with the URL for
   * the icon.
   */
  getIcons: function SRCH_ENG_getIcons() {
    let result = [];
    if (this._iconURL)
      result.push({width: 16, height: 16, url: this._iconURL});

    if (!this._iconMapObj)
      return result;

    for (let key of Object.keys(this._iconMapObj)) {
      let iconSize = JSON.parse(key);
      result.push({
        width: iconSize.width,
        height: iconSize.height,
        url: this._iconMapObj[key],
      });
    }

    return result;
  },

  /**
   * Opens a speculative connection to the engine's search URI
   * (and suggest URI, if different) to reduce request latency
   *
   * @param  options
   *         An object that must contain the following fields:
   *         {window} the content window for the window performing the search
   *         {originAttributes} the originAttributes for performing the search
   *
   * @throws NS_ERROR_INVALID_ARG if options is omitted or lacks required
   *         elemeents
   */
  speculativeConnect: function SRCH_ENG_speculativeConnect(options) {
    if (!options || !options.window) {
      Cu.reportError("invalid options arg passed to nsISearchEngine.speculativeConnect");
      throw Cr.NS_ERROR_INVALID_ARG;
    }
    let connector =
        Services.io.QueryInterface(Ci.nsISpeculativeConnect);

    let searchURI = this.getSubmission("dummy").uri;

    let callbacks = options.window.docShell
                           .QueryInterface(Ci.nsILoadContext);

    // Using the codebase principal which is constructed by the search URI
    // and given originAttributes. If originAttributes are not given, we
    // fallback to use the docShell's originAttributes.
    let attrs = options.originAttributes;

    if (!attrs) {
      attrs = options.window.docShell.getOriginAttributes();
    }

    let principal = Services.scriptSecurityManager
                            .createCodebasePrincipal(searchURI, attrs);

    connector.speculativeConnect(searchURI, principal, callbacks);

    if (this.supportsResponseType(URLTYPE_SUGGEST_JSON)) {
      let suggestURI = this.getSubmission("dummy", URLTYPE_SUGGEST_JSON).uri;
      if (suggestURI.prePath != searchURI.prePath)
        connector.speculativeConnect(suggestURI, principal, callbacks);
    }
  },
};

// nsISearchSubmission
function Submission(aURI, aPostData = null) {
  this._uri = aURI;
  this._postData = aPostData;
}
Submission.prototype = {
  get uri() {
    return this._uri;
  },
  get postData() {
    return this._postData;
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsISearchSubmission]),
};

// nsISearchParseSubmissionResult
function ParseSubmissionResult(aEngine, aTerms, aTermsOffset, aTermsLength) {
  this._engine = aEngine;
  this._terms = aTerms;
  this._termsOffset = aTermsOffset;
  this._termsLength = aTermsLength;
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

const gEmptyParseSubmissionResult =
      Object.freeze(new ParseSubmissionResult(null, "", -1, 0));

// nsISearchService
function SearchService() {
  this._initObservers = PromiseUtils.defer();
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

  // If initialization has not been completed yet, perform synchronous
  // initialization.
  // Throws in case of initialization error.
  _ensureInitialized: function SRCH_SVC__ensureInitialized() {
    if (gInitialized) {
      if (!Components.isSuccessCode(this._initRV)) {
        LOG("_ensureInitialized: failure");
        throw this._initRV;
      }
      return;
    }

    let err = new Error("Something tried to use the search service before it's been " +
      "properly intialized. Please examine the stack trace to figure out what and " +
      "where to fix it:\n");
    err.message += err.stack;
    throw err;
  },

  /**
   * Asynchronous implementation of the initializer.
   *
   * @param   [optional] skipRegionCheck
   *          A boolean value indicating whether we should explicitly await the
   *          the region check process to complete, which may be fetched remotely.
   *          Pass in `false` if the caller needs to be absolutely certain of the
   *          correct default engine and/ or ordering of visible engines.
   * @returns {Promise} A promise, resolved successfully if the initialization
   * succeeds.
   */
  async _init(skipRegionCheck) {
    LOG("_init start");

    // See if we have a cache file so we don't have to parse a bunch of XML.
    let cache = await this._readCacheFile();

    // The init flow is not going to block on a fetch from an external service,
    // but we're kicking it off as soon as possible to prevent UI flickering as
    // much as possible.
    this._ensureKnownRegionPromise = ensureKnownRegion(this)
      .catch(ex => LOG("_init: failure determining region: " + ex))
      .finally(() => this._ensureKnownRegionPromise = null);
    if (!skipRegionCheck) {
      await this._ensureKnownRegionPromise;
    }

    try {
      await this._loadEngines(cache);
    } catch (ex) {
      this._initRV = Cr.NS_ERROR_FAILURE;
      LOG("_init: failure loading engines: " + ex);
    }
    // Make sure the current list of engines is persisted, without the need to wait.
    this._buildCache();
    this._addObservers();
    gInitialized = true;
    if (Components.isSuccessCode(this._initRV)) {
      this._initObservers.resolve(this._initRV);
    } else {
      this._initObservers.reject(this._initRV);
    }
    Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "init-complete");

    LOG("_init: Completed _init");
    return this._initRV;
  },

  _metaData: { },
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
      LOG("getVerifiedGlobalAttr, invalid hash for " + name);
      return "";
    }
    return val;
  },

  _engines: { },
  __sortedEngines: null,
  _visibleDefaultEngines: [],
  _searchDefault: null,
  _searchOrder: [],
  get _sortedEngines() {
    if (!this.__sortedEngines)
      return this._buildSortedEngineList();
    return this.__sortedEngines;
  },

  // Get the original Engine object that is the default for this region,
  // ignoring changes the user may have subsequently made.
  get originalDefaultEngine() {
    let defaultEngineName = this.getVerifiedGlobalAttr("searchDefault");
    if (!defaultEngineName) {
      // We only allow the old defaultenginename pref for distributions
      // We can't use isPartnerBuild because we need to allow reading
      // of the defaultengine name pref for funnelcakes.
      if (distroID) {
        let defaultPrefB = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF);
        let nsIPLS = Ci.nsIPrefLocalizedString;

        try {
          defaultEngineName = defaultPrefB.getComplexValue("defaultenginename", nsIPLS).data;
        } catch (ex) {
          // If the default pref is invalid (e.g. an add-on set it to a bogus value)
          // use the default engine from the list.json.
          // This should eventually be the common case. We should only have the
          // defaultenginename pref for distributions.
          // Worst case, getEngineByName will just return null, which is the best we can do.
          defaultEngineName = this._searchDefault;
        }
      } else {
        defaultEngineName = this._searchDefault;
      }
    }

    let defaultEngine = this.getEngineByName(defaultEngineName);
    if (!defaultEngine) {
      // Something unexpected as happened. In order to recover the original default engine,
      // use the first visible engine which us what currentEngine will use.
      return this._getSortedEngines(false)[0];
    }

    return defaultEngine;
  },

  resetToOriginalDefaultEngine: function SRCH_SVC__resetToOriginalDefaultEngine() {
    let originalDefaultEngine = this.originalDefaultEngine;
    originalDefaultEngine.hidden = false;
    this.defaultEngine = originalDefaultEngine;
  },

  async _buildCache() {
    if (this._batchTask)
      this._batchTask.disarm();

    let cache = {};
    let locale = getLocale();
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
    cache.engines = [];

    for (let name in this._engines) {
      cache.engines.push(this._engines[name]);
    }

    try {
      if (!cache.engines.length)
        throw "cannot write without any engine.";

      LOG("_buildCache: Writing to cache file.");
      let path = OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME);
      let data = gEncoder.encode(JSON.stringify(cache));
      await OS.File.writeAtomic(path, data, {compression: "lz4",
                                             tmpPath: path + ".tmp"});
      LOG("_buildCache: cache file written to disk.");
      Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, SEARCH_SERVICE_CACHE_WRITTEN);
    } catch (ex) {
      LOG("_buildCache: Could not write to cache file: " + ex);
    }
  },

  /**
   * Loads engines asynchronously.
   *
   * @returns {Promise} A promise, resolved successfully if loading data
   * succeeds.
   */
  async _loadEngines(cache, isReload) {
    LOG("_loadEngines: start");
    Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "find-jar-engines");
    let chromeURIs = await this._findJAREngines();

    // Get the non-empty distribution directories into distDirs...
    let distDirs = [];
    let locations;
    try {
      locations = getDir(NS_APP_DISTRIBUTION_SEARCH_DIR_LIST,
                         Ci.nsISimpleEnumerator);
    } catch (e) {
      // NS_APP_DISTRIBUTION_SEARCH_DIR_LIST is defined by each app
      // so this throws during unit tests (but not xpcshell tests).
      locations = [];
    }
    for (let dir of locations) {
      let iterator = new OS.File.DirectoryIterator(dir.path,
                                                   { winPattern: "*.xml" });
      try {
        // Add dir to distDirs if it contains any files.
        let {done} = await iterator.next();
        if (!done) {
          distDirs.push(dir);
        }
      } finally {
        iterator.close();
      }
    }

    function notInCacheVisibleEngines(aEngineName) {
      return !cache.visibleDefaultEngines.includes(aEngineName);
    }

    let buildID = Services.appinfo.platformBuildID;
    let rebuildCache = !cache.engines ||
                       cache.version != CACHE_VERSION ||
                       cache.locale != getLocale() ||
                       cache.buildID != buildID ||
                       cache.visibleDefaultEngines.length != this._visibleDefaultEngines.length ||
                       this._visibleDefaultEngines.some(notInCacheVisibleEngines);

    if (!rebuildCache) {
      LOG("_loadEngines: loading from cache directories");
      this._loadEnginesFromCache(cache);
      if (Object.keys(this._engines).length) {
        LOG("_loadEngines: done using existing cache");
        return;
      }
      LOG("_loadEngines: No valid engines found in cache. Loading engines from disk.");
    }

    LOG("_loadEngines: Absent or outdated cache. Loading engines from disk.");
    for (let loadDir of distDirs) {
      let enginesFromDir = await this._loadEnginesFromDir(loadDir);
      enginesFromDir.forEach(this._addEngineToStore, this);
    }
    let enginesFromURLs = await this._loadFromChromeURLs(chromeURIs, isReload);
    enginesFromURLs.forEach(this._addEngineToStore, this);

    LOG("_loadEngines: loading user-installed engines from the obsolete cache");
    this._loadEnginesFromCache(cache, true);

    this._loadEnginesMetadataFromCache(cache);

    LOG("_loadEngines: done using rebuilt cache");
  },

  /**
   * Reloads engines asynchronously, but only when the service has already been
   * initialized.
   *
   * @return {Promise} A promise, resolved successfully if loading data succeeds.
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
      notifyAction(this._currentEngine, SEARCH_ENGINE_DEFAULT);
      notifyAction(this._currentEngine, SEARCH_ENGINE_CURRENT);
    }
    Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "engines-reloaded");
  },

  _reInit(origin) {
    LOG("_reInit");
    // Re-entrance guard, because we're using an async lambda below.
    if (gReinitializing) {
      LOG("_reInit: already re-initializing, bailing out.");
      return;
    }
    gReinitializing = true;

    // Start by clearing the initialized state, so we don't abort early.
    gInitialized = false;

    (async () => {
      try {
        this._initObservers = PromiseUtils.defer();
        if (this._batchTask) {
          LOG("finalizing batch task");
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
        this._engines = {};
        this.__sortedEngines = null;
        this._currentEngine = null;
        this._visibleDefaultEngines = [];
        this._searchDefault = null;
        this._searchOrder = [];
        this._metaData = {};

        // Tests that want to force a synchronous re-initialization need to
        // be notified when we are done uninitializing.
        Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC,
                                     "uninit-complete");

        let cache = await this._readCacheFile();
        // The init flow is not going to block on a fetch from an external service,
        // but we're kicking it off as soon as possible to prevent UI flickering as
        // much as possible.
        this._ensureKnownRegionPromise = ensureKnownRegion(this)
          .catch(ex => LOG("_reInit: failure determining region: " + ex))
          .finally(() => this._ensureKnownRegionPromise = null);
        await this._loadEngines(cache);
        // Make sure the current list of engines is persisted.
        await this._buildCache();

        // Typically we'll re-init as a result of a pref observer,
        // so signal to 'callers' that we're done.
        gInitialized = true;
        this._initObservers.resolve();
        Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "init-complete");
      } catch (err) {
        LOG("Reinit failed: " + err);
        Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "reinit-failed");
      } finally {
        gReinitializing = false;
        Services.obs.notifyObservers(null, SEARCH_SERVICE_TOPIC, "reinit-complete");
      }
    })();
  },

  /**
   * Read the cache file asynchronously.
   *
   * @returns {Promise} A promise, resolved successfully if retrieveing data
   * succeeds.
   */
  async _readCacheFile() {
    let json;
    try {
      let cacheFilePath = OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME);
      let bytes = await OS.File.read(cacheFilePath, {compression: "lz4"});
      json = JSON.parse(new TextDecoder().decode(bytes));
      if (!json.engines || !json.engines.length)
        throw "no engine in the file";
      // Reset search default expiration on major releases
      if (json.appVersion != Services.appinfo.version &&
          geoSpecificDefaultsEnabled() &&
          json.metaData) {
        json.metaData.searchDefaultExpir = 0;
      }
    } catch (ex) {
      LOG("_readCacheFile: Error reading cache file: " + ex);
      json = {};
    }
    if (!gInitialized && json.metaData)
      this._metaData = json.metaData;

    return json;
  },

  _batchTask: null,
  get batchTask() {
    if (!this._batchTask) {
      let task = async () => {
        LOG("batchTask: Invalidating engine cache");
        await this._buildCache();
      };
      this._batchTask = new DeferredTask(task, CACHE_INVALIDATION_DELAY);
    }
    return this._batchTask;
  },

  _submissionURLIgnoreList: [
    "ignore=true",
    "hspart=lvs",
    "pc=COSP",
    "clid=2308146",
    "fr=mca",
    "PC=MC0",
    "lavasoft.gosearchresults",
    "securedsearch.lavasoft",
  ],

  _loadPathIgnoreList: [
    "[other]addEngineWithDetails:searchignore@mozilla.com",
    "[https]opensearch.startpageweb.com/bing-search.xml",
    "[https]opensearch.startwebsearch.com/bing-search.xml",
    "[https]opensearch.webstartsearch.com/bing-search.xml",
    "[https]opensearch.webofsearch.com/bing-search.xml",
    "[profile]/searchplugins/Yahoo! Powered.xml",
    "[profile]/searchplugins/yahoo! powered.xml",
  ],

  _addEngineToStore: function SRCH_SVC_addEngineToStore(aEngine) {
    let url = aEngine._getURLOfType("text/html").getSubmission("dummy", aEngine).uri.spec.toLowerCase();
    if (this._submissionURLIgnoreList.some(code => url.includes(code.toLowerCase()))) {
      LOG("_addEngineToStore: Ignoring engine");
      return;
    }
    if (this._loadPathIgnoreList.includes(aEngine._loadPath)) {
      LOG("_addEngineToStore: Ignoring engine");
      return;
    }

    LOG("_addEngineToStore: Adding engine: \"" + aEngine.name + "\"");

    // See if there is an existing engine with the same name. However, if this
    // engine is updating another engine, it's allowed to have the same name.
    var hasSameNameAsUpdate = (aEngine._engineToUpdate &&
                               aEngine.name == aEngine._engineToUpdate.name);
    if (aEngine.name in this._engines && !hasSameNameAsUpdate) {
      LOG("_addEngineToStore: Duplicate engine found, aborting!");
      return;
    }

    if (aEngine._engineToUpdate) {
      // We need to replace engineToUpdate with the engine that just loaded.
      var oldEngine = aEngine._engineToUpdate;

      // Remove the old engine from the hash, since it's keyed by name, and our
      // name might change (the update might have a new name).
      delete this._engines[oldEngine.name];

      // Hack: we want to replace the old engine with the new one, but since
      // people may be holding refs to the nsISearchEngine objects themselves,
      // we'll just copy over all "private" properties (those without a getter
      // or setter) from one object to the other.
      for (var p in aEngine) {
        if (!(aEngine.__lookupGetter__(p) || aEngine.__lookupSetter__(p)))
          oldEngine[p] = aEngine[p];
      }
      aEngine = oldEngine;
      aEngine._engineToUpdate = null;

      // Add the engine back
      this._engines[aEngine.name] = aEngine;
      notifyAction(aEngine, SEARCH_ENGINE_CHANGED);
    } else {
      // Not an update, just add the new engine.
      this._engines[aEngine.name] = aEngine;
      // Only add the engine to the list of sorted engines if the initial list
      // has already been built (i.e. if this.__sortedEngines is non-null). If
      // it hasn't, we're loading engines from disk and the sorted engine list
      // will be built once we need it.
      if (this.__sortedEngines) {
        this.__sortedEngines.push(aEngine);
        this._saveSortedEngineList();
      }
      notifyAction(aEngine, SEARCH_ENGINE_ADDED);
    }

    if (aEngine._hasUpdates) {
      // Schedule the engine's next update, if it isn't already.
      if (!aEngine.getAttr("updateexpir"))
        engineUpdateService.scheduleNextUpdate(aEngine);
    }
  },

  _loadEnginesMetadataFromCache: function SRCH_SVC__loadEnginesMetadataFromCache(cache) {
    if (!cache.engines)
      return;

    for (let engine of cache.engines) {
      let name = engine._name;
      if (name in this._engines) {
        LOG("_loadEnginesMetadataFromCache, transfering metadata for " + name);
        this._engines[name]._metaData = engine._metaData || {};
      }
    }
  },

  _loadEnginesFromCache: function SRCH_SVC__loadEnginesFromCache(cache,
                                                                 skipReadOnly) {
    if (!cache.engines)
      return;

    LOG("_loadEnginesFromCache: Loading " +
        cache.engines.length + " engines from cache");

    let skippedEngines = 0;
    for (let engine of cache.engines) {
      if (skipReadOnly && engine._readOnly == undefined) {
        ++skippedEngines;
        continue;
      }

      this._loadEngineFromCache(engine);
    }

    if (skippedEngines) {
      LOG("_loadEnginesFromCache: skipped " + skippedEngines + " read-only engines.");
    }
  },

  _loadEngineFromCache: function SRCH_SVC__loadEngineFromCache(json) {
    try {
      let engine = new Engine(json._shortName, json._readOnly == undefined);
      engine._initWithJSON(json);
      this._addEngineToStore(engine);
    } catch (ex) {
      LOG("Failed to load " + json._name + " from cache: " + ex);
      LOG("Engine JSON: " + json.toSource());
    }
  },

  /**
   * Loads engines from a given directory asynchronously.
   *
   * @param aDir the directory.
   *
   * @returns {Promise} A promise, resolved successfully if retrieveing data
   * succeeds.
   */
  async _loadEnginesFromDir(aDir) {
    LOG("_loadEnginesFromDir: Searching in " + aDir.path + " for search engines.");

    let iterator = new OS.File.DirectoryIterator(aDir.path);

    let osfiles = await iterator.nextBatch();
    iterator.close();

    let engines = [];
    for (let osfile of osfiles) {
      if (osfile.isDir || osfile.isSymLink)
        continue;

      let fileInfo = await OS.File.stat(osfile.path);
      if (fileInfo.size == 0)
        continue;

      let parts = osfile.path.split(".");
      if (parts.length <= 1 || (parts.pop()).toLowerCase() != "xml") {
        // Not an engine
        continue;
      }

      let addedEngine = null;
      try {
        let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
        file.initWithPath(osfile.path);
        addedEngine = new Engine(file, false);
        await addedEngine._initFromFile(file);
        engines.push(addedEngine);
      } catch (ex) {
        LOG("_loadEnginesFromDir: Failed to load " + osfile.path + "!\n" + ex);
      }
    }
    return engines;
  },

  /**
   * Loads engines from Chrome URLs asynchronously.
   *
   * @param aURLs a list of URLs.
   * @param isReload is being called from maybeReloadEngines.
   *
   * @returns {Promise} A promise, resolved successfully if loading data
   * succeeds.
   */
  async _loadFromChromeURLs(aURLs, isReload = false) {
    let engines = [];
    for (let url of aURLs) {
      try {
        LOG("_loadFromChromeURLs: loading engine from chrome url: " + url);
        let uri = Services.io.newURI(url);
        let engine = new Engine(uri, true);
        await engine._initFromURI(uri);
        // If there is an existing engine with the same name then update that engine.
        // Only do this during reloads so it doesnt interfere with distribution
        // engines
        if (isReload && engine.name in this._engines) {
          engine._engineToUpdate = this._engines[engine.name];
        }
        engines.push(engine);
      } catch (ex) {
        LOG("_loadFromChromeURLs: failed to load engine: " + ex);
      }
    }
    return engines;
  },

  /**
   * Loads jar engines asynchronously.
   *
   * @returns {Promise} A promise, resolved successfully if finding jar engines
   * succeeds.
   */
  async _findJAREngines() {
    LOG("_findJAREngines: looking for engines in JARs");

    let listURL = APP_SEARCH_PREFIX + "list.json";
    let chan = makeChannel(listURL);
    if (!chan) {
      LOG("_findJAREngines: " + APP_SEARCH_PREFIX + " isn't registered");
      return [];
    }

    let uris = [];

    // Read list.json to find the engines we need to load.
    let request = new XMLHttpRequest();
    request.overrideMimeType("text/plain");
    let list = await new Promise(resolve => {
      request.onload = function(aEvent) {
        resolve(aEvent.target.responseText);
      };
      request.onerror = function(aEvent) {
        LOG("_findJAREngines: failed to read " + listURL);
        resolve();
      };
      request.open("GET", Services.io.newURI(listURL).spec, true);
      request.send();
    });

    this._parseListJSON(list, uris);
    return uris;
  },

  _parseListJSON: function SRCH_SVC_parseListJSON(list, uris) {
    let json;
    try {
      json = JSON.parse(list);
    } catch (e) {
      Cu.reportError("parseListJSON: Failed to parse list.json: " + e);
      dump("parseListJSON: Failed to parse list.json: " + e + "\n");
      return;
    }

    let searchRegion = Services.prefs.getCharPref("browser.search.region", null);

    let searchSettings;
    let locale = Services.locale.appLocaleAsBCP47;
    if ("locales" in json &&
        locale in json.locales) {
      searchSettings = json.locales[locale];
    } else {
      // No locales were found, so use the JSON as is.
      // It should have a default section.
      if (!("default" in json)) {
        Cu.reportError("parseListJSON: Missing default in list.json");
        dump("parseListJSON: Missing default in list.json\n");
        return;
      }
      searchSettings = json;
    }

    // Check if we have a useable region specific list of visible default engines.
    // This will only be set if we got the list from the Mozilla search server;
    // it will not be set for distributions.
    let engineNames;
    let visibleDefaultEngines = this.getVerifiedGlobalAttr("visibleDefaultEngines");
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
        if ("regionOverrides" in json &&
            searchRegion in json.regionOverrides) {
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
          LOG("_parseListJSON: ignoring visibleDefaultEngines value because " +
              engineName + " is not in the jar engines we have found");
          engineNames = null;
          break;
        }
      }
    }

    // Fallback to building a list based on the regions in the JSON
    if (!engineNames || !engineNames.length) {
      if (searchRegion && searchRegion in searchSettings &&
          "visibleDefaultEngines" in searchSettings[searchRegion]) {
        engineNames = searchSettings[searchRegion].visibleDefaultEngines;
      } else {
        engineNames = searchSettings.default.visibleDefaultEngines;
      }
    }

    // Remove any engine names that are supposed to be ignored.
    // This pref is only allowed in a partner distribution.
    let branch = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF);
    if (isPartnerBuild() &&
        branch.getPrefType("ignoredJAREngines") == branch.PREF_STRING) {
      let ignoredJAREngines = branch.getCharPref("ignoredJAREngines")
                                    .split(",");
      let filteredEngineNames = engineNames.filter(e => !ignoredJAREngines.includes(e));
      // Don't allow all engines to be hidden
      if (filteredEngineNames.length > 0) {
        engineNames = filteredEngineNames;
      }
    }

    if ("regionOverrides" in json &&
        searchRegion in json.regionOverrides) {
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

    for (let name of engineNames) {
      uris.push(APP_SEARCH_PREFIX + name + ".xml");
    }

    // Store this so that it can be used while writing the cache file.
    this._visibleDefaultEngines = engineNames;

    if (searchRegion && searchRegion in searchSettings &&
        "searchDefault" in searchSettings[searchRegion]) {
      this._searchDefault = searchSettings[searchRegion].searchDefault;
    } else if ("searchDefault" in searchSettings.default) {
      this._searchDefault = searchSettings.default.searchDefault;
    } else {
      this._searchDefault = json.default.searchDefault;
    }

    if (!this._searchDefault) {
      Cu.reportError("parseListJSON: No searchDefault");
    }

    if (searchRegion && searchRegion in searchSettings &&
        "searchOrder" in searchSettings[searchRegion]) {
      this._searchOrder = searchSettings[searchRegion].searchOrder;
    } else if ("searchOrder" in searchSettings.default) {
      this._searchOrder = searchSettings.default.searchOrder;
    } else if ("searchOrder" in json.default) {
      this._searchOrder = json.default.searchOrder;
    }
  },

  _saveSortedEngineList: function SRCH_SVC_saveSortedEngineList() {
    LOG("SRCH_SVC_saveSortedEngineList: starting");

    // Set the useDB pref to indicate that from now on we should use the order
    // information stored in the database.
    Services.prefs.setBoolPref(BROWSER_SEARCH_PREF + "useDBForOrder", true);

    var engines = this._getSortedEngines(true);

    for (var i = 0; i < engines.length; ++i) {
      engines[i].setAttr("order", i + 1);
    }

    LOG("SRCH_SVC_saveSortedEngineList: done");
  },

  _buildSortedEngineList: function SRCH_SVC_buildSortedEngineList() {
    LOG("_buildSortedEngineList: building list");
    var addedEngines = { };
    this.__sortedEngines = [];
    var engine;

    // If the user has specified a custom engine order, read the order
    // information from the metadata instead of the default prefs.
    if (Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "useDBForOrder", false)) {
      LOG("_buildSortedEngineList: using db for order");

      // Flag to keep track of whether or not we need to call _saveSortedEngineList.
      let needToSaveEngineList = false;

      for (let name in this._engines) {
        let engine = this._engines[name];
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
      var filteredEngines = this.__sortedEngines.filter(function(a) { return !!a; });
      if (this.__sortedEngines.length != filteredEngines.length)
        needToSaveEngineList = true;
      this.__sortedEngines = filteredEngines;

      if (needToSaveEngineList)
        this._saveSortedEngineList();
    } else {
      // The DB isn't being used, so just read the engine order from the prefs
      var i = 0;
      var engineName;
      var prefName;

      // The original default engine should always be first in the list
      if (this.originalDefaultEngine) {
        this.__sortedEngines.push(this.originalDefaultEngine);
        addedEngines[this.originalDefaultEngine.name] = this.originalDefaultEngine;
      }

      if (distroID) {
        try {
          var extras =
            Services.prefs.getChildList(BROWSER_SEARCH_PREF + "order.extra.");

          for (prefName of extras) {
            engineName = Services.prefs.getCharPref(prefName);

            engine = this._engines[engineName];
            if (!engine || engine.name in addedEngines)
              continue;

            this.__sortedEngines.push(engine);
            addedEngines[engine.name] = engine;
          }
        } catch (e) { }

        while (true) {
          prefName = `${BROWSER_SEARCH_PREF}order.${++i}`;
          engineName = getLocalizedPref(prefName);
          if (!engineName)
            break;

          engine = this._engines[engineName];
          if (!engine || engine.name in addedEngines)
            continue;

          this.__sortedEngines.push(engine);
          addedEngines[engine.name] = engine;
        }
      }

      for (let engineName of this._searchOrder) {
        engine = this._engines[engineName];
        if (!engine || engine.name in addedEngines)
          continue;

        this.__sortedEngines.push(engine);
        addedEngines[engine.name] = engine;
      }
    }

    // Array for the remaining engines, alphabetically sorted.
    let alphaEngines = [];

    for (let name in this._engines) {
      let engine = this._engines[name];
      if (!(engine.name in addedEngines))
        alphaEngines.push(this._engines[engine.name]);
    }

    let collation = Cc["@mozilla.org/intl/collation-factory;1"]
                      .createInstance(Ci.nsICollationFactory)
                      .CreateCollation();
    const strength = Ci.nsICollation.kCollationCaseInsensitiveAscii;
    let comparator = (a, b) => collation.compareString(strength, a.name, b.name);
    alphaEngines.sort(comparator);
    return this.__sortedEngines = this.__sortedEngines.concat(alphaEngines);
  },

  /**
   * Get a sorted array of engines.
   * @param aWithHidden
   *        True if hidden plugins should be included in the result.
   */
  _getSortedEngines: function SRCH_SVC_getSorted(aWithHidden) {
    if (aWithHidden)
      return this._sortedEngines;

    return this._sortedEngines.filter(function(engine) {
                                        return !engine.hidden;
                                      });
  },

  // nsISearchService
  async init(skipRegionCheck = false) {
    LOG("SearchService.init");
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
      throw this._initRV;
    }
    return this._initRV;
  },

  get isInitialized() {
    return gInitialized;
  },

  async getEngines() {
    await this.init(true);
    LOG("getEngines: getting all engines");
    var engines = this._getSortedEngines(true);
    return engines;
  },

  async getVisibleEngines() {
    await this.init();
    LOG("getVisibleEngines: getting all visible engines");
    var engines = this._getSortedEngines(false);
    return engines;
  },

  async getDefaultEngines() {
    await this.init(true);
    function isDefault(engine) {
      return engine._isDefault;
    }
    var engines = this._sortedEngines.filter(isDefault);
    var engineOrder = {};
    var engineName;
    var i = 1;

    // Build a list of engines which we have ordering information for.
    // We're rebuilding the list here because _sortedEngines contain the
    // current order, but we want the original order.

    if (distroID) {
      // First, look at the "browser.search.order.extra" branch.
      try {
        var extras = Services.prefs.getChildList(BROWSER_SEARCH_PREF + "order.extra.");

        for (var prefName of extras) {
          engineName = Services.prefs.getCharPref(prefName);

          if (!(engineName in engineOrder))
            engineOrder[engineName] = i++;
        }
      } catch (e) {
        LOG("Getting extra order prefs failed: " + e);
      }

      // Now look through the "browser.search.order" branch.
      for (var j = 1; ; j++) {
        let prefName = `${BROWSER_SEARCH_PREF}order.${j}`;
        engineName = getLocalizedPref(prefName);
        if (!engineName)
          break;

        if (!(engineName in engineOrder))
          engineOrder[engineName] = i++;
      }
    }

    // Now look at list.json
    for (let engineName of this._searchOrder) {
      engineOrder[engineName] = i++;
    }

    LOG("getDefaultEngines: engineOrder: " + engineOrder.toSource());

    function compareEngines(a, b) {
      var aIdx = engineOrder[a.name];
      var bIdx = engineOrder[b.name];

      if (aIdx && bIdx)
        return aIdx - bIdx;
      if (aIdx)
        return -1;
      if (bIdx)
        return 1;

      return a.name.localeCompare(b.name);
    }
    engines.sort(compareEngines);
    return engines;
  },

  async getEnginesByExtensionID(extensionID) {
    await this.init(true);
    LOG("getEngines: getting all engines for " + extensionID);
    var engines = this._getSortedEngines(true).filter(function(engine) {
      return engine._extensionID == extensionID;
    });
    return engines;
  },

  getEngineByName: function SRCH_SVC_getEngineByName(aEngineName) {
    this._ensureInitialized();
    return this._engines[aEngineName] || null;
  },

  getEngineByAlias: function SRCH_SVC_getEngineByAlias(aAlias) {
    this._ensureInitialized();
    for (var engineName in this._engines) {
      var engine = this._engines[engineName];
      if (engine && (engine.alias == aAlias || engine._internalAliases.includes(aAlias))) {
        return engine;
      }
    }
    return null;
  },

  async addEngineWithDetails(name, iconURL, alias, description, method, template, extensionID) {
    let isCurrent = false;
    var params;

    if (iconURL && typeof iconURL == "object") {
      params = iconURL;
    } else {
      params = {
        iconURL,
        alias,
        description,
        method,
        template,
        extensionID,
      };
    }

    await this.init(true);
    if (!name)
      FAIL("Invalid name passed to addEngineWithDetails!");
    if (!params.template)
      FAIL("Invalid template passed to addEngineWithDetails!");
    let existingEngine = this._engines[name];
    if (existingEngine) {
      if (params.extensionID &&
          existingEngine._loadPath.startsWith(`jar:[profile]/extensions/${params.extensionID}`)) {
        // This is a legacy extension engine that needs to be migrated to WebExtensions.
        isCurrent = this.defaultEngine == existingEngine;
        await this.removeEngine(existingEngine);
      } else {
        FAIL("An engine with that name already exists!", Cr.NS_ERROR_FILE_ALREADY_EXISTS);
      }
    }

    let newEngine = new Engine(sanitizeName(name), false);
    newEngine._initFromMetadata(name, params);
    newEngine._loadPath = "[other]addEngineWithDetails";
    if (params.extensionID) {
      newEngine._loadPath += ":" + params.extensionID;
    }
    this._addEngineToStore(newEngine);
    if (isCurrent) {
      this.defaultEngine = newEngine;
    }
  },

  async addEnginesFromExtension(extension) {
    let {IconDetails} = ExtensionParent;
    let {manifest} = extension;

    // General set of icons for an engine.
    let icons = extension.manifest.icons;
    let iconList = [];
    if (icons) {
      iconList = Object.entries(icons).map(icon => {
        return {width: icon[0], height: icon[0],
                url: extension.baseURI.resolve(icon[1])};
      });
    }
    let preferredIconUrl = icons && extension.baseURI.resolve(IconDetails.getPreferredIcon(icons).icon);

    let searchProvider = manifest.chrome_settings_overrides.search_provider;
    let params = {
      template: searchProvider.search_url,
      searchPostParams: searchProvider.search_url_post_params,
      iconURL: searchProvider.favicon_url || preferredIconUrl,
      icons: iconList,
      alias: searchProvider.keyword,
      extensionID: extension.id,
      isBuiltIn: extension.isPrivileged,
      suggestURL: searchProvider.suggest_url,
      suggestPostParams: searchProvider.suggest_url_post_params,
      queryCharset: searchProvider.encoding || "UTF-8",
      mozParams: searchProvider.params,
    };
    return this.addEngineWithDetails(searchProvider.name.trim(), params);
  },

  async addEngine(engineURL, iconURL, confirm, extensionID) {
    LOG("addEngine: Adding \"" + engineURL + "\".");
    await this.init(true);
    let errCode;
    try {
      var uri = makeURI(engineURL);
      var engine = new Engine(uri, false);
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
        engine._initFromURIAndLoad(uri);
      });
      if (errCode) {
        throw errCode;
      }
    } catch (ex) {
      // Drop the reference to the callback, if set
      if (engine)
        engine._installCallback = null;
      FAIL("addEngine: Error adding engine:\n" + ex, errCode || Cr.NS_ERROR_FAILURE);
    }
    return engine;
  },

  async removeEngine(engine) {
    await this.init(true);
    if (!engine)
      FAIL("no engine passed to removeEngine!");

    var engineToRemove = null;
    for (var e in this._engines) {
      if (engine.wrappedJSObject == this._engines[e])
        engineToRemove = this._engines[e];
    }

    if (!engineToRemove)
      FAIL("removeEngine: Can't find engine to remove!", Cr.NS_ERROR_FILE_NOT_FOUND);

    if (engineToRemove == this.defaultEngine) {
      this._currentEngine = null;
    }

    if (engineToRemove._readOnly) {
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
      if (index == -1)
        FAIL("Can't find engine to remove in _sortedEngines!", Cr.NS_ERROR_FAILURE);
      this.__sortedEngines.splice(index, 1);

      // Remove the engine from the internal store
      delete this._engines[engineToRemove.name];

      // Since we removed an engine, we need to update the preferences.
      this._saveSortedEngineList();
    }
    notifyAction(engineToRemove, SEARCH_ENGINE_REMOVED);
  },

  async moveEngine(engine, newIndex) {
    await this.init(true);
    if ((newIndex > this._sortedEngines.length) || (newIndex < 0))
      FAIL("SRCH_SVC_moveEngine: Index out of bounds!");
    if (!(engine instanceof Ci.nsISearchEngine) && !(engine instanceof Engine))
      FAIL("SRCH_SVC_moveEngine: Invalid engine passed to moveEngine!");
    if (engine.hidden)
      FAIL("moveEngine: Can't move a hidden engine!", Cr.NS_ERROR_FAILURE);

    engine = engine.wrappedJSObject;

    var currentIndex = this._sortedEngines.indexOf(engine);
    if (currentIndex == -1)
      FAIL("moveEngine: Can't find engine to move!", Cr.NS_ERROR_UNEXPECTED);

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
    if (!newIndexEngine)
      FAIL("moveEngine: Can't find engine to replace!", Cr.NS_ERROR_UNEXPECTED);

    for (var i = 0; i < this._sortedEngines.length; ++i) {
      if (newIndexEngine == this._sortedEngines[i])
        break;
      if (this._sortedEngines[i].hidden)
        newIndex++;
    }

    if (currentIndex == newIndex)
      return; // nothing to do!

    // Move the engine
    var movedEngine = this.__sortedEngines.splice(currentIndex, 1)[0];
    this.__sortedEngines.splice(newIndex, 0, movedEngine);

    notifyAction(engine, SEARCH_ENGINE_CHANGED);

    // Since we moved an engine, we need to update the preferences.
    this._saveSortedEngineList();
  },

  restoreDefaultEngines() {
    this._ensureInitialized();
    for (let name in this._engines) {
      let e = this._engines[name];
      // Unhide all default engines
      if (e.hidden && e._isDefault)
        e.hidden = false;
    }
  },

  get defaultEngine() {
    this._ensureInitialized();
    if (!this._currentEngine) {
      let name = this.getGlobalAttr("current");
      let engine = this.getEngineByName(name);
      if (engine && (this.getGlobalAttr("hash") == getVerificationHash(name) ||
                     engine._isDefault)) {
        // If the current engine is a default one, we can relax the
        // verification hash check to reduce the annoyance for users who
        // backup/sync their profile in custom ways.
        this._currentEngine = engine;
      }
      if (!name)
        this._currentEngine = this.originalDefaultEngine;
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
        if (originalDefault)
          originalDefault.hidden = false;
      }
      if (!originalDefault)
        return null;

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
    if (!(val instanceof Ci.nsISearchEngine) && !(val instanceof Engine))
      FAIL("Invalid argument passed to defaultEngine setter");

    var newCurrentEngine = this.getEngineByName(val.name);
    if (!newCurrentEngine)
      FAIL("Can't find engine in store!", Cr.NS_ERROR_UNEXPECTED);

    if (!newCurrentEngine._isDefault) {
      // If a non default engine is being set as the current engine, ensure
      // its loadPath has a verification hash.
      if (!newCurrentEngine._loadPath)
        newCurrentEngine._loadPath = "[other]unknown";
      let loadPathHash = getVerificationHash(newCurrentEngine._loadPath);
      let currentHash = newCurrentEngine.getAttr("loadPathHash");
      if (!currentHash || currentHash != loadPathHash) {
        newCurrentEngine.setAttr("loadPathHash", loadPathHash);
        notifyAction(newCurrentEngine, SEARCH_ENGINE_CHANGED);
      }
    }

    if (newCurrentEngine == this._currentEngine)
      return;

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

    notifyAction(this._currentEngine, SEARCH_ENGINE_DEFAULT);
    notifyAction(this._currentEngine, SEARCH_ENGINE_CURRENT);
  },

  async getDefault() {
    await this.init(true);
    return this.defaultEngine;
  },

  async setDefault(engine) {
    await this.init(true);
    return this.defaultEngine = engine;
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
      if (engine.name)
        result.name = engine.name;

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
        let extras =
          Services.prefs.getChildList(BROWSER_SEARCH_PREF + "order.extra.");

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
          let prefName = `${BROWSER_SEARCH_PREF}order.${++i}`;
          let engineName = getLocalizedPref(prefName);
          if (!engineName)
            break;
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
        let engineHost = engine._getURLOfType(URLTYPE_SEARCH_HTML).templateHost;
        for (let name in this._engines) {
          let innerEngine = this._engines[name];
          if (!innerEngine._isDefault) {
            continue;
          }

          let innerEngineURL = innerEngine._getURLOfType(URLTYPE_SEARCH_HTML);
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
          const urlTest =
            /^(?:www\.google\.|search\.aol\.|yandex\.)|(?:search\.yahoo|\.ask|\.bing|\.startpage|\.baidu|duckduckgo)\.com$/;
          sendSubmissionURL = urlTest.test(engineHost);
        }
      }

      if (sendSubmissionURL) {
        let uri = engine._getURLOfType("text/html")
                        .getSubmission("", engine, "searchbar").uri;
        uri = uri.mutate()
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

  _buildParseSubmissionMap: function SRCH_SVC__buildParseSubmissionMap() {
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
      SearchStaticData.getAlternateDomains(urlParsingInfo.mainDomain)
                      .forEach(d => processDomain(d, true));
    }
  },

  /**
   * Checks to see if any engine has an EngineURL of type URLTYPE_SEARCH_HTML
   * for this request-method, template URL, and query params.
   */
  hasEngineWithURL(method, template, formData) {
    this._ensureInitialized();

    // Quick helper method to ensure formData filtered/sorted for compares.
    let getSortedFormData = data => {
      return data.filter(a => a.name && a.value).sort((a, b) => {
        if (a.name > b.name) {
          return 1;
        } else if (b.name > a.name) {
          return -1;
        } else if (a.value > b.value) {
          return 1;
        }
        return (b.value > a.value) ? -1 : 0;
      });
    };

    // Sanitize method, ensure formData is pre-sorted.
    let methodUpper = method.toUpperCase();
    let sortedFormData = getSortedFormData(formData);
    let sortedFormLength = sortedFormData.length;

    return this._getSortedEngines(false).some(engine => {
      return engine._urls.some(url => {
        // Not an engineURL match if type, method, url, #params don't match.
        if (url.type != URLTYPE_SEARCH_HTML ||
            url.method != methodUpper ||
            url.template != template ||
            url.params.length != sortedFormLength) {
          return false;
        }

        // Ensure engineURL formData is pre-sorted. Then, we're
        // not an engineURL match if any queryParam doesn't compare.
        let sortedParams = getSortedFormData(url.params);
        for (let i = 0; i < sortedFormLength; i++) {
          let formData = sortedFormData[i];
          let param = sortedParams[i];
          if (param.name != formData.name ||
              param.value != formData.value ||
              param.purpose != formData.purpose) {
            return false;
          }
        }
        // Else we're a match.
        return true;
      });
    });
  },

  parseSubmissionURL: function SRCH_SVC_parseSubmissionURL(aURL) {
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
      let soughtUrl = Services.io.newURI(aURL).QueryInterface(Ci.nsIURL);

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
      if (equalPos != -1 &&
          param.substr(0, equalPos) == mapEntry.termsParameterName) {
        // This is the parameter we are looking for.
        encodedTerms = param.substr(equalPos + 1);
        break;
      }
    }
    if (encodedTerms === null) {
      return gEmptyParseSubmissionResult;
    }

    let length = 0;
    let offset = aURL.indexOf("?") + 1;
    let query = aURL.slice(offset);
    // Iterate a second time over the original input string to determine the
    // correct search term offset and length in the original encoding.
    for (let param of query.split("&")) {
      let equalPos = param.indexOf("=");
      if (equalPos != -1 &&
          param.substr(0, equalPos) == mapEntry.termsParameterName) {
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
        encodedTerms.replace(/\+/g, " "));
    } catch (ex) {
      // Decoding errors will cause this match to be ignored.
      return gEmptyParseSubmissionResult;
    }

    return new ParseSubmissionResult(mapEntry.engine, terms, offset, length);
  },

  // nsIObserver
  observe: function SRCH_SVC_observe(aEngine, aTopic, aVerb) {
    switch (aTopic) {
      case SEARCH_ENGINE_TOPIC:
        switch (aVerb) {
          case SEARCH_ENGINE_LOADED:
            var engine = aEngine.QueryInterface(Ci.nsISearchEngine);
            LOG("nsSearchService::observe: Done installation of " + engine.name
                + ".");
            this._addEngineToStore(engine.wrappedJSObject);
            if (engine.wrappedJSObject._useNow) {
              LOG("nsSearchService::observe: setting current");
              this.defaultEngine = aEngine;
            }
            // The addition of the engine to the store always triggers an ADDED
            // or a CHANGED notification, that will trigger the task below.
            break;
          case SEARCH_ENGINE_ADDED:
          case SEARCH_ENGINE_CHANGED:
          case SEARCH_ENGINE_REMOVED:
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
          this._reInit(aVerb);
        }
        break;
    }
  },

  // nsITimerCallback
  notify: function SRCH_SVC_notify(aTimer) {
    LOG("_notify: checking for updates");

    if (!Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "update", true))
      return;

    // Our timer has expired, but unfortunately, we can't get any data from it.
    // Therefore, we need to walk our engine-list, looking for expired engines
    var currentTime = Date.now();
    LOG("currentTime: " + currentTime);
    for (let name in this._engines) {
      let engine = this._engines[name].wrappedJSObject;
      if (!engine._hasUpdates)
        continue;

      LOG("checking " + engine.name);

      var expirTime = engine.getAttr("updateexpir");
      LOG("expirTime: " + expirTime + "\nupdateURL: " + engine._updateURL +
          "\niconUpdateURL: " + engine._iconUpdateURL);

      var engineExpired = expirTime <= currentTime;

      if (!expirTime || !engineExpired) {
        LOG("skipping engine");
        continue;
      }

      LOG(engine.name + " has expired");

      engineUpdateService.update(engine);

      // Schedule the next update
      engineUpdateService.scheduleNextUpdate(engine);
    } // end engine iteration
  },

  _addObservers: function SRCH_SVC_addObservers() {
    if (this._observersAdded) {
      // There might be a race between synchronous and asynchronous
      // initialization for which we try to register the observers twice.
      return;
    }
    this._observersAdded = true;

    Services.obs.addObserver(this, SEARCH_ENGINE_TOPIC);
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
      () => (async () => {
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

  _removeObservers: function SRCH_SVC_removeObservers() {
    Services.obs.removeObserver(this, SEARCH_ENGINE_TOPIC);
    Services.obs.removeObserver(this, QUIT_APPLICATION_TOPIC);
    Services.obs.removeObserver(this, TOPIC_LOCALES_CHANGE);
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsISearchService,
    Ci.nsIObserver,
    Ci.nsITimerCallback,
  ]),
};


const SEARCH_UPDATE_LOG_PREFIX = "*** Search update: ";

/**
 * Outputs aText to the JavaScript console as well as to stdout, if the search
 * logging pref (browser.search.update.log) is set to true.
 */
function ULOG(aText) {
  if (Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "update.log", false)) {
    dump(SEARCH_UPDATE_LOG_PREFIX + aText + "\n");
    Services.console.logStringMessage(aText);
  }
}

var engineUpdateService = {
  scheduleNextUpdate: function eus_scheduleNextUpdate(aEngine) {
    var interval = aEngine._updateInterval || SEARCH_DEFAULT_UPDATE_INTERVAL;
    var milliseconds = interval * 86400000; // |interval| is in days
    aEngine.setAttr("updateexpir", Date.now() + milliseconds);
  },

  update: function eus_Update(aEngine) {
    let engine = aEngine.wrappedJSObject;
    ULOG("update called for " + aEngine._name);
    if (!Services.prefs.getBoolPref(BROWSER_SEARCH_PREF + "update", true) ||
        !engine._hasUpdates)
      return;

    let testEngine = null;
    let updateURL = engine._getURLOfType(URLTYPE_OPENSEARCH);
    let updateURI = (updateURL && updateURL._hasRelation("self")) ?
                     updateURL.getSubmission("", engine).uri :
                     makeURI(engine._updateURL);
    if (updateURI) {
      if (engine._isDefault && !updateURI.schemeIs("https")) {
        ULOG("Invalid scheme for default engine update");
        return;
      }

      ULOG("updating " + engine.name + " from " + updateURI.spec);
      testEngine = new Engine(updateURI, false);
      testEngine._engineToUpdate = engine;
      testEngine._initFromURIAndLoad(updateURI);
    } else {
      ULOG("invalid updateURI");
    }

    if (engine._iconUpdateURL) {
      // If we're updating the engine too, use the new engine object,
      // otherwise use the existing engine object.
      (testEngine || engine)._setIcon(engine._iconUpdateURL, true);
    }
  },
};

var EXPORTED_SYMBOLS = ["SearchService"];
