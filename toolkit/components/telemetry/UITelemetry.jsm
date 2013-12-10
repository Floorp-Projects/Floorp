/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

this.EXPORTED_SYMBOLS = [
  "UITelemetry",
];

Cu.import("resource://gre/modules/Services.jsm");

/**
 * UITelemetry is a helper JSM used to record UI specific telemetry events.
 *
 * It implements nsIUITelemetryObserver, defined in nsIAndroidBridge.idl.
 */
this.UITelemetry = Object.freeze({
  _activeSessions: {},
  _measurements: [],

  /**
   * This exists exclusively for testing -- our events are not intended to
   * be retrieved via an XPCOM interface.
   */
  get wrappedJSObject() {
    return this;
  },

  /**
   * Holds the functions that provide UITelemetry's simple
   * measurements. Those functions are mapped to unique names,
   * and should be registered with addSimpleMeasureFunction.
   */
  _simpleMeasureFunctions: {},

  /**
   * Adds a single event described by a timestamp, an action, and the calling
   * method.
   *
   * Optionally provide a string 'extras', which will be recorded as part of
   * the event.
   *
   * All extant sessions will be recorded by name for each event.
   */
  addEvent: function(aAction, aMethod, aTimestamp, aExtras) {
    let sessions = Object.keys(this._activeSessions);
    let aEvent = {
      type: "event",
      action: aAction,
      method: aMethod,
      sessions: sessions,
      timestamp: aTimestamp,
    };

    if (aExtras) {
      aEvent.extras = aExtras;
    }

    this._recordEvent(aEvent);
  },

  /**
   * Begins tracking a session by storing a timestamp for session start.
   */
  startSession: function(aName, aTimestamp) {
    if (this._activeSessions[aName]) {
      // Do not overwrite a previous event start if it already exists.
      return;
    }
    this._activeSessions[aName] = aTimestamp;
  },

  /**
   * Tracks the end of a session with a timestamp.
   */
  stopSession: function(aName, aReason, aTimestamp) {
    let sessionStart = this._activeSessions[aName];
    delete this._activeSessions[aName];

    if (!sessionStart) {
      Services.console.logStringMessage("UITelemetry error: no session [" + aName + "] to stop!");
      return;
    }

    let aEvent = {
      type: "session",
      name: aName,
      reason: aReason,
      start: sessionStart,
      end: aTimestamp,
    };

    this._recordEvent(aEvent);
  },

  _recordEvent: function(aEvent) {
    this._measurements.push(aEvent);
  },

  /**
   * Called by TelemetryPing to populate the simple measurement
   * blob. This function will iterate over all functions added
   * via addSimpleMeasureFunction and return an object with the
   * results of those functions.
   */
  getSimpleMeasures: function() {
    let result = {};
    for (let name in this._simpleMeasureFunctions) {
      result[name] = this._simpleMeasureFunctions[name]();
    }
    return result;
  },

  /**
   * Allows the caller to register functions that will get called
   * for simple measures during a Telemetry ping. aName is a unique
   * identifier used as they key for the simple measurement in the
   * object that getSimpleMeasures returns.
   *
   * This function throws an exception if aName already has a function
   * registered for it.
   */
  addSimpleMeasureFunction: function(aName, aFunction) {
    if (aName in this._simpleMeasureFunctions) {
      throw new Error("A simple measurement function is already registered for " + aName);
    }

    if (!aFunction || typeof aFunction !== 'function') {
      throw new Error("addSimpleMeasureFunction called with non-function argument.");
    }

    this._simpleMeasureFunctions[aName] = aFunction;
  },

  removeSimpleMeasureFunction: function(aName) {
    delete this._simpleMeasureFunctions[aName];
  },

  getUIMeasurements: function getUIMeasurements() {
    return this._measurements.slice();
  }
});
