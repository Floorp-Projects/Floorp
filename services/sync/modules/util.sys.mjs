/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Observers } from "resource://services-common/observers.sys.mjs";

import { CommonUtils } from "resource://services-common/utils.sys.mjs";
import { CryptoUtils } from "resource://services-crypto/utils.sys.mjs";

import {
  DEVICE_TYPE_DESKTOP,
  MAXIMUM_BACKOFF_INTERVAL,
  PREFS_BRANCH,
  SYNC_KEY_DECODED_LENGTH,
  SYNC_KEY_ENCODED_LENGTH,
  WEAVE_VERSION,
} from "resource://services-sync/constants.sys.mjs";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
import * as FxAccountsCommon from "resource://gre/modules/FxAccountsCommon.sys.mjs";

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "cryptoSDR",
  "@mozilla.org/login-manager/crypto/SDR;1",
  "nsILoginManagerCrypto"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "localDeviceType",
  "services.sync.client.type",
  DEVICE_TYPE_DESKTOP
);

/*
 * Custom exception types.
 */
class LockException extends Error {
  constructor(message) {
    super(message);
    this.name = "LockException";
  }
}

class HMACMismatch extends Error {
  constructor(message) {
    super(message);
    this.name = "HMACMismatch";
  }
}

/*
 * Utility functions
 */
