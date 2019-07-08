/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module sends Telemetry Events periodically:
 * https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/data/event-ping.html
 */

"use strict";

var EXPORTED_SYMBOLS = ["TelemetryEventPing"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetters(this, {
  TelemetrySession: "resource://gre/modules/TelemetrySession.jsm",
  TelemetryController: "resource://gre/modules/TelemetryController.jsm",
  Log: "resource://gre/modules/Log.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  Telemetry: ["@mozilla.org/base/telemetry;1", "nsITelemetry"],
});

ChromeUtils.defineModuleGetter(
  this,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "clearTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryUtils",
  "resource://gre/modules/TelemetryUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

const Utils = TelemetryUtils;

const MS_IN_A_MINUTE = 60 * 1000;

const DEFAULT_EVENT_LIMIT = 1000;
const DEFAULT_MIN_FREQUENCY_MS = 60 * MS_IN_A_MINUTE;
const DEFAULT_MAX_FREQUENCY_MS = 10 * MS_IN_A_MINUTE;

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryEventPing::";

const EVENT_LIMIT_REACHED_TOPIC = "event-telemetry-storage-limit-reached";

var Policy = {
  setTimeout: (callback, delayMs) => setTimeout(callback, delayMs),
  clearTimeout: id => clearTimeout(id),
  sendPing: (type, payload, options) =>
    TelemetryController.submitExternalPing(type, payload, options),
};

var TelemetryEventPing = {
  Reason: Object.freeze({
    PERIODIC: "periodic", // Sent the ping containing events from the past periodic interval (default one hour).
    MAX: "max", // Sent the ping containing the maximum number (default 1000) of event records, earlier than the periodic interval.
    SHUTDOWN: "shutdown", // Recorded data was sent on shutdown.
  }),

  EVENT_PING_TYPE: "event",

  _logger: null,

  _testing: false,

  // So that if we quickly reach the max limit we can immediately send.
  _lastSendTime: -DEFAULT_MIN_FREQUENCY_MS,

  _processStartTimestamp: 0,

  get dataset() {
    return Telemetry.canRecordPrereleaseData
      ? Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS
      : Ci.nsITelemetry.DATASET_ALL_CHANNELS;
  },

  startup() {
    if (!Services.prefs.getBoolPref(Utils.Preferences.EventPingEnabled, true)) {
      return;
    }
    this._log.trace("Starting up.");

    // Calculate process creation once.
    this._processStartTimestamp =
      Math.round(
        (Date.now() - TelemetryUtils.monotonicNow()) / MS_IN_A_MINUTE
      ) * MS_IN_A_MINUTE;

    Services.obs.addObserver(this, EVENT_LIMIT_REACHED_TOPIC);

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "maxEventsPerPing",
      Utils.Preferences.EventPingEventLimit,
      DEFAULT_EVENT_LIMIT
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "maxFrequency",
      Utils.Preferences.EventPingMaximumFrequency,
      DEFAULT_MAX_FREQUENCY_MS
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "minFrequency",
      Utils.Preferences.EventPingMinimumFrequency,
      DEFAULT_MIN_FREQUENCY_MS
    );

    this._startTimer();
  },

  shutdown() {
    this._log.trace("Shutting down.");
    // removeObserver may throw, which could interrupt shutdown.
    try {
      Services.obs.removeObserver(this, EVENT_LIMIT_REACHED_TOPIC);
    } catch (ex) {}

    this._submitPing(this.Reason.SHUTDOWN, true /* discardLeftovers */);
    this._clearTimer();
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case EVENT_LIMIT_REACHED_TOPIC:
        this._log.trace("event limit reached");
        let now = Utils.monotonicNow();
        if (now - this._lastSendTime < this.maxFrequency) {
          this._log.trace("can't submit ping immediately as it's too soon");
          this._startTimer(
            this.maxFrequency - this._lastSendTime,
            this.Reason.MAX,
            true /* discardLeftovers*/
          );
        } else {
          this._log.trace("submitting ping immediately");
          this._submitPing(this.Reason.MAX);
        }
        break;
    }
  },

  _startTimer(
    delay = this.minFrequency,
    reason = this.Reason.PERIODIC,
    discardLeftovers = false
  ) {
    this._clearTimer();
    this._timeoutId = Policy.setTimeout(
      () => TelemetryEventPing._submitPing(reason, discardLeftovers),
      delay
    );
  },

  _clearTimer() {
    if (this._timeoutId) {
      Policy.clearTimeout(this._timeoutId);
      this._timeoutId = null;
    }
  },

  /**
   * Submits an "event" ping and restarts the timer for the next interval.
   *
   * @param {String} reason The reason we're sending the ping. One of TelemetryEventPing.Reason.
   * @param {bool} discardLeftovers Whether to discard event records left over from a previous ping.
   */
  _submitPing(reason, discardLeftovers = false) {
    this._log.trace("_submitPing");

    if (reason !== this.Reason.SHUTDOWN) {
      this._startTimer();
    }

    let snapshot = Telemetry.snapshotEvents(
      this.dataset,
      true /* clear */,
      this.maxEventsPerPing
    );

    if (!this._testing) {
      for (let process of Object.keys(snapshot)) {
        snapshot[process] = snapshot[process].filter(
          ([, category]) => !category.startsWith("telemetry.test")
        );
      }
    }

    let eventCount = Object.values(snapshot).reduce(
      (acc, val) => acc + val.length,
      0
    );
    if (eventCount === 0) {
      // Don't send a ping if we haven't any events.
      this._log.trace("not sending event ping due to lack of events");
      return;
    }

    // The reason doesn't matter as it will just be echo'd back.
    let sessionMeta = TelemetrySession.getMetadata(reason);

    let payload = {
      reason,
      processStartTimestamp: this._processStartTimestamp,
      sessionId: sessionMeta.sessionId,
      subsessionId: sessionMeta.subsessionId,
      lostEventsCount: 0,
      events: snapshot,
    };

    if (discardLeftovers) {
      // Any leftovers must be discarded, the count submitted in the ping.
      // This can happen on shutdown or if our max was reached before faster
      // than our maxFrequency.
      let leftovers = Telemetry.snapshotEvents(this.dataset, true /* clear */);
      let leftoverCount = Object.values(leftovers).reduce(
        (acc, val) => acc + val.length,
        0
      );
      payload.lostEventsCount = leftoverCount;
    }

    const options = {
      addClientId: true,
      addEnvironment: true,
      usePingSender: reason == this.Reason.SHUTDOWN,
    };

    this._lastSendTime = Utils.monotonicNow();
    Telemetry.getHistogramById("TELEMETRY_EVENT_PING_SENT").add(reason);
    Policy.sendPing(this.EVENT_PING_TYPE, payload, options);
  },

  /**
   * Test-only, restore to initial state.
   */
  testReset() {
    this._lastSendTime = -DEFAULT_MIN_FREQUENCY_MS;
    this._clearTimer();
    this._testing = true;
  },

  get _log() {
    if (!this._logger) {
      this._logger = Log.repository.getLoggerWithMessagePrefix(
        LOGGER_NAME,
        LOGGER_PREFIX + "::"
      );
    }

    return this._logger;
  },
};
