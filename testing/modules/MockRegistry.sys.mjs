/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MockRegistrar } from "resource://testing-common/MockRegistrar.sys.mjs";

class MockWindowsRegKey {
  key = null;

  // --- Overridden nsISupports interface functions ---
  QueryInterface = ChromeUtils.generateQI(["nsIWindowsRegKey"]);

  #assertKey() {
    if (this.key) {
      return;
    }
    throw Components.Exception("invalid registry path", Cr.NS_ERROR_FAILURE);
  }

  #findOrMaybeCreateKey(root, path, mode, maybeCreateCallback) {
    let rootKey = MockRegistry.getRoot(root);
    let parts = path.split("\\");
    if (parts.some(part => !part.length)) {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }

    let key = rootKey;
    for (let part of parts) {
      if (!key.subkeys.has(part)) {
        maybeCreateCallback(key.subkeys, part);
      }
      key = key.subkeys.get(part);
    }
    this.key = key;
  }

  // --- Overridden nsIWindowsRegKey interface functions ---
  open(root, path, mode) {
    // eslint-disable-next-line no-unused-vars
    this.#findOrMaybeCreateKey(root, path, mode, (subkeys, part) => {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    });
  }

  create(root, path, mode) {
    this.#findOrMaybeCreateKey(root, path, mode, (subkeys, part) =>
      subkeys.set(part, { subkeys: new Map(), values: new Map() })
    );
  }

  close() {
    this.key = null;
  }

  get valueCount() {
    this.#assertKey();
    return this.key.values.size;
  }

  hasValue(name) {
    this.#assertKey();
    return this.key.values.has(name);
  }

  #getValuePair(name, expectedType = null) {
    this.#assertKey();
    if (!this.key.values.has(name)) {
      throw Components.Exception("invalid value name", Cr.NS_ERROR_FAILURE);
    }
    let [value, type] = this.key.values.get(name);
    if (expectedType && type !== expectedType) {
      throw Components.Exception("unexpected value type", Cr.NS_ERROR_FAILURE);
    }
    return [value, type];
  }

  getValueType(name) {
    let [, type] = this.#getValuePair(name);
    return type;
  }

  getValueName(index) {
    if (!this.key || index >= this.key.values.size) {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }
    let names = Array.from(this.key.values.keys());
    return names[index];
  }

  readStringValue(name) {
    let [value] = this.#getValuePair(name, Ci.nsIWindowsRegKey.TYPE_STRING);
    return value;
  }

  readIntValue(name) {
    let [value] = this.#getValuePair(name, Ci.nsIWindowsRegKey.TYPE_INT);
    return value;
  }

  readInt64Value(name) {
    let [value] = this.#getValuePair(name, Ci.nsIWindowsRegKey.TYPE_INT64);
    return value;
  }

  readBinaryValue(name) {
    let [value] = this.#getValuePair(name, Ci.nsIWindowsRegKey.TYPE_BINARY);
    return value;
  }

  #writeValuePair(name, value, type) {
    this.#assertKey();
    this.key.values.set(name, [value, type]);
  }

  writeStringValue(name, value) {
    this.#writeValuePair(name, value, Ci.nsIWindowsRegKey.TYPE_STRING);
  }

  writeIntValue(name, value) {
    this.#writeValuePair(name, value, Ci.nsIWindowsRegKey.TYPE_INT);
  }

  writeInt64Value(name, value) {
    this.#writeValuePair(name, value, Ci.nsIWindowsRegKey.TYPE_INT64);
  }

  writeBinaryValue(name, value) {
    this.#writeValuePair(name, value, Ci.nsIWindowsRegKey.TYPE_BINARY);
  }

  removeValue(name) {
    this.#assertKey();
    this.key.values.delete(name);
  }

  get childCount() {
    this.#assertKey();
    return this.key.subkeys.size;
  }

  getChildName(index) {
    if (!this.key || index >= this.key.values.size) {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }
    let names = Array.from(this.key.subkeys.keys());
    return names[index];
  }

  hasChild(name) {
    this.#assertKey();
    return this.key.subkeys.has(name);
  }

  removeChild(name) {
    this.#assertKey();
    let child = this.key.subkeys.get(name);

    if (!child) {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }
    if (child.subkeys.size > 0) {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }

    this.key.subkeys.delete(name);
  }

  #findOrMaybeCreateChild(name, mode, maybeCreateCallback) {
    this.#assertKey();
    if (name.split("\\").length > 1) {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }
    if (!this.key.subkeys.has(name)) {
      maybeCreateCallback(this.key.subkeys, name);
    }
    // This won't wrap in the same way as `Cc["@mozilla.org/windows-registry-key;1"].createInstance(nsIWindowsRegKey);`.
    let subKey = new MockWindowsRegKey();
    subKey.key = this.key.subkeys.get(name);
    return subKey;
  }

  openChild(name, mode) {
    // eslint-disable-next-line no-unused-vars
    return this.#findOrMaybeCreateChild(name, mode, (subkeys, part) => {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    });
  }

  createChild(name, mode) {
    return this.#findOrMaybeCreateChild(name, mode, (subkeys, part) =>
      subkeys.set(part, { subkeys: new Map(), values: new Map() })
    );
  }
}

