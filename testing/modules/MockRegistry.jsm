/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["MockRegistry"];

const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

class MockRegistry {
  constructor() {
    // Three level structure of Maps pointing to Maps pointing to Maps
    // this.roots is the top of the structure and has ROOT_KEY_* values
    // as keys.  Maps at the second level are the values of the first
    // level Map, they have registry keys (also called paths) as keys.
    // Third level maps are the values in second level maps, they have
    // map registry names to corresponding values (which in this implementation
    // are always strings).
    this.roots = new Map([
      [Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE, new Map()],
      [Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, new Map()],
      [Ci.nsIWindowsRegKey.ROOT_KEY_CLASSES_ROOT, new Map()],
    ]);

    let registry = this;

    /**
     * This is a mock nsIWindowsRegistry implementation. It only implements a
     * subset of the interface used in tests.  In particular, only values
     * of type string are supported.
     */
    function MockWindowsRegKey() {}
    MockWindowsRegKey.prototype = {
      values: null,

      // --- Overridden nsISupports interface functions ---
      QueryInterface: ChromeUtils.generateQI(["nsIWindowsRegKey"]),

      // --- Overridden nsIWindowsRegKey interface functions ---
      open(root, path, mode) {
        let rootKey = registry.getRoot(root);
        if (!rootKey.has(path)) {
          rootKey.set(path, new Map());
        }
        this.values = rootKey.get(path);
      },

      close() {
        this.values = null;
      },

      get valueCount() {
        if (!this.values) {
          throw Components.Exception("", Cr.NS_ERROR_FAILURE);
        }
        return this.values.size;
      },

      hasValue(name) {
        if (!this.values) {
          return false;
        }
        return this.values.has(name);
      },

      getValueType(name) {
        return Ci.nsIWindowsRegKey.TYPE_STRING;
      },

      getValueName(index) {
        if (!this.values || index >= this.values.size) {
          throw Components.Exception("", Cr.NS_ERROR_FAILURE);
        }
        let names = Array.from(this.values.keys());
        return names[index];
      },

      readStringValue(name) {
        if (!this.values) {
          throw new Error("invalid registry path");
        }
        return this.values.get(name);
      },
    };

    // See bug 1688838 - nsNotifyAddrListener::CheckAdaptersAddresses might
    // attempt to use the registry off the main thread, so we disable that
    // feature while the mock registry is active.
    this.oldSuffixListPref = Services.prefs.getBoolPref(
      "network.notify.dnsSuffixList"
    );
    Services.prefs.setBoolPref("network.notify.dnsSuffixList", false);

    this.oldCheckForProxiesPref = Services.prefs.getBoolPref(
      "network.notify.checkForProxies"
    );
    Services.prefs.setBoolPref("network.notify.checkForProxies", false);

    this.oldCheckForNRPTPref = Services.prefs.getBoolPref(
      "network.notify.checkForNRPT"
    );
    Services.prefs.setBoolPref("network.notify.checkForNRPT", false);

    this.cid = MockRegistrar.register(
      "@mozilla.org/windows-registry-key;1",
      MockWindowsRegKey
    );
  }

  shutdown() {
    MockRegistrar.unregister(this.cid);
    Services.prefs.setBoolPref(
      "network.notify.dnsSuffixList",
      this.oldSuffixListPref
    );
    Services.prefs.setBoolPref(
      "network.notify.checkForProxies",
      this.oldCheckForProxiesPref
    );
    Services.prefs.setBoolPref(
      "network.notify.checkForNRPT",
      this.oldCheckForNRPTPref
    );
    this.cid = null;
  }

  getRoot(root) {
    if (!this.roots.has(root)) {
      throw new Error(`No such root ${root}`);
    }
    return this.roots.get(root);
  }

  setValue(root, path, name, value) {
    let rootKey = this.getRoot(root);

    if (!rootKey.has(path)) {
      rootKey.set(path, new Map());
    }

    let pathmap = rootKey.get(path);
    if (value == null) {
      pathmap.delete(name);
    } else {
      pathmap.set(name, value);
    }
  }
}
