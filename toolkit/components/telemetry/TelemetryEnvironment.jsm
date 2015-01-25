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

const LOGGER_NAME = "Toolkit.Telemetry";

this.TelemetryEnvironment = {
  _shutdown: true,

  // A map of (sync) listeners that will be called on environment changes.
  _changeListeners: new Map(),
  // Async task for collecting the environment data.
  _collectTask: null,
  _doNotify: false,

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
    return {};
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
