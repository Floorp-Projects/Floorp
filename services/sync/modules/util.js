/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["Utils", "Svc"];

var {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

// FxAccountsCommon.js doesn't use a "namespace", so create one here.
XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function() {
  let FxAccountsCommon = {};
  Cu.import("resource://gre/modules/FxAccountsCommon.js", FxAccountsCommon);
  return FxAccountsCommon;
});

/*
 * Utility functions
 */

this.Utils = {
  // Alias in functions from CommonUtils. These previously were defined here.
  // In the ideal world, references to these would be removed.
  nextTick: CommonUtils.nextTick,
  namedTimer: CommonUtils.namedTimer,
  makeURI: CommonUtils.makeURI,
  encodeUTF8: CommonUtils.encodeUTF8,
  decodeUTF8: CommonUtils.decodeUTF8,
  safeAtoB: CommonUtils.safeAtoB,
  byteArrayToString: CommonUtils.byteArrayToString,
  bytesAsHex: CommonUtils.bytesAsHex,
  hexToBytes: CommonUtils.hexToBytes,
  encodeBase32: CommonUtils.encodeBase32,
  decodeBase32: CommonUtils.decodeBase32,

  // Aliases from CryptoUtils.
  generateRandomBytes: CryptoUtils.generateRandomBytes,
  computeHTTPMACSHA1: CryptoUtils.computeHTTPMACSHA1,
  digestUTF8: CryptoUtils.digestUTF8,
  digestBytes: CryptoUtils.digestBytes,
  sha1: CryptoUtils.sha1,
  sha1Base32: CryptoUtils.sha1Base32,
  sha256: CryptoUtils.sha256,
  makeHMACKey: CryptoUtils.makeHMACKey,
  makeHMACHasher: CryptoUtils.makeHMACHasher,
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
      let hph = Cc["@mozilla.org/network/protocol;1?name=http"].getService(Ci.nsIHttpProtocolHandler);
      this._userAgent =
        Services.appinfo.name + "/" + Services.appinfo.version +  // Product.
        " (" + hph.oscpu + ")" +                                  // (oscpu)
        " FxSync/" + WEAVE_VERSION + "." +                        // Sync.
        Services.appinfo.appBuildID + ".";                        // Build.
    }
    return this._userAgent + Svc.Prefs.get("client.type", "desktop");
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
        thisArg._log.debug("Exception calling " + (func.name || "anonymous function"), ex);
        if (exceptionCallback) {
          return exceptionCallback.call(thisArg, ex);
        }
        return null;
      }
    };
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
        throw "Could not acquire lock. Label: \"" + label + "\".";
      }

      try {
        return await func.call(thisArg);
      } finally {
        thisArg.unlock();
      }
    };
  },

  isLockException: function isLockException(ex) {
    return ex && ex.indexOf && ex.indexOf("Could not acquire lock.") == 0;
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
      let notify = function(state, subject) {
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
    return CommonUtils.encodeBase64URL(Utils.generateRandomBytes(9));
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
    if (Array.isArray(prop))
      return prop.map(prop => Utils.deferGetSet(obj, defer, prop));

    let prot = obj.prototype;

    // Create a getter if it doesn't exist yet
    if (!prot.__lookupGetter__(prop)) {
      prot.__defineGetter__(prop, function() {
        return this[defer][prop];
      });
    }

    // Create a setter if it doesn't exist yet
    if (!prot.__lookupSetter__(prop)) {
      prot.__defineSetter__(prop, function(val) {
        this[defer][prop] = val;
      });
    }
  },

  deepEquals: function eq(a, b) {
    // If they're triple equals, then it must be equals!
    if (a === b)
      return true;

    // If they weren't equal, they must be objects to be different
    if (typeof a != "object" || typeof b != "object")
      return false;

    // But null objects won't have properties to compare
    if (a === null || b === null)
      return false;

    // Make sure all of a's keys have a matching value in b
    for (let k in a)
      if (!eq(a[k], b[k]))
        return false;

    // Do the same for b's keys but skip those that we already checked
    for (let k in b)
      if (!(k in a) && !eq(a[k], b[k]))
        return false;

    return true;
  },

  // Generator and discriminator for HMAC exceptions.
  // Split these out in case we want to make them richer in future, and to
  // avoid inevitable confusion if the message changes.
  throwHMACMismatch: function throwHMACMismatch(shouldBe, is) {
    throw "Record SHA256 HMAC mismatch: should be " + shouldBe + ", is " + is;
  },

  isHMACMismatch: function isHMACMismatch(ex) {
    const hmacFail = "Record SHA256 HMAC mismatch: ";
    return ex && ex.indexOf && (ex.indexOf(hmacFail) == 0);
  },

  /**
   * Turn RFC 4648 base32 into our own user-friendly version.
   *   ABCDEFGHIJKLMNOPQRSTUVWXYZ234567
   * becomes
   *   abcdefghijk8mn9pqrstuvwxyz234567
   */
  base32ToFriendly: function base32ToFriendly(input) {
    return input.toLowerCase()
                .replace(/l/g, "8")
                .replace(/o/g, "9");
  },

  base32FromFriendly: function base32FromFriendly(input) {
    return input.toUpperCase()
                .replace(/8/g, "L")
                .replace(/9/g, "O");
  },

  /**
   * Key manipulation.
   */

  // Return an octet string in friendly base32 *with no trailing =*.
  encodeKeyBase32: function encodeKeyBase32(keyData) {
    return Utils.base32ToFriendly(
             Utils.encodeBase32(keyData))
           .slice(0, SYNC_KEY_ENCODED_LENGTH);
  },

  decodeKeyBase32: function decodeKeyBase32(encoded) {
    return Utils.decodeBase32(
             Utils.base32FromFriendly(
               Utils.normalizePassphrase(encoded)))
           .slice(0, SYNC_KEY_DECODED_LENGTH);
  },

  jsonFilePath(filePath) {
    return OS.Path.normalize(OS.Path.join(OS.Constants.Path.profileDir, "weave", filePath + ".json"));
  },

  /**
   * Load a JSON file from disk in the profile directory.
   *
   * @param filePath
   *        JSON file path load from profile. Loaded file will be
   *        <profile>/<filePath>.json. i.e. Do not specify the ".json"
   *        extension.
   * @param that
   *        Object to use for logging.
   *
   * @return Promise<>
   *        Promise resolved when the write has been performed.
   */
  async jsonLoad(filePath, that) {
    let path = Utils.jsonFilePath(filePath);

    if (that._log && that._log.trace) {
      that._log.trace("Loading json from disk: " + filePath);
    }

    try {
      return await CommonUtils.readJSON(path);
    } catch (e) {
      if (!(e instanceof OS.File.Error && e.becauseNoSuchFile)) {
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
    let path = OS.Path.join(OS.Constants.Path.profileDir, "weave",
                            ...(filePath + ".json").split("/"));
    let dir = OS.Path.dirname(path);

    await OS.File.makeDir(dir, { from: OS.Constants.Path.profileDir });

    if (that._log) {
      that._log.trace("Saving json to disk: " + path);
    }

    let json = typeof obj == "function" ? obj.call(that) : obj;

    return CommonUtils.writeJSON(json, path);
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
    let pathFrom = OS.Path.join(OS.Constants.Path.profileDir, "weave",
                                ...(aFrom + ".json").split("/"));
    let pathTo = OS.Path.join(OS.Constants.Path.profileDir, "weave",
                              ...(aTo + ".json").split("/"));
    if (that._log) {
      that._log.trace("Moving " + pathFrom + " to " + pathTo);
    }
    return OS.File.move(pathFrom, pathTo, { noOverwrite: true });
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
    let path = OS.Path.join(OS.Constants.Path.profileDir, "weave",
                            ...(filePath + ".json").split("/"));
    if (that._log) {
      that._log.trace("Deleting " + path);
    }
    return OS.File.remove(path, { ignoreAbsent: true });
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
      return /^[abcdefghijkmnpqrstuvwxyz23456789]{26}$/.test(Utils.normalizePassphrase(s));
    }
    return false;
  },

  normalizePassphrase: function normalizePassphrase(pp) {
    // Short var name... have you seen the lines below?!
    // Allow leading and trailing whitespace.
    pp = pp.trim().toLowerCase();

    // 20-char sync key.
    if (pp.length == 23 &&
        [5, 11, 17].every(i => pp[i] == "-")) {

      return pp.slice(0, 5) + pp.slice(6, 11)
             + pp.slice(12, 17) + pp.slice(18, 23);
    }

    // "Modern" 26-char key.
    if (pp.length == 31 &&
        [1, 7, 13, 19, 25].every(i => pp[i] == "-")) {

      return pp.slice(0, 1) + pp.slice(2, 7)
             + pp.slice(8, 13) + pp.slice(14, 19)
             + pp.slice(20, 25) + pp.slice(26, 31);
    }

    // Something else -- just return.
    return pp;
  },

  /**
   * Create an array like the first but without elements of the second. Reuse
   * arrays if possible.
   */
  arraySub: function arraySub(minuend, subtrahend) {
    if (!minuend.length || !subtrahend.length)
      return minuend;
    let setSubtrahend = new Set(subtrahend);
    return minuend.filter(i => !setSubtrahend.has(i));
  },

  /**
   * Build the union of two arrays. Reuse arrays if possible.
   */
  arrayUnion: function arrayUnion(foo, bar) {
    if (!foo.length)
      return bar;
    if (!bar.length)
      return foo;
    return foo.concat(Utils.arraySub(bar, foo));
  },

  bind2: function Async_bind2(object, method) {
    return function innerBind() { return method.apply(object, arguments); };
  },

  /**
   * Is there a master password configured, regardless of current lock state?
   */
  mpEnabled: function mpEnabled() {
    let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                    .getService(Ci.nsIPK11TokenDB);
    let token = tokenDB.getInternalKeyToken();
    return token.hasPassword;
  },

  /**
   * Is there a master password configured and currently locked?
   */
  mpLocked: function mpLocked() {
    let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"]
                    .getService(Ci.nsIPK11TokenDB);
    let token = tokenDB.getInternalKeyToken();
    return token.hasPassword && !token.isLoggedIn();
  },

  // If Master Password is enabled and locked, present a dialog to unlock it.
  // Return whether the system is unlocked.
  ensureMPUnlocked: function ensureMPUnlocked() {
    if (!Utils.mpLocked()) {
      return true;
    }
    let sdr = Cc["@mozilla.org/security/sdr;1"]
                .getService(Ci.nsISecretDecoderRing);
    try {
      sdr.encryptString("bacon");
      return true;
    } catch (e) {}
    return false;
  },

  /**
   * Return a value for a backoff interval.  Maximum is eight hours, unless
   * Status.backoffInterval is higher.
   *
   */
  calculateBackoff: function calculateBackoff(attempts, baseInterval,
                                              statusInterval) {
    let backoffInterval = attempts *
                          (Math.floor(Math.random() * baseInterval) +
                           baseInterval);
    return Math.max(Math.min(backoffInterval, MAXIMUM_BACKOFF_INTERVAL),
                    statusInterval);
  },

  /**
   * Return a set of hostnames (including the protocol) which may have
   * credentials for sync itself stored in the login manager.
   *
   * In general, these hosts will not have their passwords synced, will be
   * reset when we drop sync credentials, etc.
   */
  getSyncCredentialsHosts() {
    let result = new Set(this.getSyncCredentialsHostsLegacy());
    for (let host of this.getSyncCredentialsHostsFxA()) {
      result.add(host);
    }
    return result;
  },

  /*
   * Get the "legacy" identity hosts.
   */
  getSyncCredentialsHostsLegacy() {
    // the legacy sync host
    return new Set([PWDMGR_HOST]);
  },

  /*
   * Get the FxA identity hosts.
   */
  getSyncCredentialsHostsFxA() {
    let result = new Set();
    // the FxA host
    result.add(FxAccountsCommon.FXA_PWDMGR_HOST);
    // We used to include the FxA hosts (hence the Set() result) but we now
    // don't give them special treatment (hence the Set() with exactly 1 item)
    return result;
  },

  getDefaultDeviceName() {
    // Generate a client name if we don't have a useful one yet
    let env = Cc["@mozilla.org/process/environment;1"]
                .getService(Ci.nsIEnvironment);
    let user = env.get("USER") || env.get("USERNAME") ||
               Svc.Prefs.get("account") || Svc.Prefs.get("username");
    // A little hack for people using the the moz-build environment on Windows
    // which sets USER to the literal "%USERNAME%" (yes, really)
    if (user == "%USERNAME%" && env.get("USERNAME")) {
      user = env.get("USERNAME");
    }

    let brand = Services.strings.createBundle(
      "chrome://branding/locale/brand.properties");
    let brandName = brand.GetStringFromName("brandShortName");

    let system =
      // 'device' is defined on unix systems
      Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2).get("device") ||
      // hostname of the system, usually assigned by the user or admin
      Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService).myHostName ||
      // fall back on ua info string
      Cc["@mozilla.org/network/protocol;1?name=http"].getService(Ci.nsIHttpProtocolHandler).oscpu;

    let syncStrings = Services.strings.createBundle("chrome://weave/locale/sync.properties");
    return syncStrings.formatStringFromName("client.name2", [user, brandName, system], 3);
  },

  getDeviceName() {
    const deviceName = Svc.Prefs.get("client.name", "");

    if (deviceName === "") {
      return this.getDefaultDeviceName();
    }

    return deviceName;
  },

  getDeviceType() {
    return Svc.Prefs.get("client.type", DEVICE_TYPE_DESKTOP);
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
  }
};

XPCOMUtils.defineLazyGetter(Utils, "_utf8Converter", function() {
  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  return converter;
});

/*
 * Commonly-used services
 */
this.Svc = {};
Svc.Prefs = new Preferences(PREFS_BRANCH);
Svc.Obs = Observers;

Svc.Obs.add("xpcom-shutdown", function() {
  for (let name in Svc)
    delete Svc[name];
});
