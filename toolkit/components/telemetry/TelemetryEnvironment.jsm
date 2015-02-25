/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "TelemetryEnvironment",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");

const LOGGER_NAME = "Toolkit.Telemetry";

this.TelemetryEnvironment = {
  _shutdown: true,

  // A map of (sync) listeners that will be called on environment changes.
  _changeListeners: new Map(),
  // Async task for collecting the environment data.
  _collectTask: null,
  _doNotify: false,

  // Policy to use when saving preferences. Exported for using them in tests.
  RECORD_PREF_STATE: 1, // Don't record the preference value
  RECORD_PREF_VALUE: 2, // We only record user-set prefs.

  // A map of watched preferences which trigger an Environment change when modified.
  // Every entry contains a recording policy (RECORD_PREF_STATE or RECORD_PREF_VALUE).
  _watchedPrefs: null,

  /**
   * Initialize TelemetryEnvironment.
   */
  init: function () {
    if (!this._shutdown) {
      this._log.error("init - Already initialized");
      return;
    }

    this._log = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, "TelemetryEnvironment::");
    this._log.trace("init");
    this._shutdown = false;
    this._startWatchingPrefs();
  },

  /**
   * Shutdown TelemetryEnvironment.
   * @return Promise<> that is resolved when shutdown is finished.
   */
  shutdown: Task.async(function* () {
    if (this._shutdown) {
      this._log.error("shutdown - Already shut down");
      throw new Error("Already shut down");
    }

    this._log.trace("shutdown");
    this._shutdown = true;
    this._stopWatchingPrefs();
    this._changeListeners.clear();
    yield this._collectTask;
  }),

  /**
   * Register a listener for environment changes.
   * @param name The name of the listener - good for debugging purposes.
   * @param listener A JS callback function.
   */
  registerChangeListener: function (name, listener) {
    this._log.trace("registerChangeListener for " + name);
    if (this._shutdown) {
      this._log.warn("registerChangeListener - already shutdown")
      return;
    }
    this._changeListeners.set(name, listener);
  },

  /**
   * Unregister from listening to environment changes.
   * @param name The name of the listener to remove.
   */
  unregisterChangeListener: function (name) {
    this._log.trace("unregisterChangeListener for " + name);
    if (this._shutdown) {
      this._log.warn("registerChangeListener - already shutdown")
      return;
    }
    this._changeListeners.delete(name);
  },

  /**
   * Only used in tests, set the preferences to watch.
   * @param aPreferences A map of preferences names and their recording policy.
   */
  _watchPreferences: function (aPreferences) {
    if (this._watchedPrefs) {
      this._stopWatchingPrefs();
    }

    this._watchedPrefs = aPreferences;
    this._startWatchingPrefs();
  },

  /**
   * Get an object containing the values for the watched preferences. Depending on the
   * policy, the value for a preference or whether it was changed by user is reported.
   *
   * @return An object containing the preferences values.
   */
  _getPrefData: function () {
    if (!this._watchedPrefs) {
      return {};
    }

    let prefData = {};
    for (let pref in this._watchedPrefs) {
      // We only want to record preferences if they are non-default.
      if (!Preferences.isSet(pref)) {
        continue;
      }

      // Check the policy for the preference and decide if we need to store its value
      // or whether it changed from the default value.
      let prefValue = undefined;
      if (this._watchedPrefs[pref] == this.RECORD_PREF_STATE) {
        prefValue = null;
      } else {
        prefValue = Preferences.get(pref, null);
      }
      prefData[pref] = prefValue;
    }
    return prefData;
  },

  /**
   * Start watching the preferences.
   */
  _startWatchingPrefs: function () {
    this._log.trace("_startWatchingPrefs - " + this._watchedPrefs);

    if (!this._watchedPrefs) {
      return;
    }

    for (let pref in this._watchedPrefs) {
      Preferences.observe(pref, this._onPrefChanged, this);
    }
  },

  /**
   * Do not receive any more change notifications for the preferences.
   */
  _stopWatchingPrefs: function () {
    this._log.trace("_stopWatchingPrefs");

    if (!this._watchedPrefs) {
      return;
    }

    for (let pref in this._watchedPrefs) {
      Preferences.ignore(pref, this._onPrefChanged, this);
    }

    this._watchedPrefs = null;
  },

  _onPrefChanged: function () {
    this._log.trace("_onPrefChanged");
    this._onEnvironmentChange("pref-changed");
  },

  /**
   * Get the settings data in object form.
   * @return Object containing the settings.
   */
  _getSettings: function () {
    return {
      "userPrefs": this._getPrefData(),
    };
  },

  /**
   * Get the environment data in object form.
   * @return Promise<Object> Resolved with the data on success, otherwise rejected.
   */
  getEnvironmentData: function() {
    if (this._shutdown) {
      this._log.error("getEnvironmentData - Already shut down");
      return Promise.reject("Already shutdown");
    }

    this._log.trace("getEnvironmentData");
    if (this._collectTask) {
      return this._collectTask;
    }

    this._collectTask = this._doGetEnvironmentData();
    let clear = () => this._collectTask = null;
    this._collectTask.then(clear, clear);
    return this._collectTask;
  },

  _doGetEnvironmentData: Task.async(function* () {
    this._log.trace("getEnvironmentData");

    // Define the data collection function for each section.
    let sections = {
      "settings": () => this._getSettings(),
    };

    let data = {};
    // Recover from exceptions in the collection functions and populate the data
    // object. We want to recover so that, in the worst-case, we only lose some data
    // sections instead of all.
    for (let s in sections) {
      try {
        data[s] = sections[s]();
      } catch (e) {
        this._log.error("getEnvironmentData - There was an exception collecting " + s, e);
      }
    }

    return data;
  }),

  _onEnvironmentChange: function (what) {
    this._log.trace("_onEnvironmentChange for " + what);
    if (this._shutdown) {
      this._log.trace("_onEnvironmentChange - Already shut down.");
      return;
    }

    for (let [name, listener] of this._changeListeners) {
      try {
        this._log.debug("_onEnvironmentChange - calling " + name);
        listener();
      } catch (e) {
        this._log.warning("_onEnvironmentChange - listener " + name + " caught error", e);
      }
    }
  },
};