export var Utils = {
  // Aliases from CryptoUtils.
  generateRandomBytesLegacy: CryptoUtils.generateRandomBytesLegacy,
  computeHTTPMACSHA1: CryptoUtils.computeHTTPMACSHA1,
  digestUTF8: CryptoUtils.digestUTF8,
  digestBytes: CryptoUtils.digestBytes,
  sha256: CryptoUtils.sha256,
  hkdfExpand: CryptoUtils.hkdfExpand,
  pbkdf2Generate: CryptoUtils.pbkdf2Generate,
  getHTTPMACSHA1Header: CryptoUtils.getHTTPMACSHA1Header,

  /**
   * The string to use as the base User-Agent in Sync requests.
   * This string will look something like
   *
   *   Firefox/49.0a1 (Windows NT 6.1; WOW64; rv:46.0) FxSync/1.51.0.20160516142357.desktop
   */
  _userAgent: null,
  get userAgent() {
    if (!this._userAgent) {
      let hph = Cc["@mozilla.org/network/protocol;1?name=http"].getService(
        Ci.nsIHttpProtocolHandler
      );
      /* eslint-disable no-multi-spaces */
      this._userAgent =
        Services.appinfo.name +
        "/" +
        Services.appinfo.version + // Product.
        " (" +
        hph.oscpu +
        ")" + // (oscpu)
        " FxSync/" +
        WEAVE_VERSION +
        "." + // Sync.
        Services.appinfo.appBuildID +
        "."; // Build.
      /* eslint-enable no-multi-spaces */
    }
    return this._userAgent + lazy.localDeviceType;
  },

  /**
   * Wrap a [promise-returning] function to catch all exceptions and log them.
   *
   * @usage MyObj._catch = Utils.catch;
   *        MyObj.foo = function() { this._catch(func)(); }
   *
   * Optionally pass a function which will be called if an
   * exception occurs.
   */
  catch(func, exceptionCallback) {
    let thisArg = this;
    return async function WrappedCatch() {
      try {
        return await func.call(thisArg);
      } catch (ex) {
        thisArg._log.debug(
          "Exception calling " + (func.name || "anonymous function"),
          ex
        );
        if (exceptionCallback) {
          return exceptionCallback.call(thisArg, ex);
        }
        return null;
      }
    };
  },

  throwLockException(label) {
    throw new LockException(`Could not acquire lock. Label: "${label}".`);
  },

  /**
   * Wrap a [promise-returning] function to call lock before calling the function
   * then unlock when it finishes executing or if it threw an error.
   *
   * @usage MyObj._lock = Utils.lock;
   *        MyObj.foo = async function() { await this._lock(func)(); }
   */
  lock(label, func) {
    let thisArg = this;
    return async function WrappedLock() {
      if (!thisArg.lock()) {
        Utils.throwLockException(label);
      }

      try {
        return await func.call(thisArg);
      } finally {
        thisArg.unlock();
      }
    };
  },

  isLockException: function isLockException(ex) {
    return ex instanceof LockException;
  },

  /**
   * Wrap [promise-returning] functions to notify when it starts and
   * finishes executing or if it threw an error.
   *
   * The message is a combination of a provided prefix, the local name, and
   * the event. Possible events are: "start", "finish", "error". The subject
   * is the function's return value on "finish" or the caught exception on
   * "error". The data argument is the predefined data value.
   *
   * Example:
   *
   * @usage function MyObj(name) {
   *          this.name = name;
   *          this._notify = Utils.notify("obj:");
   *        }
   *        MyObj.prototype = {
   *          foo: function() this._notify("func", "data-arg", async function () {
   *            //...
   *          }(),
   *        };
   */
  notify(prefix) {
    return function NotifyMaker(name, data, func) {
      let thisArg = this;
      let notify = function (state, subject) {
        let mesg = prefix + name + ":" + state;
        thisArg._log.trace("Event: " + mesg);
        Observers.notify(mesg, subject, data);
      };

      return async function WrappedNotify() {
        notify("start", null);
        try {
          let ret = await func.call(thisArg);
          notify("finish", ret);
          return ret;
        } catch (ex) {
          notify("error", ex);
          throw ex;
        }
      };
    };
  },

  /**
   * GUIDs are 9 random bytes encoded with base64url (RFC 4648).
   * That makes them 12 characters long with 72 bits of entropy.
   */
  makeGUID: function makeGUID() {
    return CommonUtils.encodeBase64URL(Utils.generateRandomBytesLegacy(9));
  },

  _base64url_regex: /^[-abcdefghijklmnopqrstuvwxyz0123456789_]{12}$/i,
  checkGUID: function checkGUID(guid) {
    return !!guid && this._base64url_regex.test(guid);
  },

  /**
   * Add a simple getter/setter to an object that defers access of a property
   * to an inner property.
   *
   * @param obj
   *        Object to add properties to defer in its prototype
   * @param defer
   *        Property of obj to defer to
   * @param prop
   *        Property name to defer (or an array of property names)
   */
  deferGetSet: function Utils_deferGetSet(obj, defer, prop) {
    if (Array.isArray(prop)) {
      return prop.map(prop => Utils.deferGetSet(obj, defer, prop));
    }

    let prot = obj.prototype;

    // Create a getter if it doesn't exist yet
    if (!prot.__lookupGetter__(prop)) {
      prot.__defineGetter__(prop, function () {
        return this[defer][prop];
      });
    }

    // Create a setter if it doesn't exist yet
    if (!prot.__lookupSetter__(prop)) {
      prot.__defineSetter__(prop, function (val) {
        this[defer][prop] = val;
      });
    }
  },

  deepEquals: function eq(a, b) {
    // If they're triple equals, then it must be equals!
    if (a === b) {
      return true;
    }

    // If they weren't equal, they must be objects to be different
    if (typeof a != "object" || typeof b != "object") {
      return false;
    }

    // But null objects won't have properties to compare
    if (a === null || b === null) {
      return false;
    }

    // Make sure all of a's keys have a matching value in b
    for (let k in a) {
      if (!eq(a[k], b[k])) {
        return false;
      }
    }

    // Do the same for b's keys but skip those that we already checked
    for (let k in b) {
      if (!(k in a) && !eq(a[k], b[k])) {
        return false;
      }
    }

    return true;
  },

  // Generator and discriminator for HMAC exceptions.
  // Split these out in case we want to make them richer in future, and to
  // avoid inevitable confusion if the message changes.
  throwHMACMismatch: function throwHMACMismatch(shouldBe, is) {
    throw new HMACMismatch(
      `Record SHA256 HMAC mismatch: should be ${shouldBe}, is ${is}`
    );
  },

  isHMACMismatch: function isHMACMismatch(ex) {
    return ex instanceof HMACMismatch;
  },

  /**
   * Turn RFC 4648 base32 into our own user-friendly version.
   *   ABCDEFGHIJKLMNOPQRSTUVWXYZ234567
   * becomes
   *   abcdefghijk8mn9pqrstuvwxyz234567
   */
  base32ToFriendly: function base32ToFriendly(input) {
    return input.toLowerCase().replace(/l/g, "8").replace(/o/g, "9");
  },

  base32FromFriendly: function base32FromFriendly(input) {
    return input.toUpperCase().replace(/8/g, "L").replace(/9/g, "O");
  },

  /**
   * Key manipulation.
   */

  // Return an octet string in friendly base32 *with no trailing =*.
  encodeKeyBase32: function encodeKeyBase32(keyData) {
    return Utils.base32ToFriendly(CommonUtils.encodeBase32(keyData)).slice(
      0,
      SYNC_KEY_ENCODED_LENGTH
    );
  },

  decodeKeyBase32: function decodeKeyBase32(encoded) {
    return CommonUtils.decodeBase32(
      Utils.base32FromFriendly(Utils.normalizePassphrase(encoded))
    ).slice(0, SYNC_KEY_DECODED_LENGTH);
  },

  jsonFilePath(...args) {
    let [fileName] = args.splice(-1);

    return PathUtils.join(
      PathUtils.profileDir,
      "weave",
      ...args,
      `${fileName}.json`
    );
  },

  /**
   * Load a JSON file from disk in the profile directory.
   *
   * @param filePath
   *        JSON file path load from profile. Loaded file will be
   *        extension.
   * @param that
   *        Object to use for logging.
   *
   * @return Promise<>
   *        Promise resolved when the write has been performed.
   */
  async jsonLoad(filePath, that) {
    let path;
    if (Array.isArray(filePath)) {
      path = Utils.jsonFilePath(...filePath);
    } else {
      path = Utils.jsonFilePath(filePath);
    }

    if (that._log && that._log.trace) {
      that._log.trace("Loading json from disk: " + path);
    }

    try {
      return await IOUtils.readJSON(path);
    } catch (e) {
      if (!DOMException.isInstance(e) || e.name !== "NotFoundError") {
        if (that._log) {
          that._log.debug("Failed to load json", e);
        }
      }
      return null;
    }
  },

  /**
   * Save a json-able object to disk in the profile directory.
   *
   * @param filePath
   *        JSON file path save to <filePath>.json
   * @param that
   *        Object to use for logging.
   * @param obj
   *        Function to provide json-able object to save. If this isn't a
   *        function, it'll be used as the object to make a json string.*
   *        Function called when the write has been performed. Optional.
   *
   * @return Promise<>
   *        Promise resolved when the write has been performed.
   */
  async jsonSave(filePath, that, obj) {
    let path = PathUtils.join(
      PathUtils.profileDir,
      "weave",
      ...(filePath + ".json").split("/")
    );
    let dir = PathUtils.parent(path);

    await IOUtils.makeDirectory(dir, { createAncestors: true });

    if (that._log) {
      that._log.trace("Saving json to disk: " + path);
    }

    let json = typeof obj == "function" ? obj.call(that) : obj;

    return IOUtils.writeJSON(path, json);
  },

  /**
   * Helper utility function to fit an array of records so that when serialized,
   * they will be within payloadSizeMaxBytes. Returns a new array without the
   * items.
   *
   * Note: This shouldn't be used for extremely large record sizes as
   * it uses JSON.stringify, which could lead to a heavy performance hit.
   * See Bug 1815151 for more details.
   *
   */
  tryFitItems(records, payloadSizeMaxBytes) {
    // Copy this so that callers don't have to do it in advance.
    records = records.slice();
    let encoder = Utils.utf8Encoder;
    const computeSerializedSize = () =>
      encoder.encode(JSON.stringify(records)).byteLength;
    // Figure out how many records we can pack into a payload.
    // We use byteLength here because the data is not encrypted in ascii yet.
    let size = computeSerializedSize();
    // See bug 535326 comment 8 for an explanation of the estimation
    const maxSerializedSize = (payloadSizeMaxBytes / 4) * 3 - 1500;
    if (maxSerializedSize < 0) {
      // This is probably due to a test, but it causes very bad behavior if a
      // test causes this accidentally. We could throw, but there's an obvious/
      // natural way to handle it, so we do that instead (otherwise we'd have a
      // weird lower bound of ~1125b on the max record payload size).
      return [];
    }
    if (size > maxSerializedSize) {
      // Estimate a little more than the direct fraction to maximize packing
      let cutoff = Math.ceil((records.length * maxSerializedSize) / size);
      records = records.slice(0, cutoff + 1);

      // Keep dropping off the last entry until the data fits.
      while (computeSerializedSize() > maxSerializedSize) {
        records.pop();
      }
    }
    return records;
  },

  /**
   * Move a json file in the profile directory. Will fail if a file exists at the
   * destination.
   *
   * @returns a promise that resolves to undefined on success, or rejects on failure
   *
   * @param aFrom
   *        Current path to the JSON file saved on disk, relative to profileDir/weave
   *        .json will be appended to the file name.
   * @param aTo
   *        New path to the JSON file saved on disk, relative to profileDir/weave
   *        .json will be appended to the file name.
   * @param that
   *        Object to use for logging
   */
  jsonMove(aFrom, aTo, that) {
    let pathFrom = PathUtils.join(
      PathUtils.profileDir,
      "weave",
      ...(aFrom + ".json").split("/")
    );
    let pathTo = PathUtils.join(
      PathUtils.profileDir,
      "weave",
      ...(aTo + ".json").split("/")
    );
    if (that._log) {
      that._log.trace("Moving " + pathFrom + " to " + pathTo);
    }
    return IOUtils.move(pathFrom, pathTo, { noOverwrite: true });
  },

  /**
   * Removes a json file in the profile directory.
   *
   * @returns a promise that resolves to undefined on success, or rejects on failure
   *
   * @param filePath
   *        Current path to the JSON file saved on disk, relative to profileDir/weave
   *        .json will be appended to the file name.
   * @param that
   *        Object to use for logging
   */
  jsonRemove(filePath, that) {
    let path = PathUtils.join(
      PathUtils.profileDir,
      "weave",
      ...(filePath + ".json").split("/")
    );
    if (that._log) {
      that._log.trace("Deleting " + path);
    }
    return IOUtils.remove(path, { ignoreAbsent: true });
  },

  /**
   * The following are the methods supported for UI use:
   *
   * * isPassphrase:
   *     determines whether a string is either a normalized or presentable
   *     passphrase.
   * * normalizePassphrase:
   *     take a presentable passphrase and reduce it to a normalized
   *     representation for storage. normalizePassphrase can safely be called
   *     on normalized input.
   */

  isPassphrase(s) {
    if (s) {
      return /^[abcdefghijkmnpqrstuvwxyz23456789]{26}$/.test(
        Utils.normalizePassphrase(s)
      );
    }
    return false;
  },

  normalizePassphrase: function normalizePassphrase(pp) {
    // Short var name... have you seen the lines below?!
    // Allow leading and trailing whitespace.
    pp = pp.trim().toLowerCase();

    // 20-char sync key.
    if (pp.length == 23 && [5, 11, 17].every(i => pp[i] == "-")) {
      return (
        pp.slice(0, 5) + pp.slice(6, 11) + pp.slice(12, 17) + pp.slice(18, 23)
      );
    }

    // "Modern" 26-char key.
    if (pp.length == 31 && [1, 7, 13, 19, 25].every(i => pp[i] == "-")) {
      return (
        pp.slice(0, 1) +
        pp.slice(2, 7) +
        pp.slice(8, 13) +
        pp.slice(14, 19) +
        pp.slice(20, 25) +
        pp.slice(26, 31)
      );
    }

    // Something else -- just return.
    return pp;
  },

  /**
   * Create an array like the first but without elements of the second. Reuse
   * arrays if possible.
   */
  arraySub: function arraySub(minuend, subtrahend) {
    if (!minuend.length || !subtrahend.length) {
      return minuend;
    }
    let setSubtrahend = new Set(subtrahend);
    return minuend.filter(i => !setSubtrahend.has(i));
  },

  /**
   * Build the union of two arrays. Reuse arrays if possible.
   */
  arrayUnion: function arrayUnion(foo, bar) {
    if (!foo.length) {
      return bar;
    }
    if (!bar.length) {
      return foo;
    }
    return foo.concat(Utils.arraySub(bar, foo));
  },

  /**
   * Add all the items in `items` to the provided Set in-place.
   *
   * @return The provided set.
   */
  setAddAll(set, items) {
    for (let item of items) {
      set.add(item);
    }
    return set;
  },

  /**
   * Delete every items in `items` to the provided Set in-place.
   *
   * @return The provided set.
   */
  setDeleteAll(set, items) {
    for (let item of items) {
      set.delete(item);
    }
    return set;
  },

  /**
   * Take the first `size` items from the Set `items`.
   *
   * @return A Set of size at most `size`
   */
  subsetOfSize(items, size) {
    let result = new Set();
    let count = 0;
    for (let item of items) {
      if (count++ == size) {
        return result;
      }
      result.add(item);
    }
    return result;
  },

  bind2: function Async_bind2(object, method) {
    return function innerBind() {
      return method.apply(object, arguments);
    };
  },

  /**
   * Is there a master password configured and currently locked?
   */
  mpLocked() {
    return !lazy.cryptoSDR.isLoggedIn;
  },

  // If Master Password is enabled and locked, present a dialog to unlock it.
  // Return whether the system is unlocked.
  ensureMPUnlocked() {
    if (lazy.cryptoSDR.uiBusy) {
      return false;
    }
    try {
      lazy.cryptoSDR.encrypt("bacon");
      return true;
    } catch (e) {}
    return false;
  },

  /**
   * Return a value for a backoff interval.  Maximum is eight hours, unless
   * Status.backoffInterval is higher.
   *
   */
  calculateBackoff: function calculateBackoff(
    attempts,
    baseInterval,
    statusInterval
  ) {
    let backoffInterval =
      attempts * (Math.floor(Math.random() * baseInterval) + baseInterval);
    return Math.max(
      Math.min(backoffInterval, MAXIMUM_BACKOFF_INTERVAL),
      statusInterval
    );
  },

  /**
   * Return a set of hostnames (including the protocol) which may have
   * credentials for sync itself stored in the login manager.
   *
   * In general, these hosts will not have their passwords synced, will be
   * reset when we drop sync credentials, etc.
   */
  getSyncCredentialsHosts() {
    let result = new Set();
    // the FxA host
    result.add(FxAccountsCommon.FXA_PWDMGR_HOST);
    // We used to include the FxA hosts (hence the Set() result) but we now
    // don't give them special treatment (hence the Set() with exactly 1 item)
    return result;
  },

  /**
   * Helper to implement a more efficient version of fairly common pattern:
   *
   * Utils.defineLazyIDProperty(this, "syncID", "services.sync.client.syncID")
   *
   * is equivalent to (but more efficient than) the following:
   *
   * Foo.prototype = {
   *   ...
   *   get syncID() {
   *     let syncID = Svc.PrefBranch.getStringPref("client.syncID", "");
   *     return syncID == "" ? this.syncID = Utils.makeGUID() : syncID;
   *   },
   *   set syncID(value) {
   *     Svc.PrefBranch.setStringPref("client.syncID", value);
   *   },
   *   ...
   * };
   */
  defineLazyIDProperty(object, propName, prefName) {
    // An object that exists to be the target of the lazy pref getter.
    // We can't use `object` (at least, not using `propName`) since XPCOMUtils
    // will stomp on any setter we define.
    const storage = {};
    XPCOMUtils.defineLazyPreferenceGetter(storage, "value", prefName, "");
    Object.defineProperty(object, propName, {
      configurable: true,
      enumerable: true,
      get() {
        let value = storage.value;
        if (!value) {
          value = Utils.makeGUID();
          Services.prefs.setStringPref(prefName, value);
        }
        return value;
      },
      set(value) {
        Services.prefs.setStringPref(prefName, value);
      },
    });
  },

  getDeviceType() {
    return lazy.localDeviceType;
  },

  formatTimestamp(date) {
    // Format timestamp as: "%Y-%m-%d %H:%M:%S"
    let year = String(date.getFullYear());
    let month = String(date.getMonth() + 1).padStart(2, "0");
    let day = String(date.getDate()).padStart(2, "0");
    let hours = String(date.getHours()).padStart(2, "0");
    let minutes = String(date.getMinutes()).padStart(2, "0");
    let seconds = String(date.getSeconds()).padStart(2, "0");

    return `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;
  },

  *walkTree(tree, parent = null) {
    if (tree) {
      // Skip root node
      if (parent) {
        yield [tree, parent];
      }
      if (tree.children) {
        for (let child of tree.children) {
          yield* Utils.walkTree(child, tree);
        }
      }
    }
  },
};

/**
 * A subclass of Set that serializes as an Array when passed to JSON.stringify.
 */
export class SerializableSet extends Set {
  toJSON() {
    return Array.from(this);
  }
}

ChromeUtils.defineLazyGetter(Utils, "_utf8Converter", function () {
  let converter = Cc[
    "@mozilla.org/intl/scriptableunicodeconverter"
  ].createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  return converter;
});

ChromeUtils.defineLazyGetter(Utils, "utf8Encoder", () => new TextEncoder());

/*
 * Commonly-used services
 */
export var Svc = {};

Svc.PrefBranch = Services.prefs.getBranch(PREFS_BRANCH);
Svc.Obs = Observers;

Svc.Obs.add("xpcom-shutdown", function () {
  for (let name in Svc) {
    delete Svc[name];
  }
});
