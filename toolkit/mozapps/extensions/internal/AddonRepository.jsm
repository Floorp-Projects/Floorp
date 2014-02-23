/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DeferredSave",
                                  "resource://gre/modules/DeferredSave.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository_SQLiteMigrator",
                                  "resource://gre/modules/addons/AddonRepository_SQLiteMigrator.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");

this.EXPORTED_SYMBOLS = [ "AddonRepository" ];

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
const PREF_GETADDONS_DB_SCHEMA           = "extensions.getAddons.databaseSchema"

const XMLURI_PARSE_ERROR  = "http://www.mozilla.org/newlayout/xml/parsererror.xml";

const API_VERSION = "1.5";
const DEFAULT_CACHE_TYPES = "extension,theme,locale,dictionary";

const KEY_PROFILEDIR        = "ProfD";
const FILE_DATABASE         = "addons.json";
const DB_SCHEMA             = 5;
const DB_MIN_JSON_SCHEMA    = 5;
const DB_BATCH_TIMEOUT_MS   = 50;
const DB_DATA_WRITTEN_TOPIC = "addon-repository-data-written"

const BLANK_DB = function() {
  return {
    addons: new Map(),
    schema: DB_SCHEMA
  };
}

const TOOLKIT_ID     = "toolkit@mozilla.org";

["LOG", "WARN", "ERROR"].forEach(function(aName) {
  this.__defineGetter__(aName, function logFuncGetter() {
    Components.utils.import("resource://gre/modules/addons/AddonLogging.jsm");

    LogManager.getLogger("addons.repository", this);
    return this[aName];
  });
}, this);

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

