/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AddonManagerPrivate: "resource://gre/modules/AddonManager.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  ServiceRequest: "resource://gre/modules/ServiceRequest.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
});

// The current platform as specified in the AMO API:
// http://addons-server.readthedocs.io/en/latest/topics/api/addons.html#addon-detail-platform
XPCOMUtils.defineLazyGetter(lazy, "PLATFORM", () => {
  let platform = Services.appinfo.OS;
  switch (platform) {
    case "Darwin":
      return "mac";

    case "Linux":
      return "linux";

    case "Android":
      return "android";

    case "WINNT":
      return "windows";
  }
  return platform;
});

var EXPORTED_SYMBOLS = ["AddonRepository"];

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";
const PREF_GETADDONS_CACHE_TYPES = "extensions.getAddons.cache.types";
const PREF_GETADDONS_CACHE_ID_ENABLED =
  "extensions.%ID%.getAddons.cache.enabled";
const PREF_GETADDONS_BROWSEADDONS = "extensions.getAddons.browseAddons";
const PREF_GETADDONS_BYIDS = "extensions.getAddons.get.url";
const PREF_GETADDONS_BROWSESEARCHRESULTS =
  "extensions.getAddons.search.browseURL";
const PREF_GETADDONS_DB_SCHEMA = "extensions.getAddons.databaseSchema";
const PREF_GET_LANGPACKS = "extensions.getAddons.langpacks.url";

const PREF_METADATA_LASTUPDATE = "extensions.getAddons.cache.lastUpdate";
const PREF_METADATA_UPDATETHRESHOLD_SEC =
  "extensions.getAddons.cache.updateThreshold";
const DEFAULT_METADATA_UPDATETHRESHOLD_SEC = 172800; // two days

const DEFAULT_CACHE_TYPES = "extension,theme,locale,dictionary";

const FILE_DATABASE = "addons.json";
const DB_SCHEMA = 6;
const DB_MIN_JSON_SCHEMA = 5;
const DB_BATCH_TIMEOUT_MS = 50;

const BLANK_DB = function() {
  return {
    addons: new Map(),
    schema: DB_SCHEMA,
  };
};

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.repository";

// Create a new logger for use by the Addons Repository
// (Requires AddonManager.jsm)
var logger = Log.repository.getLogger(LOGGER_ID);

function convertHTMLToPlainText(html) {
  if (!html) {
    return html;
  }
  var converter = Cc[
    "@mozilla.org/widget/htmlformatconverter;1"
  ].createInstance(Ci.nsIFormatConverter);

  var input = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );
  input.data = html.replace(/\n/g, "<br>");

  var output = {};
  converter.convert("text/html", input, "text/unicode", output);

  if (output.value instanceof Ci.nsISupportsString) {
    return output.value.data.replace(/\r\n/g, "\n");
  }
  return html;
}

