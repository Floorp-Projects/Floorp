/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = ["WindowsRegistry"];

var WindowsRegistry = {
  /**
   * Safely reads a value from the registry.
   *
   * @param aRoot
   *        The root registry to use.
   * @param aPath
   *        The registry path to the key.
   * @param aKey
   *        The key name.
   * @param [aRegistryNode=0]
   *        Optionally set to nsIWindowsRegKey.WOW64_64 (or nsIWindowsRegKey.WOW64_32)
   *        to access a 64-bit (32-bit) key from either a 32-bit or 64-bit application.
   * @return The key value or undefined if it doesn't exist.  If the key is
   *         a REG_MULTI_SZ, an array is returned.
   */
  readRegKey(aRoot, aPath, aKey, aRegistryNode = 0) {
    const kRegMultiSz = 7;
    const kMode = Ci.nsIWindowsRegKey.ACCESS_READ | aRegistryNode;
    let registry = Cc["@mozilla.org/windows-registry-key;1"].
                   createInstance(Ci.nsIWindowsRegKey);
    try {
      registry.open(aRoot, aPath, kMode);
      if (registry.hasValue(aKey)) {
        let type = registry.getValueType(aKey);
        switch (type) {
          case kRegMultiSz:
            // nsIWindowsRegKey doesn't support REG_MULTI_SZ type out of the box.
            let str = registry.readStringValue(aKey);
            return str.split("\0").filter(v => v);
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

  /**
   * Safely removes a key from the registry.
   *
   * @param aRoot
   *        The root registry to use.
   * @param aPath
   *        The registry path to the key.
   * @param aKey
   *        The key name.
   * @param [aRegistryNode=0]
   *        Optionally set to nsIWindowsRegKey.WOW64_64 (or nsIWindowsRegKey.WOW64_32)
   *        to access a 64-bit (32-bit) key from either a 32-bit or 64-bit application.
   * @return True if the key was removed or never existed, false otherwise.
   */
  removeRegKey(aRoot, aPath, aKey, aRegistryNode = 0) {
    let registry = Cc["@mozilla.org/windows-registry-key;1"].
                   createInstance(Ci.nsIWindowsRegKey);
    let result = false;
    try {
      let mode = Ci.nsIWindowsRegKey.ACCESS_QUERY_VALUE |
                 Ci.nsIWindowsRegKey.ACCESS_SET_VALUE |
                 aRegistryNode;
      registry.open(aRoot, aPath, mode);
      if (registry.hasValue(aKey)) {
        registry.removeValue(aKey);
        result = !registry.hasValue(aKey);
      } else {
        result = true;
      }
    } catch (ex) {
    } finally {
      registry.close();
      return result;
    }
  }
};
