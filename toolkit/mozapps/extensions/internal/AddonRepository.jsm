/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  Services: "resource://gre/modules/Services.jsm",
  ServiceRequest: "resource://gre/modules/ServiceRequest.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
});

this.EXPORTED_SYMBOLS = [ "AddonRepository" ];

const PREF_GETADDONS_CACHE_ENABLED       = "extensions.getAddons.cache.enabled";
const PREF_GETADDONS_CACHE_TYPES         = "extensions.getAddons.cache.types";
const PREF_GETADDONS_CACHE_ID_ENABLED    = "extensions.%ID%.getAddons.cache.enabled";
const PREF_GETADDONS_BROWSEADDONS        = "extensions.getAddons.browseAddons";
const PREF_GETADDONS_BYIDS               = "extensions.getAddons.get.url";
const PREF_GETADDONS_BYIDS_PERFORMANCE   = "extensions.getAddons.getWithPerformance.url";
const PREF_GETADDONS_BROWSERECOMMENDED   = "extensions.getAddons.recommended.browseURL";
const PREF_GETADDONS_GETRECOMMENDED      = "extensions.getAddons.recommended.url";
const PREF_GETADDONS_BROWSESEARCHRESULTS = "extensions.getAddons.search.browseURL";
const PREF_GETADDONS_GETSEARCHRESULTS    = "extensions.getAddons.search.url";
const PREF_GETADDONS_DB_SCHEMA           = "extensions.getAddons.databaseSchema";

const PREF_METADATA_LASTUPDATE           = "extensions.getAddons.cache.lastUpdate";
const PREF_METADATA_UPDATETHRESHOLD_SEC  = "extensions.getAddons.cache.updateThreshold";
const DEFAULT_METADATA_UPDATETHRESHOLD_SEC = 172800; // two days

const XMLURI_PARSE_ERROR  = "http://www.mozilla.org/newlayout/xml/parsererror.xml";

const API_VERSION = "1.5";
const DEFAULT_CACHE_TYPES = "extension,theme,locale,dictionary";

const FILE_DATABASE         = "addons.json";
const DB_SCHEMA             = 5;
const DB_MIN_JSON_SCHEMA    = 5;
const DB_BATCH_TIMEOUT_MS   = 50;

const BLANK_DB = function() {
  return {
    addons: new Map(),
    schema: DB_SCHEMA
  };
};

const TOOLKIT_ID     = "toolkit@mozilla.org";

Cu.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.repository";

// Create a new logger for use by the Addons Repository
// (Requires AddonManager.jsm)
var logger = Log.repository.getLogger(LOGGER_ID);

// A map between XML keys to AddonSearchResult keys for string values
// that require no extra parsing from XML
const STRING_KEY_MAP = {
  name:               "name",
  version:            "version",
  homepage:           "homepageURL",
  support:            "supportURL"
};

// A map between XML keys to AddonSearchResult keys for string values
// that require parsing from HTML
const HTML_KEY_MAP = {
  summary:            "description",
  description:        "fullDescription",
  developer_comments: "developerComments",
  eula:               "eula"
};

// A map between XML keys to AddonSearchResult keys for integer values
// that require no extra parsing from XML
const INTEGER_KEY_MAP = {
  total_downloads:  "totalDownloads",
  weekly_downloads: "weeklyDownloads",
  daily_users:      "dailyUsers"
};

function convertHTMLToPlainText(html) {
  if (!html)
    return html;
  var converter = Cc["@mozilla.org/widget/htmlformatconverter;1"].
                  createInstance(Ci.nsIFormatConverter);

  var input = Cc["@mozilla.org/supports-string;1"].
              createInstance(Ci.nsISupportsString);
  input.data = html.replace(/\n/g, "<br>");

  var output = {};
  converter.convert("text/html", input, input.data.length, "text/unicode",
                    output, {});

  if (output.value instanceof Ci.nsISupportsString)
    return output.value.data.replace(/\r\n/g, "\n");
  return html;
}

async function getAddonsToCache(aIds) {
  let types = Preferences.get(PREF_GETADDONS_CACHE_TYPES) || DEFAULT_CACHE_TYPES;

  types = types.split(",");

  let addons = await AddonManager.getAddonsByIDs(aIds);
  let enabledIds = [];

  for (let [i, addon] of addons.entries()) {
    var preference = PREF_GETADDONS_CACHE_ID_ENABLED.replace("%ID%", aIds[i]);
    // If the preference doesn't exist caching is enabled by default
    if (!Preferences.get(preference, true))
      continue;

    // The add-ons manager may not know about this ID yet if it is a pending
    // install. In that case we'll just cache it regardless

    // Don't cache add-ons of the wrong types
    if (addon && !types.includes(addon.type)) {
      continue;
    }

    // Don't cache system add-ons
    if (addon && addon.isSystem) {
      continue;
    }

    enabledIds.push(aIds[i]);
  }

  return enabledIds;
}

function AddonSearchResult(aId) {
  this.id = aId;
  this.icons = {};
  this._unsupportedProperties = {};
}