async function getAddonsToCache(aIds) {
  let types =
    lazy.Preferences.get(PREF_GETADDONS_CACHE_TYPES) || DEFAULT_CACHE_TYPES;

  types = types.split(",");

  let addons = await lazy.AddonManager.getAddonsByIDs(aIds);
  let enabledIds = [];

  for (let [i, addon] of addons.entries()) {
    var preference = PREF_GETADDONS_CACHE_ID_ENABLED.replace("%ID%", aIds[i]);
    // If the preference doesn't exist caching is enabled by default
    if (!lazy.Preferences.get(preference, true)) {
      continue;
    }

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
   * The support URL for the add-on
   */
  supportURL: null,

  /**
   * The contribution url of the add-on
   */
  contributionURL: null,

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
   * The number of times the add-on was downloaded the current week
   */
  weeklyDownloads: null,

  /**
   * AddonInstall object generated from the add-on XPI url
   */
  install: null,

  /**
   * nsIURI storing where this add-on was installed from
   */
  sourceURI: null,

  /**
   * The Date that the add-on was most recently updated
   */
  updateDate: null,

  toJSON() {
    let json = {};

    for (let property of Object.keys(this)) {
      let value = this[property];
      if (property.startsWith("_") || typeof value === "function") {
        continue;
      }

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
      if (!property.startsWith("_")) {
        json[property] = value;
      }
    }

    return json;
  },
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
var AddonRepository = {
  /**
   * The homepage for visiting this repository. If the corresponding preference
   * is not defined, defaults to about:blank.
   */
  get homepageURL() {
    let url = this._formatURLPref(PREF_GETADDONS_BROWSEADDONS, {});
    return url != null ? url : "about:blank";
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
      TERMS: aSearchTerms,
    });
    return url != null ? url : "about:blank";
  },

  /**
   * Whether caching is currently enabled
   */
  get cacheEnabled() {
    return Services.prefs.getBoolPref(PREF_GETADDONS_CACHE_ENABLED, false);
  },

  /**
   * Shut down AddonRepository
   * return: promise{integer} resolves with the result of flushing
   *         the AddonRepository database
   */
  shutdown() {
    return AddonDatabase.shutdown(false);
  },

  metadataAge() {
    let now = Math.round(Date.now() / 1000);
    let lastUpdate = Services.prefs.getIntPref(PREF_METADATA_LASTUPDATE, 0);
    return Math.max(0, now - lastUpdate);
  },

  isMetadataStale() {
    let threshold = Services.prefs.getIntPref(
      PREF_METADATA_UPDATETHRESHOLD_SEC,
      DEFAULT_METADATA_UPDATETHRESHOLD_SEC
    );
    return this.metadataAge() > threshold;
  },

  /**
   * Asynchronously get a cached add-on by id. The add-on (or null if the
   * add-on is not found) is passed to the specified callback. If caching is
   * disabled, null is passed to the specified callback.
   *
   * The callback variant exists only for existing code in XPIProvider.jsm
   * and XPIDatabase.jsm that requires a synchronous callback, yuck.
   *
   * @param  aId
   *         The id of the add-on to get
   */
  async getCachedAddonByID(aId, aCallback) {
    if (!aId || !this.cacheEnabled) {
      if (aCallback) {
        aCallback(null);
      }
      return null;
    }

    if (aCallback && AddonDatabase._loaded) {
      let addon = AddonDatabase.getAddon(aId);
      aCallback(addon);
      return addon;
    }

    await AddonDatabase.openConnection();

    let addon = AddonDatabase.getAddon(aId);
    if (aCallback) {
      aCallback(addon);
    }
    return addon;
  },

  /*
   * Clear and delete the AddonRepository database
   * @return Promise{null} resolves when the database is deleted
   */
  _clearCache() {
    return AddonDatabase.delete().then(() =>
      lazy.AddonManagerPrivate.updateAddonRepositoryData()
    );
  },

  /**
   * Fetch data from an API where the results may span multiple "pages".
   * This function will take care of issuing multiple requests until all
   * the results have been fetched, and will coalesce them all into a
   * single return value.  The handling here is specific to the way AMO
   * implements paging (ie a JSON result with a "next" property).
   *
   * @param {string} startURL
   *                 URL for the first page of results
   * @param {function} handler
   *                   This function will be called once per page of results,
   *                   it should return an array of objects (the type depends
   *                   on the particular API being called of course).
   *
   * @returns Promise{array} An array of all the individual results from
   *                         the API call(s).
   */
  _fetchPaged(ids, pref, handler) {
    let startURL = this._formatURLPref(pref, { IDS: ids.join(",") });
    let results = [];
    let idCheck = ids.map(id => {
      if (id.startsWith("rta:")) {
        return atob(id.split(":")[1]);
      }
      return id;
    });

    const fetchNextPage = url => {
      return new Promise((resolve, reject) => {
        let request = new lazy.ServiceRequest({ mozAnon: true });
        request.mozBackgroundRequest = true;
        request.open("GET", url, true);
        request.responseType = "json";

        request.addEventListener("error", aEvent => {
          reject(new Error(`GET ${url} failed`));
        });
        request.addEventListener("timeout", aEvent => {
          reject(new Error(`GET ${url} timed out`));
        });
        request.addEventListener("load", aEvent => {
          let response = request.response;
          if (!response || (request.status != 200 && request.status != 0)) {
            reject(new Error(`GET ${url} failed (status ${request.status})`));
            return;
          }

          try {
            let newResults = handler(response.results).filter(e =>
              idCheck.includes(e.id)
            );
            results.push(...newResults);
          } catch (err) {
            reject(err);
          }

          if (response.next) {
            resolve(fetchNextPage(response.next));
          }

          resolve(results);
        });

        request.send(null);
      });
    };

    return fetchNextPage(startURL);
  },

  /**
   * Fetch metadata for a given set of addons from AMO.
   *
   * @param  aIDs
   *         The array of ids to retrieve metadata for.
   * @returns {array<AddonSearchResult>}
   */
  async getAddonsByIDs(aIDs) {
    return this._fetchPaged(aIDs, PREF_GETADDONS_BYIDS, results =>
      results.map(entry => this._parseAddon(entry))
    );
  },

  /**
   * Fetch addon metadata for a set of addons.
   *
   * @param {array<string>} aIDs
   *                        A list of addon IDs to fetch information about.
   *
   * @returns {array<AddonSearchResult>}
   */
  async _getFullData(aIDs) {
    let addons = [];
    try {
      addons = await this.getAddonsByIDs(aIDs, false);
    } catch (err) {
      logger.error(`Error in addon metadata check: ${err.message}`);
    }

    return addons;
  },

  /**
   * Asynchronously add add-ons to the cache corresponding to the specified
   * ids. If caching is disabled, the cache is unchanged.
   *
   * @param  aIds
   *         The array of add-on ids to add to the cache
   */
  async cacheAddons(aIds) {
    logger.debug(
      "cacheAddons: enabled " + this.cacheEnabled + " IDs " + aIds.toSource()
    );
    if (!this.cacheEnabled) {
      return [];
    }

    let ids = await getAddonsToCache(aIds);

    // If there are no add-ons to cache, act as if caching is disabled
    if (!ids.length) {
      return [];
    }

    let addons = await this._getFullData(ids);
    await AddonDatabase.update(addons);

    return Array.from(addons.values());
  },

  /**
   * Performs the daily background update check.
   *
   * @return Promise{null} Resolves when the metadata update is complete.
   */
  async backgroundUpdateCheck() {
    let shutter = (async () => {
      let allAddons = await lazy.AddonManager.getAllAddons();

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
      if (!addonsToCache.length) {
        logger.debug("Clearing cache because 0 add-ons were requested");
        await this._clearCache();
        return;
      }

      let addons = await this._getFullData(addonsToCache);

      AddonDatabase.repopulate(addons);

      // Always call AddonManager updateAddonRepositoryData after we refill the cache
      await lazy.AddonManagerPrivate.updateAddonRepositoryData();
    })();
    lazy.AddonManager.beforeShutdown.addBlocker(
      "AddonRepository Background Updater",
      shutter
    );
    await shutter;
    lazy.AddonManager.beforeShutdown.removeBlocker(shutter);
  },

  /*
   * Creates an AddonSearchResult by parsing an entry from the AMO API.
   *
   * @param  aEntry
   *         An entry from the AMO search API to parse.
   * @return Result object containing the parsed AddonSearchResult
   */
  _parseAddon(aEntry) {
    let addon = new AddonSearchResult(aEntry.guid);

    addon.name = aEntry.name;
    if (typeof aEntry.current_version == "object") {
      addon.version = String(aEntry.current_version.version);
      if (Array.isArray(aEntry.current_version.files)) {
        for (let file of aEntry.current_version.files) {
          if (file.platform == "all" || file.platform == lazy.PLATFORM) {
            if (file.url) {
              addon.sourceURI = lazy.NetUtil.newURI(file.url);
            }
            break;
          }
        }
      }
    }
    addon.homepageURL = aEntry.homepage;
    addon.supportURL = aEntry.support_url;

    addon.description = convertHTMLToPlainText(aEntry.summary);
    addon.fullDescription = convertHTMLToPlainText(aEntry.description);

    addon.weeklyDownloads = aEntry.weekly_downloads;

    switch (aEntry.type) {
      case "persona":
      case "statictheme":
        addon.type = "theme";
        break;

      case "language":
        addon.type = "locale";
        break;

      default:
        addon.type = aEntry.type;
        break;
    }

    if (Array.isArray(aEntry.authors)) {
      let authors = aEntry.authors.map(
        author =>
          new lazy.AddonManagerPrivate.AddonAuthor(author.name, author.url)
      );
      if (authors.length) {
        addon.creator = authors[0];
        addon.developers = authors.slice(1);
      }
    }

    if (typeof aEntry.previews == "object") {
      addon.screenshots = aEntry.previews.map(shot => {
        let safeSize = orig =>
          Array.isArray(orig) && orig.length >= 2 ? orig : [null, null];
        let imageSize = safeSize(shot.image_size);
        let thumbSize = safeSize(shot.thumbnail_size);
        return new lazy.AddonManagerPrivate.AddonScreenshot(
          shot.image_url,
          imageSize[0],
          imageSize[1],
          shot.thumbnail_url,
          thumbSize[0],
          thumbSize[1],
          shot.caption
        );
      });
    }

    addon.contributionURL = aEntry.contributions_url;

    if (typeof aEntry.ratings == "object") {
      addon.averageRating = Math.min(5, aEntry.ratings.average);
      addon.reviewCount = aEntry.ratings.text_count;
    }

    addon.reviewURL = aEntry.ratings_url;
    if (aEntry.last_updated) {
      addon.updateDate = new Date(aEntry.last_updated);
    }

    addon.icons = aEntry.icons || {};

    return addon;
  },

  // Create url from preference, returning null if preference does not exist
  _formatURLPref(aPreference, aSubstitutions = {}) {
    let url = Services.prefs.getCharPref(aPreference, "");
    if (!url) {
      logger.warn("_formatURLPref: Couldn't get pref: " + aPreference);
      return null;
    }

    url = url.replace(/%([A-Z_]+)%/g, function(aMatch, aKey) {
      return aKey in aSubstitutions
        ? encodeURIComponent(aSubstitutions[aKey])
        : aMatch;
    });

    return Services.urlFormatter.formatURL(url);
  },

  flush() {
    return AddonDatabase.flush();
  },

  async getAvailableLangpacks() {
    // This should be the API endpoint documented at:
    // http://addons-server.readthedocs.io/en/latest/topics/api/addons.html#language-tools
    let url = this._formatURLPref(PREF_GET_LANGPACKS);

    let response = await fetch(url, { credentials: "omit" });
    if (!response.ok) {
      throw new Error("fetching available language packs failed");
    }

    let data = await response.json();

    let result = [];
    for (let entry of data.results) {
      if (
        !entry.current_compatible_version ||
        !entry.current_compatible_version.files
      ) {
        continue;
      }

      for (let file of entry.current_compatible_version.files) {
        if (
          file.platform == "all" ||
          file.platform == Services.appinfo.OS.toLowerCase()
        ) {
          result.push({
            target_locale: entry.target_locale,
            url: file.url,
            hash: file.hash,
          });
        }
      }
    }

    return result;
  },
};

