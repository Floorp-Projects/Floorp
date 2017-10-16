/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "TelemetryController",
                                  "resource://gre/modules/TelemetryController.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TelemetryEnvironment",
                                  "resource://gre/modules/TelemetryEnvironment.jsm");

/**
 * TelemetryStartup is needed to forward the "profile-after-change" notification
 * to TelemetryController.jsm.
 */
function TelemetryStartup() {
}

TelemetryStartup.prototype.classID = Components.ID("{117b219f-92fe-4bd2-a21b-95a342a9d474}");
TelemetryStartup.prototype.QueryInterface = XPCOMUtils.generateQI([Components.interfaces.nsIObserver]);
TelemetryStartup.prototype.observe = function(aSubject, aTopic, aData) {
  if (aTopic == "profile-after-change") {
    // In the content process, this is done in ContentProcessSingleton.js.
    TelemetryController.observe(null, aTopic, null);
  }
  if (aTopic == "profile-after-change") {
    annotateEnvironment();
    TelemetryEnvironment.registerChangeListener("CrashAnnotator", annotateEnvironment);
    TelemetryEnvironment.onInitialized().then(() => annotateEnvironment());
  }
};

function annotateEnvironment() {
  try {
    let cr = Cc["@mozilla.org/toolkit/crash-reporter;1"];
    if (cr) {
      let env = JSON.stringify(TelemetryEnvironment.currentEnvironment);
      cr.getService(Ci.nsICrashReporter).annotateCrashReport("TelemetryEnvironment", env);
    }
  } catch (e) {
    // crash reporting not built or disabled? Ignore errors
  }
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TelemetryStartup]);