AddonSearchResult.prototype = {
  /**
   * The ID of the add-on
   */
  id: null,

  /**
   * The add-on type (e.g. "extension" or "theme")
   */
  type: null,

  /**
   * The name of the add-on
   */
  name: null,

  /**
   * The version of the add-on
   */
  version: null,

  /**
   * The creator of the add-on
   */
  creator: null,

  /**
   * The developers of the add-on
   */
  developers: null,

  /**
   * A short description of the add-on
   */
  description: null,

  /**
   * The full description of the add-on
   */
  fullDescription: null,

  /**
   * The developer comments for the add-on. This includes any information
   * that may be helpful to end users that isn't necessarily applicable to
   * the add-on description (e.g. known major bugs)
   */
  developerComments: null,

  /**
   * The end-user licensing agreement (EULA) of the add-on
   */
  eula: null,

  /**
   * The url of the add-on's icon
   */
  get iconURL() {
    return this.icons && this.icons[32];
  },

   /**
   * The URLs of the add-on's icons, as an object with icon size as key
   */
  icons: null,

  /**
   * An array of screenshot urls for the add-on
   */
  screenshots: null,

  /**
   * The homepage for the add-on
   */
  homepageURL: null,

  /**
   * The homepage for the add-on
   */
  learnmoreURL: null,

  /**
   * The support URL for the add-on
   */
  supportURL: null,

  /**
   * The contribution url of the add-on
   */
  contributionURL: null,

  /**
   * The suggested contribution amount
   */
  contributionAmount: null,

  /**
   * The URL to visit in order to purchase the add-on
   */
  purchaseURL: null,

  /**
   * The numerical cost of the add-on in some currency, for sorting purposes
   * only
   */
  purchaseAmount: null,

  /**
   * The display cost of the add-on, for display purposes only
   */
  purchaseDisplayAmount: null,

  /**
   * The rating of the add-on, 0-5
   */
  averageRating: null,

  /**
   * The number of reviews for this add-on
   */
  reviewCount: null,

  /**
   * The URL to the list of reviews for this add-on
   */
  reviewURL: null,

  /**
   * The total number of times the add-on was downloaded
   */
  totalDownloads: null,

  /**
   * The number of times the add-on was downloaded the current week
   */
  weeklyDownloads: null,

  /**
   * The number of daily users for the add-on
   */
  dailyUsers: null,

  /**
   * AddonInstall object generated from the add-on XPI url
   */
  install: null,

  /**
   * nsIURI storing where this add-on was installed from
   */
  sourceURI: null,

  /**
   * The status of the add-on in the repository (e.g. 4 = "Public")
   */
  repositoryStatus: null,

  /**
   * The size of the add-on's files in bytes. For an add-on that have not yet
   * been downloaded this may be an estimated value.
   */
  size: null,

  /**
   * The Date that the add-on was most recently updated
   */
  updateDate: null,

  /**
   * True or false depending on whether the add-on is compatible with the
   * current version of the application
   */
  isCompatible: true,

  /**
   * True or false depending on whether the add-on is compatible with the
   * current platform
   */
  isPlatformCompatible: true,

  /**
   * Array of AddonCompatibilityOverride objects, that describe overrides for
   * compatibility with an application versions.
   **/
  compatibilityOverrides: null,

  /**
   * True if the add-on has a secure means of updating
   */
  providesUpdatesSecurely: true,

  /**
   * The current blocklist state of the add-on
   */
  blocklistState: Ci.nsIBlocklistService.STATE_NOT_BLOCKED,

  /**
   * True if this add-on cannot be used in the application based on version
   * compatibility, dependencies and blocklisting
   */
  appDisabled: false,

  /**
   * True if the user wants this add-on to be disabled
   */
  userDisabled: false,

  /**
   * Indicates what scope the add-on is installed in, per profile, user,
   * system or application
   */
  scope: AddonManager.SCOPE_PROFILE,

  /**
   * True if the add-on is currently functional
   */
  isActive: true,

  /**
   * A bitfield holding all of the current operations that are waiting to be
   * performed for this add-on
   */
  pendingOperations: AddonManager.PENDING_NONE,

  /**
   * A bitfield holding all the the operations that can be performed on
   * this add-on
   */
  permissions: 0,

  /**
   * Tests whether this add-on is known to be compatible with a
   * particular application and platform version.
   *
   * @param  appVersion
   *         An application version to test against
   * @param  platformVersion
   *         A platform version to test against
   * @return Boolean representing if the add-on is compatible
   */
  isCompatibleWith(aAppVersion, aPlatformVersion) {
    return true;
  },

  /**
   * Starts an update check for this add-on. This will perform
   * asynchronously and deliver results to the given listener.
   *
   * @param  aListener
   *         An UpdateListener for the update process
   * @param  aReason
   *         A reason code for performing the update
   * @param  aAppVersion
   *         An application version to check for updates for
   * @param  aPlatformVersion
   *         A platform version to check for updates for
   */
  findUpdates(aListener, aReason, aAppVersion, aPlatformVersion) {
    if ("onNoCompatibilityUpdateAvailable" in aListener)
      aListener.onNoCompatibilityUpdateAvailable(this);
    if ("onNoUpdateAvailable" in aListener)
      aListener.onNoUpdateAvailable(this);
    if ("onUpdateFinished" in aListener)
      aListener.onUpdateFinished(this);
  },

  toJSON() {
    let json = {};

    for (let property of Object.keys(this)) {
      let value = this[property];
      if (property.startsWith("_") ||
          typeof(value) === "function")
        continue;

      try {
        switch (property) {
          case "sourceURI":
            json.sourceURI = value ? value.spec : "";
            break;

          case "updateDate":
            json.updateDate = value ? value.getTime() : "";
            break;

          default:
            json[property] = value;
        }
      } catch (ex) {
        logger.warn("Error writing property value for " + property);
      }
    }

    for (let property of Object.keys(this._unsupportedProperties)) {
      let value = this._unsupportedProperties[property];
      if (!property.startsWith("_"))
        json[property] = value;
    }

    return json;
  }
};

/**
 * The add-on repository is a source of add-ons that can be installed. It can
 * be searched in three ways. The first takes a list of IDs and returns a
 * list of the corresponding add-ons. The second returns a list of add-ons that
 * come highly recommended. This list should change frequently. The third is to
 * search for specific search terms entered by the user. Searches are
 * asynchronous and results should be passed to the provided callback object
 * when complete. The results passed to the callback should only include add-ons
 * that are compatible with the current application and are not already
 * installed.
 */
