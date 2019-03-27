/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module sends Origin Telemetry periodically:
 * https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/data/prio-ping.html
 */

"use strict";

var EXPORTED_SYMBOLS = [
  "TelemetryPrioPing",
];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetters(this, {
  TelemetryController: "resource://gre/modules/TelemetryController.jsm",
  Log: "resource://gre/modules/Log.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  Telemetry: ["@mozilla.org/base/telemetry;1", "nsITelemetry"],
});

const {setTimeout, clearTimeout} = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const {TelemetryUtils} = ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

const Utils = TelemetryUtils;

const MILLISECONDS_PER_HOUR = 60 * 60 * 1000;

const DEFAULT_PING_FREQUENCY_HOURS = 24;

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryPrioPing";

// Triggered from native Origin Telemetry storage.
const PRIO_LIMIT_REACHED_TOPIC = "origin-telemetry-storage-limit-reached";

const PRIO_PING_VERSION = "1";

var Policy = {
  setTimeout: (callback, delayMs) => setTimeout(callback, delayMs),
  clearTimeout: (id) => clearTimeout(id),
  sendPing: (type, payload, options) => TelemetryController.submitExternalPing(type, payload, options),
  getEncodedOriginSnapshot: async (aClear) => Telemetry.getEncodedOriginSnapshot(aClear),
};

var TelemetryPrioPing = {
  Reason: Object.freeze({
    PERIODIC: "periodic", // Sent the ping containing Origin Telemetry from the past periodic interval (default 24h).
    MAX: "max",           // Sent the ping containing at least the maximum number (default 10) of prioData elements, earlier than the periodic interval.
    SHUTDOWN: "shutdown", // Recorded data was sent on shutdown.
  }),

  PRIO_PING_TYPE: "prio",

  _logger: null,
  _testing: false,
  _timeoutId: null,

  startup() {
    if (!this._testing && !Services.prefs.getBoolPref(Utils.Preferences.PrioPingEnabled, false)) {
      this._log.trace("Prio ping disabled.");
      return;
    }
    this._log.trace("Starting up.");

    Services.obs.addObserver(this, PRIO_LIMIT_REACHED_TOPIC);

    XPCOMUtils.defineLazyPreferenceGetter(this, "pingFrequency",
                                          Utils.Preferences.PrioPingFrequency,
                                          DEFAULT_PING_FREQUENCY_HOURS);
    this._startTimer();
  },

  async shutdown() {
    this._log.trace("Shutting down.");
    // removeObserver may throw, which could interrupt shutdown.
    try {
      Services.obs.removeObserver(this, PRIO_LIMIT_REACHED_TOPIC);
    } catch (ex) {}

    await this._submitPing(this.Reason.SHUTDOWN);
    this._clearTimer();
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case PRIO_LIMIT_REACHED_TOPIC:
        this._log.trace("prio limit reached");
        this._submitPing(this.Reason.MAX);
        break;
    }
  },

  _startTimer(delay = this.pingFrequency * MILLISECONDS_PER_HOUR, reason = this.Reason.PERIODIC, discardLeftovers = false) {
    this._clearTimer();
    this._timeoutId =
      Policy.setTimeout(() => TelemetryPrioPing._submitPing(reason), delay);
  },

  _clearTimer() {
    if (this._timeoutId) {
      Policy.clearTimeout(this._timeoutId);
      this._timeoutId = null;
    }
  },

  /**
   * Submits an "prio" ping and restarts the timer for the next interval.
   *
   * @param {String} reason The reason we're sending the ping. One of TelemetryPrioPing.Reason.
   */
  async _submitPing(reason) {
    this._log.trace("_submitPing");

    if (reason !== this.Reason.SHUTDOWN) {
      this._startTimer();
    }

    let snapshot = await Policy.getEncodedOriginSnapshot(true /* clear */);

    if (!this._testing) {
      snapshot = snapshot.filter(({encoding}) => !encoding.startsWith("telemetry.test"));
    }

    if (snapshot.length === 0) {
      // Don't send a ping if we haven't anything to send
      this._log.trace("nothing to send");
      return;
    }

    let payload = {
      version: PRIO_PING_VERSION,
      reason,
      prioData: snapshot,
    };

    const options = {
      addClientId: false,
      addEnvironment: false,
      usePingSender: reason === this.Reason.SHUTDOWN,
    };

    Policy.sendPing(this.PRIO_PING_TYPE, payload, options);
  },

  /**
   * Test-only, restore to initial state.
   */
  testReset() {
    this._clearTimer();
    this._testing = true;
  },

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX + "::");
    }

    return this._logger;
  },
};
