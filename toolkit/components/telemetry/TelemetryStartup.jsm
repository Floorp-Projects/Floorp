/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "TelemetryController",
  "resource://gre/modules/TelemetryController.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm"
);

/**
 * TelemetryStartup is needed to forward the "profile-after-change" notification
 * to TelemetryController.jsm.
 */
function TelemetryStartup() {}

TelemetryStartup.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIObserver",
]);
TelemetryStartup.prototype.observe = function(aSubject, aTopic, aData) {
  if (aTopic == "profile-after-change") {
    // In the content process, this is done in ContentProcessSingleton.js.
    TelemetryController.observe(null, aTopic, null);
  }
  if (aTopic == "profile-after-change") {
    annotateEnvironment();
    TelemetryEnvironment.registerChangeListener(
      "CrashAnnotator",
      annotateEnvironment
    );
    TelemetryEnvironment.onInitialized().then(() => annotateEnvironment());
  }
};

function annotateEnvironment() {
  try {
    let cr = Cc["@mozilla.org/toolkit/crash-reporter;1"];
    if (cr) {
      let env = JSON.stringify(TelemetryEnvironment.currentEnvironment);
      cr.getService(Ci.nsICrashReporter).annotateCrashReport(
        "TelemetryEnvironment",
        env
      );
    }
  } catch (e) {
    // crash reporting not built or disabled? Ignore errors
  }
}

var EXPORTED_SYMBOLS = ["TelemetryStartup"];
