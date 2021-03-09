/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["Branch", "MarionettePrefs"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "env",
  "@mozilla.org/process/environment;1",
  "nsIEnvironment"
);

const { PREF_BOOL, PREF_INT, PREF_INVALID, PREF_STRING } = Ci.nsIPrefBranch;

class Branch {
  /**
   * @param {string=} branch
   *     Preference subtree.  Uses root tree given `null`.
   */
  constructor(branch) {
    this._branch = Services.prefs.getBranch(branch);
  }

  /**
   * Gets value of `pref` in its known type.
   *
   * @param {string} pref
   *     Preference name.
   * @param {?=} fallback
   *     Fallback value to return if `pref` does not exist.
   *
   * @return {(string|boolean|number)}
   *     Value of `pref`, or the `fallback` value if `pref` does
   *     not exist.
   *
   * @throws {TypeError}
   *     If `pref` is not a recognised preference and no `fallback`
   *     value has been provided.
   */
  get(pref, fallback = null) {
    switch (this._branch.getPrefType(pref)) {
      case PREF_STRING:
        return this._branch.getStringPref(pref);

      case PREF_BOOL:
        return this._branch.getBoolPref(pref);

      case PREF_INT:
        return this._branch.getIntPref(pref);

      case PREF_INVALID:
      default:
        if (fallback != null) {
          return fallback;
        }
        throw new TypeError(`Unrecognised preference: ${pref}`);
    }
  }

  /**
   * Sets the value of `pref`.
   *
   * @param {string} pref
   *     Preference name.
   * @param {(string|boolean|number)} value
   *     `pref`'s new value.
   *
   * @throws {TypeError}
   *     If `value` is not the correct type for `pref`.
   */
  set(pref, value) {
    let typ;
    if (typeof value != "undefined" && value != null) {
      typ = value.constructor.name;
    }

    switch (typ) {
      case "String":
        // Unicode compliant
        return this._branch.setStringPref(pref, value);

      case "Boolean":
        return this._branch.setBoolPref(pref, value);

      case "Number":
        return this._branch.setIntPref(pref, value);

      default:
        throw new TypeError(`Illegal preference type value: ${typ}`);
    }
  }
}

/**
 * Provides shortcuts for lazily getting and setting typed Marionette
 * preferences.
 *
 * Some of Marionette's preferences are stored using primitive values
 * that internally are represented by complex types.  One such example
 * is `marionette.log.level` which stores a string such as `info` or
 * `DEBUG`, and which is represented as `Log.Level`.
 *
 * Because we cannot trust the input of many of these preferences,
 * this class provides abstraction that lets us safely deal with
 * potentially malformed input.  In the `marionette.log.level` example,
 * `DEBUG`, `Debug`, and `dEbUg` are considered valid inputs and the
 * `LogBranch` specialisation deserialises the string value to the
 * correct `Log.Level` by sanitising the input data first.
 *
 * A further complication is that we cannot rely on `Preferences.jsm`
 * in Marionette.  See https://bugzilla.mozilla.org/show_bug.cgi?id=1357517
 * for further details.
 */
class MarionetteBranch extends Branch {
  constructor(branch = "marionette.") {
    super(branch);
  }

  /**
   * The `marionette.debugging.clicktostart` preference delays
   * server startup until a modal dialogue has been clicked to allow
   * time for user to set breakpoints in the Browser Toolbox.
   *
   * @return {boolean}
   */
  get clickToStart() {
    return this.get("debugging.clicktostart", false);
  }

  /**
   * The `marionette.port` preference, detailing which port
   * the TCP server should listen on.
   *
   * @return {number}
   */
  get port() {
    return this.get("port", 2828);
  }

  set port(newPort) {
    this.set("port", newPort);
  }

  /**
   * Fail-safe return of the current log level from preference
   * `marionette.log.level`.
   *
   * @return {Log.Level}
   */
  get logLevel() {
    // TODO: when geckodriver's minimum supported Firefox version reaches 62,
    // the lower-casing here can be dropped (https://bugzil.la/1482829)
    switch (this.get("log.level", "info").toLowerCase()) {
      case "fatal":
        return Log.Level.Fatal;
      case "error":
        return Log.Level.Error;
      case "warn":
        return Log.Level.Warn;
      case "config":
        return Log.Level.Config;
      case "debug":
        return Log.Level.Debug;
      case "trace":
        return Log.Level.Trace;
      case "info":
      default:
        dump(`*** log: ${Log}\n\n`);
        return Log.Level.Info;
    }
  }

  /**
   * Certain log messages that are known to be long are truncated
   * before they are dumped to stdout.  The `marionette.log.truncate`
   * preference indicates that the values should not be truncated.
   *
   * @return {boolean}
   */
  get truncateLog() {
    return this.get("log.truncate");
  }

  /**
   * Gets the `marionette.prefs.recommended` preference, signifying
   * whether recommended automation preferences will be set when
   * Marionette is started.
   *
   * @return {boolean}
   */
  get recommendedPrefs() {
    return this.get("prefs.recommended", true);
  }

  /**
   * Gets the `marionette.setpermission.enabled` preference, should
   * only be used for testdriver's set_permission API.
   *
   * @return {boolean}
   */
  get setPermissionEnabled() {
    return this.get("setpermission.enabled", false);
  }
}

/** Reads a JSON serialised blob stored in the environment. */
class EnvironmentPrefs {
  /**
   * Reads the environment variable `key` and tries to parse it as
   * JSON Object, then provides an iterator over its keys and values.
   *
   * If the environment variable is not set, this function returns empty.
   *
   * @param {string} key
   *     Environment variable.
   *
   * @return {Iterable.<string, (string|boolean|number)>
   */
  static *from(key) {
    if (!env.exists(key)) {
      return;
    }

    let prefs;
    try {
      prefs = JSON.parse(env.get(key));
    } catch (e) {
      throw new TypeError(`Unable to parse prefs from ${key}`, e);
    }

    for (let prefName of Object.keys(prefs)) {
      yield [prefName, prefs[prefName]];
    }
  }
}

this.Branch = Branch;
this.EnvironmentPrefs = EnvironmentPrefs;

// There is a future potential of exposing this as Marionette.prefs.port
// if we introduce a Marionette.jsm module.
this.MarionettePrefs = new MarionetteBranch();
