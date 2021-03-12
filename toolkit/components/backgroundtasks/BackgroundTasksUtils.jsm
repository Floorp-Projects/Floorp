/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["BackgroundTasksUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = ChromeUtils.import("resource://gre/modules/Console.jsm", {})
    .ConsoleAPI;
  let consoleOptions = {
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: "toolkit.backgroundtasks.loglevel",
    prefix: "BackgroundTasksUtils",
  };
  return new ConsoleAPI(consoleOptions);
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "ProfileService",
  "@mozilla.org/toolkit/profile-service;1",
  "nsIToolkitProfileService"
);

var BackgroundTasksUtils = {
  _throwIfNotLocked(lock) {
    if (!(lock instanceof Ci.nsIProfileLock)) {
      throw new Error("Passed lock was not an instance of nsIProfileLock");
    }

    try {
      // In release builds, `.directory` throws NS_ERROR_NOT_INITIALIZED when
      // unlocked.  In debug builds, `.directory` when the profile is not locked
      // will crash via `NS_ERROR`.
      if (lock.directory) {
        return;
      }
    } catch (e) {
      if (
        !(
          e instanceof Ci.nsIException &&
          e.result == Cr.NS_ERROR_NOT_INITIALIZED
        )
      ) {
        throw e;
      }
    }
    throw new Error("Profile is not locked");
  },

  /**
   * Locks the given profile and provides the path to it to the callback.
   * The callback should return a promise and once settled the profile is
   * unlocked and then the promise returned back to the caller of this function.
   *
   * @template T
   * @param {(lock: nsIProfileLock) => Promise<T>} callback
   * @param {nsIToolkitProfile} [profile] defaults to default profile
   * @return {Promise<T>}
   */
  async withProfileLock(callback, profile = ProfileService.defaultProfile) {
    if (!profile) {
      throw new Error("No default profile exists");
    }

    let lock;
    try {
      lock = profile.lock({});
      log.info(`withProfileLock: locked profile at ${lock.directory.path}`);
    } catch (e) {
      throw new Error(`Unable to lock profile: ${e}`);
    }

    try {
      // We must await to ensure any logging is displayed after the callback resolves.
      return await callback(lock);
    } finally {
      try {
        log.info(
          `withProfileLock: unlocking profile at ${lock.directory.path}`
        );
        lock.unlock();
        log.info(`withProfileLock: unlocked profile`);
      } catch (e) {
        log.warn(`withProfileLock: error unlocking profile`, e);
      }
    }
  },

  /**
   * Reads the preferences from "prefs.js" out of a profile, optionally
   * returning only names satisfying a given predicate.
   *
   * If no `lock` is given, the default profile is locked and the preferences
   * read from it.  If `lock` is given, read from the given lock's directory.
   *
   * @param {(name: string) => boolean} [predicate] a predicate to filter
   * preferences by; if not given, all preferences are accepted.
   * @param {nsIProfileLock} [lock] optional lock to use
   * @returns {object} with keys that are string preference names and values
   * that are string|number|boolean preference values.
   */
  async readPreferences(predicate = null, lock = null) {
    if (!lock) {
      return this.withProfileLock(profileLock =>
        this.readPreferences(predicate, profileLock)
      );
    }

    this._throwIfNotLocked(lock);
    log.info(`readPreferences: profile is locked`);

    let prefs = {};
    let addPref = (kind, name, value, sticky, locked) => {
      if (predicate && !predicate(name)) {
        return;
      }
      prefs[name] = value;
    };

    // We ignore any "user.js" file, since usage is low and doing otherwise
    // requires implementing a bit more of `nsIPrefsService` than feels safe.
    let prefsFile = lock.directory.clone();
    prefsFile.append("prefs.js");
    log.info(`readPreferences: will parse prefs ${prefsFile.path}`);

    let data = await IOUtils.read(prefsFile.path);
    log.debug(
      `readPreferences: parsing prefs from buffer of length ${data.length}`
    );

    Services.prefs.parsePrefsFromBuffer(
      data,
      {
        onStringPref: addPref,
        onIntPref: addPref,
        onBoolPref: addPref,
        onError(message) {
          // Firefox itself manages "prefs.js", so errors should be infrequent.
          log.error(message);
        },
      },
      prefsFile.path
    );

    log.debug(`readPreferences: parsed prefs from buffer`, prefs);
    return prefs;
  },

  /**
   * Reads the Telemetry Client ID out of a profile.
   *
   * If no `lock` is given, the default profile is locked and the preferences
   * read from it.  If `lock` is given, read from the given lock's directory.
   *
   * @param {nsIProfileLock} [lock] optional lock to use
   * @returns {string}
   */
  async readTelemetryClientID(lock = null) {
    if (!lock) {
      return this.withProfileLock(profileLock =>
        this.readTelemetryClientID(profileLock)
      );
    }

    this._throwIfNotLocked(lock);

    let stateFile = lock.directory.clone();
    stateFile.append("datareporting");
    stateFile.append("state.json");

    log.info(
      `readPreferences: will read Telemetry client ID from ${stateFile.path}`
    );

    // This JSON is always UTF-8.
    let data = await IOUtils.readUTF8(stateFile.path);
    let state = JSON.parse(data);

    return state.clientID;
  },
};