// Wrap the XHR factory so that tests can override with a mock
let XHRequest = Components.Constructor("@mozilla.org/xmlextras/xmlhttprequest;1",
                                       "nsIXMLHttpRequest");

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
  },

  toJSON: function() {
    let json = {};

    for (let [property, value] of Iterator(this)) {
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
        WARN("Error writing property value for " + property);
      }
    }

    for (let [property, value] of Iterator(this._unsupportedProperties)) {
      if (!property.startsWith("_"))
        json[property] = value;
    }

    return json;
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
this.AddonRepository = {
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

  // Whether a migration in currently in progress
  _migrationInProgress: false,

  // A callback to be called when migration finishes
  _postMigrationCallback: null,

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
  shutdown: function AddonRepo_shutdown() {
    this.cancelSearch();

    this._addons = null;
    this._pendingCallbacks = null;
    return AddonDatabase.shutdown(false);
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
   * @param  aTimeout
   *         (Optional) timeout in milliseconds to abandon the XHR request
   *         if we have not received a response from the server.
   */
  repopulateCache: function(aIds, aCallback, aTimeout) {
    this._repopulateCacheInternal(aIds, aCallback, false, aTimeout);
  },

  _repopulateCacheInternal: function(aIds, aCallback, aSendPerformance, aTimeout) {
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
      }, aSendPerformance, aTimeout);
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
   * @param  aTimeout
   *         (Optional) timeout in milliseconds to abandon the XHR request
   *         if we have not received a response from the server.
   */
  _beginGetAddons: function(aIDs, aCallback, aSendPerformance, aTimeout) {
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

    this._beginSearch(url, ids.length, aCallback, handleResults, aTimeout);
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
          for (let authorNode of authorNodes) {
            let name = self._getDescendantTextContent(authorNode, "name");
            let link = self._getDescendantTextContent(authorNode, "link");
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
            let full = self._getUniqueDescendant(previewNode, "full");
            if (full == null)
              continue;

            let fullURL = self._getTextContent(full);
            let fullWidth = full.getAttribute("width");
            let fullHeight = full.getAttribute("height");

            let thumbnailURL, thumbnailWidth, thumbnailHeight;
            let thumbnail = self._getUniqueDescendant(previewNode, "thumbnail");
            if (thumbnail) {
              thumbnailURL = self._getTextContent(thumbnail);
              thumbnailWidth = thumbnail.getAttribute("width");
              thumbnailHeight = thumbnail.getAttribute("height");
            }
            let caption = self._getDescendantTextContent(previewNode, "caption");
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
                                      addon.name, addon.icons, addon.version);
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
  _beginSearch: function(aURI, aMaxResults, aCallback, aHandleResults, aTimeout) {
    if (this._searching || aURI == null || aMaxResults <= 0) {
      aCallback.searchFailed();
      return;
    }

    this._searching = true;
    this._callback = aCallback;
    this._maxResults = aMaxResults;

    LOG("Requesting " + aURI);

    this._request = new XHRequest();
    this._request.mozBackgroundRequest = true;
    this._request.open("GET", aURI, true);
    this._request.overrideMimeType("text/xml");
    if (aTimeout) {
      this._request.timeout = aTimeout;
    }

    this._request.addEventListener("error", aEvent => this._reportFailure(), false);
    this._request.addEventListener("timeout", aEvent => this._reportFailure(), false);
    this._request.addEventListener("load", aEvent => {
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

var AddonDatabase = {
  // true if the database connection has been opened
  initialized: false,
  // false if there was an unrecoverable error openning the database
  databaseOk: true,

  // the in-memory database
  DB: BLANK_DB(),

  /**
   * A getter to retrieve an nsIFile pointer to the DB
   */
  get jsonFile() {
    delete this.jsonFile;
    return this.jsonFile = FileUtils.getFile(KEY_PROFILEDIR, [FILE_DATABASE], true);
  },

  /**
   * Synchronously opens a new connection to the database file.
   */
  openConnection: function() {
    this.DB = BLANK_DB();
    this.initialized = true;
    delete this.connection;

    let inputDB, fstream, cstream, schema;

    try {
     let data = "";
     fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                 .createInstance(Ci.nsIFileInputStream);
     cstream = Cc["@mozilla.org/intl/converter-input-stream;1"]
                 .createInstance(Ci.nsIConverterInputStream);

     fstream.init(this.jsonFile, -1, 0, 0);
     cstream.init(fstream, "UTF-8", 0, 0);
     let (str = {}) {
       let read = 0;
       do {
         read = cstream.readString(0xffffffff, str); // read as much as we can and put it in str.value
         data += str.value;
       } while (read != 0);
     }

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

    } catch (e if e.result == Cr.NS_ERROR_FILE_NOT_FOUND) {
      LOG("No " + FILE_DATABASE + " found.");

      // Create a blank addons.json file
      this._saveDBToDisk();

      let dbSchema = 0;
      try {
        dbSchema = Services.prefs.getIntPref(PREF_GETADDONS_DB_SCHEMA);
      } catch (e) {}

      if (dbSchema < DB_MIN_JSON_SCHEMA) {
        this._migrationInProgress = AddonRepository_SQLiteMigrator.migrate((results) => {
          if (results.length)
            this.insertAddons(results);

          if (this._postMigrationCallback) {
            this._postMigrationCallback();
            this._postMigrationCallback = null;
          }

          this._migrationInProgress = false;
        });

        Services.prefs.setIntPref(PREF_GETADDONS_DB_SCHEMA, DB_SCHEMA);
      }

      return;

    } catch (e) {
      ERROR("Malformed " + FILE_DATABASE + ": " + e);
      this.databaseOk = false;
      return;

    } finally {
     cstream.close();
     fstream.close();
    }

    Services.prefs.setIntPref(PREF_GETADDONS_DB_SCHEMA, DB_SCHEMA);

    // We use _insertAddon manually instead of calling
    // insertAddons to avoid the write to disk which would
    // be a waste since this is the data that was just read.
    for (let addon of inputDB.addons) {
      this._insertAddon(addon);
    }
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
   * @param  aSkipFlush
   *         An optional boolean to skip flushing data to disk. Useful
   *         when the database is going to be deleted afterwards.
   */
  shutdown: function AD_shutdown(aSkipFlush) {
    this.databaseOk = true;

    if (!this.initialized) {
      return Promise.resolve(0);
    }

    this.initialized = false;

    this.__defineGetter__("connection", function shutdown_connectionGetter() {
      return this.openConnection();
    });

    if (aSkipFlush) {
      return Promise.resolve(0);
    } else {
      return this.Writer.flush();
    }
  },

  /**
   * Asynchronously deletes the database, shutting down the connection
   * first if initialized
   *
   * @param  aCallback
   *         An optional callback to call once complete
   */
  delete: function AD_delete(aCallback) {
    this.DB = BLANK_DB();

    this.Writer.flush()
      .then(null, () => {})
      // shutdown(true) never rejects
      .then(() => this.shutdown(true))
      .then(() => OS.File.remove(this.jsonFile.path, {}))
      .then(null, error => ERROR("Unable to delete Addon Repository file " +
                                 this.jsonFile.path, error))
      .then(aCallback);
  },

  toJSON: function AD_toJSON() {
    let json = {
      schema: this.DB.schema,
      addons: []
    }

    for (let [, value] of this.DB.addons)
      json.addons.push(value);

    return json;
  },

  /*
   * This is a deferred task writer that is used
   * to batch operations done within 50ms of each
   * other and thus generating only one write to disk
   */
  get Writer() {
    delete this.Writer;
    this.Writer = new DeferredSave(
      this.jsonFile.path,
      () => { return JSON.stringify(this); },
      DB_BATCH_TIMEOUT_MS
    );
    return this.Writer;
  },

  /**
   * Asynchronously retrieve all add-ons from the database, and pass it
   * to the specified callback
   *
   * @param  aCallback
   *         The callback to pass the add-ons back to
   */
  retrieveStoredData: function AD_retrieveStoredData(aCallback) {
    if (!this.initialized)
      this.openConnection();

    let gatherResults = () => {
      let result = {};
      for (let [key, value] of this.DB.addons)
        result[key] = value;

      executeSoon(function() aCallback(result));
    };

    if (this._migrationInProgress)
      this._postMigrationCallback = gatherResults;
    else
      gatherResults();
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
    this.DB.addons.clear();
    this.insertAddons(aAddons, aCallback);
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
    if (!this.initialized)
      this.openConnection();

    for (let addon of aAddons) {
      this._insertAddon(addon);
    }

    this._saveDBToDisk();

    if (aCallback)
      executeSoon(aCallback);
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
  _insertAddon: function AD__insertAddon(aAddon) {
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
  _parseAddon: function (aObj) {
    if (aObj instanceof AddonSearchResult)
      return aObj;

    let id = aObj.id;
    if (!aObj.id)
      return null;

    let addon = new AddonSearchResult(id);

    for (let [expectedProperty,] of Iterator(AddonSearchResult.prototype)) {
      if (!(expectedProperty in aObj) ||
          typeof(aObj[expectedProperty]) === "function")
        continue;

      let value = aObj[expectedProperty];

      try {
        switch (expectedProperty) {
          case "sourceURI":
            addon.sourceURI = value ? NetUtil.newURI(value) :  null;
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
            for (let [size, url] of Iterator(aObj.icons)) {
              addon.icons[size] = url;
            }
            break;

          case "iconURL":
            break;

          default:
            addon[expectedProperty] = value;
        }
      } catch (ex) {
        WARN("Error in parsing property value for " + expectedProperty + " | " + ex);
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
   * Write the in-memory DB to disk, after waiting for
   * the DB_BATCH_TIMEOUT_MS timeout.
   *
   * @return Promise A promise that resolves after the
   *                 write to disk has completed.
   */
  _saveDBToDisk: function() {
    return this.Writer.saveChanges().then(
      function() Services.obs.notifyObservers(null, DB_DATA_WRITTEN_TOPIC, null),
      ERROR);
  },

  /**
   * Make a developer object from a vanilla
   * JS object from the JSON database
   *
   * @param  aObj
   *         The JS object to use
   * @return The created developer
   */
  _makeDeveloper: function (aObj) {
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
  _makeScreenshot: function (aObj) {
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
  _makeCompatOverride: function (aObj) {
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

function executeSoon(aCallback) {
  Services.tm.mainThread.dispatch(aCallback, Ci.nsIThread.DISPATCH_NORMAL);
}
