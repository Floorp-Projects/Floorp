/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["WindowsRegistry"];

const WindowsRegistry = {
  /**
   * Safely reads a value from the registry.
   *
   * @param aRoot
   *        The root registry to use.
   * @param aPath
   *        The registry path to the key.
   * @param aKey
   *        The key name.
   * @return The key value or undefined if it doesn't exist.  If the key is
   *         a REG_MULTI_SZ, an array is returned.
   */
  readRegKey: function(aRoot, aPath, aKey) {
    const kRegMultiSz = 7;
    let registry = Cc["@mozilla.org/windows-registry-key;1"].
                   createInstance(Ci.nsIWindowsRegKey);
    try {
      registry.open(aRoot, aPath, Ci.nsIWindowsRegKey.ACCESS_READ);
      if (registry.hasValue(aKey)) {
        let type = registry.getValueType(aKey);
        switch (type) {
          case kRegMultiSz:
            // nsIWindowsRegKey doesn't support REG_MULTI_SZ type out of the box.
            let str = registry.readStringValue(aKey);
            return [v for each (v in str.split("\0")) if (v)];
          case Ci.nsIWindowsRegKey.TYPE_STRING:
            return registry.readStringValue(aKey);
          case Ci.nsIWindowsRegKey.TYPE_INT:
            return registry.readIntValue(aKey);
          default:
            throw new Error("Unsupported registry value.");
        }
      }
    } catch (ex) {
    } finally {
      registry.close();
    }
    return undefined;
  },
};
