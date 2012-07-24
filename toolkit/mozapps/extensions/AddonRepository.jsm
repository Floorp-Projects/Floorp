/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

var EXPORTED_SYMBOLS = [ "AddonRepository" ];

const PREF_GETADDONS_CACHE_ENABLED       = "extensions.getAddons.cache.enabled";
const PREF_GETADDONS_CACHE_TYPES         = "extensions.getAddons.cache.types";
const PREF_GETADDONS_CACHE_ID_ENABLED    = "extensions.%ID%.getAddons.cache.enabled"
const PREF_GETADDONS_BROWSEADDONS        = "extensions.getAddons.browseAddons";
const PREF_GETADDONS_BYIDS               = "extensions.getAddons.get.url";
const PREF_GETADDONS_BYIDS_PERFORMANCE   = "extensions.getAddons.getWithPerformance.url";
const PREF_GETADDONS_BROWSERECOMMENDED   = "extensions.getAddons.recommended.browseURL";
const PREF_GETADDONS_GETRECOMMENDED      = "extensions.getAddons.recommended.url";
const PREF_GETADDONS_BROWSESEARCHRESULTS = "extensions.getAddons.search.browseURL";
const PREF_GETADDONS_GETSEARCHRESULTS    = "extensions.getAddons.search.url";

const XMLURI_PARSE_ERROR  = "http://www.mozilla.org/newlayout/xml/parsererror.xml";

const API_VERSION = "1.5";
const DEFAULT_CACHE_TYPES = "extension,theme,locale,dictionary";

const KEY_PROFILEDIR = "ProfD";
const FILE_DATABASE  = "addons.sqlite";
const DB_SCHEMA      = 4;

const TOOLKIT_ID     = "toolkit@mozilla.org";

["LOG", "WARN", "ERROR"].forEach(function(aName) {
  this.__defineGetter__(aName, function logFuncGetter() {
    Components.utils.import("resource://gre/modules/AddonLogging.jsm");

    LogManager.getLogger("addons.repository", this);
    return this[aName];
  });
}, this);


// Add-on properties parsed out of AMO results
// Note: the 'install' property is added for results from
// retrieveRecommendedAddons and searchAddons
const PROP_SINGLE = ["id", "type", "name", "version", "creator", "description",
                     "fullDescription", "developerComments", "eula",
                     "homepageURL", "supportURL", "contributionURL",
                     "contributionAmount", "averageRating", "reviewCount",
                     "reviewURL", "totalDownloads", "weeklyDownloads",
                     "dailyUsers", "sourceURI", "repositoryStatus", "size",
                     "updateDate"];

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

function getAddonsToCache(aIds, aCallback) {
  try {
    var types = Services.prefs.getCharPref(PREF_GETADDONS_CACHE_TYPES);
  }
  catch (e) { }
  if (!types)
    types = DEFAULT_CACHE_TYPES;

  types = types.split(",");

  AddonManager.getAddonsByIDs(aIds, function getAddonsToCache_getAddonsByIDs(aAddons) {
    let enabledIds = [];
    for (var i = 0; i < aIds.length; i++) {
      var preference = PREF_GETADDONS_CACHE_ID_ENABLED.replace("%ID%", aIds[i]);
      try {
        if (!Services.prefs.getBoolPref(preference))
          continue;
      } catch(e) {
        // If the preference doesn't exist caching is enabled by default
      }

      // The add-ons manager may not know about this ID yet if it is a pending
      // install. In that case we'll just cache it regardless
      if (aAddons[i] && (types.indexOf(aAddons[i].type) == -1))
        continue;

      enabledIds.push(aIds[i]);
    }

    aCallback(enabledIds);
  });
}

