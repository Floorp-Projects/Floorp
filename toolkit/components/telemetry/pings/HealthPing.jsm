/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module collects data on send failures and other critical issues with Telemetry submissions.
 */

"use strict";

var EXPORTED_SYMBOLS = ["TelemetryHealthPing", "Policy"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "TelemetryController",
  "resource://gre/modules/TelemetryController.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "clearTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(lazy, "Log", "resource://gre/modules/Log.jsm");

const Utils = TelemetryUtils;

const MS_IN_A_MINUTE = 60 * 1000;
// Send health ping every hour
const SEND_TICK_DELAY = 60 * MS_IN_A_MINUTE;

// Send top 10 discarded pings only to minimize health ping size
const MAX_SEND_DISCARDED_PINGS = 10;

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryHealthPing::";

var Policy = {
  setSchedulerTickTimeout: (callback, delayMs) =>
    lazy.setTimeout(callback, delayMs),
  clearSchedulerTickTimeout: id => lazy.clearTimeout(id),
};

var TelemetryHealthPing = {
  Reason: Object.freeze({
    IMMEDIATE: "immediate", // Ping was sent immediately after recording with no delay.
    DELAYED: "delayed", // Recorded data was sent after a delay.
    SHUT_DOWN: "shutdown", // Recorded data was sent on shutdown.
  }),

  FailureType: Object.freeze({
    DISCARDED_FOR_SIZE: "pingDiscardedForSize",
    SEND_FAILURE: "sendFailure",
  }),

  OsInfo: Object.freeze({
    name: Services.appinfo.OS,
    version:
      Services.sysinfo.get("kernel_version") || Services.sysinfo.get("version"),
  }),

  HEALTH_PING_TYPE: "health",

  _logger: null,

  // The health ping is sent every every SEND_TICK_DELAY.
  // Initialize this so that first failures are sent immediately.
  _lastSendTime: -SEND_TICK_DELAY,

  /**
   * This stores reported send failures with the following structure:
   * {
   *  type1: {
   *    subtype1: value,
   *    ...
   *    subtypeN: value
   *  },
   *  ...
   * }
   */
  _failures: {},
  _timeoutId: null,

  /**
   * Record a failure to send a ping out.
   * @param {String} failureType The type of failure (e.g. "timeout", ...).
   * @returns {Promise} Test-only, resolved when the ping is stored or sent.
   */
  recordSendFailure(failureType) {
    return this._addToFailure(this.FailureType.SEND_FAILURE, failureType);
  },

  /**
   * Record that a ping was discarded and its type.
   * @param {String} pingType The type of discarded ping (e.g. "main", ...).
   * @returns {Promise} Test-only, resolved when the ping is stored or sent.
   */
  recordDiscardedPing(pingType) {
    return this._addToFailure(this.FailureType.DISCARDED_FOR_SIZE, pingType);
  },

  /**
   * Assemble payload.
   * @param {String} reason A string indicating the triggering reason (e.g. "immediate", "delayed", "shutdown").
   * @returns {Object} The assembled payload.
   */
  _assemblePayload(reason) {
    this._log.trace("_assemblePayload()");
    let payload = {
      os: this.OsInfo,
      reason,
    };

    for (let key of Object.keys(this._failures)) {
      if (key === this.FailureType.DISCARDED_FOR_SIZE) {
        payload[key] = this._getTopDiscardFailures(this._failures[key]);
      } else {
        payload[key] = this._failures[key];
      }
    }

    return payload;
  },

  /**
   * Sort input dictionary descending by value.
   * @param {Object} failures A dictionary of failures subtype and count.
   * @returns {Object} Sorted failures by value.
   */
  _getTopDiscardFailures(failures) {
    this._log.trace("_getTopDiscardFailures()");
    let sortedItems = Object.entries(failures).sort((first, second) => {
      return second[1] - first[1];
    });

    let result = {};
    sortedItems.slice(0, MAX_SEND_DISCARDED_PINGS).forEach(([key, value]) => {
      result[key] = value;
    });

    return result;
  },

  /**
   * Assemble the failure information and submit it.
   * @param {String} reason A string indicating the triggering reason (e.g. "immediate", "delayed", "shutdown").
   * @returns {Promise} Test-only promise that resolves when the ping was stored or sent (if any).
   */
  _submitPing(reason) {
    if (!TelemetryHealthPing.enabled || !this._hasDataToSend()) {
      return Promise.resolve();
    }

    this._log.trace("_submitPing(" + reason + ")");
    let payload = this._assemblePayload(reason);
    this._clearData();
    this._lastSendTime = Utils.monotonicNow();

    let options = {
      addClientId: true,
      usePingSender: reason === this.Reason.SHUT_DOWN,
    };

    return new Promise(r =>
      // If we submit the health ping immediately, the send task would be triggered again
      // before discarding oversized pings from the queue.
      // To work around this, we send the ping on the next tick.
      Services.tm.dispatchToMainThread(() =>
        r(
          lazy.TelemetryController.submitExternalPing(
            this.HEALTH_PING_TYPE,
            payload,
            options
          )
        )
      )
    );
  },

  /**
   * Accumulate failure information and trigger a ping immediately or on timeout.
   * @param {String} failureType The type of failure (e.g. "timeout", ...).
   * @param {String} failureSubType The subtype of failure (e.g. ping type, ...).
   * @returns {Promise} Test-only, resolved when the ping is stored or sent.
   */
  _addToFailure(failureType, failureSubType) {
    this._log.trace(
      "_addToFailure() - with type and subtype: " +
        failureType +
        " : " +
        failureSubType
    );

    if (!(failureType in this._failures)) {
      this._failures[failureType] = {};
    }

    let current = this._failures[failureType][failureSubType] || 0;
    this._failures[failureType][failureSubType] = current + 1;

    const now = Utils.monotonicNow();
    if (now - this._lastSendTime >= SEND_TICK_DELAY) {
      return this._submitPing(this.Reason.IMMEDIATE);
    }

    let submissionDelay = SEND_TICK_DELAY - now - this._lastSendTime;
    this._timeoutId = Policy.setSchedulerTickTimeout(
      () => TelemetryHealthPing._submitPing(this.Reason.DELAYED),
      submissionDelay
    );
    return Promise.resolve();
  },

  /**
   * @returns {boolean} Check the availability of recorded failures data.
   */
  _hasDataToSend() {
    return Object.keys(this._failures).length !== 0;
  },

  /**
   * Clear recorded failures data.
   */
  _clearData() {
    this._log.trace("_clearData()");
    this._failures = {};
  },

  /**
   * Clear and reset timeout.
   */
  _resetTimeout() {
    if (this._timeoutId) {
      Policy.clearSchedulerTickTimeout(this._timeoutId);
      this._timeoutId = null;
    }
  },

  /**
   * Submit latest ping on shutdown.
   * @returns {Promise} Test-only, resolved when the ping is stored or sent.
   */
  shutdown() {
    this._log.trace("shutdown()");
    this._resetTimeout();
    return this._submitPing(this.Reason.SHUT_DOWN);
  },

  /**
   * Test-only, restore to initial state.
   */
  testReset() {
    this._lastSendTime = -SEND_TICK_DELAY;
    this._clearData();
    this._resetTimeout();
  },

  get _log() {
    if (!this._logger) {
      this._logger = lazy.Log.repository.getLoggerWithMessagePrefix(
        LOGGER_NAME,
        LOGGER_PREFIX + "::"
      );
    }

    return this._logger;
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  TelemetryHealthPing,
  "enabled",
  TelemetryUtils.Preferences.HealthPingEnabled,
  true
);