this.AddonRepository = {
  /**
   * Whether caching is currently enabled
   */
  get cacheEnabled() {
    let preference = PREF_GETADDONS_CACHE_ENABLED;
    return Services.prefs.getBoolPref(preference, false);
  },

  // A cache of the add-ons stored in the database
  _addons: null,

  // Whether a search is currently in progress
  _searching: false,

  // XHR associated with the current request
  _request: null,

  /*
   * Addon search results callback object that contains two functions
   *
   * searchSucceeded - Called when a search has suceeded.
   *
   * @param  aAddons
   *         An array of the add-on results. In the case of searching for
   *         specific terms the ordering of results may be determined by
   *         the search provider.
   * @param  aAddonCount
   *         The length of aAddons
   * @param  aTotalResults
   *         The total results actually available in the repository
   *
   *
   * searchFailed - Called when an error occurred when performing a search.
   */
  _callback: null,

  // Maximum number of results to return
  _maxResults: null,

  /**
   * Shut down AddonRepository
   * return: promise{integer} resolves with the result of flushing
   *         the AddonRepository database
   */
  shutdown() {
    this.cancelSearch();

    this._addons = null;
    return AddonDatabase.shutdown(false);
  },

  metadataAge() {
    let now = Math.round(Date.now() / 1000);

    let lastUpdate = Services.prefs.getIntPref(PREF_METADATA_LASTUPDATE, 0);

    // Handle clock jumps
    if (now < lastUpdate) {
      return now;
    }
    return now - lastUpdate;
  },

  isMetadataStale() {
    let threshold = Services.prefs.getIntPref(PREF_METADATA_UPDATETHRESHOLD_SEC, DEFAULT_METADATA_UPDATETHRESHOLD_SEC);
    return (this.metadataAge() > threshold);
  },

  /**
   * Asynchronously get a cached add-on by id. The add-on (or null if the
   * add-on is not found) is passed to the specified callback. If caching is
   * disabled, null is passed to the specified callback.
   *
   * @param  aId
   *         The id of the add-on to get
   * @param  aCallback
   *         The callback to pass the result back to
   */
  async getCachedAddonByID(aId, aCallback) {
    if (!aId || !this.cacheEnabled) {
      aCallback(null);
      return;
    }

    function getAddon(aAddons) {
      aCallback(aAddons.get(aId) || null);
    }

    if (this._addons == null) {
      AddonDatabase.retrieveStoredData().then(aAddons => {
        this._addons = aAddons;
        getAddon(aAddons);
      });

      return;
    }

    getAddon(this._addons);
  },

  /**
   * Asynchronously repopulate cache so it only contains the add-ons
   * corresponding to the specified ids. If caching is disabled,
   * the cache is completely removed.
   *
   * @param  aTimeout
   *         (Optional) timeout in milliseconds to abandon the XHR request
   *         if we have not received a response from the server.
   * @return Promise{null}
   *         Resolves when the metadata ping is complete
   */
  repopulateCache(aTimeout) {
    return this._repopulateCacheInternal(false, aTimeout);
  },

  /*
   * Clear and delete the AddonRepository database
   * @return Promise{null} resolves when the database is deleted
   */
  _clearCache() {
    this._addons = null;
    return AddonDatabase.delete().then(() =>
      AddonManagerPrivate.updateAddonRepositoryData());
  },

  async _repopulateCacheInternal(aSendPerformance, aTimeout) {
    let allAddons = await AddonManager.getAllAddons();

    // Filter the hotfix out of our list of add-ons
    allAddons = allAddons.filter(a => a.id != AddonManager.hotfixID);

    // Completely remove cache if caching is not enabled
    if (!this.cacheEnabled) {
      logger.debug("Clearing cache because it is disabled");
      await this._clearCache();
      return;
    }

    let ids = allAddons.map(a => a.id);
    logger.debug("Repopulate add-on cache with " + ids.toSource());

    let addonsToCache = await getAddonsToCache(ids);

    // Completely remove cache if there are no add-ons to cache
    if (addonsToCache.length == 0) {
      logger.debug("Clearing cache because 0 add-ons were requested");
      await this._clearCache();
      return;
    }

    await new Promise((resolve, reject) =>
      this._beginGetAddons(addonsToCache, {
        searchSucceeded: aAddons => {
          this._addons = new Map();
          for (let addon of aAddons) {
            this._addons.set(addon.id, addon);
          }
          AddonDatabase.repopulate(aAddons, resolve);
        },
        searchFailed: () => {
          logger.warn("Search failed when repopulating cache");
          resolve();
        }
      }, aSendPerformance, aTimeout));

    // Always call AddonManager updateAddonRepositoryData after we refill the cache
    await AddonManagerPrivate.updateAddonRepositoryData();
  },

  /**
   * Asynchronously add add-ons to the cache corresponding to the specified
   * ids. If caching is disabled, the cache is unchanged and the callback is
   * immediately called if it is defined.
   *
   * @param  aIds
   *         The array of add-on ids to add to the cache
   * @param  aCallback
   *         The optional callback to call once complete
   */
  cacheAddons(aIds, aCallback) {
    logger.debug("cacheAddons: enabled " + this.cacheEnabled + " IDs " + aIds.toSource());
    if (!this.cacheEnabled) {
      if (aCallback)
        aCallback();
      return;
    }

    getAddonsToCache(aIds).then(aAddons => {
      // If there are no add-ons to cache, act as if caching is disabled
      if (aAddons.length == 0) {
        if (aCallback)
          aCallback();
        return;
      }

      this.getAddonsByIDs(aAddons, {
        searchSucceeded: aAddons => {
          for (let addon of aAddons) {
            this._addons.set(addon.id, addon);
          }
          AddonDatabase.insertAddons(aAddons, aCallback);
        },
        searchFailed: () => {
          logger.warn("Search failed when adding add-ons to cache");
          if (aCallback)
            aCallback();
        }
      });
    });
  },

  /**
   * The homepage for visiting this repository. If the corresponding preference
   * is not defined, defaults to about:blank.
   */
  get homepageURL() {
    let url = this._formatURLPref(PREF_GETADDONS_BROWSEADDONS, {});
    return (url != null) ? url : "about:blank";
  },

  /**
   * Returns whether this instance is currently performing a search. New
   * searches will not be performed while this is the case.
   */
  get isSearching() {
    return this._searching;
  },

  /**
   * The url that can be visited to see recommended add-ons in this repository.
   * If the corresponding preference is not defined, defaults to about:blank.
   */
  getRecommendedURL() {
    let url = this._formatURLPref(PREF_GETADDONS_BROWSERECOMMENDED, {});
    return (url != null) ? url : "about:blank";
  },

  /**
   * Retrieves the url that can be visited to see search results for the given
   * terms. If the corresponding preference is not defined, defaults to
   * about:blank.
   *
   * @param  aSearchTerms
   *         Search terms used to search the repository
   */
  getSearchURL(aSearchTerms) {
    let url = this._formatURLPref(PREF_GETADDONS_BROWSESEARCHRESULTS, {
      TERMS: encodeURIComponent(aSearchTerms)
    });
    return (url != null) ? url : "about:blank";
  },

  /**
   * Cancels the search in progress. If there is no search in progress this
   * does nothing.
   */
  cancelSearch() {
    this._searching = false;
    if (this._request) {
      this._request.abort();
      this._request = null;
    }
    this._callback = null;
  },

  /**
   * Begins a search for add-ons in this repository by ID. Results will be
   * passed to the given callback.
   *
   * @param  aIDs
   *         The array of ids to search for
   * @param  aCallback
   *         The callback to pass results to
   */
  getAddonsByIDs(aIDs, aCallback) {
    return this._beginGetAddons(aIDs, aCallback, false);
  },

  /**
   * Begins a search of add-ons, potentially sending performance data.
   *
   * @param  aIDs
   *         Array of ids to search for.
   * @param  aCallback
   *         Function to pass results to.
   * @param  aSendPerformance
   *         Boolean indicating whether to send performance data with the
   *         request.
   * @param  aTimeout
   *         (Optional) timeout in milliseconds to abandon the XHR request
   *         if we have not received a response from the server.
   */
  _beginGetAddons(aIDs, aCallback, aSendPerformance, aTimeout) {
    let ids = aIDs.slice(0);

    let params = {
      API_VERSION,
      IDS: ids.map(encodeURIComponent).join(",")
    };

    let pref = PREF_GETADDONS_BYIDS;

    if (aSendPerformance) {
      let type = Services.prefs.getPrefType(PREF_GETADDONS_BYIDS_PERFORMANCE);
      if (type == Services.prefs.PREF_STRING) {
        pref = PREF_GETADDONS_BYIDS_PERFORMANCE;

        let startupInfo = Services.startup.getStartupInfo();

        params.TIME_MAIN = "";
        params.TIME_FIRST_PAINT = "";
        params.TIME_SESSION_RESTORED = "";
        if (startupInfo.process) {
          if (startupInfo.main) {
            params.TIME_MAIN = startupInfo.main - startupInfo.process;
          }
          if (startupInfo.firstPaint) {
            params.TIME_FIRST_PAINT = startupInfo.firstPaint -
                                      startupInfo.process;
          }
          if (startupInfo.sessionRestored) {
            params.TIME_SESSION_RESTORED = startupInfo.sessionRestored -
                                           startupInfo.process;
          }
        }
      }
    }

    let url = this._formatURLPref(pref, params);

    let handleResults = (aElements, aTotalResults, aCompatData) => {
      // Don't use this._parseAddons() so that, for example,
      // incompatible add-ons are not filtered out
      let results = [];
      for (let i = 0; i < aElements.length && results.length < this._maxResults; i++) {
        let result = this._parseAddon(aElements[i], null, aCompatData);
        if (result == null)
          continue;

        // Ignore add-on if it wasn't actually requested
        let idIndex = ids.indexOf(result.addon.id);
        if (idIndex == -1)
          continue;

        // Ignore add-on if the add-on manager doesn't know about its type:
        if (!(result.addon.type in AddonManager.addonTypes)) {
          continue;
        }

        results.push(result);
        // Ignore this add-on from now on
        ids.splice(idIndex, 1);
      }

      // Include any compatibility overrides for addons not hosted by the
      // remote repository.
      for (let id in aCompatData) {
        let addonCompat = aCompatData[id];
        if (addonCompat.hosted)
          continue;

        let addon = new AddonSearchResult(addonCompat.id);
        // Compatibility overrides can only be for extensions.
        addon.type = "extension";
        addon.compatibilityOverrides = addonCompat.compatRanges;
        let result = {
          addon,
          xpiURL: null,
          xpiHash: null
        };
        results.push(result);
      }

      // aTotalResults irrelevant
      this._reportSuccess(results, -1);
    };

    this._beginSearch(url, ids.length, aCallback, handleResults, aTimeout);
  },

  /**
   * Performs the daily background update check.
   *
   * This API both searches for the add-on IDs specified and sends performance
   * data. It is meant to be called as part of the daily update ping. It should
   * not be used for any other purpose. Use repopulateCache instead.
   *
   * @return Promise{null} Resolves when the metadata update is complete.
   */
  backgroundUpdateCheck() {
    return this._repopulateCacheInternal(true);
  },

  /**
   * Begins a search for recommended add-ons in this repository. Results will
   * be passed to the given callback.
   *
   * @param  aMaxResults
   *         The maximum number of results to return
   * @param  aCallback
   *         The callback to pass results to
   */
  retrieveRecommendedAddons(aMaxResults, aCallback) {
    let url = this._formatURLPref(PREF_GETADDONS_GETRECOMMENDED, {
      API_VERSION,

      // Get twice as many results to account for potential filtering
      MAX_RESULTS: 2 * aMaxResults
    });

    let handleResults = (aElements, aTotalResults) => {
      this._getLocalAddonIds(aLocalAddonIds => {
        // aTotalResults irrelevant
        this._parseAddons(aElements, -1, aLocalAddonIds);
      });
    };

    this._beginSearch(url, aMaxResults, aCallback, handleResults);
  },

  /**
   * Begins a search for add-ons in this repository. Results will be passed to
   * the given callback.
   *
   * @param  aSearchTerms
   *         The terms to search for
   * @param  aMaxResults
   *         The maximum number of results to return
   * @param  aCallback
   *         The callback to pass results to
   */
  searchAddons(aSearchTerms, aMaxResults, aCallback) {
    let compatMode = "normal";
    if (!AddonManager.checkCompatibility)
      compatMode = "ignore";
    else if (AddonManager.strictCompatibility)
      compatMode = "strict";

    let substitutions = {
      API_VERSION,
      TERMS: encodeURIComponent(aSearchTerms),
      // Get twice as many results to account for potential filtering
      MAX_RESULTS: 2 * aMaxResults,
      COMPATIBILITY_MODE: compatMode,
    };

    let url = this._formatURLPref(PREF_GETADDONS_GETSEARCHRESULTS, substitutions);

    let handleResults = (aElements, aTotalResults) => {
      this._getLocalAddonIds(aLocalAddonIds => {
        this._parseAddons(aElements, aTotalResults, aLocalAddonIds);
      });
    };

    this._beginSearch(url, aMaxResults, aCallback, handleResults);
  },

  // Posts results to the callback
  _reportSuccess(aResults, aTotalResults) {
    this._searching = false;
    this._request = null;
    // The callback may want to trigger a new search so clear references early
    let addons = aResults.map(result => result.addon);
    let callback = this._callback;
    this._callback = null;
    callback.searchSucceeded(addons, addons.length, aTotalResults);
  },

  // Notifies the callback of a failure
  _reportFailure() {
    this._searching = false;
    this._request = null;
    // The callback may want to trigger a new search so clear references early
    let callback = this._callback;
    this._callback = null;
    callback.searchFailed();
  },

  // Get descendant by unique tag name. Returns null if not unique tag name.
  _getUniqueDescendant(aElement, aTagName) {
    let elementsList = aElement.getElementsByTagName(aTagName);
    return (elementsList.length == 1) ? elementsList[0] : null;
  },

  // Get direct descendant by unique tag name.
  // Returns null if not unique tag name.
  _getUniqueDirectDescendant(aElement, aTagName) {
    let elementsList = Array.filter(aElement.children,
                                    aChild => aChild.tagName == aTagName);
    return (elementsList.length == 1) ? elementsList[0] : null;
  },

  // Parse out trimmed text content. Returns null if text content empty.
  _getTextContent(aElement) {
    let textContent = aElement.textContent.trim();
    return (textContent.length > 0) ? textContent : null;
  },

  // Parse out trimmed text content of a descendant with the specified tag name
  // Returns null if the parsing unsuccessful.
  _getDescendantTextContent(aElement, aTagName) {
    let descendant = this._getUniqueDescendant(aElement, aTagName);
    return (descendant != null) ? this._getTextContent(descendant) : null;
  },

  // Parse out trimmed text content of a direct descendant with the specified
  // tag name.
  // Returns null if the parsing unsuccessful.
  _getDirectDescendantTextContent(aElement, aTagName) {
    let descendant = this._getUniqueDirectDescendant(aElement, aTagName);
    return (descendant != null) ? this._getTextContent(descendant) : null;
  },

  /*
   * Creates an AddonSearchResult by parsing an <addon> element
   *
   * @param  aElement
   *         The <addon> element to parse
   * @param  aSkip
   *         Object containing ids and sourceURIs of add-ons to skip.
   * @param  aCompatData
   *         Array of parsed addon_compatibility elements to accosiate with the
   *         resulting AddonSearchResult. Optional.
   * @return Result object containing the parsed AddonSearchResult, xpiURL and
   *         xpiHash if the parsing was successful. Otherwise returns null.
   */
  _parseAddon(aElement, aSkip, aCompatData) {
    let skipIDs = (aSkip && aSkip.ids) ? aSkip.ids : [];
    let skipSourceURIs = (aSkip && aSkip.sourceURIs) ? aSkip.sourceURIs : [];

    let guid = this._getDescendantTextContent(aElement, "guid");
    if (guid == null || skipIDs.indexOf(guid) != -1)
      return null;

    let addon = new AddonSearchResult(guid);
    let result = {
      addon,
      xpiURL: null,
      xpiHash: null
    };

    if (aCompatData && guid in aCompatData)
      addon.compatibilityOverrides = aCompatData[guid].compatRanges;

    for (let node = aElement.firstChild; node; node = node.nextSibling) {
      if (!(node instanceof Ci.nsIDOMElement))
        continue;

      let localName = node.localName;

      // Handle case where the wanted string value is located in text content
      // but only if the content is not empty
      if (localName in STRING_KEY_MAP) {
        addon[STRING_KEY_MAP[localName]] = this._getTextContent(node) || addon[STRING_KEY_MAP[localName]];
        continue;
      }

      // Handle case where the wanted string value is html located in text content
      if (localName in HTML_KEY_MAP) {
        addon[HTML_KEY_MAP[localName]] = convertHTMLToPlainText(this._getTextContent(node));
        continue;
      }

      // Handle case where the wanted integer value is located in text content
      if (localName in INTEGER_KEY_MAP) {
        let value = parseInt(this._getTextContent(node));
        if (value >= 0)
          addon[INTEGER_KEY_MAP[localName]] = value;
        continue;
      }

      // Handle cases that aren't as simple as grabbing the text content
      switch (localName) {
        case "type":
          // Map AMO's type id to corresponding string
          // https://github.com/mozilla/olympia/blob/master/apps/constants/base.py#L127
          // These definitions need to be updated whenever AMO adds a new type.
          let id = parseInt(node.getAttribute("id"));
          switch (id) {
            case 1:
              addon.type = "extension";
              break;
            case 2:
              addon.type = "theme";
              break;
            case 3:
              addon.type = "dictionary";
              break;
            case 4:
              addon.type = "search";
              break;
            case 5:
            case 6:
              addon.type = "locale";
              break;
            case 7:
              addon.type = "plugin";
              break;
            case 8:
              addon.type = "api";
              break;
            case 9:
              addon.type = "lightweight-theme";
              break;
            case 11:
              addon.type = "webapp";
              break;
            default:
              logger.info("Unknown type id " + id + " found when parsing response for GUID " + guid);
          }
          break;
        case "authors":
          let authorNodes = node.getElementsByTagName("author");
          for (let authorNode of authorNodes) {
            let name = this._getDescendantTextContent(authorNode, "name");
            let link = this._getDescendantTextContent(authorNode, "link");
            if (name == null || link == null)
              continue;

            let author = new AddonManagerPrivate.AddonAuthor(name, link);
            if (addon.creator == null)
              addon.creator = author;
            else {
              if (addon.developers == null)
                addon.developers = [];

              addon.developers.push(author);
            }
          }
          break;
        case "previews":
          let previewNodes = node.getElementsByTagName("preview");
          for (let previewNode of previewNodes) {
            let full = this._getUniqueDescendant(previewNode, "full");
            if (full == null)
              continue;

            let fullURL = this._getTextContent(full);
            let fullWidth = full.getAttribute("width");
            let fullHeight = full.getAttribute("height");

            let thumbnailURL, thumbnailWidth, thumbnailHeight;
            let thumbnail = this._getUniqueDescendant(previewNode, "thumbnail");
            if (thumbnail) {
              thumbnailURL = this._getTextContent(thumbnail);
              thumbnailWidth = thumbnail.getAttribute("width");
              thumbnailHeight = thumbnail.getAttribute("height");
            }
            let caption = this._getDescendantTextContent(previewNode, "caption");
            let screenshot = new AddonManagerPrivate.AddonScreenshot(fullURL, fullWidth, fullHeight,
                                                                     thumbnailURL, thumbnailWidth,
                                                                     thumbnailHeight, caption);

            if (addon.screenshots == null)
              addon.screenshots = [];

            if (previewNode.getAttribute("primary") == 1)
              addon.screenshots.unshift(screenshot);
            else
              addon.screenshots.push(screenshot);
          }
          break;
        case "learnmore":
          addon.learnmoreURL = this._getTextContent(node);
          addon.homepageURL = addon.homepageURL || addon.learnmoreURL;
          break;
        case "contribution_data":
          let meetDevelopers = this._getDescendantTextContent(node, "meet_developers");
          let suggestedAmount = this._getDescendantTextContent(node, "suggested_amount");
          if (meetDevelopers != null) {
            addon.contributionURL = meetDevelopers;
            addon.contributionAmount = suggestedAmount;
          }
          break;
        case "payment_data":
          let link = this._getDescendantTextContent(node, "link");
          let amountTag = this._getUniqueDescendant(node, "amount");
          let amount = parseFloat(amountTag.getAttribute("amount"));
          let displayAmount = this._getTextContent(amountTag);
          if (link != null && amount != null && displayAmount != null) {
            addon.purchaseURL = link;
            addon.purchaseAmount = amount;
            addon.purchaseDisplayAmount = displayAmount;
          }
          break;
        case "rating":
          let averageRating = parseInt(this._getTextContent(node));
          if (averageRating >= 0)
            addon.averageRating = Math.min(5, averageRating);
          break;
        case "reviews":
          let url = this._getTextContent(node);
          let num = parseInt(node.getAttribute("num"));
          if (url != null && num >= 0) {
            addon.reviewURL = url;
            addon.reviewCount = num;
          }
          break;
        case "status":
          let repositoryStatus = parseInt(node.getAttribute("id"));
          if (!isNaN(repositoryStatus))
            addon.repositoryStatus = repositoryStatus;
          break;
        case "all_compatible_os":
          let nodes = node.getElementsByTagName("os");
          addon.isPlatformCompatible = Array.some(nodes, function(aNode) {
            let text = aNode.textContent.toLowerCase().trim();
            return text == "all" || text == Services.appinfo.OS.toLowerCase();
          });
          break;
        case "install":
          // No os attribute means the xpi is compatible with any os
          if (node.hasAttribute("os")) {
            let os = node.getAttribute("os").trim().toLowerCase();
            // If the os is not ALL and not the current OS then ignore this xpi
            if (os != "all" && os != Services.appinfo.OS.toLowerCase())
              break;
          }

          let xpiURL = this._getTextContent(node);
          if (xpiURL == null)
            break;

          if (skipSourceURIs.indexOf(xpiURL) != -1)
            return null;

          result.xpiURL = xpiURL;
          addon.sourceURI = NetUtil.newURI(xpiURL);

          let size = parseInt(node.getAttribute("size"));
          addon.size = (size >= 0) ? size : null;

          let xpiHash = node.getAttribute("hash");
          if (xpiHash != null)
            xpiHash = xpiHash.trim();
          result.xpiHash = xpiHash ? xpiHash : null;
          break;
        case "last_updated":
          let epoch = parseInt(node.getAttribute("epoch"));
          if (!isNaN(epoch))
            addon.updateDate = new Date(1000 * epoch);
          break;
        case "icon":
          addon.icons[node.getAttribute("size")] = this._getTextContent(node);
          break;
      }
    }

    return result;
  },

  _parseAddons(aElements, aTotalResults, aSkip) {
    let results = [];

    let isSameApplication = aAppNode => this._getTextContent(aAppNode) == Services.appinfo.ID;

    for (let i = 0; i < aElements.length && results.length < this._maxResults; i++) {
      let element = aElements[i];

      let tags = this._getUniqueDescendant(element, "compatible_applications");
      if (tags == null)
        continue;

      let applications = tags.getElementsByTagName("appID");
      let compatible = Array.some(applications, aAppNode => {
        if (!isSameApplication(aAppNode))
          return false;

        let parent = aAppNode.parentNode;
        let minVersion = this._getDescendantTextContent(parent, "min_version");
        let maxVersion = this._getDescendantTextContent(parent, "max_version");
        if (minVersion == null || maxVersion == null)
          return false;

        let currentVersion = Services.appinfo.version;
        return (Services.vc.compare(minVersion, currentVersion) <= 0 &&
                ((!AddonManager.strictCompatibility) ||
                 Services.vc.compare(currentVersion, maxVersion) <= 0));
      });

      // Ignore add-ons not compatible with this Application
      if (!compatible) {
        if (AddonManager.checkCompatibility)
          continue;

        if (!Array.some(applications, isSameApplication))
          continue;
      }

      // Add-on meets all requirements, so parse out data.
      // Don't pass in compatiblity override data, because that's only returned
      // in GUID searches, which don't use _parseAddons().
      let result = this._parseAddon(element, aSkip);
      if (result == null)
        continue;

      // Ignore add-on missing a required attribute
      let requiredAttributes = ["id", "name", "version", "type", "creator"];
      if (requiredAttributes.some(aAttribute => !result.addon[aAttribute]))
        continue;

      // Ignore add-on with a type AddonManager doesn't understand:
      if (!(result.addon.type in AddonManager.addonTypes))
        continue;

      // Add only if the add-on is compatible with the platform
      if (!result.addon.isPlatformCompatible)
        continue;

      // Add only if there was an xpi compatible with this OS or there was a
      // way to purchase the add-on
      if (!result.xpiURL && !result.addon.purchaseURL)
        continue;

      result.addon.isCompatible = compatible;

      results.push(result);
      // Ignore this add-on from now on by adding it to the skip array
      aSkip.ids.push(result.addon.id);
    }

    // Immediately report success if no AddonInstall instances to create
    let pendingResults = results.length;
    if (pendingResults == 0) {
      this._reportSuccess(results, aTotalResults);
      return;
    }

    // Create an AddonInstall for each result
    for (let result of results) {
      let addon = result.addon;
      let callback = aInstall => {
        addon.install = aInstall;
        pendingResults--;
        if (pendingResults == 0)
          this._reportSuccess(results, aTotalResults);
      };

      if (result.xpiURL) {
        AddonManager.getInstallForURL(result.xpiURL, callback,
                                      "application/x-xpinstall", result.xpiHash,
                                      addon.name, addon.icons, addon.version);
      } else {
        callback(null);
      }
    }
  },

  // Parses addon_compatibility nodes, that describe compatibility overrides.
  _parseAddonCompatElement(aResultObj, aElement) {
    let guid = this._getDescendantTextContent(aElement, "guid");
    if (!guid) {
        logger.debug("Compatibility override is missing guid.");
      return;
    }

    let compat = {id: guid};
    compat.hosted = aElement.getAttribute("hosted") != "false";

    function findMatchingAppRange(aNodes) {
      let toolkitAppRange = null;
      for (let node of aNodes) {
        let appID = this._getDescendantTextContent(node, "appID");
        if (appID != Services.appinfo.ID && appID != TOOLKIT_ID)
          continue;

        let minVersion = this._getDescendantTextContent(node, "min_version");
        let maxVersion = this._getDescendantTextContent(node, "max_version");
        if (minVersion == null || maxVersion == null)
          continue;

        let appRange = { appID,
                         appMinVersion: minVersion,
                         appMaxVersion: maxVersion };

        // Only use Toolkit app ranges if no ranges match the application ID.
        if (appID == TOOLKIT_ID)
          toolkitAppRange = appRange;
        else
          return appRange;
      }
      return toolkitAppRange;
    }

    function parseRangeNode(aNode) {
      let type = aNode.getAttribute("type");
      // Only "incompatible" (blacklisting) is supported for now.
      if (type != "incompatible") {
        logger.debug("Compatibility override of unsupported type found.");
        return null;
      }

      let override = new AddonManagerPrivate.AddonCompatibilityOverride(type);

      override.minVersion = this._getDirectDescendantTextContent(aNode, "min_version");
      override.maxVersion = this._getDirectDescendantTextContent(aNode, "max_version");

      if (!override.minVersion) {
        logger.debug("Compatibility override is missing min_version.");
        return null;
      }
      if (!override.maxVersion) {
        logger.debug("Compatibility override is missing max_version.");
        return null;
      }

      let appRanges = aNode.querySelectorAll("compatible_applications > application");
      let appRange = findMatchingAppRange.bind(this)(appRanges);
      if (!appRange) {
        logger.debug("Compatibility override is missing a valid application range.");
        return null;
      }

      override.appID = appRange.appID;
      override.appMinVersion = appRange.appMinVersion;
      override.appMaxVersion = appRange.appMaxVersion;

      return override;
    }

    let rangeNodes = aElement.querySelectorAll("version_ranges > version_range");
    compat.compatRanges = Array.map(rangeNodes, parseRangeNode.bind(this))
                               .filter(aItem => !!aItem);
    if (compat.compatRanges.length == 0)
      return;

    aResultObj[compat.id] = compat;
  },

  // Parses addon_compatibility elements.
  _parseAddonCompatData(aElements) {
    let compatData = {};
    Array.forEach(aElements, this._parseAddonCompatElement.bind(this, compatData));
    return compatData;
  },

  // Begins a new search if one isn't currently executing
  _beginSearch(aURI, aMaxResults, aCallback, aHandleResults, aTimeout) {
    if (this._searching || aURI == null || aMaxResults <= 0) {
      logger.warn("AddonRepository search failed: searching " + this._searching + " aURI " + aURI +
                  " aMaxResults " + aMaxResults);
      aCallback.searchFailed();
      return;
    }

    this._searching = true;
    this._callback = aCallback;
    this._maxResults = aMaxResults;

    logger.debug("Requesting " + aURI);

    this._request = new ServiceRequest();
    this._request.mozBackgroundRequest = true;
    this._request.open("GET", aURI, true);
    this._request.overrideMimeType("text/xml");
    if (aTimeout) {
      this._request.timeout = aTimeout;
    }

    this._request.addEventListener("error", aEvent => this._reportFailure());
    this._request.addEventListener("timeout", aEvent => this._reportFailure());
    this._request.addEventListener("load", aEvent => {
      logger.debug("Got metadata search load event");
      let request = aEvent.target;
      let responseXML = request.responseXML;

      if (!responseXML || responseXML.documentElement.namespaceURI == XMLURI_PARSE_ERROR ||
          (request.status != 200 && request.status != 0)) {
        this._reportFailure();
        return;
      }

      let documentElement = responseXML.documentElement;
      let elements = documentElement.getElementsByTagName("addon");
      let totalResults = elements.length;
      let parsedTotalResults = parseInt(documentElement.getAttribute("total_results"));
      // Parsed value of total results only makes sense if >= elements.length
      if (parsedTotalResults >= totalResults)
        totalResults = parsedTotalResults;

      let compatElements = documentElement.getElementsByTagName("addon_compatibility");
      let compatData = this._parseAddonCompatData(compatElements);

      aHandleResults(elements, totalResults, compatData);
    });
    this._request.send(null);
  },

  // Gets the id's of local add-ons, and the sourceURI's of local installs,
  // passing the results to aCallback
  _getLocalAddonIds(aCallback) {
    let localAddonIds = {ids: null, sourceURIs: null};

    AddonManager.getAllAddons(function(aAddons) {
      localAddonIds.ids = aAddons.map(a => a.id);
      if (localAddonIds.sourceURIs)
        aCallback(localAddonIds);
    });

    AddonManager.getAllInstalls(function(aInstalls) {
      localAddonIds.sourceURIs = [];
      for (let install of aInstalls) {
        if (install.state != AddonManager.STATE_AVAILABLE)
          localAddonIds.sourceURIs.push(install.sourceURI.spec);
      }

      if (localAddonIds.ids)
        aCallback(localAddonIds);
    });
  },

  // Create url from preference, returning null if preference does not exist
  _formatURLPref(aPreference, aSubstitutions) {
    let url = Services.prefs.getCharPref(aPreference, "");
    if (!url) {
      logger.warn("_formatURLPref: Couldn't get pref: " + aPreference);
      return null;
    }

    url = url.replace(/%([A-Z_]+)%/g, function(aMatch, aKey) {
      return (aKey in aSubstitutions) ? aSubstitutions[aKey] : aMatch;
    });

    return Services.urlFormatter.formatURL(url);
  },

  // Find a AddonCompatibilityOverride that matches a given aAddonVersion and
  // application/platform version.
  findMatchingCompatOverride(aAddonVersion,
                                                                     aCompatOverrides,
                                                                     aAppVersion,
                                                                     aPlatformVersion) {
    for (let override of aCompatOverrides) {

      let appVersion = null;
      if (override.appID == TOOLKIT_ID)
        appVersion = aPlatformVersion || Services.appinfo.platformVersion;
      else
        appVersion = aAppVersion || Services.appinfo.version;

      if (Services.vc.compare(override.minVersion, aAddonVersion) <= 0 &&
          Services.vc.compare(aAddonVersion, override.maxVersion) <= 0 &&
          Services.vc.compare(override.appMinVersion, appVersion) <= 0 &&
          Services.vc.compare(appVersion, override.appMaxVersion) <= 0) {
        return override;
      }
    }
    return null;
  },

  flush() {
    return AddonDatabase.flush();
  }
};

