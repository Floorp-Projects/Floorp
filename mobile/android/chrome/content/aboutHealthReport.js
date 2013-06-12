#filter substitution
// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/SharedPreferences.jsm");

// Name of Android SharedPreference controlling whether to upload
// health reports.
const PREF_UPLOAD_ENABLED = "android.not_a_preference.healthreport.uploadEnabled";

// Name of Gecko Pref specifying report content location.
const PREF_REPORTURL = "datareporting.healthreport.about.reportUrl";

const EVENT_HEALTH_REQUEST = "HealthReport:Request";
const EVENT_HEALTH_RESPONSE = "HealthReport:Response";

function sendMessageToJava(message) {
  return Cc["@mozilla.org/android/bridge;1"]
    .getService(Ci.nsIAndroidBridge)
    .handleGeckoMessage(JSON.stringify(message));
}

// about:healthreport prefs are stored in Firefox's default Android
// SharedPreferences.
let sharedPrefs = new SharedPreferences();

let healthReportWrapper = {
  init: function () {
    let iframe = document.getElementById("remote-report");
    iframe.addEventListener("load", healthReportWrapper.initRemotePage, false);
    let report = this._getReportURI();
    iframe.src = report.spec;

    sharedPrefs.addObserver(PREF_UPLOAD_ENABLED, this, false);
    Services.obs.addObserver(this, EVENT_HEALTH_RESPONSE, false);
  },

  observe: function (subject, topic, data) {
    if (topic == PREF_UPLOAD_ENABLED) {
      this.updatePrefState();
    } else if (topic == EVENT_HEALTH_RESPONSE) {
      this.updatePayload(data);
    }
  },

  uninit: function () {
    sharedPrefs.removeObserver(PREF_UPLOAD_ENABLED, this);
    Services.obs.removeObserver(this, EVENT_HEALTH_RESPONSE);
  },

  _getReportURI: function () {
    let url = Services.urlFormatter.formatURLPref(PREF_REPORTURL);
    return Services.io.newURI(url, null, null);
  },

  onOptIn: function () {
    console.log("AboutHealthReport: page sent opt-in command.");
    sharedPrefs.setBoolPref(PREF_UPLOAD_ENABLED, true);
    this.updatePrefState();
  },

  onOptOut: function () {
    console.log("AboutHealthReport: page sent opt-out command.");
    sharedPrefs.setBoolPref(PREF_UPLOAD_ENABLED, false);
    this.updatePrefState();
  },

  updatePrefState: function () {
    console.log("AboutHealthReport: sending pref state to page.");
    try {
      let prefs = {
        enabled: sharedPrefs.getBoolPref(PREF_UPLOAD_ENABLED),
      };
      this.injectData("prefs", prefs);
    } catch (e) {
      this.reportFailure(this.ERROR_PREFS_FAILED);
    }
  },

  refreshPayload: function () {
    console.log("AboutHealthReport: page requested fresh payload.");
    sendMessageToJava({
      type: EVENT_HEALTH_REQUEST,
    });
  },

  updatePayload: function (data) {
    healthReportWrapper.injectData("payload", data);
    // Data is supposed to be a string, so the length should be
    // defined.  Just in case, we do this after injecting the data.
    console.log("AboutHealthReport: sending payload to page " +
         "(" + typeof(data) + " of length " + data.length + ").");
  },

  injectData: function (type, content) {
    let report = this._getReportURI();

    // file: URIs can't be used for targetOrigin, so we use "*" for
    // this special case.  In all other cases, pass in the URL to the
    // report so we properly restrict the message dispatch.
    let reportUrl = (report.scheme == "file") ? "*" : report.spec;

    let data = {
      type: type,
      content: content,
    };

    let iframe = document.getElementById("remote-report");
    iframe.contentWindow.postMessage(data, reportUrl);
  },

  handleRemoteCommand: function (evt) {
    switch (evt.detail.command) {
      case "DisableDataSubmission":
        this.onOptOut();
        break;
      case "EnableDataSubmission":
        this.onOptIn();
        break;
      case "RequestCurrentPrefs":
        this.updatePrefState();
        break;
      case "RequestCurrentPayload":
        this.refreshPayload();
        break;
      default:
        Cu.reportError("Unexpected remote command received: " + evt.detail.command +
                       ". Ignoring command.");
        break;
    }
  },

  initRemotePage: function () {
    let iframe = document.getElementById("remote-report").contentDocument;
    iframe.addEventListener("RemoteHealthReportCommand",
                            function onCommand(e) {healthReportWrapper.handleRemoteCommand(e);},
                            false);
    healthReportWrapper.injectData("begin", null);
  },

  // error handling
  ERROR_INIT_FAILED:    1,
  ERROR_PAYLOAD_FAILED: 2,
  ERROR_PREFS_FAILED:   3,

  reportFailure: function (error) {
    let details = {
      errorType: error,
    };
    healthReportWrapper.injectData("error", details);
  },

  handleInitFailure: function () {
    healthReportWrapper.reportFailure(healthReportWrapper.ERROR_INIT_FAILED);
  },

  handlePayloadFailure: function () {
    healthReportWrapper.reportFailure(healthReportWrapper.ERROR_PAYLOAD_FAILED);
  },
};