function AddonSearchResult(aId) {
  this.id = aId;
  this.icons = {};
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
    return this.icons[32];
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
  isCompatibleWith: function ASR_isCompatibleWith(aAppVerison, aPlatformVersion) {
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
  findUpdates: function ASR_findUpdates(aListener, aReason, aAppVersion, aPlatformVersion) {
    if ("onNoCompatibilityUpdateAvailable" in aListener)
      aListener.onNoCompatibilityUpdateAvailable(this);
    if ("onNoUpdateAvailable" in aListener)
      aListener.onNoUpdateAvailable(this);
    if ("onUpdateFinished" in aListener)
      aListener.onUpdateFinished(this);
  }
}

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
var AddonRepository = {
  /**
   * Whether caching is currently enabled
   */
  get cacheEnabled() {
    // Act as though caching is disabled if there was an unrecoverable error
    // openning the database.
    if (!AddonDatabase.databaseOk)
      return false;

    let preference = PREF_GETADDONS_CACHE_ENABLED;
    let enabled = false;
    try {
      enabled = Services.prefs.getBoolPref(preference);
    } catch(e) {
      WARN("cacheEnabled: Couldn't get pref: " + preference);
    }

    return enabled;
  },

  // A cache of the add-ons stored in the database
  _addons: null,

  // An array of callbacks pending the retrieval of add-ons from AddonDatabase
  _pendingCallbacks: null,

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
   * Initialize AddonRepository.
   */
  initialize: function AddonRepo_initialize() {
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  /**
   * Observe xpcom-shutdown notification, so we can shutdown cleanly.
   */
  observe: function AddonRepo_observe(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      Services.obs.removeObserver(this, "xpcom-shutdown");
      this.shutdown();
    }
  },

  /**
   * Shut down AddonRepository
   */
  shutdown: function AddonRepo_shutdown() {
    this.cancelSearch();

    this._addons = null;
    this._pendingCallbacks = null;
    AddonDatabase.shutdown(function shutdown_databaseShutdown() {
      Services.obs.notifyObservers(null, "addon-repository-shutdown", null);
    });
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
  getCachedAddonByID: function AddonRepo_getCachedAddonByID(aId, aCallback) {
    if (!aId || !this.cacheEnabled) {
      aCallback(null);
      return;
    }

    let self = this;
    function getAddon(aAddons) {
      aCallback((aId in aAddons) ? aAddons[aId] : null);
    }

    if (this._addons == null) {
      if (this._pendingCallbacks == null) {
        // Data has not been retrieved from the database, so retrieve it
        this._pendingCallbacks = [];
        this._pendingCallbacks.push(getAddon);
        AddonDatabase.retrieveStoredData(function getCachedAddonByID_retrieveData(aAddons) {
          let pendingCallbacks = self._pendingCallbacks;

          // Check if cache was shutdown or deleted before callback was called
          if (pendingCallbacks == null)
            return;

          // Callbacks may want to trigger a other caching operations that may
          // affect _addons and _pendingCallbacks, so set to final values early
          self._pendingCallbacks = null;
          self._addons = aAddons;

          pendingCallbacks.forEach(function(aCallback) aCallback(aAddons));
        });

        return;
      }

      // Data is being retrieved from the database, so wait
      this._pendingCallbacks.push(getAddon);
      return;
    }

    // Data has been retrieved, so immediately return result
    getAddon(this._addons);
  },

  /**
   * Asynchronously repopulate cache so it only contains the add-ons
   * corresponding to the specified ids. If caching is disabled,
   * the cache is completely removed.
   *
   * @param  aIds
   *         The array of add-on ids to repopulate the cache with
   * @param  aCallback
   *         The optional callback to call once complete
   */
  repopulateCache: function AddonRepo_repopulateCache(aIds, aCallback) {
    this._repopulateCacheInternal(aIds, aCallback, false);
  },

  _repopulateCacheInternal: function AddonRepo_repopulateCacheInternal(aIds, aCallback, aSendPerformance) {
    // Completely remove cache if caching is not enabled
    if (!this.cacheEnabled) {
      this._addons = null;
      this._pendingCallbacks = null;
      AddonDatabase.delete(aCallback);
      return;
    }

    let self = this;
    getAddonsToCache(aIds, function repopulateCache_getAddonsToCache(aAddons) {
      // Completely remove cache if there are no add-ons to cache
      if (aAddons.length == 0) {
        self._addons = null;
        self._pendingCallbacks = null;
        AddonDatabase.delete(aCallback);
        return;
      }

      self._beginGetAddons(aAddons, {
        searchSucceeded: function repopulateCacheInternal_searchSucceeded(aAddons) {
          self._addons = {};
          aAddons.forEach(function(aAddon) { self._addons[aAddon.id] = aAddon; });
          AddonDatabase.repopulate(aAddons, aCallback);
        },
        searchFailed: function repopulateCacheInternal_searchFailed() {
          WARN("Search failed when repopulating cache");
          if (aCallback)
            aCallback();
        }
      }, aSendPerformance);
    });
  },

  /**
   * Asynchronously add add-ons to the cache corresponding to the specified
   * ids. If caching is disabled, the cache is unchanged and the callback is
   * immediatly called if it is defined.
   *
   * @param  aIds
   *         The array of add-on ids to add to the cache
   * @param  aCallback
   *         The optional callback to call once complete
   */
  cacheAddons: function AddonRepo_cacheAddons(aIds, aCallback) {
    if (!this.cacheEnabled) {
      if (aCallback)
        aCallback();
      return;
    }

    let self = this;
    getAddonsToCache(aIds, function cacheAddons_getAddonsToCache(aAddons) {
      // If there are no add-ons to cache, act as if caching is disabled
      if (aAddons.length == 0) {
        if (aCallback)
          aCallback();
        return;
      }

      self.getAddonsByIDs(aAddons, {
        searchSucceeded: function cacheAddons_searchSucceeded(aAddons) {
          aAddons.forEach(function(aAddon) { self._addons[aAddon.id] = aAddon; });
          AddonDatabase.insertAddons(aAddons, aCallback);
        },
        searchFailed: function cacheAddons_searchFailed() {
          WARN("Search failed when adding add-ons to cache");
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
  getRecommendedURL: function AddonRepo_getRecommendedURL() {
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
  getSearchURL: function AddonRepo_getSearchURL(aSearchTerms) {
    let url = this._formatURLPref(PREF_GETADDONS_BROWSESEARCHRESULTS, {
      TERMS : encodeURIComponent(aSearchTerms)
    });
    return (url != null) ? url : "about:blank";
  },

  /**
   * Cancels the search in progress. If there is no search in progress this
   * does nothing.
   */
  cancelSearch: function AddonRepo_cancelSearch() {
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
  getAddonsByIDs: function AddonRepo_getAddonsByIDs(aIDs, aCallback) {
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
   */
  _beginGetAddons: function AddonRepo_beginGetAddons(aIDs, aCallback, aSendPerformance) {
    let ids = aIDs.slice(0);

    let params = {
      API_VERSION : API_VERSION,
      IDS : ids.map(encodeURIComponent).join(',')
    };

    let pref = PREF_GETADDONS_BYIDS;

    if (aSendPerformance) {
      let type = Services.prefs.getPrefType(PREF_GETADDONS_BYIDS_PERFORMANCE);
      if (type == Services.prefs.PREF_STRING) {
        pref = PREF_GETADDONS_BYIDS_PERFORMANCE;

        let startupInfo = Cc["@mozilla.org/toolkit/app-startup;1"].
                          getService(Ci.nsIAppStartup).
                          getStartupInfo();

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

    let self = this;
    function handleResults(aElements, aTotalResults, aCompatData) {
      // Don't use this._parseAddons() so that, for example,
      // incompatible add-ons are not filtered out
      let results = [];
      for (let i = 0; i < aElements.length && results.length < self._maxResults; i++) {
        let result = self._parseAddon(aElements[i], null, aCompatData);
        if (result == null)
          continue;

        // Ignore add-on if it wasn't actually requested
        let idIndex = ids.indexOf(result.addon.id);
        if (idIndex == -1)
          continue;

        results.push(result);
        // Ignore this add-on from now on
        ids.splice(idIndex, 1);
      }

      // Include any compatibility overrides for addons not hosted by the
      // remote repository.
      for each (let addonCompat in aCompatData) {
        if (addonCompat.hosted)
          continue;

        let addon = new AddonSearchResult(addonCompat.id);
        // Compatibility overrides can only be for extensions.
        addon.type = "extension";
        addon.compatibilityOverrides = addonCompat.compatRanges;
        let result = {
          addon: addon,
          xpiURL: null,
          xpiHash: null
        };
        results.push(result);
      }

      // aTotalResults irrelevant
      self._reportSuccess(results, -1);
    }

    this._beginSearch(url, ids.length, aCallback, handleResults);
  },

  /**
   * Performs the daily background update check.
   *
   * This API both searches for the add-on IDs specified and sends performance
   * data. It is meant to be called as part of the daily update ping. It should
   * not be used for any other purpose. Use repopulateCache instead.
   *
   * @param  aIDs
   *         Array of add-on IDs to repopulate the cache with.
   * @param  aCallback
   *         Function to call when data is received. Function must be an object
   *         with the keys searchSucceeded and searchFailed.
   */
  backgroundUpdateCheck: function AddonRepo_backgroundUpdateCheck(aIDs, aCallback) {
    this._repopulateCacheInternal(aIDs, aCallback, true);
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
  retrieveRecommendedAddons: function AddonRepo_retrieveRecommendedAddons(aMaxResults, aCallback) {
    let url = this._formatURLPref(PREF_GETADDONS_GETRECOMMENDED, {
      API_VERSION : API_VERSION,

      // Get twice as many results to account for potential filtering
      MAX_RESULTS : 2 * aMaxResults
    });

    let self = this;
    function handleResults(aElements, aTotalResults) {
      self._getLocalAddonIds(function retrieveRecommendedAddons_getLocalAddonIds(aLocalAddonIds) {
        // aTotalResults irrelevant
        self._parseAddons(aElements, -1, aLocalAddonIds);
      });
    }

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
  searchAddons: function AddonRepo_searchAddons(aSearchTerms, aMaxResults, aCallback) {
    let compatMode = "normal";
    if (!AddonManager.checkCompatibility)
      compatMode = "ignore";
    else if (AddonManager.strictCompatibility)
      compatMode = "strict";

    let substitutions = {
      API_VERSION : API_VERSION,
      TERMS : encodeURIComponent(aSearchTerms),
      // Get twice as many results to account for potential filtering
      MAX_RESULTS : 2 * aMaxResults,
      COMPATIBILITY_MODE : compatMode,
    };

    let url = this._formatURLPref(PREF_GETADDONS_GETSEARCHRESULTS, substitutions);

    let self = this;
    function handleResults(aElements, aTotalResults) {
      self._getLocalAddonIds(function searchAddons_getLocalAddonIds(aLocalAddonIds) {
        self._parseAddons(aElements, aTotalResults, aLocalAddonIds);
      });
    }

    this._beginSearch(url, aMaxResults, aCallback, handleResults);
  },

  // Posts results to the callback
  _reportSuccess: function AddonRepo_reportSuccess(aResults, aTotalResults) {
    this._searching = false;
    this._request = null;
    // The callback may want to trigger a new search so clear references early
    let addons = [result.addon for each(result in aResults)];
    let callback = this._callback;
    this._callback = null;
    callback.searchSucceeded(addons, addons.length, aTotalResults);
  },

  // Notifies the callback of a failure
  _reportFailure: function AddonRepo_reportFailure() {
    this._searching = false;
    this._request = null;
    // The callback may want to trigger a new search so clear references early
    let callback = this._callback;
    this._callback = null;
    callback.searchFailed();
  },

  // Get descendant by unique tag name. Returns null if not unique tag name.
  _getUniqueDescendant: function AddonRepo_getUniqueDescendant(aElement, aTagName) {
    let elementsList = aElement.getElementsByTagName(aTagName);
    return (elementsList.length == 1) ? elementsList[0] : null;
  },

  // Get direct descendant by unique tag name.
  // Returns null if not unique tag name.
  _getUniqueDirectDescendant: function AddonRepo_getUniqueDirectDescendant(aElement, aTagName) {
    let elementsList = Array.filter(aElement.children,
                                    function arrayFiltering(aChild) aChild.tagName == aTagName);
    return (elementsList.length == 1) ? elementsList[0] : null;
  },

  // Parse out trimmed text content. Returns null if text content empty.
  _getTextContent: function AddonRepo_getTextContent(aElement) {
    let textContent = aElement.textContent.trim();
    return (textContent.length > 0) ? textContent : null;
  },

  // Parse out trimmed text content of a descendant with the specified tag name
  // Returns null if the parsing unsuccessful.
  _getDescendantTextContent: function AddonRepo_getDescendantTextContent(aElement, aTagName) {
    let descendant = this._getUniqueDescendant(aElement, aTagName);
    return (descendant != null) ? this._getTextContent(descendant) : null;
  },

  // Parse out trimmed text content of a direct descendant with the specified
  // tag name.
  // Returns null if the parsing unsuccessful.
  _getDirectDescendantTextContent: function AddonRepo_getDirectDescendantTextContent(aElement, aTagName) {
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
  _parseAddon: function AddonRepo_parseAddon(aElement, aSkip, aCompatData) {
    let skipIDs = (aSkip && aSkip.ids) ? aSkip.ids : [];
    let skipSourceURIs = (aSkip && aSkip.sourceURIs) ? aSkip.sourceURIs : [];

    let guid = this._getDescendantTextContent(aElement, "guid");
    if (guid == null || skipIDs.indexOf(guid) != -1)
      return null;

    let addon = new AddonSearchResult(guid);
    let result = {
      addon: addon,
      xpiURL: null,
      xpiHash: null
    };

    if (aCompatData && guid in aCompatData)
      addon.compatibilityOverrides = aCompatData[guid].compatRanges;

    let self = this;
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
            default:
              WARN("Unknown type id when parsing addon: " + id);
          }
          break;
        case "authors":
          let authorNodes = node.getElementsByTagName("author");
          Array.forEach(authorNodes, function(aAuthorNode) {
            let name = self._getDescendantTextContent(aAuthorNode, "name");
            let link = self._getDescendantTextContent(aAuthorNode, "link");
            if (name == null || link == null)
              return;

            let author = new AddonManagerPrivate.AddonAuthor(name, link);
            if (addon.creator == null)
              addon.creator = author;
            else {
              if (addon.developers == null)
                addon.developers = [];

              addon.developers.push(author);
            }
          });
          break;
        case "previews":
          let previewNodes = node.getElementsByTagName("preview");
          Array.forEach(previewNodes, function(aPreviewNode) {
            let full = self._getUniqueDescendant(aPreviewNode, "full");
            if (full == null)
              return;

            let fullURL = self._getTextContent(full);
            let fullWidth = full.getAttribute("width");
            let fullHeight = full.getAttribute("height");

            let thumbnailURL, thumbnailWidth, thumbnailHeight;
            let thumbnail = self._getUniqueDescendant(aPreviewNode, "thumbnail");
            if (thumbnail) {
              thumbnailURL = self._getTextContent(thumbnail);
              thumbnailWidth = thumbnail.getAttribute("width");
              thumbnailHeight = thumbnail.getAttribute("height");
            }
            let caption = self._getDescendantTextContent(aPreviewNode, "caption");
            let screenshot = new AddonManagerPrivate.AddonScreenshot(fullURL, fullWidth, fullHeight,
                                                                     thumbnailURL, thumbnailWidth,
                                                                     thumbnailHeight, caption);

            if (addon.screenshots == null)
              addon.screenshots = [];

            if (aPreviewNode.getAttribute("primary") == 1)
              addon.screenshots.unshift(screenshot);
            else
              addon.screenshots.push(screenshot);
          });
          break;
        case "learnmore":
          addon.homepageURL = addon.homepageURL || this._getTextContent(node);
          break;
        case "contribution_data":
          let meetDevelopers = this._getDescendantTextContent(node, "meet_developers");
          let suggestedAmount = this._getDescendantTextContent(node, "suggested_amount");
          if (meetDevelopers != null) {
            addon.contributionURL = meetDevelopers;
            addon.contributionAmount = suggestedAmount;
          }
          break
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
          break
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
          addon.isPlatformCompatible = Array.some(nodes, function parseAddon_platformCompatFilter(aNode) {
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

  _parseAddons: function AddonRepo_parseAddons(aElements, aTotalResults, aSkip) {
    let self = this;
    let results = [];

    function isSameApplication(aAppNode) {
      return self._getTextContent(aAppNode) == Services.appinfo.ID;
    }

    for (let i = 0; i < aElements.length && results.length < this._maxResults; i++) {
      let element = aElements[i];

      let tags = this._getUniqueDescendant(element, "compatible_applications");
      if (tags == null)
        continue;

      let applications = tags.getElementsByTagName("appID");
      let compatible = Array.some(applications, function parseAddons_applicationsCompatFilter(aAppNode) {
        if (!isSameApplication(aAppNode))
          return false;

        let parent = aAppNode.parentNode;
        let minVersion = self._getDescendantTextContent(parent, "min_version");
        let maxVersion = self._getDescendantTextContent(parent, "max_version");
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
      if (requiredAttributes.some(function parseAddons_attributeFilter(aAttribute) !result.addon[aAttribute]))
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
    let self = this;
    results.forEach(function(aResult) {
      let addon = aResult.addon;
      let callback = function addonInstallCallback(aInstall) {
        addon.install = aInstall;
        pendingResults--;
        if (pendingResults == 0)
          self._reportSuccess(results, aTotalResults);
      }

      if (aResult.xpiURL) {
        AddonManager.getInstallForURL(aResult.xpiURL, callback,
                                      "application/x-xpinstall", aResult.xpiHash,
                                      addon.name, addon.iconURL, addon.version);
      }
      else {
        callback(null);
      }
    });
  },

  // Parses addon_compatibility nodes, that describe compatibility overrides.
  _parseAddonCompatElement: function AddonRepo_parseAddonCompatElement(aResultObj, aElement) {
    let guid = this._getDescendantTextContent(aElement, "guid");
    if (!guid) {
        LOG("Compatibility override is missing guid.");
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

        let appRange = { appID: appID,
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
        LOG("Compatibility override of unsupported type found.");
        return null;
      }

      let override = new AddonManagerPrivate.AddonCompatibilityOverride(type);

      override.minVersion = this._getDirectDescendantTextContent(aNode, "min_version");
      override.maxVersion = this._getDirectDescendantTextContent(aNode, "max_version");

      if (!override.minVersion) {
        LOG("Compatibility override is missing min_version.");
        return null;
      }
      if (!override.maxVersion) {
        LOG("Compatibility override is missing max_version.");
        return null;
      }

      let appRanges = aNode.querySelectorAll("compatible_applications > application");
      let appRange = findMatchingAppRange.bind(this)(appRanges);
      if (!appRange) {
        LOG("Compatibility override is missing a valid application range.");
        return null;
      }

      override.appID = appRange.appID;
      override.appMinVersion = appRange.appMinVersion;
      override.appMaxVersion = appRange.appMaxVersion;

      return override;
    }

    let rangeNodes = aElement.querySelectorAll("version_ranges > version_range");
    compat.compatRanges = Array.map(rangeNodes, parseRangeNode.bind(this))
                               .filter(function compatRangesFilter(aItem) !!aItem);
    if (compat.compatRanges.length == 0)
      return;

    aResultObj[compat.id] = compat;
  },

  // Parses addon_compatibility elements.
  _parseAddonCompatData: function AddonRepo_parseAddonCompatData(aElements) {
    let compatData = {};
    Array.forEach(aElements, this._parseAddonCompatElement.bind(this, compatData));
    return compatData;
  },

  // Begins a new search if one isn't currently executing
  _beginSearch: function AddonRepo_beginSearch(aURI, aMaxResults, aCallback, aHandleResults) {
    if (this._searching || aURI == null || aMaxResults <= 0) {
      aCallback.searchFailed();
      return;
    }

    this._searching = true;
    this._callback = aCallback;
    this._maxResults = aMaxResults;

    LOG("Requesting " + aURI);

    this._request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                    createInstance(Ci.nsIXMLHttpRequest);
    this._request.mozBackgroundRequest = true;
    this._request.open("GET", aURI, true);
    this._request.overrideMimeType("text/xml");

    let self = this;
    this._request.addEventListener("error", function beginSearch_errorListener(aEvent) {
      self._reportFailure();
    }, false);
    this._request.addEventListener("load", function beginSearch_loadListener(aEvent) {
      let request = aEvent.target;
      let responseXML = request.responseXML;

      if (!responseXML || responseXML.documentElement.namespaceURI == XMLURI_PARSE_ERROR ||
          (request.status != 200 && request.status != 0)) {
        self._reportFailure();
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
      let compatData = self._parseAddonCompatData(compatElements);

      aHandleResults(elements, totalResults, compatData);
    }, false);
    this._request.send(null);
  },

  // Gets the id's of local add-ons, and the sourceURI's of local installs,
  // passing the results to aCallback
  _getLocalAddonIds: function AddonRepo_getLocalAddonIds(aCallback) {
    let self = this;
    let localAddonIds = {ids: null, sourceURIs: null};

    AddonManager.getAllAddons(function getLocalAddonIds_getAllAddons(aAddons) {
      localAddonIds.ids = [a.id for each (a in aAddons)];
      if (localAddonIds.sourceURIs)
        aCallback(localAddonIds);
    });

    AddonManager.getAllInstalls(function getLocalAddonIds_getAllInstalls(aInstalls) {
      localAddonIds.sourceURIs = [];
      aInstalls.forEach(function(aInstall) {
        if (aInstall.state != AddonManager.STATE_AVAILABLE)
          localAddonIds.sourceURIs.push(aInstall.sourceURI.spec);
      });

      if (localAddonIds.ids)
        aCallback(localAddonIds);
    });
  },

  // Create url from preference, returning null if preference does not exist
  _formatURLPref: function AddonRepo_formatURLPref(aPreference, aSubstitutions) {
    let url = null;
    try {
      url = Services.prefs.getCharPref(aPreference);
    } catch(e) {
      WARN("_formatURLPref: Couldn't get pref: " + aPreference);
      return null;
    }

    url = url.replace(/%([A-Z_]+)%/g, function urlSubstitution(aMatch, aKey) {
      return (aKey in aSubstitutions) ? aSubstitutions[aKey] : aMatch;
    });

    return Services.urlFormatter.formatURL(url);
  },

  // Find a AddonCompatibilityOverride that matches a given aAddonVersion and
  // application/platform version.
  findMatchingCompatOverride: function AddonRepo_findMatchingCompatOverride(aAddonVersion,
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
  }

};
AddonRepository.initialize();

var AddonDatabase = {
  // true if the database connection has been opened
  initialized: false,
  // false if there was an unrecoverable error openning the database
  databaseOk: true,
  // A cache of statements that are used and need to be finalized on shutdown
  asyncStatementsCache: {},

  // The queries used by the database
  queries: {
    getAllAddons: "SELECT internal_id, id, type, name, version, " +
                  "creator, creatorURL, description, fullDescription, " +
                  "developerComments, eula, homepageURL, supportURL, " +
                  "contributionURL, contributionAmount, averageRating, " +
                  "reviewCount, reviewURL, totalDownloads, weeklyDownloads, " +
                  "dailyUsers, sourceURI, repositoryStatus, size, updateDate " +
                  "FROM addon",

    getAllDevelopers: "SELECT addon_internal_id, name, url FROM developer " +
                      "ORDER BY addon_internal_id, num",

    getAllScreenshots: "SELECT addon_internal_id, url, width, height, " +
                       "thumbnailURL, thumbnailWidth, thumbnailHeight, caption " +
                       "FROM screenshot ORDER BY addon_internal_id, num",

    getAllCompatOverrides: "SELECT addon_internal_id, type, minVersion, " +
                           "maxVersion, appID, appMinVersion, appMaxVersion " +
                           "FROM compatibility_override " +
                           "ORDER BY addon_internal_id, num",

    getAllIcons: "SELECT addon_internal_id, size, url FROM icon " +
                 "ORDER BY addon_internal_id, size",

    insertAddon: "INSERT INTO addon (id, type, name, version, " +
                 "creator, creatorURL, description, fullDescription, " +
                 "developerComments, eula, homepageURL, supportURL, " +
                 "contributionURL, contributionAmount, averageRating, " +
                 "reviewCount, reviewURL, totalDownloads, weeklyDownloads, " +
                 "dailyUsers, sourceURI, repositoryStatus, size, updateDate) " +
                 "VALUES (:id, :type, :name, :version, :creator, :creatorURL, " +
                 ":description, :fullDescription, :developerComments, :eula, " +
                 ":homepageURL, :supportURL, :contributionURL, " +
                 ":contributionAmount, :averageRating, :reviewCount, " +
                 ":reviewURL, :totalDownloads, :weeklyDownloads, :dailyUsers, " +
                 ":sourceURI, :repositoryStatus, :size, :updateDate)",

    insertDeveloper:  "INSERT INTO developer (addon_internal_id, " +
                      "num, name, url) VALUES (:addon_internal_id, " +
                      ":num, :name, :url)",

    insertScreenshot: "INSERT INTO screenshot (addon_internal_id, " +
                      "num, url, width, height, thumbnailURL, " +
                      "thumbnailWidth, thumbnailHeight, caption) " +
                      "VALUES (:addon_internal_id, " +
                      ":num, :url, :width, :height, :thumbnailURL, " +
                      ":thumbnailWidth, :thumbnailHeight, :caption)",

    insertCompatibilityOverride: "INSERT INTO compatibility_override " +
                                 "(addon_internal_id, num, type, " +
                                 "minVersion, maxVersion, appID, " +
                                 "appMinVersion, appMaxVersion) VALUES " +
                                 "(:addon_internal_id, :num, :type, " +
                                 ":minVersion, :maxVersion, :appID, " +
                                 ":appMinVersion, :appMaxVersion)",

    insertIcon: "INSERT INTO icon (addon_internal_id, size, url) " +
                "VALUES (:addon_internal_id, :size, :url)",

    emptyAddon:       "DELETE FROM addon"
  },

  /**
   * A helper function to log an SQL error.
   *
   * @param  aError
   *         The storage error code associated with the error
   * @param  aErrorString
   *         An error message
   */
  logSQLError: function AD_logSQLError(aError, aErrorString) {
    ERROR("SQL error " + aError + ": " + aErrorString);
  },

  /**
   * A helper function to log any errors that occur during async statements.
   *
   * @param  aError
   *         A mozIStorageError to log
   */
  asyncErrorLogger: function AD_asyncErrorLogger(aError) {
    ERROR("Async SQL error " + aError.result + ": " + aError.message);
  },

  /**
   * Synchronously opens a new connection to the database file.
   *
   * @param  aSecondAttempt
   *         Whether this is a second attempt to open the database
   * @return the mozIStorageConnection for the database
   */
  openConnection: function AD_openConnection(aSecondAttempt) {
    this.initialized = true;
    delete this.connection;

    let dbfile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
    let dbMissing = !dbfile.exists();

    var tryAgain = (function openConnection_tryAgain() {
      LOG("Deleting database, and attempting openConnection again");
      this.initialized = false;
      if (this.connection.connectionReady)
        this.connection.close();
      if (dbfile.exists())
        dbfile.remove(false);
      return this.openConnection(true);
    }).bind(this);

    try {
      this.connection = Services.storage.openUnsharedDatabase(dbfile);
    } catch (e) {
      this.initialized = false;
      ERROR("Failed to open database", e);
      if (aSecondAttempt || dbMissing) {
        this.databaseOk = false;
        throw e;
      }
      return tryAgain();
    }

    this.connection.executeSimpleSQL("PRAGMA locking_mode = EXCLUSIVE");

    // Any errors in here should rollback
    try {
      this.connection.beginTransaction();
      switch (this.connection.schemaVersion) {
        case 0:
          LOG("Recreating database schema");
          this._createSchema();
          break;
        case 1:
          LOG("Upgrading database schema to version 2");
          this.connection.executeSimpleSQL("ALTER TABLE screenshot ADD COLUMN width INTEGER");
          this.connection.executeSimpleSQL("ALTER TABLE screenshot ADD COLUMN height INTEGER");
          this.connection.executeSimpleSQL("ALTER TABLE screenshot ADD COLUMN thumbnailWidth INTEGER");
          this.connection.executeSimpleSQL("ALTER TABLE screenshot ADD COLUMN thumbnailHeight INTEGER");
        case 2:
          LOG("Upgrading database schema to version 3");
          this.connection.createTable("compatibility_override",
                                      "addon_internal_id INTEGER, " +
                                      "num INTEGER, " +
                                      "type TEXT, " +
                                      "minVersion TEXT, " +
                                      "maxVersion TEXT, " +
                                      "appID TEXT, " +
                                      "appMinVersion TEXT, " +
                                      "appMaxVersion TEXT, " +
                                      "PRIMARY KEY (addon_internal_id, num)");
        case 3:
          LOG("Upgrading database schema to version 4");
          this.connection.createTable("icon",
                                      "addon_internal_id INTEGER, " +
                                      "size INTEGER, " +
                                      "url TEXT, " +
                                      "PRIMARY KEY (addon_internal_id, size)");
          this._createIndices();
          this._createTriggers();
          this.connection.schemaVersion = DB_SCHEMA;
        case DB_SCHEMA:
          break;
        default:
          return tryAgain();
      }
      this.connection.commitTransaction();
    } catch (e) {
      ERROR("Failed to create database schema", e);
      this.logSQLError(this.connection.lastError, this.connection.lastErrorString);
      this.connection.rollbackTransaction();
      return tryAgain();
    }

    return this.connection;
  },

  /**
   * A lazy getter for the database connection.
   */
  get connection() {
    return this.openConnection();
  },

  /**
   * Asynchronously shuts down the database connection and releases all
   * cached objects
   *
   * @param  aCallback
   *         An optional callback to call once complete
   */
  shutdown: function AD_shutdown(aCallback) {
    this.databaseOk = true;
    if (!this.initialized) {
      if (aCallback)
        aCallback();
      return;
    }

    this.initialized = false;

    for each (let stmt in this.asyncStatementsCache)
      stmt.finalize();
    this.asyncStatementsCache = {};

    if (this.connection.transactionInProgress) {
      ERROR("Outstanding transaction, rolling back.");
      this.connection.rollbackTransaction();
    }

    let connection = this.connection;
    delete this.connection;

    // Re-create the connection smart getter to allow the database to be
    // re-loaded during testing.
    this.__defineGetter__("connection", function shutdown_connectionGetter() {
      return this.openConnection();
    });

    connection.asyncClose(aCallback);
  },

  /**
   * Asynchronously deletes the database, shutting down the connection
   * first if initialized
   *
   * @param  aCallback
   *         An optional callback to call once complete
   */
  delete: function AD_delete(aCallback) {
    this.shutdown(function delete_shutdown() {
      let dbfile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
      if (dbfile.exists())
        dbfile.remove(false);

      if (aCallback)
        aCallback();
    });
  },

  /**
   * Gets a cached async statement or creates a new statement if it doesn't
   * already exist.
   *
   * @param  aKey
   *         A unique key to reference the statement
   * @return a mozIStorageAsyncStatement for the SQL corresponding to the
   *         unique key
   */
  getAsyncStatement: function AD_getAsyncStatement(aKey) {
    if (aKey in this.asyncStatementsCache)
      return this.asyncStatementsCache[aKey];

    let sql = this.queries[aKey];
    try {
      return this.asyncStatementsCache[aKey] = this.connection.createAsyncStatement(sql);
    } catch (e) {
      ERROR("Error creating statement " + aKey + " (" + sql + ")");
      throw e;
    }
  },

  /**
   * Asynchronously retrieve all add-ons from the database, and pass it
   * to the specified callback
   *
   * @param  aCallback
   *         The callback to pass the add-ons back to
   */
  retrieveStoredData: function AD_retrieveStoredData(aCallback) {
    let self = this;
    let addons = {};

    // Retrieve all data from the addon table
    function getAllAddons() {
      self.getAsyncStatement("getAllAddons").executeAsync({
        handleResult: function getAllAddons_handleResult(aResults) {
          let row = null;
          while (row = aResults.getNextRow()) {
            let internal_id = row.getResultByName("internal_id");
            addons[internal_id] = self._makeAddonFromAsyncRow(row);
          }
        },

        handleError: self.asyncErrorLogger,

        handleCompletion: function getAllAddons_handleCompletion(aReason) {
          if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
            ERROR("Error retrieving add-ons from database. Returning empty results");
            aCallback({});
            return;
          }

          getAllDevelopers();
        }
      });
    }

    // Retrieve all data from the developer table
    function getAllDevelopers() {
      self.getAsyncStatement("getAllDevelopers").executeAsync({
        handleResult: function getAllDevelopers_handleResult(aResults) {
          let row = null;
          while (row = aResults.getNextRow()) {
            let addon_internal_id = row.getResultByName("addon_internal_id");
            if (!(addon_internal_id in addons)) {
              WARN("Found a developer not linked to an add-on in database");
              continue;
            }

            let addon = addons[addon_internal_id];
            if (!addon.developers)
              addon.developers = [];

            addon.developers.push(self._makeDeveloperFromAsyncRow(row));
          }
        },

        handleError: self.asyncErrorLogger,

        handleCompletion: function getAllDevelopers_handleCompletion(aReason) {
          if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
            ERROR("Error retrieving developers from database. Returning empty results");
            aCallback({});
            return;
          }

          getAllScreenshots();
        }
      });
    }

    // Retrieve all data from the screenshot table
    function getAllScreenshots() {
      self.getAsyncStatement("getAllScreenshots").executeAsync({
        handleResult: function getAllScreenshots_handleResult(aResults) {
          let row = null;
          while (row = aResults.getNextRow()) {
            let addon_internal_id = row.getResultByName("addon_internal_id");
            if (!(addon_internal_id in addons)) {
              WARN("Found a screenshot not linked to an add-on in database");
              continue;
            }

            let addon = addons[addon_internal_id];
            if (!addon.screenshots)
              addon.screenshots = [];
            addon.screenshots.push(self._makeScreenshotFromAsyncRow(row));
          }
        },

        handleError: self.asyncErrorLogger,

        handleCompletion: function getAllScreenshots_handleCompletion(aReason) {
          if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
            ERROR("Error retrieving screenshots from database. Returning empty results");
            aCallback({});
            return;
          }

          getAllCompatOverrides();
        }
      });
    }

    function getAllCompatOverrides() {
      self.getAsyncStatement("getAllCompatOverrides").executeAsync({
        handleResult: function getAllCompatOverrides_handleResult(aResults) {
          let row = null;
          while (row = aResults.getNextRow()) {
            let addon_internal_id = row.getResultByName("addon_internal_id");
            if (!(addon_internal_id in addons)) {
              WARN("Found a compatibility override not linked to an add-on in database");
              continue;
            }

            let addon = addons[addon_internal_id];
            if (!addon.compatibilityOverrides)
              addon.compatibilityOverrides = [];
            addon.compatibilityOverrides.push(self._makeCompatOverrideFromAsyncRow(row));
          }
        },

        handleError: self.asyncErrorLogger,

        handleCompletion: function getAllCompatOverrides_handleCompletion(aReason) {
          if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
            ERROR("Error retrieving compatibility overrides from database. Returning empty results");
            aCallback({});
            return;
          }

          getAllIcons();
        }
      });
    }

    function getAllIcons() {
      self.getAsyncStatement("getAllIcons").executeAsync({
        handleResult: function(aResults) {
          let row = null;
          while (row = aResults.getNextRow()) {
            let addon_internal_id = row.getResultByName("addon_internal_id");
            if (!(addon_internal_id in addons)) {
              WARN("Found an icon not linked to an add-on in database");
              continue;
            }

            let addon = addons[addon_internal_id];
            let { size, url } = self._makeIconFromAsyncRow(row);
            addon.icons[size] = url;
            if (size == 32)
              addon.iconURL = url;
          }
        },

        handleError: self.asyncErrorLogger,

        handleCompletion: function(aReason) {
          if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
            ERROR("Error retrieving icons from database. Returning empty results");
            aCallback({});
            return;
          }

          let returnedAddons = {};
          for each (let addon in addons)
            returnedAddons[addon.id] = addon;
          aCallback(returnedAddons);
        }
      });
    }

    // Begin asynchronous process
    getAllAddons();
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
  repopulate: function AD_repopulate(aAddons, aCallback) {
    let self = this;

    // Completely empty the database
    let stmts = [this.getAsyncStatement("emptyAddon")];

    this.connection.executeAsync(stmts, stmts.length, {
      handleResult: function emptyAddon_handleResult() {},
      handleError: self.asyncErrorLogger,

      handleCompletion: function emptyAddon_handleCompletion(aReason) {
        if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED)
          ERROR("Error emptying database. Attempting to continue repopulating database");

        // Insert the specified add-ons
        self.insertAddons(aAddons, aCallback);
      }
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
  insertAddons: function AD_insertAddons(aAddons, aCallback) {
    let self = this;
    let currentAddon = -1;

    // Chain insertions
    function insertNextAddon() {
      if (++currentAddon == aAddons.length) {
        if (aCallback)
          aCallback();
        return;
      }

      self._insertAddon(aAddons[currentAddon], insertNextAddon);
    }

    insertNextAddon();
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
  _insertAddon: function AD__insertAddon(aAddon, aCallback) {
    let self = this;
    let internal_id = null;
    this.connection.beginTransaction();

    // Simultaneously insert the developers, screenshots, and compatibility
    // overrides of the add-on.
    function insertAdditionalData() {
      let stmts = [];

      // Initialize statement and parameters for inserting an array
      function initializeArrayInsert(aStatementKey, aArray, aAddParams) {
        if (!aArray || aArray.length == 0)
          return;

        let stmt = self.getAsyncStatement(aStatementKey);
        let params = stmt.newBindingParamsArray();
        aArray.forEach(function(aElement, aIndex) {
          aAddParams(params, internal_id, aElement, aIndex);
        });

        stmt.bindParameters(params);
        stmts.push(stmt);
      }

      // Initialize statements to insert developers, screenshots, and
      // compatibility overrides
      initializeArrayInsert("insertDeveloper", aAddon.developers,
                            self._addDeveloperParams);
      initializeArrayInsert("insertScreenshot", aAddon.screenshots,
                            self._addScreenshotParams);
      initializeArrayInsert("insertCompatibilityOverride",
                            aAddon.compatibilityOverrides,
                            self._addCompatOverrideParams);
      {
        let stmt = self.getAsyncStatement("insertIcon");
        let params = stmt.newBindingParamsArray();
        let empty = true;
        for (let size in aAddon.icons) {
          self._addIconParams(params, internal_id, aAddon.icons[size], size);
          empty = false;
        }

        if (!empty) {
          stmt.bindParameters(params);
          stmts.push(stmt);
        }
      }

      // Immediately call callback if nothing to insert
      if (stmts.length == 0) {
        self.connection.commitTransaction();
        aCallback();
        return;
      }

      self.connection.executeAsync(stmts, stmts.length, {
        handleResult: function insertAdditionalData_handleResult() {},
        handleError: self.asyncErrorLogger,
        handleCompletion: function insertAdditionalData_handleCompletion(aReason) {
          if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
            ERROR("Error inserting additional addon metadata into database. Attempting to continue");
            self.connection.rollbackTransaction();
          }
          else {
            self.connection.commitTransaction();
          }

          aCallback();
        }
      });
    }

    // Insert add-on into database
    this._makeAddonStatement(aAddon).executeAsync({
      handleResult: function makeAddonStatement_handleResult() {},
      handleError: self.asyncErrorLogger,

      handleCompletion: function makeAddonStatement_handleCompletion(aReason) {
        if (aReason != Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          ERROR("Error inserting add-ons into database. Attempting to continue.");
          self.connection.rollbackTransaction();
          aCallback();
          return;
        }

        internal_id = self.connection.lastInsertRowID;
        insertAdditionalData();
      }
    });
  },

  /**
   * Make an asynchronous statement that will insert the specified add-on
   *
   * @param  aAddon
   *         The add-on to make the statement for
   * @return The asynchronous mozIStorageStatement
   */
  _makeAddonStatement: function AD__makeAddonStatement(aAddon) {
    let stmt = this.getAsyncStatement("insertAddon");
    let params = stmt.params;

    PROP_SINGLE.forEach(function(aProperty) {
      switch (aProperty) {
        case "sourceURI":
          params.sourceURI = aAddon.sourceURI ? aAddon.sourceURI.spec : null;
          break;
        case "creator":
          params.creator =  aAddon.creator ? aAddon.creator.name : null;
          params.creatorURL =  aAddon.creator ? aAddon.creator.url : null;
          break;
        case "updateDate":
          params.updateDate = aAddon.updateDate ? aAddon.updateDate.getTime() : null;
          break;
        default:
          params[aProperty] = aAddon[aProperty];
      }
    });

    return stmt;
  },

  /**
   * Add developer parameters to the specified mozIStorageBindingParamsArray
   *
   * @param  aParams
   *         The mozIStorageBindingParamsArray to add the parameters to
   * @param  aInternalID
   *         The internal_id of the add-on that this developer is for
   * @param  aDeveloper
   *         The developer to make the parameters from
   * @param  aIndex
   *         The index of this developer
   * @return The asynchronous mozIStorageStatement
   */
  _addDeveloperParams: function AD__addDeveloperParams(aParams, aInternalID,
                                                       aDeveloper, aIndex) {
    let bp = aParams.newBindingParams();
    bp.bindByName("addon_internal_id", aInternalID);
    bp.bindByName("num", aIndex);
    bp.bindByName("name", aDeveloper.name);
    bp.bindByName("url", aDeveloper.url);
    aParams.addParams(bp);
  },

  /**
   * Add screenshot parameters to the specified mozIStorageBindingParamsArray
   *
   * @param  aParams
   *         The mozIStorageBindingParamsArray to add the parameters to
   * @param  aInternalID
   *         The internal_id of the add-on that this screenshot is for
   * @param  aScreenshot
   *         The screenshot to make the parameters from
   * @param  aIndex
   *         The index of this screenshot
   */
  _addScreenshotParams: function AD__addScreenshotParams(aParams, aInternalID,
                                                         aScreenshot, aIndex) {
    let bp = aParams.newBindingParams();
    bp.bindByName("addon_internal_id", aInternalID);
    bp.bindByName("num", aIndex);
    bp.bindByName("url", aScreenshot.url);
    bp.bindByName("width", aScreenshot.width);
    bp.bindByName("height", aScreenshot.height);
    bp.bindByName("thumbnailURL", aScreenshot.thumbnailURL);
    bp.bindByName("thumbnailWidth", aScreenshot.thumbnailWidth);
    bp.bindByName("thumbnailHeight", aScreenshot.thumbnailHeight);
    bp.bindByName("caption", aScreenshot.caption);
    aParams.addParams(bp);
  },

  /**
   * Add compatibility override parameters to the specified
   * mozIStorageBindingParamsArray.
   *
   * @param  aParams
   *         The mozIStorageBindingParamsArray to add the parameters to
   * @param  aInternalID
   *         The internal_id of the add-on that this override is for
   * @param  aOverride
   *         The override to make the parameters from
   * @param  aIndex
   *         The index of this override
   */
  _addCompatOverrideParams: function AD_addCompatOverrideParams(aParams,
                                                                aInternalID,
                                                                aOverride,
                                                                aIndex) {
    let bp = aParams.newBindingParams();
    bp.bindByName("addon_internal_id", aInternalID);
    bp.bindByName("num", aIndex);
    bp.bindByName("type", aOverride.type);
    bp.bindByName("minVersion", aOverride.minVersion);
    bp.bindByName("maxVersion", aOverride.maxVersion);
    bp.bindByName("appID", aOverride.appID);
    bp.bindByName("appMinVersion", aOverride.appMinVersion);
    bp.bindByName("appMaxVersion", aOverride.appMaxVersion);
    aParams.addParams(bp);
  },

  /**
   * Add icon parameters to the specified mozIStorageBindingParamsArray.
   *
   * @param  aParams
   *         The mozIStorageBindingParamsArray to add the parameters to
   * @param  aInternalID
   *         The internal_id of the add-on that this override is for
   * @param  aURL
   *         The URL of this icon
   * @param  aSize
   *         The size of this icon
   */
  _addIconParams: function AD_addIconParams(aParams,
                                            aInternalID,
                                            aURL,
                                            aSize) {
    let bp = aParams.newBindingParams();
    bp.bindByName("addon_internal_id", aInternalID);
    bp.bindByName("url", aURL);
    bp.bindByName("size", aSize);
    aParams.addParams(bp);
  },

  /**
   * Make add-on from an asynchronous row
   * Note: This add-on will be lacking both developers and screenshots
   *
   * @param  aRow
   *         The asynchronous row to use
   * @return The created add-on
   */
  _makeAddonFromAsyncRow: function AD__makeAddonFromAsyncRow(aRow) {
    let addon = {};
    addon.icons = {};

    PROP_SINGLE.forEach(function(aProperty) {
      let value = aRow.getResultByName(aProperty);

      switch (aProperty) {
        case "sourceURI":
          addon.sourceURI = value ? NetUtil.newURI(value) : null;
          break;
        case "creator":
          let creatorURL = aRow.getResultByName("creatorURL");
          if (value || creatorURL)
            addon.creator = new AddonManagerPrivate.AddonAuthor(value, creatorURL);
          else
            addon.creator = null;
          break;
        case "updateDate":
          addon.updateDate = value ? new Date(value) : null;
          break;
        default:
          addon[aProperty] = value;
      }
    });

    return addon;
  },

  /**
   * Make a developer from an asynchronous row
   *
   * @param  aRow
   *         The asynchronous row to use
   * @return The created developer
   */
  _makeDeveloperFromAsyncRow: function AD__makeDeveloperFromAsyncRow(aRow) {
    let name = aRow.getResultByName("name");
    let url = aRow.getResultByName("url")
    return new AddonManagerPrivate.AddonAuthor(name, url);
  },

  /**
   * Make a screenshot from an asynchronous row
   *
   * @param  aRow
   *         The asynchronous row to use
   * @return The created screenshot
   */
  _makeScreenshotFromAsyncRow: function AD__makeScreenshotFromAsyncRow(aRow) {
    let url = aRow.getResultByName("url");
    let width = aRow.getResultByName("width");
    let height = aRow.getResultByName("height");
    let thumbnailURL = aRow.getResultByName("thumbnailURL");
    let thumbnailWidth = aRow.getResultByName("thumbnailWidth");
    let thumbnailHeight = aRow.getResultByName("thumbnailHeight");
    let caption = aRow.getResultByName("caption");
    return new AddonManagerPrivate.AddonScreenshot(url, width, height, thumbnailURL,
                                                   thumbnailWidth, thumbnailHeight, caption);
  },

  /**
   * Make a CompatibilityOverride from an asynchronous row
   *
   * @param  aRow
   *         The asynchronous row to use
   * @return The created CompatibilityOverride
   */
  _makeCompatOverrideFromAsyncRow: function AD_makeCompatOverrideFromAsyncRow(aRow) {
    let type = aRow.getResultByName("type");
    let minVersion = aRow.getResultByName("minVersion");
    let maxVersion = aRow.getResultByName("maxVersion");
    let appID = aRow.getResultByName("appID");
    let appMinVersion = aRow.getResultByName("appMinVersion");
    let appMaxVersion = aRow.getResultByName("appMaxVersion");
    return new AddonManagerPrivate.AddonCompatibilityOverride(type,
                                                              minVersion,
                                                              maxVersion,
                                                              appID,
                                                              appMinVersion,
                                                              appMaxVersion);
  },

  /**
   * Make an icon from an asynchronous row
   *
   * @param  aRow
   *         The asynchronous row to use
   * @return An object containing the size and URL of the icon
   */
  _makeIconFromAsyncRow: function AD_makeIconFromAsyncRow(aRow) {
    let size = aRow.getResultByName("size");
    let url = aRow.getResultByName("url");
    return { size: size, url: url };
  },

  /**
   * Synchronously creates the schema in the database.
   */
  _createSchema: function AD__createSchema() {
    LOG("Creating database schema");

      this.connection.createTable("addon",
                                  "internal_id INTEGER PRIMARY KEY AUTOINCREMENT, " +
                                  "id TEXT UNIQUE, " +
                                  "type TEXT, " +
                                  "name TEXT, " +
                                  "version TEXT, " +
                                  "creator TEXT, " +
                                  "creatorURL TEXT, " +
                                  "description TEXT, " +
                                  "fullDescription TEXT, " +
                                  "developerComments TEXT, " +
                                  "eula TEXT, " +
                                  "homepageURL TEXT, " +
                                  "supportURL TEXT, " +
                                  "contributionURL TEXT, " +
                                  "contributionAmount TEXT, " +
                                  "averageRating INTEGER, " +
                                  "reviewCount INTEGER, " +
                                  "reviewURL TEXT, " +
                                  "totalDownloads INTEGER, " +
                                  "weeklyDownloads INTEGER, " +
                                  "dailyUsers INTEGER, " +
                                  "sourceURI TEXT, " +
                                  "repositoryStatus INTEGER, " +
                                  "size INTEGER, " +
                                  "updateDate INTEGER");

      this.connection.createTable("developer",
                                  "addon_internal_id INTEGER, " +
                                  "num INTEGER, " +
                                  "name TEXT, " +
                                  "url TEXT, " +
                                  "PRIMARY KEY (addon_internal_id, num)");

      this.connection.createTable("screenshot",
                                  "addon_internal_id INTEGER, " +
                                  "num INTEGER, " +
                                  "url TEXT, " +
                                  "width INTEGER, " +
                                  "height INTEGER, " +
                                  "thumbnailURL TEXT, " +
                                  "thumbnailWidth INTEGER, " +
                                  "thumbnailHeight INTEGER, " +
                                  "caption TEXT, " +
                                  "PRIMARY KEY (addon_internal_id, num)");

      this.connection.createTable("compatibility_override",
                                  "addon_internal_id INTEGER, " +
                                  "num INTEGER, " +
                                  "type TEXT, " +
                                  "minVersion TEXT, " +
                                  "maxVersion TEXT, " +
                                  "appID TEXT, " +
                                  "appMinVersion TEXT, " +
                                  "appMaxVersion TEXT, " +
                                  "PRIMARY KEY (addon_internal_id, num)");

      this.connection.createTable("icon",
                                  "addon_internal_id INTEGER, " +
                                  "size INTEGER, " +
                                  "url TEXT, " +
                                  "PRIMARY KEY (addon_internal_id, size)");

      this._createIndices();
      this._createTriggers();

      this.connection.schemaVersion = DB_SCHEMA;
  },

  /**
   * Synchronously creates the triggers in the database.
   */
  _createTriggers: function AD__createTriggers() {
    this.connection.executeSimpleSQL("DROP TRIGGER IF EXISTS delete_addon");
    this.connection.executeSimpleSQL("CREATE TRIGGER delete_addon AFTER DELETE " +
      "ON addon BEGIN " +
      "DELETE FROM developer WHERE addon_internal_id=old.internal_id; " +
      "DELETE FROM screenshot WHERE addon_internal_id=old.internal_id; " +
      "DELETE FROM compatibility_override WHERE addon_internal_id=old.internal_id; " +
      "DELETE FROM icon WHERE addon_internal_id=old.internal_id; " +
      "END");
  },

  /**
   * Synchronously creates the indices in the database.
   */
  _createIndices: function AD__createIndices() {
      this.connection.executeSimpleSQL("CREATE INDEX IF NOT EXISTS developer_idx " +
                                       "ON developer (addon_internal_id)");
      this.connection.executeSimpleSQL("CREATE INDEX IF NOT EXISTS screenshot_idx " +
                                       "ON screenshot (addon_internal_id)");
      this.connection.executeSimpleSQL("CREATE INDEX IF NOT EXISTS compatibility_override_idx " +
                                       "ON compatibility_override (addon_internal_id)");
      this.connection.executeSimpleSQL("CREATE INDEX IF NOT EXISTS icon_idx " +
                                       "ON icon (addon_internal_id)");
  }
};
