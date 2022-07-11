/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["OsEnvironment"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  WindowsRegistry: "resource://gre/modules/WindowsRegistry.jsm",
  WindowsVersionInfo:
    "resource://gre/modules/components-utils/WindowsVersionInfo.jsm",
});

let OsEnvironment = {
  /**
   * This is a policy object used to override behavior for testing.
   */
  Policy: {
    getAllowedAppSources: () =>
      lazy.WindowsRegistry.readRegKey(
        Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer",
        "AicEnabled"
      ),
    windowsVersionHasAppSourcesFeature: () => {
      let windowsVersion = parseFloat(Services.sysinfo.getProperty("version"));
      if (isNaN(windowsVersion)) {
        throw new Error("Unable to parse Windows version");
      }
      if (windowsVersion < 10) {
        return false;
      }

      // The App Sources feature was added in Windows 10, build 15063.
      const { buildNumber } = lazy.WindowsVersionInfo.get();
      return buildNumber >= 15063;
    },
  },

  reportAllowedAppSources() {
    if (AppConstants.platform != "win") {
      // This is currently a windows-only feature.
      return;
    }

    const appSourceScalar = "os.environment.allowed_app_sources";

    let haveAppSourcesFeature;
    try {
      haveAppSourcesFeature = OsEnvironment.Policy.windowsVersionHasAppSourcesFeature();
    } catch (ex) {
      Cu.reportError(ex);
      Services.telemetry.scalarSet(appSourceScalar, "Error");
      return;
    }
    if (!haveAppSourcesFeature) {
      Services.telemetry.scalarSet(appSourceScalar, "NoSuchFeature");
      return;
    }

    let allowedAppSources;
    try {
      allowedAppSources = OsEnvironment.Policy.getAllowedAppSources();
    } catch (ex) {
      Cu.reportError(ex);
      Services.telemetry.scalarSet(appSourceScalar, "Error");
      return;
    }
    if (allowedAppSources === undefined) {
      // A return value of undefined means that the registry value didn't
      // exist. Windows treats a missing registry entry the same as if the
      // value is "Anywhere". Make sure to differentiate a missing registry
      // entry from one containing an empty string, since it is unclear how
      // Windows would treat such a setting, but it may not be the same as
      // if the value is missing.
      allowedAppSources = "Anywhere";
    }

    const expectedValues = [
      "Anywhere",
      "Recommendations",
      "PreferStore",
      "StoreOnly",
    ];
    if (!expectedValues.includes(allowedAppSources)) {
      allowedAppSources = "Error";
    }

    Services.telemetry.scalarSet(appSourceScalar, allowedAppSources);
  },
};
