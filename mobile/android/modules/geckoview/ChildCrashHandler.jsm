/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ChildCrashHandler"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

const { debug, warn } = GeckoViewUtils.initLogging("ChildCrashHandler");

function getDir(name) {
  const uAppDataPath = Services.dirsvc.get("UAppData", Ci.nsIFile).path;
  return lazy.OS.Path.join(uAppDataPath, "Crash Reports", name);
}

function getPendingMinidump(id) {
  const pendingDir = getDir("pending");

  return [".dmp", ".extra"].map(suffix => {
    return lazy.OS.Path.join(pendingDir, `${id}${suffix}`);
  });
}

var ChildCrashHandler = {
  // The event listener for this is hooked up in GeckoViewStartup.jsm
  observe(aSubject, aTopic, aData) {
    if (
      aTopic !== "ipc:content-shutdown" &&
      aTopic !== "compositor:process-aborted"
    ) {
      return;
    }

    aSubject.QueryInterface(Ci.nsIPropertyBag2);

    const disableReporting = Cc["@mozilla.org/process/environment;1"]
      .getService(Ci.nsIEnvironment)
      .get("MOZ_CRASHREPORTER_NO_REPORT");

    if (
      !aSubject.get("abnormal") ||
      !AppConstants.MOZ_CRASHREPORTER ||
      disableReporting
    ) {
      return;
    }

    // If dumpID is empty the process was likely killed by the system and we therefore do not want
    // to report the crash.
    const dumpID = aSubject.get("dumpID");
    if (!dumpID) {
      Services.telemetry
        .getHistogramById("FX_CONTENT_CRASH_DUMP_UNAVAILABLE")
        .add(1);
      return;
    }

    debug`Notifying child process crash, dump ID ${dumpID}`;
    const [minidumpPath, extrasPath] = getPendingMinidump(dumpID);

    // Report GPU process crashes as occuring in a background process, and others as foreground.
    const processType =
      aTopic === "compositor:process-aborted"
        ? "BACKGROUND_CHILD"
        : "FOREGROUND_CHILD";

    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:ChildCrashReport",
      minidumpPath,
      extrasPath,
      success: true,
      fatal: false,
      processType,
    });
  },
};
