/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["SearchCache"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
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
    prefix: "SearchCache",
    maxLogLevel: SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

// A text encoder to UTF8, used whenever we commit the cache to disk.
XPCOMUtils.defineLazyGetter(this, "gEncoder", function() {
  return new TextEncoder();
});

const CACHE_FILENAME = "search.json.mozlz4";

/**
 * This class manages the saves search cache that stores the search engine
 * settings.
 *
 * Cache attributes can be saved and obtained from this class via the
 * `*Attribute` methods.
 */
class SearchCache {
  constructor(searchService) {
    this._searchService = searchService;
  }

  QueryInterface = ChromeUtils.generateQI([Ci.nsIObserver]);

  // Delay for batching invalidation of the JSON cache (ms)
  static CACHE_INVALIDATION_DELAY = 1000;

  /**
   * A reference to the pending DeferredTask, if there is one.
   */
  _batchTask = null;

  /**
   * The current metadata stored in the cache. This stores:
   *   - current
   *       The current user-set default engine. The associated hash is called
   *       'hash'.
   *   - private
   *       The current user-set private engine. The associated hash is called
   *       'privateHash'.
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
  _metaData = {};

  /**
   * A reference to the search service so that we can save the engines list.
   */
  _searchService;

  addObservers() {
    Services.obs.addObserver(this, SearchUtils.TOPIC_ENGINE_MODIFIED);
    Services.obs.addObserver(this, SearchUtils.TOPIC_SEARCH_SERVICE);
  }

  /**
   * Cleans up, removing observers.
   */
  removeObservers() {
    Services.obs.removeObserver(this, SearchUtils.TOPIC_ENGINE_MODIFIED);
    Services.obs.removeObserver(this, SearchUtils.TOPIC_SEARCH_SERVICE);
  }

  /**
   * Reads the cache file.
   *
   * @param {string} origin
   *   If this parameter is "test", then the cache will not be written. As
   *   some tests manipulate the cache directly, we allow turning off writing to
   *   avoid writing stale cache data.
   * @returns {object}
   *   Returns the cache file data.
   */
  async get(origin = "") {
    let json;
    await this._ensurePendingWritesCompleted(origin);
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
        !gModernConfig &&
        json.appVersion != Services.appinfo.version &&
        gGeoSpecificDefaultsEnabled &&
        json.metaData
      ) {
        json.metaData.searchDefaultExpir = 0;
      }
    } catch (ex) {
      logConsole.error("_readCacheFile: Error reading cache file:", ex);
      json = {};
    }
    if (json.metaData) {
      this._metaData = json.metaData;
    }

    return json;
  }

  /**
   * Queues writing the cache until after CACHE_INVALIDATION_DELAY. If there
   * is a currently queued task then it will be restarted.
   */
  _delayedWrite() {
    if (this._batchTask) {
      this._batchTask.disarm();
    } else {
      let task = async () => {
        logConsole.debug("batchTask: Invalidating engine cache");
        await this._write();
      };
      this._batchTask = new DeferredTask(
        task,
        SearchCache.CACHE_INVALIDATION_DELAY
      );
    }
    this._batchTask.arm();
  }

  /**
   * Ensures any pending writes of the cache are completed.
   *
   * @param {string} origin
   *   If this parameter is "test", then the cache will not be written. As
   *   some tests manipulate the cache directly, we allow turning off writing to
   *   avoid writing stale cache data.
   */
  async _ensurePendingWritesCompleted(origin = "") {
    // Before we read the cache file, first make sure all pending tasks are clear.
    if (!this._batchTask) {
      return;
    }
    logConsole.debug("finalizing batch task");
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

  /**
   * Writes the cache to disk (no delay).
   */
  async _write() {
    if (this._batchTask) {
      this._batchTask.disarm();
    }

    let cache = {};
    let locale = Services.locale.requestedLocale;
    let buildID = Services.appinfo.platformBuildID;
    let appVersion = Services.appinfo.version;

    // Allows us to force a cache refresh should the cache format change.
    cache.version = SearchUtils.CACHE_VERSION;
    // We don't want to incur the costs of stat()ing each plugin on every
    // startup when the only (supported) time they will change is during
    // app updates (where the buildID is obviously going to change).
    // Extension-shipped plugins are the only exception to this, but their
    // directories are blown away during updates, so we'll detect their changes.
    cache.buildID = buildID;
    // Store the appVersion as well so we can do extra things during major updates.
    cache.appVersion = appVersion;
    cache.locale = locale;

    if (gModernConfig) {
      cache.builtInEngineList = this._searchService._searchOrder;
    } else {
      cache.visibleDefaultEngines = this._searchService._visibleDefaultEngines;
    }
    cache.engines = [...this._searchService._engines.values()];
    cache.metaData = this._metaData;

    try {
      if (!cache.engines.length) {
        throw new Error("cannot write without any engine.");
      }

      logConsole.debug("_buildCache: Writing to cache file.");
      let path = OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME);
      let data = gEncoder.encode(JSON.stringify(cache));
      await OS.File.writeAtomic(path, data, {
        compression: "lz4",
        tmpPath: path + ".tmp",
      });
      logConsole.debug("_buildCache: cache file written to disk.");
      Services.obs.notifyObservers(
        null,
        SearchUtils.TOPIC_SEARCH_SERVICE,
        "write-cache-to-disk-complete"
      );
    } catch (ex) {
      logConsole.error("_buildCache: Could not write to cache file:", ex);
    }
  }

  /**
   * Sets an attribute.
   *
   * @param {string} name
   *   The name of the attribute to set.
   * @param {*} val
   *   The value to set.
   */
  setAttribute(name, val) {
    this._metaData[name] = val;
    this._delayedWrite();
  }

  /**
   * Sets a verified attribute. This will save an additional hash
   * value, that can be verified when reading back.
   *
   * @param {string} name
   *   The name of the attribute to set.
   * @param {*} val
   *   The value to set.
   */
  setVerifiedAttribute(name, val) {
    this._metaData[name] = val;
    this._metaData[this.getHashName(name)] = SearchUtils.getVerificationHash(
      val
    );
    this._delayedWrite();
  }

  /**
   * Gets an attribute.
   *
   * @param {string} name
   *   The name of the attribute to get.
   * @returns {*}
   *   The value of the attribute, or undefined if not known.
   */
  getAttribute(name) {
    return this._metaData[name] || undefined;
  }

  /**
   * Gets a verified attribute.
   *
   * @param {string} name
   *   The name of the attribute to get.
   * @returns {*}
   *   The value of the attribute, or undefined if not known or an empty strings
   *   if it does not match the verification hash.
   */
  getVerifiedAttribute(name) {
    let val = this.getAttribute(name);
    if (
      val &&
      this.getAttribute(this.getHashName(name)) !=
        SearchUtils.getVerificationHash(val)
    ) {
      logConsole.warn("getVerifiedGlobalAttr, invalid hash for", name);
      return "";
    }
    return val;
  }

  /**
   * Returns the name for the hash for a particular attribute. This is
   * necessary because the normal default engine is named `current` with
   * its hash as `hash`. All other hashes are in the `<name>Hash` format.
   *
   * @param {string} name
   *   The name of the attribute to get the hash name for.
   * @returns {string}
   *   The hash name to use.
   */
  getHashName(name) {
    if (name == "current") {
      return "hash";
    }
    return name + "Hash";
  }

  /**
   * Handles shutdown; writing the cache if necessary.
   *
   * @param {object} state
   *   The shutdownState object that is used to help analyzing the shutdown
   *   state in case of a crash or shutdown timeout.
   */
  async shutdown(state) {
    if (!this._batchTask) {
      return;
    }
    state.step = "Finalizing batched task";
    try {
      await this._batchTask.finalize();
      state.step = "Batched task finalized";
    } catch (ex) {
      state.step = "Batched task failed to finalize";

      state.latestError.message = "" + ex;
      if (ex && typeof ex == "object") {
        state.latestError.stack = ex.stack || undefined;
      }
    }
  }

  // nsIObserver
  observe(engine, topic, verb) {
    switch (topic) {
      case SearchUtils.TOPIC_ENGINE_MODIFIED:
        switch (verb) {
          case SearchUtils.MODIFIED_TYPE.ADDED:
          case SearchUtils.MODIFIED_TYPE.CHANGED:
          case SearchUtils.MODIFIED_TYPE.REMOVED:
            this._delayedWrite();
            break;
        }
        break;
      case SearchUtils.TOPIC_SEARCH_SERVICE:
        switch (verb) {
          case "init-complete":
          case "reinit-complete":
          case "engines-reloaded":
            this._delayedWrite();
            break;
        }
        break;
    }
  }
}
