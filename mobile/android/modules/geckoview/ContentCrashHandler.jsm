/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ContentCrashHandler"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  CrashSubmit: "resource://gre/modules/CrashSubmit.jsm",
  Services: "resource://gre/modules/Services.jsm"
});

GeckoViewUtils.initLogging("ContentCrashHandler", this);

var ContentCrashHandler = {
  // The event listener for this is hooked up in GeckoViewStartup.js
  observe(aSubject, aTopic, aData) {
    aSubject.QueryInterface(Ci.nsIPropertyBag2);

    const disableReporting = Cc["@mozilla.org/process/environment;1"].
      getService(Ci.nsIEnvironment).get("MOZ_CRASHREPORTER_NO_REPORT");

    if (!aSubject.get("abnormal") || !AppConstants.MOZ_CRASHREPORTER ||
        disableReporting) {
      return;
    }

    let dumpID = aSubject.get("dumpID");
    if (!dumpID) {
      Services.telemetry
              .getHistogramById("FX_CONTENT_CRASH_DUMP_UNAVAILABLE")
              .add(1);
      return;
    }

    debug `Submitting content process crash, dump ID ${dumpID}`;
    CrashSubmit.submit(dumpID, {}).then(crashID => {
      debug `Crash submission successful: ${crashID}`;
    }, ChromeUtils.reportError);
  }
};
