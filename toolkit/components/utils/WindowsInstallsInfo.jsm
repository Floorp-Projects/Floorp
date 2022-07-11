/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["WindowsInstallsInfo"];

var WindowsInstallsInfo = {
  /**
   * Retrieve install paths of this app, based on the values in the TaskBarIDs registry key.
   *
   * Installs from unarchived packages do not have a TaskBarID registry key and
   * therefore won't appear in the result.
   *
   * @param {Number} [limit] Optional, maximum number of installation paths to count.
            Defaults to 1024.
   * @param {Set} [exclude] Optional, an Set of paths to exclude from the count.
   * @returns {Set} Set of install paths, lower cased.
   */
  getInstallPaths(limit = 1024, exclude = new Set()) {
    // This is somewhat more complicated than just collecting all values because
    // the same install can be listed in both HKCU and HKLM.  The strategy is to
    // add all paths to a Set to deduplicate.

    const lcExclude = new Set();
    exclude.forEach(p => lcExclude.add(p.toLowerCase()));

    // Add the names of the values under `rootKey\subKey` to `set`.
    // All strings are lower cased first, as Windows paths are not case-sensitive.
    function collectValues(rootKey, wowFlag, subKey, set) {
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
        for (let i = 0; i < valueCount; ++i) {
          const path = key.getValueName(i).toLowerCase();
          if (!lcExclude.has(path)) {
            set.add(path);
          }
          if (set.size >= limit) {
            break;
          }
        }
      } finally {
        key.close();
      }
    }

    const subKeyName = `Software\\Mozilla\\${Services.appinfo.name}\\TaskBarIDs`;

    const paths = new Set();

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
        paths
      );
      if (paths.size >= limit) {
        break;
      }
    }

    if (paths.size < limit) {
      // Then collect from HKCU.
      // HKCU\Software is shared between 32 and 64 so nothing special is needed for WOW64,
      // ref https://docs.microsoft.com/en-us/windows/win32/winprog64/shared-registry-keys
      collectValues(
        Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
        0 /* wowFlag */,
        subKeyName,
        paths
      );
    }

    return paths;
  },
};