export class MockRegistry {
  // All instances of `MockRegistry` share a single data-store; this is
  // conceptually parallel to the Windows registry, which is a shared global
  // resource.  It would be possible to have separate data-stores for separate
  // instances with a little adjustment to `MockWindowsRegKey`.
  //
  // Top-level map is indexed by roots.  A "key" is an object that has
  // `subkeys` and `values`, both maps indexed by strings.  Subkey items are
  // again "key" objects.  Value items are `[value, type]` pairs.
  //
  // In pseudo-code:
  //
  // {Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER:
  //  {subkeys:
  //   {child: {subkeys: {}, values: {key: ["string_value", Ci.nsIWindowsRegKey.TYPE_STRING]}}},
  //  values: {}
  //  },
  //  ...
  // }
  static roots;

  constructor() {
    MockRegistry.roots = new Map([
      [
        Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
        { subkeys: new Map(), values: new Map() },
      ],
      [
        Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
        { subkeys: new Map(), values: new Map() },
      ],
      [
        Ci.nsIWindowsRegKey.ROOT_KEY_CLASSES_ROOT,
        { subkeys: new Map(), values: new Map() },
      ],
    ]);

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
      () => new MockWindowsRegKey()
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

  static getRoot(root) {
    if (!this.roots.has(root)) {
      throw new Error(`No such root ${root}`);
    }
    return this.roots.get(root);
  }

  setValue(root, path, name, value) {
    let key = new MockWindowsRegKey();
    key.create(root, path, Ci.nsIWindowsRegKey.ACCESS_ALL);
    if (value == null) {
      try {
        key.removeValue(name);
      } catch (e) {
        if (
          !(e instanceof Ci.nsIException && e.result == Cr.NS_ERROR_FAILURE)
        ) {
          throw e;
        }
      }
    } else {
      key.writeStringValue(name, value);
    }
  }

  /**
   * Dump given `key` (or, if not given, all roots), and all its value and its
   * subkeys recursively, using the given function to `printOneLine`.
   */
  static dump(key = null, indent = "", printOneLine = console.log) {
    let types = new Map([
      [1, "REG_SZ"],
      [3, "REG_BINARY"],
      [4, "REG_DWORD"],
      [11, "REG_QWORD"],
    ]);

    if (!key) {
      let roots = [
        "ROOT_KEY_LOCAL_MACHINE",
        "ROOT_KEY_CURRENT_USER",
        "ROOT_KEY_CLASSES_ROOT",
      ];
      for (let root of roots) {
        printOneLine(indent + root);
        this.dump(
          this.roots.get(Ci.nsIWindowsRegKey[root]),
          "  " + indent,
          printOneLine
        );
      }
    } else {
      for (let [k, v] of key.values.entries()) {
        let [value, type] = v;
        printOneLine(`${indent}${k}: ${value} (${types.get(type)})`);
      }
      for (let [k, child] of key.subkeys.entries()) {
        printOneLine(indent + k);
        this.dump(child, "  " + indent, printOneLine);
      }
    }
  }
}
