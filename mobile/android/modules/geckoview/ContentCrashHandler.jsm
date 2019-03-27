/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ContentCrashHandler"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {GeckoViewUtils} = ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

ChromeUtils.defineModuleGetter(this, "OS",
                               "resource://gre/modules/osfile.jsm");

const {debug, warn} = GeckoViewUtils.initLogging("ContentCrashHandler"); // eslint-disable-line no-unused-vars

function getDir(name) {
  const uAppDataPath = Services.dirsvc.get("UAppData", Ci.nsIFile).path;
  return OS.Path.join(uAppDataPath, "Crash Reports", name);
}

function getPendingMinidump(id) {
  const pendingDir = getDir("pending");

  return [".dmp", ".extra"].map(suffix => {
    return OS.Path.join(pendingDir, `${id}${suffix}`);
  });
}

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

    const dumpID = aSubject.get("dumpID");
    if (!dumpID) {
      Services.telemetry
              .getHistogramById("FX_CONTENT_CRASH_DUMP_UNAVAILABLE")
              .add(1);
      return;
    }

    debug `Notifying content process crash, dump ID ${dumpID}`;
    const [minidumpPath, extrasPath] = getPendingMinidump(dumpID);

    EventDispatcher.instance.sendRequest({
      type: "GeckoView:ContentCrash",
      minidumpPath,
      extrasPath,
      success: true,
      fatal: false,
    });
  },
};