var AddonDatabase = {
  connectionPromise: null,
  _loaded: false,
  _saveTask: null,
  _blockerAdded: false,

  // the in-memory database
  DB: BLANK_DB(),

  /**
   * A getter to retrieve the path to the DB
   */
  get jsonFile() {
    return PathUtils.join(
      Services.dirsvc.get("ProfD", Ci.nsIFile).path,
      FILE_DATABASE
    );
  },

  /**
   * Asynchronously opens a new connection to the database file.
   *
   * @return {Promise} a promise that resolves to the database.
   */
  openConnection() {
    if (!this.connectionPromise) {
      this.connectionPromise = (async () => {
        let inputDB, schema;

        try {
          let data = await IOUtils.readUTF8(this.jsonFile);
          inputDB = JSON.parse(data);

          if (
            !inputDB.hasOwnProperty("addons") ||
            !Array.isArray(inputDB.addons)
          ) {
            throw new Error("No addons array.");
          }

          if (!inputDB.hasOwnProperty("schema")) {
            throw new Error("No schema specified.");
          }

          schema = parseInt(inputDB.schema, 10);

          if (!Number.isInteger(schema) || schema < DB_MIN_JSON_SCHEMA) {
            throw new Error("Invalid schema value.");
          }
        } catch (e) {
          if (e.name == "NotFoundError") {
            logger.debug("No " + FILE_DATABASE + " found.");
          } else {
            logger.error(
              `Malformed ${FILE_DATABASE}: ${e} - resetting to empty`
            );
          }

          // Create a blank addons.json file
          this.save();

          Services.prefs.setIntPref(PREF_GETADDONS_DB_SCHEMA, DB_SCHEMA);
          this._loaded = true;
          return this.DB;
        }

        Services.prefs.setIntPref(PREF_GETADDONS_DB_SCHEMA, DB_SCHEMA);

        // Convert the addon objects as necessary
        // and store them in our in-memory copy of the database.
        for (let addon of inputDB.addons) {
          let id = addon.id;

          let entry = this._parseAddon(addon);
          this.DB.addons.set(id, entry);
        }

        this._loaded = true;
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
    this._loaded = false;

    if (aSkipFlush) {
      return Promise.resolve();
    }

    return this.flush();
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
      .then(() => IOUtils.remove(this.jsonFile))
      .catch(error =>
        logger.error(
          "Unable to delete Addon Repository file " + this.jsonFile,
          error
        )
      )
      .then(() => (this._deleting = null))
      .then(aCallback);

    return this._deleting;
  },

  async _saveNow() {
    let json = {
      schema: this.DB.schema,
      addons: Array.from(this.DB.addons.values()),
    };

    await IOUtils.writeUTF8(this.jsonFile, JSON.stringify(json), {
      tmpPath: `${this.jsonFile}.tmp`,
    });
  },

  save() {
    if (!this._saveTask) {
      this._saveTask = new lazy.DeferredTask(
        () => this._saveNow(),
        DB_BATCH_TIMEOUT_MS
      );

      if (!this._blockerAdded) {
        lazy.AsyncShutdown.profileBeforeChange.addBlocker(
          "Flush AddonRepository",
          () => this.flush()
        );
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
   * Get an individual addon entry from the in-memory cache.
   * Note: calling this function before the database is read will
   * return undefined.
   *
   * @param {string} aId The id of the addon to retrieve.
   */
  getAddon(aId) {
    return this.DB.addons.get(aId);
  },

  /**
   * Asynchronously repopulates the database so it only contains the
   * specified add-ons
   *
   * @param {Map} aAddons
   *              Add-ons to repopulate the database with.
   */
  repopulate(aAddons) {
    this.DB = BLANK_DB();
    this._update(aAddons);

    let now = Math.round(Date.now() / 1000);
    logger.debug(
      "Cache repopulated, setting " + PREF_METADATA_LASTUPDATE + " to " + now
    );
    Services.prefs.setIntPref(PREF_METADATA_LASTUPDATE, now);
  },

  /**
   * Asynchronously insert new addons into the database.
   *
   * @param {Map} aAddons
   *              Add-ons to insert/update in the database
   */
  async update(aAddons) {
    await this.openConnection();

    this._update(aAddons);

    this.save();
  },

  /**
   * Merge the given addons into the database.
   *
   * @param {Map} aAddons
   *              Add-ons to insert/update in the database
   */
  _update(aAddons) {
    for (let addon of aAddons) {
      this.DB.addons.set(addon.id, this._parseAddon(addon));
    }

    this.save();
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
    if (aObj instanceof AddonSearchResult) {
      return aObj;
    }

    let id = aObj.id;
    if (!aObj.id) {
      return null;
    }

    let addon = new AddonSearchResult(id);

    for (let expectedProperty of Object.keys(AddonSearchResult.prototype)) {
      if (
        !(expectedProperty in aObj) ||
        typeof aObj[expectedProperty] === "function"
      ) {
        continue;
      }

      let value = aObj[expectedProperty];

      try {
        switch (expectedProperty) {
          case "sourceURI":
            addon.sourceURI = value ? lazy.NetUtil.newURI(value) : null;
            break;

          case "creator":
            addon.creator = value ? this._makeDeveloper(value) : null;
            break;

          case "updateDate":
            addon.updateDate = value ? new Date(value) : null;
            break;

          case "developers":
            if (!addon.developers) {
              addon.developers = [];
            }
            for (let developer of value) {
              addon.developers.push(this._makeDeveloper(developer));
            }
            break;

          case "screenshots":
            if (!addon.screenshots) {
              addon.screenshots = [];
            }
            for (let screenshot of value) {
              addon.screenshots.push(this._makeScreenshot(screenshot));
            }
            break;

          case "icons":
            if (!addon.icons) {
              addon.icons = {};
            }
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
        logger.warn(
          "Error in parsing property value for " + expectedProperty + " | " + ex
        );
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
      switch (typeof aObj[remainingProperty]) {
        case "boolean":
        case "number":
        case "string":
        case "object":
          // these types are accepted
          break;
        default:
          continue;
      }

      if (!remainingProperty.startsWith("_")) {
        addon._unsupportedProperties[remainingProperty] =
          aObj[remainingProperty];
      }
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
    return new lazy.AddonManagerPrivate.AddonAuthor(name, url);
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
    return new lazy.AddonManagerPrivate.AddonScreenshot(
      url,
      width,
      height,
      thumbnailURL,
      thumbnailWidth,
      thumbnailHeight,
      caption
    );
  },
};