var AddonDatabase = {
  connectionPromise: null,
  _saveTask: null,
  _blockerAdded: false,

  // the in-memory database
  DB: BLANK_DB(),

  /**
   * A getter to retrieve the path to the DB
   */
  get jsonFile() {
    return OS.Path.join(OS.Constants.Path.profileDir, FILE_DATABASE);
 },

  /**
   * Asynchronously opens a new connection to the database file.
   *
   * @return {Promise} a promise that resolves to the database.
   */
  openConnection() {
    if (!this.connectionPromise) {
     this.connectionPromise = (async () => {
       this.DB = BLANK_DB();

       let inputDB, schema;

       try {
         let data = await OS.File.read(this.jsonFile, { encoding: "utf-8"});
         inputDB = JSON.parse(data);

         if (!inputDB.hasOwnProperty("addons") ||
             !Array.isArray(inputDB.addons)) {
           throw new Error("No addons array.");
         }

         if (!inputDB.hasOwnProperty("schema")) {
           throw new Error("No schema specified.");
         }

         schema = parseInt(inputDB.schema, 10);

         if (!Number.isInteger(schema) ||
             schema < DB_MIN_JSON_SCHEMA) {
           throw new Error("Invalid schema value.");
         }
       } catch (e) {
         if (e instanceof OS.File.Error && e.becauseNoSuchFile) {
           logger.debug("No " + FILE_DATABASE + " found.");
         } else {
           logger.error(`Malformed ${FILE_DATABASE}: ${e} - resetting to empty`);
         }

         // Create a blank addons.json file
         this.save();

         Services.prefs.setIntPref(PREF_GETADDONS_DB_SCHEMA, DB_SCHEMA);
         return this.DB;
       }

       Services.prefs.setIntPref(PREF_GETADDONS_DB_SCHEMA, DB_SCHEMA);

       // We use _insertAddon manually instead of calling
       // insertAddons to avoid the write to disk which would
       // be a waste since this is the data that was just read.
       for (let addon of inputDB.addons) {
         this._insertAddon(addon);
       }

       return this.DB;
     })();
    }

    return this.connectionPromise;
  },

  /**
   * Asynchronously shuts down the database connection and releases all
   * cached objects
   *
   * @param  aCallback
   *         An optional callback to call once complete
   * @param  aSkipFlush
   *         An optional boolean to skip flushing data to disk. Useful
   *         when the database is going to be deleted afterwards.
   */
  shutdown(aSkipFlush) {
    if (!this.connectionPromise) {
      return Promise.resolve();
    }

    this.connectionPromise = null;

    if (aSkipFlush || !this._saveTask) {
      return Promise.resolve();
    }

    let promise = this._saveTask.finalize();
    this._saveTask = null;
    return promise;
  },

  /**
   * Asynchronously deletes the database, shutting down the connection
   * first if initialized
   *
   * @param  aCallback
   *         An optional callback to call once complete
   * @return Promise{null} resolves when the database has been deleted
   */
  delete(aCallback) {
    this.DB = BLANK_DB();

    if (this._saveTask) {
      this._saveTask.disarm();
      this._saveTask = null;
    }

    // shutdown(true) never rejects
    this._deleting = this.shutdown(true)
      .then(() => OS.File.remove(this.jsonFile, {}))
      .catch(error => logger.error("Unable to delete Addon Repository file " +
                                 this.jsonFile, error))
      .then(() => this._deleting = null)
      .then(aCallback);
    return this._deleting;
  },

  async _saveNow() {
    let json = {
      schema: this.DB.schema,
      addons: []
    };

    for (let [, value] of this.DB.addons)
      json.addons.push(value);

    await OS.File.writeAtomic(this.jsonFile, JSON.stringify(json),
                              {tmpPath: `${this.jsonFile}.tmp`});
  },

  save() {
    if (!this._saveTask) {
      this._saveTask = new DeferredTask(() => this._saveNow(), DB_BATCH_TIMEOUT_MS);

      if (!this._blockerAdded) {
        AsyncShutdown.profileBeforeChange.addBlocker(
          "Flush AddonRepository",
          async () => {
            if (!this._saveTask) {
              return;
            }
            await this._saveTask.finalize();
            this._saveTask = null;
          });
        this._blockerAdded = true;
      }
    }
    this._saveTask.arm();
  },

  /**
   * Flush any pending I/O on the addons.json file
   * @return: Promise{null}
   *          Resolves when the pending I/O (writing out or deleting
   *          addons.json) completes
   */
  flush() {
    if (this._deleting) {
      return this._deleting;
    }

    if (this._saveTask) {
      let promise = this._saveTask.finalize();
      this._saveTask = null;
      return promise;
    }

    return Promise.resolve();
  },

  /**
   * Asynchronously retrieve all add-ons from the database
   * @return: Promise{Map}
   *          Resolves when the add-ons are retrieved from the database
   */
  retrieveStoredData() {
    return this.openConnection().then(db => db.addons);
  },

  /**
   * Asynchronously repopulates the database so it only contains the
   * specified add-ons
   *
   * @param  aAddons
   *         The array of add-ons to repopulate the database with
   * @param  aCallback
   *         An optional callback to call once complete
   */
  repopulate(aAddons, aCallback) {
    this.DB.addons.clear();
    this.insertAddons(aAddons, function() {
      let now = Math.round(Date.now() / 1000);
      logger.debug("Cache repopulated, setting " + PREF_METADATA_LASTUPDATE + " to " + now);
      Services.prefs.setIntPref(PREF_METADATA_LASTUPDATE, now);
      if (aCallback)
        aCallback();
    });
  },

  /**
   * Asynchronously inserts an array of add-ons into the database
   *
   * @param  aAddons
   *         The array of add-ons to insert
   * @param  aCallback
   *         An optional callback to call once complete
   */
  async insertAddons(aAddons, aCallback) {
    await this.openConnection();

    for (let addon of aAddons) {
      this._insertAddon(addon);
    }

    this.save();
    aCallback && aCallback();
  },

  /**
   * Inserts an individual add-on into the database. If the add-on already
   * exists in the database (by id), then the specified add-on will not be
   * inserted.
   *
   * @param  aAddon
   *         The add-on to insert into the database
   * @param  aCallback
   *         The callback to call once complete
   */
  _insertAddon(aAddon) {
    let newAddon = this._parseAddon(aAddon);
    if (!newAddon ||
        !newAddon.id ||
        this.DB.addons.has(newAddon.id))
      return;

    this.DB.addons.set(newAddon.id, newAddon);
  },

  /*
   * Creates an AddonSearchResult by parsing an object structure
   * retrieved from the DB JSON representation.
   *
   * @param  aObj
   *         The object to parse
   * @return Returns an AddonSearchResult object.
   */
  _parseAddon(aObj) {
    if (aObj instanceof AddonSearchResult)
      return aObj;

    let id = aObj.id;
    if (!aObj.id)
      return null;

    let addon = new AddonSearchResult(id);

    for (let expectedProperty of Object.keys(AddonSearchResult.prototype)) {
      if (!(expectedProperty in aObj) ||
          typeof(aObj[expectedProperty]) === "function")
        continue;

      let value = aObj[expectedProperty];

      try {
        switch (expectedProperty) {
          case "sourceURI":
            addon.sourceURI = value ? NetUtil.newURI(value) : null;
            break;

          case "creator":
            addon.creator = value
                            ? this._makeDeveloper(value)
                            : null;
            break;

          case "updateDate":
            addon.updateDate = value ? new Date(value) : null;
            break;

          case "developers":
            if (!addon.developers) addon.developers = [];
            for (let developer of value) {
              addon.developers.push(this._makeDeveloper(developer));
            }
            break;

          case "screenshots":
            if (!addon.screenshots) addon.screenshots = [];
            for (let screenshot of value) {
              addon.screenshots.push(this._makeScreenshot(screenshot));
            }
            break;

          case "compatibilityOverrides":
            if (!addon.compatibilityOverrides) addon.compatibilityOverrides = [];
            for (let override of value) {
              addon.compatibilityOverrides.push(
                this._makeCompatOverride(override)
              );
            }
            break;

          case "icons":
            if (!addon.icons) addon.icons = {};
            for (let size of Object.keys(aObj.icons)) {
              addon.icons[size] = aObj.icons[size];
            }
            break;

          case "iconURL":
            break;

          default:
            addon[expectedProperty] = value;
        }
      } catch (ex) {
        logger.warn("Error in parsing property value for " + expectedProperty + " | " + ex);
      }

      // delete property from obj to indicate we've already
      // handled it. The remaining public properties will
      // be stored separately and just passed through to
      // be written back to the DB.
      delete aObj[expectedProperty];
    }

    // Copy remaining properties to a separate object
    // to prevent accidental access on downgraded versions.
    // The properties will be merged in the same object
    // prior to being written back through toJSON.
    for (let remainingProperty of Object.keys(aObj)) {
      switch (typeof(aObj[remainingProperty])) {
        case "boolean":
        case "number":
        case "string":
        case "object":
          // these types are accepted
          break;
        default:
          continue;
      }

      if (!remainingProperty.startsWith("_"))
        addon._unsupportedProperties[remainingProperty] =
          aObj[remainingProperty];
    }

    return addon;
  },

  /**
   * Make a developer object from a vanilla
   * JS object from the JSON database
   *
   * @param  aObj
   *         The JS object to use
   * @return The created developer
   */
  _makeDeveloper(aObj) {
    let name = aObj.name;
    let url = aObj.url;
    return new AddonManagerPrivate.AddonAuthor(name, url);
  },

  /**
   * Make a screenshot object from a vanilla
   * JS object from the JSON database
   *
   * @param  aObj
   *         The JS object to use
   * @return The created screenshot
   */
  _makeScreenshot(aObj) {
    let url = aObj.url;
    let width = aObj.width;
    let height = aObj.height;
    let thumbnailURL = aObj.thumbnailURL;
    let thumbnailWidth = aObj.thumbnailWidth;
    let thumbnailHeight = aObj.thumbnailHeight;
    let caption = aObj.caption;
    return new AddonManagerPrivate.AddonScreenshot(url, width, height, thumbnailURL,
                                                   thumbnailWidth, thumbnailHeight, caption);
  },

  /**
   * Make a CompatibilityOverride from a vanilla
   * JS object from the JSON database
   *
   * @param  aObj
   *         The JS object to use
   * @return The created CompatibilityOverride
   */
  _makeCompatOverride(aObj) {
    let type = aObj.type;
    let minVersion = aObj.minVersion;
    let maxVersion = aObj.maxVersion;
    let appID = aObj.appID;
    let appMinVersion = aObj.appMinVersion;
    let appMaxVersion = aObj.appMaxVersion;
    return new AddonManagerPrivate.AddonCompatibilityOverride(type,
                                                              minVersion,
                                                              maxVersion,
                                                              appID,
                                                              appMinVersion,
                                                              appMaxVersion);
  },
};
