/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UITelemetry"];

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);

/**
 * UITelemetry is a helper JSM used to record UI specific telemetry events.
 *
 * It implements nsIUITelemetryObserver, defined in nsIAndroidBridge.idl.
 */
var UITelemetry = {
  _enabled: undefined,
  _activeSessions: {},
  _measurements: [],

  // Lazily decide whether telemetry is enabled.
  get enabled() {
    if (this._enabled !== undefined) {
      return this._enabled;
    }

    // Set an observer to watch for changes at runtime.
    Services.prefs.addObserver(
      TelemetryUtils.Preferences.TelemetryEnabled,
      this
    );
    Services.obs.addObserver(this, "profile-before-change");

    // Pick up the current value.
    this._enabled = Services.prefs.getBoolPref(
      TelemetryUtils.Preferences.TelemetryEnabled,
      false
    );

    return this._enabled;
  },

  observe(aSubject, aTopic, aData) {
    if (aTopic == "profile-before-change") {
      Services.obs.removeObserver(this, "profile-before-change");
      Services.prefs.removeObserver(
        TelemetryUtils.Preferences.TelemetryEnabled,
        this
      );
      this._enabled = undefined;
      return;
    }

    if (aTopic == "nsPref:changed") {
      switch (aData) {
        case TelemetryUtils.Preferences.TelemetryEnabled:
          let on = Services.prefs.getBoolPref(
            TelemetryUtils.Preferences.TelemetryEnabled
          );
          this._enabled = on;

          // Wipe ourselves if we were just disabled.
          if (!on) {
            this._activeSessions = {};
            this._measurements = [];
          }
          break;
      }
    }
  },

  /**
   * This exists exclusively for testing -- our events are not intended to
   * be retrieved via an XPCOM interface.
   */
  get wrappedJSObject() {
    return this;
  },

  /**
   * A hack to generate the relative timestamp from start when we don't have
   * access to the Java timer.
   * XXX: Bug 1007647 - Support realtime and/or uptime in JavaScript.
   */
  uptimeMillis() {
    return Date.now() - Services.startup.getStartupInfo().process;
  },

  /**
   * Adds a single event described by a timestamp, an action, and the calling
   * method.
   *
   * Optionally provide a string 'extras', which will be recorded as part of
   * the event.
   *
   * All extant sessions will be recorded by name for each event.
   */
  addEvent(aAction, aMethod, aTimestamp, aExtras) {
    if (!this.enabled) {
      return;
    }

    let sessions = Object.keys(this._activeSessions);
    let aEvent = {
      type: "event",
      action: aAction,
      method: aMethod,
      sessions,
      timestamp: aTimestamp == undefined ? this.uptimeMillis() : aTimestamp,
    };

    if (aExtras) {
      aEvent.extras = aExtras;
    }

    this._recordEvent(aEvent);
  },

  /**
   * Begins tracking a session by storing a timestamp for session start.
   */
  startSession(aName, aTimestamp) {
    if (!this.enabled) {
      return;
    }

    if (this._activeSessions[aName]) {
      // Do not overwrite a previous event start if it already exists.
      return;
    }
    this._activeSessions[aName] =
      aTimestamp == undefined ? this.uptimeMillis() : aTimestamp;
  },

  /**
   * Tracks the end of a session with a timestamp.
   */
  stopSession(aName, aReason, aTimestamp) {
    if (!this.enabled) {
      return;
    }

    let sessionStart = this._activeSessions[aName];
    delete this._activeSessions[aName];

    if (!sessionStart) {
      return;
    }

    let aEvent = {
      type: "session",
      name: aName,
      reason: aReason,
      start: sessionStart,
      end: aTimestamp == undefined ? this.uptimeMillis() : aTimestamp,
    };

    this._recordEvent(aEvent);
  },

  _recordEvent(aEvent) {
    this._measurements.push(aEvent);
  },

  /**
   * Called by TelemetrySession to populate the UI measurement
   * blob.
   *
   * Optionally clears the set of measurements based on aClear.
   */
  getUIMeasurements(aClear) {
    if (!this.enabled) {
      return [];
    }

    let measurements = this._measurements.slice();
    if (aClear) {
      this._measurements = [];
    }
    return measurements;
  },
};
