/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

this.EXPORTED_SYMBOLS = [
  "UITelemetry"
];

Cu.import("resource://gre/modules/Services.jsm");

/**
 * UITelemetry is a helper JSM used to record UI specific telemetry events.
 */
this.UITelemetry =  {

  measurements: [],

  init: function init() {
    Services.obs.addObserver(this, "UITelemetry:Event", false);
    Services.obs.addObserver(this, "UITelemetry:Session", false);
  },

  observe: function observe(aMessage, aTopic, aData) {
    switch(aTopic) {
      case "UITelemetry:Event":
        let args = JSON.parse(aData);
        this.addEvent(args.action, args.method, args.extras, args.timestamp);
        break;
      case "UITelemetry:Session":
        args = JSON.parse(aData);
        let sessionName = args.name;
        let timestamp = args.timestamp;
        if (args.state == "start") {
          this.startSession(sessionName, timestamp);
        } else if (args.state == "stop") {
          this.stopSession(sessionName, timestamp);
        }
        break;
    }
  },

  /**
   * Adds a single event described by an action, and the calling method. Optional
   * paramaters are extras and timestamp. The timestamp will be set here if it is
   * not passed in by the caller.
   */
  addEvent: function addEvent(aAction, aMethod, aExtras, aTimestamp) {
    let timestamp = aTimestamp || Date.now();
    let aEvent = {
      type: "event",
      action: aAction,
      method: aMethod,
      timestamp: timestamp
    };

    if (aExtras) aEvent.extras = aExtras;
    this._logEvent(aEvent);
  },

  activeSessions: {},

  /**
   * Begins tracking a session by storing a timestamp for session start.
   */
  startSession: function startSession(aName, aTimestamp) {
   let timestamp = aTimestamp || Date.now();
   if (this.activeSessions[aName]) {
    // Do not overwrite a previous event start if it already exsts.
    return;
   }
   this.activeSessions[aName] = timestamp;
  },

  /**
   * Tracks the end of a session with a timestamp.
   */
  stopSession: function stopSession(aName, aTimestamp) {
    let timestamp = aTimestamp || Date.now();
    let sessionStart = this.activeSessions[aName];

    if (!sessionStart) {
      Services.console.logStringMessage("UITelemetry error: no session [" + aName + "] to stop!");
      return;
    }

    let aEvent = {
      type: "session",
      name: aName,
      start: sessionStart,
      end: timestamp
    };

    this._logEvent(aEvent);
  },

  _logEvent: function sendEvent(aEvent) {
    this.measurements.push(aEvent);
  }
};
