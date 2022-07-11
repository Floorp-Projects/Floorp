/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  WindowsInstallsInfo:
    "resource://gre/modules/components-utils/WindowsInstallsInfo.jsm",
});

var EXPORTED_SYMBOLS = ["UninstallPing"];

/**
 * The Windows-only "uninstall" ping, which is saved to disk for the uninstaller to find.
 * The ping is actually assembled by TelemetryControllerParent.saveUninstallPing().
 */
var UninstallPing = {
  /**
   * Maximum number of other installs to count (see
   * toolkit/components/telemetry/docs/data/uninstall-ping.rst for motivation)
   */
  MAX_OTHER_INSTALLS: 11,

  /**
   * Count other installs of this app, based on the values in the TaskBarIDs registry key.
   *
   */
  getOtherInstallsCount() {
    return lazy.WindowsInstallsInfo.getInstallPaths(
      this.MAX_OTHER_INSTALLS,
      new Set([Services.dirsvc.get("GreBinD", Ci.nsIFile).path])
    ).size;
  },
};
