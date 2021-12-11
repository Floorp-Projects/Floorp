/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var MonitorAPI = ChromeUtils.import("resource://gre/modules/CrashMonitor.jsm")
  .CrashMonitor;

function CrashMonitor() {}

CrashMonitor.prototype = {
  classID: Components.ID("{d9d75e86-8f17-4c57-993e-f738f0d86d42}"),
  contractID: "@mozilla.org/toolkit/crashmonitor;1",

  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "profile-after-change":
        MonitorAPI.init();
    }
  },
};

var EXPORTED_SYMBOLS = ["CrashMonitor"];
