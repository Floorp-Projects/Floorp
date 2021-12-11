/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

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
  getOtherInstallsCount(rootKey) {
    // This is somewhat more complicated than just counting the number of values on the key:
    // 1) the same install can be listed in both HKCU and HKLM
    // 2) the current path may not be listed
    //
    // The strategy is to add all paths to a Set (to deduplicate) and use that size.

    // Add the names of the values under `rootKey\subKey` to `set`, until the set has `maxCount`.
    // All strings are lower cased first, as Windows paths are not case-sensitive.
    function collectValues(rootKey, wowFlag, subKey, set, maxCount) {
      if (set.size >= maxCount) {
        return;
      }

      const key = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
        Ci.nsIWindowsRegKey
      );

      try {
        key.open(rootKey, subKey, key.ACCESS_READ | wowFlag);
      } catch (_e) {
        // The key may not exist, ignore.
        // (nsWindowsRegKey::Open doesn't provide detailed error codes)
        return;
      }
      const valueCount = key.valueCount;

      try {
        for (let i = 0; i < valueCount && set.size < maxCount; ++i) {
          set.add(key.getValueName(i).toLowerCase());
        }
      } finally {
        key.close();
      }
    }

    const subKeyName = `Software\\Mozilla\\${Services.appinfo.name}\\TaskBarIDs`;

    const paths = new Set();

    // The current install path may not have a value in TaskBarIDs. It is simpler to
    // pre-add it and always include it in the total, than to check for it everywhere and
    // sometimes include it.
    paths.add(Services.dirsvc.get("GreBinD", Ci.nsIFile).path.toLowerCase());

    const initialPathsCount = paths.size;
    const maxPathsCount = initialPathsCount + this.MAX_OTHER_INSTALLS;

    // First collect from HKLM for both 32-bit and 64-bit installs regardless of the architecture
    // of the current application.
    for (const wowFlag of [
      Ci.nsIWindowsRegKey.WOW64_32,
      Ci.nsIWindowsRegKey.WOW64_64,
    ]) {
      collectValues(
        Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
        wowFlag,
        subKeyName,
        paths,
        maxPathsCount
      );
    }

    // Then collect from HKCU.
    // HKCU\Software is shared between 32 and 64 so nothing special is needed for WOW64,
    // ref https://docs.microsoft.com/en-us/windows/win32/winprog64/shared-registry-keys
    collectValues(
      Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
      0 /* wowFlag */,
      subKeyName,
      paths,
      maxPathsCount
    );

    // Subtract our pre-added path, which is not an "other" install.
    return paths.size - initialPathsCount;
  },
};
