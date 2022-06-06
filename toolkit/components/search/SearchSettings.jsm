/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["SearchSettings"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logConsole", () => {
  return console.createInstance({
    prefix: "SearchSettings",
    maxLogLevel: SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

const SETTINGS_FILENAME = "search.json.mozlz4";

/**
 * This class manages the saves search settings.
 *
 * Global settings can be saved and obtained from this class via the
 * `*Attribute` methods.
 */
class SearchSettings {
  constructor(searchService) {
    this._searchService = searchService;
  }

  QueryInterface = ChromeUtils.generateQI([Ci.nsIObserver]);

  // Delay for batching invalidation of the JSON settings (ms)
  static SETTINGS_INVALIDATION_DELAY = 1000;

  /**
   * A reference to the pending DeferredTask, if there is one.
   */
  _batchTask = null;

  /**
   * The current metadata stored in the settings. This stores:
   *   - current
   *       The current user-set default engine. The associated hash is called
   *       'hash'.
   *   - private
   *       The current user-set private engine. The associated hash is called
   *       'privateHash'.
   *
   * All of the above have associated hash fields to validate the value is set
   * by the application.
   */
  _metaData = {};

  /**
   * A reference to the search service so that we can save the engines list.
   */
  _searchService = null;

  /*
   * A copy of the settings so we can persist metadata for engines that
   * are not currently active.
   */
  _currentSettings = null;

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
   * Reads the settings file.
   *
   * @param {string} origin
   *   If this parameter is "test", then the settings will not be written. As
   *   some tests manipulate the settings directly, we allow turning off writing to
   *   avoid writing stale settings data.
   * @returns {object}
   *   Returns the settings file data.
   */
  async get(origin = "") {
    let json;
    await this._ensurePendingWritesCompleted(origin);
    try {
      let settingsFilePath = PathUtils.join(
        PathUtils.profileDir,
        SETTINGS_FILENAME
      );
      json = await IOUtils.readJSON(settingsFilePath, { decompress: true });
      if (!json.engines || !json.engines.length) {
        throw new Error("no engine in the file");
      }
    } catch (ex) {
      logConsole.warn("get: No settings file exists, new profile?", ex);
      json = {};
    }
    if (json.metaData) {
      this._metaData = json.metaData;
    }
    // Versions of gecko older than 82 stored the order flag as a preference.
    // This was changed in version 6 of the settings file.
    if (json.version < 6 || !("useSavedOrder" in this._metaData)) {
      const prefName = SearchUtils.BROWSER_SEARCH_PREF + "useDBForOrder";
      let useSavedOrder = Services.prefs.getBoolPref(prefName, false);

      this.setAttribute("useSavedOrder", useSavedOrder);

      // Clear the old pref so it isn't lying around.
      Services.prefs.clearUserPref(prefName);
    }

    this._currentSettings = json;
    return json;
  }

  /**
   * Queues writing the settings until after SETTINGS_INVALIDATION_DELAY. If there
   * is a currently queued task then it will be restarted.
   */
  _delayedWrite() {
    if (this._batchTask) {
      this._batchTask.disarm();
    } else {
      let task = async () => {
        if (
          !this._searchService.isInitialized ||
          this._searchService._reloadingEngines
        ) {
          // Re-arm the task as we don't want to save potentially incomplete
          // information during the middle of (re-)initializing.
          this._batchTask.arm();
          return;
        }
        logConsole.debug("batchTask: Invalidating engine settings");
        await this._write();
      };
      this._batchTask = new DeferredTask(
        task,
        SearchSettings.SETTINGS_INVALIDATION_DELAY
      );
    }
    this._batchTask.arm();
  }

  /**
   * Ensures any pending writes of the settings are completed.
   *
   * @param {string} origin
   *   If this parameter is "test", then the settings will not be written. As
   *   some tests manipulate the settings directly, we allow turning off writing to
   *   avoid writing stale settings data.
   */
  async _ensurePendingWritesCompleted(origin = "") {
    // Before we read the settings file, first make sure all pending tasks are clear.
    if (!this._batchTask) {
      return;
    }
    logConsole.debug("finalizing batch task");
    let task = this._batchTask;
    this._batchTask = null;
    // Tests manipulate the settings directly, so let's not double-write with
    // stale settings data here.
    if (origin == "test") {
      task.disarm();
    } else {
      await task.finalize();
    }
  }

  /**
   * Writes the settings to disk (no delay).
   */
  async _write() {
    if (this._batchTask) {
      this._batchTask.disarm();
    }

    let settings = {};

    // Allows us to force a settings refresh should the settings format change.
    settings.version = SearchUtils.SETTINGS_VERSION;
    settings.engines = [...this._searchService._engines.values()];
    settings.metaData = this._metaData;

    // Persist metadata for AppProvided engines even if they aren't currently
    // active, this means if they become active again their settings
    // will be restored.
    if (this._currentSettings?.engines) {
      for (let engine of this._currentSettings.engines) {
        let included = settings.engines.some(e => e._name == engine._name);
        if (engine._isAppProvided && !included) {
          settings.engines.push(engine);
        }
      }
    }

    // Update the local copy.
    this._currentSettings = settings;

    try {
      if (!settings.engines.length) {
        throw new Error("cannot write without any engine.");
      }

      logConsole.debug("_write: Writing to settings file.");
      let path = PathUtils.join(PathUtils.profileDir, SETTINGS_FILENAME);
      await IOUtils.writeJSON(path, settings, {
        compress: true,
        tmpPath: path + ".tmp",
      });
      logConsole.debug("_write: settings file written to disk.");
      Services.obs.notifyObservers(
        null,
        SearchUtils.TOPIC_SEARCH_SERVICE,
        "write-settings-to-disk-complete"
      );
    } catch (ex) {
      logConsole.error("_write: Could not write to settings file:", ex);
    }
  }

  /**
   * Sets an attribute without verification.
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
   * Gets an attribute without verification.
   *
   * @param {string} name
   *   The name of the attribute to get.
   * @returns {*}
   *   The value of the attribute, or undefined if not known.
   */
  getAttribute(name) {
    return this._metaData[name] ?? undefined;
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
      return undefined;
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
   * Handles shutdown; writing the settings if necessary.
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
          case "engines-reloaded":
            this._delayedWrite();
            break;
        }
        break;
    }
  }
}
